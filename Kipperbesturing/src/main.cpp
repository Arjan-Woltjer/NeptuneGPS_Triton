/*
  Kipperbesturing - program controlling a steering axle proportionally to hitch/drawbar angle
  Copyright (C) 2011-2026 J.A. Woltjer.

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
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include "calibration/CalibrationKipper.hpp"
#include "implement/ImplementKipper.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceKipper.hpp"
#include "config/LanguageKipper.hpp"
#include "VehicleTractor.hpp"

// Serial ports
HardwareSerial* gSerialDebug = &Serial2;

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*   gLcd;
triton::VehicleTractor*    gTractor;
triton::ImplementKipper*   gImplement;
triton::InterfaceKipper*   gInterface;
triton::CalibrationKipper* gCalibration;

// -------------
// Setup routine
// -------------
void setup() {
    // Delay to settle voltages and let LCD startup
    delay(3000);

    // Setup serial ports
    gSerialDebug->begin(SERIALDATARATE);  // Serial for computer native usb

    // Initialise objects and interfaces
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
    gLcd->Begin();

    gTractor = new triton::VehicleTractor(gSerialDebug);

    gImplement = new triton::ImplementKipper(gTractor);
    gInterface = new triton::InterfaceKipper(gLcd, gImplement, gTractor);
    gCalibration = new triton::CalibrationKipper(gLcd, gImplement, gTractor, gInterface);

    // Print message to computer
    gSerialDebug->println("-------------------------------");
    gSerialDebug->println("-----------MeijWorks-----------");
    gSerialDebug->println("-------------------------------");
    gSerialDebug->println("   Kippercontrol version 1.0   ");
    gSerialDebug->println("(c) 2011 - 2026 by J.A. Woltjer");
    gSerialDebug->println("-------------------------------");

    gTractor->PrintCalibrationData();
    gImplement->PrintCalibrationData();

    // Write message to screen
    // Messages in LanguageKipper.hpp
    gLcd->WriteBuffer(L4_MEIJWORKS, 0);
    gLcd->WriteBuffer(L4_DEVICE, 1);
    gLcd->WriteBuffer(L4_COPYRIGHT, 2);
    gLcd->WriteBuffer(L4_AUTHOR, 3);

    gLcd->WriteScreen(-1);

    // Delay for splashscreen
    delay(2000);

    // Write initial interface to screen
    gInterface->UpdateScreen(1);
}

// ---------
// Main loop
// ---------
void loop() {
    // Update interface
    gInterface->Update();

    // Both buttons held -> enter the calibration wizard (blocking until it
    // finishes/is cancelled). See CalibrationKipper.hpp for why this
    // trigger lives here instead of inside InterfaceKipper itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}
