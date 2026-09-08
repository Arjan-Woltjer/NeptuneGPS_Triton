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
#pragma once

#include <Arduino.h>

#include <nvs.h>

class OtaManager {
public:
    OtaManager();

    void SetSelfTestCallback(bool (*callback)());
    void Begin();
    void Update();

private:
    struct FirmwareInfo {
        int    version;
        String url;
        String sha256;
        String notes;
        bool   valid;
    };

    enum Result  { SUCCESS, DOWNLOAD_FAIL, INVALID_IMAGE, PARTITION_ERR, FLASH_ERR,
                   UNTRUSTED_URL, HASH_MISMATCH };
    enum LedMode { LED_OFF, LED_ON, LED_BLINK_SLOW, LED_BLINK_FAST };

    bool   fetchFirmwareInfo(FirmwareInfo& info);
    Result performOta(const FirmwareInfo& fw);
    void   validateBoot();
    void   checkForUpdate();
    void   setLed(LedMode mode);
    void   updateLed();
    String getDeviceId();

    // The download URL arrives in the server's JSON, so it is attacker-controlled
    // the moment that server is. Both are static -- no instance state.
    static bool   trustedUrl(const String& url);
    static String toHex(const uint8_t* bytes, size_t len);

    bool (*selfTestCallback)();
    nvs_handle_t  nvs;
    bool          otaInProgress;
    LedMode       ledMode;
    bool          ledState;
    unsigned long ledLastToggle;
    unsigned long lastOtaCheck;
};
