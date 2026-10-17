/*
  ButtonPair - the two cab buttons: debounce, hold-to-repeat and "both held"
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

// One copy of the CheckButtons() every machine's interface wrote out
// (NeptuneGPS_Triton#177, seven copies). Check() returns
//    2  both buttons held for delay1 * 4 ms (delay1 is a byte)
//   -1  left held, repeating every delay2 ms
//    1  right held, repeating every delay2 ms
//    0  otherwise
// It only decides; the caller reads its own pins (digitalRead or TritonIo)
// and passes them in, so the native tests drive it without hardware.
//
// The variants are inputs, not copies:
//  - leftJoystick/rightJoystick join the single-button branches (not the
//    both-held one), as on the scraper and the rooier;
//  - the VT soft keys (Ploegbesturing Isobus) are consume-once edges, each
//    with its own branch right after the physical branch it mirrors, so the
//    physical buttons keep priority: Calibrate -> 2, Wider -> -1 (the left
//    slot), Narrower -> 1 (the right slot). They skip the hold timers (an
//    edge is true for one loop only) but re-seed them like their physical
//    counterparts. See IsobusVtInterface.hpp.
class ButtonPair {
public:
    // Timers started at nowMs, nothing pending: what the constructors did.
    void Begin(unsigned long nowMs) {
        button1Flag = false;
        button2Flag = false;
        button1Timer = nowMs;
        button2Timer = nowMs;
    }

    int Check(bool leftButton, bool rightButton, unsigned char delay1, unsigned char delay2, unsigned long nowMs,
              bool leftJoystick = false, bool rightJoystick = false,
              bool vtCalibrate = false, bool vtWider = false, bool vtNarrower = false) {
        if (button1Flag) {
            button1Timer = nowMs;
            button1Flag = false;
        }
        if (button2Flag) {
            button2Timer = nowMs;
            button2Flag = false;
        }

        if (leftButton && rightButton) {
            if (nowMs - button1Timer >= static_cast<unsigned long>(delay1) * 4) {  // * 4: delay1 is a byte
                button1Flag = true;
                return 2;
            }
            button2Flag = true;
            return 0;
        }
        if (vtCalibrate) {
            button1Flag = true;
            return 2;
        }
        if (leftButton || leftJoystick) {
            return held(-1, delay2, nowMs);
        }
        if (vtWider) {
            button2Flag = true;
            return -1;
        }
        if (rightButton || rightJoystick) {
            return held(1, delay2, nowMs);
        }
        if (vtNarrower) {
            button2Flag = true;
            return 1;
        }
        button1Flag = true;
        button2Flag = true;
        return 0;
    }

private:
    int held(int direction, unsigned char delay2, unsigned long nowMs) {
        if (nowMs - button2Timer >= delay2) {
            button2Flag = true;
            return direction;
        }
        button1Flag = true;
        return 0;
    }

    bool          button1Flag = false;
    bool          button2Flag = false;
    unsigned long button1Timer = 0;
    unsigned long button2Timer = 0;
};

}  // namespace triton
