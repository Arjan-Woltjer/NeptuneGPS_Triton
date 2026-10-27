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
//  - the VT soft keys (Ploegbesturing Isobus): Wider and Narrower are held
//    levels (#245) and join the single-button branches like the joysticks
//    (Wider -> 1, the right slot: the cab's + button; Narrower -> -1, the
//    left slot: the - button; owner, 2026-10-20), so a held
//    soft key repeats exactly like a held cab button. They never make the
//    both-held combo. Calibrate is a consume-once tap with its own branch
//    after the combo: it skips the combo's hold (a tap is true for one loop)
//    but re-seeds its timer. See IsobusVtInterface.hpp.
class ButtonPair {
public:
    // Timers started at nowMs, nothing pending: what the constructors did.
    void Begin(unsigned long nowMs) {
        restartComboTimer = false;
        restartRepeatTimer = false;
        comboStartMs = nowMs;
        repeatStartMs = nowMs;
    }

    int Check(bool leftButton, bool rightButton, unsigned char delay1, unsigned char delay2, unsigned long nowMs,
              bool leftJoystick = false, bool rightJoystick = false,
              bool vtCalibrate = false, bool vtWider = false, bool vtNarrower = false) {
        // Reset timers if their restart flags are set
        if (restartComboTimer) {
            comboStartMs = nowMs;
            restartComboTimer = false;
        }
        if (restartRepeatTimer) {
            repeatStartMs = nowMs;
            restartRepeatTimer = false;
        }

        // Decide which button(s) are held and update timers accordingly
        // Both-held combo takes precedence (for calibration)
        if (leftButton && rightButton) {
            if (nowMs - comboStartMs >= static_cast<unsigned long>(delay1) * 4) {  // * 4: delay1 is a byte
                restartComboTimer = true;
                return 2;
            }
            restartRepeatTimer = true;
            return 0;
        }
        // VT Calibrate takes precedence after the combo
        if (vtCalibrate) {
            restartComboTimer = true;
            return 2;
        }
        // Single-button holds and VT adjustments follow
        // Left button is - (decrement)
        if (leftButton || leftJoystick || vtNarrower) {
            return held(-1, delay2, nowMs);
        }
        // Right button is + (increment)
        if (rightButton || rightJoystick || vtWider) {
            return held(1, delay2, nowMs);
        }

        // No buttons held, no VT adjustments: reset both timers and return 0
        restartComboTimer = true;
        restartRepeatTimer = true;
        return 0;
    }

private:
    int held(int direction, unsigned char delay2, unsigned long nowMs) {
        if (nowMs - repeatStartMs >= delay2) {
            restartRepeatTimer = true;
            return direction;
        }
        restartComboTimer = true;
        return 0;
    }

    // Two timers, one per kind of hold (not one per button). A restart flag
    // is applied at the top of the next Check(), so the timer restarts at
    // that call's nowMs.
    bool          restartComboTimer = false;   // restart comboStartMs next Check()
    bool          restartRepeatTimer = false;  // restart repeatStartMs next Check()
    unsigned long comboStartMs = 0;   // both-held wait began; Check() returns 2 after delay1 * 4 ms
    unsigned long repeatStartMs = 0;  // single-hold repeat interval began; fires every delay2 ms
};

}  // namespace triton
