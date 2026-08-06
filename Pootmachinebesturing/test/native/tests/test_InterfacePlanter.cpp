/*
  test_InterfacePlanter - Tests for InterfacePlanter: CheckButtons() debounce/combo
  detection, and Update()'s AUTO/HOLD/MANUAL mode-selection decision (including
  the planting-element-sensor manual trigger, unique to this module).
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
#include "InterfacePlanter.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfacePlanter's constructor needs a real
// ImplementPlanter (its header is pulled in directly, not mockable away),
// which in turn touches EEPROM at construction time -- so, like
// test_ImplementPlanter.cpp, every test constructs fresh instances after
// resetAll() re-erases the fake EEPROM.
// ---------------------------------------------------------------------------
static VehicleGps       mockGps;
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();

    mockGps.xte = 0;
    mockGps.xteFixAge = 0;
    mockGps.ggaFixAge = 0;
    mockGps.vtgFixAge = 0;
    mockGps.quality = 4;
    mockGps.minSpeed = true;
    mockGps.speed = 0;
    mockTractor.speed = 0;

    digitalReadValue(LEFT_BUTTON_3, false);
    digitalReadValue(RIGHT_BUTTON_3, false);
    digitalReadValue(PLANTINGELEMENT_PIN_3, false);
    // HIGH ("not manual") by default -- Update()'s manual condition is
    // !digitalRead(MODE_PIN_3), so tests that want the auto/hold ladder need
    // this true; manual-specific tests override it back to false explicitly.
    digitalReadValue(MODE_PIN_3, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfacePlanter, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfacePlanter, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfacePlanter, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(RIGHT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfacePlanter, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_3, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfacePlanter, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_3, true);
    digitalReadValue(RIGHT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfacePlanter, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_3, true);
    digitalReadValue(RIGHT_BUTTON_3, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), 0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), 2);
}

// ---------------------------------------------------------------------------
// Update() -- AUTO/HOLD/MANUAL mode selection, asserted via GetMode().
// ---------------------------------------------------------------------------

test(InterfacePlanter, update_modePinLow_selectsManual) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(MODE_PIN_3, false);  // !digitalRead(MODE_PIN_3) -> true -> manual
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfacePlanter, update_plantingelementSensor_selectsManual) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    // GetPlantingelement() == digitalRead(PLANTINGELEMENT_PIN_3) ^ invertPlantingelementSensor(false).
    digitalReadValue(PLANTINGELEMENT_PIN_3, true);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfacePlanter, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    // All fix ages "fresh" relative to millis()=0 (0-0=0, not > 2000); quality
    // good (4) and minSpeed good; not manual (MODE_PIN_3 true, planting
    // element sensor false, per resetAll()).
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfacePlanter, update_staleGgaFix_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    millisValue(2001);
    mockGps.ggaFixAge = 0;     // 2001 - 0 = 2001 > 2000 -> stale
    mockGps.vtgFixAge = 2001;  // fresh
    mockGps.xteFixAge = 2001;  // fresh
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_staleVtgFix_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    millisValue(2001);
    mockGps.ggaFixAge = 2001;  // fresh
    mockGps.vtgFixAge = 0;     // stale
    mockGps.xteFixAge = 2001;  // fresh
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_staleXteFix_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    millisValue(2001);
    mockGps.ggaFixAge = 2001;  // fresh
    mockGps.vtgFixAge = 2001;  // fresh
    mockGps.xteFixAge = 0;     // stale
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_qualityNotFour_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    mockGps.quality = 2;
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_belowMinSpeed_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGps);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGps);

    mockGps.minSpeed = false;
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}
