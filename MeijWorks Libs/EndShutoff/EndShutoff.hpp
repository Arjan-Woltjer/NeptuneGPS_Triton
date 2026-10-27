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

// End-of-travel latch for one valve pair. While the valve drives one way and
// the measured position does not change for longer than the shutoff time,
// the implement is at its end stop: that direction latches and is not driven
// again until the implement moves the other way. Driving the other way
// clears both latches; Stopped() restarts the timer.
//
// The caller decides what "the position changed" means (the plough compares
// its filtered position, the kipper its raw steer reading) and whether a
// manual press may drive through a latch (the plough lets it). Header-only,
// so it builds for the Arduino Micro as well.
class EndShutoff {
public:
    enum Direction { kNarrower = 0, kWider = 1 };

    // Initialize: not latched, timer started at nowMs.
    void Begin(unsigned long timeMs, unsigned long nowMs) {
        shutoffTime = timeMs;
        latched[kNarrower] = false;
        latched[kWider] = false;
        timer = nowMs;
    }

    // Called on every cycle the valve drives `direction`, after driving it.
    void Moving(Direction direction, bool positionChanged, unsigned long nowMs) {
        Direction other;

        // Determine the other direction.
        if (direction == kNarrower) {
            other = kWider;
        } else {
            other = kNarrower;
        }

        // Reset the timer if the position changed.
        if (positionChanged) {
            timer = nowMs;
        }

        // Clear both latches if the other direction is latched.
        if (latched[other]) {
            latched[kNarrower] = false;
            latched[kWider] = false;
        }

        // Latch this direction once the position has not changed for longer than the shutoff time.
        if (nowMs - timer > shutoffTime) {
            latched[direction] = true;
            latched[other] = false;
        }
    }

    // Returns whether the given direction is currently latched.
    bool Latched(Direction direction) const { return latched[direction]; }

    // Called when the setpoint is reached and the valves stop.
    void Stopped(unsigned long nowMs) { timer = nowMs; }

private:
    unsigned long shutoffTime = 0;
    bool          latched[2] = { false, false };
    unsigned long timer = 0;
};

}  // namespace triton
