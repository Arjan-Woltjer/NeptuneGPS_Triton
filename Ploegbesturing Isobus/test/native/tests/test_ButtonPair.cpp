/*
  test_ButtonPair - the shared cab-button logic (MeijWorks Libs/ButtonPair)
  returns exactly what the interfaces' CheckButtons() copies returned
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
#include <AUnit.h>
#include "ButtonPair.hpp"

using namespace aunit;
using namespace triton;

namespace {

// The three shapes of CheckButtons(), verbatim apart from the pins becoming
// parameters and millis() becoming `now`: plough/planter/kipper/sprayer
// (plain), scraper/rooier (joystick in the single-button branches) and
// Ploegbesturing Isobus (VT soft keys).
struct LegacyButtons {
    int           buttons = 0;
    bool          button1Flag = false;
    bool          button2Flag = false;
    unsigned long button1Timer;
    unsigned long button2Timer;

    explicit LegacyButtons(unsigned long now) : button1Timer(now), button2Timer(now) {}

    int Check(bool left, bool right, bool joyLeft, bool joyRight, bool vtCal, bool vtWider, bool vtNarrower,
              unsigned char delay1, unsigned char delay2, unsigned long now) {
        if (button1Flag) {
            button1Timer = now;
            button1Flag = false;
        }
        if (button2Flag) {
            button2Timer = now;
            button2Flag = false;
        }
        if (left && right) {
            if (now - button1Timer >= delay1 * 4) {
                button1Flag = true;
                buttons = 2;
                return 2;
            }
            else {
                button2Flag = true;
                buttons = 0;
                return 0;
            }
        }
        else if (vtCal) {
            button1Flag = true;
            buttons = 2;
            return 2;
        }
        else if (left || joyLeft) {
            if (now - button2Timer >= delay2) {
                button2Flag = true;
                buttons = -1;
                return -1;
            }
            else {
                button1Flag = true;
                buttons = 0;
                return 0;
            }
        }
        else if (vtWider) {
            button2Flag = true;
            buttons = -1;
            return -1;
        }
        else if (right || joyRight) {
            if (now - button2Timer >= delay2) {
                button2Flag = true;
                buttons = 1;
                return 1;
            }
            else {
                button1Flag = true;
                buttons = 0;
                return 0;
            }
        }
        else if (vtNarrower) {
            button2Flag = true;
            buttons = 1;
            return 1;
        }
        else {
            button1Flag = true;
            button2Flag = true;
            buttons = 0;
            return 0;
        }
    }
};

// The first step (run * 1000 + step) where the shared logic and the copy disagree, or -1.
long FirstMismatch(bool withJoystick, bool withVt) {
    unsigned long seed = 4242 + withJoystick * 7 + withVt * 13;
    auto next = [&seed]() { seed = seed * 1103515245UL + 12345UL; return (seed >> 16) & 0x7FFF; };
    for (int run = 0; run < 100; run++) {
        unsigned long now = next();
        LegacyButtons legacy(now);
        ButtonPair shared;
        shared.Begin(now);
        const unsigned char delay1 = run % 2 ? 255 : 50, delay2 = run % 3 ? 200 : 0;
        for (long step = 0; step < 500; step++) {
            now += next() % 400;
            const unsigned r = next();
            const bool left = r & 1, right = r & 2;
            const bool joyLeft = withJoystick && (r & 4), joyRight = withJoystick && (r & 8);
            const bool vtCal = withVt && (r & 0x70) == 0x10, vtWider = withVt && (r & 0x70) == 0x20,
                       vtNarrower = withVt && (r & 0x70) == 0x30;
            const int expected = legacy.Check(left, right, joyLeft, joyRight, vtCal, vtWider, vtNarrower, delay1, delay2, now);
            const int actual = shared.Check(left, right, delay1, delay2, now, joyLeft, joyRight, vtCal, vtWider, vtNarrower);
            if (expected != actual) {
                return run * 1000L + step;
            }
        }
    }
    return -1;
}

}  // namespace

test(ButtonPair, plain_matchesTheCopies) { assertEqual(FirstMismatch(false, false), -1L); }
test(ButtonPair, joystick_matchesTheCopies) { assertEqual(FirstMismatch(true, false), -1L); }
test(ButtonPair, vtSoftKeys_matchTheCopy) { assertEqual(FirstMismatch(false, true), -1L); }

// The behaviour itself.
test(ButtonPair, bothHeldReportsTwoAfterDelay1TimesFour) {
    ButtonPair b;
    b.Begin(0);
    assertEqual(b.Check(true, true, 255, 200, 1019), 0);   // 255 * 4 = 1020 ms not yet
    assertEqual(b.Check(true, true, 255, 200, 1020), 2);
}

test(ButtonPair, physicalButtonBeatsAVtPressInTheSameLoop) {
    ButtonPair b;
    b.Begin(0);
    assertEqual(b.Check(true, false, 255, 0, 10, false, false, false, false, true), -1);  // left wins over VT Narrower
}
