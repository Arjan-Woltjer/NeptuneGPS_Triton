/*
  VehicleTractor - a library for a tractor
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
#include "VehicleTractor.h"

VehicleTractor::VehicleTractor(Stream* serialDebug)
    : serialDebug(serialDebug),
      speed(0), simspeed(1.0f), simtime(2),
      sim(true), inversion(false),
      vConst(250), wheelspeedPulses(0), distance(0),
      wheelspeedPulse(false), updateAge(0) {
#ifdef DEBUG
    serialDebug->println("-------------------------------");
    serialDebug->println("Initialising tractor");
    serialDebug->println("-------------------------------");
#endif

    pinMode(WHEEL_SPEED_PIN_1, INPUT);
    pinMode(HITCH_PIN_1, INPUT);

#if defined __MK66FX1M0__ || defined __MK64FX512__ || defined __MK20DX256__ || __IMXRT1062__
    digitalWrite(WHEEL_SPEED_PIN_1, HIGH);
    digitalWrite(HITCH_PIN_1, HIGH);
#else
    digitalWrite(WHEEL_SPEED_PIN_1, LOW);
    digitalWrite(HITCH_PIN_1, LOW);
#endif

    updateAge = millis();
    readCalibrationData();

#ifdef DEBUG
    PrintCalibrationData();
#endif
}

void VehicleTractor::Update(byte mode) {
    if (mode != 4) {
        if (wheelspeedPulse != digitalRead(WHEEL_SPEED_PIN_1)) {
            wheelspeedPulses++;
            wheelspeedPulse = !wheelspeedPulse;
        }

        if (millis() - updateAge >= 1000) {
            speed = float(wheelspeedPulses) * 40 / vConst;

            updateAge = millis();

            if (mode == 0) {
                distance += wheelspeedPulses;
            }

            wheelspeedPulses = 0;
        }
    }
    else {
        speed = simspeed;
    }
}

unsigned int VehicleTractor::CalibrateSpeed(int buttons) {
    if (wheelspeedPulse != digitalRead(WHEEL_SPEED_PIN_1)) {
        wheelspeedPulses++;
        wheelspeedPulse = !wheelspeedPulse;
    }

    if (buttons == 2) {
        vConst = wheelspeedPulses;
    }

    return wheelspeedPulses;
}

bool VehicleTractor::readCalibrationData() {
    if (EEPROM.read(20) != 255 || EEPROM.read(21) != 255
        || EEPROM.read(22) != 255 || EEPROM.read(24) != 255
        || EEPROM.read(26) != 255 || EEPROM.read(28) != 255) {
        vConst   = word(EEPROM.read(20), EEPROM.read(21));
        simspeed = EEPROM.read(22) / 10.0f;
        simtime  = EEPROM.read(24);
        if (simtime > 10) {
            simtime = 10;
        }
        sim       = EEPROM.read(26);
        inversion = EEPROM.read(28);
        return true;
    }
    return false;
}

void VehicleTractor::PrintCalibrationData() {
    serialDebug->println("-------------------------------");
    serialDebug->println("Tractor using following data:");
    serialDebug->println("-------------------------------");
    serialDebug->println("Speed calibration");
    serialDebug->println(vConst);
    serialDebug->println("-------------------------------");
    serialDebug->println("Simspeed");
    serialDebug->println(simspeed);
    serialDebug->println("-------------------------------");
    serialDebug->println("Sim time");
    serialDebug->println(simtime);
    serialDebug->println("-------------------------------");
    serialDebug->println("Sim mode");
    serialDebug->println(sim);
    serialDebug->println("-------------------------------");
    serialDebug->println("Invert hitch signal");
    serialDebug->println(inversion);
    serialDebug->println("-------------------------------");
}

void VehicleTractor::writeCalibrationData() {
    EEPROM.write(20, highByte(vConst));
    EEPROM.write(21, lowByte(vConst));
    EEPROM.write(22, byte(simspeed * 10));
    EEPROM.write(24, simtime);
    EEPROM.write(26, sim);
    EEPROM.write(28, inversion);
}