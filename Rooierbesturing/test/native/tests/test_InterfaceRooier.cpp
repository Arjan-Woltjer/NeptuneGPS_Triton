/*
  test_InterfaceRooier - Tests for InterfaceRooier: CheckButtons() debounce/combo
  detection, and Update()'s AUTO/MANUAL mode-selection decision.
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
#include "InterfaceRooier.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfaceRooier's constructor needs a real
// ImplementRooier (its header is pulled in directly, not mockable away),
// which in turn touches EEPROM at construction time -- so, like
// test_ImplementRooier.cpp, every test constructs fresh instances after
// resetAll() re-erases the fake EEPROM.
// ---------------------------------------------------------------------------
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    analogReadValue(HEIGHT_SENS_PIN_L_8, 0);
    analogReadValue(HEIGHT_SENS_PIN_R_8, 0);

    mockTractor.hitch = false;

    digitalReadValue(LEFT_BUTTON_8, false);
    digitalReadValue(RIGHT_BUTTON_8, false);
    digitalReadValue(JOY_LEFT_8, false);
    digitalReadValue(JOY_RIGHT_8, false);
    // HIGH ("not manual") by default -- Update()'s manual condition is
    // !digitalRead(MODE_PIN_8) || !digitalRead(JOY_MODE_8) || GetHitch(),
    // so tests that want the auto ladder need both pins true and hitch
    // false; manual-specific tests override these back explicitly.
    digitalReadValue(MODE_PIN_8, true);
    digitalReadValue(JOY_MODE_8, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfaceRooier, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfaceRooier, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_8, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfaceRooier, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(RIGHT_BUTTON_8, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfaceRooier, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_8, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfaceRooier, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_8, true);
    digitalReadValue(RIGHT_BUTTON_8, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfaceRooier, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_8, true);
    digitalReadValue(RIGHT_BUTTON_8, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), 0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), 2);
}

// ---------------------------------------------------------------------------
// Update() -- AUTO/MANUAL mode selection, asserted via GetMode(). No Hold
// mode exists in this module -- GPS was dropped for this pass, so the only
// two states are Auto (mode 0) and Manual (mode 2).
// ---------------------------------------------------------------------------

test(InterfaceRooier, update_modePinLow_selectsManual) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(MODE_PIN_8, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceRooier, update_joyModeLow_selectsManual) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(JOY_MODE_8, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceRooier, update_hitchEngaged_selectsManual) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    mockTractor.hitch = true;
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceRooier, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementRooier impl;
    InterfaceRooier iface(&mockLcd, &impl, &mockTractor);

    // Not manual (MODE_PIN_8/JOY_MODE_8 both true, hitch disengaged, per
    // resetAll()).
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}
