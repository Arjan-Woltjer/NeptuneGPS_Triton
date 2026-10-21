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

#include "TritonIo.hpp"

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
#if defined(ESP32S3)
#include <isobus/hardware_integration/twai_plugin.hpp>
#else
#include <isobus/hardware_integration/flex_can_t4_plugin.hpp>
#endif
#pragma pop_macro("max")
#pragma pop_macro("min")
#include "isobus/IsobusDebugMenu.hpp"
#include "isobus/IsobusGuidanceChannel.hpp"
#include "isobus/IsobusLightbarChannel.hpp"
#include "isobus/IsobusVtInterface.hpp"
#include "isobus/IsobusTcInterface.hpp"
#else
#include "InterfaceGuidance.hpp"
#include "SerialGuidanceChannel.hpp"
#if defined(CAN_ACAN) || defined(CAN_ACAN_T4)
// Plain CAN, no ISOBUS stack: the bus is read the way ploegbesturing
// v1.20-v1.31 read it -- acceptance filters for the guidance messages, and
// every matching frame handed to CanFrameGuidanceChannel. Listen-only in
// effect: no address claim, no VT, no TC.
//  - Teensy 3.2 ([env:teensy32_can], -D CAN_ACAN): the full ISOBUS build does
//    not fit the 3.2 (341 KB flash / 146 KB RAM against 256 / 64), and
//    AgIsoStack's FlexCAN plugin is Teensy 4 only. pierremolinaro's ACAN on
//    the 3.2's CAN0.
//  - Teensy 4.1 / 4.0 ([env:teensy41_can], [env:teensy40_can], -D
//    CAN_ACAN_T4): ACAN_T4, its Teensy 4 successor, on the FlexCAN the board
//    wires to its transceiver (ISOBUS_CAN_CHANNEL, ConfigPlough.hpp). What
//    the regular Ploegbesturing's build does, on the board's own CAN pins.
#define PLAIN_CAN
#include "CanFrameGuidanceChannel.hpp"
#endif
#ifdef CAN_ACAN
#include <ACAN.h>
#define GPlainCan ACAN::can0
typedef ACANSettings GPlainCanSettings;
#endif
#ifdef CAN_ACAN_T4
#include <ACAN_T4.h>
#ifndef ISOBUS_CAN_CHANNEL
// An undefined macro reads as 0 in #if: every board would silently get FLEXCAN1.
#error "ISOBUS_CAN_CHANNEL undefined: config/ConfigPlough.hpp must be included before this point"
#endif
#if ISOBUS_CAN_CHANNEL == 0
#define GPlainCan ACAN_T4::can1   // FLEXCAN1: pins 22 TX / 23 RX
#elif ISOBUS_CAN_CHANNEL == 1
#define GPlainCan ACAN_T4::can2   // FLEXCAN2: pins 1 TX / 0 RX
#else
#define GPlainCan ACAN_T4::can3   // FLEXCAN3: pins 31 TX / 30 RX
#endif
typedef ACAN_T4_Settings GPlainCanSettings;
#endif
#ifdef PLAIN_CAN
#include "PlainCanGuidance.hpp"   // after the ACAN headers: it uses their filter types
#endif
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
#if !defined(ESP32S3)
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
#endif
#else
triton::SerialGuidanceChannel* gGuidanceChannel;
#ifdef PLAIN_CAN
// The receiver on the UART and the CAN frames ACAN/ACAN_T4 hands GHandleCanMessage()
// both commit into gGuidance, as in the regular Ploegbesturing build.
triton::CanFrameGuidanceChannel* gCanChannel;

static void GCanSetup();
static void GHandleCanMessage(const CANMessage& inMessage);
#endif
#endif

// -------------
// Setup routine
// -------------
void setup() {
    // Delay to settle voltages and let LCD startup
    delay(3000);

#if defined(ESP32S3)
    // Triton01 (ESP32-S3): the EEPROM is a flash-backed emulation that must
    // be opened before the first read (every class below reads its
    // calibration in its constructor) and committed after writes, see loop().
    // UART1 to the RS232 transceiver sits on GPIO-matrix pins, not the core
    // defaults; the I2C pins go in through the LCD constructor below.
    EEPROM.begin(256);
    Serial1.setPins(42, 41);
#endif

    // Setup serial ports
    Serial.begin(SERIALDATARATE);  // Serial for computer native usb

    // Print message to computer. Not on the Arduino Micro: this banner and the
    // calibration dumps below are what pushed micro_serial past its 28 KB of
    // flash (30.9 KB with them, 28.4 KB without), and AVR keeps every string
    // literal in RAM as well. The LCD splash screen still shows on every board.
#ifndef MICRO
    gSerialDebug->println("--------------------------------");
    gSerialDebug->println("-----------MeijWorks------------");
    gSerialDebug->println("--------------------------------");
    gSerialDebug->println("Ploughcontrol ISOBUS version 0.1");
    gSerialDebug->println("(c) 2011 - 2026 by J.A. Woltjer ");
    gSerialDebug->println("--------------------------------");
#endif

    // Setup I2C LCD
#if defined(ESP32S3)
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4, 1, 2);   // SDA GPIO 1, SCL GPIO 2
#else
    gLcd = new triton::InterfaceI2CLCD(gLcdWire, 0x27, 20, 4);
