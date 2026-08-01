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

using namespace isobus;

namespace triton
{

// ------------------------------------------------------------------
// PGN table
// ------------------------------------------------------------------
// Standard NMEA2000 (confirmed broadcast by the reference Fendt 6240):
static constexpr std::uint32_t kPgnPositionNmea2000 = 129025;  // Position, Rapid Update
static constexpr std::uint32_t kPgnSpeedNmea2000     = 129026;  // COG & SOG, Rapid Update
static constexpr std::uint32_t kPgnXteNmea2000       = 129283;  // Cross Track Error

// Legacy proprietary, ported from VehicleGps.cpp's CAN_POS_ID/CAN_SPD_ID/
// CAN_XTE_ID/CAN_XTE_ID2 (0x0CFEF31C/0x18FEF31C, 0x0CFEE81C/0x18FEE81C,
// 0x0CFFFF2A, 0x1CEBACAA) -- kept for backwards compatibility with equipment
// that only broadcasts these instead of the NMEA2000 set. PGN extracted from
// the 29-bit CAN ID (PF>=240 -> PDU2, PGN=(PF<<8)|PS; PF<240 -> PDU1,
// PGN=(PF<<8), PS is a destination address, not part of the PGN).
static constexpr std::uint32_t kPgnPositionLegacy    = 0xFEF3;  // 65267, PDU2
static constexpr std::uint32_t kPgnSpeedLegacy       = 0xFEE8;  // 65256, PDU2
static constexpr std::uint32_t kPgnXteJohnDeereLegacy = 0xFFFF; // 65535, PDU2 -- heavily overloaded
                                                                 // proprietary PGN, source address
                                                                 // 0x2A must be rechecked in the callback
static constexpr std::uint8_t  kSourceAddressJohnDeere = 0x2A;
static constexpr std::uint32_t kPgnXteTrimbleLegacy  = 0xEB00;  // 60160, PDU1 -- legacy filter required
                                                                 // destination address 0xAC (fixed); our
                                                                 // claimed SA is dynamic, so whether this
                                                                 // is actually delivered needs real-bus
                                                                 // verification (see plan's open risk)
static constexpr std::uint8_t  kSourceAddressTrimble = 0xAA;

// AISO (All Implement Stop Operations) -- DBC arbitration ID 2365391614 =
// 0x8CFD02FE with the SocketCAN EFF flag (0x80000000) masked off ->
// 0x0CFD02FE: PF=0xFD (253, PDU2) -> PGN=(0xFD<<8)|0x02=0xFD02=64770.
static constexpr std::uint32_t kPgnAllImplementStop = 0xFD02;  // 64770, PDU2

// ------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------
IsobusGuidanceChannel::IsobusGuidanceChannel(Stream* serialDebug, GuidanceSource* guidance, ImplementPlough* implement)
    : serialDebug(serialDebug), guidance(guidance), implement(implement),
      // Channel 2 = FlexCAN3 (Teensy pins 31 TX / 30 RX), matching the besturing 0.1
      // board's CAN bodge wire -- channel 0 (FlexCAN1, pins 22/23) has nothing
      // physically connected to it on this board.
      can0(std::make_shared<FlexCANT4Plugin>(2)) {
}

// ------------------------------------------------------------------
// Begin -- call once from setup()
// ------------------------------------------------------------------
void IsobusGuidanceChannel::Begin() {
    CANHardwareInterface::set_number_of_can_channels(1);
    CANHardwareInterface::assign_can_channel_frame_handler(0, can0);
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
    deviceName.set_function_code(
        static_cast<uint8_t>(NAME::Function::SteeringControl));  // TODO: confirm correct function
    deviceName.set_manufacturer_code(64);
    deviceName.set_identity_number(1);
    deviceName.set_ecu_instance(0);
    deviceName.set_function_instance(0);
    deviceName.set_device_class_instance(0);

    controlFunction = InternalControlFunction::create(deviceName, 0x81, 0);

    // J1939 address claim takes at least 250 ms; block until our address is
    // confirmed before sending PGN requests so the source address is valid
    // in the outgoing frame. One-time startup cost, not called from loop().
    while (!controlFunction->get_address_valid())
        CANHardwareInterface::update();

    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnPositionNmea2000, OnPositionNmea2000, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnSpeedNmea2000, OnSpeedNmea2000, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteNmea2000, OnXteNmea2000, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnPositionLegacy, OnLegacyPosition, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnSpeedLegacy, OnLegacySpeed, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteJohnDeereLegacy, OnLegacyXteJohnDeere, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnXteTrimbleLegacy, OnLegacyXteTrimble, this);
    CANNetworkManager::CANNetwork.add_any_control_function_parameter_group_number_callback(kPgnAllImplementStop, OnAllImplementStop, this);

