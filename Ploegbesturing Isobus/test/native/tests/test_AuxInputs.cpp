/*
  test_AuxInputs - Tests for Breder / Smaller on AUX-N inputs (#254): held
  levels, the timeout and learn mode.
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
#include <AUnit.h>

#include "isobus/AuxInputs.hpp"

using namespace aunit;
using namespace triton;

namespace {
// Boolean momentary value 1 (ISO 11783-6 Annex J)
constexpr uint16_t kReleased = 0;
constexpr uint16_t kPressed  = 1;
constexpr uint16_t kHeld     = 2;
}  // namespace

test(AuxInputs, nothingAssigned_nothingHeld) {
    AuxInputs aux;
    assertFalse(aux.IsWiderHeld(1000));
    assertFalse(aux.IsNarrowerHeld(1000));
    assertFalse(aux.InLearnMode(1000));
}

test(AuxInputs, pressedThenHeld_holdsUntilReleased) {
    AuxInputs aux;
    aux.OnFunctionValue(true, kPressed, 1000);
    assertTrue(aux.IsWiderHeld(1000));
    assertFalse(aux.IsNarrowerHeld(1000));
    aux.OnFunctionValue(true, kHeld, 1200);              // repeats while held
    assertTrue(aux.IsWiderHeld(1700));
    aux.OnFunctionValue(true, kReleased, 1750);
    assertFalse(aux.IsWiderHeld(1750));
}

test(AuxInputs, narrowerIsItsOwnLevel) {
    AuxInputs aux;
    aux.OnFunctionValue(false, kPressed, 1000);
    assertTrue(aux.IsNarrowerHeld(1000));
    assertFalse(aux.IsWiderHeld(1000));
}

// A silent input unit stops the valve, as a lost soft key release does.
test(AuxInputs, noStatusForTheTimeout_counts_asReleased) {
    AuxInputs aux;
    aux.OnFunctionValue(true, kHeld, 1000);
    assertTrue(aux.IsWiderHeld(1000 + AuxInputs::kHeldTimeoutMs));
    assertFalse(aux.IsWiderHeld(1000 + AuxInputs::kHeldTimeoutMs + 1));
}

test(AuxInputs, otherValues_doNotHold) {
    AuxInputs aux;
    aux.OnFunctionValue(true, 3, 1000);
    assertFalse(aux.IsWiderHeld(1000));
    aux.OnFunctionValue(true, 0xFFFF, 1100);
    assertFalse(aux.IsWiderHeld(1100));
}

// Learn mode: the operator is pairing inputs, so nothing acts, also not a
// press that was held when learn mode began, and not for a second after.
test(AuxInputs, learnMode_releasesAndHoldsOff) {
    AuxInputs aux;
    aux.OnFunctionValue(true, kHeld, 1000);
    aux.OnInputStatus(0x01, 1100);
    assertTrue(aux.InLearnMode(1100));
    assertFalse(aux.IsWiderHeld(1100));
    aux.OnFunctionValue(true, kHeld, 1200);
    assertFalse(aux.IsWiderHeld(1200));
    assertFalse(aux.InLearnMode(1100 + AuxInputs::kLearnHoldoffMs));
    aux.OnFunctionValue(true, kHeld, 2200);
    assertTrue(aux.IsWiderHeld(2200));
}

test(AuxInputs, statusWithoutTheLearnBit_changesNothing) {
    AuxInputs aux;
    aux.OnFunctionValue(false, kHeld, 1000);
    aux.OnInputStatus(0x02, 1050);                       // input active, not learn mode
    assertFalse(aux.InLearnMode(1050));
    assertTrue(aux.IsNarrowerHeld(1050));
}
