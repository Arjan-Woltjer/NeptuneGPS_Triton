/*
  OtaManager - HTTPS OTA firmware update manager for ESP32
  Copyright (C) 2011-2026 J.A. Woltjer.
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "OtaManager.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <mbedtls/md.h>
#include <nvs_flash.h>

#include <ArduinoJson.h>

#include "ConfigOta.h"
#include "OtaCerts.h"

// SHA-256 as lowercase hex.
static constexpr unsigned kSha256HexLen = 64;

// What the digest does and does not buy: it proves the bytes that reached flash
// are the bytes the manifest described, so a truncated or corrupted download can
// no longer boot. It proves nothing about who wrote the manifest -- a server that
// serves a malicious image will serve its matching hash just as happily. Closing
// that needs the image signed with a key the device holds (esp_secure_boot, or a
// signature field verified here); the transport CA pin is the only thing standing
// in for it today.

OtaManager::OtaManager()
    : selfTestCallback(nullptr),
      nvs(0),
      otaInProgress(false),
      ledMode(LED_OFF),
      ledState(false),
      ledLastToggle(0),
      lastOtaCheck(0) {
}

void OtaManager::SetSelfTestCallback(bool (*callback)()) {
    selfTestCallback = callback;
}

void OtaManager::Begin() {
    pinMode(LED_PIN, OUTPUT);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_open("triton", NVS_READWRITE, &nvs);

    Serial.printf("=== Triton IO | Firmware v%d | %s ===\n",
                  CURRENT_FW_VERSION, getDeviceId().c_str());

    validateBoot();
    checkForUpdate();
}

void OtaManager::Update() {
    updateLed();

    if (millis() - lastOtaCheck >= OTA_CHECK_INTERVAL_MS) {
        lastOtaCheck = millis();
        checkForUpdate();
    }
}

void OtaManager::validateBoot() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;

    Serial.println("[OTA] New firmware pending verification...");

    bool ok = selfTestCallback ? selfTestCallback() : true;

    if (ok) {
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println("[OTA] Verified OK.");
        nvs_set_u32(nvs, "boot_fail_count", 0);
        nvs_commit(nvs);
    } else {
        Serial.println("[OTA] Self-test failed — rolling back.");
        nvs_set_u32(nvs, "last_rollback", CURRENT_FW_VERSION);
        nvs_commit(nvs);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

bool OtaManager::fetchFirmwareInfo(FirmwareInfo& info) {
    info = {0, "", "", "", false};
    HTTPClient http;

    http.begin(OTA_VERSION_URL, OTA_CA_CERT);
    http.setTimeout(10000);
    http.addHeader("X-Device-ID",       getDeviceId());
    http.addHeader("X-Current-Version", String(CURRENT_FW_VERSION));

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] Version check failed: HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, payload)) return false;

    info.version = doc["version"] | 0;
    info.url     = doc["url"].as<String>();
    info.sha256  = doc["sha256"].as<String>();
    info.notes   = doc["notes"].as<String>();

    // A manifest without a usable digest is not something to flash. The hash was
    // already being parsed here and then never used; requiring it makes an
    // unverifiable image fail closed rather than install silently.
    info.valid = (info.version > 0 &&
                  info.url.length() > 0 &&
                  info.sha256.length() == kSha256HexLen &&
                  trustedUrl(info.url));

    if (!info.valid) {
        Serial.println("[OTA] Manifest rejected: needs version, trusted https URL and sha256.");
    }

    return info.valid;
}

// Only https, and only from the configured server. HTTPClient::begin(url, ca)
// ignores the CA argument entirely for an http:// URL, so without this check a
// manifest could downgrade its own firmware download to cleartext just by
// changing the scheme.
bool OtaManager::trustedUrl(const String& url) {
    if (!url.startsWith("https://")) {
        Serial.println("[OTA] Refusing non-https firmware URL.");
        return false;
    }
    if (!url.startsWith(OTA_SERVER_HOST "/")) {
        Serial.println("[OTA] Refusing firmware URL outside OTA_SERVER_HOST.");
        return false;
    }
    return true;
}

String OtaManager::toHex(const uint8_t* bytes, size_t len) {
    static const char* digits = "0123456789abcdef";
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        out += digits[(bytes[i] >> 4) & 0x0F];
        out += digits[bytes[i] & 0x0F];
    }
    return out;
}

OtaManager::Result OtaManager::performOta(const FirmwareInfo& fw) {
    HTTPClient http;

    // Re-checked here rather than trusted from fetchFirmwareInfo(), so this stays
    // correct if performOta() ever gains another caller.
    if (!trustedUrl(fw.url)) {
        return UNTRUSTED_URL;
    }

    Serial.printf("[OTA] Downloading v%d from %s\n", fw.version, fw.url.c_str());

    http.begin(fw.url, OTA_CA_CERT);
    http.setTimeout(60000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] Download failed: HTTP %d\n", code);
        http.end();
        return DOWNLOAD_FAIL;
    }

    int contentLen = http.getSize();
    if (contentLen <= 0) {
        http.end();
        return INVALID_IMAGE;
    }

    const esp_partition_t* part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        Serial.println("[OTA] No OTA partition — check partitions.csv");
        http.end();
        return PARTITION_ERR;
    }

    esp_ota_handle_t handle;
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) {
        http.end();
        return FLASH_ERR;
    }

    esp_task_wdt_delete(NULL);

    // Hash the image as it streams past, so verification costs no extra flash
    // reads and no second pass.
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    if (mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) != 0 ||
        mbedtls_md_starts(&md) != 0) {
        mbedtls_md_free(&md);
        esp_ota_abort(handle);
        http.end();
        esp_task_wdt_add(NULL);
        return FLASH_ERR;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t written = 0;
    int stalls = 0;

    while (written < (size_t)contentLen) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(buf, min(avail, (int)sizeof(buf)));
            if (esp_ota_write(handle, buf, n) != ESP_OK) {
                mbedtls_md_free(&md);
                esp_ota_abort(handle);
                http.end();
                esp_task_wdt_add(NULL);
                return FLASH_ERR;
            }
            mbedtls_md_update(&md, buf, n);
            written += n;
            stalls = 0;
            if (written % 65536 < (size_t)n) {
                setLed(LED_BLINK_FAST);
                Serial.printf("[OTA] %zu / %d bytes\n", written, contentLen);
            }
        } else {
            delay(1);
            if (++stalls > 5000) {
                mbedtls_md_free(&md);
                esp_ota_abort(handle);
                http.end();
                esp_task_wdt_add(NULL);
                return DOWNLOAD_FAIL;
            }
        }
    }

    http.end();
    esp_task_wdt_add(NULL);

    uint8_t digest[32];
    int mdRc = mbedtls_md_finish(&md, digest);
    mbedtls_md_free(&md);
    if (mdRc != 0) {
        esp_ota_abort(handle);
        return FLASH_ERR;
    }

    // Checked before esp_ota_end()/esp_ota_set_boot_partition(), so a mismatched
    // image is discarded rather than left bootable.
    const String actual = toHex(digest, sizeof(digest));
    if (!actual.equalsIgnoreCase(fw.sha256)) {
        Serial.printf("[OTA] SHA-256 mismatch.\n  expected %s\n  actual   %s\n",
                      fw.sha256.c_str(), actual.c_str());
        esp_ota_abort(handle);
        return HASH_MISMATCH;
    }
    Serial.println("[OTA] SHA-256 verified.");

    if (esp_ota_end(handle) != ESP_OK) return INVALID_IMAGE;
    if (esp_ota_set_boot_partition(part) != ESP_OK) return FLASH_ERR;

    nvs_set_u32(nvs, "prev_version",    CURRENT_FW_VERSION);
    nvs_set_u32(nvs, "pending_version", fw.version);
    nvs_set_str(nvs, "update_notes",    fw.notes.c_str());
    nvs_commit(nvs);

    Serial.printf("[OTA] Flash complete — rebooting into v%d\n", fw.version);
    return SUCCESS;
}

void OtaManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (otaInProgress) return;

    FirmwareInfo latest;
    if (!fetchFirmwareInfo(latest)) {
        Serial.println("[OTA] Could not fetch firmware info.");
        return;
    }

    if (latest.version <= CURRENT_FW_VERSION) {
        Serial.printf("[OTA] Up to date (v%d).\n", CURRENT_FW_VERSION);
        return;
    }

    Serial.printf("[OTA] Update available: v%d -> v%d | %s\n",
                  CURRENT_FW_VERSION, latest.version, latest.notes.c_str());

    otaInProgress = true;
    setLed(LED_BLINK_SLOW);

    Result result = performOta(latest);
    otaInProgress = false;

    if (result == SUCCESS) {
        setLed(LED_ON);
        delay(500);
        esp_restart();
    } else {
        Serial.printf("[OTA] Update failed (code %d)\n", result);
        setLed(LED_OFF);
        uint32_t n = 0;
        nvs_get_u32(nvs, "ota_fail_count", &n);
        nvs_set_u32(nvs, "ota_fail_count", n + 1);
        nvs_commit(nvs);
    }
}

void OtaManager::setLed(LedMode mode) {
    ledMode = mode;
    if (mode == LED_OFF) { digitalWrite(LED_PIN, LOW);  ledState = false; }
    if (mode == LED_ON)  { digitalWrite(LED_PIN, HIGH); ledState = true;  }
}

void OtaManager::updateLed() {
    unsigned long interval = 0;
    if      (ledMode == LED_BLINK_SLOW) interval = 500;
    else if (ledMode == LED_BLINK_FAST) interval = 125;
    else return;

    if (millis() - ledLastToggle >= interval) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        ledLastToggle = millis();
    }
}

String OtaManager::getDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[13];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(id);
}
