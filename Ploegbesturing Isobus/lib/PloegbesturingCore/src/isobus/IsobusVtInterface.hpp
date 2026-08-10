/*
  IsobusVtInterface - ISOBUS Virtual Terminal client and diagnostics for the plough controller
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

// See IsobusGuidanceChannel.hpp's matching comment: ARDUINO excludes this
// from [env:native]; ISOBUS is required too so this header (and its
// #include <AgIsoStack.hpp>) becomes entirely empty on teensy41_serial,
// where AgIsoStack isn't installed as a lib_dep -- PlatformIO's LDF compiles
// every .cpp under a pulled-in library folder regardless of which #ifdef
// branch main.cpp's own #include takes.
#if defined(ARDUINO) && defined(ISOBUS)

#include <Arduino.h>
#include <AgIsoStack.hpp>

#include "ImplementPlough.hpp"
#include "GuidanceSource.hpp"

namespace triton
{

// Owns the ISOBUS VirtualTerminalClient (tractor touchscreen UI) and
// DiagnosticProtocol (product identification, DM1/DM2 fault reporting).
// Depends on an already-address-claimed InternalControlFunction --
// construct after IsobusGuidanceChannel::Begin() completes.
//
// Soft-key handling (Wider/Narrower/Auto) is currently display + event
// logging only, NOT wired to drive ImplementPlough. main.cpp's loop()
// already calls InterfacePlough::Update(), which unconditionally calls
// ImplementPlough::Adjust() every tick based on the physical-button/GPS
// mode ladder; having VT soft keys *also* call Wider()/Narrower()/Adjust()
// independently would race that same-tick call and is a real machine-
// safety-behavior decision (how VT input should arbitrate with the
// physical button ladder), not something to invent silently here. Flagged
// for a deliberate follow-up decision.
class IsobusVtInterface {
public:
    IsobusVtInterface(Stream* serialDebug, ImplementPlough* implement, GuidanceSource* guidance,
                       std::shared_ptr<isobus::InternalControlFunction> controlFunction);

    // Call once from setup(), after the control function's address is claimed.
    void Begin();

    // Call every loop() iteration.
    void Update();

    // Diagnostics for IsobusDebugMenu.
    inline bool IsConnected() const { return vtClient && vtClient->get_is_connected(); }

    // Coarse progress through the ~23-step connect/upload/activate handshake
    // (isobus::VirtualTerminalClient::StateMachineState) -- NOT byte-accurate
    // upload progress (that would need the transport-protocol session's own
    // percentage, which AgIsoStack doesn't expose a public path to reach).
    // Backed by a locally-patched get_state() on VirtualTerminalClient --
    // see Documentation/AgIsoStackVendorPatches.md.
    int         GetStateStep() const;
    int         GetStateTotalSteps() const;
    const char* GetStateName() const;

    // Which VT version we actually negotiated (public, unpatched --
    // isobus::VirtualTerminalClient::get_connected_vt_version() already
    // existed). Only meaningful once past WaitForPartnerVTStatusMessage --
    // added 2026-08-10 to check a live suspicion: our hand-rolled VT3 object
    // pool's WorkingSet object includes a language-code list, and it's worth
    // confirming what VT version is on the other end when the VT rejects the
    // pool (see VTObjectPool.cpp).
    const char* GetVtVersionName() const;

private:
    class Logger : public isobus::CANStackLogger {
    public:
        void sink_CAN_stack_log(isobus::CANStackLogger::LoggingLevel level, const std::string& text) override;
    };

    Stream*          serialDebug;
    ImplementPlough* implement;
    GuidanceSource*  guidance;

    std::shared_ptr<isobus::InternalControlFunction>    controlFunction;
    std::shared_ptr<isobus::DiagnosticProtocol>         diagnostics;
    std::shared_ptr<isobus::VirtualTerminalClient>      vtClient;
    isobus::EventCallbackHandle                         softKeyListener;
    isobus::EventCallbackHandle                         buttonListener;
    Logger                                              logger;

    unsigned long lastVtUpdate = 0;

    // On-change gating for updateVtVariables() -- added 2026-08-10 (van
    // Mastwijk) after Session 4 found intermittent post-Connect drops
    // ("[VT]: Status Timeout", AgIsoStack's own 3 s VT_STATUS_TIMEOUT_MS)
    // that did NOT reproduce while a swapped-in AgIsoStack reference pool
    // was loaded. That reference pool's own example only calls
    // send_change_numeric_value() on a button press; this class used to call
    // it unconditionally 4x every 100 ms (40 msg/s) regardless of whether
    // anything changed, for as long as the VT stayed connected -- real,
    // bound OutputNumber widgets in OUR pool means the VT does real redraw
    // work each time, unlike the reference pool's IDs (which likely don't
    // resolve to a NumberVariable at all, so get cheaply rejected). Leading
    // hypothesis, not yet re-verified against hardware: sustained redraw
    // load intermittently starves the VT's own periodic status broadcast
    // past our 3 s window. Fix sends only on real value change, plus a 1 s
    // heartbeat resend (so a dropped CAN frame can't leave the VT stale
    // forever) -- cuts steady-state traffic roughly 10x for slow-changing
    // plough telemetry with no functional loss (a human reading a numeric
    // field can't perceive 10 Hz vs. on-change+1 Hz).
    bool          sentInitialVtVariables = false;
    unsigned long lastVtVariableHeartbeat = 0;
    int32_t       lastSentPosition = 0;
    int32_t       lastSentSetpoint = 0;
    int32_t       lastSentXte      = 0;
    int32_t       lastSentOffset   = 0;

    void updateVtVariables();
    void onVtKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event);
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
