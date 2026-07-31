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
    IsobusGuidanceChannel(Stream* serialDebug, GuidanceSource* guidance, ImplementPlough* implement);

    // Brings up the CAN hardware plugin, claims a NAME/address (blocks until
    // claim completes -- a one-time startup cost per ISO 11783's >=250ms
    // contention window, never called again after setup()), and registers
    // every PGN callback + sends the initial PGN requests once claimed.
    void Begin();

    // Call every loop() iteration -- pumps CAN I/O.
    void Update();

    inline std::shared_ptr<isobus::InternalControlFunction> GetControlFunction() { return controlFunction; }

private:
    Stream*                serialDebug;
    GuidanceSource*  guidance;
    ImplementPlough*       implement;

    std::shared_ptr<isobus::FlexCANT4Plugin>         can0;
    std::shared_ptr<isobus::InternalControlFunction> controlFunction;

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
