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
#pragma once

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>
#include <EEPROM.h>

#include "../implement/ImplementScraper.hpp"
#include "InterfaceI2CLCD.hpp"
#include "../InterfaceScraper.hpp"
#include "GuidanceSource.hpp"
#include "../config/LanguageScraper.hpp"
#include "ScraperSettings.hpp"
#ifdef ISOBUS
#include "PreferredVt.hpp"
#endif

namespace triton
{

// The cab-LCD calibration wizard as a step runner (NeptuneGPS_Triton#289,
// the plough's #31 pattern): Start() opens it, then main.cpp's loop() calls
// Step() once per pass instead of InterfaceScraper::Update(), so the CAN
// stack and the receiver keep running while it is open -- a reference point
// is taken from a live position, and the address claim never stalls.
//
// The screens are the Kilverbakbesturing wizard's own, in its order and with
// its buttons (+ accepts, - declines, both held finishes an edit):
//   Keuzemenu: + sets the reference points, - calibrates.
//   + : reference A, then reference B, each captured on +.
//   - : one screen per ScraperSettings line, then "Calibratie afronden",
//       whose + saves the draft and whose - drops it.
// While it is open the control holds: Start() stops the valves and nothing
// moves them until the wizard closes. The caller (main.cpp) starts it on
// InterfaceScraper::GetButtons() == 2; this class never triggers itself.
class CalibrationScraper {
public:
    CalibrationScraper(InterfaceI2CLCD* lcd, ImplementScraper* implement, GuidanceSource* guidance,
                        InterfaceScraper* interface);

    // Opens the wizard on Keuzemenu. False, doing nothing, while another
    // front end (the VT page, #292) holds the settings session.
    bool Start();
    bool IsActive() const { return screen != kIdle; }
    // One pass of the open wizard; nothing when it is closed.
    void Step();

    ScraperSettings& GetSettings() { return settings; }

    // The guidance values this board persists. GuidanceSource is a shared
    // data model with no storage of its own (NeptuneGPS_Triton#78), so this
    // class owns their bytes, where the plough keeps them: the receiver rate
    // index the boot autodetect (InterfaceGuidance) finds, and the GGA
    // quality that counts as RTK (#289; Kilverbakbesturing left byte 11
    // alone). Read at construction, written by the settings' Save and by
    // main.cpp after a detect found a different rate.
    static constexpr int kEepromGpsBaudIndex = 10;
    static constexpr int kEepromRtkQuality   = 11;

    byte GetGpsBaudIndex() const   { return gpsBaudIndex; }
    void SetGpsBaudIndex(byte idx) { gpsBaudIndex = idx % 8; }
    void CommitGuidanceCalibration();

#ifdef ISOBUS
    // The preferred VT number (#255), held until CommitGuidanceCalibration().
    byte GetPreferredVt() const   { return preferredVt; }
    void SetPreferredVt(byte vt)  { preferredVt = vt <= kHighestPreferredVt ? vt : 0; }
#endif

    void PrintCalibrationData(Stream* serial);

private:
    InterfaceI2CLCD*  lcd;
    ImplementScraper* implement;
    GuidanceSource*   guidance;
    InterfaceScraper* interface;

    byte gpsBaudIndex = 0;   // 4800 x {1,2,3,4,6,8,12,24}; 0 until stored or detected
#ifdef ISOBUS
    byte preferredVt = 0;    // #255: the VT number, 0 = none
#endif

    ScraperSettings settings;

    // The wizard's screens: Keuzemenu, the two reference points, one per
    // settings line in table order, then COMPLETE.
    static constexpr uint8_t kChoice   = 0;
    static constexpr uint8_t kRefA     = 1;
    static constexpr uint8_t kRefB     = 2;
    static constexpr uint8_t kSetting0 = 3;
    static constexpr uint8_t kComplete = kSetting0 + ScraperSettings::kCount;
    static constexpr uint8_t kIdle     = 0xFF;
    // The pause after a screen's done / declined line, as the wizard's
    // delay(1000) did; Step() just waits it out.
    static constexpr unsigned long kPauseMs = 1000;

    enum class Phase : uint8_t {
        AskRelease,    // title shown, waiting for both buttons up
        Ask,           // + accepts, - declines (Keuzemenu: + references, - calibrate)
        EditRelease,   // edit screen shown, waiting for both buttons up
        Edit,          // + / - change the value, both held finishes
        Pause,         // done / declined shown for kPauseMs
    };

    uint8_t       screen = kIdle;
    Phase         phase = Phase::AskRelease;
    int16_t       value = 0;
    unsigned long pauseStartMs = 0;

    bool loadGuidanceCalibration();
    void enterScreen(uint8_t next);
    void choose(int buttons);
    void accept();
    void decline();
    void afterPause();
    void finish();
    void pause();
    void drawValue();
    ScraperSetting setting() const { return ScraperSetting(screen - kSetting0); }
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
