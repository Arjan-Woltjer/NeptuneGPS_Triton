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

// See IsobusGuidanceChannel.hpp's matching comment: ISOBUS is required so this
// header becomes entirely empty on teensy41_serial, where AgIsoStack isn't
// installed as a lib_dep -- PlatformIO's LDF compiles every .cpp under a
// pulled-in library folder regardless of which #ifdef branch main.cpp's own
// #include takes. EPOXY_DUINO admits it to [env:native] (#98).
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

// Specific headers rather than <AgIsoStack.hpp>, which pulls in the
// Teensy-only FlexCAN files; the min()/max() macros Arduino.h defines are
// parked around them, as in IsobusTcInterface.hpp.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <can_internal_control_function.hpp>
#include <can_message.hpp>
#include <can_partnered_control_function.hpp>
#include <can_stack_logger.hpp>
#include <event_dispatcher.hpp>
#include <isobus_diagnostic_protocol.hpp>
#include <isobus_virtual_terminal_client.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "../implement/ImplementPlough.hpp"
#include "GuidanceSource.hpp"

namespace triton
{

// Owns the ISOBUS VirtualTerminalClient (tractor touchscreen UI) and
// DiagnosticProtocol (product identification, DM1/DM2 fault reporting).
// Depends on an already-address-claimed InternalControlFunction --
// construct after IsobusGuidanceChannel::Begin() completes.
//
// Soft-key handling: Wider/Narrower (2026-08-10) and Calibrate (2026-09-08)
// are wired through ConsumeWiderPress()/ConsumeNarrowerPress()/
// ConsumeCalibratePress() below -- main.cpp's loop() passes all three into
// InterfacePlough::Update(), which lands each in the CheckButtons() branch
// that mirrors the physical button meaning the same thing (Wider -> the
// LEFT_BUTTON_2 slot, Narrower -> RIGHT_BUTTON_2, Calibrate -> the both-held
// combo main.cpp turns into a CalibrationPlough::Calibrate() call), with the
// physical buttons keeping priority in an arbitration. They get their own
// branches rather than being OR'd into the physical conditions (which is how
// Wider/Narrower were first wired) because a consume-once edge can never
// satisfy those branches' hold-duration debounce -- see CheckButtons() for
// the full reasoning.
//
// Auto (Key_Auto) is deliberately NOT wired -- InterfacePlough's AUTO mode is
// derived from GPS/hitch state, not user-settable via a button, so there's no
// existing target for it; still display + log only.
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
    // see NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md.
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

    // Raw count/age of VT Status Messages (PGN 0xE600/VirtualTerminalToECU,
    // function 0xFE) actually seen on the bus, independent of AgIsoStack's
    // own internal state machine -- added 2026-09-05 to tell apart "the VT
    // isn't sending its mandatory status broadcast" from "it's sending it,
    // we're just not acting on it in time" after a reproducible
    // Status Timeout ~3s post-Connect on an Ag Leader InCommand 1200 (see
    // HardwareTestNotes.md). AgIsoStack's own VirtualTerminalClient tracks
    // this internally (lastVTStatusTimestamp_ms) but doesn't expose it, so
    // this is a second, independent global PGN listener alongside the
    // client's own -- CANNetworkManager supports multiple listeners per PGN,
    // this doesn't steal or alter the message the client itself reacts to.
    unsigned int  GetVtStatusMessageCount() const { return vtStatusMessageCount; }
    unsigned long GetVtStatusMessageAgeMs() const {
        return vtStatusMessageCount == 0 ? 0 : millis() - lastVtStatusMessageMs;
    }

    // The bound VT partner's own address and validity. Added 2026-09-05: the
    // whole of GitHub issue #17 turned on partnerControlFunction->
    // get_address_valid() silently going false (the partner evicted from
    // AgIsoStack's control-function table), and neither of those was visible
    // anywhere -- the diagnosis needed source-diving instead of reading a
    // debug line. Surface both so the next occurrence is legible.
    bool         IsPartnerAddressValid() const { return partner && partner->get_address_valid(); }
    std::uint8_t GetPartnerAddress() const { return partner ? partner->get_address() : 0xFE; }

