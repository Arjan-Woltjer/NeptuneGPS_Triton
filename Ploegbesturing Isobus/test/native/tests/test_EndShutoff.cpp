/*
  test_EndShutoff - the shared end-of-travel latch (MeijWorks Libs/EndShutoff)
  behaves exactly like the copies it replaces (NeptuneGPS_Triton#177).
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
#include "EndShutoff.hpp"

using namespace aunit;
using namespace triton;

namespace {

// The copies, verbatim apart from the names: the latch part of every
// implement's Adjust() (narrower branch, wider branch, setpoint reached).
struct LegacyLatch {
    unsigned long shutoffTime;
    bool          shutoffWide = false;
    bool          shutoffNarrow = false;
    unsigned long shutoffTimer;

    LegacyLatch(unsigned long time, unsigned long now) : shutoffTime(time), shutoffTimer(now) {}

    void Narrower(bool changed, unsigned long now) {
        if (changed) {
            shutoffTimer = now;
        }
        if (shutoffWide) {
            shutoffNarrow = false;
            shutoffWide = false;
        }
        if (now - shutoffTimer > shutoffTime) {
            shutoffNarrow = true;
            shutoffWide = false;
        }
    }
    void Wider(bool changed, unsigned long now) {
        if (changed) {
            shutoffTimer = now;
        }
        if (shutoffNarrow) {
            shutoffWide = false;
            shutoffNarrow = false;
        }
        if (now - shutoffTimer > shutoffTime) {
            shutoffNarrow = false;
            shutoffWide = true;
        }
    }
    void Stopped(unsigned long now) { shutoffTimer = now; }
};

// Reported as the expected value on a mismatch: no step count reaches it.
constexpr long kMismatch = 100000000L;

}  // namespace

// Long pseudo-random timelines of narrower / wider / stop steps, with the
// position changing or not and time advancing in steps around the shutoff
// time; both latches compared after every step. The guard "is this
// direction latched?" the implements read is part of the comparison.
test(EndShutoff, matchesTheCopiesOnRandomTimelines) {
    unsigned long seed = 12345;
    auto next = [&seed]() { seed = seed * 1103515245UL + 12345UL; return (seed >> 16) & 0x7FFF; };
    const unsigned long times[] = { 3000, 20000, 500 };
    for (unsigned long shutoffTime : times) {
        for (int run = 0; run < 50; run++) {
            unsigned long now = 1000 + next();
            LegacyLatch legacy(shutoffTime, now);
            EndShutoff shared;
            shared.Begin(shutoffTime, now);
            for (long step = 0; step < 400; step++) {
                now += next() % (shutoffTime / 2 + 1);
                const int action = next() % 5;            // narrower x2, wider x2, stop
                const bool changed = (next() % 3) == 0;
                if (action < 2) {
                    if (!legacy.shutoffNarrow) legacy.Narrower(changed, now);
                    if (!shared.Latched(EndShutoff::kNarrower)) shared.Moving(EndShutoff::kNarrower, changed, now);
                }
                else if (action < 4) {
                    if (!legacy.shutoffWide) legacy.Wider(changed, now);
                    if (!shared.Latched(EndShutoff::kWider)) shared.Moving(EndShutoff::kWider, changed, now);
                }
                else {
                    legacy.Stopped(now);
                    shared.Stopped(now);
                }
                if (legacy.shutoffNarrow != shared.Latched(EndShutoff::kNarrower) ||
                    legacy.shutoffWide != shared.Latched(EndShutoff::kWider)) {
                    assertEqual(step, kMismatch);
                    return;
                }
            }
        }
    }
}

// The behaviour itself: held against an end stop, the moving direction
// latches after the shutoff time; the other way clears it.
test(EndShutoff, latchesAtAnEndStopAndClearsTheOtherWay) {
    EndShutoff s;
    s.Begin(3000, 0);
    s.Moving(EndShutoff::kNarrower, true, 100);     // moving
    s.Moving(EndShutoff::kNarrower, false, 3100);   // stuck, exactly the shutoff time: not yet
    assertFalse(s.Latched(EndShutoff::kNarrower));
    s.Moving(EndShutoff::kNarrower, false, 3101);   // past it
    assertTrue(s.Latched(EndShutoff::kNarrower));
    assertFalse(s.Latched(EndShutoff::kWider));
    s.Moving(EndShutoff::kWider, true, 3200);       // the other way
    assertFalse(s.Latched(EndShutoff::kNarrower));
    assertFalse(s.Latched(EndShutoff::kWider));
}

test(EndShutoff, standingStillRestartsTheTimer) {
    EndShutoff s;
    s.Begin(3000, 0);
    s.Moving(EndShutoff::kWider, true, 0);
    s.Stopped(2900);
    s.Moving(EndShutoff::kWider, false, 5000);      // 2100 ms since the stop
    assertFalse(s.Latched(EndShutoff::kWider));
}
