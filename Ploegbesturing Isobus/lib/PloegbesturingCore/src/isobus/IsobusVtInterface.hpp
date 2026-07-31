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

#ifdef ARDUINO

#include <Arduino.h>
#include <AgIsoStack.hpp>

#include "ImplementPlough.hpp"
#include "IsobusGuidanceSource.hpp"

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
    IsobusVtInterface(Stream* serialDebug, ImplementPlough* implement, IsobusGuidanceSource* guidance,
                       std::shared_ptr<isobus::InternalControlFunction> controlFunction);

    // Call once from setup(), after the control function's address is claimed.
    void Begin();

    // Call every loop() iteration.
    void Update();

private:
    class Logger : public isobus::CANStackLogger {
    public:
        void sink_CAN_stack_log(isobus::CANStackLogger::LoggingLevel level, const std::string& text) override;
    };

    Stream*                serialDebug;
    ImplementPlough*       implement;
    IsobusGuidanceSource*  guidance;

    std::shared_ptr<isobus::InternalControlFunction>  controlFunction;
    std::shared_ptr<isobus::DiagnosticProtocol>        diagnostics;
    std::shared_ptr<isobus::VirtualTerminalClient>     vtClient;
    std::shared_ptr<void>                              softKeyListener;
    std::shared_ptr<void>                              buttonListener;
    Logger                                              logger;

    unsigned long lastVtUpdate = 0;

    void updateVtVariables();
    void onVtKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event);
};

}  // namespace triton

#endif  // ARDUINO
