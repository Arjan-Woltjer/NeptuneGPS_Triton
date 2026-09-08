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
#include "IsobusVtInterface.hpp"

// The header is itself empty unless ISOBUS is defined (see its own comment) --
// guard the body too, so this compiles to an empty translation unit instead
// of failing on undeclared isobus:: symbols when PlatformIO's LDF pulls this
// file in on teensy41_serial anyway.
#ifdef ISOBUS

#include "VTObjectPool.hpp"

using namespace isobus;

namespace triton
{

// ----------------------------------------------------------------
// Logger
// ----------------------------------------------------------------
void IsobusVtInterface::Logger::sink_CAN_stack_log(CANStackLogger::LoggingLevel level, const std::string& text) {
    switch (level) {
        case LoggingLevel::Debug:    Serial.print('D'); break;
        case LoggingLevel::Info:     Serial.print('I'); break;
        case LoggingLevel::Warning:  Serial.print('W'); break;
        case LoggingLevel::Error:    Serial.print('E'); break;
        case LoggingLevel::Critical: Serial.print('!'); break;
    }
    Serial.print("] ");
    Serial.println(text.c_str());
}

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------
IsobusVtInterface::IsobusVtInterface(Stream* serialDebug, ImplementPlough* implement, GuidanceSource* guidance,
                                      std::shared_ptr<InternalControlFunction> controlFunction)
    : serialDebug(serialDebug), implement(implement), guidance(guidance), controlFunction(controlFunction) {
}

// ----------------------------------------------------------------
// Begin -- call once from setup(), after address claim
// ----------------------------------------------------------------
void IsobusVtInterface::Begin() {
    BuildObjectPool();

    CANStackLogger::set_can_stack_logger_sink(&logger);
    // Bumped Warning -> Info 2026-09-05: at Warning, AgIsoStack's own [NM]
    // control-function lifecycle lines (address claims, "is now offline")
    // are invisible on the serial console -- confirmed by re-reading Session
    // 5's captured logs, which contain zero [NM] lines despite a control-
    // function eviction being the leading theory for GitHub issue #17's VT
    // Status Timeout (see NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md patch #3
    // and HardwareTestNotes.md Session 5). Info is the cheapest way to see
    // these events directly instead of inferring them.
    CANStackLogger::set_log_level(CANStackLogger::LoggingLevel::Info);

    diagnostics = std::make_shared<DiagnosticProtocol>(controlFunction);
    diagnostics->initialize();
    diagnostics->set_product_identification_brand("MeijWorks");
    diagnostics->set_product_identification_model("Ploegbesturing Isobus");
    diagnostics->set_product_identification_code("001");
    diagnostics->set_software_id_field(0, "0.1.0");

    const NAMEFilter vtFilter(NAME::NAMEParameters::FunctionCode, static_cast<uint8_t>(NAME::Function::VirtualTerminal));
    const std::vector<NAMEFilter> vtNameFilters = { vtFilter };
    // Must go through the factory method, not a direct std::make_shared
    // construction -- same class of bug as IsobusGuidanceChannel::Begin()'s
    // create_internal_control_function() comment. Only
    // create_partnered_control_function() registers the object into
    // CANNetworkManager's partneredControlFunctions list, and only members
    // of that list are ever checked against an incoming Address Claim frame
    // (can_network_manager.cpp's update_control_functions()) -- so a directly
    // constructed partner can NEVER become address-valid, no matter how long
    // a real VT sits on the bus. Confirmed on hardware 2026-08-10: VT stuck
    // at state 0/22 "Disconnected" indefinitely against a live Fendt
    // Universal Terminal. AgIsoStack's own header (can_control_function.hpp)
    // and its VirtualTerminal.ino example both use the factory form.
    // Kept as a member (not a local) so IsPartnerAddressValid()/
    // GetPartnerAddress() can surface it -- see their comment in the header.
    partner = CANNetworkManager::CANNetwork.create_partnered_control_function(0, vtNameFilters);

    vtClient = std::make_shared<VirtualTerminalClient>(partner, controlFunction);
    // Bumped MW01 -> MW02, 2026-08-10 (van Mastwijk): the pool's structure
    // genuinely changed (WorkingSet gained a real child object, see
    // VTObjectPool.cpp's appendWorkingSet() comment for the confirmed bug
    // this fixes) -- same discipline as the TC DDOP's TC01->TC02 bump in
    // Session 3, so no terminal that cached a pool under "MW01" confuses it
    // with this one.
    // Bumped MW02 -> MW03, 2026-08-10 (desktop, same day): WorkingSet's
    // child changed from a placeholder OutputString to a real PictureGraphic
    // icon (Icon_Plough) -- see VTObjectPool.cpp's appendPictureGraphic()
    // call site and GitHub issue #14.
    vtClient->set_object_pool(0, VT3PoolData, VT3PoolSize, "MW03");
    softKeyListener = vtClient->get_vt_soft_key_event_dispatcher().add_listener(
        [this](const VirtualTerminalClient::VTKeyEvent& e) { onVtKeyEvent(e); });
    buttonListener = vtClient->get_vt_button_event_dispatcher().add_listener(
        [this](const VirtualTerminalClient::VTKeyEvent& e) { onVtKeyEvent(e); });
    vtClient->initialize(false);

    // See the header's GetVtStatusMessageCount()/GetVtStatusMessageAgeMs()
    // comment -- independent raw counter for the VT's own periodic status
    // broadcast, alongside (not replacing) vtClient's internal tracking.
    CANNetworkManager::CANNetwork.add_global_parameter_group_number_callback(
        static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU), OnVtToEcuMessage, this);
}