    // Trigger an immediate first transmission from whatever's on the bus;
    // the reference Fendt 6240 then continues broadcasting on its own
    // schedule (~10 Hz) without needing a repetition-rate request.
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnPositionNmea2000, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnSpeedNmea2000, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnXteNmea2000, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnPositionLegacy, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnSpeedLegacy, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnXteJohnDeereLegacy, controlFunction, nullptr);
    ParameterGroupNumberRequestProtocol::request_parameter_group_number(kPgnXteTrimbleLegacy, controlFunction, nullptr);
}

// ------------------------------------------------------------------
// Update -- call every loop() iteration
// ------------------------------------------------------------------
void IsobusGuidanceChannel::Update() {
    CANHardwareInterface::update();
}

// ------------------------------------------------------------------
// PGN 129025 - Position, Rapid Update (single frame, 8 bytes)
//   Bytes 0-3: int32 latitude  (1e-7 deg; 0x7FFFFFFF = N/A)
//   Bytes 4-7: int32 longitude (1e-7 deg; 0x7FFFFFFF = N/A)
// Lat/lon are decoded-and-discarded: nothing in this project consumes the
// coordinate value, only the fix-age timestamp (HOLD-mode staleness
// watchdog in InterfacePlough::Update()) -- same as VehicleGps's CAN_POS
// handling before this port.
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnPositionNmea2000(const CANMessage& msg, void* context) {
    if (msg.get_data_length() < 8) return;
    const auto& d = msg.get_data();

    auto rawLat = int32_t(uint32_t(d[0]) | (uint32_t(d[1]) << 8) | (uint32_t(d[2]) << 16) | (uint32_t(d[3]) << 24));
    auto rawLon = int32_t(uint32_t(d[4]) | (uint32_t(d[5]) << 8) | (uint32_t(d[6]) << 16) | (uint32_t(d[7]) << 24));

    if (rawLat != int32_t(0x7FFFFFFF) && rawLon != int32_t(0x7FFFFFFF)) {
        static_cast<IsobusGuidanceChannel*>(context)->guidance->NoteGgaFixReceived();
    }
}

// ------------------------------------------------------------------
// PGN 129026 - COG & SOG, Rapid Update (single frame, 8 bytes)
//   Byte 0: SID | Byte 1: COG-ref (2b) + reserved (6b)
//   Bytes 2-3: COG uint16 LE (0.0001 rad; 0xFFFF = N/A)
//   Bytes 4-5: SOG uint16 LE (0.01 m/s;  0xFFFF = N/A)
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnSpeedNmea2000(const CANMessage& msg, void* context) {
    if (msg.get_data_length() < 6) return;
    const auto& d = msg.get_data();

    uint16_t sog = uint16_t(d[4]) | (uint16_t(d[5]) << 8);
    if (sog != 0xFFFF) {
        auto* self = static_cast<IsobusGuidanceChannel*>(context);
        self->guidance->SetSpeedKnots(sog * 0.01f / GPS_MS_PER_KNOT);
    }
}

