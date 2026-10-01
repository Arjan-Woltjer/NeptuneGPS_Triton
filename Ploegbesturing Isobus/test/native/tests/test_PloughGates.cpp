/*
  test_PloughGates - Tests for the GPS and speed gates shared by the plough
  control's HOLD decision and the VT screen's indicators.
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

#include "PloughGates.hpp"

using namespace aunit;
using namespace triton;

namespace {

// Every fix fresh at `at` ms, RTK fixed quality, 10 kn.
void allFreshAt(GuidanceSource& g, unsigned long at) {
    millisValue(at);
    g.SetRtkQuality(4);
    g.SetQuality(4);
    g.NoteGgaFixReceived();
    g.SetSpeedKnots(10.0f);
    g.SetXte(0);
}

}  // namespace

test(PloughGates, gps_okWhenEverythingFreshAndRtk) {
    GuidanceSource g;
    allFreshAt(g, 10000);
    assertTrue(GpsReadyToSteer(g, 10000));
    assertTrue(GpsReadyToSteer(g, 10000 + kGuidanceFixMaxAgeMs));  // the limit itself is still OK
    millisValue(0);
}

test(PloughGates, gps_notOkOnceAnyFixIsTooOld) {
    GuidanceSource g;
    allFreshAt(g, 10000);
    assertFalse(GpsReadyToSteer(g, 10000 + kGuidanceFixMaxAgeMs + 1));

    // Only XTE stale.
    allFreshAt(g, 10000);
    millisValue(13000);
    g.NoteGgaFixReceived();
    g.SetSpeedKnots(10.0f);
    assertFalse(GpsReadyToSteer(g, 13000));

    // Only VTG stale.
    allFreshAt(g, 10000);
    millisValue(13000);
    g.NoteGgaFixReceived();
    g.SetXte(0);
    assertFalse(GpsReadyToSteer(g, 13000));

    // Only GGA stale.
    allFreshAt(g, 10000);
    millisValue(13000);
    g.SetSpeedKnots(10.0f);
    g.SetXte(0);
    assertFalse(GpsReadyToSteer(g, 13000));
    millisValue(0);
}

test(PloughGates, gps_notOkWithoutRtkQuality) {
    GuidanceSource g;
    allFreshAt(g, 10000);
    g.SetQuality(5);  // RTK float
    assertFalse(GpsReadyToSteer(g, 10000));
    g.SetQuality(1);  // autonomous
    assertFalse(GpsReadyToSteer(g, 10000));
    millisValue(0);
}

test(PloughGates, speed_followsMinSpeed) {
    GuidanceSource g;
    allFreshAt(g, 10000);
    g.SetSpeedKnots(0.0f);
    assertFalse(SpeedReadyToSteer(g));
    g.SetSpeedKnots(10.0f);
    assertTrue(SpeedReadyToSteer(g));
    assertEqual(SpeedReadyToSteer(g), g.MinSpeed());
    millisValue(0);
}
