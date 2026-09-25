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
#pragma once

#include <Arduino.h>
#include <cstdint>

namespace triton
{

// ------------------------------------------------------------------
// PGN / source-address table -- shared between the decode functions below
// (source-address filtering) and IsobusGuidanceChannel's PGN callback
// registration/request calls.
// ------------------------------------------------------------------
// Standard NMEA2000 (confirmed broadcast by the reference Fendt 6240):
static constexpr std::uint32_t kPgnPositionNmea2000 = 129025;  // Position, Rapid Update
static constexpr std::uint32_t kPgnSpeedNmea2000    = 129026;  // COG & SOG, Rapid Update
static constexpr std::uint32_t kPgnXteNmea2000      = 129283;  // Cross Track Error
// Added 2026-09-09 so a Raven rig is covered: Raven publishes this set on the
// bus according to a setting on its own VT screen, and neither of the two rigs
// captured so far broadcast any of it.
static constexpr std::uint32_t kPgnPositionDeltaNmea2000 = 129027;  // Position Delta, High Precision Rapid Update
static constexpr std::uint32_t kPgnGnssPositionData      = 129029;  // GNSS Position Data -- FAST PACKET, 43 bytes

// Legacy proprietary, ported from VehicleGps.cpp's CAN_POS_ID/CAN_SPD_ID/
// CAN_XTE_ID/CAN_XTE_ID2 (0x0CFEF31C/0x18FEF31C, 0x0CFEE81C/0x18FEE81C,
// 0x0CFFFF2A, 0x1CEBACAA) -- kept for backwards compatibility with equipment
// that only broadcasts these instead of the NMEA2000 set. PGN extracted from
// the 29-bit CAN ID (PF>=240 -> PDU2, PGN=(PF<<8)|PS; PF<240 -> PDU1,
// PGN=(PF<<8), PS is a destination address, not part of the PGN).
static constexpr std::uint32_t kPgnPositionLegacy     = 0xFEF3;  // 65267, PDU2
static constexpr std::uint32_t kPgnSpeedLegacy        = 0xFEE8;  // 65256, PDU2
static constexpr std::uint32_t kPgnXteJohnDeereLegacy = 0xFFFF;  // 65535, PDU2 -- heavily overloaded
                                                                 // proprietary PGN, source address
                                                                 // must be rechecked in the callback
// PGN 0xFFFF is heavily overloaded across manufacturers. Two source
// addresses have been seen broadcasting XTE on it, and they are NOT
// interchangeable -- only one of them is decoded:
//  - 0x2A: John Deere (implied by the old fixed CAN ID 0x0CFFFF2A, and by
//    the captured fixture known_good_sentences.txt:32, whose xte/quality
//    math is cross-tested against CanSerialParser). Confirmed years ago
//    against a real John Deere system, and re-confirmed 2026-09-05.
//    DECODED.
//  - 0x80: Ag Leader/Raven, seen 2026-08-08 via IsobusDebugMenu's per-PGN
//    "last SA=" readout. Session 1 widened the John Deere decoder to accept
//    it, assuming both vendors shared this PGN's payload layout. Session 6
//    (2026-09-05) disproved that against live ground truth -- see
//    DecodeLegacyXteJohnDeere() and GitHub issue #20. DIAGNOSTICS ONLY: its
//    raw bytes are still captured (deriving the real layout needs them),
//    but nothing is committed to GuidanceSource.
//
// Note this is only John Deere's *proprietary* XTE path. The same equipment
// family also broadcasts standard NMEA2000 XTE (PGN 129283, legacy CAN ID
// 1DF9031C, SA 0x1C) alongside its position/speed PGNs -- that path is
// handled by DecodeXteNmea2000() below, which needs no source-address
// filter because the PGN is not overloaded.
static constexpr std::uint8_t  kSourceAddressJohnDeere     = 0x2A;
static constexpr std::uint8_t  kSourceAddressAgLeaderRaven = 0x80;

// PGN 0xFFFF does not carry one message: data[0] is a message selector, and a
// single source address sends several different messages under it. 0x77 is the
// cross-track error message; everything else on this PGN has a different
// layout and must not be run through the XTE decode (GitHub issue #30).
//
// Established from the CANedge full-bus captures in Documentation/canlogs/ and
// card sessions 7/8/9: John Deere's guidance source (0x2A) interleaves 0x77 at
// ~5 Hz with 0x92 at ~1 Hz, and SA 0x1C and 0xF0 on the same bus use eight and
// seven distinct selectors respectively. Ag Leader/Raven (0x80) sends 0x51,
// which issue #20's closing analysis identified as a DOP-like triple rather
// than cross-track error at all -- so on that rig this PGN carries no XTE.
static constexpr std::uint8_t  kMessageSelectorXte         = 0x77;
static constexpr std::uint32_t kPgnXteTrimbleLegacy = 0xEB00;  // 60160, PDU1 -- legacy filter required
                                                                 // destination address 0xAC (fixed); our
                                                                 // claimed SA is dynamic, so whether this
                                                                 // is actually delivered needs real-bus
                                                                 // verification (see plan's open risk)
static constexpr std::uint8_t  kSourceAddressTrimble = 0xAA;

// AISO (All Implement Stop Operations) -- DBC arbitration ID 2365391614 =
// 0x8CFD02FE with the SocketCAN EFF flag (0x80000000) masked off ->
// 0x0CFD02FE: PF=0xFD (253, PDU2) -> PGN=(0xFD<<8)|0x02=0xFD02=64770.
static constexpr std::uint32_t kPgnAllImplementStop = 0xFD02;  // 64770, PDU2

// PGN 44032 (0xAC00, PDU1) -- Agricultural Guidance Machine Info, the standard
// ISO 11783-7 guidance channel. Broadcast at 10 Hz by the tractor ECU on every
// rig captured so far, John Deere and CNH alike, with no Task Controller
// session, no DDOP and no handshake needed.
//
// **This is not cross-track error and must never be treated as one.** It
// carries estimated *curvature* -- the reciprocal of the turning radius -- plus
// the steering system's own status. Committing it to GuidanceSource::xte would
// be the same category error the design doc warns about for DDI 513, so
// nothing here reaches the control path; it is read for diagnostics and for
// the interlock states, which say *why* guidance is or is not happening.
static constexpr std::uint32_t kPgnGuidanceMachineInfo = 0xAC00;  // 44032, PDU1

// ------------------------------------------------------------------
// Decode results. Each carries a `valid` flag: whether IsobusGuidanceChannel
// should commit the decoded value to GuidanceSource/ImplementPlough. Fields
// marked "diagnostic" are populated whenever the length guard passes,
// independent of `valid`, matching IsobusDebugMenu's existing per-PGN raw
// readouts (they show what actually arrived on the wire even when a
// handler's own filtering would otherwise drop the message).
// ------------------------------------------------------------------
struct PositionResult {
    bool fixPresent = false;  // true => NoteGgaFixReceived() should be called
    // true => latitude/longitude below are meaningful and SetPosition() should
    // be called. Separate from fixPresent on purpose: fixPresent drives the
    // GGA fix-age timer that InterfacePlough gates plough control on, and must
    // keep its existing meaning exactly. Coordinates are diagnostics only --
    // nothing in the control path reads them (checked 2026-09-05: the sole
    // consumer of GuidanceSource::GetLatitude/GetLongitude is
    // IsobusDebugMenu's dump) -- so a coordinate that fails the plausibility
    // check must never cost us a fix.
    bool  hasCoordinates = false;
    // Degrees. float, matching GuidanceSource's own storage and the serial
    // parsers' behaviour -- which caps useful precision at roughly 1e-5 deg
    // (~1 m). Fine for confirming a receiver is sane on the debug dump; not
    // good enough to do geodesy with.
    float latitude  = 0.0f;
    float longitude = 0.0f;
};

// Both speed-bearing PGNs carry more than speed: the legacy PGN 65256 frame is
// course + speed + altitude, and NMEA2000's 129026 is COG + SOG. Each field is
// independently available or not, so each gets its own has* flag alongside the
// existing lengthOk/valid convention rather than a parallel result type
// (GitHub issue #37). A decoder leaves a field's flag false when its sender
// does not carry it -- 129026 has no altitude, for instance.
// ------------------------------------------------------------------
// Process Data (PGN 0xCB00) command classification -- GitHub issue #21.
//
// Pure so it can be tested: the counters that misled session 9 lived inside a
// CAN callback where nothing could reach them. The command is the low nibble
// of byte 0.
//
// Note the trap this deliberately does NOT fall into: for the Device
// Descriptor command (1) the *high* nibble of byte 0 is a sub-command, not
// element-number bits, so a caller must not read an element or DDI out of
// those frames. Classify() reports them as DeviceDescriptor precisely so the
// caller knows not to.
// ------------------------------------------------------------------
enum class ProcessDataKind : std::uint8_t {
    TechnicalCapabilities,  // 0
    DeviceDescriptor,       // 1  -- high nibble is a sub-command, not an element
    RequestValue,           // 2  -- the TC asking us for a value
    SetValue,               // 3 and 10 -- the TC writing a value to us
    Measurement,            // 4-8 -- the TC configuring reporting on one of our DPDs
    TaskControllerStatus,   // 14
    WorkingSetTask,         // 15
    Other,
};

ProcessDataKind ClassifyProcessDataCommand(std::uint8_t byte0);

// DDI carried in bytes 2-3, little-endian. Only meaningful for the kinds where
// bytes 2-3 really are a DDI -- RequestValue, SetValue and Measurement.
inline std::uint16_t ProcessDataDdi(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[2] | (data[3] << 8));
}

