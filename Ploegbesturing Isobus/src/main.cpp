/*
  Ploegbesturing Isobus - ISOBUS plough offset controller
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

#include "CalibrationPlough.hpp"
#include "ImplementPlough.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlough.hpp"
#include "IsobusGuidanceChannel.hpp"
#include "IsobusGuidanceSource.hpp"
#include "IsobusVtInterface.hpp"
#include "LanguagePlough.hpp"
#include "VehicleTractor.hpp"

// Serial port -- no serial GPS left, CAN-only guidance data acquisition via
// IsobusGuidanceChannel/AgIsoStack.
HardwareSerial* gSerialDebug = &Serial2;

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*       gLcd;
triton::VehicleTractor*        gTractor;
triton::ImplementPlough*       gImplement;
triton::InterfacePlough*       gInterface;
triton::CalibrationPlough*     gCalibration;
triton::IsobusGuidanceSource*  gGuidance;
triton::IsobusGuidanceChannel* gGuidanceChannel;
triton::IsobusVtInterface*     gVtInterface;

// -------------
// Setup routine
// -------------
void setup() {
    // Delay to settle voltages and let LCD startup
    delay(3000);

    // Setup serial ports
    gSerialDebug->begin(SERIALDATARATE);  // Serial for computer native usb

    // gGuidance and gImplement have no CAN/AgIsoStack dependency, so they're
    // constructed first -- gGuidanceChannel needs both already built (it
    // feeds gGuidance from its PGN callbacks and calls gImplement->Stop()
    // on AISO).
    gGuidance = new triton::IsobusGuidanceSource(gSerialDebug);
    gImplement = new triton::ImplementPlough(gSerialDebug, gGuidance);

    // CAN/ISOBUS bring-up: CAN hardware plugin, NAME + address claim (blocks
    // until claimed), PGN callback registration, initial PGN requests.
    gGuidanceChannel = new triton::IsobusGuidanceChannel(gSerialDebug, gGuidance, gImplement);
    gGuidanceChannel->Begin();

    gVtInterface = new triton::IsobusVtInterface(gSerialDebug, gImplement, gGuidance, gGuidanceChannel->GetControlFunction());
    gVtInterface->Begin();

    // Initialise objects and interfaces
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
    gLcd->Begin();

    gTractor = new triton::VehicleTractor(gSerialDebug);
    gInterface = new triton::InterfacePlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance);
    gCalibration = new triton::CalibrationPlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance, gInterface);

    // Print message to computer
    gSerialDebug->println("-------------------------------");
    gSerialDebug->println("-----------MeijWorks-----------");
    gSerialDebug->println("-------------------------------");
    gSerialDebug->println("  Ploughcontrol ISOBUS version 0.1");
    gSerialDebug->println("(c) 2011 - 2026 by J.A. Woltjer");
    gSerialDebug->println("-------------------------------");

    gTractor->PrintCalibrationData();
    gGuidance->PrintCalibrationData();
    gImplement->PrintCalibrationData();

    // Write message to screen
    // Messages in LanguagePlough.hpp
    gLcd->WriteBuffer(L2_MEIJWORKS, 0);
    gLcd->WriteBuffer(L2_DEVICE, 1);
    gLcd->WriteBuffer(L2_COPYRIGHT, 2);
    gLcd->WriteBuffer(L2_AUTHOR, 3);

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
    // Pumps CAN I/O; guidance/speed/XTE/AISO data arrives asynchronously via
    // IsobusGuidanceChannel's own registered PGN callbacks.
    gGuidanceChannel->Update();
    gVtInterface->Update();

    // Update interface
    gInterface->Update();

    // Both buttons held -> enter the calibration wizard (blocking until it
    // finishes/is cancelled). See CalibrationPlough.hpp for why this trigger
    // lives here instead of inside InterfacePlough itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}
