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
#pragma once

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>

#include "../implement/ImplementPlough.hpp"
#include "GuidanceSource.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

class CalibrationPlough;

// One line per setting, in the wizard's order (#260). Every front end (the
// cab wizard, the VT calibration page, #240) edits through this table, so a
// setting's range is defined once (#177 step 4).
enum class PloughSetting : uint8_t {
    Shares,
    Swap,
    HitchInversion,
    Bypass,
    Deadband,
    MaxCorrectionPerShare,
#ifdef ISOBUS
    PreferredVt,
    Lightbar,          // the Ag Leader lightbar emulation (#272)
#endif
    RtkQuality,
    Count
};

// How a value is shown and stepped. RtkQuality has two values only, 4 (RTK
// fixed) and 2 (DGPS, the old Autofarm A5 units). LightbarMode is one of
// Auto (0), On (1), Off (2), stepped like a number.
enum class SettingKind : uint8_t {
    Number,
    YesNo,
    PloughSide,
    RtkQuality,
    LightbarMode,
};

struct PloughSettingLine {
    PloughSetting id;
    const char*   key;            // short ASCII name, unique
    SettingKind   kind;
    int16_t       lowest;
    int16_t       highest;
    int16_t       step;           // one + or - press
    int16_t       eepromAddress;  // where the value's byte is stored
};

// Who holds the open session. One at a time: while the cab wizard edits, the
// VT page cannot, and the other way round.
enum class SettingsOwner : uint8_t {
    None,
    Cab,
    Vt,
};

class PloughSettings {
public:
    static constexpr uint8_t kCount = uint8_t(PloughSetting::Count);

    static const PloughSettingLine& Line(PloughSetting id);
    static const PloughSettingLine* Find(const char* key);

    // The one range check: what an edit may set is exactly what the boot load
    // keeps from EEPROM, so a saved value always survives the next boot (#163).
    static bool IsValid(PloughSetting id, int16_t value);

    // One + (direction 1) or - (direction -1) press from `value`, never past
    // the line's bounds. Yes/no, side and RTK quality jump to their value for
    // that key, as the wizard's screens do.
    static int16_t Step(PloughSetting id, int16_t value, int direction);

    PloughSettings(ImplementPlough* implement, VehicleTractor* tractor,
                   GuidanceSource* guidance, CalibrationPlough* calibration);

    // Opens a session for `who` with a draft of the values the plough holds.
    // False while another owner has one open; true, keeping the draft, when
    // `who` already has it.
    bool Begin(SettingsOwner who);
    SettingsOwner GetOwner() const { return owner; }

    // The draft's value while a session is open, else the value the plough holds.
    int16_t Get(PloughSetting id) const;

    // Stages a value in `who`'s draft. False when `who` holds no session or
    // the value is out of range; the draft keeps its old value then.
    bool Set(SettingsOwner who, PloughSetting id, int16_t value);

    // Stages the three width sensor readings the width procedure captured.
    // False when `who` holds no session or the readings would not survive the
    // boot load (ImplementPlough::IsValidPositionCalibration).
    bool StageWidthCalibration(SettingsOwner who, const short int adc[3]);
    bool HasStagedWidthCalibration() const { return widthStaged; }

    // Applies the draft to the plough and writes EEPROM through the same
    // commits the wizard's COMPLETE step makes, then closes the session.
    // False, changing nothing, when `who` holds no session.
    bool Save(SettingsOwner who);

    // Drops the draft and closes the session; nothing was applied or stored.
    void Cancel(SettingsOwner who);

private:
    ImplementPlough*   implement;
    VehicleTractor*    tractor;
    GuidanceSource*    guidance;
    CalibrationPlough* calibration;

    SettingsOwner owner = SettingsOwner::None;
    int16_t       draft[kCount] = {};
    short int     widthDraft[3] = {};
    bool          widthStaged = false;

    int16_t held(PloughSetting id) const;
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
