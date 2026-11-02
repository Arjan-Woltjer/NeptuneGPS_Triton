/*
  ScraperGates - when the kilverbak may level, and which source it levels on
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

#include "GuidanceSource.hpp"

namespace triton
{

// One definition of "may level" (NeptuneGPS_Triton#290), used where
// InterfaceScraper::Update() decides HOLD and where the screens show why --
// so a screen can never say OK while the control holds, or the other way
// round. The plough's PloughGates.hpp is the same idea.

// How old a position, altitude, quality or VTG may be before the control holds.
static const unsigned long kScraperFixMaxAgeMs = 2000;

// Why the control holds, in the order it is checked.
enum class ScraperHold : uint8_t {
    None,     // may level
    NoFix,    // no fresh position or altitude (LCD: G!)
    NoRtk,    // the fix is not RTK fixed, or its quality went quiet (R!)
    Speed,    // the speed gate: no fresh VTG, or below MINSPEED (S!)
};

// A timestamp from GuidanceSource (0 = never) no older than the limit.
inline bool ScraperFresh(unsigned long timestamp, unsigned long nowMs) {
    return timestamp != 0 && nowMs - timestamp <= kScraperFixMaxAgeMs;
}

// The gates, owner 2026-11-01/02: Auto needs a fresh position and altitude
// and an RTK fixed fix stated within the same window (the RTK quality is the
// setting, 4 or 2); with the speed gate on, as on the plough, also a fresh
// VTG and MINSPEED.
inline ScraperHold ScraperHoldReason(GuidanceSource& guidance, bool speedGate, unsigned long nowMs) {
    if (!ScraperFresh(guidance.GetGgaTimestamp(), nowMs) || !ScraperFresh(guidance.GetAltitudeTimestamp(), nowMs)) {
        return ScraperHold::NoFix;
    }
    if (!ScraperFresh(guidance.GetQualityTimestamp(), nowMs) || !guidance.IsRtkQuality()) {
        return ScraperHold::NoRtk;
    }
    if (speedGate && (!ScraperFresh(guidance.GetVtgTimestamp(), nowMs) || !guidance.MinSpeed())) {
        return ScraperHold::Speed;
    }
    return ScraperHold::None;
}

// The source the control reads (owner, 2026-11-01): the receiver on the UART
// while it has a fresh GGA, otherwise the CAN bus (on the ISOBUS build: PGN
// 129029 for the height). The chosen source is copied whole, timestamps
// included, so its ages keep their meaning; `out` keeps its own RTK quality
// setting, which the calibration sets on it. Returns true when the UART won.
// The CAN source must not take PGN 65256's coarse altitude
// (GuidanceSource::SetUseCoarseAltitude(false)).
inline bool SelectScraperGuidance(GuidanceSource& out, GuidanceSource& uart, const GuidanceSource& can,
                                  unsigned long nowMs) {
    const bool useUart = ScraperFresh(uart.GetGgaTimestamp(), nowMs);
    const byte rtkQuality = out.GetRtkQuality();
    out = useUart ? uart : can;
    out.SetRtkQuality(rtkQuality);
    return useUart;
}

}  // namespace triton
