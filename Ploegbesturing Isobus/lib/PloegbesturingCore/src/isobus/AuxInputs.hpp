/*
  AuxInputs - Breder / Smaller on joystick or armrest inputs (NeptuneGPS_Triton#254)
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

namespace triton
{

// AUX-N (ISO 11783-6 Annex J): the pool declares Breder and Smaller as
// Auxiliary Function Type 2 objects, the operator pairs each with an input of
// an input unit (joystick, armrest) on the VT's assignment screen, and the
// input unit reports the input in its Auxiliary Input Type 2 Status message.
// AgIsoStack passes value 1 on together with the function it is assigned to.
//
// Both functions are Boolean momentary: value 1 is 0 released, 1 pressed,
// 2 held. Held levels like the soft keys (#245): held from a pressed or held
// status until a released one, and for at most kHeldTimeoutMs after the last
// status, so a silent input unit stops the valve. An input unit repeats the
// status of an input in use at least every 200 ms (AgIsoStack's own input
// side: every 50 ms).
//
// Nothing acts while an input unit reports learn mode: the operator is
// pairing inputs then. In learn mode a unit addresses its status to the VT,
// so assigned inputs normally fall silent anyway; the bit is checked on every
// status that does arrive, and holds everything off for kLearnHoldoffMs.
class AuxInputs {
public:
    static constexpr unsigned long kHeldTimeoutMs  = 600;
    static constexpr unsigned long kLearnHoldoffMs = 1000;

    // A status for an input assigned to Breder (wider) or Smaller.
    void OnFunctionValue(bool wider, uint16_t value1, unsigned long nowMs) {
        Level& level = wider ? widerLevel : narrowerLevel;
        level.held = value1 == 1 || value1 == 2;
        level.eventMs = nowMs;
    }

    // The operating-state byte of any Auxiliary Input Type 2 Status: bit 0 is
    // learn mode.
    void OnInputStatus(uint8_t operatingState, unsigned long nowMs) {
        if (operatingState & 0x01) {
            learnSeen = true;
            learnMs = nowMs;
            widerLevel.held = false;
            narrowerLevel.held = false;
        }
    }

    bool InLearnMode(unsigned long nowMs) const { return learnSeen && nowMs - learnMs < kLearnHoldoffMs; }
    bool IsWiderHeld(unsigned long nowMs) const { return isHeld(widerLevel, nowMs); }
    bool IsNarrowerHeld(unsigned long nowMs) const { return isHeld(narrowerLevel, nowMs); }

private:
    struct Level {
        bool          held = false;
        unsigned long eventMs = 0;
    };

    bool isHeld(const Level& level, unsigned long nowMs) const {
        return level.held && nowMs - level.eventMs <= kHeldTimeoutMs && !InLearnMode(nowMs);
    }

    Level         widerLevel;
    Level         narrowerLevel;
    bool          learnSeen = false;
    unsigned long learnMs = 0;
};

}  // namespace triton
