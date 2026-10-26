/*
  Interpolation - the piecewise-linear calibration lookup the Triton
  implements share: sensor reading in, calibrated value out
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

namespace triton
{

// Piecewise-linear calibration lookup. `x` holds the sensor readings captured
// at each calibration point, `y` the value each point stands for. Pick the
// segment with one of the Segment functions, then Evaluate() it:
// a = raw - x[i-1], b = x[i] - x[i-1], c = y[i] - y[i-1], d = y[i-1],
// value = a * c / b + d.
//
//  - SegmentAscending(): the first point the reading does not exceed, at
//    least 1, at most n - 1. Assumes readings rise with the points (planter,
//    kipper, scraper, rooier, sprayer).
//  - SegmentAroundMiddle(): three points, readings rising OR falling; which
//    side of the middle reading it is on picks the segment (plough).
//
// Past the first and last point the end segments are extended, not clamped:
// callers rely on that (the plough's end-shutoff needs the position to keep
// changing past the calibrated range). Clamp the result in the caller if
// needed, as the sprayer does for its PWM.
//
// Header-only templates, so each caller keeps its own element types (int,
// short int, byte) and the arithmetic that goes with them, on AVR, ARM and
// ESP32 alike.
struct Interpolation {
    template <typename X>
    static int SegmentAscending(const X* x, int n, int raw) {
        int i = 0;
        while (i < n - 1 && raw > x[i]) {
            i++;
        }
        if (i == 0) {
            i++;
        }
        return i;
    }

    template <typename X>
    static int SegmentAroundMiddle(const X* x, int raw) {
        if (x[0] < x[1]) {
            return raw < x[1] ? 1 : 2;
        }
        return raw < x[1] ? 2 : 1;
    }

    // Linear between points i - 1 and i. A segment of zero width -- two points
    // captured at the same reading, as a disconnected or seized sensor gives --
    // would divide by zero; its start point is returned instead.
    template <typename X, typename Y>
    static float Evaluate(const X* x, const Y* y, int i, int raw) {
        float a = raw - x[i - 1];
        float b = x[i] - x[i - 1];
        float c = y[i] - y[i - 1];
        float d = y[i - 1];
        if (b == 0.0f) {
            return d;
        }
        return ((a * c) / b) + d;
    }
};

}  // namespace triton
