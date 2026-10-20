/*
  test_CalibrationWizard - Tests for the cab-LCD calibration wizard as a step
  runner (#31, #240): its screens, order and buttons as the blocking wizard
  had them, the draft that only COMPLETE's accept applies and stores, and
  that every pass returns, so loop() keeps the CAN stack running.
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
#include <string>

#include <AUnit.h>
#include "calibration/CalibrationPlough.hpp"
#include "EepromStore.hpp"

using namespace aunit;
using namespace triton;

static GuidanceSource  wzGuidance;
static VehicleTractor  wzTractor;
static InterfaceI2CLCD wzLcd;
static unsigned long   wzNow = 0;

static void at(unsigned long ms) {
    wzNow = ms;
    millisValue(ms);
}
static void later(unsigned long ms) { at(wzNow + ms); }

// The cab buttons: - is the left one, + the right one.
static void buttons(bool minus, bool plus) {
    digitalReadValue(LEFT_BUTTON_2, minus);
    digitalReadValue(RIGHT_BUTTON_2, plus);
}

static void resetAll() {
    at(0);
    EEPROM.eepromReset();
    wzGuidance = GuidanceSource();
    wzTractor  = VehicleTractor();
    wzLcd.Clear();
    buttons(false, false);
    digitalReadValue(MODE_PIN_2, true);
    digitalReadValue(PLOUGHSIDE_PIN_2, false);
    analogReadValue(POSITION_SENS_PIN_2, 461);
}

struct Wizard {
    ImplementPlough   impl;
    InterfacePlough   iface;
    CalibrationPlough cal;
    Wizard() : impl(nullptr, &wzGuidance),
               iface(nullptr, &wzLcd, &impl, &wzTractor, &wzGuidance),
               cal(nullptr, &wzLcd, &impl, &wzTractor, &wzGuidance, &iface) {}
    void pass() { cal.Step(); }
};

static std::string row(uint8_t n) { return wzLcd.Row(n); }

// On a title: + accepts. Ends in the edit (or width) screen, buttons up.
static void accept(Wizard& w) {
    buttons(false, false); w.pass();
    buttons(false, true);  w.pass();
    buttons(false, false); w.pass();
}

// On a title: - declines, then the one-second pause. Ends on the next title.
static void decline(Wizard& w) {
    buttons(false, false); w.pass();
    buttons(true, false);  w.pass();
    buttons(false, false); w.pass();
    later(1000);           w.pass();
}

// On an edit screen: one + or - step (held for the 255 ms repeat delay).
static void step(Wizard& w, bool plus) {
    buttons(!plus, plus);  w.pass();
    later(255);            w.pass();
    buttons(false, false); w.pass();
}

// On an edit screen: both held finishes it; then the pause. Ends on the next title.
static void finish(Wizard& w) {
    buttons(true, true);   w.pass();
    buttons(false, false); w.pass();
    later(1000);           w.pass();
}

// Declines titles until the one with this text.
static void skipTo(Wizard& w, const char* title) {
    for (int i = 0; i < 12 && row(0) != std::string(title); i++) decline(w);
}

// Row 3 of an edit screen: its text with the value written in.
static std::string editRow(const char* text, uint8_t col, const std::string& digits) {
    std::string r(text);
    r.replace(col, digits.size(), digits);
    return r;
}

// A macro: AUnit's assertions return from the test they are in.
#define assertTitle(title)                              do {                                                    assertEqual(row(0).c_str(), title);                 assertEqual(row(1).c_str(), L_CAL_ACCEPT);          assertEqual(row(2).c_str(), L_CAL_DECLINE);         assertEqual(row(3).c_str(), L_BLANK);           } while (false)

// AUnit times the whole run with millis(), which these tests move on by
// seconds: the teardown puts the mocked clock back, after a failed
// assertion too, or every later test would count as timed out.
class CalibrationWizard : public TestOnce {
protected:
    void teardown() override {
        at(0);
        TestOnce::teardown();
    }
};

testF(CalibrationWizard, start_opensOnTheWidthScreen_andHoldsTheSession) {
    resetAll();
    Wizard w;
    assertFalse(w.cal.IsActive());
    assertTrue(w.cal.Start());
    assertTrue(w.cal.IsActive());
    assertEqual((int)w.cal.GetSettings().GetOwner(), (int)SettingsOwner::Cab);
    assertTitle(L_CAL_POS);
}

// #260's order, every screen declined; COMPLETE declined saves nothing.
testF(CalibrationWizard, screens_inTheOwnersOrder_declineAll_savesNothing) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    static const char* const order[] = {
        L_CAL_POS, L_CAL_SHARES, L_CAL_SWAP, L_CAL_INVERSION, L_CAL_BYPASS,
        L_CAL_DEADBAND, L_CAL_MAXCOR, L_CAL_VT, L_CAL_QUAL,
    };
    for (const char* title : order) {
        assertTitle(title);
        decline(w);
    }
    assertTitle(L_CAL_COMPLETE);
    buttons(false, false); w.pass();
    buttons(true, false);  w.pass();
    assertEqual(row(1).c_str(), L_CAL_DECLINED);
    assertEqual(row(2).c_str(), L_CAL_NOSAVE);
    buttons(false, false);
    later(1000); w.pass();
    assertFalse(w.cal.IsActive());
    assertEqual((int)w.cal.GetSettings().GetOwner(), (int)SettingsOwner::None);
    for (int a = 0; a < 80; a++) assertEqual((int)EEPROM.read(a), 255);
}

testF(CalibrationWizard, declinedScreen_showsDeclined_forOneSecond) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    buttons(false, false); w.pass();
    buttons(true, false);  w.pass();
    assertEqual(row(1).c_str(), L_CAL_DECLINED);
    assertEqual(row(2).c_str(), L_BLANK);
    buttons(false, false);
    later(999); w.pass();
    assertEqual(row(0).c_str(), L_CAL_POS);
    later(1);   w.pass();
    assertTitle(L_CAL_SHARES);
}

// #247's W1 at the cab: 4 -> 10 shares, saved, moves the width 160 -> 400.
testF(CalibrationWizard, shares_W1_fourToTen_savedAtComplete_movesTheWidth) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_SHARES);
    accept(w);
    assertEqual(row(1).c_str(), L_CAL_ADJUST);
    assertEqual(row(2).c_str(), L_CAL_ENTER);
    assertEqual(row(3).c_str(), editRow(L_CAL_SHARES_AD, 15, "04").c_str());
    for (int i = 0; i < 7; i++) step(w, true);   // one past the top: stays at 10
    assertEqual(row(3).c_str(), editRow(L_CAL_SHARES_AD, 15, "10").c_str());
    assertEqual((int)w.impl.GetShares(), 4);      // staged only
    buttons(true, true); w.pass();
    assertEqual(row(1).c_str(), L_CAL_DONE);
    assertEqual(row(3).c_str(), editRow(L_CAL_SHARES_AD, 15, "10").c_str());
    buttons(false, false); later(1000); w.pass();
    skipTo(w, L_CAL_COMPLETE);
    assertEqual((int)w.impl.GetShares(), 4);
    assertEqual((int)EEPROM.read(64), 255);
    accept(w);
    assertEqual(row(1).c_str(), L_CAL_DDONE);
    assertEqual(row(2).c_str(), L_CAL_SAVE);
    assertEqual((int)w.impl.GetShares(), 10);
    assertEqual((int)w.impl.GetOffset(), 400);
    assertEqual((int)EEPROM.read(64), 10);
    assertEqual((int)EepromStore::ReadInt16Le(66), 400);
    later(1000); w.pass();
    assertFalse(w.cal.IsActive());
}

testF(CalibrationWizard, editsThenCompleteDeclined_leaveThePloughAndEepromAsTheyWere) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_SHARES);
    accept(w);
    step(w, true);
    finish(w);
    skipTo(w, L_CAL_COMPLETE);
    buttons(false, false); w.pass();
    buttons(true, false);  w.pass();
    buttons(false, false); later(1000); w.pass();
    assertFalse(w.cal.IsActive());
    assertEqual((int)w.impl.GetShares(), 4);
    assertEqual((int)EEPROM.read(64), 255);
}

testF(CalibrationWizard, swap_showsTheSideItGives) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_SWAP);
    accept(w);
    assertEqual(row(3).c_str(), editRow(L_CAL_SWAP_AD, 16, "R").c_str());   // input low, no swap
    step(w, true);
    assertEqual(row(3).c_str(), editRow(L_CAL_SWAP_AD, 16, "L").c_str());
    step(w, false);
    assertEqual(row(3).c_str(), editRow(L_CAL_SWAP_AD, 16, "R").c_str());
}

testF(CalibrationWizard, yesNoScreens_showJaNee_andAreSaved) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_INVERSION);
    accept(w);
    assertEqual(row(3).c_str(), L_CAL_NO);
    step(w, true);
    assertEqual(row(3).c_str(), L_CAL_YES);
    finish(w);
    assertTitle(L_CAL_BYPASS);
    accept(w);
    assertEqual(row(3).c_str(), L_CAL_NO);
    step(w, true);
    assertEqual(row(3).c_str(), L_CAL_YES);
    finish(w);
    skipTo(w, L_CAL_COMPLETE);
    accept(w);
    assertTrue(wzTractor.GetInversion());
    assertTrue(w.impl.GetBypass());
    assertEqual((int)EEPROM.read(28), 1);
    assertEqual((int)EEPROM.read(68), 1);
}

// A long + press that accepts a yes/no screen does not also set "ja": the
// edit waits for the buttons to come up first, as the number screens did.
testF(CalibrationWizard, yesNoScreen_longAcceptPress_doesNotSetYes) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_BYPASS);
    buttons(false, false); w.pass();
    buttons(false, true);  w.pass();   // accept
    for (int i = 0; i < 60; i++) {     // held on for 600 ms, as loop() passes
        later(10);
        w.pass();
    }
    assertEqual(row(3).c_str(), L_CAL_NO);
}

testF(CalibrationWizard, maxCorrection_startsOnTheFiveMmGrid_andSaves) {
    resetAll();
    EEPROM.write(60, 27);   // the boot keeps 27; the wizard shows 25
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_MAXCOR);
    accept(w);
    assertEqual(row(3).c_str(), editRow(L_CAL_MAXCOR_AD, 14, "025").c_str());
    step(w, true);
    assertEqual(row(3).c_str(), editRow(L_CAL_MAXCOR_AD, 14, "030").c_str());
    finish(w);
    skipTo(w, L_CAL_COMPLETE);
    accept(w);
    assertEqual((int)EEPROM.read(60), 30);
}

testF(CalibrationWizard, preferredVt_andRtkQuality_oneDigit_andSaved) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    skipTo(w, L_CAL_VT);
    accept(w);
    assertEqual(row(3).c_str(), editRow(L_CAL_VT_AD, 16, "0").c_str());
    step(w, true);
    step(w, true);
    assertEqual(row(3).c_str(), editRow(L_CAL_VT_AD, 16, "2").c_str());
    finish(w);
    assertTitle(L_CAL_QUAL);
    accept(w);
    assertEqual(row(3).c_str(), editRow(L_BLANK, 16, "4").c_str());
    step(w, false);
    assertEqual(row(3).c_str(), editRow(L_BLANK, 16, "2").c_str());
    finish(w);
    assertTitle(L_CAL_COMPLETE);
    accept(w);
    assertEqual((int)EEPROM.read(70), 2);
    assertEqual((int)EEPROM.read(11), 2);
    assertEqual((int)wzGuidance.GetRtkQuality(), 2);
}

// The width procedure: three points, each captured with both held, staged,
// and written by COMPLETE.
static void capture(Wizard& w, int adc) {
    analogReadValue(POSITION_SENS_PIN_2, adc);
    buttons(true, true);   w.pass();
    buttons(false, false); w.pass();
}

testF(CalibrationWizard, width_threePoints_stagedThenSavedAtComplete) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    accept(w);
    assertEqual(row(1).c_str(), L_CAL_ADJUST);
    assertEqual(row(2).c_str(), L_CAL_ENTER);
    const short int target = w.impl.GetPositionCalibrationPoint(0);
    std::string digits;
    digits += target < 0 ? '-' : ' ';
    digits += char(abs(target / 10) + '0');
    digits += char(abs(target) % 10 + '0');
    assertEqual(row(3).c_str(), editRow(L_CAL_POS_AD, 12, digits).c_str());
    capture(w, 610);
    capture(w, 455);
    capture(w, 290);
    assertEqual(row(1).c_str(), L_CAL_DONE);
    assertTrue(w.cal.GetSettings().HasStagedWidthCalibration());
    assertEqual((int)EEPROM.read(40), 255);
    later(1000); w.pass();
    skipTo(w, L_CAL_COMPLETE);
    accept(w);
    assertEqual((int)EepromStore::ReadUint16Be(40), 610);
    assertEqual((int)EepromStore::ReadUint16Be(42), 455);
    assertEqual((int)EepromStore::ReadUint16Be(44), 290);
}

// A capture the boot load would replace (two equal neighbours) is not
// staged: "declined", and COMPLETE writes the width the plough already had.
testF(CalibrationWizard, width_degenerateCapture_isDeclined_andTheOldWidthKept) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    accept(w);
    capture(w, 500);
    capture(w, 500);
    capture(w, 300);
    assertEqual(row(1).c_str(), L_CAL_DECLINED);
    assertFalse(w.cal.GetSettings().HasStagedWidthCalibration());
    later(1000); w.pass();
    skipTo(w, L_CAL_COMPLETE);
    accept(w);
    for (int i = 0; i < 3; i++) {
        assertEqual((int)EepromStore::ReadUint16Be(40 + 2 * i), (int)kDefaultPositionCalibration[i]);
    }
}

testF(CalibrationWizard, width_plusAndMinus_showTheDirection) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    accept(w);
    buttons(false, true); w.pass();
    assertEqual(row(3)[19], '>');
    buttons(true, false); w.pass();
    assertEqual(row(3)[19], '<');
    buttons(false, false); w.pass();
    assertEqual(row(3)[19], ' ');
}

// #31: every pass returns. A held button kept the blocking wizard spinning
// inside Calibrate(); here each Step() comes back while it is held, so
// loop() keeps pumping the CAN stack, the VT and the TC.
testF(CalibrationWizard, step_returnsEveryPass_evenWithAButtonHeld) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.Start());
    buttons(false, true);
    for (int i = 0; i < 1000; i++) {
        later(10);
        w.pass();
    }
    assertTrue(w.cal.IsActive());
    assertEqual(row(0).c_str(), L_CAL_POS);
}

testF(CalibrationWizard, start_isRefused_whileTheVtHoldsTheSession) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.GetSettings().Begin(SettingsOwner::Vt));
    assertFalse(w.cal.Start());
    assertFalse(w.cal.IsActive());
}
