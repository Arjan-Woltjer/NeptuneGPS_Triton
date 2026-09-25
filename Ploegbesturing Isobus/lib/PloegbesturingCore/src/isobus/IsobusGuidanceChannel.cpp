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

// Specific headers for what the body uses beyond the class declaration; see
// the header for why the AgIsoStack.hpp umbrella is avoided, and for why the
// min()/max() macros are parked around them.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <can_hardware_interface_single_thread.hpp>
#include <can_network_manager.hpp>
#include <can_parameter_group_number_request_protocol.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "GuidanceCommit.hpp"
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
    // ---- Our ISOBUS identity ----------------------------------------
    //
    // Confirmed on the bus 2026-09-09: every terminal we join sees us as
    // manufacturer 1407 = **Open-Agriculture** (Gescher, Germany), which is
    // AgIsoStack's own code, permitted for non-commercial use and used by
    // their reference examples. There is **no MeijWorks entry** in the AEF
    // manufacturer registry -- checked against
    // NeptuneGPS Documentation/ISOBUS/reference/parameters-csv/Manufacturer IDs.csv,
    // database version 2026090801.
    //
    // Keeping 1407 is a deliberate holding position, not an oversight: it is a
    // real, allocated code rather than the 64 this used to claim, which belongs
    // to a different manufacturer. Shipping commercially needs a MeijWorks code
    // from the AEF. Tracked as an issue rather than fixed here, because
    // changing the NAME changes how every terminal caches our pools.
    //
    // The identity number is the part that is actually wrong -- see below.
    // Named so the two values that define who we are on the bus are visible
    // in one place rather than buried as literals in a setter chain.
    constexpr std::uint16_t kIsobusManufacturerCode = 1407;  // Open-Agriculture
    constexpr std::uint32_t kIsobusIdentityNumber   = 1;     // see the note below

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
    deviceName.set_manufacturer_code(kIsobusManufacturerCode);
    // **Every Triton claims identity 1.** The identity number is the serial
    // number field of the NAME, and NAME is what makes a control function
    // unique on the bus: two Ploegbesturing units on one ISOBUS would present
    // byte-identical NAMEs. We do set arbitrary-address-capable, so they would
    // not deadlock over an address, but terminals key their cached VT object
    // pools and DDOPs on NAME, so two units would fight over one cache entry.
    //
    // Deliberately NOT changed here. The NAME is an input to that caching, and
    // altering it mid-investigation would invalidate every terminal's stored
    // pool and add a variable to the #21 hunt -- the same class of confound
    // that the stale MW03 label produced in session 9. Derive it from the
    // Teensy's OCOTP serial in its own change, with a rig session to confirm
    // the re-upload behaves.
    deviceName.set_identity_number(kIsobusIdentityNumber);
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
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnGuidanceMachineInfo, OnGuidanceMachineInfo, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnPositionDeltaNmea2000, OnPositionDeltaNmea2000, this);

    // PGN 129029 is 43 bytes, so it arrives as NMEA2000 Fast Packet rather
    // than as a single frame and the ordinary PGN callback never sees it. It
    // needs the fast-packet protocol's own registration, plus
    // allow_any_control_function() -- these are global broadcasts from a
    // receiver we have not partnered with, and without that call the protocol
    // only reassembles messages addressed to one of our internal control
    // functions.
    auto& fastPacket = CANNetworkManager::CANNetwork.get_fast_packet_protocol(0);
    fastPacket->allow_any_control_function(true);
    fastPacket->register_multipacket_message_callback(kPgnGnssPositionData, OnGnssPositionData, this);

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
// Byte-level decode for every PGN below lives in IsobusPgnDecode.cpp and
// the commit rules in GuidanceCommit.hpp, both shared with
// CanFrameGuidanceChannel and both native-testable in isolation. These
// callbacks count, keep their diagnostics, extract data/length/source
// address from the real isobus::CANMessage, call the matching Decode*(), and
// hand the result to GApply() -- so this build and a directly attached bus
// commit identically, and a rule changes in one place (#98 phase 3).
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnPositionNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.positionNmea2000++;

    const auto& d = msg.get_data();
    GApply(DecodePositionNmea2000(d.data(), static_cast<uint8_t>(msg.get_data_length())), self->guidance);
}

void IsobusGuidanceChannel::OnSpeedNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.speedNmea2000++;

    // COG travels in the same frame as SOG and was once read past (#37).
    const auto& d = msg.get_data();
    GApply(DecodeSpeedNmea2000(d.data(), static_cast<uint8_t>(msg.get_data_length())), self->guidance);
}

void IsobusGuidanceChannel::OnXteNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.xteNmea2000++;

    const auto& d = msg.get_data();
    GApply(DecodeXteNmea2000(d.data(), static_cast<uint8_t>(msg.get_data_length())), self->guidance);
}

