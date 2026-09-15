/*
  test_InterfaceKipper - Tests for InterfaceKipper: CheckButtons() debounce/combo
  detection, and Update()'s AUTO/HOLD/MANUAL mode-selection decision.
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
#include "InterfaceKipper.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfaceKipper's constructor needs a real
// ImplementKipper (its header is pulled in directly, not mockable away),
// which in turn touches EEPROM at construction time -- so, like
// test_ImplementKipper.cpp, every test constructs fresh instances after
// resetAll() re-erases the fake EEPROM.
// ---------------------------------------------------------------------------
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    analogReadValue(ANGLE_SENS_PIN_4, 428);
    analogReadValue(STEER_SENS_PIN_4, 428);

    mockTractor.minSpeedFlag = true;

    digitalReadValue(LEFT_BUTTON_4, false);
    digitalReadValue(RIGHT_BUTTON_4, false);
    // HIGH ("not manual") by default -- Update()'s manual condition is
    // !digitalRead(MODE_PIN_4), so tests that want the Hold/Auto ladder
    // need it true; manual-specific tests override it back to false.
    digitalReadValue(MODE_PIN_4, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfaceKipper, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfaceKipper, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_4, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfaceKipper, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(RIGHT_BUTTON_4, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfaceKipper, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_4, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfaceKipper, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_4, true);
    digitalReadValue(RIGHT_BUTTON_4, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfaceKipper, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(LEFT_BUTTON_4, true);
    digitalReadValue(RIGHT_BUTTON_4, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), 0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), 2);
}

// ---------------------------------------------------------------------------
// Update() -- AUTO/HOLD/MANUAL mode selection, asserted via GetMode(). Hold
// is reconstructed (see InterfaceKipper::Update()'s own comment) --
// triggered by !tractor->MinSpeed() rather than the legacy source's dead
// GPS-quality check.
// ---------------------------------------------------------------------------

test(InterfaceKipper, update_modePinLow_selectsManual) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(MODE_PIN_4, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceKipper, update_belowMinSpeed_selectsHold) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    mockTractor.minSpeedFlag = false;
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfaceKipper, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    // Not manual (MODE_PIN_4 true per resetAll()), tractor above min speed.
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfaceKipper, update_manualOverridesHold) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);

    digitalReadValue(MODE_PIN_4, false);
    mockTractor.minSpeedFlag = false;
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

// ---------------------------------------------------------------------------
// UpdateScreen(rewrite) per mode and writeValue()'s width and sign cases,
// plus the remaining CheckButtons() branches (NeptuneGPS_Triton#92). The LCD
// stub records nothing, so these pin the mode and button state each screen
// is drawn for and prove the draw paths run.
// ---------------------------------------------------------------------------

test(InterfaceKipper, updateScreen_rewrite_autoHoldManual) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);
    iface.Update();
    assertEqual((int)iface.GetMode(), 0);
    iface.UpdateScreen(true);
    iface.UpdateScreen(false);

    mockTractor.minSpeedFlag = false;
    iface.Update();
    assertEqual((int)iface.GetMode(), 1);
    iface.UpdateScreen(true);

    mockTractor.minSpeedFlag = true;
    digitalReadValue(MODE_PIN_4, false);
    iface.Update();
    assertEqual((int)iface.GetMode(), 2);
    digitalReadValue(LEFT_BUTTON_4, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    iface.UpdateScreen(true);
    digitalReadValue(LEFT_BUTTON_4, false);
    digitalReadValue(RIGHT_BUTTON_4, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
    iface.UpdateScreen(true);
    digitalReadValue(RIGHT_BUTTON_4, false);
    assertEqual(iface.CheckButtons(0, 0), 0);
    iface.UpdateScreen(true);
}

// writeValue() picks a layout per magnitude (three digits, two, one) and
// places the sign in front of the first digit. The offset field takes any
// in-memory value, so it drives all six cases.
test(InterfaceKipper, updateScreen_writeValue_everyWidthAndSign) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);
    const int values[6] = { 150, -150, 42, -42, 7, -7 };
    for (int i = 0; i < 6; ++i) {
        impl.SetOffset(values[i]);
        iface.UpdateScreen(false);
        assertEqual(impl.GetOffset(), values[i]);
    }
}

test(InterfaceKipper, checkButtons_rightHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);
    assertEqual(iface.CheckButtons(0, 50), 0);
    digitalReadValue(RIGHT_BUTTON_4, true);
    millisValue(10);
    assertEqual(iface.CheckButtons(0, 50), 0);
    millisValue(60);
    assertEqual(iface.CheckButtons(0, 50), 1);
}

test(InterfaceKipper, checkButtons_bothHeld_withDelay_thenReleased_returnsZero) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    InterfaceKipper iface(&mockLcd, &impl, &mockTractor);
    assertEqual(iface.CheckButtons(10, 0), 0);
    digitalReadValue(LEFT_BUTTON_4, true);
    digitalReadValue(RIGHT_BUTTON_4, true);
    millisValue(5);
    assertEqual(iface.CheckButtons(10, 0), 0);
    millisValue(50);
    assertEqual(iface.CheckButtons(10, 0), 2);
    digitalReadValue(LEFT_BUTTON_4, false);
    digitalReadValue(RIGHT_BUTTON_4, false);
    assertEqual(iface.CheckButtons(10, 0), 0);
}
