/*
  GuidanceGeometry - small field-scale geometry on GuidanceSource coordinates
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

// Arduino.h defines min()/max() as macros, and libstdc++'s <cmath> (which
// <math.h> includes under C++) uses std::min/std::max with three arguments;
// including math.h after Arduino.h then fails to compile. Every consumer of
// this header includes Arduino.h first, so the macros are parked around the
// include and restored afterwards.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <math.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace triton
{

// Distance between two lat/lon points in metres, from VehicleGps::
// DistanceBetween(). Equirectangular approximation (not full great-circle/
// Haversine): appropriate at the field scale every caller operates at (tens
// to low hundreds of metres), and better numerically conditioned than
// Haversine's sin^2(d/2) term at short range. The one consumer today is the
// scraper's two-reference-point levelling plane (ImplementScraper::
// calculateDistances()); it lives here rather than on GuidanceSource because
// it has nothing to do with a receiver.
inline float DistanceBetween(float lat1, float lon1, float lat2, float lon2) {
    constexpr float kMetersPerDegreeLat = 111320.0f;
    constexpr float kRadiansPerDegree   = 0.017453292f;
    const float latMeters = (lat2 - lat1) * kMetersPerDegreeLat;
    const float lonMeters = (lon2 - lon1) * kMetersPerDegreeLat * cosf(lat1 * kRadiansPerDegree);
    return sqrtf(latMeters * latMeters + lonMeters * lonMeters);
}

}  // namespace triton
