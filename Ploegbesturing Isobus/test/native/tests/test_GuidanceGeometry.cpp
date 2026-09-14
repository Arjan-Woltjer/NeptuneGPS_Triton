/*
  test_GuidanceGeometry - Tests for the shared DistanceBetween() helper
  (MeijWorks Libs/VehicleGuidance/GuidanceGeometry.hpp).
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
#include <AUnit.h>

#include "GuidanceGeometry.hpp"

using namespace aunit;
using namespace triton;

test(GuidanceGeometry, same_point_is_zero) {
    assertNear(DistanceBetween(52.0f, 5.0f, 52.0f, 5.0f), 0.0f, 1e-6f);
}

// One degree of latitude is 111320 m in this approximation, whatever the
// longitude. The tolerance is float's: near 52 degrees one ulp of latitude
// is about 0.4 m, which is also why the scraper's own test with the same
// fixture asserts on the reference height, not the distance.
test(GuidanceGeometry, hundred_metres_due_north) {
    const float lat2 = 52.0f + 100.0f / 111320.0f;
    assertNear(DistanceBetween(52.0f, 5.0f, lat2, 5.0f), 100.0f, 0.5f);
}

// Longitude shrinks with cos(latitude): at 60 N one degree of longitude is half a degree of latitude.
test(GuidanceGeometry, longitude_scales_with_cos_latitude) {
    const float atEquator = DistanceBetween(0.0f, 5.0f, 0.0f, 5.001f);
    const float atSixty   = DistanceBetween(60.0f, 5.0f, 60.0f, 5.001f);
    assertNear(atEquator, 111.32f, 0.05f);
    assertNear(atSixty, 55.66f, 0.05f);
}

test(GuidanceGeometry, symmetric_and_direction_free) {
    const float ab = DistanceBetween(52.0f, 5.0f, 52.0005f, 5.0007f);
    const float ba = DistanceBetween(52.0005f, 5.0007f, 52.0f, 5.0f);
    assertNear(ab, ba, 0.01f);   // cos() is taken at each call's own first point
    assertMore(ab, 0.0f);
}
