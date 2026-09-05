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
#include "IsobusGuidanceChannel.hpp"

// The header is itself empty unless ISOBUS is defined (see its own comment) --
// guard the body too, so this compiles to an empty translation unit instead
// of failing on undeclared isobus:: symbols when PlatformIO's LDF pulls this
// file in on teensy41_serial anyway.
#ifdef ISOBUS

#include "IsobusPgnDecode.hpp"

using namespace isobus;

namespace triton
{

// ------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------
IsobusGuidanceChannel::IsobusGuidanceChannel(Stream* serialDebug, 
                                             std::shared_ptr<isobus::CANHardwarePlugin> canPlugin, 
                                             GuidanceSource* guidance,
                                             ImplementPlough* implement)
                                           : serialDebug(serialDebug),
                                             guidance(guidance),
                                             implement(implement),
                                             canPlugin(canPlugin) {
}

// ------------------------------------------------------------------
// Begin -- call once from setup()
// ------------------------------------------------------------------
void IsobusGuidanceChannel::Begin() {
    CANHardwareInterface::set_number_of_can_channels(1);
    CANHardwareInterface::assign_can_channel_frame_handler(0, canPlugin);
    CANHardwareInterface::start();

    CANHardwareInterface::update();

    // ISOBUS NAME
    // TODO: request an official manufacturer code from the ISOBUS foundation
    // for MeijWorks; placeholder values below carried forward from the prior
    // Ploeg ISOBUS prototype, which never resolved them either.
    NAME deviceName(0);
    deviceName.set_arbitrary_address_capable(true);
    deviceName.set_industry_group(2);   // Agriculture and Forestry
    deviceName.set_device_class(8);     // Non-self-propelled work machine
    deviceName.set_function_code(static_cast<uint8_t>(NAME::Function::SteeringControl));  // TODO: confirm correct function
    // Was 64 -- belongs to a real, different manufacturer (flagged as a TODO
    // in Triton_TC_Client_Design.md sec 7, never actually fixed until now).
    // 1407 is AgIsoStack's own permitted-for-non-commercial-use code, the
    // same one their reference examples (VirtualTerminal.ino, SpeedMessages.ino)
    // use. Tested 2026-08-10 at van Mastwijk as a candidate for the VT pool
    // rejection that's persisted across every pool-CONTENT variant tried
    // against both Fendt and CNH -- a real, unfixed identity-level bug is a
    // more promising remaining variable than more pool-byte bisection.
    deviceName.set_manufacturer_code(1407);
    deviceName.set_identity_number(1);
    deviceName.set_ecu_instance(0);
    deviceName.set_function_instance(0);
    deviceName.set_device_class_instance(0);

    // Must go through the factory method, not a direct std::make_shared
    // construction -- only create_internal_control_function() registers the
    // control function into CANNetworkManager's internalControlFunctions
    // list, and only members of that list ever get their
    // update_address_claiming() state machine driven (from
    // CANNetworkManager::update(), called transitively by
    // CANHardwareInterface::update() below). A directly-constructed
    // InternalControlFunction never leaves State::None, so
    // get_address_valid() can never become true -- confirmed on hardware
    // 2026-08-04 as the actual cause of an infinite block here, not a timing
    // issue. Matches AgIsoStack-Arduino's own reference examples
    // (examples/SpeedMessages, examples/VirtualTerminal), which all use this
    // factory method.
    controlFunction = CANNetworkManager::CANNetwork.create_internal_control_function(deviceName, 0, 0x81);

    serialDebug->print("IsobusGuidanceChannel: claiming address ");
    // J1939 address claim takes at least 250 ms; block until our address is
    // confirmed before sending PGN requests so the source address is valid
    // in the outgoing frame. One-time startup cost, not called from loop().
    while (!controlFunction->get_address_valid())
        CANHardwareInterface::update();

    serialDebug->print("address claimed: 0x");
    serialDebug->println(controlFunction->get_address(), HEX);

    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnPositionNmea2000,   OnPositionNmea2000,   this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnSpeedNmea2000,      OnSpeedNmea2000,      this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteNmea2000,        OnXteNmea2000,        this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnPositionLegacy,     OnLegacyPosition,     this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnSpeedLegacy,        OnLegacySpeed,        this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteJohnDeereLegacy, OnLegacyXteJohnDeere, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteTrimbleLegacy,   OnLegacyXteTrimble,   this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnAllImplementStop,   OnAllImplementStop,   this);