#endif
    gLcd->Begin();

    // Write message to screen
    // Messages in LanguagePlough.hpp
    gLcd->WriteBuffer(L_MEIJWORKS, 0);
    gLcd->WriteBuffer(L_DEVICE, 1);
    gLcd->WriteBuffer(L_COPYRIGHT, 2);
    gLcd->WriteBuffer(L_AUTHOR, 3);

    gLcd->WriteScreen(-1);

    // gGuidance, gTractor and gImplement have no CAN/AgIsoStack dependency, so they're
    // constructed first -- gGuidanceChannel needs both already built (the
    // ISOBUS one feeds gGuidance from its PGN callbacks and calls
    // gImplement->Stop() on AISO).
    gGuidance = new triton::GuidanceSource();
    gTractor = new triton::VehicleTractor(gSerialDebug);
    gImplement = new triton::ImplementPlough(gSerialDebug, gGuidance);

#ifndef MICRO
    gTractor->PrintCalibrationData();
    gImplement->PrintCalibrationData();
#endif

    // Initialise interfaces. CalibrationPlough also loads the stored RTK
    // quality into gGuidance (GuidanceSource itself never touches EEPROM --
    // the shared data model leaves storage to the project, #78).
    gInterface = new triton::InterfacePlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance);
    gCalibration = new triton::CalibrationPlough(gSerialDebug, gLcd, gImplement, gTractor, gGuidance, gInterface);

#ifndef MICRO
    gCalibration->PrintCalibrationData();
    gImplement->PrintCalibrationData();
#endif

#ifdef ISOBUS
    // Board-specific CAN wiring, chosen in ConfigPlough.hpp per board:
    // besturing 0.1 (Teensy 4.1) has its CAN transceiver bodge-wired to
    // FLEXCAN3 (pins 31 TX / 30 RX), not the FlexCAN1 default (22/23) -- see
    // MeijWorks Hardware/Triton/MeijWorks besturing 0.1/Design documents/
    // teensy41-application-note.md. The Teensy 4.0 board uses FLEXCAN1 on 22/23.
#if defined(ESP32S3)
    // Triton01: the ESP32-S3's TWAI controller on the pins from
    // ConfigPlough.hpp, 250 kbit/s per ISO 11783, accept-all filter. Static:
    // TWAIPlugin keeps the three pointers and reads them in open(). 0xFF tells
    // the guidance channel there is no FlexCAN error state to sample.
    static twai_general_config_t gTwaiGeneral = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(ISOBUS_CAN_TX_PIN), static_cast<gpio_num_t>(ISOBUS_CAN_RX_PIN), TWAI_MODE_NORMAL);
    static twai_timing_config_t gTwaiTiming = TWAI_TIMING_CONFIG_250KBITS();
    static twai_filter_config_t gTwaiFilter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    constexpr std::uint8_t kIsobusCanChannel = 0xFF;
    auto gCanPlugin = std::make_shared<isobus::TWAIPlugin>(&gTwaiGeneral, &gTwaiTiming, &gTwaiFilter);
#else
    constexpr std::uint8_t kIsobusCanChannel = ISOBUS_CAN_CHANNEL;
    auto gCanPlugin = std::make_shared<isobus::FlexCANT4Plugin>(kIsobusCanChannel);
    gFlexCanPlugin = gCanPlugin.get();
