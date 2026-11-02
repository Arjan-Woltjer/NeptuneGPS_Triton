/*
  test_CalibrationScraper - Tests for the scraper's calibration wizard: the
  guidance bytes it owns (the receiver rate index since #82, the RTK quality
  since #289) and the step-runner wizard itself (NeptuneGPS_Triton#289):
  Keuzemenu, the reference points, the settings screens and COMPLETE.
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <string> uses std::min/max with three arguments.
#include <string>

#include <AUnit.h>
#include "calibration/CalibrationScraper.hpp"
#include "EepromStore.hpp"

using namespace aunit;
using namespace triton;

struct CalCaptureStream : public Stream {
    std::string out;
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
};

static GuidanceSource  calGuidance;
static VehicleSensors  calTractor;
static InterfaceI2CLCD calLcd;
static unsigned long   calNow = 0;

static void at(unsigned long ms) {
    calNow = ms;
    millisValue(ms);
}
static void later(unsigned long ms) { at(calNow + ms); }

// The cab buttons: - is the left one, + the right one.
static void buttons(bool minus, bool plus) {
    digitalReadValue(LEFT_BUTTON_5, minus);
    digitalReadValue(RIGHT_BUTTON_5, plus);
}

static void resetAll() {
    at(0);
    EEPROM.eepromReset();
    calGuidance = GuidanceSource();
    calLcd.Clear();
    buttons(false, false);
    digitalReadValue(JOY_LEFT_5, false);
    digitalReadValue(JOY_RIGHT_5, false);
    digitalReadValue(MODE_PIN_5, true);
    digitalReadValue(JOY_MODE_5, true);
}

// ---------------------------------------------------------------------------
// The guidance bytes
// ---------------------------------------------------------------------------

test(CalibrationScraper, eepromSlot_isTheOneVehicleGpsUsed) {
    assertEqual(CalibrationScraper::kEepromGpsBaudIndex, 10);
    assertEqual(CalibrationScraper::kEepromRtkQuality, 11);
}

test(CalibrationScraper, erasedEeprom_keepsIndex0) {
    resetAll();
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
    assertEqual((int)calGuidance.GetRtkQuality(), 4);
}

test(CalibrationScraper, storedByte_restoresTheIndex) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 5);   // 4800 x 8 = 38400
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 5);
}

test(CalibrationScraper, corruptByte_wrapsModuloEight) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 9);
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 1);
    cal.SetGpsBaudIndex(200);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
    cal.SetGpsBaudIndex(7);
    assertEqual((int)cal.GetGpsBaudIndex(), 7);
}

test(CalibrationScraper, constructor_doesNotWrite_commitWritesTheByte) {
    resetAll();
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)EEPROM.read(10), 255);
    cal.SetGpsBaudIndex(3);
    cal.CommitGuidanceCalibration();
    assertEqual((int)EEPROM.read(10), 3);

    CalibrationScraper again(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)again.GetGpsBaudIndex(), 3);
}

// The RTK quality the plough keeps at byte 11: 2 for receivers that report
// RTK as quality 2 (the AutoFarm A5), anything else reads as 4 (#289).
test(CalibrationScraper, rtkQuality_storedTwoIsKept_otherBytesReadAsFour) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromRtkQuality, 2);
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)calGuidance.GetRtkQuality(), 2);

    EEPROM.write(CalibrationScraper::kEepromRtkQuality, 7);
    CalibrationScraper corrupt(&calLcd, &impl, &calGuidance, &iface);
    assertEqual((int)calGuidance.GetRtkQuality(), 4);
}

test(CalibrationScraper, printCalibrationData_reportsTheRateAndTheRtkQuality) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 2);   // 4800 x 3 = 14400
    CalCaptureStream dbg;
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calGuidance, &iface);
    cal.PrintCalibrationData(&dbg);
    assertTrue(dbg.has("Guidance source using following data:"));
    assertTrue(dbg.has("Baudrate\n14400\n"));
    assertTrue(dbg.has("RTK Quality\n4\n"));
}

// ---------------------------------------------------------------------------
// The step-runner wizard (#289)
// ---------------------------------------------------------------------------

struct Wizard {
    ImplementScraper   impl;
    InterfaceScraper   iface;
    CalibrationScraper cal;
    Wizard() : impl(&calGuidance),
               iface(&calLcd, &impl, &calTractor, &calGuidance),
               cal(&calLcd, &impl, &calGuidance, &iface) {}
    void pass() { cal.Step(); }
};

static std::string row(uint8_t n) { return calLcd.Row(n); }

// On a title: + accepts. Ends in the edit screen, buttons up.
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

// On Keuzemenu: + (references) or - (calibrate).
static void choose(Wizard& w, bool plus) {
    buttons(false, false); w.pass();
    buttons(!plus, plus);  w.pass();
    buttons(false, false); w.pass();
}

// Declines titles until the one with this text.
static void skipTo(Wizard& w, const char* title) {
    for (int i = 0; i < 12 && row(0) != std::string(title); i++) decline(w);
}

test(CalibrationScraper, start_opensKeuzemenuAndStopsTheValves) {
    resetAll();
    Wizard w;
    w.impl.Adjust(2, 5);                       // manual, driving
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 255);
    assertTrue(w.cal.Start());
    assertTrue(w.cal.IsActive());
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 0);
    assertEqual(row(0).c_str(), L_CHOICE);
    assertEqual(row(1).c_str(), L_REF);
    assertEqual(row(2).c_str(), L_CAL);
}

// The VT page (#292) holding the settings keeps the cab wizard closed.
test(CalibrationScraper, start_isRefusedWhileTheVtHoldsTheSettings) {
    resetAll();
    Wizard w;
    assertTrue(w.cal.GetSettings().Begin(SettingsOwner::Vt));
    assertFalse(w.cal.Start());
    assertFalse(w.cal.IsActive());
}

// + on Keuzemenu: reference A, then B, each captured on + from the live
// position; then the wizard closes.
test(CalibrationScraper, referenceBranch_capturesAThenBAndCloses) {
    resetAll();
    Wizard w;
    w.cal.Start();
    choose(w, true);
    assertEqual(row(0).c_str(), L_CAL_REF);

    calGuidance.SetPosition(52.0f, 5.0f);
    calGuidance.SetAltitude(10.0f);
    accept(w);
    assertEqual(row(1).c_str(), L_CAL_REF_AD);
    assertNotEqual((int)EEPROM.read(150), 255);
    later(1000); w.pass();

    std::string titleB(L_CAL_REF);
    titleB[16] = 'B';
    assertEqual(row(0).c_str(), titleB.c_str());
    calGuidance.SetPosition(52.0009f, 5.0f);
    accept(w);
    assertNotEqual((int)EEPROM.read(160), 255);
    later(1000); w.pass();
    assertFalse(w.cal.IsActive());
}

// - on Keuzemenu: the settings screens. A slope of +3 saved on COMPLETE
// reaches the implement and EEPROM; the reference points stay (owner, #289).
test(CalibrationScraper, calibrateBranch_savesTheSlopeAndKeepsTheReferences) {
    resetAll();
    Wizard w;
    calGuidance.SetPosition(52.0f, 5.0f);
    calGuidance.SetAltitude(10.0f);
    w.impl.SetRefA();
    const int refByte = EEPROM.read(150);

    w.cal.Start();
    choose(w, false);
    assertEqual(row(0).c_str(), L_CAL_SLOPE);
    accept(w);
    step(w, true);
    step(w, true);
    step(w, true);
    assertEqual(row(3).substr(13, 4).c_str(), " 003");
    finish(w);
    skipTo(w, L_CAL_COMPLETE);
    accept(w);
    assertEqual(row(2).c_str(), L_CAL_SAVE);
    assertEqual((int)w.impl.GetSlope(), 3);
    assertEqual((int)EepromStore::ReadInt16Le(146), 3);
    assertEqual((int)EEPROM.read(150), refByte);
    later(1000); w.pass();
    assertFalse(w.cal.IsActive());
}

// COMPLETE declined: nothing is applied or stored.
test(CalibrationScraper, completeDeclined_storesNothing) {
    resetAll();
    Wizard w;
    w.cal.Start();
    choose(w, false);
    accept(w);
    step(w, false);
    finish(w);
    skipTo(w, L_CAL_COMPLETE);
    decline(w);
    assertEqual((int)w.impl.GetSlope(), 0);
    assertEqual((int)EEPROM.read(146), 255);
    assertFalse(w.cal.IsActive());
}

// The table's bounds hold on the cab: the old wizard counted the slope and
// the margin without any limit, past what the boot load keeps (#163 class).
test(CalibrationScraper, edits_stopAtTheTableBounds) {
    resetAll();
    Wizard w;
    w.impl.SetError(10);
    w.cal.Start();
    choose(w, false);
    decline(w);                                // slope
    assertEqual(row(0).c_str(), L_CAL_MARGIN);
    accept(w);
    step(w, true);                             // 10 is the highest
    assertEqual(row(3).substr(15, 2).c_str(), "10");
    finish(w);
    assertEqual((int)w.cal.GetSettings().Get(ScraperSetting::ErrorMargin), 10);
}

// The swap and the speed gate are yes/no screens: + is yes, - is no.
test(CalibrationScraper, yesNoScreens_setTheSwapAndTheGate) {
    resetAll();
    Wizard w;
    w.cal.Start();
    choose(w, false);
    skipTo(w, L_CAL_SWAP);
    accept(w);
    assertEqual(row(3).c_str(), L_CAL_NO);
    step(w, true);
    assertEqual(row(3).c_str(), L_CAL_YES);
    finish(w);
    assertEqual(row(0).c_str(), L_CAL_SPEEDGATE);
    accept(w);
    assertEqual(row(3).c_str(), L_CAL_YES);   // on by default
    step(w, false);
    finish(w);
    skipTo(w, L_CAL_COMPLETE);
    accept(w);
    assertTrue(w.impl.GetSwap());
    assertFalse(w.impl.GetSpeedGate());
}
