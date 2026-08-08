/*
  CalibrationKipper - LCD/button calibration wizard for the MeijWorks kipper interface
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
#include "CalibrationKipper.hpp"

namespace triton
{

CalibrationKipper::CalibrationKipper(InterfaceI2CLCD* lcd, ImplementKipper* implement,
                                      VehicleTractor* tractor, InterfaceKipper* interface)
    : lcd(lcd), implement(implement), tractor(tractor), interface(interface) {
}

// -------------------------------------------------------------------------
// Shared accept/decline + adjust-by-one-per-press step, matching the shape
// every step in the legacy calibrate() fragment repeated four times (KP,
// KI, KD, offset).
// -------------------------------------------------------------------------
bool CalibrationKipper::adjustValue(const char* title, const char* prompt, int* value, int minValue, int maxValue) {
    lcd->WriteBuffer(title, 0);
    lcd->WriteBuffer(L4_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L4_CAL_DECLINE, 2);
    lcd->WriteBuffer(L4_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        int buttons = interface->CheckButtons(0, 0);

        if (buttons == -1) {
            lcd->WriteBuffer(title, 0);
            lcd->WriteBuffer(L4_CAL_DECLINED, 1);
            lcd->WriteBuffer(L4_BLANK, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);
            delay(1000);
            return false;
        }
        else if (buttons == 1) {
            lcd->WriteBuffer(title, 0);
            lcd->WriteBuffer(L4_CAL_ADJUST, 1);
            lcd->WriteBuffer(L4_CAL_ENTER, 2);
            lcd->WriteBuffer(prompt, 3);

            lcd->WriteScreen(-1);

            int temp = *value;

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                    if (temp > maxValue) {
                        temp = maxValue;
                    }
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                    if (temp < minValue) {
                        temp = minValue;
                    }
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }

                int temp2 = abs(temp) / 10 % 10;
                int temp3 = abs(temp) / 100 % 10;

                lcd->WriteBuffer(temp3 + '0', 3, 13);
                lcd->WriteBuffer('.', 3, 14);
                lcd->WriteBuffer(temp2 + '0', 3, 15);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 16);
            }

            *value = temp;

            lcd->WriteBuffer(title, 0);
            lcd->WriteBuffer(L4_CAL_DONE, 1);
            lcd->WriteBuffer(L4_BLANK, 2);
            lcd->WriteBuffer(prompt, 3);

            lcd->WriteScreen(-1);
            delay(1000);
            return true;
        }
    }
}

// --------------------------------
// Method for calibrating implement
// --------------------------------
void CalibrationKipper::Calibrate() {
    // Stop any adjusting
    implement->Adjust(0);

    lcd->WriteScreen(-1);

    // ----------------
    // Steer calibration
    // ----------------
    lcd->WriteBuffer(L4_CAL_STEER, 0);
    lcd->WriteBuffer(L4_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L4_CAL_DECLINE, 2);
    lcd->WriteBuffer(L4_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        int buttons = interface->CheckButtons(0, 0);

        if (buttons == -1) {
            lcd->WriteBuffer(L4_CAL_STEER, 0);
            lcd->WriteBuffer(L4_CAL_DECLINED, 1);
            lcd->WriteBuffer(L4_BLANK, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);
            delay(1000);
            break;
        }
        else if (buttons == 1) {
            lcd->WriteBuffer(L4_CAL_STEER, 0);
            lcd->WriteBuffer(L4_CAL_ADJUST, 1);
            lcd->WriteBuffer(L4_CAL_ENTER, 2);
            lcd->WriteBuffer(L4_CAL_STEER_AD, 3);

            lcd->WriteScreen(-1);

            // Loop through calibration process
            for (int i = 0; i < 3; i++) {
                int temp = implement->GetSteerCalibrationPoint(i);
                int temp2 = abs(temp) / 10 % 10;

                lcd->WriteBuffer(temp < 0 ? '-' : ' ', 3, 12);
                lcd->WriteBuffer(temp2 + '0', 3, 13);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 14);

                lcd->WriteScreen(3);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                // Adjust loop: drive the axle to the reference angle, then
                // latch the sensor reading there.
                while (true) {
                    lcd->WriteScreen(1);
                    interface->CheckButtons(0, 0);
                    implement->Adjust(interface->GetButtons());

                    if (interface->GetButtons() == 1) {
                        lcd->WriteBuffer('>', 3, 19);
                    }
                    else if (interface->GetButtons() == -1) {
                        lcd->WriteBuffer('<', 3, 19);
                    }
                    else if (interface->GetButtons() == 2) {
                        implement->Adjust(0);

                        implement->SetSteerCalibrationData(i);

                        break;
                    }
                    else {
                        lcd->WriteBuffer(' ', 3, 19);
                    }
                }
            }

            lcd->WriteBuffer(L4_CAL_STEER, 0);
            lcd->WriteBuffer(L4_CAL_DONE, 1);
            lcd->WriteBuffer(L4_BLANK, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);
            delay(1000);
            break;
        }
    }

    // ----------------
    // Angle calibration
    // ----------------
    lcd->WriteBuffer(L4_CAL_ANGLE, 0);
    lcd->WriteBuffer(L4_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L4_CAL_DECLINE, 2);
    lcd->WriteBuffer(L4_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        int buttons = interface->CheckButtons(0, 0);

        if (buttons == -1) {
            lcd->WriteBuffer(L4_CAL_ANGLE, 0);
            lcd->WriteBuffer(L4_CAL_DECLINED, 1);
            lcd->WriteBuffer(L4_BLANK, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);
            delay(1000);
            break;
        }
        else if (buttons == 1) {
            lcd->WriteBuffer(L4_CAL_ANGLE, 0);
            lcd->WriteBuffer(L4_BLANK, 1);
            lcd->WriteBuffer(L4_CAL_ENTER, 2);
            lcd->WriteBuffer(L4_CAL_ANGLE_AD, 3);

            lcd->WriteScreen(-1);

            // Loop through calibration process. Unlike the steer step, the
            // angle sensor isn't driven by any output -- it's read while
            // the operator manually swings the hitch/drawbar to each
            // reference angle -- so there's no Adjust() call here, matching
            // the legacy source's own shape.
            for (int i = 0; i < 3; i++) {
                int temp = implement->GetAngleCalibrationPoint(i);
                int temp2 = abs(temp) / 10 % 10;

                lcd->WriteBuffer(temp < 0 ? '-' : ' ', 3, 12);
                lcd->WriteBuffer(temp2 + '0', 3, 13);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 14);

                lcd->WriteScreen(3);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                delay(1000);

                while (true) {
                    if (interface->CheckButtons(0, 0) == 2) {
                        implement->SetAngleCalibrationData(i);

                        break;
                    }
                    lcd->WriteScreen(1);
                }
            }

            lcd->WriteBuffer(L4_CAL_ANGLE, 0);
            lcd->WriteBuffer(L4_CAL_DONE, 1);
            lcd->WriteBuffer(L4_BLANK, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);
            delay(1000);
            break;
        }
    }

    // ---------------
    // PID gain adjust
    // ---------------
    int kp = implement->GetKP();
    if (adjustValue(L4_CAL_KP, L4_CAL_KP_AD, &kp, 0, 255)) {
        implement->SetKP(byte(kp));
    }

    int ki = implement->GetKI();
    if (adjustValue(L4_CAL_KI, L4_CAL_KI_AD, &ki, 0, 255)) {
        implement->SetKI(byte(ki));
    }

    int kd = implement->GetKD();
    if (adjustValue(L4_CAL_KD, L4_CAL_KD_AD, &kd, 0, 255)) {
        implement->SetKD(byte(kd));
    }

    // ------------
    // Adjust offset
    // ------------
    int offset = implement->GetOffset();
    if (adjustValue(L4_CAL_OFFSET, L4_CAL_OFFSET_AD, &offset, -20, 20)) {
        implement->SetOffset(offset);
    }

    // ----------------------
    // Store calibration data
    // ----------------------
    lcd->WriteBuffer(L4_CAL_COMPLETE, 0);
    lcd->WriteBuffer(L4_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L4_CAL_DECLINE, 2);
    lcd->WriteBuffer(L4_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        int buttons = interface->CheckButtons(0, 0);

        if (buttons == -1) {
            lcd->WriteBuffer(L4_CAL_COMPLETE, 0);
            lcd->WriteBuffer(L4_CAL_DECLINED, 1);
            lcd->WriteBuffer(L4_CAL_NOSAVE, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);

            implement->ResetCalibration();
            tractor->ResetCalibration();

            break;
        }
        else if (buttons == 1) {
            implement->CommitCalibration();
            tractor->CommitCalibration();

            lcd->WriteBuffer(L4_CAL_COMPLETE, 0);
            lcd->WriteBuffer(L4_CAL_DDONE, 1);
            lcd->WriteBuffer(L4_CAL_SAVE, 2);
            lcd->WriteBuffer(L4_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // After calibration rewrite total screen
    interface->UpdateScreen(1);
    lcd->WriteScreen(-1);
}

}  // namespace triton
