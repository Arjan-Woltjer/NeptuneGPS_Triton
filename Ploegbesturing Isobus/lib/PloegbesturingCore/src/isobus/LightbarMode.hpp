/*
  LightbarMode - when the Ag Leader lightbar emulation runs (NeptuneGPS_Triton#272)
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

#include <stdint.h>
#include <EEPROM.h>

namespace triton
{

// The emulation only makes sense next to an Ag Leader display: on a John
// Deere bus (Mookhoek 57, session 21) it claimed 0xDC as an L160 for nothing.
// Owner, 2026-10-24: a setting, autodetecting wherever possible.
//   Auto  start once the InCommand's display function is on the bus, then
//         keep running until the next boot
//   On    start at boot, as before #272
//   Off   never; switching to Off takes the control function off the bus
// Set in the calibration wizard and on the VT page, read by
// IsobusLightbarChannel.
enum class LightbarMode : uint8_t {
    Auto = 0,
    On   = 1,
    Off  = 2,
};
constexpr uint8_t kHighestLightbarMode = uint8_t(LightbarMode::Off);

// EEPROM 12, in block 1 (GPS + ISOBUS, 10-19): a guidance setting, not a
// plough one, and block 1 keeps its addresses through the #78 layout move.
// 13 stays free so the field can grow to a word.
constexpr int kEepromLightbarMode = 12;

// An erased (255) or out-of-range byte reads as Auto.
inline LightbarMode ReadLightbarMode() {
    const uint8_t stored = EEPROM.read(kEepromLightbarMode);
    return stored <= kHighestLightbarMode ? LightbarMode(stored) : LightbarMode::Auto;
}

inline void WriteLightbarMode(LightbarMode mode) {
    EEPROM.write(kEepromLightbarMode, uint8_t(mode) <= kHighestLightbarMode ? uint8_t(mode) : 0);
}

// Whether the emulation should run: On always, Auto once an Ag Leader display
// has been seen, Off never.
inline bool LightbarShouldRun(LightbarMode mode, bool agLeaderDisplaySeen) {
    return mode == LightbarMode::On || (mode == LightbarMode::Auto && agLeaderDisplaySeen);
}

inline const char* LightbarModeName(LightbarMode mode) {
    return mode == LightbarMode::On ? "ON" : mode == LightbarMode::Off ? "OFF" : "AUTO";
}

}  // namespace triton
