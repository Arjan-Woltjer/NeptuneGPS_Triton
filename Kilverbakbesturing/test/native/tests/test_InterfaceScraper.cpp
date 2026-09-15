/*
  test_InterfaceScraper - Tests for InterfaceScraper: CheckButtons() debounce/combo
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
#include "InterfaceScraper.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfaceScraper's constructor needs a real
// ImplementScraper (its header is pulled in directly, not mockable away),
// which in turn touches EEPROM at construction time -- so, like
// test_ImplementScraper.cpp, every test constructs fresh instances after
// resetAll() re-erases the fake EEPROM.
// ---------------------------------------------------------------------------
static GuidanceSource   mockGuidance;
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();

    // Fresh source (no fix ever, position 0/0, altitude 0), then a speed
    // above MinSpeed(); its VTG stamp lands at millis()=0, matching the stub
    // this suite was written against.
    mockGuidance = GuidanceSource();
    mockGuidance.SetSpeedKnots(10.0f);

    digitalReadValue(LEFT_BUTTON_5, false);
    digitalReadValue(RIGHT_BUTTON_5, false);
    digitalReadValue(JOY_LEFT_5, false);
    digitalReadValue(JOY_RIGHT_5, false);
    // HIGH ("not manual") by default -- Update()'s manual condition is
    // !digitalRead(MODE_PIN_5) || !digitalRead(JOY_MODE_5), so tests that
    // want the auto/hold ladder need both true; manual-specific tests
    // override them back to false explicitly.
    digitalReadValue(MODE_PIN_5, true);
    digitalReadValue(JOY_MODE_5, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfaceScraper, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfaceScraper, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfaceScraper, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(RIGHT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfaceScraper, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_5, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfaceScraper, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_5, true);
    digitalReadValue(RIGHT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfaceScraper, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_5, true);
    digitalReadValue(RIGHT_BUTTON_5, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), 0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), 2);
}

// ---------------------------------------------------------------------------
// Update() -- AUTO/HOLD/MANUAL mode selection, asserted via GetMode().
// ---------------------------------------------------------------------------

test(InterfaceScraper, update_modePinLow_selectsManual) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(MODE_PIN_5, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceScraper, update_joyModeLow_selectsManual) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(JOY_MODE_5, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceScraper, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    // All fix ages "fresh" relative to millis()=0 (0-0=0, not > 2000); not
    // manual (MODE_PIN_5/JOY_MODE_5 both true per resetAll()).
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfaceScraper, update_staleGgaFix_selectsHold) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    millisValue(2001);
    // lastGgaFix stays 0 from resetAll(): 2001 - 0 = 2001 > 2000 -> stale
    mockGuidance.SetSpeedKnots(10.0f);  // lastVtgFix=2001 (fresh)
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfaceScraper, update_staleVtgFix_selectsHold) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);

    millisValue(2001);
    mockGuidance.NoteGgaFixReceived();  // lastGgaFix=2001 (fresh)
    // lastVtgFix stays 0 from resetAll(): stale
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

// ---------------------------------------------------------------------------
// UpdateScreen(rewrite) per mode, the joystick inputs and the remaining
// CheckButtons() branches (NeptuneGPS_Triton#91). The LCD stub records
// nothing, so these pin the mode and button state each screen is drawn for
// and prove the draw paths run.
// ---------------------------------------------------------------------------

test(InterfaceScraper, updateScreen_rewrite_autoMode) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    mockGuidance.NoteGgaFixReceived();
    iface.Update();
    assertEqual((int)iface.GetMode(), 0);
    iface.UpdateScreen(true);
    iface.UpdateScreen(false);
}

test(InterfaceScraper, updateScreen_rewrite_holdMode_movingAndStandingStill) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    millisValue(3000);                          // both fixes stale: HOLD
    iface.Update();
    assertEqual((int)iface.GetMode(), 1);
    iface.UpdateScreen(true);                   // MinSpeed() true: the "still moving" hint
    mockGuidance.SetSpeedKnots(0.1f);
    iface.UpdateScreen(true);                   // standing still
}

test(InterfaceScraper, updateScreen_rewrite_manualMode_eachButtonState) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    digitalReadValue(MODE_PIN_5, false);
    iface.Update();
    assertEqual((int)iface.GetMode(), 2);
    digitalReadValue(LEFT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    iface.UpdateScreen(true);
    digitalReadValue(LEFT_BUTTON_5, false);
    digitalReadValue(RIGHT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
    iface.UpdateScreen(true);
    digitalReadValue(RIGHT_BUTTON_5, false);
    assertEqual(iface.CheckButtons(0, 0), 0);
    iface.UpdateScreen(true);
}

test(InterfaceScraper, joystick_leftAndRight_actLikeTheButtons) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    digitalReadValue(JOY_LEFT_5, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    digitalReadValue(JOY_LEFT_5, false);
    digitalReadValue(JOY_RIGHT_5, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
    digitalReadValue(JOY_RIGHT_5, false);
    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfaceScraper, checkButtons_rightHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    assertEqual(iface.CheckButtons(0, 50), 0);
    digitalReadValue(RIGHT_BUTTON_5, true);
    millisValue(10);
    assertEqual(iface.CheckButtons(0, 50), 0);
    millisValue(60);
    assertEqual(iface.CheckButtons(0, 50), 1);
}

test(InterfaceScraper, checkButtons_bothHeld_withDelay_thenReleased_returnsZero) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    assertEqual(iface.CheckButtons(10, 0), 0);
    digitalReadValue(LEFT_BUTTON_5, true);
    digitalReadValue(RIGHT_BUTTON_5, true);
    millisValue(5);
    assertEqual(iface.CheckButtons(10, 0), 0);
    millisValue(50);
    assertEqual(iface.CheckButtons(10, 0), 2);
    digitalReadValue(LEFT_BUTTON_5, false);
    digitalReadValue(RIGHT_BUTTON_5, false);
    assertEqual(iface.CheckButtons(10, 0), 0);
}

// The screen renders reference height, height, distance and slope digit by
// digit with separate branches for the sign and for five-digit values.
// Drive them with a high reference, a deep negative one and a far B point.
test(InterfaceScraper, updateScreen_rendersLargeAndNegativeValues) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGuidance);
    impl.SetSlope(-12);
    mockGuidance.SetPosition(52.0f, 5.0f);
    mockGuidance.SetAltitude(150.0f);              // 15000 cm: five digits
    mockGuidance.NoteGgaFixReceived();
    impl.SetRefA();
    mockGuidance.SetPosition(52.02f, 5.0f);        // ~2.2 km north
    mockGuidance.SetAltitude(-150.0f);
    mockGuidance.NoteGgaFixReceived();
    impl.SetRefB();
    millisValue(1);
    mockGuidance.SetPosition(52.019f, 5.001f);
    mockGuidance.SetAltitude(-140.0f);
    mockGuidance.NoteGgaFixReceived();
    iface.Update();
    iface.UpdateScreen(true);
    millisValue(2);
    mockGuidance.SetPosition(52.001f, 4.999f);
    mockGuidance.SetAltitude(140.0f);
    mockGuidance.NoteGgaFixReceived();
    iface.Update();
    iface.UpdateScreen(true);
    assertEqual(impl.GetSlope(), (short int)-12);
}
