/*
  CalibrationRooier - LCD/button calibration wizard for the MeijWorks windrower interface
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
#include "CalibrationRooier.hpp"

namespace triton
{

CalibrationRooier::CalibrationRooier(InterfaceI2CLCD* lcd, ImplementRooier* implement,
                                      VehicleTractor* tractor, InterfaceRooier* interface)
    : lcd(lcd), implement(implement), tractor(tractor), interface(interface) {
}

// -------------------------------------------------------------------------
// Shared accept/decline + adjust-by-one-per-press step, matching the shape
// every step in the legacy calibrate() fragment repeated seven times (skew,
// error margin, KP, KI, KD, PWM manual, PWM auto).
// -------------------------------------------------------------------------
bool CalibrationRooier::adjustValue(const char* title, const char* prompt, int* value, int minValue, int maxValue) {
    lcd->WriteBuffer(title, 0);
    lcd->WriteBuffer(L8_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L8_CAL_DECLINE, 2);
    lcd->WriteBuffer(L8_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        int buttons = interface->CheckButtons(0, 0);

        if (buttons == -1) {
            lcd->WriteBuffer(L8_CAL_DECLINED, 1);
            lcd->WriteBuffer(L8_BLANK, 2);

            lcd->WriteScreen(-1);
            delay(1000);
            return false;
        }
        else if (buttons == 1) {
            lcd->WriteBuffer(L8_CAL_ADJUST, 1);
            lcd->WriteBuffer(L8_CAL_ENTER, 2);
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

                lcd->WriteBuffer(temp < 0 ? '-' : ' ', 3, 15);
                lcd->WriteBuffer(temp3 + '0', 3, 16);
                lcd->WriteBuffer(temp2 + '0', 3, 17);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 18);
            }

            *value = temp;

            lcd->WriteBuffer(L8_CAL_DONE, 1);
            lcd->WriteBuffer(L8_BLANK, 2);

            lcd->WriteScreen(-1);
            delay(1000);
            return true;
        }
    }
}

// --------------------------------
// Method for calibrating implement
// --------------------------------
void CalibrationRooier::Calibrate() {
    // Stop any adjusting
    implement->Stop();

    lcd->WriteScreen(-1);

    // ----------------
    // Skew calibration
    // ----------------
    int skew = implement->GetSkew();
    if (adjustValue(L8_CAL_SKEW, L8_CAL_SKEW_AD, &skew, -30, 30)) {
        implement->SetSkew(skew);
    }

    // -----------------
    // Margin calibration
    // -----------------
    int error = implement->GetError();
    if (adjustValue(L8_CAL_MARGIN, L8_CAL_MARGIN_AD, &error, 0, 10)) {
        implement->SetError(byte(error));
    }

    // ---------------------------
    // Height calibration L, then R
    // ---------------------------
    for (int side = 0; side < 2; side++) {
        const char* title = (side == 0) ? L8_CAL_POS_L : L8_CAL_POS_R;

        lcd->WriteBuffer(title, 0);
        lcd->WriteBuffer(L8_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L8_CAL_DECLINE, 2);
        lcd->WriteBuffer(L8_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            int buttons = interface->CheckButtons(0, 0);

            if (buttons == -1) {
                lcd->WriteBuffer(L8_CAL_DECLINED, 1);
                lcd->WriteBuffer(L8_BLANK, 2);

                lcd->WriteScreen(-1);
                delay(1000);
                break;
            }
            else if (buttons == 1) {
                lcd->WriteBuffer(L8_CAL_ADJUST, 1);
                lcd->WriteBuffer(L8_CAL_ENTER, 2);
                lcd->WriteBuffer(L8_CAL_POS_AD, 3);

                lcd->WriteScreen(-1);

                // Loop through the three calibration points
                for (int i = 0; i < 3; i++) {
                    int temp = implement->GetPositionCalibrationPoint(i);
                    int temp2 = abs(temp) / 10 % 10;

                    lcd->WriteBuffer(temp < 0 ? '-' : ' ', 3, 13);
                    lcd->WriteBuffer(temp2 + '0', 3, 14);
                    lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 15);

                    lcd->WriteScreen(3);

                    while (interface->CheckButtons(0, 0) != 0) {
                    }

                    // Adjust loop: drive the implement to the reference
                    // point, then latch the sensor reading there.
                    while (true) {
                        lcd->WriteScreen(1);
                        interface->CheckButtons(0, 0);
                        implement->Adjust(3, interface->GetButtons());

                        if (interface->GetButtons() == 1) {
                            lcd->WriteBuffer('^', 3, 19);
                        }
                        else if (interface->GetButtons() == -1) {
                            lcd->WriteBuffer('v', 3, 19);
                        }
                        else if (interface->GetButtons() == 2) {
                            implement->Adjust(3, 0);

                            if (side == 0) {
                                implement->SetPositionCalibrationDataL(i);
                            }
                            else {
                                implement->SetPositionCalibrationDataR(i);
                            }
                            break;
                        }
                        else {
                            lcd->WriteBuffer(' ', 3, 19);
                        }
                    }
                }

                lcd->WriteBuffer(L8_CAL_DONE, 1);
                lcd->WriteBuffer(L8_BLANK, 2);
                lcd->WriteBuffer(L8_BLANK, 3);

                lcd->WriteScreen(-1);
                delay(1000);
                break;
            }
        }
    }

    // ---------------
    // PID gain adjust
    // ---------------
    int kp = implement->GetKP();
    if (adjustValue(L8_CAL_KP, L8_CAL_KP_AD, &kp, 0, 255)) {
        implement->SetKP(byte(kp));
    }

    int ki = implement->GetKI();
    if (adjustValue(L8_CAL_KI, L8_CAL_KI_AD, &ki, 0, 255)) {
        implement->SetKI(byte(ki));
    }

    int kd = implement->GetKD();
    if (adjustValue(L8_CAL_KD, L8_CAL_KD_AD, &kd, 0, 255)) {
        implement->SetKD(byte(kd));
    }

    // -----------
    // PWM adjust
    // -----------
    int pwmMan = implement->GetPwmMan();
    if (adjustValue(L8_CAL_PWM_M, L8_CAL_PWM_M_AD, &pwmMan, 0, 255)) {
        implement->SetPwmMan(byte(pwmMan));
    }

    int pwmAuto = implement->GetPwmAuto();
    if (adjustValue(L8_CAL_PWM_A, L8_CAL_PWM_A_AD, &pwmAuto, 0, 255)) {
        implement->SetPwmAuto(byte(pwmAuto));
    }

    // ----------------------
    // Store calibration data
    // ----------------------
    lcd->WriteBuffer(L8_CAL_COMPLETE, 0);
    lcd->WriteBuffer(L8_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L8_CAL_DECLINE, 2);
    lcd->WriteBuffer(L8_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        int buttons = interface->CheckButtons(0, 0);

        if (buttons == -1) {
            lcd->WriteBuffer(L8_CAL_DECLINED, 1);
            lcd->WriteBuffer(L8_CAL_NOSAVE, 2);

            lcd->WriteScreen(-1);

            implement->ResetCalibration();
            tractor->ResetCalibration();

            break;
        }
        else if (buttons == 1) {
            implement->CommitCalibration();
            tractor->CommitCalibration();

            lcd->WriteBuffer(L8_CAL_DDONE, 1);
            lcd->WriteBuffer(L8_CAL_SAVE, 2);

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
