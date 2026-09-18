/*
  ConfigSprayer - operator-adjustable settings for the MeijWorks loofdoes
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
#include "ConfigSprayer.hpp"

#ifdef ARDUINO
#include <Preferences.h>
#include <esp_system.h>
#endif

namespace triton
{

namespace {
// The 4800 x n rate table the Triton receivers have always been configured
// with (it came from VehicleGps, which Spuitcomputer LD no longer uses); the index
// is stored in NVS with the other settings, never in EEPROM.
constexpr uint8_t kBaudMultipliers[ConfigSprayer::kMaxGpsBaudIndex + 1] = { 1, 2, 3, 4, 6, 8, 12, 24 };
constexpr long    kBaudBase = 4800;

constexpr const char* kNamespace  = "sprayer_cfg";
constexpr const char* kKeyWidth   = "width_cm";
constexpr const char* kKeyGuid    = "guid_ms";
constexpr const char* kKeyBaud    = "gps_baud";
constexpr const char* kKeyMinQ    = "gps_minq";
constexpr const char* kKeyBuzzer  = "buzzer";
constexpr const char* kKeyPasskey = "passkey";
}  // namespace

bool ConfigSprayer::SetWidthCm(int cm) {
    if (cm < kMinWidthCm || cm > kMaxWidthCm) return false;
    settings.widthCm = cm;
    return true;
}

bool ConfigSprayer::SetGuidanceTimeoutMs(unsigned long ms) {
    if (ms < kMinGuidanceMs || ms > kMaxGuidanceMs) return false;
    settings.guidanceTimeoutMs = ms;
    return true;
}

bool ConfigSprayer::SetGpsBaudIndex(uint8_t index) {
    if (index > kMaxGpsBaudIndex) return false;
    settings.gpsBaudIndex = index;
    return true;
}

bool ConfigSprayer::SetGpsMinQuality(uint8_t quality) {
    // Only the levels that mean something as a threshold. 5 (RTK float) is a
    // fix a receiver reports, not a bar the operator sets; 3 (PPS) and above
    // 5 never occur on the receivers this runs with.
    if (quality != 0 && quality != 1 && quality != 2 && quality != kGpsQualityRtkFixed) return false;
    settings.gpsMinQuality = quality;
    return true;
}

void ConfigSprayer::SetBuzzerEnabled(bool enabled) {
    settings.buzzerEnabled = enabled;
}

long ConfigSprayer::BaudFromIndex(uint8_t index) {
    // Out of range can only come from a corrupt store; 115200 is what every
    // board in the field has always been opened at, so that is the safe rate.
    if (index > kMaxGpsBaudIndex) index = kMaxGpsBaudIndex;
    return kBaudBase * (long)kBaudMultipliers[index];
}

bool ConfigSprayer::GuidanceQualityOk(uint8_t quality) const {
    const uint8_t minimum = settings.gpsMinQuality;
    if (minimum == 0) return true;
    if (minimum == kGpsQualityRtkFixed) return quality == kGpsQualityRtkFixed;
    return quality >= minimum;
}

#ifdef ARDUINO
bool ConfigSprayer::Load() {
    Preferences prefs;
    prefs.begin(kNamespace, true);
    const bool stored = prefs.isKey(kKeyWidth);
    if (stored) {
        // Route every stored value through its setter so a corrupt or
        // out-of-range entry falls back to the default instead of being
        // trusted.
        SetWidthCm(prefs.getInt(kKeyWidth, settings.widthCm));
        SetGuidanceTimeoutMs(prefs.getULong(kKeyGuid, settings.guidanceTimeoutMs));
        SetGpsBaudIndex(prefs.getUChar(kKeyBaud, settings.gpsBaudIndex));
        SetGpsMinQuality(prefs.getUChar(kKeyMinQ, settings.gpsMinQuality));
        SetBuzzerEnabled(prefs.getBool(kKeyBuzzer, settings.buzzerEnabled));
    }
    const bool havePasskey = prefs.isKey(kKeyPasskey);
    if (havePasskey) settings.passkey = prefs.getUInt(kKeyPasskey, settings.passkey);
    prefs.end();
    if (!havePasskey) {
        // First boot: draw the code once and keep it, so it survives every
        // later Save() and a phone stays bonded across reboots.
        settings.passkey = 100000UL + (esp_random() % 900000UL);
        Preferences rw;
        rw.begin(kNamespace, false);
        rw.putUInt(kKeyPasskey, settings.passkey);
        rw.end();
    }
    return stored;
}

void ConfigSprayer::Save() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putInt(kKeyWidth, settings.widthCm);
    prefs.putULong(kKeyGuid, settings.guidanceTimeoutMs);
    prefs.putUChar(kKeyBaud, settings.gpsBaudIndex);
    prefs.putUChar(kKeyMinQ, settings.gpsMinQuality);
    prefs.putBool(kKeyBuzzer, settings.buzzerEnabled);
    prefs.end();
}
#else
bool ConfigSprayer::Load() { return false; }
void ConfigSprayer::Save() {}
#endif

}  // namespace triton
