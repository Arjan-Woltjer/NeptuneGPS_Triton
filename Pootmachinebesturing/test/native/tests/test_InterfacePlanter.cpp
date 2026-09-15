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
static GuidanceSource   mockGuidance;
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();

    // Fresh source, then RTK fixed and a speed above MinSpeed(). Every fix
    // timestamp stays 0 (SetSpeedKnots() stamps at millis()=0), matching the
    // stub this suite was written against; tests that need a fresh fix at a
    // later time stamp it themselves.
    mockGuidance = GuidanceSource();
    mockGuidance.SetQuality(4);
    mockGuidance.SetSpeedKnots(10.0f);
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
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfacePlanter, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfacePlanter, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(RIGHT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfacePlanter, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_3, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfacePlanter, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_3, true);
    digitalReadValue(RIGHT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfacePlanter, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

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
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(MODE_PIN_3, false);  // !digitalRead(MODE_PIN_3) -> true -> manual
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfacePlanter, update_plantingelementSensor_selectsManual) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // GetPlantingelement() == digitalRead(PLANTINGELEMENT_PIN_3) ^ invertPlantingelementSensor(false).
    digitalReadValue(PLANTINGELEMENT_PIN_3, true);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfacePlanter, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // All fix ages "fresh" relative to millis()=0 (0-0=0, not > 2000); quality
    // good (4) and minSpeed good; not manual (MODE_PIN_3 true, planting
    // element sensor false, per resetAll()).
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfacePlanter, update_staleGgaFix_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    millisValue(2001);
    // lastGgaFix stays 0 from resetAll(): 2001 - 0 = 2001 > 2000 -> stale
    mockGuidance.SetSpeedKnots(10.0f);  // lastVtgFix=2001 (fresh); keeps MinSpeed() true
    mockGuidance.SetXte(0);             // lastXteFix=2001 (fresh)
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_staleVtgFix_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    millisValue(2001);
    mockGuidance.NoteGgaFixReceived();  // lastGgaFix=2001 (fresh)
    // lastVtgFix stays 0 from resetAll(): stale
    mockGuidance.SetXte(0);             // lastXteFix=2001 (fresh)
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_staleXteFix_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    millisValue(2001);
    mockGuidance.NoteGgaFixReceived();  // lastGgaFix=2001 (fresh)
    mockGuidance.SetSpeedKnots(10.0f);  // lastVtgFix=2001 (fresh)
    // lastXteFix stays 0 from resetAll(): stale
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_qualityNotFour_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    mockGuidance.SetQuality(2);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlanter, update_belowMinSpeed_selectsHold) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    mockGuidance.SetSpeedKnots(0.0f);  // GetSpeedMs()=0 < MINSPEED(0.5) -> MinSpeed() false
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

// ---------------------------------------------------------------------------
// The guidance-less constructor, UpdateScreen(rewrite) per mode, and the
// remaining CheckButtons() branches (NeptuneGPS_Triton#90). The LCD stub
// records nothing, so these pin the mode and button state each screen is
// drawn for and prove the draw paths run.
// ---------------------------------------------------------------------------

test(InterfacePlanter, fourArgConstructor_startsInManual_withoutGuidance) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor);
    assertEqual((int)iface.GetMode(), 2);
    assertEqual(iface.GetButtons(), 0);
    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfacePlanter, updateScreen_rewrite_autoMode_plantingElementBothStates) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);
    iface.Update();
    assertEqual((int)iface.GetMode(), 0);
    iface.UpdateScreen(true);
    digitalReadValue(PLANTINGELEMENT_PIN_3, true);
    iface.UpdateScreen(true);
    iface.UpdateScreen(false);
    digitalReadValue(PLANTINGELEMENT_PIN_3, false);
}

test(InterfacePlanter, updateScreen_rewrite_holdMode_slowThenStaleSpeed) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);
    mockGuidance.SetSpeedKnots(0.1f);
    iface.Update();
    assertEqual((int)iface.GetMode(), 1);
    iface.UpdateScreen(true);
    millisValue(3000);
    iface.Update();
    assertEqual((int)iface.GetMode(), 1);
    iface.UpdateScreen(true);
}

test(InterfacePlanter, updateScreen_rewrite_manualMode_eachButtonState) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);
    digitalReadValue(MODE_PIN_3, false);
    iface.Update();
    assertEqual((int)iface.GetMode(), 2);

    digitalReadValue(LEFT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    iface.UpdateScreen(true);
    digitalReadValue(LEFT_BUTTON_3, false);
    digitalReadValue(RIGHT_BUTTON_3, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
    iface.UpdateScreen(true);
    digitalReadValue(RIGHT_BUTTON_3, false);
    assertEqual(iface.CheckButtons(0, 0), 0);
    iface.UpdateScreen(true);
}

test(InterfacePlanter, checkButtons_rightHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);
    assertEqual(iface.CheckButtons(0, 50), 0);
    digitalReadValue(RIGHT_BUTTON_3, true);
    millisValue(10);
    assertEqual(iface.CheckButtons(0, 50), 0);
    millisValue(60);
    assertEqual(iface.CheckButtons(0, 50), 1);
}

test(InterfacePlanter, checkButtons_bothHeld_withDelay_thenReleased_returnsZero) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    InterfacePlanter iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);
    assertEqual(iface.CheckButtons(10, 0), 0);
    digitalReadValue(LEFT_BUTTON_3, true);
    digitalReadValue(RIGHT_BUTTON_3, true);
    millisValue(5);
    assertEqual(iface.CheckButtons(10, 0), 0);
    millisValue(50);
    assertEqual(iface.CheckButtons(10, 0), 2);
    digitalReadValue(LEFT_BUTTON_3, false);
    digitalReadValue(RIGHT_BUTTON_3, false);
    assertEqual(iface.CheckButtons(10, 0), 0);
}
