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

#include "calibration/CalibrationPlough.hpp"
#include "guidance/GuidanceSource.hpp"
#include "implement/ImplementPlough.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlough.hpp"
#include "config/LanguagePlough.hpp"
#include "VehicleTractor.hpp"

#ifdef ISOBUS
#include "isobus/IsobusDebugMenu.hpp"
#include "isobus/IsobusGuidanceChannel.hpp"
#include "isobus/IsobusVtInterface.hpp"
#include "isobus/IsobusTcInterface.hpp"
#else
#include "serial/SerialGuidanceChannel.hpp"
#endif

// Serial ports
Stream* gSerialDebug = &Serial;
#ifndef ISOBUS
// Only the serial guidance path needs a GPS UART -- the ISOBUS build
// acquires guidance data over CAN instead (IsobusGuidanceChannel/AgIsoStack).
HardwareSerial* gSerialGps = &Serial1;
#endif

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*   gLcd;
triton::GuidanceSource*    gGuidance;
triton::VehicleTractor*    gTractor;
triton::ImplementPlough*   gImplement;
triton::InterfacePlough*   gInterface;
triton::CalibrationPlough* gCalibration;
#ifdef ISOBUS
triton::IsobusGuidanceChannel* gGuidanceChannel;
triton::IsobusVtInterface*     gVtInterface;
triton::IsobusTcInterface*     gTcInterface;
triton::IsobusDebugMenu*       gDebugMenu;
#else
triton::SerialGuidanceChannel* gGuidanceChannel;
#endif

// -------------
// Setup routine
// -------------
void setup() {
    // Delay to settle voltages and let LCD startup
    delay(3000);

    // Setup serial ports
    Serial.begin(SERIALDATARATE);  // Serial for computer native usb

    // Print message to computer
    gSerialDebug->println("--------------------------------");
    gSerialDebug->println("-----------MeijWorks------------");
    gSerialDebug->println("--------------------------------");
    gSerialDebug->println("Ploughcontrol ISOBUS version 0.1"); 
    gSerialDebug->println("(c) 2011 - 2026 by J.A. Woltjer ");
    gSerialDebug->println("--------------------------------");

    // Setup I2C LCD
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
    gLcd->Begin();

    // Write message to screen
    // Messages in LanguagePlough.hpp
    gLcd->WriteBuffer(L2_MEIJWORKS, 0);
    gLcd->WriteBuffer(L2_DEVICE, 1);
    gLcd->WriteBuffer(L2_COPYRIGHT, 2);
    gLcd->WriteBuffer(L2_AUTHOR, 3);

    gLcd->WriteScreen(-1);

    // gGuidance, gTractor and gImplement have no CAN/AgIsoStack dependency, so they're
    // constructed first -- gGuidanceChannel needs both already built (the
    // ISOBUS one feeds gGuidance from its PGN callbacks and calls
    // gImplement->Stop() on AISO).
    gGuidance = new triton::GuidanceSource(gSerialDebug);
    gTractor = new triton::VehicleTractor(gSerialDebug);
    gImplement = new triton::ImplementPlough(gSerialDebug, gGuidance);

    gGuidance->PrintCalibrationData();
    gTractor->PrintCalibrationData();
    gImplement->PrintCalibrationData();
    
    // Initialise interfaces
    gInterface = new triton::InterfacePlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance);
    gCalibration = new triton::CalibrationPlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance, gInterface);

    gGuidance->PrintCalibrationData();
    gImplement->PrintCalibrationData();

#ifdef ISOBUS
    // Board-specific CAN wiring: besturing 0.1's CAN transceiver is bodge-wired
    // to FLEXCAN3 (Teensy pins 31 TX / 30 RX), not the FlexCAN1 default (22/23)
    // -- see MeijWorks Hardware/Triton/MeijWorks besturing 0.1/Design documents/
    // teensy41-application-note.md. Constructed here, next to gSerialGps below,
    // so a future board revision only needs this one line changed.
    auto gCanPlugin = std::make_shared<isobus::FlexCANT4Plugin>(2);

    // CAN/ISOBUS bring-up: CAN hardware plugin, NAME + address claim (blocks
    // until claimed), PGN callback registration, initial PGN requests.
    gGuidanceChannel = new triton::IsobusGuidanceChannel(gSerialDebug, gCanPlugin, gGuidance, gImplement);
    gGuidanceChannel->Begin();

    gVtInterface = new triton::IsobusVtInterface(gSerialDebug, gImplement, gGuidance, gGuidanceChannel->GetControlFunction());
    gVtInterface->Begin();

    // Constructed after gVtInterface->Begin() so the VT's partner exists --
    // the TC client uses it for language/unit data on TC servers older than
    // version 4. See IsobusTcInterface's constructor comment.
    gTcInterface = new triton::IsobusTcInterface(gSerialDebug, gImplement, gGuidance, gGuidanceChannel->GetControlFunction(),
                                                  gVtInterface->GetPartner());
    gTcInterface->Begin();

    gDebugMenu = new triton::IsobusDebugMenu(gSerialDebug, gGuidanceChannel, gGuidance, gTcInterface, gVtInterface);
    gDebugMenu->Begin();
#else
    // 4800 baud is the common NMEA default. No baudrate calibration/UI
    // exists in this project today (CalibrationPlough only exposes RTK
    // quality, not GPS baud rate), so this is a fixed value, not tracked
    // or persisted anywhere -- add a real calibration step later if a
    // variable-baud GPS receiver ever needs it.
    gSerialGps->begin(4800);
    gGuidanceChannel = new triton::SerialGuidanceChannel(gSerialDebug, gSerialGps, gGuidance);
#endif

    // Delay for splashscreen
    delay(2000);

    // Write initial interface to screen
    gInterface->UpdateScreen(1);
}

// ---------
// Main loop
// ---------
void loop() {
    // Pumps guidance data acquisition -- CAN I/O + PGN callbacks (ISOBUS) or
    // the UART sentence dispatcher (serial), feeding gGuidance either way.
    gGuidanceChannel->Update();
#ifdef ISOBUS
    gVtInterface->Update();
    gTcInterface->Update();
    gDebugMenu->Update();

    // Update interface -- VT Wider/Narrower soft-key presses OR straight
    // into InterfacePlough's existing button arbitration, see
    // IsobusVtInterface.hpp/InterfacePlough.cpp for the full rationale.
    gInterface->Update(gVtInterface->ConsumeWiderPress(), gVtInterface->ConsumeNarrowerPress());
#else
    // Update interface
    gInterface->Update();
#endif

    // Both buttons held -> enter the calibration wizard (blocking until it
    // finishes/is cancelled). See CalibrationPlough.hpp for why this trigger
    // lives here instead of inside InterfacePlough itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}
