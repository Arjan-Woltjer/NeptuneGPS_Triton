/*
  IsobusVtCalibration - the plough's calibration page on the VT
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
#include "IsobusVtCalibration.hpp"

#include "VTObjectPool.hpp"
#include "../FlashMem.hpp"

namespace triton
{

FLASHMEM void IsobusVtCalibration::OnCalibrateKey() {
    if (open) return;
    if (!settings->Begin(SettingsOwner::Vt)) {
        serialDebug->println("VT: calibration refused, the cab wizard is open");
        return;
    }
    implement->Stop();
    open = true;
    forgetShown();
    wantedMask = Plough_CalDataMask;
    serialDebug->println("VT: calibration page open");
}

FLASHMEM bool IsobusVtCalibration::OnSoftKey(uint16_t keyObjectId) {
    if (keyObjectId != Key_CalSave && keyObjectId != Key_CalCancel) return false;
    // A page left on the VT after a reconnect has no draft behind it: either
    // key just goes back to the main mask.
    wantedMask = Plough_DataMask;
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

FLASHMEM void IsobusVtCalibration::OnNumericValue(uint16_t objectId, uint32_t value) {
    if (!open) return;
    if (objectId < Input_Cal0 || objectId >= Input_Cal0 + PloughSettings::kCount) return;
    const uint8_t i = uint8_t(objectId - Input_Cal0);
    const PloughSetting id = PloughSetting(i);
    if (!settings->Set(SettingsOwner::Vt, id, FromVt(id, value))) {
        serialDebug->print("VT: calibration value refused for ");
        serialDebug->println(PloughSettings::Line(id).key);
    }
    // The VT shows what it was sent last, or what the operator entered:
    // either way, resend the draft's value if it differs.
    shown[i] = value;
}

FLASHMEM void IsobusVtCalibration::OnConnectionLost() {
    if (open) {
        settings->Cancel(SettingsOwner::Vt);
        serialDebug->println("VT: calibration dropped, the VT went away");
    }
    open = false;
    wantedMask = 0;
    forgetShown();
}

FLASHMEM void IsobusVtCalibration::Update(VtCalibrationOutput& out) {
    if (open) {
        for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
            const PloughSetting id = PloughSetting(i);
            const uint32_t vtValue = ToVt(id, settings->Get(id));
            if (vtValue != shown[i] && out.ChangeNumericValue(Input_Cal0 + i, vtValue)) {
                shown[i] = vtValue;
            }
        }
    }
    if (wantedMask != 0 && out.ChangeActiveMask(wantedMask)) {
        wantedMask = 0;
    }
}

FLASHMEM uint32_t IsobusVtCalibration::ToVt(PloughSetting id, int16_t value) const {
    switch (PloughSettings::Line(id).kind) {
        case SettingKind::PloughSide: {
            // The side the swap gives: the plough-side input XOR the swap,
            // as the cab wizard shows it (L = 1, R = 0).
            const bool input = implement->GetSide() != implement->GetSwap();
            return (input != (value != 0)) ? 1 : 0;
        }
        case SettingKind::RtkQuality:
            return value == 4 ? 1 : 0;
        default:
            return uint32_t(value);
    }
}

FLASHMEM int16_t IsobusVtCalibration::FromVt(PloughSetting id, uint32_t vtValue) const {
    switch (PloughSettings::Line(id).kind) {
        case SettingKind::PloughSide: {
            const bool input = implement->GetSide() != implement->GetSwap();
            return (input != (vtValue != 0)) ? 1 : 0;
        }
        case SettingKind::RtkQuality:
            return vtValue == 1 ? 4 : vtValue == 0 ? 2 : -1;
        default:
            return vtValue > 0x7FFF ? -1 : int16_t(vtValue);
    }
}

FLASHMEM void IsobusVtCalibration::forgetShown() {
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) shown[i] = kNotSent;
}

}  // namespace triton
