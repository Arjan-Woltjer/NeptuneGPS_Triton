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
    auto partnerVT = std::make_shared<PartneredControlFunction>(0, vtNameFilters);

    vtClient = std::make_shared<VirtualTerminalClient>(partnerVT, controlFunction);
    vtClient->set_object_pool(0, VT3PoolData, VT3PoolSize, "MW01");
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

    vtClient->send_change_numeric_value(Var_Position, static_cast<uint32_t>(implement->GetPosition()));
    vtClient->send_change_numeric_value(Var_Setpoint, static_cast<uint32_t>(implement->GetSetpoint()));
    // XTE is signed (cm); bias by +1000 so the uint32 variable stays non-negative
    vtClient->send_change_numeric_value(Var_XTE, static_cast<uint32_t>(guidance->GetXte() + 1000));
    vtClient->send_change_numeric_value(Var_Offset, static_cast<uint32_t>(implement->GetOffset()));
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
