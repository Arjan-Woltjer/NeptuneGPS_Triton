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

// Position, speed and cross-track all fresh, and RTK fixed quality.
inline bool GpsReadyToSteer(GuidanceSource& guidance, unsigned long nowMs) {
    return nowMs - guidance.GetGgaTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetVtgTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetXteTimestamp() <= kGuidanceFixMaxAgeMs &&
           guidance.IsRtkQuality();
}

// At or above GuidanceSource's MINSPEED.
inline bool SpeedReadyToSteer(GuidanceSource& guidance) {
    return guidance.MinSpeed();
}

}  // namespace triton
