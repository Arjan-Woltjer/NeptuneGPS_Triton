/*
  triton_ota - Triton IO Board firmware entry point
  Copyright (C) 2011-2026 J.A. Woltjer.

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
#include <WiFi.h>

#include "ConfigOta.h"
#include "OtaManager.h"
#include "OtaWebServer.h"

static OtaManager    otaManager;
static OtaWebServer  otaWebServer(OTA_WEB_PORT);

// ── Self-test ─────────────────────────────────────────────────────────────────
// Return false to trigger automatic rollback to the previous firmware version.

static bool selfTest() {
    // TODO: add Triton IO peripheral checks, e.g.:
    //   if (!i2cExpanderPing()) return false;
    //   if (!adcRailOk())       return false;
    return true;
}

// ── Application ───────────────────────────────────────────────────────────────

static void appSetup() {
    // TODO: initialise peripherals
}

static void appLoop() {
    // TODO: application logic (keep non-blocking)
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WIFI] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());

    otaManager.SetSelfTestCallback(selfTest);
    otaManager.Begin();
    otaWebServer.Begin();

    appSetup();
}

void loop() {
    otaManager.Update();
    otaWebServer.Update();
    appLoop();
}
