/*
  test_InterfacePlough - Tests for InterfacePlough: CheckButtons() debounce/combo
  detection, VT soft-key press handling, and Update()'s AUTO/HOLD/MANUAL
  mode-selection decision.
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
#include "InterfacePlough.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfacePlough's constructor needs a real ImplementPlough
// (its header is pulled in directly, not mockable away), which in turn touches
// EEPROM at construction time -- so, like test_ImplementPlough.cpp, every test
// constructs fresh instances after resetAll() re-erases the fake EEPROM,
// rather than sharing one static instance across tests.
// ---------------------------------------------------------------------------
static GuidanceSource   mockGuidance;
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();

    // Fresh gga/vtg/xte fixes (all timestamped at millis()=0, matching "now"),
    // rtk-equivalent quality (SetQuality(4) matches GuidanceSource's own
    // default rtkQuality=4, so IsRtkQuality() reads true), and fast enough for
    // MinSpeed() -- everything Update()'s AUTO-eligibility check needs to pass
    // by default. Individual tests below make exactly one of these stale/bad.
    mockGuidance.SetXte(0);
    mockGuidance.NoteGgaFixReceived();
    mockGuidance.SetSpeedKnots(10.0f);
    mockGuidance.SetQuality(4);
    mockTractor.hitch = false;

    digitalReadValue(LEFT_BUTTON_2, false);
    digitalReadValue(RIGHT_BUTTON_2, false);
    // HIGH ("not manual") by default -- Update()'s manual condition is
    // !digitalRead(MODE_PIN_2), so tests that want the auto/hold ladder need
    // this true; manual-specific tests override it back to false explicitly.
    digitalReadValue(MODE_PIN_2, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfacePlough, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    assertEqual(iface.CheckButtons(0, 0), (short int)0);
}

test(InterfacePlough, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_2, true);
    assertEqual(iface.CheckButtons(0, 0), (short int)-1);
    assertEqual(iface.GetButtons(), (short int)-1);
}

test(InterfacePlough, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(RIGHT_BUTTON_2, true);
    assertEqual(iface.CheckButtons(0, 0), (short int)1);
}

test(InterfacePlough, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_2, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), (short int)0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), (short int)-1);
}

test(InterfacePlough, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_2, true);
    digitalReadValue(RIGHT_BUTTON_2, true);
    assertEqual(iface.CheckButtons(0, 0), (short int)2);
}

test(InterfacePlough, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(LEFT_BUTTON_2, true);
    digitalReadValue(RIGHT_BUTTON_2, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), (short int)0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), (short int)2);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- VT soft-key presses (vtWiderPressed/vtNarrowerPressed/
// vtCalibratePressed). These are consume-once edges from IsobusVtInterface,
// true for exactly one call, so unlike the physical buttons they must act
// without any hold debounce -- and they must lose an arbitration against a
// physical button pressed at the same time.
// ---------------------------------------------------------------------------

test(InterfacePlough, checkButtons_vtWider_actsImmediatelyDespiteDelay) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // delay2=255 would gate a physical LEFT press for 255 ms. A VT press is a
    // single-iteration edge, so gating it that way would drop it entirely --
    // this must return -1 on the very first call, at millis()=0.
    assertEqual(iface.CheckButtons(255, 255, true, false, false), (short int)-1);
    assertEqual(iface.GetButtons(), (short int)-1);
}

test(InterfacePlough, checkButtons_vtNarrower_actsImmediatelyDespiteDelay) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    assertEqual(iface.CheckButtons(255, 255, false, true, false), (short int)1);
    assertEqual(iface.GetButtons(), (short int)1);
}

test(InterfacePlough, checkButtons_vtCalibrate_returnsComboWithoutHoldDuration) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // The regression this pins: Update() calls CheckButtons() with delay1=255,
    // so the physical combo needs 255*4 = 1020 ms of hold. A one-iteration VT
    // edge can never reach that, so vtCalibratePressed needs its own branch --
    // 2 here at millis()=0 is what main.cpp turns into Calibrate().
    assertEqual(iface.CheckButtons(255, 0, false, false, true), (short int)2);
    assertEqual(iface.GetButtons(), (short int)2);
}

test(InterfacePlough, checkButtons_vtPressNotRepeated_returnsZeroOnNextCall) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    assertEqual(iface.CheckButtons(255, 0, true, false, false), (short int)-1);
    // IsobusVtInterface cleared the flag when main.cpp consumed it, so the
    // next iteration passes false and must fall through to the idle branch.
    assertEqual(iface.CheckButtons(255, 0, false, false, false), (short int)0);
    assertEqual(iface.GetButtons(), (short int)0);
}

test(InterfacePlough, checkButtons_physicalLeftBeatsVtNarrower) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // Physical branches are tested first, so a held LEFT wins over a VT
    // Narrower arriving in the same iteration -- the two mean opposite
    // directions and must never be merged into one movement.
    digitalReadValue(LEFT_BUTTON_2, true);
    assertEqual(iface.CheckButtons(0, 0, false, true, false), (short int)-1);
}

test(InterfacePlough, checkButtons_physicalComboBeatsVtCalibrate) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // Both physical buttons held but not yet past delay1*4: the combo branch
    // owns the iteration and returns 0 (still waiting), rather than letting a
    // simultaneous VT Calibrate short-circuit the hold the operator started.
    digitalReadValue(LEFT_BUTTON_2, true);
    digitalReadValue(RIGHT_BUTTON_2, true);
    assertEqual(iface.CheckButtons(10, 0, false, false, true), (short int)0);
}

test(InterfacePlough, checkButtons_vtCalibrateReseedsComboTimer) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // A VT Calibrate sets button1Flag, so button1Timer is re-seeded on the
    // next call. Without that, Calibrate()'s long blocking run would leave
    // button1Timer stale and the first both-held read afterwards would clear
    // delay1*4 instantly, re-entering the wizard the operator just left.
    assertEqual(iface.CheckButtons(10, 0, false, false, true), (short int)2);

    millisValue(5000);  // stands in for time spent inside Calibrate()
    digitalReadValue(LEFT_BUTTON_2, true);
    digitalReadValue(RIGHT_BUTTON_2, true);
    // This call re-seeds button1Timer to 5000 and reports "still waiting".
    assertEqual(iface.CheckButtons(10, 0), (short int)0);
    // Only after a real 40 ms hold does the combo fire again.
    millisValue(5039);
    assertEqual(iface.CheckButtons(10, 0), (short int)0);
    millisValue(5040);
    assertEqual(iface.CheckButtons(10, 0), (short int)2);
}

// ---------------------------------------------------------------------------
// Update() -- AUTO/HOLD/MANUAL mode selection, asserted via GetMode().
// ---------------------------------------------------------------------------

test(InterfacePlough, update_modePinLow_selectsManual) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    digitalReadValue(MODE_PIN_2, false);  // !digitalRead(MODE_PIN_2) -> true -> manual
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfacePlough, update_hitchEngaged_selectsManual) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    mockTractor.hitch = true;
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfacePlough, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // All fix ages "fresh" relative to millis()=0 (0-0=0, not > 2000); rtkQuality
    // and minSpeed both good; not manual (MODE_PIN_2 true, hitch false, per resetAll()).
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfacePlough, update_staleGgaFix_selectsHold) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // gga fix received at t=0 (resetAll's NoteGgaFixReceived()), never refreshed
    // -> stale once millis() reaches 2001. vtg/xte refreshed here to stay fresh.
    millisValue(2001);
    mockGuidance.SetSpeedKnots(10.0f);  // lastVtgFix=2001 (fresh); keeps MinSpeed() true
    mockGuidance.SetXte(0);             // lastXteFix=2001 (fresh)
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlough, update_staleVtgFix_selectsHold) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // vtg fix (SetSpeedKnots) from resetAll() at t=0 is never refreshed here
    // -> stale once millis() reaches 2001. gga/xte refreshed to stay fresh.
    millisValue(2001);
    mockGuidance.NoteGgaFixReceived();  // lastGgaFix=2001 (fresh)
    mockGuidance.SetXte(0);             // lastXteFix=2001 (fresh)
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlough, update_staleXteFix_selectsHold) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    // xte fix (SetXte) from resetAll() at t=0 is never refreshed here -> stale
    // once millis() reaches 2001. gga/vtg refreshed to stay fresh.
    millisValue(2001);
    mockGuidance.NoteGgaFixReceived();  // lastGgaFix=2001 (fresh)
    mockGuidance.SetSpeedKnots(10.0f);  // lastVtgFix=2001 (fresh); keeps MinSpeed() true
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlough, update_notRtkQuality_selectsHold) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    mockGuidance.SetQuality(0);  // 0 != rtkQuality(4) -> IsRtkQuality() false
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfacePlough, update_belowMinSpeed_selectsHold) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    InterfacePlough iface(nullptr, &mockLcd, &impl, &mockTractor, &mockGuidance);

    mockGuidance.SetSpeedKnots(0.0f);  // GetSpeedMs()=0 < MINSPEED(0.5) -> MinSpeed() false
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}
