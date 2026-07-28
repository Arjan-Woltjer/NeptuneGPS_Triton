/**
 * Triton IO Board — Production OTA Firmware
 * ESP32-based | Secure HTTPS OTA with rollback & version checking
 *
 * Features:
 *  - HTTPS pull-based OTA with CA cert validation
 *  - Version checking before download
 *  - Automatic rollback if new firmware fails to boot
 *  - NVS-backed version & diagnostics storage
 *  - Watchdog-safe update loop
 *  - Status LED feedback during update
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <ArduinoJson.h>
#include "ota_config.h"
#include "ota_certs.h"

// ─── State ────────────────────────────────────────────────────────────────────

static nvs_handle_t nvs_handle_g;
static bool         ota_in_progress = false;

// ─── Boot validation (called FIRST in setup) ──────────────────────────────────

void validateBoot() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t   state;

  if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
      Serial.println("[OTA] New firmware pending verification...");

      // Run your board self-test here
      bool boardOk = runSelfTest();

      if (boardOk) {
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println("[OTA] Firmware verified OK. Rollback cancelled.");
        nvs_set_u32(nvs_handle_g, "boot_fail_count", 0);
        nvs_commit(nvs_handle_g);
      } else {
        Serial.println("[OTA] Self-test FAILED — triggering rollback!");
        nvs_set_u32(nvs_handle_g, "last_rollback", CURRENT_FW_VERSION);
        nvs_commit(nvs_handle_g);
        esp_ota_mark_app_invalid_rollback_and_reboot();
        // Does not return
      }
    }
  }
}

// ─── Self-test (customise for Triton IO peripherals) ──────────────────────────

bool runSelfTest() {
  Serial.println("[SELFTEST] Running board diagnostics...");

  // ① Wi-Fi connectivity
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[SELFTEST] FAIL: No Wi-Fi");
    return false;
  }

  // ② NVS read/write round-trip
  uint32_t probe = 0xDEADBEEF;
  nvs_set_u32(nvs_handle_g, "selftest_probe", probe);
  nvs_commit(nvs_handle_g);
  uint32_t read_back = 0;
  nvs_get_u32(nvs_handle_g, "selftest_probe", &read_back);
  if (read_back != probe) {
    Serial.println("[SELFTEST] FAIL: NVS round-trip");
    return false;
  }

  // ③ Add Triton IO-specific checks here:
  //    - GPIO expander comms (I2C/SPI)
  //    - ADC rail check
  //    - Relay coil check
  //    - UART loopback

  Serial.println("[SELFTEST] All checks passed.");
  return true;
}

// ─── Version check ────────────────────────────────────────────────────────────

struct FirmwareInfo {
  int    version;
  String url;
  String sha256;
  String notes;
  bool   valid;
};

FirmwareInfo fetchLatestFirmwareInfo() {
  FirmwareInfo info = {0, "", "", "", false};
  HTTPClient   http;

  Serial.printf("[OTA] Checking version at %s\n", OTA_VERSION_URL);
  http.begin(OTA_VERSION_URL, CA_CERT);
  http.setTimeout(10000);
  http.addHeader("X-Device-ID", getDeviceID());
  http.addHeader("X-Current-Version", String(CURRENT_FW_VERSION));

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Version check failed: HTTP %d\n", code);
    http.end();
    return info;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
    return info;
  }

  info.version = doc["version"] | 0;
  info.url     = doc["url"].as<String>();
  info.sha256  = doc["sha256"].as<String>();
  info.notes   = doc["notes"].as<String>();
  info.valid   = (info.version > 0 && info.url.length() > 0);

  return info;
}

// ─── Core OTA download & flash ────────────────────────────────────────────────

OtaResult performOTA(const FirmwareInfo& fw) {
  HTTPClient http;
  Serial.printf("[OTA] Downloading firmware v%d from %s\n", fw.version, fw.url.c_str());

  http.begin(fw.url, CA_CERT);
  http.setTimeout(60000);  // 60s for binary download
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Download failed: HTTP %d\n", code);
    http.end();
    return OTA_DOWNLOAD_FAIL;
  }

  int contentLen = http.getSize();
  if (contentLen <= 0) {
    Serial.println("[OTA] Invalid content length");
    http.end();
    return OTA_INVALID_IMAGE;
  }

  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    Serial.println("[OTA] No OTA partition found — check partition table!");
    http.end();
    return OTA_PARTITION_ERR;
  }

  Serial.printf("[OTA] Writing to partition: %s\n", update_partition->label);

  esp_ota_handle_t ota_handle;
  esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
  if (err != ESP_OK) {
    Serial.printf("[OTA] esp_ota_begin failed: %s\n", esp_err_to_name(err));
    http.end();
    return OTA_FLASH_ERR;
  }

  // Disable task watchdog during flash write
  esp_task_wdt_delete(NULL);

  WiFiClient* stream   = http.getStreamPtr();
  uint8_t     buf[1024];
  size_t      written  = 0;
  int         retries  = 0;

  while (written < (size_t)contentLen) {
    int available = stream->available();
    if (available > 0) {
      int toRead = min(available, (int)sizeof(buf));
      int read   = stream->readBytes(buf, toRead);
      err        = esp_ota_write(ota_handle, buf, read);
      if (err != ESP_OK) {
        Serial.printf("[OTA] Write error: %s\n", esp_err_to_name(err));
        esp_ota_abort(ota_handle);
        http.end();
        esp_task_wdt_add(NULL);
        return OTA_FLASH_ERR;
      }
      written += read;
      retries  = 0;

      // Progress every 64KB
      if (written % 65536 < (size_t)read) {
        Serial.printf("[OTA] Progress: %zu / %d bytes (%.0f%%)\n",
                      written, contentLen, 100.0f * written / contentLen);
        setStatusLED(LED_BLINK_FAST);
      }
    } else {
      delay(1);
      if (++retries > 5000) {
        Serial.println("[OTA] Stream timeout");
        esp_ota_abort(ota_handle);
        http.end();
        esp_task_wdt_add(NULL);
        return OTA_DOWNLOAD_FAIL;
      }
    }
  }

  http.end();
  esp_task_wdt_add(NULL);  // Re-enable watchdog

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    Serial.printf("[OTA] esp_ota_end failed: %s\n", esp_err_to_name(err));
    return OTA_INVALID_IMAGE;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    Serial.printf("[OTA] Set boot partition failed: %s\n", esp_err_to_name(err));
    return OTA_FLASH_ERR;
  }

  // Record update in NVS before reboot
  nvs_set_u32(nvs_handle_g, "prev_version",    CURRENT_FW_VERSION);
  nvs_set_u32(nvs_handle_g, "pending_version", fw.version);
  nvs_set_str(nvs_handle_g, "update_notes",    fw.notes.c_str());
  nvs_commit(nvs_handle_g);

  Serial.printf("[OTA] Flash complete. Rebooting into v%d...\n", fw.version);
  setStatusLED(LED_ON);
  delay(500);

  return OTA_SUCCESS;
}

// ─── OTA check orchestrator ───────────────────────────────────────────────────

void checkForUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (ota_in_progress) return;

  FirmwareInfo latest = fetchLatestFirmwareInfo();

  if (!latest.valid) {
    Serial.println("[OTA] Could not fetch firmware info.");
    return;
  }

  if (latest.version <= CURRENT_FW_VERSION) {
    Serial.printf("[OTA] Already up to date (v%d).\n", CURRENT_FW_VERSION);
    return;
  }

  Serial.printf("[OTA] Update available: v%d → v%d\n", CURRENT_FW_VERSION, latest.version);
  Serial.printf("[OTA] Notes: %s\n", latest.notes.c_str());

  ota_in_progress = true;
  setStatusLED(LED_BLINK_SLOW);

  OtaResult result = performOTA(latest);
  ota_in_progress  = false;

  if (result == OTA_SUCCESS) {
    delay(1000);
    esp_restart();
  } else {
    Serial.printf("[OTA] Update failed with code %d\n", result);
    setStatusLED(LED_OFF);
    // Increment failure counter
    uint32_t fail_count = 0;
    nvs_get_u32(nvs_handle_g, "ota_fail_count", &fail_count);
    nvs_set_u32(nvs_handle_g, "ota_fail_count", fail_count + 1);
    nvs_commit(nvs_handle_g);
  }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

String getDeviceID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char id[18];
  snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(id);
}

void setStatusLED(LedMode mode) {
  // Map to your Triton IO board's status LED pin
  // Implement blink modes using a non-blocking timer pattern
  switch (mode) {
    case LED_OFF:        digitalWrite(LED_PIN, LOW);  break;
    case LED_ON:         digitalWrite(LED_PIN, HIGH); break;
    case LED_BLINK_SLOW: /* handled in loop() */       break;
    case LED_BLINK_FAST: /* handled in loop() */       break;
  }
}

// ─── Setup & Loop ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Init NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  nvs_open("triton", NVS_READWRITE, &nvs_handle_g);

  Serial.printf("\n=== Triton IO Board | Firmware v%d ===\n", CURRENT_FW_VERSION);

  // Connect Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // ① Validate boot & potentially roll back before doing anything else
  validateBoot();

  // ② Check for OTA immediately on boot
  checkForUpdate();
}

static unsigned long lastOtaCheck = 0;

void loop() {
  // Periodic OTA check (default: every OTA_CHECK_INTERVAL_MS ms)
  if (millis() - lastOtaCheck > OTA_CHECK_INTERVAL_MS) {
    lastOtaCheck = millis();
    checkForUpdate();
  }

  // ── Your Triton IO application logic below ──

  delay(10);
}
