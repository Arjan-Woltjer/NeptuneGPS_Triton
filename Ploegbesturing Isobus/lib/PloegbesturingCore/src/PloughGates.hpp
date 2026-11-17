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
inline bool GpsReadyToSteer(GuidanceSource& guidance, unsigned long nowMs) {
    return nowMs - guidance.GetGgaTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetVtgTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetXteTimestamp() <= kGuidanceFixMaxAgeMs &&
           nowMs - guidance.GetQualityTimestamp() <= kGuidanceFixMaxAgeMs &&
           guidance.IsRtkQuality();
}

// The least speed the control steers at, in 0.1 km/h: MIN km/h on the
// settings page (2026-11-17; GuidanceSource's fixed MINSPEED, 0.5 m/s =
// 1.8 km/h, until then). CalibrationPlough loads it at boot (EEPROM 13) and
// stores it on Save; the settings table edits it.
constexpr uint8_t kLowestMinSpeedTenthsKmh  = 3;    // 0.3 km/h
constexpr uint8_t kHighestMinSpeedTenthsKmh = 95;   // 9.5 km/h
constexpr uint8_t kDefaultMinSpeedTenthsKmh = 18;   // 1.8 km/h, the old 0.5 m/s
constexpr int     kEepromMinSpeed           = 13;
inline uint8_t    gMinSpeedTenthsKmh        = kDefaultMinSpeedTenthsKmh;

// At or above the MIN km/h setting (m/s x 36 = 0.1 km/h).
inline bool SpeedReadyToSteer(GuidanceSource& guidance) {
    return guidance.GetSpeedMs() * 36.0f >= float(gMinSpeedTenthsKmh);
}

// Why the control holds, as the LCD (S! / G!) and the VT (TE TRAAG / GPS FOUT, #318)
// both show it: speed while a fresh VTG says below MIN km/h, otherwise GPS.
// Only meaningful while InterfacePlough is in HOLD.
enum class HoldReason : uint8_t { Gps, Speed };
inline HoldReason PloughHoldReason(GuidanceSource& guidance, unsigned long nowMs) {
    return !SpeedReadyToSteer(guidance) && nowMs - guidance.GetVtgTimestamp() < kGuidanceFixMaxAgeMs
               ? HoldReason::Speed
               : HoldReason::Gps;
}

}  // namespace triton
