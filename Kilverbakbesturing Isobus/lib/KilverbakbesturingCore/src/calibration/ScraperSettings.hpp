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
#pragma once

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>

#include "../implement/ImplementScraper.hpp"
#include "GuidanceSource.hpp"

namespace triton
{

class CalibrationScraper;

// One line per setting, in the wizard's order (NeptuneGPS_Triton#289), the
// same pattern as the plough's PloughSettings. Every front end (the cab
// wizard, the VT calibration page of #292) edits through this table, so a
// setting's range is defined once. The reference points are not settings:
// they are captured, by the wizard's + branch and on the VT page.
// There is no hitch inversion line: the kilverbak's control never reads the
// hitch (owner, 2026-11-02).
enum class ScraperSetting : uint8_t {
    Slope,           // cm per 100 m along A -> B
    ErrorMargin,     // cm
    Swap,            // exchange the two valve outputs
    SpeedGate,       // Auto needs a fresh VTG and MINSPEED, as the plough (#290)
#ifdef ISOBUS
    PreferredVt,     // #255, as the plough
#endif
    RtkQuality,      // the GGA quality that counts as RTK: 4, or 2 (Autofarm A5)
    Count
};

// How a value is shown and stepped. RtkQuality has two values only, 4 (RTK
// fixed) and 2 (DGPS).
enum class SettingKind : uint8_t {
    Number,
    YesNo,
    RtkQuality,
};

struct ScraperSettingLine {
    ScraperSetting id;
    const char*    key;            // short ASCII name, unique
    SettingKind    kind;
    int16_t        lowest;
    int16_t        highest;
    int16_t        step;           // one + or - press
    int16_t        eepromAddress;  // where the value is stored (Slope: int16 at 146-147)
};

// Who holds the open session. One at a time: while the cab wizard edits, the
// VT page cannot, and the other way round.
enum class SettingsOwner : uint8_t {
    None,
    Cab,
    Vt,
};

class ScraperSettings {
public:
    static constexpr uint8_t kCount = uint8_t(ScraperSetting::Count);

    static const ScraperSettingLine& Line(ScraperSetting id);
    static const ScraperSettingLine* Find(const char* key);

    // The one range check: what an edit may set is exactly what the boot load
    // keeps from EEPROM, so a saved value always survives the next boot (the
    // #163 trap; the old wizard's slope and margin had no bounds at all).
    static bool IsValid(ScraperSetting id, int16_t value);

    // One + (direction 1) or - (direction -1) press from `value`, never past
    // the line's bounds. Yes/no and RTK quality jump to their value for that
    // key, as the plough's screens do.
    static int16_t Step(ScraperSetting id, int16_t value, int direction);

    ScraperSettings(ImplementScraper* implement, GuidanceSource* guidance, CalibrationScraper* calibration);

    // Opens a session for `who` with a draft of the values the kilverbak
    // holds. False while another owner has one open; true, keeping the draft,
    // when `who` already has it.
    bool Begin(SettingsOwner who);
    SettingsOwner GetOwner() const { return owner; }

    // The draft's value while a session is open, else the value held.
    int16_t Get(ScraperSetting id) const;

    // Stages a value in `who`'s draft. False when `who` holds no session or
    // the value is out of range; the draft keeps its old value then.
    bool Set(SettingsOwner who, ScraperSetting id, int16_t value);

    // Applies the draft and writes EEPROM, then closes the session. Only the
    // settings' own bytes are written: the reference points, the offset and
    // the receiver rate index stay (owner, #289). False, changing nothing,
    // when `who` holds no session.
    bool Save(SettingsOwner who);

    // Drops the draft and closes the session; nothing was applied or stored.
    void Cancel(SettingsOwner who);

private:
    ImplementScraper*   implement;
    GuidanceSource*     guidance;
    CalibrationScraper* calibration;

    SettingsOwner owner = SettingsOwner::None;
    int16_t       draft[kCount] = {};

    int16_t held(ScraperSetting id) const;
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
