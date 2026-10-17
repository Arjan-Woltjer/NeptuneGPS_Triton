/*
  test_Interpolation - the shared calibration lookup (MeijWorks Libs/
  Interpolation) gives exactly what the per-project copies it replaces gave
  (NeptuneGPS_Triton#177).
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <cstring> pulls in std helpers that collide with them.
#include <cstring>

#include <AUnit.h>
#include "Interpolation.hpp"

using namespace aunit;
using namespace triton;

namespace {

// The copies, verbatim apart from the names and the array parameters.

// Planter, kipper, scraper, rooier (3 points) and sprayer (12 points).
float LegacyAscending(const int* data, const int* points, int n, int readRaw) {
    int i = 0;
    while (i < n - 1 && readRaw > data[i]) {
        i++;
    }
    if (i == 0) {
        i++;
    }
    float a = readRaw - data[i - 1];
    float b = data[i] - data[i - 1];
    float c = points[i] - points[i - 1];
    float d = points[i - 1];
    if (b == 0.0f) {
        return d;
    }
    return (((a * c) / b) + d);
}

// The sprayer's points are bytes.
float LegacyAscendingBytePoints(const int* data, const byte* points, int n, int readRaw) {
    int i = 0;
    while (i < n - 1 && readRaw > data[i]) {
        i++;
    }
    if (i == 0) {
        i++;
    }
    float a = readRaw - data[i - 1];
    float b = data[i] - data[i - 1];
    float c = points[i] - points[i - 1];
    float d = points[i - 1];
    if (b == 0.0f) {
        return d;
    }
    return (((a * c) / b) + d);
}

// The plough (short int, rising or falling readings).
float LegacyPlough(const short int* data, const short int* points, short int readRaw) {
    short int i = 0;
    if (data[0] < data[1]) {
        i = readRaw < data[1] ? 1 : 2;
    }
    else {
        i = readRaw < data[1] ? 2 : 1;
    }
    float a = readRaw - data[i - 1];
    float b = data[i] - data[i - 1];
    float c = points[i] - points[i - 1];
    float d = points[i - 1];
    if (b == 0.0f) {
        return d;
    }
    return (((a * c) / b) + d);
}

bool SameBits(float p, float q) {
    return std::memcmp(&p, &q, sizeof(float)) == 0;
}

}  // namespace

// Rising, falling and degenerate (two points at one reading) calibrations,
// every reading from well below 0 to well above the 10-bit ADC range.
test(Interpolation, ascending_matchesTheCopiesBitForBit) {
    const int sets[][3] = { { 100, 500, 900 }, { 0, 0, 900 }, { 300, 700, 700 }, { 512, 512, 512 },
                            { 900, 500, 100 } };
    const int points[3] = { 34, 42, 50 };
    for (const auto& data : sets) {
        for (int raw = -200; raw <= 1300; raw++) {
            const float legacy = LegacyAscending(data, points, 3, raw);
            const float shared = Interpolation::Evaluate(data, points,
                                                         Interpolation::SegmentAscending(data, 3, raw), raw);
            if (!SameBits(legacy, shared)) { assertEqual(raw, -99999); return; }
        }
    }
}

test(Interpolation, ascendingTwelvePointsWithBytePoints_matchesTheSprayer) {
    const int data[12] = { 0, 40, 90, 150, 220, 300, 390, 490, 600, 720, 850, 990 };
    const byte points[12] = { 0, 23, 46, 69, 92, 115, 138, 161, 184, 207, 230, 255 };
    for (int raw = -100; raw <= 1500; raw++) {
        const float legacy = LegacyAscendingBytePoints(data, points, 12, raw);
        const float shared = Interpolation::Evaluate(data, points,
                                                     Interpolation::SegmentAscending(data, 12, raw), raw);
        if (!SameBits(legacy, shared)) { assertEqual(raw, -99999); return; }
    }
}

test(Interpolation, aroundMiddle_matchesThePloughBitForBit) {
    const short int sets[][3] = { { 600, 461, 308 }, { 0, 100, 200 }, { 600, 600, 308 }, { 600, 461, 461 } };
    const short int points[3] = { 34, 42, 50 };
    for (const auto& data : sets) {
        for (int raw = -200; raw <= 1300; raw++) {
            const float legacy = LegacyPlough(data, points, static_cast<short int>(raw));
            const float shared = Interpolation::Evaluate(data, points,
                                                         Interpolation::SegmentAroundMiddle(data, raw), raw);
            if (!SameBits(legacy, shared)) { assertEqual(raw, -99999); return; }
        }
    }
}

// The values themselves, for the plough's factory curve.
test(Interpolation, ploughFactoryCurve_hitsItsPointsAndExtrapolates) {
    const short int data[3] = { 600, 461, 308 };
    const short int points[3] = { 34, 42, 50 };
    auto at = [&](int raw) {
        return Interpolation::Evaluate(data, points, Interpolation::SegmentAroundMiddle(data, raw), raw);
    };
    assertNear(at(600), 34.0f, 1e-4f);
    assertNear(at(461), 42.0f, 1e-4f);
    assertNear(at(308), 50.0f, 1e-4f);
    assertTrue(at(1000) < 34.0f);   // beyond the first point: extended, not clamped
    assertTrue(at(0) > 50.0f);      // and past the last
}

test(Interpolation, zeroWidthSegment_givesItsStartPoint) {
    const int data[3] = { 300, 300, 900 };
    const int points[3] = { 10, 20, 30 };
    assertNear(Interpolation::Evaluate(data, points, 1, 300), 10.0f, 0.0f);
}
