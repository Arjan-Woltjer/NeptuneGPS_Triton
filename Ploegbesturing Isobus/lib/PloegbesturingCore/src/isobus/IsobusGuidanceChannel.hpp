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

// ARDUINO: excluded from [env:native] (matches CalibrationPlough's exclusion --
// this is a hardware/protocol adapter, not pure logic worth native testing).
// ISOBUS: PlatformIO's LDF compiles every .cpp under a library folder it's
// pulled in at all, regardless of which #ifdef branch main.cpp's own
// #include takes -- so on teensy41_serial (no ISOBUS, no AgIsoStack lib_dep
// installed) this header must itself become empty, not just conditionally
// unused, or IsobusGuidanceChannel.cpp's #include <AgIsoStack.hpp> below
// fails to resolve.
#if defined(ARDUINO) && defined(ISOBUS)

#include <Arduino.h>
#include <AgIsoStack.hpp>

#include "ImplementPlough.hpp"
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
    IsobusGuidanceChannel(Stream* serialDebug, std::shared_ptr<isobus::CANHardwarePlugin> canPlugin, GuidanceSource* guidance, ImplementPlough* implement);

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
        uint8_t  lastXteTrimbleLegacySourceAddress     = 0xFF;

        inline uint32_t Total() const {
            return positionNmea2000 + speedNmea2000 + xteNmea2000 + positionLegacy
                 + speedLegacy + xteJohnDeereLegacy + xteTrimbleLegacy + allImplementStop;
        }
    };

    // Snapshot, safe to call at any time -- single-threaded loop, no
    // concurrent writer.
    inline MessageCounters GetMessageCounters() const { return counters; }
    inline void            ResetMessageCounters()      { counters = MessageCounters(); }

private:
    Stream*          serialDebug;
    GuidanceSource*  guidance;
    ImplementPlough* implement;

    std::shared_ptr<isobus::CANHardwarePlugin>       canPlugin;
    std::shared_ptr<isobus::InternalControlFunction> controlFunction;

    MessageCounters counters;

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
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
