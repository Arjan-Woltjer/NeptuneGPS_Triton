/*
  Spuitcomputer SP - program using tractor wheel speed to dose a hose-pump sprayer
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

#include "CalibrationSprayer.hpp"
#include "ImplementSprayer.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceSprayer.hpp"
#include "LanguageSprayer.hpp"
#include "VehicleTractor.hpp"

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*   gLcd;
triton::VehicleTractor*    gTractor;
triton::ImplementSprayer*  gImplement;
triton::InterfaceSprayer*  gInterface;
triton::CalibrationSprayer* gCalibration;

// -------------
// Setup routine
// -------------
void setup() {
    // Delay to settle voltages and let LCD startup
    delay(3000);

    // Setup serial port
    Serial.begin(SERIALDATARATE);  // Serial for computer native usb

    // Initialise objects and interfaces
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
    gLcd->Begin();

    gTractor = new triton::VehicleTractor(&Serial);

    gImplement = new triton::ImplementSprayer(gTractor);
    gInterface = new triton::InterfaceSprayer(gLcd, gImplement, gTractor);
    gCalibration = new triton::CalibrationSprayer(gLcd, gImplement, gTractor, gInterface);

    // Print message to computer
    Serial.println(S_DIVIDE);
    Serial.print(S_MEIJWORKS);
    Serial.print(" ");
    Serial.println(S_DEVICE);
    Serial.println(S_COPYRIGHT);
    Serial.println(S_DIVIDE);

    // Write message to screen
    // Messages in LanguageSprayer.hpp
    gLcd->WriteBuffer(L_MEIJWORKS, 0);
    gLcd->WriteBuffer(L_DEVICE, 1);
    gLcd->WriteBuffer(L_COPYRIGHT, 2);
    gLcd->WriteBuffer(L_AUTHOR, 3);

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
    // finishes/is cancelled). See CalibrationSprayer.hpp for why this
    // trigger lives here instead of inside InterfaceSprayer itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}
