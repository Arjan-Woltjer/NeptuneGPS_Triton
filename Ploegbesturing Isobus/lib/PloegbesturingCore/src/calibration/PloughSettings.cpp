/*
  PloughSettings - the plough's settings as one table, with a draft that only
  reaches the plough and EEPROM on Save
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
#include "PloughSettings.hpp"

#include <string.h>

#include "CalibrationPlough.hpp"

namespace triton
{

// The bounds are the ones the boot loads apply: ImplementPlough's
// readCalibrationData() (58-68), VehicleTractor's (28), CalibrationPlough's
// (11) and ReadPreferredVt() (70). test_PloughSettings checks every byte
// value against those loads.
static const PloughSettingLine kLines[PloughSettings::kCount] = {
    { PloughSetting::Shares,                "shares",   SettingKind::Number,     kLowestShares,   kHighestShares,   1, 64 },
    { PloughSetting::Swap,                  "side",     SettingKind::PloughSide, 0,               1,                1, 62 },
    { PloughSetting::HitchInversion,        "hitchinv", SettingKind::YesNo,      0,               1,                1, 28 },
    { PloughSetting::Bypass,                "bypass",   SettingKind::YesNo,      0,               1,                1, 68 },
    { PloughSetting::Deadband,              "deadband", SettingKind::Number,     kLowestDeadband, kHighestDeadband, 1, 58 },
    { PloughSetting::MaxCorrectionPerShare, "maxcorr",  SettingKind::Number,
      kLowestMaxCorrectionPerShareMm, kHighestMaxCorrectionPerShareMm, 5, 60 },
#ifdef ISOBUS
    { PloughSetting::PreferredVt,           "vt",       SettingKind::Number,     0,               kHighestPreferredVt, 1, kEepromPreferredVt },
#endif
    { PloughSetting::RtkQuality,            "rtkq",     SettingKind::RtkQuality, 2,               4,                2,
      CalibrationPlough::kEepromRtkQuality },
};

const PloughSettingLine& PloughSettings::Line(PloughSetting id) {
    return kLines[uint8_t(id)];
}

const PloughSettingLine* PloughSettings::Find(const char* key) {
    for (uint8_t i = 0; i < kCount; i++) {
        if (strcmp(kLines[i].key, key) == 0) return &kLines[i];
    }
    return nullptr;
}

bool PloughSettings::IsValid(PloughSetting id, int16_t value) {
    const PloughSettingLine& line = Line(id);
    if (line.kind == SettingKind::RtkQuality) return value == 4 || value == 2;
    return value >= line.lowest && value <= line.highest;
}

int16_t PloughSettings::Step(PloughSetting id, int16_t value, int direction) {
    const PloughSettingLine& line = Line(id);
    if (line.kind != SettingKind::Number) {
        if (direction == 1) return line.highest;
        if (direction == -1) return line.lowest;
        return value;
    }
    if (direction == 1 && value + line.step <= line.highest) return value + line.step;
    if (direction == -1 && value - line.step >= line.lowest) return value - line.step;
    return value;
}

PloughSettings::PloughSettings(ImplementPlough* implement, VehicleTractor* tractor,
                               GuidanceSource* guidance, CalibrationPlough* calibration)
    : implement(implement), tractor(tractor), guidance(guidance), calibration(calibration) {}

int16_t PloughSettings::held(PloughSetting id) const {
    switch (id) {
        case PloughSetting::Shares:                return implement->GetShares();
        case PloughSetting::Swap:                  return implement->GetSwap();
        case PloughSetting::HitchInversion:        return tractor->GetInversion();
        case PloughSetting::Bypass:                return implement->GetBypass();
        case PloughSetting::Deadband:              return implement->GetDeadband();
        case PloughSetting::MaxCorrectionPerShare: return implement->GetMaxCorrectionPerShare();
#ifdef ISOBUS
        case PloughSetting::PreferredVt:           return calibration->GetPreferredVt();
#endif
        case PloughSetting::RtkQuality:            return guidance->GetRtkQuality();
        default:                                   return 0;
    }
}

bool PloughSettings::Begin(SettingsOwner who) {
    if (who == SettingsOwner::None) return false;
    if (owner == who) return true;
    if (owner != SettingsOwner::None) return false;
    for (uint8_t i = 0; i < kCount; i++) draft[i] = held(PloughSetting(i));
    widthStaged = false;
    owner = who;
    return true;
}

int16_t PloughSettings::Get(PloughSetting id) const {
    return owner != SettingsOwner::None ? draft[uint8_t(id)] : held(id);
}

bool PloughSettings::Set(SettingsOwner who, PloughSetting id, int16_t value) {
    if (who == SettingsOwner::None || who != owner) return false;
    if (!IsValid(id, value)) return false;
    draft[uint8_t(id)] = value;
    return true;
}

bool PloughSettings::StageWidthCalibration(SettingsOwner who, const short int adc[3]) {
    if (who == SettingsOwner::None || who != owner) return false;
    if (!ImplementPlough::IsValidPositionCalibration(adc)) return false;
    for (int i = 0; i < 3; i++) widthDraft[i] = adc[i];
    widthStaged = true;
    return true;
}

bool PloughSettings::Save(SettingsOwner who) {
    if (who == SettingsOwner::None || who != owner) return false;

    // Shares first: SetShares() re-checks the width against the new number
    // of shares (#247), as the wizard's shares step did.
    implement->SetShares(byte(draft[uint8_t(PloughSetting::Shares)]));
    implement->SetSwap(draft[uint8_t(PloughSetting::Swap)] != 0);
    tractor->SetInversion(draft[uint8_t(PloughSetting::HitchInversion)] != 0);
    implement->SetBypass(draft[uint8_t(PloughSetting::Bypass)] != 0);
    implement->SetDeadband(byte(draft[uint8_t(PloughSetting::Deadband)]));
    implement->SetMaxCorrectionPerShare(byte(draft[uint8_t(PloughSetting::MaxCorrectionPerShare)]));
#ifdef ISOBUS
    calibration->SetPreferredVt(byte(draft[uint8_t(PloughSetting::PreferredVt)]));
#endif
    guidance->SetRtkQuality(byte(draft[uint8_t(PloughSetting::RtkQuality)]));
    if (widthStaged) {
        for (int i = 0; i < 3; i++) implement->SetPositionCalibrationValue(i, widthDraft[i]);
    }

    // The wizard's COMPLETE-accept: the same three commits, so the same bytes.
    implement->CommitCalibration();
    tractor->CommitCalibration();
    calibration->CommitGuidanceCalibration();

    owner = SettingsOwner::None;
    widthStaged = false;
    return true;
}

void PloughSettings::Cancel(SettingsOwner who) {
    if (who == SettingsOwner::None || who != owner) return;
    owner = SettingsOwner::None;
    widthStaged = false;
}

}  // namespace triton
