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
#pragma once

#if defined(ARDUINO) || defined(EPOXY_DUINO)
#include <Arduino.h>
#else
#include <stdint.h>
#endif

namespace triton
{

// Settings the operator changes between implements or receivers, as opposed
// to the calibration tables ImplementSprayer owns. Persisted in the NVS
// namespace "sprayer_cfg" next to the calibration's "sprayer_cal"; the
// defaults reproduce the behaviour the firmware had while these were
// compile-time constants, so a board without a stored set behaves as before.
struct SprayerSettings {
    int           widthCm           = 300;
    unsigned long guidanceTimeoutMs = 2000;
    uint8_t       gpsBaudIndex      = 7;   // into the 4800 x n table below; 7 = 115200
    uint8_t       gpsMinQuality     = 0;   // GGA quality: 0 any, 1 GPS, 2 DGPS, 4 RTK fixed
};

class ConfigSprayer {
public:
    static constexpr int           kMinWidthCm         = 50;
    static constexpr int           kMaxWidthCm         = 5000;
    static constexpr unsigned long kMinGuidanceMs      = 500;
    static constexpr unsigned long kMaxGuidanceMs      = 10000;
    static constexpr uint8_t       kMaxGpsBaudIndex    = 7;
    static constexpr uint8_t       kGpsQualityRtkFixed = 4;

    // Load() returns true when a stored set was found and applied; otherwise
    // the defaults stay. Both are no-ops off the board.
    bool Load();
    void Save();

    const SprayerSettings& Get() const { return settings; }

    // Each setter validates and returns false, leaving the value unchanged,
    // when the input is out of range. None of them persist; call Save().
    bool SetWidthCm(int cm);
    bool SetGuidanceTimeoutMs(unsigned long ms);
    bool SetGpsBaudIndex(uint8_t index);
    bool SetGpsMinQuality(uint8_t quality);

    // The same 4800 x {1,2,3,4,6,8,12,24} table VehicleGps prints; index 7
    // is the 115200 the port has always been opened at.
    static long BaudFromIndex(uint8_t index);

    // Whether a fix of this GGA quality may be dosed on. Minimum 4 means
    // RTK fixed only: RTK float reports 5, which is numerically higher but
    // a worse fix, so this is not a plain >= comparison.
    bool GuidanceQualityOk(uint8_t quality) const;

private:
    SprayerSettings settings;
};

}  // namespace triton
