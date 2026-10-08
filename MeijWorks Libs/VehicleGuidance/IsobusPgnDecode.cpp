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
// rather than pulled in via that header so this file stays a pure byte-math
// unit with no dependency on the shared data model. 1 knot = 0.51444444 m/s
// exactly; both must stay numerically identical.
static constexpr float kMetersPerSecondPerKnot = 0.51444444f;

// Shared plausibility guard for both position decoders. The legacy encoding
// carries no documented "not available" sentinel, so a range check is the
// only defence against publishing garbage as a position.
namespace {
bool GIsPlausibleLatLon(float lat, float lon) {
    return (lat >= -90.0f) && (lat <= 90.0f) && (lon >= -180.0f) && (lon <= 180.0f);
}

// Course and altitude get the same treatment (GitHub issue #37). Both do have
// a documented 0xFFFF sentinel, checked at the call site, so these are the
// second line of defence against a garbage frame rather than the only one --
// worth having because this PGN family is proprietary and overloaded.
bool GIsPlausibleCourseDeg(float degrees) {
    return (degrees >= 0.0f) && (degrees <= 360.0f);
}

// Deliberately not a wide range: 5000 m comfortably clears any field on
// Earth, while still rejecting the 5691.875 m that the legacy altitude
// scaling produces from an all-ones frame should the sentinel check ever be
// bypassed.
bool GIsPlausibleAltitudeM(float meters) {
    return (meters >= -500.0f) && (meters <= 5000.0f);
}
}  // namespace

// 129026 reports course in radians; the rest of the codebase, GuidanceSource
// included, works in degrees.
static constexpr float kDegreesPerRadian = 57.2957795f;

// ------------------------------------------------------------------
// PGN 129025 - Position, Rapid Update (single frame, 8 bytes)
//   Bytes 0-3: int32 latitude  (1e-7 deg; 0x7FFFFFFF = N/A)
//   Bytes 4-7: int32 longitude (1e-7 deg; 0x7FFFFFFF = N/A)
// Coordinates are now decoded as well as the fix-age timestamp. They remain
// diagnostics only -- the control path (InterfacePlough's HOLD-mode staleness
// watchdog) consumes the timestamp, not the position -- but the values were
// already being computed here and thrown away, and their absence is what made
// the debug dump read "Lat/Lon: 0.000000 / 0.000000" on a live rig. See
// HardwareTestNotes.md Session 1 bug #6.
// ------------------------------------------------------------------
PositionResult DecodePositionNmea2000(const uint8_t* data, uint8_t length) {
    PositionResult result;
    if (length < 8) return result;

    auto rawLat = int32_t(uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24));
    auto rawLon = int32_t(uint32_t(data[4]) | (uint32_t(data[5]) << 8) | (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24));

    result.fixPresent = (rawLat != int32_t(0x7FFFFFFF) && rawLon != int32_t(0x7FFFFFFF));
    if (!result.fixPresent) return result;

    // Already decoded above and previously thrown away -- same omission as the
    // legacy decoder's, just less visible because the raw values were right
    // there. 1e-7 degree units, signed, no bias (unlike the legacy encoding).
    const float lat = float(rawLat) / 10000000.0f;
    const float lon = float(rawLon) / 10000000.0f;
    if (GIsPlausibleLatLon(lat, lon)) {
        result.hasCoordinates = true;
        result.latitude  = lat;
        result.longitude = lon;
    }
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

    // COG sits beside SOG in the same frame and was previously read past
    // (GitHub issue #37). 0.0001 radian units, so a full turn is 62832 and
    // every valid value fits below the 0xFFFF not-available sentinel.
    // Byte 1 bits 0-1 select the reference (0 = True, 1 = Magnetic); we take
    // it as-is, matching the serial parsers, which do not distinguish either.
    if (length >= 4) {
        uint16_t cog = uint16_t(data[2]) | (uint16_t(data[3]) << 8);
        if (cog != 0xFFFF) {
            const float degrees = cog * 0.0001f * kDegreesPerRadian;
            if (GIsPlausibleCourseDeg(degrees)) {
                result.hasCourse = true;
                result.courseDeg = degrees;
            }
        }
    }
    // 129026 carries no altitude -- hasAltitude stays false.
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
// Ag Leader InCommand lightbar cross-track error, PGN 65462 (see the header).
// Valid only while the display has a line and autosteer is engaged: the
// capture shows the display re-referencing to a line on the other side at the
// moment of engaging (a 1.4 m jump in 0.6 s at 2 km/h), so a value from before
// that moment can belong to a different line than the one being followed.
// ------------------------------------------------------------------
LightbarXteResult DecodeXteAgLeaderLightbar(const uint8_t* data, uint8_t length) {
    LightbarXteResult result;
    if (length != 8) return result;
    result.lengthOk = true;
    for (uint8_t i = 0; i < 8; i++) result.rawPayload[i] = data[i];

    result.magnitudeCm = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    result.engaged     = (data[2] == 2);
    result.hasLine     = (data[7] == 4);
    result.rawStatus   = data[6];
    result.rightOfLine = (data[6] & 0x80) != 0;

    // 0xFFFF is the usual "not available" and anything beyond 300 m is not a
    // cross-track error on a field.
    if (result.magnitudeCm == 0xFFFF || result.magnitudeCm > 30000) return result;
    if (!result.hasLine || !result.engaged) return result;

    result.valid = true;
    result.xteHundredthsMeter = result.rightOfLine ? int(result.magnitudeCm) : -int(result.magnitudeCm);
    return result;
}