// ----------------------------------------------------------------
// Update -- call every loop() iteration
// ----------------------------------------------------------------
void IsobusVtInterface::Update() {
    if (millis() - lastVtUpdate >= 100) {
        lastVtUpdate = millis();
        updateVtVariables();
    }

    diagnostics->update();
    vtClient->update();
    updateReconnectWatchdog();
}

// ----------------------------------------------------------------
// Reconnect watchdog -- see the header's reconnect fields for the rationale
// and for why initialize() alone is not enough. GitHub issue #18.
// ----------------------------------------------------------------
void IsobusVtInterface::updateReconnectWatchdog() {
    // How long a post-connection outage is tolerated before forcing a clean
    // re-attempt. Comfortably longer than AgIsoStack's own 3 s
    // VT_STATUS_TIMEOUT_MS plus a normal automatic re-handshake, so a
    // connection that is recovering on its own is never interrupted.
    constexpr unsigned long kReconnectAfterMs = 10000UL;
    // Minimum spacing between attempts, so a partner that is genuinely gone
    // produces one line every 10 s rather than a churning callback list.
    constexpr unsigned long kReconnectRetryIntervalMs = 10000UL;

    const unsigned long now = millis();

    if (IsConnected()) {
        hasEverConnected    = true;
        disconnectedSinceMs = 0;
        return;
    }

    // Never armed before the first successful connection: the initial
    // handshake has its own timing, and a watchdog firing during it would
    // interrupt a connection that was progressing normally.
    if (!hasEverConnected) return;

    if (disconnectedSinceMs == 0) {
        disconnectedSinceMs = now;
        return;
    }
    if (now - disconnectedSinceMs < kReconnectAfterMs) return;
    if (reconnectAttempts != 0 && (now - lastReconnectTryMs) < kReconnectRetryIntervalMs) return;

    lastReconnectTryMs = now;
    reconnectAttempts++;

    serialDebug->print("VT: disconnected ");
    serialDebug->print((now - disconnectedSinceMs) / 1000);
    serialDebug->print("s after having been connected -- forcing reconnect attempt ");
    serialDebug->print(reconnectAttempts);
    serialDebug->print(" (partner addr=0x");
    serialDebug->print(GetPartnerAddress(), HEX);
    serialDebug->print(" valid=");
    serialDebug->print(IsPartnerAddressValid() ? "Y" : "N");
    serialDebug->println(")");

    // terminate() then initialize(): see the header. terminate()'s
    // delete-object-pool branch only runs when Connected, which we are not,
    // so this is just a callback teardown and a state-machine reset.
    vtClient->terminate();
    vtClient->initialize(false);
}

