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

#include "HeldKey.hpp"

namespace triton
{

// AUX-N (ISO 11783-6 Annex J): the pool declares Breder and Smaller as
// Auxiliary Function Type 2 objects, the operator pairs each with an input of
// an input unit (joystick, armrest) on the VT's assignment screen, and the
// input unit reports the input in its Auxiliary Input Type 2 Status message.
// AgIsoStack passes value 1 on together with the function it is assigned to.
//
// Both functions are Boolean momentary: value 1 is 0 released, 1 pressed,
// 2 held. Held levels by the soft keys' rule (HeldKey, #245, #320): a pressed
// status starts the hold, held statuses keep it alive but never start it, and
// a released status, kHeldTimeoutMs without one, or a lost VT ends it, so a
// silent input unit stops the valve. An input unit repeats the status of an
// input in use at least every 200 ms (AgIsoStack's own input side: every
// 50 ms; the CNH armrest in session 23 sent 1 first, then 2).
//
// Nothing acts while an input unit reports learn mode: the operator is
// pairing inputs then. In learn mode a unit addresses its status to the VT,
// so assigned inputs normally fall silent anyway; the bit is checked on every
// status that does arrive, and holds everything off for kLearnHoldoffMs.
class AuxInputs {
public:
    static constexpr unsigned long kHeldTimeoutMs  = HeldKey::kTimeoutMs;
    static constexpr unsigned long kLearnHoldoffMs = 1000;

    // A status for an input assigned to Breder (wider) or Smaller. Values
    // other than 1 and 2 count as released. Nothing starts in learn mode.
    HeldKey::Change OnFunctionValue(bool wider, uint16_t value1, unsigned long nowMs) {
        HeldKey& key = wider ? widerKey : narrowerKey;
        if (InLearnMode(nowMs)) {
            key.Drop();
            return HeldKey::Change::None;
        }
        const HeldKey::Event event = value1 == 1   ? HeldKey::Event::Press
                                     : value1 == 2 ? HeldKey::Event::StillHeld
                                                   : HeldKey::Event::Release;
        return key.On(event, nowMs);
    }

    // The operating-state byte of any Auxiliary Input Type 2 Status: bit 0 is
    // learn mode.
    void OnInputStatus(uint8_t operatingState, unsigned long nowMs) {
        if (operatingState & 0x01) {
            learnSeen = true;
            learnMs = nowMs;
            Drop();
        }
    }

    // The VT the inputs were assigned on went away.
    void Drop() {
        widerKey.Drop();
        narrowerKey.Drop();
    }

    bool InLearnMode(unsigned long nowMs) const { return learnSeen && nowMs - learnMs < kLearnHoldoffMs; }
    bool IsWiderHeld(unsigned long nowMs) const { return widerKey.IsHeld(nowMs) && !InLearnMode(nowMs); }
    bool IsNarrowerHeld(unsigned long nowMs) const { return narrowerKey.IsHeld(nowMs) && !InLearnMode(nowMs); }

private:
    HeldKey       widerKey;
    HeldKey       narrowerKey;
    bool          learnSeen = false;
    unsigned long learnMs = 0;
};

}  // namespace triton
