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
// its implement and, from #291 on, its VT working set.
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include "calibration/CalibrationScraper.hpp"
#include "GuidanceSource.hpp"
#include "implement/ImplementScraper.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceScraper.hpp"
#include "config/LanguageScraper.hpp"
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
#include "isobus/ScraperName.hpp"
#else
// Serial builds: what Kilverbakbesturing does -- the receiver on the UART
// (baud rate autodetect at boot) plus the guidance messages on a plain,
// listen-only CAN bus through ACAN_T4. No address claim, no VT.
#include <ACAN_T4.h>
#include "CanFrameGuidanceChannel.hpp"
#include "InterfaceGuidance.hpp"
#include "PlainCanGuidance.hpp"
#include "SerialGuidanceChannel.hpp"
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
#ifndef ISOBUS
// The receiver; the ISOBUS build has no UART receiver yet (#290 adds it).
HardwareSerial* gSerialGps = &Serial1;
#endif

// I2C
TwoWire* gLcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*    gLcd;
triton::GuidanceSource*     gGuidance;
triton::VehicleSensors*     gTractor;
triton::ImplementScraper*   gImplement;
triton::InterfaceScraper*   gInterface;
triton::CalibrationScraper* gCalibration;
#ifdef ISOBUS
triton::IsobusGuidanceChannel* gGuidanceChannel;
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
// The receiver on the UART and the CAN frames ACAN_T4 hands GHandleCanMessage()
// both commit into gGuidance, as in Kilverbakbesturing.
triton::SerialGuidanceChannel*   gGuidanceChannel;
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

    // The splash screen's two seconds here, before the ISOBUS stack, the CAN
    // bus or the receiver port start, and not at the end of setup(): once
    // they run, loop() has to service them. Waiting after the stack had
    // started left the CAN receive queue to overflow at every boot on a
    // tractor's bus (Ploegbesturing Isobus, session 28).
    delay(2000);

    // gGuidance, gTractor and gImplement have no CAN dependency, so they are
    // built first: the ISOBUS guidance channel feeds gGuidance from its PGN
    // callbacks and stops gImplement on AISO.
    gGuidance = new triton::GuidanceSource();
    gTractor = new triton::VehicleSensors(gSerialDebug);
    gImplement = new triton::ImplementScraper(gGuidance);
    gInterface = new triton::InterfaceScraper(gLcd, gImplement, gTractor, gGuidance);
    // Also loads the stored receiver rate index for the detect below (the
    // data model itself never touches EEPROM, #78).
    gCalibration = new triton::CalibrationScraper(gLcd, gImplement, gGuidance, gInterface);

    gTractor->PrintCalibrationData();
    gCalibration->PrintCalibrationData(gSerialDebug);
    gImplement->PrintCalibrationData();

#ifdef ISOBUS
    // CAN on the board's FlexCAN (ConfigScraper.hpp): FLEXCAN1 on the Teensy
    // 4.0 board, FLEXCAN3 on besturing 0.1.
    constexpr std::uint8_t kIsobusCanChannel = ISOBUS_CAN_CHANNEL;
    auto gCanPlugin = std::make_shared<isobus::FlexCANT4Plugin>(kIsobusCanChannel);
    gFlexCanPlugin = gCanPlugin.get();

    // CAN/ISOBUS bring-up: NAME + address claim (blocks until claimed), PGN
    // callbacks, initial PGN requests. The NAME is the kilverbak's own
    // (ScraperName.hpp); AISO stops the valves.
    gGuidanceChannel = new triton::IsobusGuidanceChannel(gSerialDebug, gCanPlugin, gGuidance, [] { gImplement->Stop(); },
                                                         triton::BuildScraperName(), kIsobusCanChannel);
    gGuidanceChannel->Begin();

    // The shared status menu. No VT and no machine sections yet: the VT
    // working set arrives with #291.
    gDebugMenu = new triton::IsobusDebugMenu(gSerialDebug, gGuidanceChannel, gGuidance);
    gDebugMenu->Begin();
    gDebugMenu->SetCanReceiveQueueStatsSource(GReadCanReceiveQueueStats);
#else
    gGuidanceChannel = new triton::SerialGuidanceChannel(gSerialDebug, gSerialGps, gGuidance);
    gCanChannel = new triton::CanFrameGuidanceChannel(gGuidance);

    // Blocking receiver baudrate autodetect on the LCD, as in
    // Kilverbakbesturing. The scraper reads position and speed only, so a
    // receiver without XTE sentences is found promptly. The found index is
    // persisted only when it differs from the stored one, so a stable
    // installation never writes.
    {
        triton::InterfaceGuidance detect(gLcd, gGuidanceChannel, gGuidance);
        const byte stored = gCalibration->GetGpsBaudIndex();
        const byte found  = detect.DetectBaudrate(stored, triton::InterfaceGuidance::kGga | triton::InterfaceGuidance::kVtg);
        if (found != triton::InterfaceGuidance::kNotFound && found != stored) {
            gCalibration->SetGpsBaudIndex(found);
            gCalibration->CommitGuidanceCalibration();
        }
    }

    // The CAN bus last, right before loop() starts to read it: nothing
    // services its receive buffer during setup().
    GCanSetup();
#endif

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
#else
    // Plain CAN: the received frames, through GHandleCanMessage().
    GPlainCan.dispatchReceivedMessage();
#endif

    // Guidance data acquisition -- CAN I/O and PGN callbacks (ISOBUS) or the
    // UART sentence dispatcher (serial), feeding gGuidance either way.
    gGuidanceChannel->Update();

#ifdef ISOBUS
    gDebugMenu->Update();
#endif

    // The calibration wizard is a step runner (#289): while it is open it
    // takes the buttons and the LCD instead of InterfaceScraper, one pass per
    // loop(), and everything above keeps running. See CalibrationScraper.hpp.
    if (gCalibration->IsActive()) {
        gCalibration->Step();
        return;
    }

    // Update interface
    gInterface->Update();

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
// The same guidance filter set as Kilverbakbesturing (full GPS: position and
// altitude for the two-reference-point leveling calculation), on the board's
// own CAN pins.
static void GCanSetup() {
    triton::PlainCanBegin<ACAN_T4_Settings>(GPlainCan, gSerialDebug, GHandleCanMessage);
}

static void GHandleCanMessage(const CANMessage& inMessage) {
    gCanChannel->Update(inMessage.id, inMessage.data, inMessage.len);
}
#endif