// ----------------------------------------------------------------
// Private helpers
// ----------------------------------------------------------------
void IsobusVtInterface::updateVtVariables() {
    if (!vtClient->get_is_connected()) return;

    const int32_t position = static_cast<int32_t>(implement->GetPosition());
    const int32_t setpoint = static_cast<int32_t>(implement->GetSetpoint());
    // XTE is signed (cm); bias by +1000 so the uint32 variable stays
    // non-negative. That bias silently assumes +/-10 m, so enforce it rather
    // than implying it: an out-of-range value used to wrap through the
    // uint32_t cast below and render as a plausible-looking huge number.
    // Session 6 (2026-09-05) saw exactly that -- the VT displayed
    // 42949532.47, which decodes as (uint32)(-14049), i.e. a GetXte() of
    // -15049 from the then-broken Ag Leader decode (GitHub issue #20).
    // Clamping makes a bad reading peg visibly at the limit instead.
    constexpr int32_t kXteBias      = 1000;   // cm, = 10 m
    constexpr int32_t kXteBiasedMin = 0;      // -10 m or worse
    constexpr int32_t kXteBiasedMax = 2000;   // +10 m or worse
    int32_t xteBiased = static_cast<int32_t>(guidance->GetXte()) + kXteBias;
    if (xteBiased < kXteBiasedMin) xteBiased = kXteBiasedMin;
    if (xteBiased > kXteBiasedMax) xteBiased = kXteBiasedMax;
    const int32_t xte = xteBiased;
    const int32_t offset   = static_cast<int32_t>(implement->GetOffset());

    // See the header's comment on these fields for why this is on-change +
    // heartbeat rather than unconditional every-100ms.
    const bool heartbeatDue = (millis() - lastVtVariableHeartbeat >= 1000);
    const bool forceSend = !sentInitialVtVariables || heartbeatDue;

    if (forceSend || position != lastSentPosition) {
        vtClient->send_change_numeric_value(Var_Position, static_cast<uint32_t>(position));
        lastSentPosition = position;
    }
    if (forceSend || setpoint != lastSentSetpoint) {
        vtClient->send_change_numeric_value(Var_Setpoint, static_cast<uint32_t>(setpoint));
        lastSentSetpoint = setpoint;
    }
    if (forceSend || xte != lastSentXte) {
        vtClient->send_change_numeric_value(Var_XTE, static_cast<uint32_t>(xte));
        lastSentXte = xte;
    }
    if (forceSend || offset != lastSentOffset) {
        vtClient->send_change_numeric_value(Var_Offset, static_cast<uint32_t>(offset));
        lastSentOffset = offset;
    }

    sentInitialVtVariables = true;
    if (heartbeatDue) lastVtVariableHeartbeat = millis();
}

// ----------------------------------------------------------------
// State-machine step name/index -- see the header comment and
// NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md. Order matches
// isobus::VirtualTerminalClient::StateMachineState exactly (0-based);
// keep in sync if that enum changes.
// ----------------------------------------------------------------
namespace {
constexpr const char* kVtStateNames[] = {
    "Disconnected",
    "WaitForPartnerVTStatusMessage",
    "SendWorkingSetMasterMessage",
    "ReadyForObjectPool",
    "SendGetMemory",
    "WaitForGetMemoryResponse",
    "SendGetNumberSoftkeys",
    "WaitForGetNumberSoftKeysResponse",
    "SendGetTextFontData",
    "WaitForGetTextFontDataResponse",
    "SendGetHardware",
    "WaitForGetHardwareResponse",
    "SendGetVersions",
    "WaitForGetVersionsResponse",
    "SendStoreVersion",
    "WaitForStoreVersionResponse",
    "SendLoadVersion",
    "WaitForLoadVersionResponse",
    "UploadObjectPool",
    "SendEndOfObjectPool",
    "WaitForEndOfObjectPoolResponse",
    "Connected",
    "Failed",
};
constexpr int kVtStateCount = sizeof(kVtStateNames) / sizeof(kVtStateNames[0]);
}  // namespace

