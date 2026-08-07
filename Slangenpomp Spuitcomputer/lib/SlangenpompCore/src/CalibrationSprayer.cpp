/*
  CalibrationSprayer - LCD/button calibration wizard for the MeijWorks sprayer interface
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
#include "CalibrationSprayer.hpp"

namespace triton
{

CalibrationSprayer::CalibrationSprayer(InterfaceI2CLCD* lcd, ImplementSprayer* implement,
                                        VehicleTractor* tractor, InterfaceSprayer* interface)
    : lcd(lcd), implement(implement), tractor(tractor), interface(interface) {
}

// --------------------------------
// Method for calibrating implement
// --------------------------------
void CalibrationSprayer::Calibrate() {
    // Stop any adjusting
    implement->Update(interface->GetMode(), interface->GetButtons());
    bool exitFlag = false;
    bool saveFlag = true;

    // Write complete screen
    lcd->WriteScreen(-1);

    // Temporary variables
    unsigned int temp = 0;
    unsigned int temp2 = 0;
    unsigned int temp3 = 0;
    unsigned int temp4 = 0;
    unsigned int temp5 = 0;

    while (!exitFlag) {
        // -----------
        // Section SIM
        // -----------
        lcd->WriteBuffer(L_MEN_SIM, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == 1) {
                // ---------------
                // Simulation mode
                // ---------------
                lcd->WriteBuffer(L_CAL_SIM, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        lcd->WriteBuffer(L_CAL_DECLINED, 1);
                        lcd->WriteBuffer(L_BLANK, 2);
                        lcd->WriteBuffer(L_BLANK, 3);

                        lcd->WriteScreen(-1);
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        // Setting sim mode
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);

                        lcd->WriteScreen(-1);

                        temp = tractor->GetSim();

                        while (true) {
                            lcd->WriteScreen(1);
                            interface->CheckButtons(0, 255);

                            if (interface->GetButtons() == 1) {
                                temp = 1;
                                saveFlag = false;
                            }
                            else if (interface->GetButtons() == -1) {
                                temp = 0;
                                saveFlag = false;
                            }
                            else if (interface->GetButtons() == 2) {
                                break;
                            }

                            if (temp) {
                                lcd->WriteBuffer(L_CAL_ON, 3);
                            }
                            else {
                                lcd->WriteBuffer(L_CAL_OFF, 3);
                            }
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        if (temp) {
                            tractor->EnableSim();
                        }
                        else {
                            tractor->DisableSim();
                        }

                        lcd->WriteScreen(-1);

                        break;
                    }
                }
                delay(1000);

                // ------------
                // Set simspeed
                // ------------
                lcd->WriteBuffer(L_CAL_SIMS, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        lcd->WriteBuffer(L_CAL_DECLINED, 1);
                        lcd->WriteBuffer(L_BLANK, 2);
                        lcd->WriteBuffer(L_BLANK, 3);

                        lcd->WriteScreen(-1);
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        // Sim speed setting
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_SIMS_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = tractor->GetSimSpeedKmh();

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
                            lcd->WriteBuffer(temp2 + '0', 3, 13);
                            lcd->WriteBuffer('.', 3, 14);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 15);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteScreen(-1);

                        tractor->SetSimSpeedKmh(temp);
                        saveFlag = false;
                        break;
                    }
                }
                delay(1000);

                // -----------
                // Set simtime
                // -----------
                lcd->WriteBuffer(L_CAL_SIMT, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        lcd->WriteBuffer(L_CAL_DECLINED, 1);
                        lcd->WriteBuffer(L_BLANK, 2);
                        lcd->WriteBuffer(L_BLANK, 3);

                        lcd->WriteScreen(-1);
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        // Sim time setting
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_SIMT_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = tractor->GetSimTime();

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
                            lcd->WriteBuffer(temp2 + '0', 3, 14);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 15);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteScreen(-1);

                        tractor->SetSimTime(temp);
                        saveFlag = false;
                        break;
                    }
                }
                delay(1000);
                break;
            }
            else if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
        }

        // =============
        // Section Speed
        // =============

        // -----------------
        // SPEED calibration
        // -----------------
        lcd->WriteBuffer(L_CAL_SPEED, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
            else if (interface->CheckButtons(0, 0) == 1) {
                // SPEED calibration
                lcd->WriteBuffer(L_CAL_SPEED_100, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        // Manual speed calibration pulses per 100m
                        lcd->WriteBuffer(L_CAL_SPEED_MAN, 0);
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_SPEED_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = tractor->GetVconst();

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
                            temp4 = temp / 1000;

                            // write all to screen
                            lcd->WriteBuffer(temp4 + '0', 3, 13);
                            lcd->WriteBuffer(temp3 % 10 + '0', 3, 14);
                            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }

                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteScreen(-1);

                        tractor->SetVconst(temp);
                        saveFlag = false;
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        // Speed calibration pulses per 100m driving
                        lcd->WriteBuffer(L_CAL_SPEED_100, 0);
                        lcd->WriteBuffer(L_BLANK, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_SPEED_AD, 3);

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
                            saveFlag = false;

                            if (interface->GetButtons() == 2) {
                                break;
                            }

                            // Calculate derived variables
                            temp2 = temp / 10;
                            temp3 = temp / 100;
                            temp4 = temp / 1000;
                            temp5 = temp / 10000;

                            lcd->WriteBuffer(temp5 + '0', 3, 12);
                            lcd->WriteBuffer(temp4 % 10 + '0', 3, 13);
                            lcd->WriteBuffer(temp3 % 10 + '0', 3, 14);
                            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }

                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteScreen(-1);

                        break;
                    }
                }
                delay(1000);
                break;
            }
            else if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
        }

        // ============
        // Section Flow
        // ============

        // ----------------
        // Flow calibration
        // ----------------
        lcd->WriteBuffer(L_CAL_FLOW, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
            else if (interface->CheckButtons(0, 0) == 1) {
                // PWM calibration
                lcd->WriteBuffer(L_BLANK, 1);
                lcd->WriteBuffer(L_BLANK, 2);
                lcd->WriteBuffer(L_CAL_FLOW_AD, 3);

                lcd->WriteScreen(-1);

                // Loop through calibration process (12 points, 5s each,
                // real time -- see ImplementSprayer::CalibratePump())
                implement->CalibratePump();

                while (true) {
                    // FLOW calibration
                    lcd->WriteBuffer(L_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L_CAL_ENTER, 2);
                    lcd->WriteBuffer(L_CAL_FLOW_AD, 3);

                    lcd->WriteScreen(-1);

                    temp = implement->GetFlowCalibration();

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
                        temp4 = temp / 1000;

                        lcd->WriteBuffer(abs((int)temp4) + '0', 3, 9);
                        lcd->WriteBuffer(abs((int)temp3) % 10 + '0', 3, 10);
                        lcd->WriteBuffer(abs((int)temp2) % 10 + '0', 3, 11);
                        lcd->WriteBuffer(abs((int)temp) % 10 + '0', 3, 12);
                    }
                    implement->SetFlowCalibration(temp);
                    saveFlag = false;

                    lcd->WriteBuffer(L_CAL_DONE, 1);
                    lcd->WriteBuffer(L_BLANK, 2);

                    lcd->WriteScreen(-1);

                    break;
                }
                temp2 = temp / 10;
                temp3 = temp / 100;

                lcd->WriteBuffer(L_CAL_DONE, 1);

                lcd->WriteBuffer(abs((int)temp4) + '0', 3, 9);
                lcd->WriteBuffer(abs((int)temp3) % 10 + '0', 3, 10);
                lcd->WriteBuffer(abs((int)temp2) % 10 + '0', 3, 11);
                lcd->WriteBuffer(abs((int)temp) % 10 + '0', 3, 12);

                lcd->WriteScreen(-1);
                delay(1000);
                break;
            }
        }

        // ==============
        // Section System
        // ==============
        lcd->WriteBuffer(L_MEN_SYSTEM, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == 1) {
                // -----------------
                // Teeth calibration
                // -----------------
                lcd->WriteBuffer(L_CAL_TEETH, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_TEETH_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = implement->GetTeeth();

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
                            temp2 = temp / 10;

                            lcd->WriteBuffer(temp2 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteBuffer(temp2 + '0', 3, 15);
                        lcd->WriteBuffer(temp % 10 + '0', 3, 16);

                        lcd->WriteScreen(-1);

                        implement->SetTeeth(temp);
                        saveFlag = false;
                        delay(1000);
                        break;
                    }
                }

                // -----------------
                // Pumps calibration
                // -----------------
                lcd->WriteBuffer(L_CAL_PUMPS, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_PUMPS_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = implement->GetPumps();

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
                            temp2 = temp / 10;

                            lcd->WriteBuffer(temp2 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteScreen(-1);

                        implement->SetPumps(temp);
                        saveFlag = false;
                        delay(1000);
                        break;
                    }
                }

                // -----------------
                // Width calibration
                // -----------------
                lcd->WriteBuffer(L_CAL_WIDTH, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_WIDTH_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = implement->GetWidth();

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
                            temp2 = temp / 10;

                            lcd->WriteBuffer(temp2 + '0', 3, 17);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 18);
                            lcd->WriteBuffer('0', 3, 19);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteScreen(-1);

                        implement->SetWidth(temp);
                        saveFlag = false;
                        delay(1000);
                        break;
                    }
                }
                break;
            }
            else if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
        }

        // ===========
        // Section PID
        // ===========
        lcd->WriteBuffer(L_MEN_PID, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == 1) {
                // ---------
                // Adjust KP
                // ---------
                lcd->WriteBuffer(L_CAL_KP, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_KP_AD, 3);

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
                            temp2 = temp / 10;
                            temp3 = temp / 100;

                            lcd->WriteBuffer(temp3 + '0', 3, 13);
                            lcd->WriteBuffer('.', 3, 14);
                            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteBuffer(temp3 + '0', 3, 13);
                        lcd->WriteBuffer('.', 3, 14);
                        lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                        lcd->WriteBuffer(temp % 10 + '0', 3, 16);

                        lcd->WriteScreen(-1);

                        implement->SetKP(temp);
                        saveFlag = false;
                        delay(1000);
                        break;
                    }
                }

                // ---------
                // Adjust KI
                // ---------
                lcd->WriteBuffer(L_CAL_KI, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_KI_AD, 3);

                        lcd->WriteScreen(-1);

                        temp = implement->GetKI();

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
                            temp2 = temp / 10;
                            temp3 = temp / 100;

                            lcd->WriteBuffer(temp3 + '0', 3, 13);
                            lcd->WriteBuffer('.', 3, 14);
                            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteBuffer(temp3 + '0', 3, 13);
                        lcd->WriteBuffer('.', 3, 14);
                        lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                        lcd->WriteBuffer(temp % 10 + '0', 3, 16);

                        lcd->WriteScreen(-1);

                        implement->SetKI(temp);
                        saveFlag = false;
                        delay(1000);
                        break;
                    }
                }

                // ---------
                // Adjust KD
                // ---------
                lcd->WriteBuffer(L_CAL_KD, 0);
                lcd->WriteBuffer(L_CAL_ACCEPT, 1);
                lcd->WriteBuffer(L_CAL_DECLINE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);

                while (interface->CheckButtons(0, 0) != 0) {
                }

                while (true) {
                    if (interface->CheckButtons(0, 0) == -1) {
                        break;
                    }
                    else if (interface->CheckButtons(0, 0) == 1) {
                        lcd->WriteBuffer(L_CAL_ADJUST, 1);
                        lcd->WriteBuffer(L_CAL_ENTER, 2);
                        lcd->WriteBuffer(L_CAL_KD_AD, 3);

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
                            temp2 = temp / 10;
                            temp3 = temp / 100;

                            lcd->WriteBuffer(temp3 + '0', 3, 13);
                            lcd->WriteBuffer('.', 3, 14);
                            lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                            lcd->WriteBuffer(temp % 10 + '0', 3, 16);
                        }
                        lcd->WriteBuffer(L_CAL_DONE, 1);
                        lcd->WriteBuffer(L_BLANK, 2);

                        lcd->WriteBuffer(temp3 + '0', 3, 13);
                        lcd->WriteBuffer('.', 3, 14);
                        lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                        lcd->WriteBuffer(temp % 10 + '0', 3, 16);

                        lcd->WriteScreen(-1);

                        implement->SetKD(temp);
                        saveFlag = false;
                        delay(1000);
                        break;
                    }
                }
                break;
            }
            else if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
        }

        // ====
        // Exit
        // ====
        lcd->WriteBuffer(L_CAL_EXIT, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
            else if (interface->CheckButtons(0, 0) == 1) {
                exitFlag = true;
                delay(1000);
                break;
            }
        }
    }

    if (!saveFlag) {
        // ======================
        // Store calibration data
        // ======================
        lcd->WriteBuffer(L_CAL_COMPLETE, 0);
        lcd->WriteBuffer(L_CAL_ACCEPT, 1);
        lcd->WriteBuffer(L_CAL_DECLINE, 2);
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);

        while (interface->CheckButtons(0, 0) != 0) {
        }

        while (true) {
            if (interface->CheckButtons(0, 0) == -1) {
                break;
            }
            else if (interface->CheckButtons(0, 0) == 1) {
                // Commit data
                implement->CommitCalibration();
                tractor->CommitCalibration();
                saveFlag = true;

                // Print message to LCD
                lcd->WriteBuffer(L_CAL_COMPLETE, 0);
                lcd->WriteBuffer(L_CAL_DDONE, 1);
                lcd->WriteBuffer(L_CAL_SAVE, 2);
                lcd->WriteBuffer(L_BLANK, 3);

                lcd->WriteScreen(-1);
                delay(1000);
                break;
            }
        }
    }

    // After calibration reset all values to their stored values (whether
    // just committed or left unsaved -- same unconditional refresh-from-EEPROM
    // the legacy source did here, not the accept-commits/decline-resets split
    // Ploegbesturing's/Pootmachinebesturing's calibration wizards use).
    implement->ResetCalibration();
    tractor->ResetCalibration();

    // After calibration rewrite total screen
    interface->UpdateScreen(1);
    lcd->WriteScreen(-1);
}

}  // namespace triton
