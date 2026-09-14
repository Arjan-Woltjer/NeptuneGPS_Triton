/*
  Pootmachinebesturing - program using gps-data to control the offset of a planter
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

#include "CalibrationPlanter.hpp"
#include "CanFrameGuidanceChannel.hpp"
#include "GuidanceSource.hpp"
#include "ImplementPlanter.hpp"
#include "InterfaceGuidance.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlanter.hpp"
#include "LanguagePlanter.hpp"
#include "SerialGuidanceChannel.hpp"
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
triton::InterfaceI2CLCD*   gLcd;
triton::VehicleTractor*    gTractor;
triton::ImplementPlanter*  gImplement;
triton::InterfacePlanter*  gInterface;
triton::CalibrationPlanter* gCalibration;
// Guidance data model and the two channels that feed it (shared MeijWorks
// Libs/VehicleGuidance, NeptuneGPS_Triton#76/#80): the receiver on the UART
// and the raw frames ACAN_T4 hands GHandleCanMessage(). Everything else
// only reads gGuidance.
triton::GuidanceSource*          gGuidance;
triton::SerialGuidanceChannel*   gGuidanceChannel;
triton::CanFrameGuidanceChannel* gCanChannel;

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
    gGuidance = new triton::GuidanceSource();
    gGuidanceChannel = new triton::SerialGuidanceChannel(gSerialDebug, gSerialGps, gGuidance);
    gCanChannel = new triton::CanFrameGuidanceChannel(gGuidance);

    gImplement = new triton::ImplementPlanter(gSerialDebug, gTractor, gGuidance);
    gInterface = new triton::InterfacePlanter(gSerialDebug, gLcd, gImplement, gTractor, gGuidance);
    // Also loads the stored receiver rate index for the detect below (the
    // data model itself never touches EEPROM, #78).
    gCalibration = new triton::CalibrationPlanter(gSerialDebug, gLcd, gImplement, gTractor, gGuidance, gInterface);

    // Blocking receiver baudrate autodetect on the LCD, as InterfaceGps did
    // from its constructor; the found index is persisted only when it
    // differs from the stored one, so a stable installation never writes.
    {
        triton::InterfaceGuidance detect(gLcd, gGuidanceChannel, gGuidance);
        const byte stored = gCalibration->GetGpsBaudIndex();
        const byte found  = detect.DetectBaudrate(stored, triton::InterfaceGuidance::kAll);
        if (found != triton::InterfaceGuidance::kNotFound && found != stored) {
            gCalibration->SetGpsBaudIndex(found);
            gCalibration->CommitGuidanceCalibration();
        }
    }

    // Print message to computer
    gSerialDebug->println(S_DIVIDE);
    gSerialDebug->print(S_MEIJWORKS);
    gSerialDebug->print(" ");
    gSerialDebug->println(S_DEVICE);
    gSerialDebug->println(S_COPYRIGHT);
    gSerialDebug->println(S_DIVIDE);

    gTractor->PrintCalibrationData();
    gCalibration->PrintCalibrationData();
    gImplement->PrintCalibrationData();

    // Write message to screen
    // Messages in LanguagePlanter.hpp
    gLcd->WriteBuffer(L3_MEIJWORKS, 0);
    gLcd->WriteBuffer(L3_DEVICE, 1);
    gLcd->WriteBuffer(L3_COPYRIGHT, 2);
    gLcd->WriteBuffer(L3_AUTHOR, 3);

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
    // Pump both guidance channels: CAN frames through GHandleCanMessage(),
    // and the receiver on the UART. Both commit into gGuidance.
    ACAN_T4::can1.dispatchReceivedMessage();
    gGuidanceChannel->Update();

    // Update interface
    gInterface->Update();

    // Both buttons held -> enter the calibration wizard (blocking until it
    // finishes/is cancelled). See CalibrationPlanter.hpp for why this trigger
    // lives here instead of inside InterfacePlanter itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Calibrate();
    }
}

// ------------------------
// Setup CAN-bus for ISOBUS
// ------------------------
// Same ACAN_T4 (Teensy 4.x FlexCAN, can1/CAN1) port Ploegbesturing already
// went through.
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
    gCanChannel->Update(inMessage.id, inMessage.data, inMessage.len);
}