int IsobusVtInterface::GetStateStep() const {
    if (!vtClient) return 0;
    return static_cast<int>(vtClient->get_state());
}

int IsobusVtInterface::GetStateTotalSteps() const {
    return kVtStateCount - 1;  // Failed isn't a forward step, exclude it from "of N"
}

const char* IsobusVtInterface::GetStateName() const {
    if (!vtClient) return "(no client)";
    int index = static_cast<int>(vtClient->get_state());
    if (index < 0 || index >= kVtStateCount) return "(unknown)";
    return kVtStateNames[index];
}

const char* IsobusVtInterface::GetVtVersionName() const {
    if (!vtClient) return "(no client)";
    switch (vtClient->get_connected_vt_version()) {
        case VirtualTerminalClient::VTVersion::Version2OrOlder:   return "<=2";
        case VirtualTerminalClient::VTVersion::Version3:          return "3";
        case VirtualTerminalClient::VTVersion::Version4:          return "4";
        case VirtualTerminalClient::VTVersion::Version5:          return "5";
        case VirtualTerminalClient::VTVersion::Version6:          return "6";
        default:                                                  return "(unknown)";
    }
}

void IsobusVtInterface::OnVtToEcuMessage(const CANMessage& message, void* parentPointer) {
    if (parentPointer == nullptr || message.get_data_length() < 1) return;
    if (message.get_uint8_at(0) != static_cast<std::uint8_t>(VirtualTerminalClient::Function::VTStatusMessage)) return;

    IsobusVtInterface* self = static_cast<IsobusVtInterface*>(parentPointer);
    self->vtStatusMessageCount++;
    self->lastVtStatusMessageMs = millis();
}

void IsobusVtInterface::onVtKeyEvent(const VirtualTerminalClient::VTKeyEvent& event) {
    if (event.keyEvent != VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased) return;

    // Wider/Narrower/Calibrate set a consume-once pending flag, picked up by
    // main.cpp's loop() and passed into InterfacePlough::CheckButtons() --
    // see this class's own header comment and the ConsumeXxxPress() accessors
    // for the full rationale. Auto has no existing target (InterfacePlough's
    // AUTO mode is derived, not user-settable) so stays log-only.
    //
    // Calibrate is a heavier action than the other two and carries two real
    // caveats worth knowing before relying on it in the field:
    //   1. It is a single tap, where the cab-button equivalent is holding
    //      both buttons for delay1 * 4 = ~1 s. There is no hold gesture
    //      available over a VT soft key (the VT reports discrete press and
    //      release events, and this class acts on release), so an accidental
    //      tap enters the wizard immediately.
    //   2. CalibrationPlough::Calibrate() blocks main.cpp's loop() until the
    //      operator finishes or cancels, and it drives itself from the LCD
    //      and the physical buttons only -- it calls
    //      InterfacePlough::CheckButtons(0, 0) with the VT flags left at
    //      their defaults. So while the wizard is up, this class's Update()
    //      (and with it CANNetworkManager) is not being pumped: the VT
    //      connection will time out and have to be re-established by the
    //      reconnect watchdog afterwards, and the wizard itself cannot be
    //      driven from the VT. Triggering calibration from the VT is
    //      therefore a "start it, then finish it at the cab display"
    //      affordance, not full VT-side calibration -- see GitHub issue #31.
    switch (event.objectID) {
        case Key_Wider:     pendingWiderPress = true;     serialDebug->println("VT: Wider pressed");     break;
        case Key_Narrower:  pendingNarrowerPress = true;  serialDebug->println("VT: Narrower pressed");  break;
        case Key_Calibrate: pendingCalibratePress = true; serialDebug->println("VT: Calibrate pressed"); break;
        case Key_Auto:      serialDebug->println("VT: Auto pressed (not wired to control)");             break;
        default: break;
    }
}

}  // namespace triton

#endif  // ISOBUS