// ------------------------------------------------------------------
// PGN 129283 - Cross Track Error (single frame, 8 bytes)
//   Byte 0: SID | Byte 1: XTE mode (4b) + reserved (4b)
//   Bytes 2-5: XTE int32 LE (0.01 m, signed; 0x7FFFFFFF = N/A)
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnXteNmea2000(const CANMessage& msg, void* context) {
    if (msg.get_data_length() < 6) return;
    const auto& d = msg.get_data();

    auto rawXte = int32_t(uint32_t(d[2]) | (uint32_t(d[3]) << 8) | (uint32_t(d[4]) << 16) | (uint32_t(d[5]) << 24));
    if (rawXte != int32_t(0x7FFFFFFF)) {
        // rawXte is already hundredths of a metre (0.01 m units) -- matches
        // GuidanceSource::SetXte's hundredths-of-a-metre convention
        // directly, no rescale needed. Quality isn't part of this PGN;
        // assume RTK-equivalent (4) since a standards-compliant guidance
        // source broadcasting real XTE implies it trusts its own fix.
        static_cast<IsobusGuidanceChannel*>(context)->guidance->SetXte(int(rawXte), 4);
    }
}

// ------------------------------------------------------------------
// Legacy proprietary decode, ported verbatim from VehicleGps.cpp's
// Update(long id, const uint8_t* data, byte len).
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnLegacyPosition(const CANMessage& msg, void* context) {
    if (msg.get_data_length() != 8) return;
    static_cast<IsobusGuidanceChannel*>(context)->guidance->NoteGgaFixReceived();
}

void IsobusGuidanceChannel::OnLegacySpeed(const CANMessage& msg, void* context) {
    if (msg.get_data_length() != 8) return;
    const auto& d = msg.get_data();

    unsigned long val = (unsigned long)((d[3] << 8) | d[2]);
    float speed = float(val) / 256.0f;
    static_cast<IsobusGuidanceChannel*>(context)->guidance->SetSpeedKnots(speed);
}

void IsobusGuidanceChannel::OnLegacyXteJohnDeere(const CANMessage& msg, void* context) {
    if (msg.get_data_length() != 8) return;

    // PGN 0xFFFF is a heavily-overloaded manufacturer-proprietary PGN --
    // add_any_control_function_parameter_group_number_callback dispatches by
    // PGN alone, so the sender's source address must be rechecked here to
    // replicate the legacy exact-CAN-ID filter's actual specificity.
    auto sourceCF = msg.get_source_control_function();
    if (sourceCF == nullptr || sourceCF->get_address() != kSourceAddressJohnDeere) {
        return;
    }

    const auto& d = msg.get_data();
    unsigned long val = (unsigned long)((d[4] << 8) | d[3]);
    int xte = int(val - 32000) >> 1;
    byte quality = (d[1] == 0x15) ? 4 : 0;
    static_cast<IsobusGuidanceChannel*>(context)->guidance->SetXte(xte, quality);
}

void IsobusGuidanceChannel::OnLegacyXteTrimble(const CANMessage& msg, void* context) {
    if (msg.get_data_length() != 8) return;

    // PDU1-format PGN: the legacy exact-CAN-ID filter (0x1CEBACAA) required
    // this message be addressed to a fixed destination address (0xAC). Our
    // claimed source address is dynamic, so whether AgIsoStack's normal
    // addressed-message delivery (keyed to our own claimed address) actually
    // surfaces a message the sender addressed to a different, fixed DA needs
    // real-bus verification -- see the plan's open risk on this PGN.
    auto sourceCF = msg.get_source_control_function();
    if (sourceCF == nullptr || sourceCF->get_address() != kSourceAddressTrimble) {
        return;
    }

    const auto& d = msg.get_data();
    if (d[0] == 2 && d[5] == 7) {
        union { unsigned long a; float b; } tofloat;
        tofloat.a = ((unsigned long)d[1] << 24) | ((unsigned long)d[2] << 16) | (d[3] << 8) | d[4];
        int xte = int(tofloat.b * 100);
        static_cast<IsobusGuidanceChannel*>(context)->guidance->SetXte(xte, 4);
    }
}

// ------------------------------------------------------------------
// AISO - All Implement Stop Operations. Exact signal layout isn't
// documented in the available DBC notes (PGN-level identification only) --
// treat receipt of this PGN as an unconditional, immediate stop, the safe/
// conservative interpretation of "should stop plough movement immediately
// when received."
// ------------------------------------------------------------------
void IsobusGuidanceChannel::OnAllImplementStop(const CANMessage&, void* context) {
    static_cast<IsobusGuidanceChannel*>(context)->implement->Stop();
}

}  // namespace triton

#endif  // ISOBUS