struct SpeedResult {
    bool     lengthOk = false;   // true once the length guard passed (rawValue meaningful)
    bool     valid = false;      // true => SetSpeedKnots(speedKnots) should be called
    float    speedKnots = 0.0f;  // meaningful only when valid
    uint16_t rawValue = 0;       // diagnostic: pre-scale raw value
    bool     hasCourse = false;  // true => SetCourseDeg(courseDeg) should be called
    float    courseDeg = 0.0f;   // meaningful only when hasCourse
    bool     hasAltitude = false;// true => SetAltitude(altitudeMeters) should be called
    float    altitudeMeters = 0.0f; // meaningful only when hasAltitude
};

// Field layout verified against the CSS Electronics ISOBUS DBC v2.4
// (NeptuneGPS Documentation/ISOBUS/CSS-Electronics-ISOBUS-2022-08_v2.4.dbc,
// message GMS / 2360147710) rather than inferred:
//   bits  0-15  EstimatedCurvature      0.25 km^-1 per bit, offset -8032
//   bits 16-17  MechanicalSystemLockout
//   bits 18-19  GuidanceSteeringSystemReadiness
//   bits 20-21  SteeringInputPositionStatus
//   bits 29-31  GuidanceLimitStatus
//   bits 38-39  GuidanceSystemRemoteEngageSwitchStatus
// The two-bit states share one encoding: 0 = no/not ready, 1 = yes/ready,
// 2 = error, 3 = not available.
// PGN 129027, single frame. Deltas from the previous fix, not an absolute
// position -- a receiver sends these between 129025 fixes to raise the
// effective update rate. Layout from the canboat NMEA2000 DBC
// (PGN_129027_positionDeltaRapidUpd).
struct PositionDeltaResult {
    bool  lengthOk = false;
    std::uint8_t sid = 0;
    float timeDeltaSeconds = 0.0f;   // 0.005 s per bit
    float latitudeDeltaDeg = 0.0f;   // 2.77778e-09 deg per bit, signed 24-bit
    float longitudeDeltaDeg = 0.0f;
};