void IsobusGuidanceChannel::OnLegacyPosition(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.positionLegacy++;

    const auto& d = msg.get_data();
    GApply(DecodeLegacyPosition(d.data(), static_cast<uint8_t>(msg.get_data_length())), self->guidance);
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
    // This PGN is course + speed + altitude. Only speed used to be committed,
    // so GetCourse()/GetAltitude() read a permanent 0.0 on the ISOBUS build
    // while the values sat decoded on the bus (#37).
    GApply(result, self->guidance);
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
    if (result.lengthOk) {
        for (uint8_t i = 0; i < 8; i++) {
            self->counters.lastXteJohnDeereLegacyPayload[i] = result.rawPayload[i];
        }
        self->counters.lastXteJohnDeereLegacyPayloadMs = millis();
    }

    GApply(result, self->guidance);
}

// The standard ISO 11783-7 guidance channel, broadcast at 10 Hz by the tractor
// ECU on every rig captured so far without any Task Controller session.
//
// Read for diagnostics only. It carries estimated *curvature*, not cross-track
// error, and committing it to GuidanceSource would be the same category error
// the design doc warns about for DDI 513 -- a plough nulling a quantity
// measured somewhere else, with nothing to show that anything changed.
//
// Its value today is the status fields. Session 9's CNH tractor reported
// MechanicalSystemLockout = "locked out" for all 8420 frames, which explains
// why nothing guidance-related could happen on that rig and which no other
// message on that bus stated.
// PGN 129027 -- position deltas between absolute fixes. Counted and exposed,
// but deliberately NOT applied to the stored position and NOT used to refresh
// the fix age: applying deltas is stateful, and the fix age gates
// InterfacePlough's HOLD watchdog. Neither belongs in a change that has never
// seen this PGN on a real bus.
void IsobusGuidanceChannel::OnPositionDeltaNmea2000(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.positionDeltaNmea2000++;
}

// PGN 129029 -- the comprehensive GNSS message, and the reason this exists:
// **it is the only message on any bus captured so far that carries a GNSS
// quality indicator.** Without it, an ISOBUS build can only get quality from
// the John Deere legacy XTE decoder (source address 0x2A) or the Trimble one
// (0xAA). On a rig with neither -- the Ag Leader/CNH of session 9 -- quality
// is structurally stuck at 0, IsRtkQuality() can never become true, and
// InterfacePlough stays in HOLD no matter what else works.
void IsobusGuidanceChannel::OnGnssPositionData(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.gnssPositionData++;

    const auto& d = msg.get_data();
    auto result = DecodeGnssPositionData(d.data(), static_cast<std::uint8_t>(msg.get_data_length()));
    if (!result.lengthOk) return;

    self->counters.lastGnssMethod     = result.method;
    self->counters.lastGnssSvCount    = result.numberOfSvs;
    self->counters.lastGnssHasHdop    = result.hasHdop;
    self->counters.lastGnssHdop       = result.hdop;
    self->counters.lastGnssPositionMs = millis();

    // Same split as the other position decoders: the fix drives the staleness
    // watchdog the control path gates on, the coordinates are diagnostics, and
    // a coordinate failing its plausibility check must never cost us a fix.
    self->guidance->NoteGgaFixReceived();
    if (result.hasCoordinates) {
        self->guidance->SetPosition(result.latitude, result.longitude);
    }
    if (result.hasAltitude) {
        self->guidance->SetAltitude(result.altitudeMeters);
    }
    // The point of the whole message. Field 8's encoding matches NMEA 0183 GGA
    // for 0-5, so it needs no translation: 4 is the same RTK-fixed that
    // IsRtkQuality() tests for.
    if (result.hasQuality) {
        self->guidance->SetQuality(result.method);
    }
}

void IsobusGuidanceChannel::OnGuidanceMachineInfo(const CANMessage& msg, void* context) {
    auto* self = static_cast<IsobusGuidanceChannel*>(context);
    self->counters.guidanceMachineInfo++;

    const auto& d = msg.get_data();
    auto result = DecodeGuidanceMachineInfo(d.data(), static_cast<std::uint8_t>(msg.get_data_length()));
    if (!result.lengthOk) return;

    self->counters.lastGuidanceMechanicalLockout = result.mechanicalLockout;
    self->counters.lastGuidanceSteeringReadiness = result.steeringReadiness;
    self->counters.lastGuidanceRemoteEngage      = result.remoteEngageSwitch;
    self->counters.lastGuidanceHasCurvature      = result.hasCurvature;
    self->counters.lastGuidanceCurvaturePerKm    = result.curvaturePerKm;
    self->counters.lastGuidanceMachineInfoMs     = millis();
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
    GApply(DecodeLegacyXteTrimble(sourceAddress, d.data(), static_cast<uint8_t>(msg.get_data_length())), self->guidance);
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
