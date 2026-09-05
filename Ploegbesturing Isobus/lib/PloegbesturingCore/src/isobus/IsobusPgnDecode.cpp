/*
  IsobusPgnDecode - pure byte-level decode for the guidance PGNs IsobusGuidanceChannel consumes
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
#include "IsobusPgnDecode.hpp"

namespace triton
{

// Mirrors GuidanceSource.hpp's GPS_MS_PER_KNOT -- kept as a local constant
// rather than pulled in via that header so this file stays free of the
// GuidanceSource/EEPROM dependency chain. 1 knot = 0.51444444 m/s exactly;
// both must stay numerically identical.
static constexpr float kMetersPerSecondPerKnot = 0.51444444f;

// ------------------------------------------------------------------
// PGN 129025 - Position, Rapid Update (single frame, 8 bytes)
//   Bytes 0-3: int32 latitude  (1e-7 deg; 0x7FFFFFFF = N/A)
//   Bytes 4-7: int32 longitude (1e-7 deg; 0x7FFFFFFF = N/A)
// Lat/lon are decoded-and-discarded: nothing in this project consumes the
// coordinate value, only the fix-age timestamp (HOLD-mode staleness
// watchdog in InterfacePlough::Update()) -- same as VehicleGps's CAN_POS
// handling before this port.
// ------------------------------------------------------------------
PositionResult DecodePositionNmea2000(const uint8_t* data, uint8_t length) {
    PositionResult result;
    if (length < 8) return result;

    auto rawLat = int32_t(uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24));
    auto rawLon = int32_t(uint32_t(data[4]) | (uint32_t(data[5]) << 8) | (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24));

    result.fixPresent = (rawLat != int32_t(0x7FFFFFFF) && rawLon != int32_t(0x7FFFFFFF));
    return result;
}

// ------------------------------------------------------------------
// PGN 129026 - COG & SOG, Rapid Update (single frame, 8 bytes)
//   Byte 0: SID | Byte 1: COG-ref (2b) + reserved (6b)
//   Bytes 2-3: COG uint16 LE (0.0001 rad; 0xFFFF = N/A)
//   Bytes 4-5: SOG uint16 LE (0.01 m/s;  0xFFFF = N/A)
// ------------------------------------------------------------------
SpeedResult DecodeSpeedNmea2000(const uint8_t* data, uint8_t length) {
    SpeedResult result;
    if (length < 6) return result;
    result.lengthOk = true;

    uint16_t sog = uint16_t(data[4]) | (uint16_t(data[5]) << 8);
    result.rawValue = sog;
    if (sog != 0xFFFF) {
        result.valid = true;
        result.speedKnots = sog * 0.01f / kMetersPerSecondPerKnot;
    }
    return result;
}

// ------------------------------------------------------------------
// PGN 129283 - Cross Track Error (single frame, 8 bytes)
//   Byte 0: SID
//   Byte 1: bits 0-3 XTE mode, bits 4-5 reserved, bit 6 Navigation
//           Terminated, bit 7 reserved (canboat/NMEA2000 field layout --
//           AgIsoStack's own NMEA2000Messages namespace doesn't cover this
//           PGN, so there's no vendored reference for it).
//   Bytes 2-5: XTE int32 LE (0.01 m, signed; 0x7FFFFFFF = N/A,
//              0x7FFFFFFE = error)
// ------------------------------------------------------------------
XteResult DecodeXteNmea2000(const uint8_t* data, uint8_t length) {
    XteResult result;
    if (length < 6) return result;
    result.lengthOk = true;

    // Navigation Terminated (bit 6): the source has stopped navigating this
    // route/leg, so any XTE value in this frame is stale/meaningless --
    // gate on it rather than feeding a leftover number to GuidanceSource.
    bool navigationTerminated = (data[1] >> 6) & 0x01;
    if (navigationTerminated) return result;

    auto rawXte = int32_t(uint32_t(data[2]) | (uint32_t(data[3]) << 8) | (uint32_t(data[4]) << 16) | (uint32_t(data[5]) << 24));
    if (rawXte == int32_t(0x7FFFFFFF) || rawXte == int32_t(0x7FFFFFFE)) return result;

    // rawXte is already hundredths of a metre (0.01 m units) -- matches
    // GuidanceSource::SetXte's hundredths-of-a-metre convention directly,
    // no rescale needed. Quality isn't part of this PGN and there is no
    // verified quality source wired up for the ISOBUS path yet (PGN 129029
    // deliberately not pursued -- not reliably present across hardware, see
    // Triton_TC_Client_Design.md); leaving hasQuality false leaves
    // GuidanceSource::quality untouched rather than lying that every frame
    // is RTK-equivalent, which previously defeated InterfacePlough's
    // IsRtkQuality() interlock unconditionally.
    result.valid = true;
    result.xteHundredthsMeter = int(rawXte);
    return result;
}

// ------------------------------------------------------------------
// Legacy proprietary decode, ported verbatim from VehicleGps.cpp's
// Update(long id, const uint8_t* data, byte len).
// ------------------------------------------------------------------
PositionResult DecodeLegacyPosition(const uint8_t* data, uint8_t length) {
    (void)data;
    PositionResult result;
    result.fixPresent = (length == 8);
    return result;
}

SpeedResult DecodeLegacySpeed(const uint8_t* data, uint8_t length) {
    SpeedResult result;
    if (length != 8) return result;
    result.lengthOk = true;

    unsigned long val = (unsigned long)((data[3] << 8) | data[2]);
    result.rawValue = uint16_t(val);
    // 0xFFFF = "speed not available", matching DecodeSpeedNmea2000's guard --
    // confirmed on hardware 2026-08-08: without this, an unavailable
    // reading was being printed as an impossible 131.70 m/s (256.00 kn).
    if (val == 0xFFFF) return result;

    result.valid = true;
    result.speedKnots = float(val) / 256.0f;
    return result;
}

XteResult DecodeLegacyXteJohnDeere(uint8_t sourceAddress, const uint8_t* data, uint8_t length) {
    XteResult result;
    if (length != 8) return result;
    result.lengthOk = true;

    // PGN 0xFFFF is a heavily-overloaded manufacturer-proprietary PGN --
    // IsobusGuidanceChannel dispatches by PGN alone, so the sender's source
    // address must be rechecked here to replicate the legacy exact-CAN-ID
    // filter's actual specificity.
    if (sourceAddress != kSourceAddressJohnDeere && sourceAddress != kSourceAddressAgLeaderRaven) {
        return result;
    }

    // Raw diagnostics are captured for BOTH senders, decoded or not: deriving
    // Ag Leader's real payload layout needs exactly this data off a live bus
    // (GitHub issue #20). Matches this header's documented convention that
    // diagnostic fields populate whenever the length guard passed,
    // independent of `valid`.
    unsigned long val = (unsigned long)((data[4] << 8) | data[3]);
    result.rawWord = uint16_t(val);
    result.rawByte1 = data[1];
    for (uint8_t i = 0; i < 8; i++) {
        result.rawPayload[i] = data[i];
    }

    // ...but only John Deere's payload is actually decoded. Session 6
    // (2026-09-05) disproved Session 1's assumption that Ag Leader/Raven
    // (0x80) shares John Deere's layout on this PGN: against live ground
    // truth the terminal's own XTE swung 144 -> 8 -> 118 -> 14 cm while
    // data[3..4] produced only two distinct values across ~400 consecutive
    // samples, and data[1] reads 0x03, which the John Deere quality nibble
    // check below can never accept. Decoding it anyway produced a
    // confidently wrong number and a permanently failing quality gate, which
    // is worse than no reading at all -- so 0x80 is diagnostics-only until
    // a raw capture establishes its real layout. See issue #20 and
    // HardwareTestNotes.md Session 6, phases 4d and 5.
    if (sourceAddress != kSourceAddressJohnDeere) {
        return result;
    }

    result.valid = true;
    result.hasQuality = true;
    result.xteHundredthsMeter = int(val - 32000) >> 1;
    // High nibble of byte 1 == 0x1 signals quality=4 -- reverse-engineered
    // and verified years ago; matches CanSerialParser's CAN_XTE case
    // (t[2] == '1', the ASCII-hex form of this same nibble check).
    result.quality = ((data[1] & 0xF0) == 0x10) ? 4 : 0;
    return result;
}

XteResult DecodeLegacyXteTrimble(uint8_t sourceAddress, const uint8_t* data, uint8_t length) {
    XteResult result;
    if (length != 8) return result;
    result.lengthOk = true;

    // PDU1-format PGN: the legacy exact-CAN-ID filter (0x1CEBACAA) required
    // this message be addressed to a fixed destination address (0xAC). Our
    // claimed source address is dynamic, so whether AgIsoStack's normal
    // addressed-message delivery (keyed to our own claimed address) actually
    // surfaces a message the sender addressed to a different, fixed DA needs
    // real-bus verification -- see the plan's open risk on this PGN.
    if (sourceAddress != kSourceAddressTrimble) {
        return result;
    }

    if (data[0] == 2 && data[5] == 7) {
        union { unsigned long a; float b; } tofloat;
        tofloat.a = ((unsigned long)data[1] << 24) | ((unsigned long)data[2] << 16) | (data[3] << 8) | data[4];
        result.valid = true;
        result.hasQuality = true;
        result.xteHundredthsMeter = int(tofloat.b * 100);
        result.quality = 4;
    }
    return result;
}

// ------------------------------------------------------------------
// AISO - All Implement Stop Operations Switch State (ISO 11783-7 / AEF
// Guideline 004 ISB). Byte 7, bits 0-1 carry a 2-bit state: 00 = Stop
// implement operations, 01 = Permit all implements to operate ON, 10 =
// Error, 11 = Not available. This is a periodic broadcast (roughly 1 Hz,
// per AgIsoStack's own isobus_shortcut_button_interface.cpp), not a
// one-shot stop event -- most received frames carry state 01 (Permit).
// Only 00 means stop; 01/10/11 must NOT request a stop, or every routine
// Permit broadcast would halt the plough. Layout confirmed against the
// vendored AgIsoStack ShortcutButtonInterface::process_message(), which
// reads the identical field the same way (messageData.at(7) & 0x03).
// ------------------------------------------------------------------
AisoResult DecodeAllImplementStop(const uint8_t* data, uint8_t length) {
    AisoResult result;
    if (length != 8) return result;
    result.lengthOk = true;

    constexpr uint8_t kStopImplementOperations = 0;
    result.state = data[7] & 0x03;
    result.stopRequested = (result.state == kStopImplementOperations);
    return result;
}

}  // namespace triton
