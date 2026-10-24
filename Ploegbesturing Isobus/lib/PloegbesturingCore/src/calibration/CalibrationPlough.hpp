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
#pragma once

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>
#include <EEPROM.h>

#include "../implement/ImplementPlough.hpp"
#include "InterfaceI2CLCD.hpp"
#include "../InterfacePlough.hpp"
#include "GuidanceSource.hpp"
#include "../config/LanguagePlough.hpp"
#include "VehicleTractor.hpp"
#include "PloughSettings.hpp"
#include "WidthProcedure.hpp"
#ifdef ISOBUS
#include "isobus/PreferredVt.hpp"
#include "isobus/LightbarMode.hpp"
#endif

#if defined(ROTATION) || defined(SPEED_L)
#error "The calibration step runner has no rotation or speed screens (#240); add them before enabling ROTATION or SPEED_L."
#endif

namespace triton
{

// The cab-LCD calibration wizard, as a step runner (#31, #240): Start() opens
// it, then main.cpp's loop() calls Step() once per pass instead of
// InterfacePlough::Update(), so the CAN stack, the VT, the TC and guidance keep
// running while it is open. The screens, their order (#260) and the buttons
// are the wizard's own; the values are a PloughSettings draft, applied and
// stored only by COMPLETE's accept. While it runs the control holds: no valve
// moves except the width procedure's, which drives in manual. The caller
// (main.cpp) starts it on InterfacePlough::GetButtons() == 2; this class never
// triggers itself.
class CalibrationPlough {
public:
    CalibrationPlough(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlough* implement,
                       VehicleTractor* tractor, GuidanceSource* guidance, InterfacePlough* interface);

    // Opens the wizard on its first screen. False, doing nothing, while
    // another front end holds the settings session.
    bool Start();
    bool IsActive() const { return screen != kIdle; }
    // One pass of the open wizard; nothing when it is closed.
    void Step();

    PloughSettings& GetSettings() { return settings; }

#ifdef ISOBUS
    // While the VT calibration page (#240) holds the settings session: the
    // control holds, the cab shows that the calibration is on the VT, and
    // main.cpp calls this instead of InterfacePlough::Update(), so the cab
    // buttons do nothing. True while the VT holds it; the pass after it
    // lets go redraws the main screen and returns false.
    bool HoldWhileVtCalibrates();
#endif

    // The two guidance values this board persists, as in the regular
    // Ploegbesturing. GuidanceSource is a shared data model with no storage
    // of its own (NeptuneGPS_Triton#78), so this class owns their EEPROM
    // bytes: the RTK quality the wizard sets, and the receiver rate index the
    // boot autodetect (InterfaceGuidance) finds on the serial builds. Both sit
    // where VehicleGps kept them, so a board coming from that firmware keeps
    // its settings. Read at construction, written by the wizard's Save step
    // and by main.cpp after a detect found a different rate.
    static constexpr int kEepromGpsBaudIndex = 10;
    static constexpr int kEepromRtkQuality   = 11;

    byte GetGpsBaudIndex() const   { return gpsBaudIndex; }
    void SetGpsBaudIndex(byte idx) { gpsBaudIndex = idx % 8; }
    void CommitGuidanceCalibration();

    void PrintCalibrationData();

#ifdef ISOBUS
    // The preferred VT number (#255), held until CommitGuidanceCalibration().
    byte GetPreferredVt() const   { return preferredVt; }
    void SetPreferredVt(byte vt)  { preferredVt = vt <= kHighestPreferredVt ? vt : 0; }

    // The Ag Leader lightbar setting (#272), held until CommitGuidanceCalibration().
    LightbarMode GetLightbarMode() const     { return lightbarMode; }
    void SetLightbarMode(LightbarMode value) { lightbarMode = uint8_t(value) <= kHighestLightbarMode ? value : LightbarMode::Auto; }
#endif

    // One + or - press on a number screen: one step, never past the bounds
    // the boot load accepts, so a saved value always survives the next boot
    // (the #163 trap). `buttons` is GetButtons(): 1 up, -1 down, else none.
    static int StepWithin(int value, int buttons, int step, int lowest, int highest) {
        if (buttons == 1 && value + step <= highest) return value + step;
        if (buttons == -1 && value - step >= lowest) return value - step;
        return value;
    }

private:
    Stream*          serialDebug;
    InterfaceI2CLCD* lcd;
    ImplementPlough* implement;
    VehicleTractor*        tractor;
    GuidanceSource*  guidance;
    InterfacePlough*       interface;

    // 4800 x {1,2,3,4,6,8,12,24}; index 0 (4800, the common NMEA default)
    // until a detect or a stored byte says otherwise.
    byte gpsBaudIndex = 0;
#ifdef ISOBUS
    byte             preferredVt = 0;  // #255: the VT number, 0 = none
    LightbarMode     lightbarMode = LightbarMode::Auto;  // #272
#endif

    PloughSettings settings;
    WidthProcedure width;

    // The wizard's screens: the width procedure, one per settings line in
    // table order, then COMPLETE.
    static constexpr uint8_t kWidth    = 0;
    static constexpr uint8_t kComplete = PloughSettings::kCount + 1;
    static constexpr uint8_t kIdle     = 0xFF;
    // The pause after a screen's done / declined line, as the wizard's
    // delay(1000) did; Step() just waits it out.
    static constexpr unsigned long kPauseMs = 1000;

    enum class Phase : uint8_t {
        AskRelease,    // title shown, waiting for both buttons up
        Ask,           // + accepts, - declines
        EditRelease,   // edit screen shown, waiting for both buttons up
        Edit,          // + / - change the value, both held finishes
        PointRelease,  // width point shown, waiting for both buttons up
        Point,         // + / - drive the plough, both held captures
        Pause,         // done / declined shown for kPauseMs
    };

    uint8_t       screen = kIdle;
    Phase         phase = Phase::AskRelease;
    int16_t       value = 0;
    unsigned long pauseStartMs = 0;
    bool          holdingForVt = false;

    bool loadGuidanceCalibration();
    void enterScreen(uint8_t next);
    void accept();
    void decline();
    void finish();
    void pause();
    void showPoint();
    void drawValue();
    PloughSetting setting() const { return PloughSetting(screen - 1); }
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
