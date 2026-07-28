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
#include <nvs_flash.h>

#include <ArduinoJson.h>

#include "ConfigOta.h"
#include "OtaCerts.h"

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
    info.valid   = (info.version > 0 && info.url.length() > 0);

    return info.valid;
}

OtaManager::Result OtaManager::performOta(const FirmwareInfo& fw) {
    HTTPClient http;
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

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t written = 0;
    int stalls = 0;

    while (written < (size_t)contentLen) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(buf, min(avail, (int)sizeof(buf)));
            if (esp_ota_write(handle, buf, n) != ESP_OK) {
                esp_ota_abort(handle);
                http.end();
                esp_task_wdt_add(NULL);
                return FLASH_ERR;
            }
            written += n;
            stalls = 0;
            if (written % 65536 < (size_t)n) {
                setLed(LED_BLINK_FAST);
                Serial.printf("[OTA] %zu / %d bytes\n", written, contentLen);
            }
        } else {
            delay(1);
            if (++stalls > 5000) {
                esp_ota_abort(handle);
                http.end();
                esp_task_wdt_add(NULL);
                return DOWNLOAD_FAIL;
            }
        }
    }

    http.end();
    esp_task_wdt_add(NULL);

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
