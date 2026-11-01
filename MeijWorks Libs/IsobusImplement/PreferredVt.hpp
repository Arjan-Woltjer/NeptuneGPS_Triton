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
// EEPROM 14, in block 1 (GPS + ISOBUS, 10-19) of the EEPROM map: a VT
// preference is an ISOBUS setting, not a plough one, and block 1 keeps its
// addresses through the #78 layout move. 15 stays free so the field can grow
// to a word.
constexpr int     kEepromPreferredVt  = 14;
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
