/*
  test_IsobusVtCalibration - Tests for the calibration page on the VT (#240):
  the Calibrate key opens it on the settings draft, the VT's inputs edit the
  draft, Opslaan saves and Annuleren drops it, and a lost VT drops it too.
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
#include <vector>

#include <AUnit.h>
#include "calibration/CalibrationPlough.hpp"
#include "isobus/IsobusVtCalibration.hpp"
#include "isobus/VTObjectPool.hpp"

using namespace aunit;
using namespace triton;

namespace {

struct PageCaptureStream : public Stream {
    std::string out;
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
};

// What the page sent, and whether the VT client would have queued it.
struct FakeVtOutput : public VtCalibrationOutput {
    std::vector<uint16_t> masks;
    std::vector<std::pair<uint16_t, uint32_t>> values;
    bool accept = true;
    bool ChangeActiveMask(uint16_t mask) override {
        if (!accept) return false;
        masks.push_back(mask);
        return true;
    }
    bool ChangeNumericValue(uint16_t objectId, uint32_t value) override {
        if (!accept) return false;
        values.push_back({ objectId, value });
        return true;
    }
    // The last value sent to an object, or -1.
    long last(uint16_t objectId) const {
        for (auto it = values.rbegin(); it != values.rend(); ++it) {
            if (it->first == objectId) return long(it->second);
        }
        return -1;
    }
    void clear() { masks.clear(); values.clear(); }
};

GuidanceSource    pgGuidance;
VehicleTractor    pgTractor;
InterfaceI2CLCD   pgLcd;
PageCaptureStream pgDebug;

void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    pgGuidance = GuidanceSource();
    pgTractor  = VehicleTractor();
    pgDebug.out.clear();
    digitalReadValue(PLOUGHSIDE_PIN_2, false);
}

// The plough as main.cpp builds it, with the page on the wizard's settings.
struct Rig {
    ImplementPlough     impl;
    CalibrationPlough   cal;
    IsobusVtCalibration page;
    FakeVtOutput        out;
    Rig() : impl(nullptr, &pgGuidance),
            cal(&pgDebug, &pgLcd, &impl, &pgTractor, &pgGuidance, nullptr),
            page(&pgDebug, &cal.GetSettings(), &impl) {}
    PloughSettings& settings() { return cal.GetSettings(); }
};

uint16_t input(PloughSetting id) { return uint16_t(Input_Cal0 + uint8_t(id)); }

}  // namespace

test(IsobusVtCalibration, calibrateKey_opensThePage_onTheDraft_andShowsEveryValue) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    assertTrue(r.page.IsOpen());
    assertEqual((int)r.settings().GetOwner(), (int)SettingsOwner::Vt);
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.size(), 1);
    assertEqual((int)r.out.masks[0], (int)Plough_CalDataMask);
    assertEqual((int)r.out.values.size(), (int)PloughSettings::kCount);
    assertEqual(r.out.last(input(PloughSetting::Shares)), 4L);
    assertEqual(r.out.last(input(PloughSetting::Deadband)), 2L);
    assertEqual(r.out.last(input(PloughSetting::MaxCorrectionPerShare)), 50L);
    assertEqual(r.out.last(input(PloughSetting::Bypass)), 0L);
    assertEqual(r.out.last(input(PloughSetting::Swap)), 0L);         // input low, no swap: R
    assertEqual(r.out.last(input(PloughSetting::RtkQuality)), 1L);   // 4 is the RTK entry
    assertTrue(pgDebug.has("VT: calibration page open"));
}

test(IsobusVtCalibration, values_areSentOnce_thenOnlyWhenTheyChange) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.Update(r.out);
    r.out.clear();
    r.page.Update(r.out);
    assertEqual((int)r.out.values.size(), 0);
    assertEqual((int)r.out.masks.size(), 0);
}

test(IsobusVtCalibration, calibrateKey_isRefused_whileTheCabWizardIsOpen) {
    resetAll();
    Rig r;
    assertTrue(r.cal.Start());
    r.page.OnCalibrateKey();
    assertFalse(r.page.IsOpen());
    assertEqual((int)r.settings().GetOwner(), (int)SettingsOwner::Cab);
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.size(), 0);
    assertTrue(pgDebug.has("the cab wizard is open"));
}

test(IsobusVtCalibration, enteredValue_landsInTheDraft_notOnThePlough) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.Update(r.out);
    r.out.clear();
    r.page.OnNumericValue(input(PloughSetting::Shares), 6);
    assertEqual(r.settings().Get(PloughSetting::Shares), (int16_t)6);
    assertEqual((int)r.impl.GetShares(), 4);
    assertEqual((int)EEPROM.read(64), 255);
    r.page.Update(r.out);
    assertEqual((int)r.out.values.size(), 0);   // the VT already shows it
}

test(IsobusVtCalibration, outOfRangeValue_isRefused_andTheHeldValueShownAgain) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.Update(r.out);
    r.out.clear();
    r.page.OnNumericValue(input(PloughSetting::Shares), 11);
    assertEqual(r.settings().Get(PloughSetting::Shares), (int16_t)4);
    r.page.Update(r.out);
    assertEqual(r.out.last(input(PloughSetting::Shares)), 4L);
    r.page.OnNumericValue(input(PloughSetting::RtkQuality), 2);    // no third entry
    assertEqual(r.settings().Get(PloughSetting::RtkQuality), (int16_t)4);
    assertTrue(pgDebug.has("VT: calibration value refused for shares"));
}

test(IsobusVtCalibration, lists_mapTheirEntriesToTheSettings) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.OnNumericValue(input(PloughSetting::Bypass), 1);
    r.page.OnNumericValue(input(PloughSetting::HitchInversion), 1);
    r.page.OnNumericValue(input(PloughSetting::RtkQuality), 0);
    r.page.OnNumericValue(input(PloughSetting::Swap), 1);          // LINKS with the input low: swap
    assertEqual(r.settings().Get(PloughSetting::Bypass), (int16_t)1);
    assertEqual(r.settings().Get(PloughSetting::HitchInversion), (int16_t)1);
    assertEqual(r.settings().Get(PloughSetting::RtkQuality), (int16_t)2);
    assertEqual(r.settings().Get(PloughSetting::Swap), (int16_t)1);
}

// The side list shows the side the plough gives, which follows the
// plough-side input while the page is open.
test(IsobusVtCalibration, sideList_followsThePloughSideInput) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.Update(r.out);
    assertEqual(r.out.last(input(PloughSetting::Swap)), 0L);
    digitalReadValue(PLOUGHSIDE_PIN_2, true);
    r.page.Update(r.out);
    assertEqual(r.out.last(input(PloughSetting::Swap)), 1L);
    // LINKS with the input high is no swap.
    r.page.OnNumericValue(input(PloughSetting::Swap), 1);
    assertEqual(r.settings().Get(PloughSetting::Swap), (int16_t)0);
}

test(IsobusVtCalibration, opslaan_savesTheDraft_andGoesBackToTheMainMask) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.Update(r.out);
    r.out.clear();
    r.page.OnNumericValue(input(PloughSetting::Shares), 10);
    r.page.OnNumericValue(input(PloughSetting::PreferredVt), 2);
    assertTrue(r.page.OnSoftKey(Key_CalSave));
    assertFalse(r.page.IsOpen());
    assertEqual((int)r.settings().GetOwner(), (int)SettingsOwner::None);
    assertEqual((int)r.impl.GetShares(), 10);
    assertEqual((int)r.impl.GetOffset(), 400);   // #247, as at the cab
    assertEqual((int)EEPROM.read(64), 10);
    assertEqual((int)EEPROM.read(70), 2);
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.size(), 1);
    assertEqual((int)r.out.masks[0], (int)Plough_DataMask);
    assertEqual((int)r.out.values.size(), 0);
}

test(IsobusVtCalibration, annuleren_dropsTheDraft_andGoesBackToTheMainMask) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.OnNumericValue(input(PloughSetting::Shares), 10);
    assertFalse(r.page.OnSoftKey(Key_CalCancel));
    assertFalse(r.page.IsOpen());
    assertEqual((int)r.impl.GetShares(), 4);
    for (int a = 0; a < 80; a++) assertEqual((int)EEPROM.read(a), 255);
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.back(), (int)Plough_DataMask);
}

test(IsobusVtCalibration, lostVt_dropsTheDraft_andNothingIsSaved) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.page.OnNumericValue(input(PloughSetting::Shares), 10);
    r.page.OnConnectionLost();
    assertFalse(r.page.IsOpen());
    assertEqual((int)r.settings().GetOwner(), (int)SettingsOwner::None);
    assertEqual((int)EEPROM.read(64), 255);
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.size(), 0);     // the new VT starts on the main mask
    // A page a VT kept showing: its keys only go back to the main mask.
    assertFalse(r.page.OnSoftKey(Key_CalSave));
    assertEqual((int)EEPROM.read(64), 255);
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.back(), (int)Plough_DataMask);
    // And the cab wizard can open again.
    assertTrue(r.cal.Start());
}

test(IsobusVtCalibration, messagesTheVtCouldNotTake_areSentAgain) {
    resetAll();
    Rig r;
    r.page.OnCalibrateKey();
    r.out.accept = false;
    r.page.Update(r.out);
    r.out.accept = true;
    r.page.Update(r.out);
    assertEqual((int)r.out.masks.size(), 1);
    assertEqual((int)r.out.values.size(), (int)PloughSettings::kCount);
}

test(IsobusVtCalibration, closedPage_andOtherObjects_areIgnored) {
    resetAll();
    Rig r;
    r.page.OnNumericValue(input(PloughSetting::Shares), 6);          // not open
    assertFalse(r.page.OnSoftKey(Key_Wider));
    r.page.OnCalibrateKey();
    r.page.OnNumericValue(Var_Offset, 6);                            // not the page's
    r.page.OnNumericValue(Input_Cal0 + PloughSettings::kCount, 6);  // past the last row
    assertEqual(r.settings().Get(PloughSetting::Shares), (int16_t)4);
}