#endif

    // CAN/ISOBUS bring-up: CAN hardware plugin, NAME + address claim (blocks
    // until claimed), PGN callback registration, initial PGN requests.
    // The channel again, so the dump can read that controller's error state (#149).
    gGuidanceChannel = new triton::IsobusGuidanceChannel(gSerialDebug, gCanPlugin, gGuidance, gImplement, kIsobusCanChannel);
    gGuidanceChannel->Begin();

    // A second control function that presents as an Ag Leader L160 lightbar:
    // an InCommand only broadcasts its cross-track error (PGN 65462) once a
    // lightbar has identified itself (#42, card log 42).
    // AgIsoStack port 0 (the one the guidance channel registered the plugin
    // on) -- NOT kIsobusCanChannel, which is the FlexCAN controller number and
    // only means something to the error-register readout. On the bench the
    // wrong one made the stack retry the claim every loop on a port it does
    // not have, flooding the serial log (2026-10-09).
    gLightbarChannel = new triton::IsobusLightbarChannel(gSerialDebug, gGuidance);
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
#if !defined(ESP32S3)
    gDebugMenu->SetCanReceiveQueueStatsSource(GReadCanReceiveQueueStats);
#endif
#else
    // Receiver port at the stored rate, then the blocking autodetect on the
    // LCD, as the regular Ploegbesturing does: the receivers and the
    // CAN-to-serial converters run anywhere from 4800 (NMEA default) to
    // 115200 (the JD and CNH converters). A detect starts at the stored rate,
    // so a stable installation finds it within two seconds and never writes;
    // the index is persisted only when it changed.
    gSerialGps->begin(triton::InterfaceGuidance::BaudFromIndex(gCalibration->GetGpsBaudIndex()));
    gGuidanceChannel = new triton::SerialGuidanceChannel(gSerialDebug, gSerialGps, gGuidance);
    {
        triton::InterfaceGuidance detect(gLcd, gGuidanceChannel, gGuidance);
        const byte stored = gCalibration->GetGpsBaudIndex();
        const byte found  = detect.DetectBaudrate(stored, triton::InterfaceGuidance::kAll);
        if (found != triton::InterfaceGuidance::kNotFound && found != stored) {
            gCalibration->SetGpsBaudIndex(found);
            gCalibration->CommitGuidanceCalibration();
        }
    }
#ifdef PLAIN_CAN
    gCanChannel = new triton::CanFrameGuidanceChannel(gGuidance);
    GCanSetup();
#endif
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
#endif
#if defined(ESP32S3)
    // One I2C read per iteration refreshes every expander-backed input; the
    // reads further down are then plain bit tests. EEPROM.commit() returns
    // at once unless a write changed something.
    triton::RefreshExpanderInputs();
    EEPROM.commit();
#endif

    // Pumps guidance data acquisition -- CAN I/O + PGN callbacks (ISOBUS) or
    // the UART sentence dispatcher (serial), feeding gGuidance either way.
#ifdef PLAIN_CAN
    // Plain CAN: also the received CAN frames, through GHandleCanMessage().
    GPlainCan.dispatchReceivedMessage();
#endif
    gGuidanceChannel->Update();
#ifdef ISOBUS
    gLightbarChannel->Update();
    gVtInterface->Update();
    gTcInterface->Update();
    gDebugMenu->Update();

    // Update interface -- the VT's Wider/Narrower soft keys (held levels, #245)
    // and Calibrate tap feed InterfacePlough's button handling, see
    // IsobusVtInterface.hpp. ConsumeCalibratePress() must run on every
    // iteration: it is the only thing that clears the pending tap.
    gInterface->Update(gVtInterface->IsWiderHeld(),
                       gVtInterface->IsNarrowerHeld(),
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
#ifdef ISOBUS
        // The wizard may have changed the preferred VT (#255).
        gVtInterface->LoadPreferredVt();
#endif
    }
}

#ifdef PLAIN_CAN
// --------------------------------------------
// Setup plain CAN (Teensy 3.2, Teensy 4.1 / 4.0)
// --------------------------------------------
// The regular Ploegbesturing's ACAN_T4 setup, for every plain-CAN board: the
// same four filters ploegbesturing v1.30/v1.31 used (StarFire position and
// speed, JD and Trimble/CNH XTE), plus the three standard NMEA2000 PGNs
// CanFrameGuidanceChannel decodes, from any source address, so a receiver
// that only broadcasts those is read too. GPlainCan is ACAN::can0 on the 3.2
// (its only controller, pins 3 TX / 4 RX) and the board's FlexCAN through
// ACAN_T4 on a Teensy 4 -- see the includes at the top.
static void GCanSetup() {
    triton::PlainCanBegin<GPlainCanSettings>(GPlainCan, gSerialDebug, GHandleCanMessage);
}

static void GHandleCanMessage(const CANMessage& inMessage) {
    gCanChannel->Update(inMessage.id, inMessage.data, inMessage.len);
}
#endif
