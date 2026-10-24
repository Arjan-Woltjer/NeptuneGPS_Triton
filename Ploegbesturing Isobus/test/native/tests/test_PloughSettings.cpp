/*
  test_PloughSettings - Tests for the plough's settings table (#240, #177
  step 4): one range check that equals the boot load, a draft that reaches the
  plough and EEPROM only on Save, and one session owner at a time.
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
#include <string.h>

#include <AUnit.h>
#include "calibration/PloughSettings.hpp"
#include "calibration/CalibrationPlough.hpp"
#include "EepromStore.hpp"

using namespace aunit;
using namespace triton;

static GuidanceSource  psGuidance;
static VehicleTractor  psTractor;
static InterfaceI2CLCD psLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    psGuidance = GuidanceSource();
    psTractor  = VehicleTractor();
}

// What main.cpp builds at boot: the implement and the wizard read EEPROM in
// their constructors, so a new Rig is a reboot.
struct Rig {
    ImplementPlough   impl;
    CalibrationPlough cal;
    PloughSettings    settings;
    Rig() : impl(nullptr, &psGuidance),
            cal(nullptr, &psLcd, &impl, &psTractor, &psGuidance, nullptr),
            settings(&impl, &psTractor, &psGuidance, &cal) {}
};

static int16_t storedByte(PloughSetting id) {
    return EEPROM.read(PloughSettings::Line(id).eepromAddress);
}

// A valid value other than the default for every line, in table order. The
// lightbar's 2 is Off (#272).
static const int16_t kEdited[PloughSettings::kCount] = { 6, 1, 1, 1, 5, 75, 2, 2, 2 };

test(PloughSettings, table_followsTheWizardOrder_withUniqueKeys) {
    static const char* const expected[] = { "shares", "side", "hitchinv", "bypass", "deadband", "maxcorr", "vt", "lightbar", "rtkq" };
    assertEqual((int)PloughSettings::kCount, 9);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSettingLine& line = PloughSettings::Line(PloughSetting(i));
        assertEqual((int)line.id, (int)i);
        assertEqual(strcmp(line.key, expected[i]), 0);
        assertTrue(PloughSettings::Find(line.key) == &line);
    }
    assertTrue(PloughSettings::Find("nope") == nullptr);
}

// The one range check, held against the boot loads themselves: for every
// byte a line's EEPROM address can hold, IsValid() says yes exactly when the
// boot keeps that byte, and otherwise the boot falls back to the default.
// The hitch inversion is left out: the native build stubs VehicleTractor,
// whose real load (`EEPROM.read(28) == 1`) only runs on the board.
test(PloughSettings, isValid_isWhatTheBootLoadKeeps_forEveryByte) {
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSetting id = PloughSetting(i);
        if (id == PloughSetting::HitchInversion) continue;
        resetAll();
        int16_t fallback;
        {
            Rig erased;
            fallback = erased.settings.Get(id);
        }
        for (int v = 0; v < 256; v++) {
            resetAll();
            EEPROM.write(PloughSettings::Line(id).eepromAddress, byte(v));
            Rig rebooted;
            const int16_t loaded = rebooted.settings.Get(id);
            if (PloughSettings::IsValid(id, int16_t(v))) {
                assertEqual(loaded, (int16_t)v);
            }
            else {
                assertEqual(loaded, fallback);
            }
        }
    }
}

test(PloughSettings, set_isStagedOnly_nothingReachesThePloughOrEepromBeforeSave) {
    resetAll();
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertTrue(r.settings.Set(SettingsOwner::Cab, PloughSetting(i), kEdited[i]));
        assertEqual(r.settings.Get(PloughSetting(i)), kEdited[i]);
    }
    assertEqual((int)r.impl.GetShares(), (int)kDefaultShares);
    assertFalse(r.impl.GetSwap());
    assertFalse(psTractor.GetInversion());
    assertFalse(r.impl.GetBypass());
    assertEqual((int)r.impl.GetDeadband(), (int)kDefaultDeadband);
    assertEqual((int)r.impl.GetMaxCorrectionPerShare(), (int)kDefaultMaxCorrectionPerShareMm);
    assertEqual((int)r.cal.GetPreferredVt(), 0);
    assertEqual((int)r.cal.GetLightbarMode(), (int)LightbarMode::Auto);
    assertEqual((int)psGuidance.GetRtkQuality(), 4);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertEqual(storedByte(PloughSetting(i)), (int16_t)255);
    }
}

test(PloughSettings, save_appliesTheDraft_andWritesEachValueAtItsTableAddress) {
    resetAll();
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Vt));
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertTrue(r.settings.Set(SettingsOwner::Vt, PloughSetting(i), kEdited[i]));
    }
    assertTrue(r.settings.Save(SettingsOwner::Vt));
    assertEqual((int)r.settings.GetOwner(), (int)SettingsOwner::None);

    assertEqual((int)r.impl.GetShares(), 6);
    assertTrue(r.impl.GetSwap());
    assertTrue(psTractor.GetInversion());
    assertTrue(r.impl.GetBypass());
    assertEqual((int)r.impl.GetDeadband(), 5);
    assertEqual((int)r.impl.GetMaxCorrectionPerShare(), 75);
    assertEqual((int)r.cal.GetPreferredVt(), 2);
    assertEqual((int)r.cal.GetLightbarMode(), (int)LightbarMode::Off);
    assertEqual((int)psGuidance.GetRtkQuality(), 2);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertEqual(storedByte(PloughSetting(i)), kEdited[i]);
        assertEqual(r.settings.Get(PloughSetting(i)), kEdited[i]);  // no session: the held value
    }
}

test(PloughSettings, save_thenReboot_bringsEveryValueBack) {
    resetAll();
    {
        Rig r;
        assertTrue(r.settings.Begin(SettingsOwner::Cab));
        for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
            assertTrue(r.settings.Set(SettingsOwner::Cab, PloughSetting(i), kEdited[i]));
        }
        assertTrue(r.settings.Save(SettingsOwner::Cab));
    }
    psGuidance = GuidanceSource();   // the boot rebuilds it with the default 4
    Rig rebooted;
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        // The stubbed tractor keeps its inversion in memory; see above.
        if (PloughSetting(i) == PloughSetting::HitchInversion) continue;
        assertEqual(rebooted.settings.Get(PloughSetting(i)), kEdited[i]);
    }
}

test(PloughSettings, set_outOfRange_isRefused_andTheDraftKeepsItsValue) {
    resetAll();
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Shares, 11));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Shares, 1));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Deadband, 11));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::MaxCorrectionPerShare, 24));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::MaxCorrectionPerShare, 101));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Swap, 2));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::PreferredVt, 10));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Lightbar, 3));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Lightbar, -1));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::RtkQuality, 3));
    assertEqual(r.settings.Get(PloughSetting::Shares), (int16_t)kDefaultShares);
    assertEqual(r.settings.Get(PloughSetting::RtkQuality), (int16_t)4);
    // Off the 5 mm step but inside what the boot keeps: accepted.
    assertTrue(r.settings.Set(SettingsOwner::Cab, PloughSetting::MaxCorrectionPerShare, 27));
}

test(PloughSettings, cancel_leavesThePloughAndEepromUntouched) {
    resetAll();
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertTrue(r.settings.Set(SettingsOwner::Cab, PloughSetting(i), kEdited[i]));
    }
    r.settings.Cancel(SettingsOwner::Cab);
    assertEqual((int)r.settings.GetOwner(), (int)SettingsOwner::None);
    assertEqual(r.settings.Get(PloughSetting::Shares), (int16_t)kDefaultShares);
    assertEqual((int)r.impl.GetShares(), (int)kDefaultShares);
    assertEqual((int)psGuidance.GetRtkQuality(), 4);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertEqual(storedByte(PloughSetting(i)), (int16_t)255);
    }
    assertEqual((int)EEPROM.read(40), 255);   // no width written either
}

test(PloughSettings, oneOwnerAtATime) {
    resetAll();
    Rig r;
    assertFalse(r.settings.Begin(SettingsOwner::None));
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    assertTrue(r.settings.Set(SettingsOwner::Cab, PloughSetting::Shares, 6));
    assertFalse(r.settings.Begin(SettingsOwner::Vt));
    assertFalse(r.settings.Set(SettingsOwner::Vt, PloughSetting::Shares, 7));
    assertFalse(r.settings.Save(SettingsOwner::Vt));
    r.settings.Cancel(SettingsOwner::Vt);                      // not the owner: ignored
    assertEqual((int)r.settings.GetOwner(), (int)SettingsOwner::Cab);
    assertTrue(r.settings.Begin(SettingsOwner::Cab));          // again: the draft is kept
    assertEqual(r.settings.Get(PloughSetting::Shares), (int16_t)6);
    r.settings.Cancel(SettingsOwner::Cab);
    assertTrue(r.settings.Begin(SettingsOwner::Vt));
    assertFalse(r.settings.Set(SettingsOwner::Cab, PloughSetting::Shares, 6));
    assertFalse(r.settings.Save(SettingsOwner::Cab));
    assertEqual((int)EEPROM.read(64), 255);
}

test(PloughSettings, step_staysInsideTheBounds_andJumpsForTwoValueSettings) {
    assertEqual(PloughSettings::Step(PloughSetting::Shares, 4, 1), (int16_t)5);
    assertEqual(PloughSettings::Step(PloughSetting::Shares, 10, 1), (int16_t)10);
    assertEqual(PloughSettings::Step(PloughSetting::Shares, 2, -1), (int16_t)2);
    assertEqual(PloughSettings::Step(PloughSetting::Shares, 4, 0), (int16_t)4);
    assertEqual(PloughSettings::Step(PloughSetting::MaxCorrectionPerShare, 95, 1), (int16_t)100);
    assertEqual(PloughSettings::Step(PloughSetting::MaxCorrectionPerShare, 100, 1), (int16_t)100);
    assertEqual(PloughSettings::Step(PloughSetting::MaxCorrectionPerShare, 25, -1), (int16_t)25);
    assertEqual(PloughSettings::Step(PloughSetting::PreferredVt, 9, 1), (int16_t)9);
    assertEqual(PloughSettings::Step(PloughSetting::PreferredVt, 0, -1), (int16_t)0);
    // The lightbar has three values (#272): stepped one at a time, not jumped.
    assertEqual(PloughSettings::Step(PloughSetting::Lightbar, 0, 1), (int16_t)1);
    assertEqual(PloughSettings::Step(PloughSetting::Lightbar, 1, 1), (int16_t)2);
    assertEqual(PloughSettings::Step(PloughSetting::Lightbar, 2, 1), (int16_t)2);
    assertEqual(PloughSettings::Step(PloughSetting::Lightbar, 2, -1), (int16_t)1);
    assertEqual(PloughSettings::Step(PloughSetting::Lightbar, 0, -1), (int16_t)0);
    // The wizard's screens: + is yes / swap / RTK fixed, - is no / no swap / DGPS.
    assertEqual(PloughSettings::Step(PloughSetting::Bypass, 0, 1), (int16_t)1);
    assertEqual(PloughSettings::Step(PloughSetting::Bypass, 1, 1), (int16_t)1);
    assertEqual(PloughSettings::Step(PloughSetting::Swap, 1, -1), (int16_t)0);
    assertEqual(PloughSettings::Step(PloughSetting::RtkQuality, 2, 1), (int16_t)4);
    assertEqual(PloughSettings::Step(PloughSetting::RtkQuality, 4, -1), (int16_t)2);
    assertEqual(PloughSettings::Step(PloughSetting::RtkQuality, 4, 0), (int16_t)4);
}

test(PloughSettings, widthCalibration_isCheckedAsTheBootChecksIt_andWrittenOnSave) {
    resetAll();
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    const short int same[3]     = { 500, 500, 300 };
    const short int tooHigh[3]  = { 1024, 450, 300 };
    const short int negative[3] = { 600, 450, -1 };
    const short int good[3]     = { 610, 455, 290 };
    assertFalse(r.settings.StageWidthCalibration(SettingsOwner::Cab, same));
    assertFalse(r.settings.StageWidthCalibration(SettingsOwner::Cab, tooHigh));
    assertFalse(r.settings.StageWidthCalibration(SettingsOwner::Cab, negative));
    assertFalse(r.settings.HasStagedWidthCalibration());
    assertFalse(r.settings.StageWidthCalibration(SettingsOwner::Vt, good));   // not the owner
    assertTrue(r.settings.StageWidthCalibration(SettingsOwner::Cab, good));
    assertTrue(r.settings.HasStagedWidthCalibration());
    assertEqual((int)EEPROM.read(40), 255);
    assertTrue(r.settings.Save(SettingsOwner::Cab));
    assertEqual((int)EepromStore::ReadUint16Be(40), 610);
    assertEqual((int)EepromStore::ReadUint16Be(42), 455);
    assertEqual((int)EepromStore::ReadUint16Be(44), 290);
}

// The boot load uses the same width check: stored points with two equal
// neighbours are replaced by the defaults, which a Save then writes back.
test(PloughSettings, bootLoad_degenerateWidthCalibration_fallsBackToTheDefaults) {
    resetAll();
    EepromStore::WriteUint16Be(40, 500);
    EepromStore::WriteUint16Be(42, 500);
    EepromStore::WriteUint16Be(44, 300);
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    assertTrue(r.settings.Save(SettingsOwner::Cab));
    for (int i = 0; i < 3; i++) {
        assertEqual((int)EepromStore::ReadUint16Be(40 + 2 * i), (int)kDefaultPositionCalibration[i]);
    }
}

// Saving without a width procedure writes the width the plough holds, as the
// wizard's COMPLETE step always did.
test(PloughSettings, save_withoutWidthProcedure_writesTheHeldWidth) {
    resetAll();
    Rig r;
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    assertTrue(r.settings.Save(SettingsOwner::Cab));
    for (int i = 0; i < 3; i++) {
        assertEqual((int)EepromStore::ReadUint16Be(40 + 2 * i), (int)kDefaultPositionCalibration[i]);
    }
}

// #247 kept: a share change that leaves the width out of range for the new
// number of shares moves it to the middle (W1: 4 -> 10 shares, 160 -> 400),
// one that keeps it in range leaves it (W3: 4 -> 5, 160 stays), and the
// width is written with the rest.
test(PloughSettings, save_sharesChange_rechecksTheWidth) {
    resetAll();
    Rig r;
    assertEqual((int)r.impl.GetOffset(), 160);
    assertTrue(r.settings.Begin(SettingsOwner::Cab));
    assertTrue(r.settings.Set(SettingsOwner::Cab, PloughSetting::Shares, 10));
    assertEqual((int)r.impl.GetOffset(), 160);   // not before Save
    assertTrue(r.settings.Save(SettingsOwner::Cab));
    assertEqual((int)r.impl.GetOffset(), 400);
    assertEqual((int)EepromStore::ReadInt16Le(66), 400);

    resetAll();
    Rig r5;
    assertTrue(r5.settings.Begin(SettingsOwner::Cab));
    assertTrue(r5.settings.Set(SettingsOwner::Cab, PloughSetting::Shares, 5));
    assertTrue(r5.settings.Save(SettingsOwner::Cab));
    assertEqual((int)r5.impl.GetOffset(), 160);
}