// ------------------------------------------------------------------
// Legacy proprietary decode, ported verbatim from VehicleGps.cpp's
// Update(long id, const uint8_t* data, byte len).
// ------------------------------------------------------------------
namespace {
// Sign-extend a little-endian 24-bit field.
std::int32_t GSigned24(const std::uint8_t* p) {
    std::int32_t v = static_cast<std::int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
    if (v & 0x00800000) v |= static_cast<std::int32_t>(0xFF000000);
    return v;
}

std::int64_t GSigned64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];   // little-endian
    return static_cast<std::int64_t>(v);
}
}  // namespace

PositionDeltaResult DecodePositionDeltaNmea2000(const std::uint8_t* data, std::uint8_t length) {
    PositionDeltaResult result;
    if (length < 8) return result;
    result.lengthOk = true;
    result.sid = data[0];
    result.timeDeltaSeconds  = data[1] * 0.005f;
    result.latitudeDeltaDeg  = GSigned24(&data[2]) * 2.77778e-09f;
    result.longitudeDeltaDeg = GSigned24(&data[5]) * 2.77778e-09f;
    return result;
}

GnssPositionDataResult DecodeGnssPositionData(const std::uint8_t* data, std::uint8_t length) {
    GnssPositionDataResult result;
    // 43 bytes is the fixed part; a frame carrying reference-station entries is
    // longer, which is fine -- everything read here sits below byte 43.
    if (length < 43) return result;
    result.lengthOk = true;

    // 1e-16 degrees per bit. Scaling straight to float would lose the value
    // entirely (53 deg is ~5.3e17, well past a float's mantissa), so reduce in
    // integer arithmetic first: /1e8 leaves units of 1e-8 deg, which is far
    // finer than the float GuidanceSource stores anyway.
    const std::int64_t rawLat = GSigned64(&data[7]);
    const std::int64_t rawLon = GSigned64(&data[15]);
    const float lat = static_cast<float>(rawLat / 100000000LL) * 1e-8f;
    const float lon = static_cast<float>(rawLon / 100000000LL) * 1e-8f;
    if (GIsPlausibleLatLon(lat, lon)) {
        result.hasCoordinates = true;
        result.latitude  = lat;
        result.longitude = lon;
    }

    // 1e-6 m per bit.
    const float alt = static_cast<float>(GSigned64(&data[23]) / 1000LL) * 1e-3f;
    if (GIsPlausibleAltitudeM(alt)) {
        result.hasAltitude = true;
        result.altitudeMeters = alt;
    }

    // Field 7 is the low nibble, field 8 -- the quality -- the high nibble.
    result.typeOfSystem = static_cast<std::uint8_t>(data[31] & 0x0F);
    const std::uint8_t method = static_cast<std::uint8_t>((data[31] >> 4) & 0x0F);
    result.method = method;
    // 15 is the not-available code for a 4-bit field. Anything else is a real
    // statement about the fix, including 0 = "no GNSS", which must reach
    // GuidanceSource rather than being dropped as unknown -- losing RTK is
    // exactly the case the interlock exists for.
    result.hasQuality = (method != 0x0F);

    result.numberOfSvs = data[33];
    const std::uint16_t rawHdop = static_cast<std::uint16_t>(data[34] | (data[35] << 8));
    if (rawHdop != 0xFFFF) {
        result.hasHdop = true;
        result.hdop = rawHdop * 0.01f;
    }
    return result;
}

