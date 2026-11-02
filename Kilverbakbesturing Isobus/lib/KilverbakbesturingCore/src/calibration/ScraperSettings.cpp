/*
  ScraperSettings - the kilverbak's settings as one table, with a draft that
  only reaches the implement and EEPROM on Save
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
#include "ScraperSettings.hpp"

#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <string.h>

#include "CalibrationScraper.hpp"

namespace triton
{

// The bounds are the ones the boot loads apply: ImplementScraper's
// readCalibrationData() (142, 146) and readSettings() (137, 139),
// CalibrationScraper's (11) and ReadPreferredVt() (14). test_ScraperSettings
// checks every line against those loads.
static const ScraperSettingLine kLines[ScraperSettings::kCount] = {
    { ScraperSetting::Slope,       "slope",     SettingKind::Number,     -99, 99, 1, 146 },
    { ScraperSetting::ErrorMargin, "margin",    SettingKind::Number,     0,   10, 1, 142 },
    { ScraperSetting::Swap,        "swap",      SettingKind::YesNo,      0,   1,  1, ImplementScraper::kEepromSwap },
    { ScraperSetting::SpeedGate,   "speedgate", SettingKind::YesNo,      0,   1,  1, ImplementScraper::kEepromSpeedGate },
#ifdef ISOBUS
    { ScraperSetting::PreferredVt, "vt",        SettingKind::Number,     0,   kHighestPreferredVt, 1, kEepromPreferredVt },
#endif
    { ScraperSetting::RtkQuality,  "rtkq",      SettingKind::RtkQuality, 2,   4,  2, CalibrationScraper::kEepromRtkQuality },
};

const ScraperSettingLine& ScraperSettings::Line(ScraperSetting id) {
    return kLines[uint8_t(id)];
}

const ScraperSettingLine* ScraperSettings::Find(const char* key) {
    for (uint8_t i = 0; i < kCount; i++) {
        if (strcmp(kLines[i].key, key) == 0) return &kLines[i];
    }
    return nullptr;
}

bool ScraperSettings::IsValid(ScraperSetting id, int16_t value) {
    const ScraperSettingLine& line = Line(id);
    if (line.kind == SettingKind::RtkQuality) return value == 4 || value == 2;
    return value >= line.lowest && value <= line.highest;
}

int16_t ScraperSettings::Step(ScraperSetting id, int16_t value, int direction) {
    const ScraperSettingLine& line = Line(id);
    if (line.kind != SettingKind::Number) {
        if (direction == 1) return line.highest;
        if (direction == -1) return line.lowest;
        return value;
    }
    if (direction == 1 && value + line.step <= line.highest) return value + line.step;
    if (direction == -1 && value - line.step >= line.lowest) return value - line.step;
    return value;
}

ScraperSettings::ScraperSettings(ImplementScraper* implement, GuidanceSource* guidance,
                                 CalibrationScraper* calibration)
    : implement(implement), guidance(guidance), calibration(calibration) {}

int16_t ScraperSettings::held(ScraperSetting id) const {
    switch (id) {
        case ScraperSetting::Slope:       return implement->GetSlope();
        case ScraperSetting::ErrorMargin: return implement->GetError();
        case ScraperSetting::Swap:        return implement->GetSwap() ? 1 : 0;
        case ScraperSetting::SpeedGate:   return implement->GetSpeedGate() ? 1 : 0;
#ifdef ISOBUS
        case ScraperSetting::PreferredVt: return calibration->GetPreferredVt();
#endif
        case ScraperSetting::RtkQuality:  return guidance->GetRtkQuality();
        default:                          return 0;
    }
}

bool ScraperSettings::Begin(SettingsOwner who) {
    if (who == SettingsOwner::None) return false;
    if (owner == who) return true;
    if (owner != SettingsOwner::None) return false;
    for (uint8_t i = 0; i < kCount; i++) draft[i] = held(ScraperSetting(i));
    owner = who;
    return true;
}

int16_t ScraperSettings::Get(ScraperSetting id) const {
    return owner != SettingsOwner::None ? draft[uint8_t(id)] : held(id);
}

bool ScraperSettings::Set(SettingsOwner who, ScraperSetting id, int16_t value) {
    if (who == SettingsOwner::None || who != owner) return false;
    if (!IsValid(id, value)) return false;
    draft[uint8_t(id)] = value;
    return true;
}

bool ScraperSettings::Save(SettingsOwner who) {
    if (who == SettingsOwner::None || who != owner) return false;

    implement->SetSlope(draft[uint8_t(ScraperSetting::Slope)]);
    implement->SetError(byte(draft[uint8_t(ScraperSetting::ErrorMargin)]));
    implement->SetSwap(draft[uint8_t(ScraperSetting::Swap)] != 0);
    implement->SetSpeedGate(draft[uint8_t(ScraperSetting::SpeedGate)] != 0);
#ifdef ISOBUS
    calibration->SetPreferredVt(byte(draft[uint8_t(ScraperSetting::PreferredVt)]));
#endif
    guidance->SetRtkQuality(byte(draft[uint8_t(ScraperSetting::RtkQuality)]));

    // The implement's own bytes and the guidance bytes: no wipe (#289).
    implement->CommitCalibration();
    calibration->CommitGuidanceCalibration();

    owner = SettingsOwner::None;
    return true;
}

void ScraperSettings::Cancel(SettingsOwner who) {
    if (who == SettingsOwner::None || who != owner) return;
    owner = SettingsOwner::None;
}

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