    // The VT's partnered control function itself. IsobusTcInterface passes
    // this to TaskControllerClient as its `primaryVT`, which is what the TC
    // client uses to source ISO 11783-7 language/unit data when the connected
    // TC server is older than version 4 -- see IsobusTcInterface::Begin().
    // Null until Begin() has run.
    std::shared_ptr<isobus::PartneredControlFunction> GetPartner() const { return partner; }

    // Reconnect watchdog counters (GitHub issue #18) -- see the private
    // reconnect fields for why this exists.
    unsigned int  GetReconnectAttemptCount() const { return reconnectAttempts; }

    // Consume-once VT soft-key press signals -- set by onVtKeyEvent() on key
    // release, cleared by the call itself (edge-triggered, matching a
    // discrete VT tap rather than a held physical button). Direction mapping
    // confirmed against ImplementPlough::Adjust(): direction=-1
    // (LEFT_BUTTON_2's slot) -> Wider(), direction=+1 (RIGHT_BUTTON_2's
    // slot) -> Narrower() -- so Key_Wider must feed the LEFT slot and
    // Key_Narrower the RIGHT slot for both input paths to mean the same
    // thing. See InterfacePlough::CheckButtons() for where these land.
    inline bool ConsumeWiderPress() {
        bool v = pendingWiderPress;
        pendingWiderPress = false;
        return v;
    }
    inline bool ConsumeNarrowerPress() {
        bool v = pendingNarrowerPress;
        pendingNarrowerPress = false;
        return v;
    }
    inline bool ConsumeCalibratePress() {
        bool v = pendingCalibratePress;
        pendingCalibratePress = false;
        return v;
    }

private:
    // The native suite drives the VT-to-ECU callback, the key handler and the
    // reconnect watchdog directly, and sets the connection history the
    // watchdog keys off: a real VT server is the one thing a host test cannot
    // stand up (NeptuneGPS_Triton#98). Declared here, defined only in the
    // test build.
    friend struct IsobusVtInterfaceTestAccess;

    class Logger : public isobus::CANStackLogger {
    public:
        void sink_CAN_stack_log(isobus::CANStackLogger::LoggingLevel level, const std::string& text) override;
    };

    Stream*          serialDebug;
    ImplementPlough* implement;
    GuidanceSource*  guidance;

    std::shared_ptr<isobus::InternalControlFunction>    controlFunction;
    std::shared_ptr<isobus::PartneredControlFunction>   partner;
    std::shared_ptr<isobus::DiagnosticProtocol>         diagnostics;
    std::shared_ptr<isobus::VirtualTerminalClient>      vtClient;
    // Zero-initialised: EventCallbackHandle is a plain std::size_t, so without
    // this both hold garbage between construction and Begin() (found by cppcheck
    // once check_src_filters finally brought this library into scope).
    isobus::EventCallbackHandle                         softKeyListener = 0;
    isobus::EventCallbackHandle                         buttonListener = 0;
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

    bool pendingWiderPress     = false;
    bool pendingNarrowerPress  = false;
    bool pendingCalibratePress = false;

    unsigned int  vtStatusMessageCount  = 0;
    unsigned long lastVtStatusMessageMs = 0;

    // Reconnect watchdog (GitHub issue #18). AgIsoStack's own state machine
    // already retries from Disconnected as soon as the partner's address is
    // valid again, so this exists for the cases where that never happens: the
    // client wedged in an intermediate state, or a partner that came back but
    // did not re-trigger the retry. Only armed after a first successful
    // connection, so it can never interfere with the initial handshake.
    //
    // Note the obvious implementation does NOT work: VirtualTerminalClient::
    // initialize() is guarded by `if (!initialized)`, so calling it again on
    // a live client is a no-op. A real re-attempt needs terminate() first,
    // which drops the PGN callbacks and resets the state machine so
    // initialize() will actually rebuild them.
    bool          hasEverConnected     = false;
    unsigned long disconnectedSinceMs  = 0;
    unsigned long lastReconnectTryMs   = 0;
    unsigned int  reconnectAttempts    = 0;

    void updateVtVariables();
    void updateReconnectWatchdog();
    void onVtKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event);
    static void OnVtToEcuMessage(const isobus::CANMessage& message, void* parentPointer);
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