    // Trigger an immediate first transmission from whatever's on the bus;
    // the reference Fendt 6240 then continues broadcasting on its own
    // schedule (~10 Hz) without needing a repetition-rate request.
    RequestGuidancePgns();
    lastPgnRetryMs = millis();
}

// ------------------------------------------------------------------
// Update -- call every loop() iteration
// ------------------------------------------------------------------
void IsobusGuidanceChannel::Update() {
    CANHardwareInterface::update();

    // Legacy GPS units aren't always listening yet the moment Begin()'s
    // one-shot request goes out (their own power-on race), and a single
    // request frame can simply get lost -- so keep re-requesting whichever
    // PGN families haven't produced a single message yet, every
    // kPgnRetryIntervalMs, until each of position/speed/XTE has. Any
    // variant (NMEA2000 or legacy) counting as a hit is enough to stop
    // retrying that family; decode-level correctness (address filters,
    // scale factors) is a separate concern a repeated request can't fix.
    bool havePosition = (counters.positionNmea2000 > 0) || (counters.positionLegacy > 0);
    bool haveSpeed    = (counters.speedNmea2000 > 0)    || (counters.speedLegacy > 0);
    bool haveXte      = (counters.xteNmea2000 > 0)      || (counters.xteJohnDeereLegacy > 0) || (counters.xteTrimbleLegacy > 0);

    if (!havePosition || !haveSpeed || !haveXte) {
        unsigned long now = millis();
        if (now - lastPgnRetryMs >= kPgnRetryIntervalMs) {
            lastPgnRetryMs = now;
            RequestGuidancePgns();
        }
    }
}

// ------------------------------------------------------------------
// Sends one PGN request per guidance PGN this channel consumes. Called
// once from Begin() and then repeated from Update() (see its comment)
// until every family has produced at least one message.
// ------------------------------------------------------------------
void IsobusGuidanceChannel::RequestGuidancePgns() {
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnPositionNmea2000,   controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnSpeedNmea2000,      controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnXteNmea2000,        controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnPositionLegacy,     controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnSpeedLegacy,        controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnXteJohnDeereLegacy, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnXteTrimbleLegacy,   controlFunction, nullptr);
}

// ------------------------------------------------------------------
// Byte-level decode for every PGN below now lives in IsobusPgnDecode.cpp,
// native-testable in isolation (see test_IsobusPgnDecode.cpp) -- these
// callbacks just extract data/length/source-address from the real
// isobus::CANMessage, call the matching Decode*(), and apply the result.
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnPositionNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.positionNmea2000++;

    const auto& d = msg.get_data();
    auto result = DecodePositionNmea2000(d.data(), static_cast<uint8_t>(msg.get_data_length()));
    if (result.fixPresent) self->guidance->NoteGgaFixReceived();
}

void IsobusGuidanceChannel::OnSpeedNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.speedNmea2000++;

    const auto& d = msg.get_data();
    auto result = DecodeSpeedNmea2000(d.data(), static_cast<uint8_t>(msg.get_data_length()));
    if (result.valid) self->guidance->SetSpeedKnots(result.speedKnots);
}

void IsobusGuidanceChannel::OnXteNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.xteNmea2000++;

    const auto& d = msg.get_data();
    auto result = DecodeXteNmea2000(d.data(), static_cast<uint8_t>(msg.get_data_length()));
    if (result.valid) self->guidance->SetXte(result.xteHundredthsMeter);
}

void IsobusGuidanceChannel::OnLegacyPosition(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.positionLegacy++;

    const auto& d = msg.get_data();
    auto result = DecodeLegacyPosition(d.data(), static_cast<uint8_t>(msg.get_data_length()));
    if (result.fixPresent) self->guidance->NoteGgaFixReceived();
}

