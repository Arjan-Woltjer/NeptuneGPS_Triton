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
};

struct SpeedResult {
    bool     lengthOk = false;   // true once the length guard passed (rawValue meaningful)
    bool     valid = false;      // true => SetSpeedKnots(speedKnots) should be called
    float    speedKnots = 0.0f;  // meaningful only when valid
    uint16_t rawValue = 0;       // diagnostic: pre-scale raw value
};

struct XteResult {
    bool     lengthOk = false;   // true once the length guard passed
    bool     valid = false;      // true => SetXte(...) should be called
    bool     hasQuality = false; // true => call the two-arg SetXte(xte, quality) overload
    int      xteHundredthsMeter = 0;
    byte     quality = 0;
    uint16_t rawWord = 0;        // diagnostic, JD/AgLeader-Raven legacy only
    byte     rawByte1 = 0;       // diagnostic, JD/AgLeader-Raven legacy only
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
