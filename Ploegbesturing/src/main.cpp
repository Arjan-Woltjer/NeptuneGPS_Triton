/*
  Ploegbesturing - program using gps-data to control the offset of a plough
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
#include <ACAN_T4.h>

#include "CalibrationPlough.hpp"
#include "ImplementPlough.hpp"
#include "InterfaceGps.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlough.hpp"
#include "LanguagePlough.hpp"
#include "VehicleGps.hpp"
#include "VehicleTractor.hpp"

// Serial ports
HardwareSerial* gSerialDebug = &Serial2;
#if defined(__AVR_ATmega32U4__) || defined __MK66FX1M0__ || defined __MK64FX512__ || defined __MK20DX256__ || defined __IMXRT1062__
HardwareSerial* gSerialGps = &Serial1;
#else
HardwareSerial* gSerialGps = &Serial;
#endif

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*  gLcd;
triton::VehicleTractor*   gTractor;
triton::ImplementPlough*  gImplement;
triton::InterfacePlough*  gInterface;
triton::CalibrationPlough* gCalibration;
triton::VehicleGps*       gGps;
triton::InterfaceGps*     gInterfaceGps;

static void GCanSetup();
static void GHandleCanMessage(const CANMessage& inMessage);

// -------------
// Setup routine
// -------------
void setup() {
    // Delay to settle voltages and let LCD startup
    delay(3000);

    // Setup serial ports
    gSerialDebug->begin(SERIALDATARATE);  // Serial for computer native usb

    GCanSetup();

    // Initialise objects and interfaces
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
    gLcd->Begin();

    gTractor = new triton::VehicleTractor(gSerialDebug);
    gGps = new triton::VehicleGps(gSerialDebug, gSerialGps);
    // Constructor also runs a real (blocking) GPS baud-rate autodetect probe;
    // CheckGps() is never called again after setup().
    gInterfaceGps = new triton::InterfaceGps(gLcd, gGps);

    gImplement = new triton::ImplementPlough(gSerialDebug, gGps);
    gInterface = new triton::InterfacePlough(gSerialDebug, gLcd, gImplement, gTractor, gGps);
    gCalibration = new triton::CalibrationPlough(gSerialDebug, gLcd, gImplement, gTractor, gGps, gInterface);

    // Print message to computer
    gSerialDebug->println("-------------------------------");
    gSerialDebug->println("-----------MeijWorks-----------");
    gSerialDebug->println("-------------------------------");
    gSerialDebug->println("  Ploughcontrol version 1.4    ");
    gSerialDebug->println("(c) 2011 - 2026 by J.A. Woltjer");
    gSerialDebug->println("-------------------------------");

    gTractor->PrintCalibrationData();
    gGps->PrintCalibrationData();
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
    // Update CAN messages
    ACAN_T4::can1.dispatchReceivedMessage();

    // Update interface
    gInterface->Update();

    // Both buttons held -> enter the calibration wizard (blocking until it
    // finishes/is cancelled). See CalibrationPlough.hpp for why this trigger
    // lives here instead of inside InterfacePlough itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}

// ------------------------
// Setup CAN-bus for ISOBUS
// ------------------------
// Ported from the legacy ACAN library (Teensy 3.x's FlexCAN0 only) to ACAN_T4
// (Teensy 4.x's FlexCAN peripherals) -- can1/CAN1 is the module wired on the
// production board. ACAN_T4 keeps the same ACANPrimaryFilter/
// dispatchReceivedMessage() shape as the original ACAN library, so this is
// otherwise a close port, not a rewrite.
static void GCanSetup() {
    ACAN_T4_Settings settings(250UL * 1000UL);  // CAN bit rate 250 kbit/s

    const ACANPrimaryFilter primaryFilters[]{
        ACANPrimaryFilter(kData, kExtended, 0x00FFFFFF, 0x00FEF31C, GHandleCanMessage), // Position
        ACANPrimaryFilter(kData, kExtended, 0x00FFFFFF, 0x00FEE81C, GHandleCanMessage), // Direction, speed
        ACANPrimaryFilter(kData, kExtended, 0x0CFFFF2A, GHandleCanMessage),             // JD XTE
        ACANPrimaryFilter(kData, kExtended, 0x1CEBACAA, GHandleCanMessage)              // Trimble CNH XTE
    };

    const uint32_t errorCode = ACAN_T4::can1.begin(settings, primaryFilters, 4);

    if (errorCode == 0) {
        gSerialDebug->print("Bit Rate prescaler: ");
        gSerialDebug->println(settings.mBitRatePrescaler);
        gSerialDebug->print("Propagation Segment: ");
        gSerialDebug->println(settings.mPropagationSegment);
        gSerialDebug->print("Phase segment 1: ");
        gSerialDebug->println(settings.mPhaseSegment1);
        gSerialDebug->print("Phase segment 2: ");
        gSerialDebug->println(settings.mPhaseSegment2);
        gSerialDebug->print("RJW:");
        gSerialDebug->println(settings.mRJW);
        gSerialDebug->print("Triple Sampling: ");
        gSerialDebug->println(settings.mTripleSampling ? "yes" : "no");
        gSerialDebug->print("Actual bit rate: ");
        gSerialDebug->print(settings.actualBitRate());
        gSerialDebug->println(" bit/s");
        gSerialDebug->print("Exact bit rate ? ");
        gSerialDebug->println(settings.exactBitRate() ? "yes" : "no");
        gSerialDebug->print("Sample point: ");
        gSerialDebug->print(settings.samplePointFromBitStart());
        gSerialDebug->println("%");
    }
    else {
        gSerialDebug->print("Error can1: 0x");
        gSerialDebug->println(errorCode, HEX);
    }
}

static void GHandleCanMessage(const CANMessage& inMessage) {
    gGps->Update(inMessage.id, inMessage.data, inMessage.len);
}
