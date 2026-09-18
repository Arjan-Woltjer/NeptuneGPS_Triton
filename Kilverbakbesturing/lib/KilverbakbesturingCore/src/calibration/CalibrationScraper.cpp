/*
  CalibrationScraper - LCD/button calibration wizard for the MeijWorks scraper interface
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
#include "CalibrationScraper.hpp"

namespace triton
{

CalibrationScraper::CalibrationScraper(InterfaceI2CLCD* lcd, ImplementScraper* implement, VehicleTractor* tractor,
                                        GuidanceSource* guidance, SerialGuidanceChannel* gpsChannel, InterfaceScraper* interface)
    : lcd(lcd), implement(implement), tractor(tractor), guidance(guidance), gpsChannel(gpsChannel), interface(interface) {
    loadGuidanceCalibration();
}

// Returns true when a stored index was found; an erased byte (255) leaves
// the default 0 (4800, the common NMEA default).
bool CalibrationScraper::loadGuidanceCalibration() {
    const byte stored = EEPROM.read(kEepromGpsBaudIndex);
    if (stored == 255) return false;
    SetGpsBaudIndex(stored);
    return true;
}

void CalibrationScraper::CommitGuidanceCalibration() {
    EEPROM.write(kEepromGpsBaudIndex, gpsBaudIndex);
}

void CalibrationScraper::PrintCalibrationData(Stream* serial) {
    static const byte rates[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };
    serial->println("=====================================");
    serial->println("Guidance source using following data:");
    serial->println("=====================================");
    serial->println("Baudrate");
    serial->println(rates[gpsBaudIndex % 8] * long(4800));
    serial->println("-------------------------------");
}

// --------------------------------
// Method for calibrating implement
// --------------------------------
void CalibrationScraper::Calibrate() {
    // Stop any adjusting
    implement->Stop();

    // Write complete screen
    lcd->WriteScreen(-1);

    // Temporary variables
    int temp = 0, temp2 = 0, temp3 = 0;

    // --------------------------
    // Set refpoints or calibrate
    // --------------------------
    lcd->WriteBuffer(L5_CHOICE, 0);
    lcd->WriteBuffer(L5_REF, 1);
    lcd->WriteBuffer(L5_CAL, 2);
    lcd->WriteBuffer(L5_BLANK, 3);

    lcd->WriteScreen(-1);

    while (interface->CheckButtons(0, 0) != 0) {
    }

    while (true) {
        if (interface->CheckButtons(0, 0) == 1) {
            // --------------------
            // Set reference points
            // --------------------

            // --------------
            // Set refpoint A
            // --------------
            lcd->WriteBuffer(L5_CAL_REF, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                gpsChannel->Update();

                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Set refpoint
                    lcd->WriteBuffer(L5_CAL_REF_AD, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);
                    lcd->WriteBuffer(L5_BLANK, 3);

                    lcd->WriteScreen(-1);

                    implement->SetRefA();

                    while (interface->CheckButtons(0, 0) != 0) {
                    }
                    break;
                }
            }
            delay(1000);

            // --------------
            // Set refpoint B
            // --------------
            lcd->WriteBuffer(L5_CAL_REF, 0);
            lcd->WriteBuffer('B', 0, 16);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                gpsChannel->Update();

                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Set refpoint
                    lcd->WriteBuffer(L5_CAL_REF_AD, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);
                    lcd->WriteBuffer(L5_BLANK, 3);

                    lcd->WriteScreen(-1);

                    implement->SetRefB();

                    while (interface->CheckButtons(0, 0) != 0) {
                    }
                    break;
                }
            }
            delay(1000);

            // After calibration rewrite total screen
            interface->UpdateScreen(1);
            lcd->WriteScreen(-1);
            break;
        }
        else if (interface->CheckButtons(0, 0) == -1) {
            // ---------
            // Calibrate
            // ---------

            // ------------
            // Adjust slope
            // ------------
            lcd->WriteBuffer(L5_CAL_SLOPE, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Adjust slope
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_SLOPE_AD, 3);

                    lcd->WriteScreen(-1);

                    temp = implement->GetSlope();

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
                        temp2 = abs(temp) / 10;
                        temp3 = abs(temp) / 100;

                        // Write to screen
                        if (temp < 0) {
                            lcd->WriteBuffer('-', 3, 13);
                        }
                        else {
                            lcd->WriteBuffer(' ', 3, 13);
                        }
                        lcd->WriteBuffer(temp3 + '0', 3, 14);
                        lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                        lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 16);
                    }
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    if (temp < 0) {
                        lcd->WriteBuffer('-', 3, 13);
                    }
                    else {
                        lcd->WriteBuffer(' ', 3, 13);
                    }
                    lcd->WriteBuffer(temp3 + '0', 3, 14);
                    lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                    lcd->WriteBuffer(abs(temp) % 10 + '0', 3, 16);

                    lcd->WriteScreen(-1);

                    implement->SetSlope(temp);
                    break;
                }
            }
            delay(1000);

#ifndef NOSENS
            // --------------------
            // Position calibration
            // --------------------
            lcd->WriteBuffer(L5_CAL_POS, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Position calibration
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_POS_AD, 3);

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
                            implement->Adjust(2, interface->GetButtons());

                            if (interface->GetButtons() == 1) {
                                lcd->WriteBuffer('>', 3, 19);
                            }
                            else if (interface->GetButtons() == -1) {
                                lcd->WriteBuffer('<', 3, 19);
                            }
                            else if (interface->GetButtons() == 2) {
                                implement->Adjust(2, 0);

                                implement->SetPositionCalibrationData(i);

                                break;
                            }
                            else {
                                lcd->WriteBuffer(' ', 3, 19);
                            }
                        }
                    }
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);
                    lcd->WriteBuffer(L5_BLANK, 3);

                    lcd->WriteScreen(-1);

                    break;
                }
            }
            delay(1000);
#endif

#ifdef SPEED_L
            // -----------------
            // SPEED calibration
            // -----------------
            lcd->WriteBuffer(L5_CAL_SPEED, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);
                    lcd->WriteBuffer(L5_BLANK, 3);

                    lcd->WriteScreen(-1);

                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Speed calibration
                    lcd->WriteBuffer(L5_BLANK, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_SPEED_AD, 3);

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

                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);

                    break;
                }
            }
            delay(1000);
#endif

#ifdef PID_KP
            // ---------
            // Adjust KP
            // ---------
            lcd->WriteBuffer(L5_CAL_KP, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Adjust KP
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_KP_AD, 3);

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
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

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
            lcd->WriteBuffer(L5_CAL_PWM_M, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Adjust PWM manual
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_PWM_M_AD, 3);

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
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

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
            lcd->WriteBuffer(L5_CAL_PWM_A, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Adjust PWM auto
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_PWM_A_AD, 3);

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
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

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
            lcd->WriteBuffer(L5_CAL_MARGIN, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Adjust error
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_MARGIN_AD, 3);

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
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteBuffer(temp2 + '0', 3, 15);
                    lcd->WriteBuffer(temp % 10 + '0', 3, 16);

                    lcd->WriteScreen(-1);

                    implement->SetError(byte(temp));
                    break;
                }
            }
            delay(1000);

#ifndef NOSENS
            // -------------------------
            // Adjust maximum correction
            // -------------------------
            lcd->WriteBuffer(L5_CAL_MAXCOR, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteScreen(-1);
                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Adjust maximum correction
                    lcd->WriteBuffer(L5_CAL_ADJUST, 1);
                    lcd->WriteBuffer(L5_CAL_ENTER, 2);
                    lcd->WriteBuffer(L5_CAL_MAXCOR_AD, 3);

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
                    lcd->WriteBuffer(L5_CAL_DONE, 1);
                    lcd->WriteBuffer(L5_BLANK, 2);

                    lcd->WriteBuffer(temp3 + '0', 3, 14);
                    lcd->WriteBuffer(temp2 % 10 + '0', 3, 15);
                    lcd->WriteBuffer(temp % 10 + '0', 3, 16);

                    lcd->WriteScreen(-1);

                    implement->SetMaxCorrection(temp);
                    break;
                }
            }
            delay(1000);
#endif

            // Store calibration data
            lcd->WriteBuffer(L5_CAL_COMPLETE, 0);
            lcd->WriteBuffer(L5_CAL_ACCEPT, 1);
            lcd->WriteBuffer(L5_CAL_DECLINE, 2);
            lcd->WriteBuffer(L5_BLANK, 3);

            lcd->WriteScreen(-1);

            while (interface->CheckButtons(0, 0) != 0) {
            }

            while (true) {
                if (interface->CheckButtons(0, 0) == -1) {
                    lcd->WriteBuffer(L5_CAL_DECLINED, 1);
                    lcd->WriteBuffer(L5_CAL_NOSAVE, 2);

                    lcd->WriteScreen(-1);

                    implement->ResetCalibration();
                    tractor->ResetCalibration();

                    break;
                }
                else if (interface->CheckButtons(0, 0) == 1) {
                    // Commit data
                    implement->CommitCalibration();
                    tractor->CommitCalibration();

                    // Print message to LCD
                    lcd->WriteBuffer(L5_CAL_DDONE, 1);
                    lcd->WriteBuffer(L5_CAL_SAVE, 2);

                    lcd->WriteScreen(-1);

                    break;
                }
            }
            delay(1000);

            // After calibration rewrite total screen
            interface->UpdateScreen(1);
            lcd->WriteScreen(-1);

            break;
        }
    }
}

}  // namespace triton
