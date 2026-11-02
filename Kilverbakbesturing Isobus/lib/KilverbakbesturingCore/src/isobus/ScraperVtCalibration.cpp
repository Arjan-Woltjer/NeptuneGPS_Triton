/*
  ScraperVtCalibration - the kilverbak's calibration page on the VT
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
#include "ScraperVtCalibration.hpp"

#ifdef ISOBUS

#include "VTObjectPool.hpp"

namespace triton
{

void ScraperVtCalibration::OnCalibrateKey() {
    if (open) return;
    if (!settings->Begin(SettingsOwner::Vt)) {
        serialDebug->println("VT: calibration refused, the cab wizard is open");
        return;
    }
    implement->Stop();
    open = true;
    forgetShown();
    wantedMask = Scraper_CalDataMask;
    serialDebug->println("VT: calibration page open");
}

bool ScraperVtCalibration::OnSoftKey(uint16_t keyObjectId) {
    if (keyObjectId != Key_CalSave && keyObjectId != Key_CalCancel) return false;
    // A page left on the VT after a reconnect has no draft behind it: either
    // key just goes back to the working screen.
    wantedMask = Scraper_DataMask;
    if (!open) return false;
    const bool save = keyObjectId == Key_CalSave;
    if (save) {
        settings->Save(SettingsOwner::Vt);
        serialDebug->println("VT: calibration saved");
    }
    else {
        settings->Cancel(SettingsOwner::Vt);
        serialDebug->println("VT: calibration cancelled");
    }
    open = false;
    return save;
}

void ScraperVtCalibration::OnNumericValue(uint16_t objectId, uint32_t value) {
    if (!open) return;
    if (objectId < Input_Cal0 || objectId >= Input_Cal0 + ScraperSettings::kCount) return;
    const uint8_t i = uint8_t(objectId - Input_Cal0);
    const ScraperSetting id = ScraperSetting(i);
    int16_t setting = 0;
    if (!FromVt(id, value, setting) || !settings->Set(SettingsOwner::Vt, id, setting)) {
        serialDebug->print("VT: calibration value refused for ");
        serialDebug->println(ScraperSettings::Line(id).key);
    }
    // The VT shows what the operator entered: resend the draft's value if it
    // differs.
    shown[i] = value;
}

void ScraperVtCalibration::OnConnectionLost() {
    if (open) {
        settings->Cancel(SettingsOwner::Vt);
        serialDebug->println("VT: calibration dropped, the VT went away");
    }
    open = false;
    wantedMask = 0;
    forgetShown();
}

void ScraperVtCalibration::Update(ScraperVtOutput& out) {
    if (open) {
        for (uint8_t i = 0; i < ScraperSettings::kCount; i++) {
            const ScraperSetting id = ScraperSetting(i);
            const uint32_t vtValue = ToVt(id, settings->Get(id));
            if (vtValue != shown[i] && out.ChangeNumericValue(Input_Cal0 + i, vtValue)) shown[i] = vtValue;
        }
        // The stored points, biased as the main screen's heights; the
        // distance pegs at its field's range.
        float distance = implement->GetRefDistance();
        if (distance > 99999.0f) distance = 99999.0f;
        const uint32_t points[kPointRows] = {
            uint32_t(int32_t(implement->GetRefAHeight()) + kLargeBias),
            uint32_t(int32_t(implement->GetRefBHeight()) + kLargeBias),
            uint32_t(int32_t(distance + 0.5f) + kLargeBias),
        };
        const uint16_t pointVariables[kPointRows] = { Var_CalRefA, Var_CalRefB, Var_CalRefDistance };
        for (uint8_t p = 0; p < kPointRows; p++) {
            if (points[p] != shownPoints[p] && out.ChangeNumericValue(pointVariables[p], points[p])) {
                shownPoints[p] = points[p];
            }
        }
    }
    if (wantedMask != 0 && out.ChangeActiveMask(wantedMask)) wantedMask = 0;
}

uint32_t ScraperVtCalibration::ToVt(ScraperSetting id, int16_t value) {
    const ScraperSettingLine& line = ScraperSettings::Line(id);
    switch (line.kind) {
        case SettingKind::RtkQuality:
            return value == 4 ? 1 : 0;
        case SettingKind::YesNo:
            return value != 0 ? 1 : 0;
        default:
            return uint32_t(int32_t(value) + CalInputBias(line.lowest));
    }
}

bool ScraperVtCalibration::FromVt(ScraperSetting id, uint32_t vtValue, int16_t& value) {
    const ScraperSettingLine& line = ScraperSettings::Line(id);
    switch (line.kind) {
        case SettingKind::RtkQuality:
            if (vtValue > 1) return false;
            value = vtValue == 1 ? 4 : 2;
            return true;
        case SettingKind::YesNo:
            if (vtValue > 1) return false;
            value = int16_t(vtValue);
            return true;
        default: {
            if (vtValue > 0x7FFF) return false;
            value = int16_t(int32_t(vtValue) - CalInputBias(line.lowest));
            return true;
        }
    }
}

void ScraperVtCalibration::forgetShown() {
    for (uint8_t i = 0; i < ScraperSettings::kCount; i++) shown[i] = kNotSent;
    for (uint8_t p = 0; p < kPointRows; p++) shownPoints[p] = kNotSent;
}

}  // namespace triton

#endif  // ISOBUS