PositionDeltaResult DecodePositionDeltaNmea2000(const std::uint8_t* data, std::uint8_t length);

// PGN 129029, **fast packet, 43 bytes**. The comprehensive GNSS message, and
// the only one on any of these buses that carries a GNSS quality indicator.
//
// Field order from the NMEA 2000 v1.301 Appendix B PGN field list (archived at
// NeptuneGPS Documentation/ISOBUS/july 2010 nmea2000_v1-301_app_b_pgn_field_list.pdf):
//   1 SID (byte 0), 2 date (1-2), 3 time (3-6), 4 latitude (7-14, int64,
//   1e-16 deg), 5 longitude (15-22), 6 altitude (23-30, int64, 1e-6 m),
//   7 Type of System + 8 Method (byte 31, low and high nibble), 9 integrity
//   (byte 32), 11 number of SVs (byte 33), 12 HDOP (34-35), 13 PDOP (36-37),
//   14 geoidal separation (38-41), 15 reference stations (byte 42).
//
// **Field 8, "Method, GNSS", is the quality.** Its encoding matches NMEA 0183
// GGA for every value we care about -- 0 no fix, 1 GNSS, 2 DGNSS, 3 precise,
// 4 RTK fixed, 5 RTK float -- so it maps onto GuidanceSource::quality with no
// translation, and 4 means the same RTK-fixed that IsRtkQuality() tests for.
struct GnssPositionDataResult {
    bool  lengthOk = false;
    bool  hasCoordinates = false;
    float latitude = 0.0f;
    float longitude = 0.0f;
    bool  hasAltitude = false;
    float altitudeMeters = 0.0f;
    bool  hasQuality = false;
    std::uint8_t method = 0xFF;      // field 8; 4 = RTK fixed
    std::uint8_t typeOfSystem = 0xFF;
    std::uint8_t numberOfSvs = 0xFF;
    bool  hasHdop = false;
    float hdop = 0.0f;
};

