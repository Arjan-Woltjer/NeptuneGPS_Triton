/*
  test_InterfaceSprayer - Tests for InterfaceSprayer: CheckButtons() debounce/combo
  detection, and Update()'s Off/Sim/Auto mode-selection decision.
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
#include "InterfaceSprayer.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfaceSprayer's constructor needs a real
// ImplementSprayer (its header is pulled in directly, not mockable away),
// which in turn touches EEPROM at construction time -- so, like
// test_ImplementSprayer.cpp, every test constructs fresh instances after
// resetAll() re-erases the fake EEPROM.
// ---------------------------------------------------------------------------
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();

    mockTractor.speed = 0;
    mockTractor.hitch = true;          // "not off" by default
    mockTractor.simSpeedFlag = true;   // SimSpeed() true -> not the sim condition's first operand
    mockTractor.simTime = 10;          // GetSimTime() -- seconds
    mockTractor.sim = false;
    mockTractor.distance = 0;

    digitalReadValue(LEFT_BUTTON, false);
    digitalReadValue(RIGHT_BUTTON, false);
    // HIGH ("not off") by default -- Update()'s off condition is
    // !digitalRead(MODE_PIN), so tests that want the sim/auto ladder need
    // this true; off-specific tests override it back to false explicitly.
    digitalReadValue(MODE_PIN, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfaceSprayer, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfaceSprayer, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfaceSprayer, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(RIGHT_BUTTON, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfaceSprayer, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfaceSprayer, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON, true);
    digitalReadValue(RIGHT_BUTTON, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfaceSprayer, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON, true);
    digitalReadValue(RIGHT_BUTTON, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), 0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), 2);
}

// ---------------------------------------------------------------------------
// Update() -- Off/Sim/Auto mode selection, asserted via GetMode().
// ---------------------------------------------------------------------------

test(InterfaceSprayer, update_modePinLow_selectsOff) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(MODE_PIN, false);  // !digitalRead(MODE_PIN) -> true -> off
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceSprayer, update_hitchNotEngaged_selectsOff) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    mockTractor.hitch = false;  // !GetHitch() -> true -> off
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceSprayer, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    // Not off (MODE_PIN true, hitch true); SimSpeed() true and GetSim()
    // false, per resetAll() -> neither sim condition holds -> Auto.
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfaceSprayer, update_belowSimSpeedWithinSimTime_selectsSim) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    mockTractor.simSpeedFlag = false;  // !SimSpeed() -> true
    millisValue(500);                  // 500ms < GetSimTime()(10)*1000
    iface.Update();
    assertEqual(iface.GetMode(), (byte)4);
}

test(InterfaceSprayer, update_simFlagForced_selectsSim) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    InterfaceSprayer iface(&mockLcd, &impl, &mockTractor);

    mockTractor.sim = true;  // GetSim() -> true, regardless of SimSpeed()/simTime
    iface.Update();
    assertEqual(iface.GetMode(), (byte)4);
}
