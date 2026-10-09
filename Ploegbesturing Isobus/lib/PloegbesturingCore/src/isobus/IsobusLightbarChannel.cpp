/*
  IsobusLightbarChannel - a second ISOBUS control function that presents as
  an Ag Leader L160 lightbar and reads the InCommand's cross-track error
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
#include "IsobusLightbarChannel.hpp"

#ifdef ISOBUS

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/isobus/can_network_manager.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "GuidanceCommit.hpp"

using namespace isobus;

namespace triton
{

namespace {
// The L160's NAME on the 2026-10-08 capture: 0x200083000C21887E. Industry
// group 2, device class 0, function 131 (Position Control in that class),
// manufacturer 97 (Ag Leader), identity 100478, arbitrary-address capable
// was NOT set (bit 63 clear) -- but we set it, because a second unit of ours
// on the same bus must not deadlock over 0xDC.
constexpr std::uint16_t kAgLeaderManufacturerCode = 97;
constexpr std::uint8_t  kLightbarFunctionCode     = 131;
constexpr std::uint32_t kLightbarIdentityNumber   = 100478;
constexpr std::uint8_t  kLightbarPreferredAddress = 0xDC;
}  // namespace

IsobusLightbarChannel::IsobusLightbarChannel(Stream* serialDebug, GuidanceSource* guidance, std::uint8_t canPort)
    : serialDebug(serialDebug), guidance(guidance), canPort(canPort) {
}

void IsobusLightbarChannel::Begin() {
    NAME lightbarName(0);
    lightbarName.set_arbitrary_address_capable(true);
    lightbarName.set_industry_group(2);
    lightbarName.set_device_class(0);
    lightbarName.set_function_code(kLightbarFunctionCode);
    lightbarName.set_manufacturer_code(kAgLeaderManufacturerCode);
    lightbarName.set_identity_number(kLightbarIdentityNumber);
    lightbarName.set_ecu_instance(0);
    lightbarName.set_function_instance(0);
    lightbarName.set_device_class_instance(0);

    // Through the factory, like the guidance channel's: only that registers
    // the control function for the stack's address-claim state machine.
    controlFunction = CANNetworkManager::CANNetwork.create_internal_control_function(lightbarName, canPort, kLightbarPreferredAddress);

    // Addressed traffic: the display's identification reads and its one
    // Proprietary A frame. Global: the cross-track error. All three go through
    // the "any control function" callback, which also fires for messages
    // addressed to one of our own control functions; the addressed ones are
    // filtered on destination in the callback.
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(AgLeaderLightbarEmulation::kPgnDiagnostic,   OnDiagnostic,   this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(AgLeaderLightbarEmulation::kPgnProprietaryA, OnProprietaryA, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteAgLeaderLightbar,                     OnXte,          this);

    serialDebug->println("Lightbar: presenting as an Ag Leader L160, claiming 0xDC");
}

bool IsobusLightbarChannel::IsClaimed() const {
    return controlFunction != nullptr && controlFunction->get_address_valid();
}

std::uint8_t IsobusLightbarChannel::GetAddress() const {
    return IsClaimed() ? controlFunction->get_address() : 0xFF;
}

void IsobusLightbarChannel::Update() {
    if (!IsClaimed()) return;
    const unsigned long now = millis();

    if (!claimAnnounced) {
        claimedAddress = controlFunction->get_address();
        claimAnnounced = true;
        serialDebug->print("Lightbar: address claimed: 0x");
        serialDebug->println(claimedAddress, HEX);
    }

    // The L160 broadcasts its software identification right after its claim
    // (52 bytes, so the stack sends it as a BAM).
    if (!counters.softwareIdSent) {
        std::uint8_t length = 0;
        const std::uint8_t* id = AgLeaderLightbarEmulation::SoftwareIdentification(length);
        SendGlobal(AgLeaderLightbarEmulation::kPgnSoftwareIdentification, id, length);
        counters.softwareIdSent = true;
    }

    if (emulation.HeartbeatDue(now)) {
        static const std::uint8_t kAllOnes[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        SendGlobal(AgLeaderLightbarEmulation::kPgnHeartbeat, kAllOnes, 8);
        counters.heartbeats++;
    }

    AgLeaderLightbarEmulation::Outgoing outgoing;
    while (emulation.PopFrame(outgoing, now)) {
        Send(outgoing);
    }
}

bool IsobusLightbarChannel::IsAddressedToUs(const CANMessage& msg) const {
    return claimedAddress != 0xFF && msg.get_identifier().get_destination_address() == claimedAddress;
}

void IsobusLightbarChannel::Send(const AgLeaderLightbarEmulation::Outgoing& outgoing) {
    // The display's control function must be in the stack's table (it claimed
    // an address, so it is); without it a PDU1 frame cannot be addressed.
    auto destination = CANNetworkManager::CANNetwork.get_control_function(canPort, outgoing.destination);
    if (destination == nullptr || controlFunction == nullptr) {
        counters.sendFailures++;
        return;
    }
    const bool sent = CANNetworkManager::CANNetwork.send_can_message(outgoing.pgn, outgoing.frame.data, outgoing.frame.length,
                                                                     controlFunction, destination);
    if (sent) counters.framesSent++;
    else      counters.sendFailures++;
}

void IsobusLightbarChannel::SendGlobal(std::uint32_t pgn, const std::uint8_t* data, std::uint32_t length) {
    if (controlFunction == nullptr) {
        counters.sendFailures++;
        return;
    }
    const bool sent = CANNetworkManager::CANNetwork.send_can_message(pgn, data, length, controlFunction, nullptr);
    if (sent) counters.framesSent++;
    else      counters.sendFailures++;
}

// ------------------------------------------------------------------
// Callbacks -- void* context is always `this`.
// ------------------------------------------------------------------
void IsobusLightbarChannel::OnDiagnostic(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusLightbarChannel*>(context);
    if (!self->IsAddressedToUs(msg)) return;
    self->counters.diagnosticFrames++;
    const auto& d = msg.get_data();
    self->emulation.OnDiagnosticFrame(msg.get_identifier().get_source_address(), d.data(),
                                      static_cast<std::uint8_t>(msg.get_data_length()), millis());
}

void IsobusLightbarChannel::OnProprietaryA(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusLightbarChannel*>(context);
    if (!self->IsAddressedToUs(msg)) return;
    self->emulation.OnProprietaryA(msg.get_identifier().get_source_address());
}

// PGN 65462 is proprietary B, so it could in principle be anyone's; the
// decoder's structure checks (engaged 1/2, line flag 1/4, a magnitude under
// 300 m) are what keep another manufacturer's use of the same number out.
void IsobusLightbarChannel::OnXte(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusLightbarChannel*>(context);
    self->counters.xteFrames++;
    self->counters.lastXteSourceAddress = msg.get_identifier().get_source_address();
    self->counters.lastXteMs = millis();

    const auto& d = msg.get_data();
    auto result = DecodeXteAgLeaderLightbar(d.data(), static_cast<std::uint8_t>(msg.get_data_length()));
    self->counters.lastXte = result;
    if (GApply(result, self->guidance)) self->counters.xteCommitted++;
}

}  // namespace triton

#endif  // ISOBUS
