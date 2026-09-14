/*
  InterfaceGuidance - LCD-guided receiver baudrate detection at boot
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

#include <Arduino.h>

#include "GuidanceSource.hpp"
#include "InterfaceI2CLCD.hpp"
#include "SerialGuidanceChannel.hpp"

namespace triton
{

// The successor to InterfaceGps (MeijWorks Libs/InterfaceGps), on the shared
// GuidanceSource/SerialGuidanceChannel pair instead of VehicleGps. At boot,
// with nothing else running yet, it cycles the receiver port through the
// eight rates the Triton receivers are configured with, shows on the LCD
// which sentences arrive, and stops at the first rate that produces any of
// the ones the caller asked for.
//
// Blocking, by design: every rate is given at least two seconds (up to ten
// while sentences trickle in), so a full sweep can take over a minute.
// Nothing in here persists the result -- the project decides where the index
// is stored (NeptuneGPS_Triton#78) and hands it back as startIndex next boot.
class InterfaceGuidance {
public:
    // Which sentences count. InterfaceGps compiled this choice in with a
    // SCRAPER macro that no project defined; here every project says what
    // its receiver actually sends (the scraper has no XTE, #81).
    static constexpr byte kXte = 1;
    static constexpr byte kVtg = 2;
    static constexpr byte kGga = 4;
    static constexpr byte kAll = kGga | kVtg | kXte;

    static constexpr byte kRateCount = 8;
    static constexpr byte kNotFound  = 0xFF;

    // 4800 x {1, 2, 3, 4, 6, 8, 12, 24}: the table VehicleGps printed and
    // ConfigSprayer stores an index into.
    static long BaudFromIndex(byte index);

    InterfaceGuidance(InterfaceI2CLCD* lcd, SerialGuidanceChannel* channel, GuidanceSource* guidance);

    // Try each rate in turn, starting at startIndex and wrapping. Returns
    // the index of the first rate on which any `required` sentence arrived;
    // the port is left open at that rate. Returns kNotFound, with the port
    // reopened at startIndex's rate, when none did.
    byte DetectBaudrate(byte startIndex, byte required);

private:
    InterfaceI2CLCD*       lcd;
    SerialGuidanceChannel* channel;
    GuidanceSource*        guidance;

    // Listen at `baud` and return the mask of `required` sentences seen.
    byte testRate(long baud, byte required);
    void showSentence(byte row, bool seen);
};

}  // namespace triton
