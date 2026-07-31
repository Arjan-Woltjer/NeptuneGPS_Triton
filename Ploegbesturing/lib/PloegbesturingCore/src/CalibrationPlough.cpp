/*
  CalibrationPlough - LCD/button calibration wizard for the MeijWorks plough interface
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
#include "CalibrationPlough.hpp"

namespace triton
{

CalibrationPlough::CalibrationPlough(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlough* implement,
                                      VehicleTractor* tractor, VehicleGps* gps, InterfacePlough* interface)
    : serialDebug(serialDebug), lcd(lcd), implement(implement), tractor(tractor), gps(gps), interface(interface) {
}

// --------------------------------
// Method for calibrating implement
// --------------------------------
void CalibrationPlough::Calibrate() {
    // Stop any adjusting
    implement->Stop();

    // Write complete screen
    lcd->WriteScreen(-1);

    // Temporary variables
    short int temp = 0, temp2 = 0, temp3 = 0;

    // --------------------
    // Position calibration
    // --------------------
    lcd->WriteBuffer(L2_CAL_POS, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Width calibration
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_POS_AD, 3);

            lcd->WriteScreen(-1);

            // Loop through calibration process
            for (int i = 0; i < 3; i++) {
                temp = implement->GetPositionCalibrationPoint(i);
                temp2 = temp / 10;

                if (temp < 0) {
                    lcd->WriteBuffer('-', 3, 12);
                }
                else {
                    lcd->WriteBuffer(' ', 3, 12);
                }
                lcd->WriteBuffer(abs(temp2) + '0', 3, 13);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 14);

                lcd->WriteScreen(3);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                // Adjust loop
                while (true) {
                    lcd->WriteScreen(1);
                    interface->CheckButtons(0, 0);
                    implement->Adjust(interface->GetMode(), interface->GetButtons());

                    if (interface->GetButtons() == 1) {
                        lcd->WriteBuffer('>', 3, 19);
                    }
                    else if (interface->GetButtons() == -1) {
                        lcd->WriteBuffer('<', 3, 19);
                    }
                    else if (interface->GetButtons() == 2) {
                        implement->Stop();

                        implement->SetPositionCalibrationData(i);

                        break;
                    }
                    else {
                        lcd->WriteBuffer(' ', 3, 19);
                    }
                }
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);
            lcd->WriteBuffer(L2_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

#ifdef ROTATION
    // Rotation calibration
    lcd->WriteBuffer(L2_CAL_ROTATION, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Rotation calibration
            lcd->WriteBuffer(L2_BLANK, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_ROTATION_AD, 3);

            lcd->WriteScreen(-1);

            // Loop through calibration process
            for (int i = 0; i < 3; i++) {
                temp = implement->GetRotationCalibrationPoint(i);
                temp2 = temp / 10;

                if (temp < 0) {
                    lcd->WriteBuffer('-', 3, 12);
                }
                else {
                    lcd->WriteBuffer(' ', 3, 12);
                }
                lcd->WriteBuffer(abs(temp2) + '0', 3, 13);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 14);

                lcd->WriteScreen(3);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                // Adjust loop
                while (true) {
                    lcd->WriteScreen(1);
                    interface->CheckButtons(0, 0);

                    if (interface->GetButtons() == 2) {
                        implement->SetRotationCalibrationData(i);

                        break;
                    }
                }
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);
            lcd->WriteBuffer(L2_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);
#endif

#ifdef SPEED_L
    // ----------------
    // SPEED calibration
    // ----------------
    lcd->WriteBuffer(L2_CAL_SPEED, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {

        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);
            lcd->WriteBuffer(L2_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Speed calibration
            lcd->WriteBuffer(L2_BLANK, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_SPEED_AD, 3);

            lcd->WriteScreen(-1);

            // Loop through calibration process
            tractor->ResetWheelspeedPulses();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            // Adjust loop
            while (true) {
                lcd->WriteScreen(1);

                interface->CheckButtons(0, 255);

                temp = tractor->CalibrateSpeed(interface->GetButtons());

                if (interface->GetButtons() == 2) {
                    break;
                }

                temp = temp / 100;

                // Calculate derived variables
                temp2 = temp / 10;
                temp3 = temp / 100;

                lcd->WriteBuffer(abs(temp3) + '0', 3, 14);
                lcd->WriteBuffer(abs(temp2) % 10 + '0', 3, 15);
                lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 16);
            }

            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);
#endif

    // -----------------------
    // Adjust number of shares
    // -----------------------
    lcd->WriteBuffer(L2_CAL_SHARES, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust amount of shares
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_SHARES_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetShares();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            // Adjust loop
            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }
                // Calculate derived variables
                temp2 = temp / 10;

                // Write to screen
                lcd->WriteBuffer(temp2 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteBuffer(temp2 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetShares(temp);
            break;
        }
    }
    delay(1000);

#ifdef PID_KP
    // ---------
    // Adjust KP
    // ---------
    lcd->WriteBuffer(L2_CAL_KP, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust KP
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_KP_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetKP();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            // Adjust loop
            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }
                // Calculate derived variables
                temp2 = temp / 10;
                temp3 = temp / 100;

                // Write to screen
                lcd->WriteBuffer(temp3 + '0', 3, 13);
                lcd->WriteBuffer('.', 3, 14);
                lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteBuffer(temp3 + '0', 3, 13);
            lcd->WriteBuffer('.', 3, 14);
            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetKP(temp);
            break;
        }
    }
    delay(1000);
#endif

#ifdef PWM_MAN
    // -----------------
    // Adjust PWM manual
    // -----------------
    lcd->WriteBuffer(L2_CAL_PWM_M, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust PWM manual
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_PWM_M_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetPwmMan();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            // Adjust loop
            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }
                // Calculate derived variables
                temp2 = temp / 10;

                // Write to screen
                lcd->WriteBuffer(temp2 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteBuffer(temp2 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetPwmMan(byte(temp));
            break;
        }
    }
    delay(1000);
#endif

#ifdef PWM_AUTO
    // ---------------
    // Adjust PWM auto
    // ---------------
    lcd->WriteBuffer(L2_CAL_PWM_A, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust PWM auto
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_PWM_A_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetPwmAuto();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            // Adjust loop
            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }
                // Calculate derived variables
                temp2 = temp / 10;

                // Write to screen
                lcd->WriteBuffer(temp2 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteBuffer(temp2 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetPwmAuto(byte(temp));
            break;
        }
    }
    delay(1000);
#endif

    // ------------
    // Adjust error
    // ------------
    lcd->WriteBuffer(L2_CAL_MARGIN, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust error
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_MARGIN_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetError();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }
                // Calculate derived variables
                temp2 = temp / 10;

                lcd->WriteBuffer(temp2 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteBuffer(temp2 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetError(byte(temp));
            break;
        }
    }
    delay(1000);

    // -------------------------
    // Adjust maximum correction
    // -------------------------
    lcd->WriteBuffer(L2_CAL_MAXCOR, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust maximum correction
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_MAXCOR_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetMaxCorrection();

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }
                // Calculate derived variables
                temp2 = temp / 10;
                temp3 = temp / 100;

                // Write to screen
                lcd->WriteBuffer(temp3 + '0', 3, 14);
                lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }

            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteBuffer(temp3 + '0', 3, 14);
            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetMaxCorrection(temp);
            break;
        }
    }
    delay(1000);

    // -----------------
    // Adjust ploughside
    // -----------------
    lcd->WriteBuffer(L2_CAL_SWAP, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust maximum correction
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);
            lcd->WriteBuffer(L2_CAL_SWAP_AD, 3);

            if (implement->GetSide()) {
                lcd->WriteBuffer('L', 3, 16);
            }
            else {
                lcd->WriteBuffer('R', 3, 16);
            }

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    implement->SetSwap(true);
                }
                else if (interface->GetButtons() == -1) {
                    implement->SetSwap(false);
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }

                if (implement->GetSide()) {
                    lcd->WriteBuffer('L', 3, 16);
                }
                else {
                    lcd->WriteBuffer('R', 3, 16);
                }
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            if (implement->GetSide()) {
                lcd->WriteBuffer('L', 3, 16);
            }
            else {
                lcd->WriteBuffer('R', 3, 16);
            }

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // ----------
    // RTK ident
    // ----------
    lcd->WriteBuffer(L2_CAL_QUAL, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = gps->GetRtkQuality();

            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp = 4;
                    gps->SetRtkQuality(4);
                }
                else if (interface->GetButtons() == -1) {
                    temp = 2;
                    gps->SetRtkQuality(2);
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }

                lcd->WriteBuffer(temp + '0', 3, 16);

            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // ----------
    // Deutz
    // ----------
    lcd->WriteBuffer(L2_CAL_DEUTZ, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = tractor->GetInversion();

            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp = 1;
                }
                else if (interface->GetButtons() == -1) {
                    temp = 0;
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }

                if (temp) {
                    lcd->WriteBuffer(L2_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L2_CAL_OFF, 3);
                }

            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            if (temp) {
                tractor->SetInversion(true);
            }
            else {
                tractor->SetInversion(false);
            }

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -----------------------
    // Adjust program
    // -----------------------
    lcd->WriteBuffer(L2_CAL_PROG, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust amount of shares
            lcd->WriteBuffer(L2_CAL_ADJUST, 1);
            lcd->WriteBuffer(L2_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = 2;

            while (interface->CheckButtons(0, 0) != 0) {
            }

            // Adjust loop
            while (true) {
                lcd->WriteScreen(1);
                interface->CheckButtons(0, 255);

                if (interface->GetButtons() == 1) {
                    temp++;
                    if (temp > 8) {
                        temp = 8;
                    }
                }
                else if (interface->GetButtons() == -1) {
                    temp--;
                    if (temp < 2) {
                        temp = 2;
                    }
                }
                else if (interface->GetButtons() == 2) {
                    break;
                }

                // Write to screen
                switch (temp) {
                    case 2:
                        lcd->WriteBuffer(L2_CAL_PROG_AD2, 3);
                        break;
                    case 3:
                        lcd->WriteBuffer(L2_CAL_PROG_AD3, 3);
                        break;
                    case 4:
                        lcd->WriteBuffer(L2_CAL_PROG_AD4, 3);
                        break;
                    case 5:
                        lcd->WriteBuffer(L2_CAL_PROG_AD5, 3);
                        break;
                    case 6:
                        lcd->WriteBuffer(L2_CAL_PROG_AD6, 3);
                        break;
                    case 7:
                        lcd->WriteBuffer(L2_CAL_PROG_AD7, 3);
                        break;
                    case 8:
                        lcd->WriteBuffer(L2_CAL_PROG_AD8, 3);
                        break;
                }
            }
            lcd->WriteBuffer(L2_CAL_DONE, 1);
            lcd->WriteBuffer(L2_BLANK, 2);

            lcd->WriteScreen(-1);

            EEPROM.write(1, temp);
            break;
        }
    }
    delay(1000);

    // Store calibration data
    lcd->WriteBuffer(L2_CAL_COMPLETE, 0);
    lcd->WriteBuffer(L2_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L2_CAL_DECLINE, 2);
    lcd->WriteBuffer(L2_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L2_CAL_DECLINED, 1);
            lcd->WriteBuffer(L2_CAL_NOSAVE, 2);

            lcd->WriteScreen(-1);

            implement->ResetCalibration();
            tractor->ResetCalibration();

            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Commit data
            implement->CommitCalibration();
            tractor->CommitCalibration();
            gps->CommitCalibration();

            // Print message to LCD
            lcd->WriteBuffer(L2_CAL_DDONE, 1);
            lcd->WriteBuffer(L2_CAL_SAVE, 2);

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
