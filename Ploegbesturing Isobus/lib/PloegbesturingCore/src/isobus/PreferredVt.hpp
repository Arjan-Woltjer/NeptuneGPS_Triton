/*
  PreferredVt - the operator's preferred Virtual Terminal (NeptuneGPS_Triton#255)
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

// The setting is the VT number as the terminals show it, which is the VT's
// NAME function instance + 1 (owner, 2026-10-19: the InCommand 1200,
// instance 0, is VT 1; the CNH VT, instance 1, is VT 2). 0 = no preference.
// Set in the calibration wizard, read by IsobusVtInterface.
//
// EEPROM 70 in today's layout: free on a plough board (52-56, the removed
// PWM/KP settings, still hold old values on boards in the field). Block 3's
// 42 after the #78 move.
constexpr int     kEepromPreferredVt  = 70;
constexpr uint8_t kHighestPreferredVt = 9;

// An erased (255) or out-of-range byte reads as no preference.
inline uint8_t ReadPreferredVt() {
    const uint8_t stored = EEPROM.read(kEepromPreferredVt);
    return stored <= kHighestPreferredVt ? stored : 0;
}

inline void WritePreferredVt(uint8_t vtNumber) {
    EEPROM.write(kEepromPreferredVt, vtNumber <= kHighestPreferredVt ? vtNumber : 0);
}

// Whether a control function is the preferred VT: a Virtual Terminal whose
// function instance + 1 is the chosen number.
inline bool IsPreferredVt(bool isVirtualTerminal, uint8_t functionInstance, uint8_t preferredVt) {
    return preferredVt != 0 && isVirtualTerminal && functionInstance + 1 == preferredVt;
}

}  // namespace triton
