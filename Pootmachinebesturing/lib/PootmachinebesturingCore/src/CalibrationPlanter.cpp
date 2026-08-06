/*
  CalibrationPlanter - LCD/button calibration wizard for the MeijWorks planter interface
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
#include "CalibrationPlanter.hpp"

namespace triton
{

CalibrationPlanter::CalibrationPlanter(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlanter* implement,
                                        VehicleTractor* tractor, VehicleGps* gps, InterfacePlanter* interface)
    : serialDebug(serialDebug), lcd(lcd), implement(implement), tractor(tractor), gps(gps), interface(interface) {
}

// --------------------------------
// Method for calibrating implement
// --------------------------------
void CalibrationPlanter::Calibrate() {
    // Stop any adjusting
    implement->Stop();

    // Write complete screen
    lcd->WriteScreen(-1);

    // Temporary variables
    int temp = 0, temp2 = 0, temp3 = 0;

    // --------------------
    // Position calibration
    // --------------------
    lcd->WriteBuffer(L3_CAL_POS, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Width calibration
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_POS_AD, 3);

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
                        implement->Adjust(interface->GetMode(), 0);

                        implement->SetPositionCalibrationData(i);

                        break;
                    }
                    else {
                        lcd->WriteBuffer(' ', 3, 19);
                    }
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);
            lcd->WriteBuffer(L3_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

#ifndef GPS
    // ---------------
    // XTE calibration
    // ---------------
    lcd->WriteBuffer(L3_CAL_XTE, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);
            lcd->WriteBuffer(L3_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // XTE calibration
            lcd->WriteBuffer(L3_BLANK, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_XTE_AD, 3);

            lcd->WriteScreen(-1);

            // Loop through calibration process
            for (int i = 0; i < 3; i++) {
                temp = implement->GetXteCalibrationPoint(i);
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
                    if (interface->CheckButtons(0, 0) == 2) {
                        implement->SetXteCalibrationData(i);

                        break;
                    }
                    lcd->WriteScreen(1);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);
            lcd->WriteBuffer(L3_BLANK, 3);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);
#endif

    // ---------
    // Adjust KP
    // ---------
    lcd->WriteBuffer(L3_CAL_KP, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust KP
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_KP_AD, 3);

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
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

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

    // ---------
    // Adjust KI
    // ---------
    lcd->WriteBuffer(L3_CAL_KI, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust KI
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_KI_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetKI();

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
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteBuffer(temp3 + '0', 3, 13);
            lcd->WriteBuffer('.', 3, 14);
            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetKI(temp);
            break;
        }
    }
    delay(1000);

    // ---------
    // Adjust KD
    // ---------
    lcd->WriteBuffer(L3_CAL_KD, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust KD
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_KD_AD, 3);

            lcd->WriteScreen(-1);

            temp = implement->GetKD();

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
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteBuffer(temp3 + '0', 3, 13);
            lcd->WriteBuffer('.', 3, 14);
            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetKD(temp);
            break;
        }
    }
    delay(1000);

    // -----------------
    // Adjust PWM manual
    // -----------------
    lcd->WriteBuffer(L3_CAL_PWM_M, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust PWM manual
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_PWM_M_AD, 3);

            temp = implement->GetPwmMan();

            lcd->WriteScreen(-1);

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
                // calculate derived variables
                temp2 = temp / 10;
                temp3 = temp / 100;

                // write all to screen
                lcd->WriteBuffer(temp3 + '0', 3, 14);
                lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteBuffer(temp3 + '0', 3, 14);
            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetPwmMan(byte(temp));
            break;
        }
    }
    delay(1000);

    // ---------------
    // Adjust PWM auto
    // ---------------
    lcd->WriteBuffer(L3_CAL_PWM_A, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust PWM auto
            lcd->WriteBuffer(L3_CAL_PWM_A, 0);
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_PWM_A_AD, 3);

            temp = implement->GetPwmAuto();

            lcd->WriteScreen(-1);

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

                // Write all to screen
                lcd->WriteBuffer(temp3 % 10 + '0', 3, 14);
                lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteBuffer(temp3 + '0', 3, 14);
            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetPwmAuto(byte(temp));
            break;
        }
    }
    delay(1000);

    // ------------
    // Adjust error
    // ------------
    lcd->WriteBuffer(L3_CAL_OFFSET, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust error
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);
            lcd->WriteBuffer(L3_CAL_OFFSET_AD, 3);

            temp = implement->GetOffset();

            lcd->WriteScreen(-1);

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
                // calculate derived variables
                temp2 = temp / 10;

                // write all to screen
                lcd->WriteBuffer(temp2 + '0', 3, 15);
                lcd->WriteBuffer(temp % 10 + '0', 3, 16);
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteBuffer(temp2 + '0', 3, 15);
            lcd->WriteBuffer(temp % 10 + '0', 3, 16);

            lcd->WriteScreen(-1);

            implement->SetOffset(temp);
            break;
        }
    }
    delay(1000);

    // ----------
    // Enable GPS
    // ----------
    lcd->WriteBuffer(L3_CAL_GPS_EN, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = implement->GetGpsEnabled();

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            implement->SetGpsEnabled(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -----------------
    // Enable XTE sensor
    // -----------------
    lcd->WriteBuffer(L3_CAL_SEN_EN, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = implement->GetSensorEnabled();

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            implement->SetSensorEnabled(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -------------------
    // Enable PWM on valve
    // -------------------
    lcd->WriteBuffer(L3_CAL_PWM_EN, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = implement->GetPwmEnabled();

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            implement->SetPwmEnabled(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -------------------
    // Zwart wit ventiel
    // -------------------
    lcd->WriteBuffer(L3_CAL_1_0_EN, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = implement->GetOnOffValve();

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            implement->SetOnOffValve(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -------------------
    // Inversie hydrauliek
    // -------------------
    lcd->WriteBuffer(L3_CAL_INV_HY, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = implement->GetInvertHydraulics();

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            implement->SetInvertHydraulics(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -----------------------
    // Inversie pootbalksensor
    // -----------------------
    lcd->WriteBuffer(L3_CAL_INV_SE, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

            lcd->WriteScreen(-1);

            temp = implement->GetInvertPlantingelementSensor();

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            implement->SetInvertPlantingelementSensor(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // -------------------
    // Inversie hefsignaal
    // -------------------
    lcd->WriteBuffer(L3_CAL_INV_HI, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Setting sim mode
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

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
                    lcd->WriteBuffer(L3_CAL_ON, 3);
                }
                else {
                    lcd->WriteBuffer(L3_CAL_OFF, 3);
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            tractor->SetInversion(temp);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // --------------
    // Select program
    // --------------
    lcd->WriteBuffer(L3_CAL_PROG, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Adjust program
            lcd->WriteBuffer(L3_CAL_ADJUST, 1);
            lcd->WriteBuffer(L3_CAL_ENTER, 2);

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
                        lcd->WriteBuffer(L3_CAL_PROG_AD2, 3);
                        break;
                    case 3:
                        lcd->WriteBuffer(L3_CAL_PROG_AD3, 3);
                        break;
                    case 4:
                        lcd->WriteBuffer(L3_CAL_PROG_AD4, 3);
                        break;
                    case 5:
                        lcd->WriteBuffer(L3_CAL_PROG_AD5, 3);
                        break;
                    case 6:
                        lcd->WriteBuffer(L3_CAL_PROG_AD6, 3);
                        break;
                    case 7:
                        lcd->WriteBuffer(L3_CAL_PROG_AD7, 3);
                        break;
                    case 8:
                        lcd->WriteBuffer(L3_CAL_PROG_AD8, 3);
                        break;
                }
            }
            lcd->WriteBuffer(L3_CAL_DONE, 1);
            lcd->WriteBuffer(L3_BLANK, 2);

            lcd->WriteScreen(-1);

            EEPROM.write(1, temp);
            break;
        }
    }
    delay(1000);

    // Store calibration data
    lcd->WriteBuffer(L3_CAL_COMPLETE, 0);
    lcd->WriteBuffer(L3_CAL_ACCEPT, 1);
    lcd->WriteBuffer(L3_CAL_DECLINE, 2);
    lcd->WriteBuffer(L3_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == -1) {
            // print message to LCD
            lcd->WriteBuffer(L3_CAL_DECLINED, 1);
            lcd->WriteBuffer(L3_CAL_NOSAVE, 2);

            lcd->WriteScreen(-1);

            tractor->ResetCalibration();
            implement->ResetCalibration();

            break;
        }
        else if (interface->CheckButtons(0, 0) == 1) {
            // Commit data
            tractor->CommitCalibration();
            implement->CommitCalibration();

            // Print message to LCD
            lcd->WriteBuffer(L3_CAL_DDONE, 1);
            lcd->WriteBuffer(L3_CAL_SAVE, 2);

            lcd->WriteScreen(-1);

            break;
        }
    }
    delay(1000);

    // Refresh total screen
    interface->UpdateScreen(1);
    lcd->WriteScreen(-1);
}

}  // namespace triton
