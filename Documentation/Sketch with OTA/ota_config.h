#pragma once

// ─── Firmware Version ─────────────────────────────────────────────────────────
// Increment this with every build you deploy.
#define CURRENT_FW_VERSION  1

// ─── Wi-Fi Credentials ────────────────────────────────────────────────────────
#define WIFI_SSID      "YOUR_SSID"
#define WIFI_PASSWORD  "YOUR_PASSWORD"

// ─── OTA Server ───────────────────────────────────────────────────────────────
#define OTA_SERVER_HOST    "https://ota.yourdomain.com"
#define OTA_VERSION_URL    OTA_SERVER_HOST "/api/firmware/latest"

// ─── OTA Timing ───────────────────────────────────────────────────────────────
// How often the board polls for updates (ms). Default: 1 hour.
#define OTA_CHECK_INTERVAL_MS  (60UL * 60UL * 1000UL)

// ─── Hardware ─────────────────────────────────────────────────────────────────
#define LED_PIN  2   // Onboard/status LED — adjust for Triton IO board

// ─── Types ────────────────────────────────────────────────────────────────────
typedef enum {
  OTA_SUCCESS       = 0,
  OTA_DOWNLOAD_FAIL = 1,
  OTA_INVALID_IMAGE = 2,
  OTA_PARTITION_ERR = 3,
  OTA_FLASH_ERR     = 4,
} OtaResult;

typedef enum {
  LED_OFF,
  LED_ON,
  LED_BLINK_SLOW,
  LED_BLINK_FAST,
} LedMode;
