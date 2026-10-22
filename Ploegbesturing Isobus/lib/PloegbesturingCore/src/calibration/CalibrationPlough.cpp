/*
  CalibrationPlough - LCD/button calibration wizard for the MeijWorks plough interface
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
#include "CalibrationPlough.hpp"
#include "FlashMem.hpp"

namespace triton
{

CalibrationPlough::CalibrationPlough(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlough* implement,
                                      VehicleTractor* tractor, GuidanceSource* guidance, InterfacePlough* interface)
    : serialDebug(serialDebug), lcd(lcd), implement(implement), tractor(tractor), guidance(guidance), interface(interface),
      settings(implement, tractor, guidance, this), width(implement) {
    loadGuidanceCalibration();
}

// Returns true when at least one stored value was found. An erased byte
// (255) leaves the default: rate index 0, and GuidanceSource's own RTK
// fixed (4). SetRtkQuality() clamps anything that is not 4 or 2 back to 4,
// so a stale or corrupt byte cannot leak through.
bool CalibrationPlough::loadGuidanceCalibration() {
    bool found = false;
    const byte storedBaud = EEPROM.read(kEepromGpsBaudIndex);
    if (storedBaud != 255) {
        SetGpsBaudIndex(storedBaud);
        found = true;
    }
    const byte storedRtk = EEPROM.read(kEepromRtkQuality);
    if (storedRtk != 255) {
        guidance->SetRtkQuality(storedRtk);
        found = true;
    }
#ifdef ISOBUS
    // Not a calibration value: "found" stays as it was.
    preferredVt = ReadPreferredVt();
#endif
    return found;
}

void CalibrationPlough::CommitGuidanceCalibration() {
    EEPROM.write(kEepromGpsBaudIndex, gpsBaudIndex);
    EEPROM.write(kEepromRtkQuality, guidance->GetRtkQuality());
#ifdef ISOBUS
    WritePreferredVt(preferredVt);
#endif
}

FLASHMEM void CalibrationPlough::PrintCalibrationData() {
    static const byte rates[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };
    serialDebug->println("=====================================");
    serialDebug->println("Guidance source using following data:");
    serialDebug->println("=====================================");
    serialDebug->println("Baudrate");
    serialDebug->println(rates[gpsBaudIndex % 8] * long(4800));
    serialDebug->println("-------------------------------");
    serialDebug->println("RTK Quality");
    serialDebug->println(guidance->GetRtkQuality());
    serialDebug->println("-------------------------------");
#ifdef ISOBUS
    serialDebug->println("Preferred VT (0 = none)");
    serialDebug->println(preferredVt);
    serialDebug->println("-------------------------------");
#endif
}

// -------------------------------------------
// The calibration wizard, one pass at a time
// -------------------------------------------
// Each screen is as the blocking wizard drew it: a title with + accept and
// - decline; on accept an edit line on row 3 that + and - change and both
// held finishes; then "done" or "declined" for a second. The width
// procedure captures three points, COMPLETE saves or drops the draft.

// Title and row-3 text per settings line, in table order. nullptr: row 3
// carries only the value (yes/no, RTK quality).
static const char* const kTitles[PloughSettings::kCount] = {
    L_CAL_SHARES, L_CAL_SWAP, L_CAL_INVERSION, L_CAL_BYPASS, L_CAL_DEADBAND, L_CAL_MAXCOR,
#ifdef ISOBUS
    L_CAL_VT,
#endif
    L_CAL_QUAL,
};
static const char* const kEditLines[PloughSettings::kCount] = {
    L_CAL_SHARES_AD, L_CAL_SWAP_AD, nullptr, nullptr, L_CAL_DEADBAND_AD, L_CAL_MAXCOR_AD,
#ifdef ISOBUS
    L_CAL_VT_AD,
#endif
    nullptr,
};

FLASHMEM bool CalibrationPlough::Start() {
    if (IsActive()) return true;
    if (!settings.Begin(SettingsOwner::Cab)) return false;
    implement->Stop();
    enterScreen(kWidth);
    return true;
}

FLASHMEM void CalibrationPlough::Step() {
    if (!IsActive()) return;

    // Keeps the sensor readings live: the VT shows them, and the width
    // procedure's end shutoff watches them (the blocking wizard never
    // refreshed them, so 3 s of driving latched it however far the plough
    // moved). In manual, Update() leaves the width alone.
    implement->Update(2, 0);
    // One changed LCD character per pass, as the main screen does: a
    // whole screen at once is ~150 ms of I2C on the board, a pass that
    // long fills most of the CAN receive queue (bench 2026-10-22). A new
    // screen builds up in a fraction of a second this way.
    lcd->WriteScreen(1);

    switch (phase) {
        case Phase::AskRelease:
        case Phase::EditRelease:
        case Phase::PointRelease:
            if (interface->CheckButtons(0, 0) == 0) {
                phase = phase == Phase::AskRelease  ? Phase::Ask
                      : phase == Phase::EditRelease ? Phase::Edit
                                                    : Phase::Point;
            }
            break;

        case Phase::Ask: {
            const short int b = interface->CheckButtons(0, 0);
            if (b == -1) decline();
            else if (b == 1) accept();
            break;
        }

        case Phase::Edit: {
            const short int b = interface->CheckButtons(0, 255);
            if (b == 2) {
                lcd->WriteBuffer(L_CAL_DONE, 1);
                lcd->WriteBuffer(L_BLANK, 2);
                drawValue();
                pause();
                break;
            }
            value = PloughSettings::Step(setting(), value, b);
            settings.Set(SettingsOwner::Cab, setting(), value);
            drawValue();
            break;
        }

        case Phase::Point: {
            const short int b = interface->CheckButtons(0, 0);
            if (b == 2) {
                if (!width.Capture()) {
                    showPoint();
                    break;
                }
                // A capture the boot load would throw away is not staged:
                // the plough keeps the width it had.
                const bool staged = settings.StageWidthCalibration(SettingsOwner::Cab, width.Readings());
                lcd->WriteBuffer(staged ? L_CAL_DONE : L_CAL_DECLINED, 1);
                lcd->WriteBuffer(L_BLANK, 2);
                lcd->WriteBuffer(L_BLANK, 3);
                pause();
                break;
            }
            // The only valve movement while the wizard is open (WidthProcedure).
            width.Drive(b);
            lcd->WriteBuffer(b == 1 ? '>' : b == -1 ? '<' : ' ', 3, 19);
            break;
        }

        case Phase::Pause:
            if (millis() - pauseStartMs >= kPauseMs) enterScreen(screen + 1);
            break;
    }
}

#ifdef ISOBUS
FLASHMEM bool CalibrationPlough::HoldWhileVtCalibrates() {
    const bool vtHolds = settings.GetOwner() == SettingsOwner::Vt;
    if (vtHolds && !holdingForVt) {
        implement->Stop();
        lcd->WriteBuffer(L_CAL_ON_VT, 0);
        lcd->WriteBuffer(L_CAL_ON_VT_2, 1);
        lcd->WriteBuffer(L_BLANK, 2);
        lcd->WriteBuffer(L_BLANK, 3);
        holdingForVt = true;
    }
    else if (!vtHolds && holdingForVt) {
        holdingForVt = false;
        interface->UpdateScreen(1);
    }
    if (!vtHolds) return false;
    implement->Update(2, 0);
    lcd->WriteScreen(1);
    return true;
}
#endif

FLASHMEM void CalibrationPlough::enterScreen(uint8_t next) {
    screen = next;
    if (screen > kComplete) {
        finish();
        return;
    }
    const char* title = screen == kWidth    ? L_CAL_POS
                      : screen == kComplete ? L_CAL_COMPLETE
                                            : kTitles[screen - 1];
    lcd->WriteBuffer(title, 0);
    lcd->WriteBuffer(L_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L_CAL_DECLINE, 2);
    lcd->WriteBuffer(L_BLANK, 3);
    phase = Phase::AskRelease;
}

FLASHMEM void CalibrationPlough::accept() {
    if (screen == kComplete) {
        settings.Save(SettingsOwner::Cab);
        lcd->WriteBuffer(L_CAL_DDONE, 1);
        lcd->WriteBuffer(L_CAL_SAVE, 2);
        pause();
        return;
    }
    lcd->WriteBuffer(L_CAL_ADJUST, 1);
    lcd->WriteBuffer(L_CAL_ENTER, 2);
    if (screen == kWidth) {
        lcd->WriteBuffer(L_CAL_POS_AD, 3);
        width.Begin();
        showPoint();
        return;
    }
    value = settings.Get(setting());
    if (setting() == PloughSetting::MaxCorrectionPerShare) {
        // Onto the 5 mm grid the + and - steps keep to, as the wizard did.
        value = (value / 5) * 5;
        settings.Set(SettingsOwner::Cab, setting(), value);
    }
    const char* editLine = kEditLines[screen - 1];
    lcd->WriteBuffer(editLine != nullptr ? editLine : L_BLANK, 3);
    drawValue();
    phase = Phase::EditRelease;
}

FLASHMEM void CalibrationPlough::decline() {
    lcd->WriteBuffer(L_CAL_DECLINED, 1);
    if (screen == kComplete) {
        settings.Cancel(SettingsOwner::Cab);
        lcd->WriteBuffer(L_CAL_NOSAVE, 2);
    }
    else {
        lcd->WriteBuffer(L_BLANK, 2);
    }
    pause();
}

FLASHMEM void CalibrationPlough::finish() {
    screen = kIdle;
    // COMPLETE has already saved or dropped the draft; this is a no-op then.
    settings.Cancel(SettingsOwner::Cab);
    interface->UpdateScreen(1);
}

FLASHMEM void CalibrationPlough::pause() {
    pauseStartMs = millis();
    phase = Phase::Pause;
}

// The target of the width point being captured: sign and two digits in cm.
FLASHMEM void CalibrationPlough::showPoint() {
    const short int target = width.Target();
    lcd->WriteBuffer(target < 0 ? '-' : ' ', 3, 12);
    lcd->WriteBuffer(abs(target / 10) + '0', 3, 13);
    lcd->WriteBuffer(abs(target) % 10 + '0', 3, 14);
    phase = Phase::PointRelease;
}

FLASHMEM void CalibrationPlough::drawValue() {
    switch (setting()) {
        case PloughSetting::Shares:
        case PloughSetting::Deadband:
            lcd->WriteBuffer(value / 10 + '0', 3, 15);
            lcd->WriteBuffer(value % 10 + '0', 3, 16);
            break;
        case PloughSetting::MaxCorrectionPerShare:
            lcd->WriteBuffer(value / 100 + '0', 3, 14);
            lcd->WriteBuffer((value / 10) % 10 + '0', 3, 15);
            lcd->WriteBuffer(value % 10 + '0', 3, 16);
            break;
        case PloughSetting::Swap: {
            // The side this swap gives: the plough-side input XOR the swap.
            const bool input = implement->GetSide() != implement->GetSwap();
            lcd->WriteBuffer((input != (value != 0)) ? 'L' : 'R', 3, 16);
            break;
        }
        case PloughSetting::HitchInversion:
        case PloughSetting::Bypass:
            lcd->WriteBuffer(value ? L_CAL_YES : L_CAL_NO, 3);
            break;
        default:  // the preferred VT, the RTK quality: one digit
            lcd->WriteBuffer(value + '0', 3, 16);
            break;
    }
}

}  // namespace triton
