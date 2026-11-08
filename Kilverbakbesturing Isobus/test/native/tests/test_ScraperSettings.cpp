/*
  test_ScraperSettings - Tests for the kilverbak's settings table
  (NeptuneGPS_Triton#289): every line's range is what the boot load keeps, one
  owner at a time, and Save writes exactly the settings' own bytes.
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
#include "calibration/CalibrationScraper.hpp"
#include "EepromStore.hpp"

using namespace aunit;
using namespace triton;

static GuidanceSource  stGuidance;
static VehicleSensors  stTractor;
static InterfaceI2CLCD stLcd;

static void resetSettings() {
    millisValue(0);
    EEPROM.eepromReset();
    stGuidance = GuidanceSource();
}

struct SettingsRig {
    ImplementScraper   impl;
    InterfaceScraper   iface;
    CalibrationScraper cal;
    SettingsRig() : impl(&stGuidance),
                    iface(&stLcd, &impl, &stTractor, &stGuidance),
                    cal(&stLcd, &impl, &stGuidance, &iface) {}
    ScraperSettings& settings() { return cal.GetSettings(); }
};

// ---------------------------------------------------------------------------
// The table
// ---------------------------------------------------------------------------

test(ScraperSettings, keys_areUniqueAndFindable) {
    for (uint8_t i = 0; i < ScraperSettings::kCount; i++) {
        const ScraperSettingLine& line = ScraperSettings::Line(ScraperSetting(i));
        assertEqual((int)line.id, (int)i);
        assertTrue(ScraperSettings::Find(line.key) == &line);
    }
    assertTrue(ScraperSettings::Find("hitchinv") == nullptr);   // not a kilverbak setting
}

// What an edit may set is exactly what the boot load keeps: a slope of +-99
// (else 0), a margin up to 10 (else 2), quality 4 or 2.
test(ScraperSettings, ranges_matchTheBootLoads) {
    assertTrue(ScraperSettings::IsValid(ScraperSetting::Slope, -99));
    assertTrue(ScraperSettings::IsValid(ScraperSetting::Slope, 99));
    assertFalse(ScraperSettings::IsValid(ScraperSetting::Slope, 100));
    assertFalse(ScraperSettings::IsValid(ScraperSetting::Slope, -100));
    assertTrue(ScraperSettings::IsValid(ScraperSetting::ErrorMargin, 0));
    assertTrue(ScraperSettings::IsValid(ScraperSetting::ErrorMargin, 10));
    assertFalse(ScraperSettings::IsValid(ScraperSetting::ErrorMargin, 11));
    assertFalse(ScraperSettings::IsValid(ScraperSetting::Swap, 2));
    assertTrue(ScraperSettings::IsValid(ScraperSetting::RtkQuality, 2));
    assertTrue(ScraperSettings::IsValid(ScraperSetting::RtkQuality, 4));
    assertFalse(ScraperSettings::IsValid(ScraperSetting::RtkQuality, 3));
}

// Every valid value of every line survives a save and a reboot unchanged.
test(ScraperSettings, everyValidValue_survivesSaveAndReboot) {
    for (uint8_t i = 0; i < ScraperSettings::kCount; i++) {
        const ScraperSetting id = ScraperSetting(i);
        const ScraperSettingLine& line = ScraperSettings::Line(id);
        for (int16_t v = line.lowest; v <= line.highest; v++) {
            if (!ScraperSettings::IsValid(id, v)) continue;
            resetSettings();
            {
                SettingsRig rig;
                assertTrue(rig.settings().Begin(SettingsOwner::Cab));
                assertTrue(rig.settings().Set(SettingsOwner::Cab, id, v));
                assertTrue(rig.settings().Save(SettingsOwner::Cab));
            }
            SettingsRig rebooted;
            assertEqual((int)rebooted.settings().Get(id), (int)v);
        }
    }
}

test(ScraperSettings, step_staysWithinTheBounds) {
    assertEqual((int)ScraperSettings::Step(ScraperSetting::Slope, 99, 1), 99);
    assertEqual((int)ScraperSettings::Step(ScraperSetting::Slope, -99, -1), -99);
    assertEqual((int)ScraperSettings::Step(ScraperSetting::Slope, 4, 1), 5);
    assertEqual((int)ScraperSettings::Step(ScraperSetting::Swap, 0, 1), 1);
    assertEqual((int)ScraperSettings::Step(ScraperSetting::Swap, 1, -1), 0);
    assertEqual((int)ScraperSettings::Step(ScraperSetting::RtkQuality, 4, -1), 2);
}

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------

test(ScraperSettings, oneOwnerAtATime) {
    resetSettings();
    SettingsRig rig;
    assertTrue(rig.settings().Begin(SettingsOwner::Vt));
    assertFalse(rig.settings().Begin(SettingsOwner::Cab));
    assertFalse(rig.settings().Set(SettingsOwner::Cab, ScraperSetting::Slope, 5));
    assertFalse(rig.settings().Save(SettingsOwner::Cab));
    assertTrue(rig.settings().Set(SettingsOwner::Vt, ScraperSetting::Slope, 5));
    assertFalse(rig.settings().Set(SettingsOwner::Vt, ScraperSetting::Slope, 100));
    assertEqual((int)rig.settings().Get(ScraperSetting::Slope), 5);
    rig.settings().Cancel(SettingsOwner::Vt);
    assertEqual((int)rig.settings().Get(ScraperSetting::Slope), 0);   // the held value again
    assertTrue(rig.settings().Begin(SettingsOwner::Cab));
}

// Save writes the settings' own bytes and nothing else: the reference points,
// the offset and the receiver rate index stay (owner, #289).
test(ScraperSettings, save_writesItsBytesAndKeepsTheRest) {
    resetSettings();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 3);
    EepromStore::WriteInt16Le(148, 7);
    for (int a = 150; a < 170; a++) EEPROM.write(a, uint8_t(a));
    SettingsRig rig;
    assertTrue(rig.settings().Begin(SettingsOwner::Cab));
    rig.settings().Set(SettingsOwner::Cab, ScraperSetting::Slope, -12);
    rig.settings().Set(SettingsOwner::Cab, ScraperSetting::ErrorMargin, 4);
    rig.settings().Set(SettingsOwner::Cab, ScraperSetting::Swap, 1);
    rig.settings().Set(SettingsOwner::Cab, ScraperSetting::SpeedGate, 0);
    rig.settings().Set(SettingsOwner::Cab, ScraperSetting::RtkQuality, 2);
    assertTrue(rig.settings().Save(SettingsOwner::Cab));
    assertEqual((int)rig.settings().GetOwner(), (int)SettingsOwner::None);

    assertEqual((int)EepromStore::ReadInt16Le(146), -12);
    assertEqual((int)EEPROM.read(142), 4);
    assertEqual((int)EEPROM.read(ImplementScraper::kEepromSwap), 1);
    assertEqual((int)EEPROM.read(ImplementScraper::kEepromSpeedGate), 0);
    assertEqual((int)EEPROM.read(CalibrationScraper::kEepromRtkQuality), 2);
    assertEqual((int)EEPROM.read(CalibrationScraper::kEepromGpsBaudIndex), 3);
    assertEqual((int)EepromStore::ReadInt16Le(148), 7);
    for (int a = 150; a < 170; a++) assertEqual((int)EEPROM.read(a), a);

    // And the live values are the saved ones.
    assertEqual((int)rig.impl.GetSlope(), -12);
    assertTrue(rig.impl.GetSwap());
    assertFalse(rig.impl.GetSpeedGate());
    assertEqual((int)stGuidance.GetRtkQuality(), 2);
}

test(ScraperSettings, cancel_appliesAndStoresNothing) {
    resetSettings();
    SettingsRig rig;
    rig.settings().Begin(SettingsOwner::Cab);
    rig.settings().Set(SettingsOwner::Cab, ScraperSetting::Slope, 20);
    rig.settings().Cancel(SettingsOwner::Cab);
    assertEqual((int)rig.impl.GetSlope(), 0);
    assertEqual((int)EEPROM.read(146), 255);
}