GnssPositionDataResult DecodeGnssPositionData(const std::uint8_t* data, std::uint8_t length);

struct GuidanceMachineInfoResult {
    bool  lengthOk = false;
    bool  hasCurvature = false;      // false when the field reads its 0xFFFF sentinel
    float curvaturePerKm = 0.0f;     // km^-1, signed; meaningful only when hasCurvature
    std::uint16_t rawCurvature = 0;  // diagnostic, pre-scale
    // Default to 3 = "not available", so an undersized or absent frame reads as
    // unknown rather than as a confident "not locked out / not ready".
    std::uint8_t mechanicalLockout     = 3;
    std::uint8_t steeringReadiness     = 3;
    std::uint8_t steeringInputPosition = 3;
    std::uint8_t limitStatus           = 7;
    std::uint8_t remoteEngageSwitch    = 3;
};

GuidanceMachineInfoResult DecodeGuidanceMachineInfo(const std::uint8_t* data, std::uint8_t length);

struct XteResult {
    bool     lengthOk = false;   // true once the length guard passed
    bool     valid = false;      // true => SetXte(...) should be called
    bool     hasQuality = false; // true => call the two-arg SetXte(xte, quality) overload
    int      xteHundredthsMeter = 0;
    byte     quality = 0;
    uint16_t rawWord = 0;        // diagnostic, JD/AgLeader-Raven legacy only
    byte     rawByte1 = 0;       // diagnostic, JD/AgLeader-Raven legacy only
    // Full 8-byte payload, diagnostic, JD/AgLeader-Raven legacy only. rawWord
    // and rawByte1 above expose only 3 of these 8 bytes, which is not enough
    // to reverse-engineer Ag Leader's layout on this overloaded PGN -- see
    // GitHub issue #20. Deriving it needs every byte logged against
    // ground-truth XTE read off the terminal, so capture the lot.
    // Populated on the same terms as rawWord/rawByte1: length guard passed
    // and the source address is one we recognise, independent of `valid`.
    byte     rawPayload[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct AisoResult {
    bool    lengthOk = false;
    uint8_t state = 0xFF;         // decoded 2-bit state; 0xFF = frame too short to decode
    bool    stopRequested = false;
};

// ------------------------------------------------------------------
// Pure byte-math decode functions -- no CAN/AgIsoStack/GuidanceSource
// dependency, so they're native-testable exactly like NmeaParser/
// TrimbleParser/CanSerialParser. IsobusGuidanceChannel's On*() callbacks
// extract `data`/`length` (and `sourceAddress`, where a legacy handler needs
// one) from a real isobus::CANMessage and apply the returned result.
// ------------------------------------------------------------------
PositionResult DecodePositionNmea2000(const uint8_t* data, uint8_t length);
SpeedResult    DecodeSpeedNmea2000(const uint8_t* data, uint8_t length);
XteResult      DecodeXteNmea2000(const uint8_t* data, uint8_t length);

PositionResult DecodeLegacyPosition(const uint8_t* data, uint8_t length);
SpeedResult    DecodeLegacySpeed(const uint8_t* data, uint8_t length);
XteResult      DecodeLegacyXteJohnDeere(uint8_t sourceAddress, const uint8_t* data, uint8_t length);
XteResult      DecodeLegacyXteTrimble(uint8_t sourceAddress, const uint8_t* data, uint8_t length);

AisoResult DecodeAllImplementStop(const uint8_t* data, uint8_t length);

}  // namespace triton