void IsobusGuidanceChannel::OnLegacySpeed(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.speedLegacy++;

    if (msg.get_data_length() != 8) return;
    const auto& d = msg.get_data();

    // Read the address straight off the CAN identifier, not via
    // get_source_control_function() -- confirmed on hardware 2026-08-08
    // that this legacy sender never broadcasts a real ISO Address Claim
    // (PGN 60928), so AgIsoStack's control-function table never resolves an
    // entry for it and that accessor is permanently nullptr for this
    // device, even while the PGN counter climbs at full rate.
    self->counters.lastSpeedLegacySourceAddress = msg.get_identifier().get_source_address();

    auto result = DecodeLegacySpeed(d.data(), static_cast<uint8_t>(msg.get_data_length()));
    self->counters.lastSpeedLegacyRaw = result.rawValue;
    if (result.valid) self->guidance->SetSpeedKnots(result.speedKnots);
}

void IsobusGuidanceChannel::OnLegacyXteJohnDeere(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.xteJohnDeereLegacy++;

    if (msg.get_data_length() != 8) return;

    // PGN 0xFFFF is a heavily-overloaded manufacturer-proprietary PGN --
    // add_any_control_function_parameter_group_number_callback dispatches by
    // PGN alone, so the sender's source address must be rechecked here to
    // replicate the legacy exact-CAN-ID filter's actual specificity. Read
    // the address straight off the CAN identifier, not via
    // get_source_control_function() -- confirmed on hardware 2026-08-08
    // that this legacy sender never broadcasts a real ISO Address Claim
    // (PGN 60928), so AgIsoStack's control-function table never resolves an
    // entry for it and that accessor was permanently nullptr here, silently
    // dropping every one of these messages regardless of the address filter
    // value.
    std::uint8_t sourceAddress = msg.get_identifier().get_source_address();
    self->counters.lastXteJohnDeereLegacySourceAddress = sourceAddress;

    const auto& d = msg.get_data();
    auto result = DecodeLegacyXteJohnDeere(sourceAddress, d.data(), static_cast<uint8_t>(msg.get_data_length()));

    // Raw diagnostics are stored whenever the decoder got far enough to read
    // them, independent of `valid` -- matching IsobusPgnDecode.hpp's stated
    // convention, and load-bearing for GitHub issue #20: an Ag Leader/Raven
    // sender (SA 0x80) is deliberately not decoded any more, but its raw
    // bytes are exactly what a future capture needs to derive its real
    // layout, so they must still reach IsobusDebugMenu's readout.
    self->counters.lastXteJohnDeereLegacyRawWord  = result.rawWord;
    self->counters.lastXteJohnDeereLegacyRawByte1 = result.rawByte1;

    if (!result.valid) return;
    self->guidance->SetXte(result.xteHundredthsMeter, result.quality);
}

void IsobusGuidanceChannel::OnLegacyXteTrimble(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.xteTrimbleLegacy++;

    if (msg.get_data_length() != 8) return;

    // Read the sender's address straight off the CAN identifier, not via
    // get_source_control_function() -- confirmed on hardware 2026-08-08
    // that legacy senders on this PGN family never broadcast a real ISO
    // Address Claim (PGN 60928), so AgIsoStack's control-function table
    // never resolves an entry for them and that accessor was permanently
    // nullptr here.
    std::uint8_t sourceAddress = msg.get_identifier().get_source_address();
    self->counters.lastXteTrimbleLegacySourceAddress = sourceAddress;

    const auto& d = msg.get_data();
    auto result = DecodeLegacyXteTrimble(sourceAddress, d.data(), static_cast<uint8_t>(msg.get_data_length()));
    if (result.valid) self->guidance->SetXte(result.xteHundredthsMeter, result.quality);
}

void IsobusGuidanceChannel::OnAllImplementStop(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.allImplementStop++;

    const auto& d = msg.get_data();
    auto result = DecodeAllImplementStop(d.data(), static_cast<uint8_t>(msg.get_data_length()));
    if (!result.lengthOk) return;

    self->counters.lastAllImplementStopState = result.state;
    if (result.stopRequested) {
        self->counters.lastAllImplementStopMs = millis();
        self->implement->Stop();
    }
}

}  // namespace triton

#endif  // ISOBUS
