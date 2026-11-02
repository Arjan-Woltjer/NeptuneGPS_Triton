/*
  CalibrationScraper - LCD/button calibration wizard for the MeijWorks scraper interface
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
#include "CalibrationScraper.hpp"

namespace triton
{

CalibrationScraper::CalibrationScraper(InterfaceI2CLCD* lcd, ImplementScraper* implement, GuidanceSource* guidance,
                                        InterfaceScraper* interface)
    : lcd(lcd), implement(implement), guidance(guidance), interface(interface),
      settings(implement, guidance, this) {
    loadGuidanceCalibration();
}

// Returns true when at least one stored value was found. An erased byte
// (255) leaves the default: rate index 0 (4800, the common NMEA default), and
// GuidanceSource's own RTK fixed (4). SetRtkQuality() turns anything that is
// not 4 or 2 back into 4, so a stale or corrupt byte cannot leak through.
bool CalibrationScraper::loadGuidanceCalibration() {
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

void CalibrationScraper::CommitGuidanceCalibration() {
    EEPROM.write(kEepromGpsBaudIndex, gpsBaudIndex);
    EEPROM.write(kEepromRtkQuality, guidance->GetRtkQuality());
#ifdef ISOBUS
    WritePreferredVt(preferredVt);
#endif
}

void CalibrationScraper::PrintCalibrationData(Stream* serial) {
    static const byte rates[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };
    serial->println("=====================================");
    serial->println("Guidance source using following data:");
    serial->println("=====================================");
    serial->println("Baudrate");
    serial->println(rates[gpsBaudIndex % 8] * long(4800));
    serial->println("-------------------------------");
    serial->println("RTK Quality");
    serial->println(guidance->GetRtkQuality());
    serial->println("-------------------------------");
#ifdef ISOBUS
    serial->println("Preferred VT (0 = none)");
    serial->println(preferredVt);
    serial->println("-------------------------------");
#endif
}

// -------------------------------------------
// The calibration wizard, one pass at a time
// -------------------------------------------
// Each settings screen is as the blocking wizard drew it: a title with
// + accept and - decline; on accept an edit line on row 3 that + and -
// change and both held finishes; then "done" or "declined" for a second.

// Title and row-3 text per settings line, in table order. nullptr: row 3
// carries only the value (yes / no).
static const char* const kTitles[ScraperSettings::kCount] = {
    L_CAL_SLOPE, L_CAL_MARGIN, L_CAL_SWAP, L_CAL_SPEEDGATE,
#ifdef ISOBUS
    L_CAL_VT,
#endif
    L_CAL_QUAL,
};
static const char* const kEditLines[ScraperSettings::kCount] = {
    L_CAL_SLOPE_AD, L_CAL_MARGIN_AD, nullptr, nullptr,
#ifdef ISOBUS
    L_CAL_VT_AD,
#endif
    L_CAL_QUAL_AD,
};

bool CalibrationScraper::Start() {
    if (IsActive()) return true;
    if (!settings.Begin(SettingsOwner::Cab)) return false;
    implement->Stop();
    enterScreen(kChoice);
    return true;
}

void CalibrationScraper::Step() {
    if (!IsActive()) return;

    // One changed LCD character per pass, as the main screen does, so a pass
    // stays short and the CAN receive queue keeps draining (the plough's
    // bench finding, 2026-10-22). A new screen builds up in a fraction of a
    // second this way.
    lcd->WriteScreen(1);

    switch (phase) {
        case Phase::AskRelease:
        case Phase::EditRelease:
            if (interface->CheckButtons(0, 0) == 0) {
                phase = phase == Phase::AskRelease ? Phase::Ask : Phase::Edit;
            }
            break;

        case Phase::Ask: {
            const int b = interface->CheckButtons(0, 0);
            if (screen == kChoice) choose(b);
            else if (b == -1) decline();
            else if (b == 1) accept();
            break;
        }

        case Phase::Edit: {
            const int b = interface->CheckButtons(0, 255);
            if (b == 2) {
                lcd->WriteBuffer(L_CAL_DONE, 1);
                lcd->WriteBuffer(L_BLANK, 2);
                drawValue();
                pause();
                break;
            }
            value = ScraperSettings::Step(setting(), value, b);
            settings.Set(SettingsOwner::Cab, setting(), value);
            drawValue();
            break;
        }

        case Phase::Pause:
            if (millis() - pauseStartMs >= kPauseMs) afterPause();
            break;
    }
}

void CalibrationScraper::enterScreen(uint8_t next) {
    screen = next;
    if (screen == kChoice) {
        lcd->WriteBuffer(L_CHOICE, 0);
        lcd->WriteBuffer(L_REF, 1);
        lcd->WriteBuffer(L_CAL, 2);
        lcd->WriteBuffer(L_BLANK, 3);
        phase = Phase::AskRelease;
        return;
    }
    const char* title = screen == kRefA || screen == kRefB ? L_CAL_REF
                      : screen == kComplete                ? L_CAL_COMPLETE
                                                           : kTitles[screen - kSetting0];
    lcd->WriteBuffer(title, 0);
    if (screen == kRefB) lcd->WriteBuffer('B', 0, 16);
    lcd->WriteBuffer(L_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L_CAL_DECLINE, 2);
    lcd->WriteBuffer(L_BLANK, 3);
    phase = Phase::AskRelease;
}

// Keuzemenu: + sets the reference points, - calibrates. Nothing else leaves
// it, as in the blocking wizard.
void CalibrationScraper::choose(int buttons) {
    if (buttons == 1) enterScreen(kRefA);
    else if (buttons == -1) enterScreen(kSetting0);
}

void CalibrationScraper::accept() {
    if (screen == kRefA || screen == kRefB) {
        // Captured at once from the live position (the loop keeps the
        // receiver running); this also zeroes the offset, as it always did.
        if (screen == kRefA) implement->SetRefA();
        else implement->SetRefB();
        lcd->WriteBuffer(L_CAL_REF_AD, 1);
        lcd->WriteBuffer(L_BLANK, 2);
        lcd->WriteBuffer(L_BLANK, 3);
        pause();
        return;
    }
    if (screen == kComplete) {
        settings.Save(SettingsOwner::Cab);
        lcd->WriteBuffer(L_CAL_DDONE, 1);
        lcd->WriteBuffer(L_CAL_SAVE, 2);
        pause();
        return;
    }
    lcd->WriteBuffer(L_CAL_ADJUST, 1);
    lcd->WriteBuffer(L_CAL_ENTER, 2);
    value = settings.Get(setting());
    const char* editLine = kEditLines[screen - kSetting0];
    lcd->WriteBuffer(editLine != nullptr ? editLine : L_BLANK, 3);
    drawValue();
    phase = Phase::EditRelease;
}

void CalibrationScraper::decline() {
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

// The pause is over: the next screen. After reference B and after COMPLETE
// the wizard closes.
void CalibrationScraper::afterPause() {
    if (screen == kRefB || screen == kComplete) finish();
    else enterScreen(screen + 1);
}

void CalibrationScraper::finish() {
    screen = kIdle;
    // COMPLETE has already saved or dropped the draft; this is a no-op then.
    settings.Cancel(SettingsOwner::Cab);
    interface->UpdateScreen(1);
}

void CalibrationScraper::pause() {
    pauseStartMs = millis();
    phase = Phase::Pause;
}

void CalibrationScraper::drawValue() {
    switch (setting()) {
        case ScraperSetting::Slope: {
            // Sign and three digits, as the blocking wizard drew it.
            const int magnitude = abs(value);
            lcd->WriteBuffer(value < 0 ? '-' : ' ', 3, 13);
            lcd->WriteBuffer(magnitude / 100 + '0', 3, 14);
            lcd->WriteBuffer((magnitude / 10) % 10 + '0', 3, 15);
            lcd->WriteBuffer(magnitude % 10 + '0', 3, 16);
            break;
        }
        case ScraperSetting::ErrorMargin:
            lcd->WriteBuffer(value / 10 + '0', 3, 15);
            lcd->WriteBuffer(value % 10 + '0', 3, 16);
            break;
        case ScraperSetting::Swap:
        case ScraperSetting::SpeedGate:
            lcd->WriteBuffer(value ? L_CAL_YES : L_CAL_NO, 3);
            break;
        default:  // the preferred VT, the RTK quality: one digit
            lcd->WriteBuffer(value + '0', 3, 16);
            break;
    }
}

}  // namespace triton
