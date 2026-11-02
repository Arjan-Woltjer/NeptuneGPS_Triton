/*
  test_ScraperVtCalibration - The kilverbak's calibration page on the VT: the
  draft it edits, the values it shows, the stored points and the masks it
  asks for (NeptuneGPS_Triton#292).
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
#ifdef ISOBUS

// Standard headers first: the Arduino stub defines min/max macros.
#include <cstring>
#include <utility>
#include <vector>

#include <AUnit.h>

#include "EepromStore.hpp"
#include "calibration/CalibrationScraper.hpp"
#include "isobus/ScraperVtCalibration.hpp"
#include "isobus/VTObjectPool.hpp"

using namespace aunit;
using namespace triton;

namespace {

class PageStream : public Stream {
public:
    size_t write(uint8_t c) override {
        if (len < sizeof(text) - 1) text[len++] = (char)c;
        text[len] = '\0';
        return 1;
    }
    bool Contains(const char* needle) const { return strstr(text, needle) != nullptr; }

    char   text[1024] = {};
    size_t len = 0;
};

class PageOutput : public ScraperVtOutput {
public:
    bool ChangeActiveMask(uint16_t dataMask) override {
        if (!accept) return false;
        masks.push_back(dataMask);
        return true;
    }
    bool ChangeNumericValue(uint16_t objectId, uint32_t value) override {
        if (!accept) return false;
        values.emplace_back(objectId, value);
        return true;
    }
    uint32_t Last(uint16_t objectId) const {
        for (auto it = values.rbegin(); it != values.rend(); ++it)
            if (it->first == objectId) return it->second;
        return 0xFFFFFFFF;
    }
    size_t SentTo(uint16_t objectId) const {
        size_t n = 0;
        for (const auto& v : values) n += v.first == objectId;
        return n;
    }

    bool accept = true;
    std::vector<uint16_t> masks;
    std::vector<std::pair<uint16_t, uint32_t>> values;
};

VehicleSensors  pageTractor;
InterfaceI2CLCD pageLcd;

// One kilverbak per test, on an erased EEPROM.
struct Fixture {
    PageStream           debug;
    GuidanceSource       guidance;
    ImplementScraper     implement;
    InterfaceScraper     scraperInterface;
    CalibrationScraper   wizard;
    ScraperVtCalibration page;

    Fixture()
        : implement((reset(), &guidance)), scraperInterface(&pageLcd, &implement, &pageTractor, &guidance),
          wizard(&pageLcd, &implement, &guidance, &scraperInterface),
          page(&debug, &wizard.GetSettings(), &implement) {}

    static void reset() {
        millisValue(0);
        EEPROM.eepromReset();
    }

    uint16_t Input(ScraperSetting id) const { return uint16_t(Input_Cal0 + uint8_t(id)); }
};

}  // namespace

// ---------------------------------------------------------------------------
// The values an input holds
// ---------------------------------------------------------------------------

test(ScraperVtCalibration, toVt_fromVt_roundTripEveryValueOfEveryLine) {
    for (uint8_t i = 0; i < ScraperSettings::kCount; i++) {
        const ScraperSetting id = ScraperSetting(i);
        const ScraperSettingLine& line = ScraperSettings::Line(id);
        for (int32_t v = line.lowest; v <= line.highest; v++) {
            if (!ScraperSettings::IsValid(id, int16_t(v))) continue;
            int16_t back = 0x7777;
            assertTrue(ScraperVtCalibration::FromVt(id, ScraperVtCalibration::ToVt(id, int16_t(v)), back));
            assertEqual((int)back, (int)v);
        }
    }
}

// The slope goes below 0, an input's value cannot: -99 is held as 0, 99 as
// 198, and the input's offset (-99) shows them as they are.
test(ScraperVtCalibration, toVt_slopeIsLiftedAboveZero) {
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::Slope, -99), (uint32_t)0);
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::Slope, 0), (uint32_t)99);
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::Slope, 99), (uint32_t)198);
    // The other number lines are not lifted.
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::ErrorMargin, 3), (uint32_t)3);
}

test(ScraperVtCalibration, toVt_listsAreIndexes) {
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::RtkQuality, 4), (uint32_t)1);
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::RtkQuality, 2), (uint32_t)0);
    assertEqual(ScraperVtCalibration::ToVt(ScraperSetting::Swap, 1), (uint32_t)1);
    int16_t value = 0;
    assertFalse(ScraperVtCalibration::FromVt(ScraperSetting::RtkQuality, 2, value));
    assertFalse(ScraperVtCalibration::FromVt(ScraperSetting::SpeedGate, 2, value));
    assertFalse(ScraperVtCalibration::FromVt(ScraperSetting::Slope, 0x80000000u, value));
}

// ---------------------------------------------------------------------------
// Opening, editing, saving
// ---------------------------------------------------------------------------

test(ScraperVtCalibration, open_showsEveryHeldValue_andThePage) {
    Fixture f;
    f.implement.SetSlope(-5);
    f.page.OnCalibrateKey();
    assertTrue(f.page.IsOpen());
    assertEqual((int)f.wizard.GetSettings().GetOwner(), (int)SettingsOwner::Vt);
    PageOutput out;
    f.page.Update(out);
    assertEqual((uint32_t)out.masks.size(), (uint32_t)1);
    assertEqual((int)out.masks[0], (int)Scraper_CalDataMask);
    assertEqual(out.Last(f.Input(ScraperSetting::Slope)), (uint32_t)(99 - 5));
    assertEqual(out.Last(f.Input(ScraperSetting::ErrorMargin)), (uint32_t)2);   // the default
    assertEqual(out.Last(f.Input(ScraperSetting::SpeedGate)), (uint32_t)1);     // on unless 0
    assertEqual(out.Last(f.Input(ScraperSetting::RtkQuality)), (uint32_t)1);    // RTK fixed
    // No references yet: both at 0 m, 0 m apart.
    assertEqual(out.Last(Var_CalRefA), (uint32_t)kLargeBias);
    assertEqual(out.Last(Var_CalRefDistance), (uint32_t)kLargeBias);
    // Sent once, not every pass.
    f.page.Update(out);
    assertEqual((uint32_t)out.SentTo(f.Input(ScraperSetting::Slope)), (uint32_t)1);
    assertEqual((uint32_t)out.masks.size(), (uint32_t)1);
}

test(ScraperVtCalibration, open_stopsTheValves) {
    Fixture f;
    analogWrite(OUTPUT_WIDE_5, 255);
    f.page.OnCalibrateKey();
    assertEqual(analogWriteValue(OUTPUT_WIDE_5), 0);
}

// The operator enters -12 for the slope (held as 87) and turns the swap on;
// OPSLAAN applies and stores both, and nothing else.
test(ScraperVtCalibration, save_appliesAndStoresTheDraft) {
    Fixture f;
    f.page.OnCalibrateKey();
    f.page.OnNumericValue(f.Input(ScraperSetting::Slope), 87);
    f.page.OnNumericValue(f.Input(ScraperSetting::Swap), 1);
    assertEqual((int)f.implement.GetSlope(), 0);  // only the draft so far
    assertTrue(f.page.OnSoftKey(Key_CalSave));
    assertFalse(f.page.IsOpen());
    assertEqual((int)f.implement.GetSlope(), -12);
    assertTrue(f.implement.GetSwap());
    assertEqual((int)EepromStore::ReadInt16Le(146), -12);
    assertEqual((int)EEPROM.read(ImplementScraper::kEepromSwap), 1);
    assertEqual((int)EEPROM.read(150), 0xFF);  // reference A untouched (#289)
    assertEqual((int)f.wizard.GetSettings().GetOwner(), (int)SettingsOwner::None);
    PageOutput out;
    f.page.Update(out);
    assertEqual((int)out.masks.back(), (int)Scraper_DataMask);
}

test(ScraperVtCalibration, cancel_dropsTheDraft) {
    Fixture f;
    f.page.OnCalibrateKey();
    f.page.OnNumericValue(f.Input(ScraperSetting::ErrorMargin), 7);
    assertFalse(f.page.OnSoftKey(Key_CalCancel));
    assertFalse(f.page.IsOpen());
    assertEqual((int)f.implement.GetError(), 2);
    assertEqual((int)EEPROM.read(142), 0xFF);
    assertEqual((int)f.wizard.GetSettings().GetOwner(), (int)SettingsOwner::None);
}

// A value the table refuses (a VT that let it through, or a stale page) is
// logged, the draft keeps its value, and the VT is shown that value again.
test(ScraperVtCalibration, refusedValue_isShownTheDraftsValueAgain) {
    Fixture f;
    f.page.OnCalibrateKey();
    PageOutput out;
    f.page.Update(out);
    f.page.OnNumericValue(f.Input(ScraperSetting::ErrorMargin), 11);
    assertTrue(f.debug.Contains("refused for margin"));
    assertEqual((int)f.wizard.GetSettings().Get(ScraperSetting::ErrorMargin), 2);
    f.page.Update(out);
    assertEqual(out.Last(f.Input(ScraperSetting::ErrorMargin)), (uint32_t)2);
    assertEqual((uint32_t)out.SentTo(f.Input(ScraperSetting::ErrorMargin)), (uint32_t)2);
}

test(ScraperVtCalibration, notThePagesObject_isIgnored) {
    Fixture f;
    f.page.OnCalibrateKey();
    f.page.OnNumericValue(Input_Cal0 + ScraperSettings::kCount, 1);
    f.page.OnNumericValue(Var_Speed, 1);
    assertFalse(f.debug.Contains("refused"));
}

// One session at a time: the cab wizard open, CALIBR is refused; the page
// open, the wizard is.
test(ScraperVtCalibration, oneSessionAtATime) {
    Fixture f;
    assertTrue(f.wizard.Start());
    f.page.OnCalibrateKey();
    assertFalse(f.page.IsOpen());
    assertTrue(f.debug.Contains("cab wizard is open"));

    Fixture g;
    g.page.OnCalibrateKey();
    assertTrue(g.page.IsOpen());
    assertFalse(g.wizard.Start());
}

// A VT that goes away takes the draft with it; the new one starts on the
// working screen, so no mask is pending.
test(ScraperVtCalibration, connectionLost_dropsTheDraft_noMaskPending) {
    Fixture f;
    f.page.OnCalibrateKey();
    f.page.OnNumericValue(f.Input(ScraperSetting::Slope), 150);
    f.page.OnConnectionLost();
    assertFalse(f.page.IsOpen());
    assertEqual((int)f.implement.GetSlope(), 0);
    assertEqual((int)f.wizard.GetSettings().GetOwner(), (int)SettingsOwner::None);
    PageOutput out;
    f.page.Update(out);
    assertTrue(out.masks.empty());
}

// OPSLAAN or ANNUL. on a page the VT kept after a reconnect: there is no
// draft, so it just goes back to the working screen.
test(ScraperVtCalibration, keyOnAStalePage_goesBackToTheWorkingScreen) {
    Fixture f;
    assertFalse(f.page.OnSoftKey(Key_CalSave));
    PageOutput out;
    f.page.Update(out);
    assertEqual((uint32_t)out.masks.size(), (uint32_t)1);
    assertEqual((int)out.masks[0], (int)Scraper_DataMask);
}

test(ScraperVtCalibration, maskAndValues_retriedUntilTheVtTakesThem) {
    Fixture f;
    f.page.OnCalibrateKey();
    PageOutput out;
    out.accept = false;
    f.page.Update(out);
    out.accept = true;
    f.page.Update(out);
    assertEqual((uint32_t)out.masks.size(), (uint32_t)1);
    assertEqual(out.Last(f.Input(ScraperSetting::Slope)), (uint32_t)99);
}

// The stored points: heights as set, and the distance between them in m.
test(ScraperVtCalibration, storedPoints_heightsAndDistance) {
    Fixture f;
    millisValue(1000);
    f.guidance.SetPosition(52.0f, 5.0f);
    f.guidance.SetAltitude(10.25f);
    f.implement.SetRefA();
    f.guidance.SetPosition(52.0f + 100.0f / 111320.0f, 5.0f);  // about 100 m north
    f.guidance.SetAltitude(10.5f);
    f.implement.SetRefB();
    f.page.OnCalibrateKey();
    PageOutput out;
    f.page.Update(out);
    assertEqual(out.Last(Var_CalRefA), (uint32_t)(1025 + kLargeBias));
    assertEqual(out.Last(Var_CalRefB), (uint32_t)(1050 + kLargeBias));
    const uint32_t distance = out.Last(Var_CalRefDistance) - kLargeBias;
    assertTrue(distance >= 99u && distance <= 101u);
}

#endif  // ISOBUS
