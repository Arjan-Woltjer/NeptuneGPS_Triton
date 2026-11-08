/*
  test_StackWatermark - the stack's peak is read from the lowest word that no
  longer holds the boot pattern (MeijWorks Libs/StackWatermark).
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
// macros, and GCC's <string> uses std::min/max with three arguments.
#include <string>

#include <AUnit.h>
#include "StackWatermark.hpp"

using namespace aunit;
using namespace triton;

namespace
{
constexpr uint32_t kWords = 64;

struct Region {
    uint32_t words[kWords];
    uint32_t* Bottom() { return words; }
    uint32_t* Top() { return words + kWords; }
    Region() { StackWatermark::PaintRegion(Bottom(), Top()); }
};
} // namespace

test(StackWatermark, untouched_thenThePeakIsZero) {
    Region region;
    assertEqual(StackWatermark::PeakInRegion(region.Bottom(), region.Top()), (uint32_t)0);
}

test(StackWatermark, framesNearTheTop_thenThePeakCountsDownFromTheTop) {
    Region region;
    region.words[kWords - 1] = 1;
    region.words[kWords - 2] = 2;
    assertEqual(StackWatermark::PeakInRegion(region.Bottom(), region.Top()), (uint32_t)(2 * 4));
}

test(StackWatermark, aHoleAboveTheDeepestWord_thenThePeakStillReachesIt) {
    // A frame reserves space it never writes (a buffer, padding): the words
    // above the deepest write may still hold the pattern.
    Region region;
    region.words[kWords - 1] = 1;
    region.words[10] = 0;
    assertEqual(StackWatermark::PeakInRegion(region.Bottom(), region.Top()), (uint32_t)((kWords - 10) * 4));
}

test(StackWatermark, theWholeRegionUsed_thenThePeakIsItsSize) {
    Region region;
    region.words[0] = 0;
    assertEqual(StackWatermark::PeakInRegion(region.Bottom(), region.Top()), (uint32_t)(kWords * 4));
}

test(StackWatermark, offTheTeensy4_thenNotAvailable) {
    StackWatermark::Paint();
    const StackUse use = StackWatermark::Read();
    assertFalse(use.available);
    assertEqual(use.peak, (uint32_t)0);
}
