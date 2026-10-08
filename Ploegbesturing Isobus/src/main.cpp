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
#include "GuidanceSource.hpp"
#include "implement/ImplementPlough.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlough.hpp"
#include "config/LanguagePlough.hpp"
#include "VehicleTractor.hpp"

#ifdef ISOBUS
// The FlexCAN plugin used to arrive through the AgIsoStack.hpp umbrella the
// Arduino packaging generated; AgIsoStack-plus-plus has no umbrella (#189).
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/hardware_integration/flex_can_t4_plugin.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")
#include "isobus/IsobusDebugMenu.hpp"
#include "isobus/IsobusGuidanceChannel.hpp"
#include "isobus/IsobusLightbarChannel.hpp"
#include "isobus/IsobusVtInterface.hpp"
#include "isobus/IsobusTcInterface.hpp"
#else
#include "SerialGuidanceChannel.hpp"
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
triton::IsobusLightbarChannel* gLightbarChannel;
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
    gGuidance = new triton::GuidanceSource();
    gTractor = new triton::VehicleTractor(gSerialDebug);
    gImplement = new triton::ImplementPlough(gSerialDebug, gGuidance);

    gTractor->PrintCalibrationData();
    gImplement->PrintCalibrationData();

    // Initialise interfaces. CalibrationPlough also loads the stored RTK
    // quality into gGuidance (GuidanceSource itself never touches EEPROM --
    // the shared data model leaves storage to the project, #78).
    gInterface = new triton::InterfacePlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance);
    gCalibration = new triton::CalibrationPlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance, gInterface);

    gCalibration->PrintCalibrationData();
    gImplement->PrintCalibrationData();

#ifdef ISOBUS
    // Board-specific CAN wiring: besturing 0.1's CAN transceiver is bodge-wired
    // to FLEXCAN3 (Teensy pins 31 TX / 30 RX), not the FlexCAN1 default (22/23)
    // -- see MeijWorks Hardware/Triton/MeijWorks besturing 0.1/Design documents/
    // teensy41-application-note.md. Constructed here, next to gSerialGps below,
    // so a future board revision only needs this one line changed.
    constexpr std::uint8_t kIsobusCanChannel = 2;   // FLEXCAN3
    auto gCanPlugin = std::make_shared<isobus::FlexCANT4Plugin>(kIsobusCanChannel);

    // CAN/ISOBUS bring-up: CAN hardware plugin, NAME + address claim (blocks
    // until claimed), PGN callback registration, initial PGN requests.
    // The channel again, so the dump can read that controller's error state (#149).
    gGuidanceChannel = new triton::IsobusGuidanceChannel(gSerialDebug, gCanPlugin, gGuidance, gImplement, kIsobusCanChannel);
    gGuidanceChannel->Begin();

    // A second control function that presents as an Ag Leader L160 lightbar:
    // an InCommand only broadcasts its cross-track error (PGN 65462) once a
    // lightbar has identified itself (#42, card log 42).
    gLightbarChannel = new triton::IsobusLightbarChannel(gSerialDebug, gGuidance, kIsobusCanChannel);
    gLightbarChannel->Begin();
    gVtInterface = new triton::IsobusVtInterface(gSerialDebug, gImplement, gGuidance, gGuidanceChannel->GetControlFunction());
    gVtInterface->Begin();

    // Constructed after gVtInterface->Begin() so the VT's partner exists --
    // the TC client uses it for language/unit data on TC servers older than
    // version 4. See IsobusTcInterface's constructor comment.
    gTcInterface = new triton::IsobusTcInterface(gSerialDebug, gImplement, gGuidance, gGuidanceChannel->GetControlFunction(),
                                                  gVtInterface->GetPartner());
    gTcInterface->Begin();

    gDebugMenu = new triton::IsobusDebugMenu(gSerialDebug, gGuidanceChannel, gGuidance, gTcInterface, gVtInterface, gLightbarChannel);
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
    gLightbarChannel->Update();
    gVtInterface->Update();
    gTcInterface->Update();
    gDebugMenu->Update();

    // Update interface -- VT Wider/Narrower/Calibrate soft-key presses feed
    // InterfacePlough's existing button arbitration, see
    // IsobusVtInterface.hpp/InterfacePlough.cpp for the full rationale.
    // Every Consume*() call must happen on every iteration: they are the only
    // thing that clears the pending flags, so short-circuiting one would leave
    // a press latched until the next one arrives.
    gInterface->Update(gVtInterface->ConsumeWiderPress(),
                       gVtInterface->ConsumeNarrowerPress(),
                       gVtInterface->ConsumeCalibratePress());
#else
    // Update interface
    gInterface->Update();
#endif

    // Both buttons held (or the VT's Calibrate soft key) -> enter the
    // calibration wizard (blocking until it finishes/is cancelled). See
    // CalibrationPlough.hpp for why this trigger lives here instead of inside
    // InterfacePlough itself, and GitHub issue #31 for what blocking here
    // costs the ISOBUS stack when the trigger came from the VT.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}