GuidanceMachineInfoResult DecodeGuidanceMachineInfo(const std::uint8_t* data, std::uint8_t length) {
    GuidanceMachineInfoResult result;
    if (length < 8) return result;
    result.lengthOk = true;

    const std::uint16_t raw = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    result.rawCurvature = raw;
    if (raw != 0xFFFF) {
        result.hasCurvature = true;
        result.curvaturePerKm = (raw * 0.25f) - 8032.0f;
    }

    result.mechanicalLockout     = static_cast<std::uint8_t>((data[2] >> 0) & 0x03);
    result.steeringReadiness     = static_cast<std::uint8_t>((data[2] >> 2) & 0x03);
    result.steeringInputPosition = static_cast<std::uint8_t>((data[2] >> 4) & 0x03);
    result.limitStatus           = static_cast<std::uint8_t>((data[3] >> 5) & 0x07);
    result.remoteEngageSwitch    = static_cast<std::uint8_t>((data[4] >> 6) & 0x03);
    return result;
}

ProcessDataKind ClassifyProcessDataCommand(std::uint8_t byte0) {
    switch (byte0 & 0x0F) {
        case 0:  return ProcessDataKind::TechnicalCapabilities;
        case 1:  return ProcessDataKind::DeviceDescriptor;
        case 2:  return ProcessDataKind::RequestValue;
        case 3:  return ProcessDataKind::SetValue;
        case 4:  // measurement time interval
        case 5:  // measurement distance interval
        case 6:  // measurement minimum threshold
        case 7:  // measurement maximum threshold
        case 8:  // measurement change threshold
            return ProcessDataKind::Measurement;
        case 10: return ProcessDataKind::SetValue;   // set value and acknowledge
        case 14: return ProcessDataKind::TaskControllerStatus;
        case 15: return ProcessDataKind::WorkingSetTask;
        default: return ProcessDataKind::Other;
    }
}

