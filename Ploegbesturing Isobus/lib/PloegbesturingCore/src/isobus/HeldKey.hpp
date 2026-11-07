/*
  HeldKey - a VT soft key or AUX-N input held as a level (NeptuneGPS_Triton#320)
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

#include <stdint.h>

namespace triton
{

// One rule for everything that drives a valve while it is held: the main
// screen's Breder / Smaller, the width procedure's, and the AUX-N inputs.
//
//  - A press starts the hold.
//  - A still-held repeat keeps a live hold alive: one started by a press,
//    whose last event is at most kTimeoutMs old. It never starts one.
//  - A release, kTimeoutMs without an event, or Drop() (the VT went away)
//    ends it.
//
// Session 24 (#320): a VT that never got a release through a cable pull kept
// repeating still-held after the reconnect; when a repeat counted as a press
// (#245) that drove the valve for 30 s. A lost press now costs a second
// press instead.
class HeldKey {
public:
    // A VT repeats still-held about every 200 ms (session 19: CNH VT
    // 194-206 ms), an input unit at least every 200 ms.
    static constexpr unsigned long kTimeoutMs = 600;

    enum class Event : uint8_t { Press, StillHeld, Release };
    // What an event changed, for the log.
    enum class Change : uint8_t { None, Pressed, Released, Ignored };

    Change On(Event event, unsigned long nowMs) {
        Change change = Change::None;
        switch (event) {
            case Event::Press:
                held = true;
                ignoring = false;
                change = Change::Pressed;
                break;
            case Event::StillHeld: {
                const bool live = IsHeld(nowMs);
                // Ignored repeats are reported once, not every 200 ms.
                if (!live && !ignoring) change = Change::Ignored;
                ignoring = !live;
                held = live;
                break;
            }
            case Event::Release:
                if (held) change = Change::Released;
                held = false;
                ignoring = false;
                break;
        }
        eventMs = nowMs;
        return change;
    }

    bool IsHeld(unsigned long nowMs) const { return held && nowMs - eventMs <= kTimeoutMs; }

    void Drop() {
        held = false;
        ignoring = false;
    }

private:
    bool          held = false;
    bool          ignoring = false;   // still-held repeats without a live press
    unsigned long eventMs = 0;
};

}  // namespace triton
