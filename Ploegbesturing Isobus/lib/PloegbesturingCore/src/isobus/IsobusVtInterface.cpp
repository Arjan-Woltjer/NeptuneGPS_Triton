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
    CANStackLogger::set_log_level(CANStackLogger::LoggingLevel::Warning);

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
    auto partnerVT = CANNetworkManager::CANNetwork.create_partnered_control_function(0, vtNameFilters);

    vtClient = std::make_shared<VirtualTerminalClient>(partnerVT, controlFunction);
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
}

// ----------------------------------------------------------------
// Private helpers
// ----------------------------------------------------------------
void IsobusVtInterface::updateVtVariables() {
    if (!vtClient->get_is_connected()) return;

    const int32_t position = static_cast<int32_t>(implement->GetPosition());
    const int32_t setpoint = static_cast<int32_t>(implement->GetSetpoint());
    // XTE is signed (cm); bias by +1000 so the uint32 variable stays non-negative
    const int32_t xte      = static_cast<int32_t>(guidance->GetXte() + 1000);
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
// Documentation/AgIsoStackVendorPatches.md. Order matches
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

void IsobusVtInterface::onVtKeyEvent(const VirtualTerminalClient::VTKeyEvent& event) {
    if (event.keyEvent != VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased) return;

    // Display/telemetry only for now -- see the class comment in
    // IsobusVtInterface.hpp for why these don't drive ImplementPlough yet.
    switch (event.objectID) {
        case Key_Wider:    serialDebug->println("VT: Wider pressed (not wired to control)");    break;
        case Key_Narrower: serialDebug->println("VT: Narrower pressed (not wired to control)"); break;
        case Key_Auto:     serialDebug->println("VT: Auto pressed (not wired to control)");     break;
        default: break;
    }
}

}  // namespace triton

#endif  // ISOBUS