PositionResult DecodeLegacyPosition(const uint8_t* data, uint8_t length) {
    PositionResult result;
    result.fixPresent = (length == 8);
    if (!result.fixPresent) return result;

    // Legacy encoding, distinct from NMEA2000 129025's: little-endian uint32
    // biased by 2100000000, in 1e-7 degree units, latitude then longitude.
    // Taken from CanSerialParser's CAN_POS case, which decodes the same wire
    // format off the serial transport and whose math is covered by the
    // known_good_sentences fixtures -- this is the sibling implementation
    // HardwareTestNotes.md Session 1 bug #6 pointed at when noting that this
    // decoder, unlike that one, never produced coordinates at all.
    constexpr uint32_t kLegacyLatLonBias = 2100000000u;
    const uint32_t rawLat = uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
                            (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
    const uint32_t rawLon = uint32_t(data[4]) | (uint32_t(data[5]) << 8) |
                            (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24);

    const float lat = float(int32_t(rawLat - kLegacyLatLonBias)) / 10000000.0f;
    const float lon = float(int32_t(rawLon - kLegacyLatLonBias)) / 10000000.0f;

    if (GIsPlausibleLatLon(lat, lon)) {
        result.hasCoordinates = true;
        result.latitude  = lat;
        result.longitude = lon;
    }
    return result;
}

SpeedResult DecodeLegacySpeed(const uint8_t* data, uint8_t length) {
    SpeedResult result;
    if (length != 8) return result;
    result.lengthOk = true;

    // This frame is not speed alone: it is course + speed + altitude, three
    // independent 16-bit little-endian fields. The sibling decoder
    // CanSerialParser::CAN_SPD has always read all three off the identical
    // wire format; this one read only the middle one and discarded the rest,
    // leaving GetCourse()/GetAltitude() permanently 0.0 on the ISOBUS build
    // (GitHub issue #37). Scales cross-validate against known_good_sentences
    // fixture "0CFEE81C,002D00020000804F" -> 90.0 deg / 2.0 kn / 44.0 m.
    unsigned long val = (unsigned long)((data[3] << 8) | data[2]);
    result.rawValue = uint16_t(val);
    // 0xFFFF = "speed not available", matching DecodeSpeedNmea2000's guard --
    // confirmed on hardware 2026-08-08: without this, an unavailable
    // reading was being printed as an impossible 131.70 m/s (256.00 kn).
    // Each field carries its own sentinel, so an unavailable speed must not
    // suppress a good course: card session 24 has all three unavailable at
    // once (the whole frame is 0xFF), but that is one case, not the rule.
    if (val != 0xFFFF) {
        result.valid = true;
        result.speedKnots = float(val) / 256.0f;
    }

    const uint16_t rawCourse = uint16_t((data[1] << 8) | data[0]);
    if (rawCourse != 0xFFFF) {
        const float degrees = float(rawCourse) / 128.0f;
        if (GIsPlausibleCourseDeg(degrees)) {
            result.hasCourse = true;
            result.courseDeg = degrees;
        }
    }

    // Altitude is bytes 6-7; bytes 4-5 are not part of any field the legacy
    // parser reads, and their meaning is unknown.
    const uint16_t rawAltitude = uint16_t((data[7] << 8) | data[6]);
    if (rawAltitude != 0xFFFF) {
        const float meters = float(rawAltitude) / 8.0f - 2500.0f;
        if (GIsPlausibleAltitudeM(meters)) {
            result.hasAltitude = true;
            result.altitudeMeters = meters;
        }
    }
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
    result.rawCaptured = true;

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

    // ...and only the cross-track error *message*. data[0] is a message
    // selector, not payload: John Deere's guidance source interleaves 0x77
    // (XTE, ~5 Hz) with 0x92 (~1 Hz) on this same PGN and source address.
    // Without this check the 0x92 payload `92 FF 80 3E FC FF FF FF` runs
    // through the math below as val = 0xFC3E = 64574 -> +16287 hundredths and
    // is committed to GuidanceSource as a +162.87 m cross-track error roughly
    // once a second, refreshing lastXteFix so the staleness guard beside it
    // never fires either. Quality lands at 0 on that frame, which also drags
    // IsRtkQuality() false and trips InterfacePlough's Hold branch at 1 Hz.
    // Confirmed on four separate CANedge captures -- see GitHub issue #30 and
    // HardwareTestNotes.md's 2026-09-08 follow-up section.
    //
    // Deliberately placed *after* the raw-diagnostics capture above: the
    // selector gates what we believe, not what we can see on the bus.
    if (data[0] != kMessageSelectorXte) {
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
