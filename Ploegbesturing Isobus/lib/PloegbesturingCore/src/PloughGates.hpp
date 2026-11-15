/*
  PloughGates - the conditions the plough control needs before it may steer
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

// One definition of "GPS OK" and "fast enough", used both where
// InterfacePlough::Update() decides HOLD and where the VT screen shows its
// GPS and speed indicators -- so the screen can never say OK while the
// control is holding for that reason, or the other way round.

// How old a GGA, VTG or XTE fix may be before the control holds.
static const unsigned long kGuidanceFixMaxAgeMs = 2000;

// Position, speed and cross-track all fresh, and RTK fixed quality, stated
// within the same window (#261): a quality whose source went quiet counts as
// none, even while another source keeps the fixes fresh.
//
// -D AOG_TC_GUIDANCE (the AgOpenGPS TC as the guidance source): that TC sends
// speed (129026) and XTE (129283) but no position and no fix quality on the
// bus, so fresh speed and XTE are enough -- AgOpenGPS only steers a line on
// a fix it already accepted. A fix quality that does arrive (129029, the
// Ag Leader status) still has to be RTK while it is fresh.
inline bool GpsReadyToSteer(GuidanceSource& guidance, unsigned long nowMs) {
#ifdef AOG_TC_GUIDANCE
    const unsigned long qualityAt = guidance.GetQualityTimestamp();
    const bool qualityFresh = qualityAt != 0 && nowMs - qualityAt <= kGuidanceFixMaxAgeMs;
    return guidance.GetVtgTimestamp() != 0 && nowMs - guidance.GetVtgTimestamp() <= kGuidanceFixMaxAgeMs &&
           guidance.GetXteTimestamp() != 0 && nowMs - guidance.GetXteTimestamp() <= kGuidanceFixMaxAgeMs &&
           (!qualityFresh || guidance.IsRtkQuality());
#else
    return nowMs - guidance.GetGgaTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetVtgTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetXteTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetQualityTimestamp() <= kGuidanceFixMaxAgeMs &&
           guidance.IsRtkQuality();
#endif
}

// At or above GuidanceSource's MINSPEED.
inline bool SpeedReadyToSteer(GuidanceSource& guidance) {
    return guidance.MinSpeed();
}

// Why the control holds, as the LCD (S! / G!) and the VT (H S! / H G!, #318)
// both show it: speed while a fresh VTG says below MINSPEED, otherwise GPS.
// Only meaningful while InterfacePlough is in HOLD.
enum class HoldReason : uint8_t { Gps, Speed };
inline HoldReason PloughHoldReason(GuidanceSource& guidance, unsigned long nowMs) {
    return !SpeedReadyToSteer(guidance) && nowMs - guidance.GetVtgTimestamp() < kGuidanceFixMaxAgeMs
               ? HoldReason::Speed
               : HoldReason::Gps;
}

}  // namespace triton
