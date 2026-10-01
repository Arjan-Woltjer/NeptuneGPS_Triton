/*
  IsobusGuidanceChannel - ISOBUS CAN bus gateway for guidance data acquisition
  Copyright (C) 2011-2026 J.A. Woltjer.
  All rights reserved.

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
#pragma once

// ISOBUS: PlatformIO's LDF compiles every .cpp under a library folder it's
// pulled in at all, regardless of which #ifdef branch main.cpp's own
// #include takes -- so on teensy41_serial (no ISOBUS, no AgIsoStack lib_dep
// installed) this header must itself become empty, not just conditionally
// unused, or the AgIsoStack includes below fail to resolve.
//
// EPOXY_DUINO admits it to [env:native]. This header used to say the class
// was "a hardware/protocol adapter, not pure logic worth native testing":
// that was true only while AgIsoStack could not be built off-target. It can
// (NeptuneGPS_Triton#98), so the commit rules and the per-PGN counters -- the
// half of this class that is not the stack -- are tested there now.
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

// Specific headers rather than <AgIsoStack.hpp>: that umbrella pulls in the
// Teensy-only FlexCAN files. Arduino.h above defines min()/max() as macros,
// which mangle the three-argument std::min/std::max these headers reach
// through the STL, so they are parked around the include and restored after
// -- the same sandwich GuidanceGeometry.hpp puts around its <math.h>.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/hardware_integration/can_hardware_plugin.hpp>
#include <isobus/isobus/can_internal_control_function.hpp>
#include <isobus/isobus/can_message.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "../implement/ImplementPlough.hpp"
#include "CanErrorMonitor.hpp"
#include "GuidanceSource.hpp"

namespace triton
{

// Owns the CAN hardware plugin, ISOBUS address claim, and every PGN this
// controller consumes: standard NMEA2000 guidance messages (129025/129026/
// 129283, confirmed broadcast by the reference Fendt 6240 tractor), the
// legacy JD/Trimble/CNH proprietary messages VehicleGps used to decode (kept
// for backwards compatibility with equipment that doesn't broadcast the
// NMEA2000 set), and AISO (All Implement Stop Operations, a real ISOBUS
// safety broadcast). Feeds GuidanceSource; stops ImplementPlough
// directly on AISO.
class IsobusGuidanceChannel {
public:
    // canPlugin is injected rather than constructed internally, the same way
    // SerialGuidanceChannel takes a HardwareSerial* instead of picking its own
    // port -- which physical CAN peripheral/pins to use is a board-specific
    // choice that belongs at the call site (see main.cpp), not hardcoded here.
    //
    // canChannel is the same channel number the FlexCANT4Plugin was built with
    // (0, 1, 2 = FLEXCAN1, 2, 3). It is only used to read that controller's
    // error registers for the debug dump (#149); the default, 0xFF, reads
    // nothing -- as the native tests, which have no FlexCAN, construct it.
    IsobusGuidanceChannel(Stream* serialDebug, std::shared_ptr<isobus::CANHardwarePlugin> canPlugin, GuidanceSource* guidance, ImplementPlough* implement,
                          std::uint8_t canChannel = 0xFF);

    // Brings up the CAN hardware plugin, claims a NAME/address (blocks until
    // claim completes -- a one-time startup cost per ISO 11783's >=250ms
    // contention window, never called again after setup()), and registers
    // every PGN callback + sends the initial PGN requests once claimed.
    void Begin();

    // Call every loop() iteration -- pumps CAN I/O.
    void Update();

    inline std::shared_ptr<isobus::InternalControlFunction> GetControlFunction() { return controlFunction; }

    // Per-PGN receive counters, incremented from the On* callbacks below --
    // AgIsoStack itself exposes no message/error counters (only
    // CANNetworkManager::get_estimated_busload()), so this is the only place
    // that can count them. Read by IsobusDebugMenu.
    struct MessageCounters {
        uint32_t positionNmea2000 = 0, speedNmea2000 = 0, xteNmea2000 = 0;
        uint32_t positionLegacy = 0, speedLegacy = 0;
        uint32_t xteJohnDeereLegacy = 0, xteTrimbleLegacy = 0;
        uint32_t allImplementStop = 0;
        unsigned long lastAllImplementStopMs = 0;
        // Last decoded 2-bit state (0=Stop, 1=Permit, 2=Error, 3=Not available),
        // updated on every AISO frame regardless of state -- lastAllImplementStopMs
        // only advances on an actual Stop. 0xFF = no AISO frame seen yet.
        uint8_t lastAllImplementStopState = 0xFF;

        // Bus-diagnostic snapshot, updated unconditionally on receipt (ahead
        // of any source-address filter or sentinel check) -- lets
        // IsobusDebugMenu show what's actually arriving on the wire even
        // when a handler's own filtering drops the message before it
        // reaches GuidanceSource. 0xFF = no message with a resolvable
        // source control function seen yet.
        uint8_t  lastSpeedLegacySourceAddress          = 0xFF;
        uint16_t lastSpeedLegacyRaw                    = 0;
        uint8_t  lastXteJohnDeereLegacySourceAddress   = 0xFF;
        uint16_t lastXteJohnDeereLegacyRawWord         = 0;  // d[4]<<8|d[3], before the -32000/>>1 decode
        uint8_t  lastXteJohnDeereLegacyRawByte1         = 0;  // d[1], expected 0x15 for quality=4
        // All 8 payload bytes of the last accepted PGN 65535 message. The
        // word/byte1 fields above cover only 3 of them, which is not enough
        // to derive Ag Leader's layout on this overloaded PGN (GitHub issue
        // #20) -- that needs every byte, logged over time against
        // ground-truth XTE read off the terminal.
        uint8_t  lastXteJohnDeereLegacyPayload[8]      = { 0, 0, 0, 0, 0, 0, 0, 0 };
        uint32_t lastXteJohnDeereLegacyPayloadMs       = 0;
        // The xteJohnDeereLegacy count and last-SA above cover every PGN 0xFFFF
        // sender, which on a John Deere bus is mostly other traffic (#153). The
        // raw capture is only from a sender the decoder recognises, and records
        // which one; the carrier fields count frames that decoded to XTE.
        uint8_t  lastXteJohnDeereLegacyPayloadSourceAddress = 0xFF;
        uint32_t xteJohnDeereCarrier                   = 0;
        uint32_t lastXteJohnDeereCarrierMs             = 0;
        uint8_t  lastXteTrimbleLegacySourceAddress     = 0xFF;

        // PGN 44032, the standard ISO 11783-7 guidance channel. Diagnostics
        // only and deliberately so: it carries curvature, not cross-track
        // error, so nothing here reaches the control path. What it does give
        // is the steering system's own account of why guidance is or is not
        // happening -- session 9's CNH tractor reported MECHANICALLY LOCKED
        // OUT for all 8420 frames, which no other message on that bus said.
        uint8_t  lastGuidanceMechanicalLockout = 3;   // 3 = not available
        uint8_t  lastGuidanceSteeringReadiness = 3;
        uint8_t  lastGuidanceRemoteEngage      = 3;
        bool     lastGuidanceHasCurvature      = false;
        float    lastGuidanceCurvaturePerKm    = 0.0f;
        uint32_t lastGuidanceMachineInfoMs     = 0;

        uint32_t guidanceMachineInfo = 0;
        uint32_t positionDeltaNmea2000 = 0;
        uint32_t gnssPositionData = 0;

        // PGN 129029 is the only message on any bus captured so far that
        // carries a GNSS quality indicator, so these are worth showing even
        // when nothing else about the fix is interesting.
        uint8_t  lastGnssMethod      = 0xFF;   // field 8; 4 = RTK fixed
        uint8_t  lastGnssSvCount     = 0xFF;
        float    lastGnssHdop        = 0.0f;
        bool     lastGnssHasHdop     = false;
        uint32_t lastGnssPositionMs  = 0;

        inline uint32_t Total() const {
            return positionNmea2000 + speedNmea2000 + xteNmea2000 + positionLegacy
                 + speedLegacy + xteJohnDeereLegacy + xteTrimbleLegacy + allImplementStop
                 + guidanceMachineInfo + positionDeltaNmea2000 + gnssPositionData;
        }
    };

    // Snapshot, safe to call at any time -- single-threaded loop, no
    // concurrent writer.
    // Our NAME's 21-bit identity number from the board's 24-bit Teensy serial
    // (#45): its low 21 bits, so the mapping stays traceable by hand.
    static std::uint32_t IdentityNumberFromSerial(std::uint32_t teensySerial);

    // Whether Update() should re-request the guidance PGNs now: true when the
    // last request is at least a retry interval old and any of position (GGA),
    // speed/course (VTG) or cross-track (XTE) has not produced a committed
    // message within that interval (0 = never). Pure, so the rule is testable
    // without the CAN stack (#153).
    static bool NeedsGuidanceRequest(unsigned long nowMs, unsigned long lastRequestMs,
                                     unsigned long ggaMs, unsigned long vtgMs, unsigned long xteMs);

    inline MessageCounters GetMessageCounters() const { return counters; }
    inline void            ResetMessageCounters()      { counters = MessageCounters(); canErrors.Reset(); }

    // The CAN controller's error state, sampled every Update() (#149). Always
    // "no samples" on a build without the i.MX RT1062 FlexCAN.
    inline const CanErrorMonitor& GetCanErrors() const { return canErrors; }

private:
    // The native suite drives the eight PGN callbacks below directly, with a
    // CANMessage it builds itself. Routing a frame through the network
    // manager instead would make every test wait on the stack's own
    // std::chrono timers for an address claim; these callbacks are bytes in,
    // GuidanceSource and counters out (NeptuneGPS_Triton#98). Declared here,
    // defined only in the test build.
    friend struct IsobusGuidanceChannelTestAccess;

    Stream*          serialDebug;
    GuidanceSource*  guidance;
    ImplementPlough* implement;

    std::shared_ptr<isobus::CANHardwarePlugin>       canPlugin;
    std::shared_ptr<isobus::InternalControlFunction> controlFunction;

    MessageCounters counters;

    // FlexCAN error-state watch (#149). Sampled only once Begin() has started
    // the CAN interface: FlexCAN_T4::begin() is what switches the controller's
    // clock on, and reading an unclocked peripheral's registers can hard-fault
    // the i.MX RT1062.
    CanErrorMonitor canErrors;
    std::uint8_t    canChannel;
    bool            canStarted = false;
    void SampleCanErrors();
    void PrintCanState(const char* label);

    // Position/speed/XTE PGN requests are retried every kPgnRetryIntervalMs
    // (see Update()) until each family has produced at least one message --
    // a one-shot request at address-claim time can race a legacy GPS unit's
    // own power-on bring-up, or simply get lost.
    static constexpr unsigned long kPgnRetryIntervalMs = 10000UL;
    unsigned long lastPgnRetryMs = 0;
    void RequestGuidancePgns();

    // Static PGN callbacks -- void* context is always `this`.
    static void OnPositionNmea2000(const isobus::CANMessage& msg, void* context);
    static void OnSpeedNmea2000(const isobus::CANMessage& msg, void* context);
    static void OnXteNmea2000(const isobus::CANMessage& msg, void* context);
    static void OnLegacyPosition(const isobus::CANMessage& msg, void* context);
    static void OnLegacySpeed(const isobus::CANMessage& msg, void* context);
    static void OnLegacyXteJohnDeere(const isobus::CANMessage& msg, void* context);
    static void OnLegacyXteTrimble(const isobus::CANMessage& msg, void* context);
    static void OnAllImplementStop(const isobus::CANMessage& msg, void* context);
    static void OnGuidanceMachineInfo(const isobus::CANMessage& msg, void* context);
    static void OnPositionDeltaNmea2000(const isobus::CANMessage& msg, void* context);
    static void OnGnssPositionData(const isobus::CANMessage& msg, void* context);
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
