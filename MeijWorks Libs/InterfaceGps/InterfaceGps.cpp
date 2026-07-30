/*
  InterfaceGps - an interface for the VehicleGps library
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
#include "InterfaceGps.hpp"

namespace triton
{

InterfaceGps::InterfaceGps(InterfaceI2CLCD* lcd, VehicleGps* gps)
    : gps(gps), lcd(lcd) {
#ifdef DEBUG
    Serial.println("-------------------------------");
    Serial.println("Initialising GPS interface");
    Serial.println("-------------------------------");
#endif

    if (DetectGps()) {
#ifdef DEBUG
        Serial.println("Success");
#endif
    }
}

boolean InterfaceGps::DetectGps() {
    HardwareSerial* serialGps = gps->GetSerial();
    byte rate    = gps->GetBaudrate();
    byte gpsrate = 0;

    byte rates[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };

    int           success  = 0;
    unsigned long baudrate = 0;

    lcd->WriteBuffer("GPS: testing        ", 0);
    lcd->WriteBuffer("GGA string: --      ", 1);
    lcd->WriteBuffer("VTG string: --      ", 2);
    lcd->WriteBuffer("XTE string: --      ", 3);
    lcd->WriteScreen(0xFF);

    for (int i = rate; i < rate + 8; i++) {
        baudrate = long(4800) * rates[i % 8];

        lcd->WriteBuffer('0' + baudrate % 10,           0, 19);
        lcd->WriteBuffer('0' + (baudrate / 10)    % 10, 0, 18);
        lcd->WriteBuffer('0' + (baudrate / 100)   % 10, 0, 17);
        lcd->WriteBuffer('0' + (baudrate / 1000)  % 10, 0, 16);
        lcd->WriteBuffer('0' + (baudrate / 10000) % 10, 0, 15);
        lcd->WriteBuffer('0' + (baudrate / 100000)% 10, 0, 14);

        success = testRate(serialGps, baudrate);

        if (success != 0) {
            gpsrate = i % 8;
            break;
        }
    }

    lcd->WriteScreen(0xFF);

    if (success == 7) {
        serialGps->begin(baudrate);
        gps->SetBaudrate(gpsrate);
        gps->CommitCalibration();
        return true;
    }
    else if (success != 0) {
        serialGps->begin(baudrate);
        gps->SetBaudrate(gpsrate);
        gps->CommitCalibration();
        return false;
    }
    else {
        serialGps->begin(long(4800) * rates[rate]);
        delay(1000);
        return false;
    }
}

byte InterfaceGps::testRate(HardwareSerial* serialGps, unsigned long baudrate) {
    unsigned long starttime = millis();
    bool gga = false;
    bool vtg = false;
    bool xte = false;
    byte result = 0;

    serialGps->begin(baudrate);

#ifndef SCRAPER
    while ((millis() - starttime < 2000 || result != 0)
           && millis() - starttime < 10000 && result != 7) {
#else
    while ((millis() - starttime < 2000 || result != 0)
           && millis() - starttime < 10000 && result != 6) {
#endif
        gps->Update();

        if (millis() - gps->GetGgaFixAge() < 2000) {
            lcd->WriteBuffer('O', 1, 12);
            lcd->WriteBuffer('K', 1, 13);
            gga = true;
        }
        else {
            lcd->WriteBuffer('-', 1, 12);
            lcd->WriteBuffer('-', 1, 13);
            gga = false;
        }

        if (millis() - gps->GetVtgFixAge() < 2000) {
            lcd->WriteBuffer('O', 2, 12);
            lcd->WriteBuffer('K', 2, 13);
            vtg = true;
        }
        else {
            lcd->WriteBuffer('-', 2, 12);
            lcd->WriteBuffer('-', 2, 13);
            vtg = false;
        }

#ifndef SCRAPER
        if (millis() - gps->GetXteFixAge() < 2000) {
            lcd->WriteBuffer('O', 3, 12);
            lcd->WriteBuffer('K', 3, 13);
            xte = true;
        }
        else {
            lcd->WriteBuffer('-', 3, 12);
            lcd->WriteBuffer('-', 3, 13);
            xte = false;
        }
#endif
        result = gga * 4 + vtg * 2 + xte * 1;
        lcd->WriteScreen(1);
    }

    lcd->WriteScreen(0xFF);
    serialGps->end();
    return result;
}

}  // namespace triton