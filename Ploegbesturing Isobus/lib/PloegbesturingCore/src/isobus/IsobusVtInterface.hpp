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

    void updateVtVariables();
    void onVtKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event);
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
