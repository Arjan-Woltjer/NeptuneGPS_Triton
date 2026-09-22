/*
  ConfigOta - Build-time configuration for the Triton IO OTA update system
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
#pragma once

#define CURRENT_FW_VERSION     1

#define WIFI_SSID              "YOUR_SSID"
#define WIFI_PASSWORD          "YOUR_PASSWORD"

#define OTA_SERVER_HOST        "https://ota.yourdomain.com"
#define OTA_VERSION_URL        OTA_SERVER_HOST "/api/firmware/latest"
#define OTA_CHECK_INTERVAL_MS  (60UL * 60UL * 1000UL)

#define LED_PIN                2

#define OTA_WEB_PORT           80

// The local web upload endpoint flashes firmware, so it is opt-in rather than on
// by default. Enable it and supply credentials through build_flags:
//
//   build_flags =
//       -D OTA_WEB_ENABLED=1
//       -D OTA_WEB_USER='"someuser"'
//       -D OTA_WEB_PASSWORD='"a-real-password"'
//
// Credentials deliberately have no usable default -- OtaWebServer.cpp refuses to
// compile with OTA_WEB_ENABLED=1 unless both are set. A password committed to
// source is a published password.
#ifndef OTA_WEB_ENABLED
#define OTA_WEB_ENABLED        0
#endif

#ifndef OTA_WEB_USER
#define OTA_WEB_USER           ""
#endif

#ifndef OTA_WEB_PASSWORD
#define OTA_WEB_PASSWORD       ""
#endif
