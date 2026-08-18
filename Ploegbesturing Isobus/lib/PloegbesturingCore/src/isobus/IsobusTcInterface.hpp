/*
  IsobusTcInterface - ISOBUS Task Controller client for the plough controller
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

// Same guard rationale as IsobusVtInterface.hpp: ARDUINO excludes this
// from [env:native]; ISOBUS is required too so this header (and its
// #include <AgIsoStack.hpp>) becomes entirely empty on teensy41_serial.
#if defined(ARDUINO) && defined(ISOBUS)

#include <Arduino.h>
#include <AgIsoStack.hpp>

#include "../implement/ImplementPlough.hpp"
#include "../guidance/GuidanceSource.hpp"

namespace triton
{

// Owns the ISOBUS TaskControllerClient and this plough's DDOP.
//
// PURPOSE: obtains DDI 513 (guidance line deviation at the implement's
// Device Reference Point) and DDI 514 (GNSS quality) from the Task
// Controller. This is a DATA SOURCE ONLY -- the TC has no control
// authority over the plough. The plough control loop remains entirely in
// ImplementPlough/InterfacePlough.
//
// NOT a replacement for the PGN 129283 path in IsobusGuidanceChannel: TC
// process data is on-change/interval triggered and is a reference
// channel, not a control-loop feedback path. Both run concurrently. See
// the source arbitration comment on drpDeviationMm/tcGnssQuality below,
// and NeptuneGPS Documentation/Design documents/Triton_TC_Client_Design.md
// sec 6 for the full writeup.
//
// SCOPE (design doc sec 8, phases 1-3): this declares only what's needed
// through geometry validation -- Device/Connector/Function elements, the
// connector's X/Y offsets (load-bearing for DDI 513's correctness, see
// design doc sec 4.3), and the two TC-writable process data variables.
// The design doc's own sketch also listed DDI 67/70 (working width) and
// 141 (actual work state) on the Function element -- deliberately NOT
// declared here: this implement controls plough *offset*, not width
// (ImplementPlough has no working-width concept at all), and has no
// engaged/disengaged state ImplementPlough exposes either. Declaring
// DDIs with no real backing value would mean inventing data. Revisit if/
// when those capabilities exist.
//
// Depends on an already-address-claimed InternalControlFunction --
// construct after IsobusGuidanceChannel::Begin() completes, exactly like
// IsobusVtInterface.
class IsobusTcInterface {
public:
    IsobusTcInterface(Stream* serialDebug, ImplementPlough* implement, GuidanceSource* guidance,
                       std::shared_ptr<isobus::InternalControlFunction> controlFunction);

    // Call once from setup(), after the control function's address is claimed.
    void Begin();

    // Call every loop() iteration.
    void Update();

    // Diagnostics for IsobusDebugMenu.
    inline bool          IsConnected() const        { return tcClient && tcClient->get_is_connected(); }
    // AgIsoStack's own docs warn not all TCs report this correctly (e.g. John
    // Deere reports "always in task") -- treat as advisory only, never gate
    // safety-relevant behaviour on it.
    inline bool          IsTaskActive() const        { return tcClient && tcClient->get_is_task_active(); }
    inline int           GetDrpDeviationMm() const    { return drpDeviationMm; }
    inline uint8_t       GetTcGnssQuality() const     { return tcGnssQuality; }
    inline unsigned long GetDrpTimestamp() const      { return lastDrpUpdate; }
    inline unsigned long GetQualityTimestamp() const  { return lastQualityUpdate; }

    // Raw activity counters, independent of DDI 513/514 specifically -- added
    // 2026-08-10 to answer "is the TC sending us ANYTHING at all" separately
    // from "is it sending 513/514". A DDOP can connect and have an active
    // task while the TC still sends zero process data for any DDI (e.g. no
    // guidance line set, or our device elements not mapped into the task on
    // the UT's own setup screen) -- these counters distinguish that from a
    // callback-wiring bug.
    inline unsigned long GetValueCommandCount() const   { return valueCommandCount; }
    inline std::uint16_t GetLastValueCommandDdi() const  { return lastValueCommandDdi; }
    inline unsigned long GetLastValueCommandMs() const   { return lastValueCommandMs; }
    inline unsigned long GetValueRequestCount() const   { return valueRequestCount; }

    // Whether the CONNECTED TC itself reports TC-GEO support -- read from its
    // own ParameterVersion handshake message (isobus_task_controller_client.cpp,
    // TechnicalDataMessageCommands::ParameterVersion), not something we
    // configure. Added 2026-08-10 to settle session 3's "does this TC
    // implement TC-GEO at all" theory directly instead of inferring it from
    // DDI silence -- see Documentation/ISOBUS_TC_Manufacturer_Comparison.md
    // (TC-GEO is a separately licensed/gated feature on most brands) and
    // Documentation/TCGEO_Field_Test_Log.md. Meaningless before IsConnected()
    // returns true; returns false (not "unknown") until then.
    inline bool SupportsTcGeoWithPosition() const {
        return tcClient && tcClient->get_connected_tc_option_supported(
            isobus::TaskControllerClient::ServerOptions::SupportsTCGEOWithPositionBasedControl);
    }
    inline bool SupportsTcGeoWithoutPosition() const {
        return tcClient && tcClient->get_connected_tc_option_supported(
            isobus::TaskControllerClient::ServerOptions::SupportsTCGEOWithoutPositionBasedControl);
    }

private:
    // --- TC callbacks (static, AgIsoStack uses raw function pointers) ---
    // Value types are int32_t, matching the AgIsoStack-Arduino version
    // actually vendored for this build (.pio/libdeps/teensy41_isobus/
    // AgIsoStack -- confirmed against isobus_task_controller_client.hpp's
    // RequestValueCommandCallback/ValueCommandCallback typedefs; NOT the
    // stale, differently-versioned copy under .pio/libdeps/teensy41/
    // AgIsoStack, which uses uint32_t and would silently compile against
    // the wrong signatures if referenced instead).
    static bool OnValueCommand(std::uint16_t elementNumber,
                               std::uint16_t DDI,
                               std::int32_t  processVariableValue,
                               void*         parentPointer);

    static bool OnValueRequest(std::uint16_t elementNumber,
                               std::uint16_t DDI,
                               std::int32_t& processVariableValue,
                               void*         parentPointer);

    void buildDdop();

    Stream*           serialDebug;
    ImplementPlough*  implement;
    GuidanceSource*   guidance;

    std::shared_ptr<isobus::InternalControlFunction>        controlFunction;
    std::shared_ptr<isobus::DeviceDescriptorObjectPool>     ddop;
    std::shared_ptr<isobus::TaskControllerClient>           tcClient;

    // Received values. Deliberately NOT written straight into GuidanceSource --
    // DDI 513 is a different quantity to PGN 129283 XTE (implement Device
    // Reference Point vs. tractor guidance reference). Design doc sec 6
    // recommends starting at option C (cross-check only, DDI 513 never
    // drives the plough) before considering option A (a separate field on
    // GuidanceSource) -- this class intentionally has no wiring into
    // ImplementPlough/InterfacePlough yet, only diagnostics exposure.
    int           drpDeviationMm     = 0;
    uint8_t       tcGnssQuality      = 0;
    unsigned long lastDrpUpdate      = 0;
    unsigned long lastQualityUpdate  = 0;

    unsigned long   valueCommandCount    = 0;
    std::uint16_t   lastValueCommandDdi  = 0xFFFF;  // 0xFFFF = none received yet
    unsigned long   lastValueCommandMs   = 0;
    unsigned long   valueRequestCount    = 0;
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
