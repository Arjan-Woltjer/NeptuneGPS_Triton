/*
  Kilverbakbesturing Isobus - ISOBUS scraper (leveling bucket) controller
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
// The Kilverbakbesturing on the Ploegbesturing Isobus layout
// (NeptuneGPS_Triton#286, #288). The ISOBUS layer is the shared one in
// MeijWorks Libs/IsobusImplement; what is the kilverbak's own is its NAME,
// its implement and its VT working set (#291).
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include "calibration/CalibrationScraper.hpp"
#include "GuidanceSource.hpp"
#include "implement/ImplementScraper.hpp"
#include "InterfaceGuidance.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceScraper.hpp"
#include "config/LanguageScraper.hpp"
#include "ScraperGates.hpp"
#include "SerialGuidanceChannel.hpp"
#include "VehicleSensors.hpp"

#ifdef ISOBUS
// See Ploegbesturing Isobus' main.cpp: the FlexCAN plugin is included by
// name, with the min()/max() macros parked around it.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/hardware_integration/flex_can_t4_plugin.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")
#include "IsobusDebugMenu.hpp"
#include "IsobusGuidanceChannel.hpp"
#include "IsobusVtInterface.hpp"
#include "isobus/ScraperName.hpp"
#include "isobus/ScraperVtWorkingSet.hpp"
#else
// Serial builds: what Kilverbakbesturing does -- the receiver on the UART
// (baud rate autodetect at boot) plus the guidance messages on a plain,
// listen-only CAN bus through ACAN_T4. No address claim, no VT.
#include <ACAN_T4.h>
#include "CanFrameGuidanceChannel.hpp"
#include "PlainCanGuidance.hpp"
#ifndef ISOBUS_CAN_CHANNEL
// An undefined macro reads as 0 in #if: every board would silently get FLEXCAN1.
#error "ISOBUS_CAN_CHANNEL undefined: config/ConfigScraper.hpp must be included before this point"
#endif
#if ISOBUS_CAN_CHANNEL == 0
#define GPlainCan ACAN_T4::can1   // FLEXCAN1: pins 22 TX / 23 RX
#else
#define GPlainCan ACAN_T4::can3   // FLEXCAN3: pins 31 TX / 30 RX
#endif
#endif

// Serial ports. Debug on the USB port, as on the plough board: the debug
// menu reads its keys there. (Kilverbakbesturing printed on Serial2.)
Stream* gSerialDebug = &Serial;
// The receiver on the kilverbak, on every build (#290).
HardwareSerial* gSerialGps = &Serial1;

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*    gLcd;
triton::VehicleSensors*     gTractor;
triton::ImplementScraper*   gImplement;
triton::InterfaceScraper*   gInterface;
triton::CalibrationScraper* gCalibration;

// The guidance data (NeptuneGPS_Triton#290). Each source feeds its own
// GuidanceSource; every loop() pass copies the one the control may use into
// gGuidance, which is all the rest reads: the UART while its GGA is fresh,
// otherwise the CAN bus (owner: the UART wins). The CAN source takes no PGN
// 65256 altitude (0.125 m steps): on the ISOBUS build its height comes from
// PGN 129029, on the serial builds the bus gives no height at all.
triton::GuidanceSource*        gGuidance;
triton::GuidanceSource*        gUartGuidance;
triton::GuidanceSource*        gCanGuidance;
triton::SerialGuidanceChannel* gUartChannel;
#ifdef ISOBUS
triton::IsobusGuidanceChannel* gGuidanceChannel;
triton::ScraperVtWorkingSet*   gVtWorkingSet;
triton::IsobusVtInterface*     gVtInterface;
triton::IsobusDebugMenu*       gDebugMenu;
// For the dump's receive-queue line (#248). The hardware interface keeps the
// plugin alive.
static isobus::FlexCANT4Plugin* gFlexCanPlugin = nullptr;
static triton::CanReceiveQueueStats GReadCanReceiveQueueStats() {
    triton::CanReceiveQueueStats stats;
    if (gFlexCanPlugin != nullptr) {
        stats.available = true;
        stats.highWater = gFlexCanPlugin->get_receive_queue_high_water();
        stats.size      = isobus::FlexCANT4Plugin::RECEIVE_QUEUE_SIZE;
        stats.overflows = gFlexCanPlugin->get_receive_queue_overflow_count();
    }
    return stats;
}
#else
// The CAN frames ACAN_T4 hands GHandleCanMessage().
triton::CanFrameGuidanceChannel* gCanChannel;
static void GCanSetup();
static void GHandleCanMessage(const CANMessage& inMessage);
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
    gSerialDebug->println("   Triton KB-ISO version 0.01   ");
    gSerialDebug->println("(c) 2011 - 2026 by J.A. Woltjer ");
    gSerialDebug->println("--------------------------------");

    // Setup I2C LCD
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
    gLcd->Begin();

    // Write message to screen
    // Messages in LanguageScraper.hpp
    gLcd->WriteBuffer(L_MEIJWORKS, 0);
    gLcd->WriteBuffer(L_DEVICE, 1);
    gLcd->WriteBuffer(L_COPYRIGHT, 2);
    gLcd->WriteBuffer(L_AUTHOR, 3);
    gLcd->WriteScreen(-1);

    // The guidance data and the implement have no CAN dependency, so they
    // are built first: the ISOBUS guidance channel feeds gCanGuidance from its
    // PGN callbacks and stops gImplement on AISO.
    gGuidance     = new triton::GuidanceSource();
    gUartGuidance = new triton::GuidanceSource();
    gCanGuidance  = new triton::GuidanceSource();
    gCanGuidance->SetUseCoarseAltitude(false);
    gTractor = new triton::VehicleSensors(gSerialDebug);
    gImplement = new triton::ImplementScraper(gGuidance);
    gInterface = new triton::InterfaceScraper(gLcd, gImplement, gTractor, gGuidance);
    // Also loads the stored receiver rate index and the RTK quality, which it
    // sets on gGuidance (the data model itself never touches EEPROM, #78).
    gCalibration = new triton::CalibrationScraper(gLcd, gImplement, gGuidance, gInterface);

    gTractor->PrintCalibrationData();
    gCalibration->PrintCalibrationData(gSerialDebug);
    gImplement->PrintCalibrationData();

    gUartChannel = new triton::SerialGuidanceChannel(gSerialDebug, gSerialGps, gUartGuidance);

#ifdef ISOBUS
    // The receiver port at the stored rate, without the blocking autodetect:
    // with no receiver on the kilverbak (the antenna on the tractor, height
    // from PGN 129029) the detect would hold the boot for up to a minute
    // before the address claim. A serial build finds and stores the rate.
    gSerialGps->begin(triton::InterfaceGuidance::BaudFromIndex(gCalibration->GetGpsBaudIndex()));

    // CAN on the board's FlexCAN (ConfigScraper.hpp): FLEXCAN1 on the Teensy
    // 4.0 board, FLEXCAN3 on besturing 0.1.
    constexpr std::uint8_t kIsobusCanChannel = ISOBUS_CAN_CHANNEL;
    auto gCanPlugin = std::make_shared<isobus::FlexCANT4Plugin>(kIsobusCanChannel);
    gFlexCanPlugin = gCanPlugin.get();

    // CAN/ISOBUS bring-up: NAME + address claim (blocks until claimed), PGN
    // callbacks, initial PGN requests. The NAME is the kilverbak's own
    // (ScraperName.hpp); AISO stops the valves.
    gGuidanceChannel = new triton::IsobusGuidanceChannel(gSerialDebug, gCanPlugin, gCanGuidance, [] { gImplement->Stop(); },
                                                         triton::BuildScraperName(), kIsobusCanChannel);
    gGuidanceChannel->Begin();

    // The kilverbak's screen and keys on the VT (#291); the shared VT
    // interface owns the connection and calls into it.
    gVtWorkingSet = new triton::ScraperVtWorkingSet(gSerialDebug, gImplement, gGuidance, gInterface, gCalibration);
    gVtInterface = new triton::IsobusVtInterface(gSerialDebug, gVtWorkingSet, gGuidanceChannel->GetControlFunction());
    gVtInterface->Begin();

    // The shared status menu, on the data the control reads, with the VT's
    // section. No machine sections: the kilverbak has no TC or lightbar.
    gDebugMenu = new triton::IsobusDebugMenu(gSerialDebug, gGuidanceChannel, gGuidance, gVtInterface);
    gDebugMenu->Begin();
    gDebugMenu->SetCanReceiveQueueStatsSource(GReadCanReceiveQueueStats);
#else
    gCanChannel = new triton::CanFrameGuidanceChannel(gCanGuidance);
    GCanSetup();

    // Blocking receiver baudrate autodetect on the LCD, as in
    // Kilverbakbesturing. The scraper reads position and speed only, so a
    // receiver without XTE sentences is found promptly. The found index is
    // persisted only when it differs from the stored one, so a stable
    // installation never writes.
    {
        triton::InterfaceGuidance detect(gLcd, gUartChannel, gUartGuidance);
        const byte stored = gCalibration->GetGpsBaudIndex();
        const byte found  = detect.DetectBaudrate(stored, triton::InterfaceGuidance::kGga | triton::InterfaceGuidance::kVtg);
        if (found != triton::InterfaceGuidance::kNotFound && found != stored) {
            gCalibration->SetGpsBaudIndex(found);
            gCalibration->CommitGuidanceCalibration();
        }
    }
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
#ifdef ISOBUS
    // How long each pass takes (#248): the dump shows the longest.
    static unsigned long lastPassUs = micros();
    const unsigned long passStartUs = micros();
    gDebugMenu->NoteLoopPass(passStartUs - lastPassUs);
    lastPassUs = passStartUs;

    // CAN I/O and the PGN callbacks, feeding gCanGuidance.
    gGuidanceChannel->Update();
#else
    // Plain CAN: the received frames, through GHandleCanMessage().
    GPlainCan.dispatchReceivedMessage();
#endif

    // The receiver on the UART, feeding gUartGuidance.
    gUartChannel->Update();

    // The source the control reads this pass (#290): the UART while its GGA
    // is fresh, otherwise the CAN bus.
    triton::SelectScraperGuidance(*gGuidance, *gUartGuidance, *gCanGuidance, millis());

#ifdef ISOBUS
    // The VT: the connection, the working screen's values, its keys and the
    // reference page (#291).
    gVtInterface->Update();
    gDebugMenu->Update();
#endif

    // The calibration wizard is a step runner (#289): while it is open it
    // takes the buttons and the LCD instead of InterfaceScraper, one pass per
    // loop(), and everything above keeps running. See CalibrationScraper.hpp.
    if (gCalibration->IsActive()) {
        gCalibration->Step();
#ifdef ISOBUS
        // The wizard may have changed the preferred VT (#255).
        if (!gCalibration->IsActive()) gVtInterface->LoadPreferredVt();
#endif
        return;
    }

#ifdef ISOBUS
    // The VT's reference page is open (#291): the valves stop and the cab
    // buttons do nothing until OPSLAAN or ANNUL., as during the cab wizard.
    if (gVtWorkingSet->IsCapturingReference()) {
        gImplement->Stop();
        return;
    }

    // Update interface -- the VT's + and - (held levels) join the cab
    // buttons, see ScraperVtWorkingSet.hpp.
    gInterface->Update(gVtWorkingSet->IsPlusHeld(), gVtWorkingSet->IsMinusHeld());
#else
    // Update interface
    gInterface->Update();
#endif

    // Both buttons held -> open the calibration wizard. See
    // CalibrationScraper.hpp for why this trigger lives here instead of
    // inside InterfaceScraper itself.
    if (gInterface->GetButtons() == 2) {
        gCalibration->Start();
    }
}

#ifndef ISOBUS
// ------------------------
// Plain CAN, listen only
// ------------------------
// The same guidance filter set as Kilverbakbesturing, on the board's own CAN
// pins. It feeds gCanGuidance, which the control reads only while the UART is
// quiet; it carries no height (PGN 65256's coarse altitude is not taken).
static void GCanSetup() {
    triton::PlainCanBegin<ACAN_T4_Settings>(GPlainCan, gSerialDebug, GHandleCanMessage);
}

static void GHandleCanMessage(const CANMessage& inMessage) {
    gCanChannel->Update(inMessage.id, inMessage.data, inMessage.len);
}
#endif
