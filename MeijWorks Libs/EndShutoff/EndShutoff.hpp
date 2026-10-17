/*
  EndShutoff - stops driving a valve towards an end the implement has reached
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

// One copy of the end-of-travel latch six implements wrote out twice each
// (NeptuneGPS_Triton#177; the rooier runs one per side). While a valve drives
// one way and the measured position stops changing for longer than the
// shutoff time, the implement is at its end stop: that direction latches and
// is not driven again until the implement moves the other way. Moving the
// other way clears both latches; standing still restarts the timer.
//
// The caller decides what "the position changed" means (the plough compares
// its filtered position, the kipper its raw steer reading) and whether a
// manual press may drive through a latch (the plough lets it). Header-only:
// it builds for the Arduino Micro as well.
class EndShutoff {
public:
    enum Direction { kNarrower = 0, kWider = 1 };

    // Not latched, timer started at nowMs: what every implement's constructor did.
    void Begin(unsigned long timeMs, unsigned long nowMs) {
        shutoffTime = timeMs;
        latched[kNarrower] = false;
        latched[kWider] = false;
        timer = nowMs;
    }

    bool Latched(Direction direction) const { return latched[direction]; }

    // Called on every cycle the valve drives `direction`, after driving it.
    void Moving(Direction direction, bool positionChanged, unsigned long nowMs) {
        const Direction other = direction == kNarrower ? kWider : kNarrower;
        if (positionChanged) {
            timer = nowMs;
        }
        if (latched[other]) {
            latched[kNarrower] = false;
            latched[kWider] = false;
        }
        if (nowMs - timer > shutoffTime) {
            latched[direction] = true;
            latched[other] = false;
        }
    }

    // Called when the setpoint is reached and the valves stop.
    void Stopped(unsigned long nowMs) { timer = nowMs; }

private:
    unsigned long shutoffTime = 0;
    bool          latched[2] = { false, false };
    unsigned long timer = 0;
};

}  // namespace triton
