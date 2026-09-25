/*
  test_IsobusPgnDecode - Tests for IsobusPgnDecode: byte-level decode of the
  guidance PGNs IsobusGuidanceChannel consumes (NMEA2000 + legacy proprietary
  + AISO). Several cases pin regressions from Documentation/HardwareTestNotes.md
  (wrong John Deere source address, wrong XTE quality-byte check).
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
#include <AUnit.h>
#include "IsobusPgnDecode.hpp"

using namespace aunit;
using namespace triton;

// Float comparison helper (matches test_GpsParsers.cpp's own).
static bool near(float a, float b, float eps = 1e-3f) {
    float d = a - b;
    return d > -eps && d < eps;
}

// --- DecodePositionNmea2000 --------------------------------------------------

test(IsobusPgnDecode, positionNmea2000_validLatLon_fixPresent) {
    uint8_t d[8] = { 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
    auto r = DecodePositionNmea2000(d, 8);
    assertTrue(r.fixPresent);
}

// 52.0 deg = 520000000 in 1e-7 units = 0x1EFE9200; 5.0 deg = 50000000 =
// 0x02FAF080. Both little-endian, signed, no bias (unlike the legacy encoding).
test(IsobusPgnDecode, positionNmea2000_validLatLon_decodesCoordinates) {
    uint8_t d[8] = { 0x00, 0x92, 0xFE, 0x1E, 0x80, 0xF0, 0xFA, 0x02 };
    auto r = DecodePositionNmea2000(d, 8);
    assertTrue(r.fixPresent);
    assertTrue(r.hasCoordinates);
    assertTrue(near(r.latitude, 52.0f, 1e-5f));
    assertTrue(near(r.longitude, 5.0f, 1e-5f));
}

test(IsobusPgnDecode, positionNmea2000_implausibleLatLon_keepsFixDropsCoordinates) {
    // Latitude decodes to ~214.7 deg -- out of range, so the coordinate is
    // rejected. fixPresent must survive regardless: it drives the staleness
    // watchdog InterfacePlough gates plough control on, and a bad coordinate
    // must never cost us a fix.
    uint8_t d[8] = { 0xFF, 0xFF, 0xFF, 0x7E, 0x80, 0xF0, 0xFA, 0x02 };
    auto r = DecodePositionNmea2000(d, 8);
    assertTrue(r.fixPresent);
    assertFalse(r.hasCoordinates);
}

test(IsobusPgnDecode, positionNmea2000_latSentinel_notFixPresent) {
    // 0x7FFFFFFF little-endian in bytes 0-3 = N/A latitude.
    uint8_t d[8] = { 0xFF, 0xFF, 0xFF, 0x7F, 0x01, 0x00, 0x00, 0x00 };
    auto r = DecodePositionNmea2000(d, 8);
    assertFalse(r.fixPresent);
}

test(IsobusPgnDecode, positionNmea2000_shortFrame_notFixPresent) {
    uint8_t d[7] = { 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
    auto r = DecodePositionNmea2000(d, 7);
    assertFalse(r.fixPresent);
}

// --- DecodeSpeedNmea2000 -----------------------------------------------------

test(IsobusPgnDecode, speedNmea2000_validSog_convertsToKnots) {
    // SOG = 1000 (0.01 m/s units) = 10.00 m/s = 19.43844 knots (1 knot = 0.514444 m/s).
    uint8_t d[6] = { 0, 0, 0, 0, 0xE8, 0x03 };  // bytes 4-5 = 1000 LE
    auto r = DecodeSpeedNmea2000(d, 6);
    assertTrue(r.lengthOk);
    assertTrue(r.valid);
    assertTrue(near(r.speedKnots, 19.43844f, 0.01f));
}

test(IsobusPgnDecode, speedNmea2000_sentinel_invalid) {
    uint8_t d[6] = { 0, 0, 0, 0, 0xFF, 0xFF };
    auto r = DecodeSpeedNmea2000(d, 6);
    assertTrue(r.lengthOk);
    assertFalse(r.valid);
    assertEqual((int)r.rawValue, 0xFFFF);
}

test(IsobusPgnDecode, speedNmea2000_shortFrame_lengthNotOk) {
    uint8_t d[5] = { 0, 0, 0, 0, 0 };
    auto r = DecodeSpeedNmea2000(d, 5);
    assertFalse(r.lengthOk);
    assertFalse(r.valid);
}

// --- DecodeXteNmea2000 --------------------------------------------------------

test(IsobusPgnDecode, xteNmea2000_valid_passesThroughHundredthsMeter) {
    // Byte 1 = 0 (Navigation Terminated bit 6 clear). XTE = 123 (0.01 m units).
    uint8_t d[6] = { 0, 0x00, 0x7B, 0x00, 0x00, 0x00 };
    auto r = DecodeXteNmea2000(d, 6);
    assertTrue(r.lengthOk);
    assertTrue(r.valid);
    assertFalse(r.hasQuality);
    assertEqual(r.xteHundredthsMeter, 123);
}

test(IsobusPgnDecode, xteNmea2000_navigationTerminated_invalid) {
    uint8_t d[6] = { 0, 0x40, 0x7B, 0x00, 0x00, 0x00 };  // bit 6 of byte 1 set
    auto r = DecodeXteNmea2000(d, 6);
    assertTrue(r.lengthOk);
    assertFalse(r.valid);
}

test(IsobusPgnDecode, xteNmea2000_notAvailableSentinel_invalid) {
    uint8_t d[6] = { 0, 0x00, 0xFF, 0xFF, 0xFF, 0x7F };  // 0x7FFFFFFF
    auto r = DecodeXteNmea2000(d, 6);
    assertFalse(r.valid);
}

test(IsobusPgnDecode, xteNmea2000_errorSentinel_invalid) {
    uint8_t d[6] = { 0, 0x00, 0xFE, 0xFF, 0xFF, 0x7F };  // 0x7FFFFFFE
    auto r = DecodeXteNmea2000(d, 6);
    assertFalse(r.valid);
}

test(IsobusPgnDecode, xteNmea2000_shortFrame_lengthNotOk) {
    uint8_t d[5] = { 0, 0, 0, 0, 0 };
    auto r = DecodeXteNmea2000(d, 5);
    assertFalse(r.lengthOk);
}

// --- DecodeLegacyPosition -----------------------------------------------------

test(IsobusPgnDecode, legacyPosition_fullFrame_fixPresent) {
    uint8_t d[8] = { 0 };
    auto r = DecodeLegacyPosition(d, 8);
    assertTrue(r.fixPresent);
    // All-zero decodes to -210 deg once the 2100000000 bias is removed, which
    // is out of range -- so no coordinate, but the fix still counts.
    assertFalse(r.hasCoordinates);
}

// Cross-validation against the already-verified sibling decoder: these are the
// exact payload bytes of known_good_sentences fixture
// "$0CFEF31C,00072A9C80652680", which test_GpsParsers asserts CanSerialParser
// decodes as lat=52.0degN lon=5.0degE. Both transports carry the same legacy
// wire format, so agreeing here means this decoder matches math that was
// verified against real hardware years ago -- the same trick the John Deere
// XTE test uses. See HardwareTestNotes.md Session 1 bug #6.
test(IsobusPgnDecode, legacyPosition_validLatLon_matchesSerialParserFixture) {
    uint8_t d[8] = { 0x00, 0x07, 0x2A, 0x9C, 0x80, 0x65, 0x26, 0x80 };
    auto r = DecodeLegacyPosition(d, 8);
    assertTrue(r.fixPresent);
    assertTrue(r.hasCoordinates);
    assertTrue(near(r.latitude, 52.0f, 1e-5f));
    assertTrue(near(r.longitude, 5.0f, 1e-5f));
}

test(IsobusPgnDecode, legacyPosition_shortFrame_notFixPresent) {
    uint8_t d[7] = { 0 };
    auto r = DecodeLegacyPosition(d, 7);
    assertFalse(r.fixPresent);
}

// --- DecodeLegacySpeed ---------------------------------------------------------

test(IsobusPgnDecode, legacySpeed_validRaw_dividesBy256) {
    // val = (d[3]<<8)|d[2] = 512 -> 2.0 knots.
    uint8_t d[8] = { 0, 0, 0x00, 0x02, 0, 0, 0, 0 };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.lengthOk);
    assertTrue(r.valid);
    assertTrue(near(r.speedKnots, 2.0f, 0.001f));
}

// --- PGN 65256 is course + speed + altitude -- GitHub issue #37 ------------
// The sibling decoder CanSerialParser::CAN_SPD has always read all three
// fields off this identical wire format; this one read only speed.

// Cross-validation against known_good_sentences fixture
// "$0CFEE81C,002D00020000804F", which test_GpsParsers asserts CanSerialParser
// decodes as course=90.0deg speed=2.0kn alt=44.0m. Agreeing here means all
// three scales match math verified against real hardware years ago.
test(IsobusPgnDecode, legacySpeed_allThreeFields_matchSerialParserFixture) {
    uint8_t d[8] = { 0x00, 0x2D, 0x00, 0x02, 0x00, 0x00, 0x80, 0x4F };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.valid);
    assertTrue(near(r.speedKnots, 2.0f, 0.001f));
    assertTrue(r.hasCourse);
    assertTrue(near(r.courseDeg, 90.0f, 0.01f));
    assertTrue(r.hasAltitude);
    assertTrue(near(r.altitudeMeters, 44.0f, 0.01f));
}

// Real frame from Documentation/canlogs/..._log25_xte-outside.MF4, SA 0x1C:
// the rig parked at ~119 deg heading, 4.25 m up, speed effectively zero.
test(IsobusPgnDecode, legacySpeed_realBusPayload_decodesCourseAndAltitude) {
    uint8_t d[8] = { 0x49, 0x3B, 0x01, 0x00, 0xFB, 0x63, 0x42, 0x4E };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.valid);
    assertTrue(near(r.speedKnots, 0.0039f, 0.001f));
    assertTrue(r.hasCourse);
    assertTrue(near(r.courseDeg, 118.57f, 0.01f));
    assertTrue(r.hasAltitude);
    assertTrue(near(r.altitudeMeters, 4.25f, 0.01f));
}

// Card session 24's no-reception frame: every field unavailable at once.
test(IsobusPgnDecode, legacySpeed_allSentinels_nothingCommitted) {
    uint8_t d[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.lengthOk);
    assertFalse(r.valid);
    assertFalse(r.hasCourse);
    assertFalse(r.hasAltitude);
}

// Each field carries its own sentinel, so an unavailable speed must not
// suppress a good course and altitude. Session 24 happens to have all three
// missing together, which would hide a decoder that gated them jointly.
test(IsobusPgnDecode, legacySpeed_speedUnavailable_courseAndAltitudeStillDecoded) {
    uint8_t d[8] = { 0x00, 0x2D, 0xFF, 0xFF, 0x00, 0x00, 0x80, 0x4F };
    auto r = DecodeLegacySpeed(d, 8);
    assertFalse(r.valid);
    assertTrue(r.hasCourse);
    assertTrue(near(r.courseDeg, 90.0f, 0.01f));
    assertTrue(r.hasAltitude);
    assertTrue(near(r.altitudeMeters, 44.0f, 0.01f));
}

// Course raw 0xFF00 = 65280 -> 510 deg, past the 360 deg guard. Not the
// sentinel, so only the plausibility check can reject it.
test(IsobusPgnDecode, legacySpeed_implausibleCourse_dropped) {
    uint8_t d[8] = { 0x00, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x80, 0x4F };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.valid);
    assertFalse(r.hasCourse);
    assertTrue(r.hasAltitude);
}

// Altitude raw 0xFF00 = 65280 -> 5660 m, past the 5000 m guard.
test(IsobusPgnDecode, legacySpeed_implausibleAltitude_dropped) {
    uint8_t d[8] = { 0x00, 0x2D, 0x00, 0x02, 0x00, 0x00, 0x00, 0xFF };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.valid);
    assertTrue(r.hasCourse);
    assertFalse(r.hasAltitude);
}

// --- 129026 carries COG beside SOG -- same issue, other transport ----------

// COG = 15708 * 0.0001 rad = 1.5708 rad = 90.0 deg.
test(IsobusPgnDecode, speedNmea2000_cog_decodesToDegrees) {
    uint8_t d[6] = { 0, 0, 0x5C, 0x3D, 0xE8, 0x03 };
    auto r = DecodeSpeedNmea2000(d, 6);
    assertTrue(r.valid);
    assertTrue(r.hasCourse);
    assertTrue(near(r.courseDeg, 90.0f, 0.05f));
    // This PGN has no altitude field at all.
    assertFalse(r.hasAltitude);
}

test(IsobusPgnDecode, speedNmea2000_cogSentinel_noCourse) {
    uint8_t d[6] = { 0, 0, 0xFF, 0xFF, 0xE8, 0x03 };
    auto r = DecodeSpeedNmea2000(d, 6);
    assertTrue(r.valid);
    assertFalse(r.hasCourse);
}

test(IsobusPgnDecode, speedNmea2000_sogSentinel_courseStillDecoded) {
    uint8_t d[6] = { 0, 0, 0x5C, 0x3D, 0xFF, 0xFF };
    auto r = DecodeSpeedNmea2000(d, 6);
    assertFalse(r.valid);
    assertTrue(r.hasCourse);
    assertTrue(near(r.courseDeg, 90.0f, 0.05f));
}

test(IsobusPgnDecode, legacySpeed_sentinel_invalidButRawCaptured) {
    // Confirmed on hardware 2026-08-08: without this guard, 0xFFFF prints as
    // an impossible 131.70 m/s (256.00 kn) -- see HardwareTestNotes.md.
    uint8_t d[8] = { 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 };
    auto r = DecodeLegacySpeed(d, 8);
    assertTrue(r.lengthOk);
    assertFalse(r.valid);
    assertEqual((int)r.rawValue, 0xFFFF);
}

test(IsobusPgnDecode, legacySpeed_wrongLength_lengthNotOk) {
    uint8_t d[7] = { 0 };
    auto r = DecodeLegacySpeed(d, 7);
    assertFalse(r.lengthOk);
}

// --- DecodeLegacyXteJohnDeere ---------------------------------------------------
// Only 0x2A (John Deere, verified years ago on real hardware and re-confirmed
// 2026-09-05) is decoded on this shared/overloaded PGN. Quality is the nibble
// check (d[1] high nibble == 0x1), not an exact-byte match.
//
// 0x80 (Ag Leader/Raven) is deliberately NOT decoded -- see the
// agLeaderRavenAddress_ test below and GitHub issue #20. Session 1
// (2026-08-18) widened this decoder to accept it on the assumption that both
// vendors share the payload layout; Session 6 (2026-09-05) disproved that
// against live ground truth.

// Bytes follow known-good CanSerialParser fixture "0CFFFF2A,001000007D000000"
// (known_good_sentences.txt) -- CAN ID 0x0CFFFF2A encodes source address
// 0x2A directly, so this doubles as cross-validation against already-tested
// CanSerialParser math for the same legacy protocol.
//
// One byte deviates from that fixture, deliberately: data[0] is 0x77 here,
// not 0x00. That fixture is a *synthetic minimal* frame -- every byte the
// decoder does not read was zeroed when it was written (data[0], data[2] and
// data[5..7] are all 0x00, and data[1] is 0x10 where the bus sends 0x15,
// keeping only the quality nibble). Real John Deere frames carry the message
// selector in data[0], which this decoder now checks (issue #30), so the
// selector has to be restored for a fixture that asserts a successful decode.
// CanSerialParser's own tests keep the original string untouched: that path
// runs behind a CAN-to-serial bridge whose treatment of data[0] we have no
// capture of, so it is deliberately not gated -- see issue #30's follow-up.
test(IsobusPgnDecode, legacyXteJohnDeere_johnDeereAddress_acceptedZeroXte) {
    uint8_t d[8] = { 0x77, 0x10, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertTrue(r.hasQuality);
    assertEqual(r.xteHundredthsMeter, 0);
    assertEqual((int)r.quality, 4);
}

// Ag Leader/Raven is diagnostics-only: nothing is committed to guidance, but
// the raw bytes must still be captured, because deriving this vendor's real
// payload layout from a live bus needs exactly them (GitHub issue #20).
// Field evidence for not decoding it (Session 6, 2026-09-05): against the
// terminal's own XTE swinging 144 -> 8 -> 118 -> 14 cm, these byte offsets
// produced only two distinct values across ~400 consecutive samples, and
// d[1] reads 0x03, which the quality nibble check can never accept.
test(IsobusPgnDecode, legacyXteJohnDeere_agLeaderRavenAddress_diagnosticsOnlyNotDecoded) {
    // Same bytes that yield xte=32/quality=4 for John Deere below -- proving
    // the rejection is by source address, not by payload content.
    uint8_t d[8] = { 0x00, 0x10, 0x00, 0x40, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x80, d, 8);
    assertFalse(r.valid);
    assertFalse(r.hasQuality);
    // ...but the diagnostics are still populated.
    assertTrue(r.lengthOk);
    assertEqual((int)r.rawWord, 0x7D40);
    assertEqual((int)r.rawByte1, 0x10);
    // The whole payload is captured too, not just the three John Deere
    // fields -- deriving Ag Leader's own layout needs every byte (#20).
    for (int i = 0; i < 8; i++) {
        assertEqual((int)r.rawPayload[i], (int)d[i]);
    }
}

test(IsobusPgnDecode, legacyXteJohnDeere_unknownAddress_capturesNoPayload) {
    // An unrecognised sender yields no payload capture either -- this PGN is
    // shared across manufacturers, so most traffic on it is not XTE at all
    // and logging its bytes would pollute the capture we derive layouts from.
    uint8_t d[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    auto r = DecodeLegacyXteJohnDeere(0x2B, d, 8);
    assertFalse(r.valid);
    for (int i = 0; i < 8; i++) {
        assertEqual((int)r.rawPayload[i], 0);
    }
}

test(IsobusPgnDecode, legacyXteJohnDeere_johnDeereAddress_sameBytesStillDecoded) {
    // val = (d[4]<<8)|d[3] = 0x7D40 = 32064 -> xte = (32064-32000)>>1 = 32.
    uint8_t d[8] = { 0x77, 0x10, 0x00, 0x40, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertEqual(r.xteHundredthsMeter, 32);
    assertEqual((int)r.quality, 4);
}

test(IsobusPgnDecode, legacyXteJohnDeere_unknownAddress_rejected) {
    uint8_t d[8] = { 0x00, 0x10, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x2B, d, 8);
    assertFalse(r.valid);
    // A wholly unknown sender yields no diagnostics either -- unlike 0x80,
    // which is a known sender we deliberately don't decode. This PGN is
    // shared across manufacturers, so most traffic on it isn't XTE at all
    // and its bytes would be meaningless in the debug readout.
    assertEqual((int)r.rawWord, 0);
    assertEqual((int)r.rawByte1, 0);
}

test(IsobusPgnDecode, legacyXteJohnDeere_qualityNibbleRange_notExactByte) {
    // byte1 = 0x1F (high nibble 0x1, not the old exact-match 0x15) must still
    // report quality 4 -- the check is a nibble range, not one specific byte.
    uint8_t d[8] = { 0x77, 0x1F, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertEqual((int)r.quality, 4);
}

test(IsobusPgnDecode, legacyXteJohnDeere_qualityNibbleMismatch_zero) {
    uint8_t d[8] = { 0x77, 0x20, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertEqual((int)r.quality, 0);
}

test(IsobusPgnDecode, legacyXteJohnDeere_wrongLength_invalid) {
    uint8_t d[7] = { 0 };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 7);
    assertFalse(r.lengthOk);
    assertFalse(r.valid);
}

// --- The message selector (data[0]) -- GitHub issue #30 --------------------
// PGN 65535 does not carry one message. data[0] selects which, and John
// Deere's guidance source sends at least two on it. Decoding both with the
// XTE layout turns a non-XTE message into a confident, wrong cross-track
// error. Payloads below are real, straight off the CANedge captures in
// Documentation/canlogs/ (2026-09-08, John Deere rig).

// The exact frame that motivated the issue. Sent ~1 Hz alongside the XTE
// message's ~5 Hz, with a constant payload across card sessions 7, 8, 9 and
// 25. Without the selector check this decodes to +16287 hundredths --
// +162.87 m -- and is written into GuidanceSource once a second.
test(IsobusPgnDecode, legacyXteJohnDeere_nonXteMessageSelector_notDecoded) {
    uint8_t d[8] = { 0x92, 0xFF, 0x80, 0x3E, 0xFC, 0xFF, 0xFF, 0xFF };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertFalse(r.valid);
    assertFalse(r.hasQuality);
    assertEqual(r.xteHundredthsMeter, 0);
}

// ...but the raw diagnostics still populate: the selector gates the decode
// and the SetXte call, not the bus-visibility readout. Deriving what the
// other messages on this PGN mean needs exactly these bytes.
test(IsobusPgnDecode, legacyXteJohnDeere_nonXteMessageSelector_stillCapturesDiagnostics) {
    uint8_t d[8] = { 0x92, 0xFF, 0x80, 0x3E, 0xFC, 0xFF, 0xFF, 0xFF };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.lengthOk);
    assertEqual((int)r.rawWord, 0xFC3E);
    assertEqual((int)r.rawByte1, 0xFF);
    for (int i = 0; i < 8; i++) {
        assertEqual((int)r.rawPayload[i], (int)d[i]);
    }
}

// Ground truth from card session 25: the rig sat still, then was nudged once
// at t=88 s. The operator called "a steady 21 cm" and then "11 cm, other
// side"; the receiver's own position (PGN 65267) stepped 0.38 m at the same
// instant. Both payloads must decode to those numbers, with the sign flip.
test(IsobusPgnDecode, legacyXteJohnDeere_realBusPayload_matchesGroundTruthNegative) {
    uint8_t d[8] = { 0x77, 0x15, 0x10, 0xD6, 0x7C, 0x59, 0x89, 0xFF };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertEqual(r.xteHundredthsMeter, -21);
    assertEqual((int)r.quality, 4);
}

test(IsobusPgnDecode, legacyXteJohnDeere_realBusPayload_matchesGroundTruthPositive) {
    uint8_t d[8] = { 0x77, 0x15, 0x10, 0x16, 0x7D, 0x3F, 0x89, 0xFF };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertEqual(r.xteHundredthsMeter, 11);
    assertEqual((int)r.quality, 4);
}

// Ag Leader/Raven's message on this PGN is selector 0x51, not 0x77 (issue
// #20's closing analysis: it is the same message *definition*, and 0x51 is
// a DOP-like triple, not cross-track error at all). It must fail on both
// counts -- source address and selector.
test(IsobusPgnDecode, legacyXteJohnDeere_agLeaderSelector_notDecoded) {
    uint8_t d[8] = { 0x51, 0x03, 0x02, 0xFF, 0x00, 0x63, 0x00, 0xFF };
    auto r = DecodeLegacyXteJohnDeere(0x80, d, 8);
    assertFalse(r.valid);
}

// --- DecodeGnssPositionData (PGN 129029) and DecodePositionDeltaNmea2000 ---
// Added so a Raven rig is covered -- it publishes this set per a setting on
// its own VT screen. **Neither PGN has ever been seen on a real bus here**, so
// unlike the rest of this file these fixtures are constructed from the NMEA
// 2000 v1.301 Appendix B field list rather than captured. Treat a green test
// here as "matches the spec as we read it", not as "verified against
// hardware".
//
// 129029 matters more than its position fields suggest: field 8, "Method,
// GNSS", is the only GNSS quality indicator on any bus captured so far.

// A full frame at the Groningen test site, RTK fixed.
test(IsobusPgnDecode, gnssPositionData_rtkFixed_decodesPositionAltitudeAndQuality) {
    uint8_t d[43] = { 0x2A, 0xE1, 0x50, 0x00, 0x51, 0x25, 0x02, 0x40, 0x50, 0x5F, 0x7F, 0xF7, 0x12, 0x6A, 0x07, 0x00, 0x8E, 0xF0, 0x4A, 0x38, 0xC3, 0xEE, 0x00, 0x90, 0xD9, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xFC, 0x12, 0x55, 0x00, 0x96, 0x00, 0xCC, 0x10, 0x00, 0x00, 0x00 };
    auto r = DecodeGnssPositionData(d, 43);
    assertTrue(r.lengthOk);
    assertTrue(r.hasCoordinates);
    assertTrue(near(r.latitude, 53.426036f, 1e-4f));
    assertTrue(near(r.longitude, 6.7205691f, 1e-4f));
    assertTrue(r.hasAltitude);
    assertTrue(near(r.altitudeMeters, 4.25f, 0.01f));
    assertTrue(r.hasQuality);
    assertEqual((int)r.method, 4);          // RTK fixed -- what IsRtkQuality() wants
    assertEqual((int)r.numberOfSvs, 18);
    assertTrue(r.hasHdop);
    assertTrue(near(r.hdop, 0.85f, 0.01f));
}

// Losing the fix must reach GuidanceSource, not be swallowed as "unknown".
// Method 0 is a statement, and it is exactly the case the RTK interlock exists
// for -- treating it as no-information would leave a stale quality of 4 in
// place while the receiver says it has nothing.
test(IsobusPgnDecode, gnssPositionData_noFix_stillReportsQuality) {
    uint8_t d[43] = { 0x2A, 0xE1, 0x50, 0x00, 0x51, 0x25, 0x02, 0x40, 0x50, 0x5F, 0x7F, 0xF7, 0x12, 0x6A, 0x07, 0x00, 0x8E, 0xF0, 0x4A, 0x38, 0xC3, 0xEE, 0x00, 0x90, 0xD9, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x12, 0x55, 0x00, 0x96, 0x00, 0xCC, 0x10, 0x00, 0x00, 0x00 };
    auto r = DecodeGnssPositionData(d, 43);
    assertTrue(r.hasQuality);
    assertEqual((int)r.method, 0);
}

// 0x0F is the not-available code for a 4-bit field, and is the one value that
// genuinely carries no information.
test(IsobusPgnDecode, gnssPositionData_methodNotAvailable_noQuality) {
    uint8_t d[43] = { 0x2A, 0xE1, 0x50, 0x00, 0x51, 0x25, 0x02, 0x40, 0x50, 0x5F, 0x7F, 0xF7, 0x12, 0x6A, 0x07, 0x00, 0x8E, 0xF0, 0x4A, 0x38, 0xC3, 0xEE, 0x00, 0x90, 0xD9, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFC, 0x12, 0x55, 0x00, 0x96, 0x00, 0xCC, 0x10, 0x00, 0x00, 0x00 };
    auto r = DecodeGnssPositionData(d, 43);
    assertFalse(r.hasQuality);
}

// Fast packet reassembly can hand us a short buffer if a sequence is
// incomplete. Reading a quality out of that would be worse than reading none.
test(IsobusPgnDecode, gnssPositionData_shortFrame_rejected) {
    uint8_t d[42] = { 0 };
    auto r = DecodeGnssPositionData(d, 42);
    assertFalse(r.lengthOk);
    assertFalse(r.hasQuality);
    assertFalse(r.hasCoordinates);
}

// The 1e-16 deg scaling is the trap: 53 degrees is ~5.3e17, far past a float's
// mantissa, so scaling straight to float loses the value. This pins that the
// reduction happens in integer arithmetic first.
test(IsobusPgnDecode, gnssPositionData_latitudePrecision_survivesScaling) {
    uint8_t d[43] = { 0x2A, 0xE1, 0x50, 0x00, 0x51, 0x25, 0x02, 0x40, 0x50, 0x5F, 0x7F, 0xF7, 0x12, 0x6A, 0x07, 0x00, 0x8E, 0xF0, 0x4A, 0x38, 0xC3, 0xEE, 0x00, 0x90, 0xD9, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xFC, 0x12, 0x55, 0x00, 0x96, 0x00, 0xCC, 0x10, 0x00, 0x00, 0x00 };
    auto r = DecodeGnssPositionData(d, 43);
    assertTrue(r.latitude > 53.42f);
    assertTrue(r.latitude < 53.43f);
}

// 129027: signed 24-bit deltas, which must sign-extend. A southward/westward
// delta read as unsigned would come out as a ~46 degree jump.
test(IsobusPgnDecode, positionDelta_negativeDeltas_signExtend) {
    // -1000 in both fields = 0xFFFC18 little-endian.
    uint8_t d[8] = { 0x05, 0x02, 0x18, 0xFC, 0xFF, 0x18, 0xFC, 0xFF };
    auto r = DecodePositionDeltaNmea2000(d, 8);
    assertTrue(r.lengthOk);
    assertEqual((int)r.sid, 5);
    assertTrue(near(r.timeDeltaSeconds, 0.01f, 0.001f));
    assertTrue(r.latitudeDeltaDeg < 0.0f);
    assertTrue(r.longitudeDeltaDeg < 0.0f);
}

test(IsobusPgnDecode, positionDelta_shortFrame_rejected) {
    uint8_t d[7] = { 0 };
    assertFalse(DecodePositionDeltaNmea2000(d, 7).lengthOk);
}

// --- DecodeGuidanceMachineInfo (PGN 44032) --------------------------------
// The standard ISO 11783-7 guidance channel. Layout verified against the CSS
// Electronics ISOBUS DBC v2.4, not inferred. Payloads below are real frames
// from the 2026-09-09 captures.

// John Deere, card session 26: steering free but not ready, which is the state
// it sat in for 4215 of 4433 frames.
test(IsobusPgnDecode, guidanceMachineInfo_johnDeere_notLockedOutNotReady) {
    uint8_t d[8] = { 0xA5, 0x7D, 0x10, 0x20, 0xFF, 0xFF, 0xFF, 0xFF };
    auto r = DecodeGuidanceMachineInfo(d, 8);
    assertTrue(r.lengthOk);
    assertTrue(r.hasCurvature);
    assertTrue(near(r.curvaturePerKm, 9.25f, 0.01f));
    assertEqual((int)r.mechanicalLockout, 0);   // not locked out
    assertEqual((int)r.steeringReadiness, 0);   // not ready
}

// The same tractor with readiness set -- 218 frames of the same session. Only
// two bits differ from the case above, which is exactly the kind of change a
// hand-rolled bit layout gets wrong.
test(IsobusPgnDecode, guidanceMachineInfo_johnDeere_steeringReady) {
    uint8_t d[8] = { 0xA5, 0x7D, 0x14, 0x20, 0xFF, 0xFF, 0xFF, 0xFF };
    auto r = DecodeGuidanceMachineInfo(d, 8);
    assertEqual((int)r.mechanicalLockout, 0);
    assertEqual((int)r.steeringReadiness, 1);   // ready
}

// The CNH tractor under the Ag Leader kit, card session 28: mechanically
// locked out for all 8420 frames. This is the message that explains why
// nothing guidance-related could happen on that rig, and no other message on
// that bus said it.
test(IsobusPgnDecode, guidanceMachineInfo_cnh_reportsMechanicalLockout) {
    uint8_t d[8] = { 0x00, 0x7D, 0x3D, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF };
    auto r = DecodeGuidanceMachineInfo(d, 8);
    assertEqual((int)r.mechanicalLockout, 1);   // LOCKED OUT
    assertEqual((int)r.steeringReadiness, 3);   // not available
    assertEqual((int)r.limitStatus, 7);
}

test(IsobusPgnDecode, guidanceMachineInfo_curvatureSentinel_noCurvature) {
    uint8_t d[8] = { 0xFF, 0xFF, 0x10, 0x20, 0xFF, 0xFF, 0xFF, 0xFF };
    auto r = DecodeGuidanceMachineInfo(d, 8);
    assertTrue(r.lengthOk);
    assertFalse(r.hasCurvature);
    // The status bits must still decode -- an unavailable curvature says
    // nothing about whether the steering system is locked out.
    assertEqual((int)r.mechanicalLockout, 0);
}

// Straight-ahead is raw 32128, not raw 0: the field is offset by -8032 km^-1.
test(IsobusPgnDecode, guidanceMachineInfo_zeroCurvature_isOffsetNotZeroRaw) {
    uint8_t d[8] = { 0x80, 0x7D, 0x10, 0x20, 0xFF, 0xFF, 0xFF, 0xFF };
    auto r = DecodeGuidanceMachineInfo(d, 8);
    assertTrue(near(r.curvaturePerKm, 0.0f, 0.01f));
}

// A short frame must read as "not available" everywhere rather than as a
// confident "not locked out, ready", which is what zero-initialising would
// have given.
test(IsobusPgnDecode, guidanceMachineInfo_shortFrame_readsAsNotAvailable) {
    uint8_t d[7] = { 0 };
    auto r = DecodeGuidanceMachineInfo(d, 7);
    assertFalse(r.lengthOk);
    assertEqual((int)r.mechanicalLockout, 3);
    assertEqual((int)r.steeringReadiness, 3);
    assertEqual((int)r.remoteEngageSwitch, 3);
}

// --- ClassifyProcessDataCommand -- GitHub issue #21 ------------------------
// These exist because session 9's TC counters answered "did the Task
// Controller ask us anything?" wrongly in both directions at once, from inside
// a CAN callback where no test could reach them. The classification is pure
// now, so it can be pinned.

// The exact frame the John Deere TC sent us on 2026-09-09, from card session
// 26: a change threshold on DDI 515. Nothing in the firmware could see this at
// the time -- AgIsoStack consumes measurement commands internally and never
// routes them to the value-command callback, so the counter read 0 while this
// was on the wire.
test(IsobusPgnDecode, processData_realMeasurementCommand_classifiedAsMeasurement) {
    uint8_t d[8] = { 0x28, 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00 };
    assertTrue(ClassifyProcessDataCommand(d[0]) == ProcessDataKind::Measurement);
    assertEqual((int)ProcessDataDdi(d), 515);
}

// All five measurement commands must land in the same bucket -- gating on only
// the change threshold would reproduce the original blind spot for the other
// four.
test(IsobusPgnDecode, processData_allMeasurementCommands_classifiedAsMeasurement) {
    for (uint8_t cmd = 4; cmd <= 8; cmd++) {
        assertTrue(ClassifyProcessDataCommand(cmd) == ProcessDataKind::Measurement);
    }
}

test(IsobusPgnDecode, processData_requestAndSetValue_areDistinct) {
    assertTrue(ClassifyProcessDataCommand(0x02) == ProcessDataKind::RequestValue);
    assertTrue(ClassifyProcessDataCommand(0x03) == ProcessDataKind::SetValue);
    // Set value and acknowledge. Needs version 4 on both ends, so it is not
    // expected against the version-3 TCs seen so far, but it must count as a
    // set rather than falling into Other and reading as silence.
    assertTrue(ClassifyProcessDataCommand(0x0A) == ProcessDataKind::SetValue);
}

// The high nibble is an element number for these commands, and must not change
// the classification. Real frames carry it non-zero.
test(IsobusPgnDecode, processData_elementBitsInHighNibble_ignored) {
    assertTrue(ClassifyProcessDataCommand(0x28) == ProcessDataKind::Measurement);
    assertTrue(ClassifyProcessDataCommand(0xF2) == ProcessDataKind::RequestValue);
    assertTrue(ClassifyProcessDataCommand(0x73) == ProcessDataKind::SetValue);
}

// Device Descriptor is the trap: there the high nibble is a SUB-command, not
// element bits, so bytes 2-3 are not a DDI. It gets its own kind so a caller
// knows not to read one out. 0x61 is the real object-pool transfer frame from
// session 28; 0x71 and 0x91 are the transfer and activate responses.
test(IsobusPgnDecode, processData_deviceDescriptorSubcommands_allClassifiedAsDeviceDescriptor) {
    for (uint8_t sub = 0; sub <= 13; sub++) {
        uint8_t byte0 = static_cast<uint8_t>((sub << 4) | 1);
        assertTrue(ClassifyProcessDataCommand(byte0) == ProcessDataKind::DeviceDescriptor);
    }
}

// The TC status broadcast goes to the global address at ~1 Hz. It is by far
// the most common frame on this PGN and must not be mistaken for the TC
// addressing us.
test(IsobusPgnDecode, processData_statusAndWorkingSet_notMistakenForTraffic) {
    assertTrue(ClassifyProcessDataCommand(0xFE) == ProcessDataKind::TaskControllerStatus);
    assertTrue(ClassifyProcessDataCommand(0xFF) == ProcessDataKind::WorkingSetTask);
    assertTrue(ClassifyProcessDataCommand(0x00) == ProcessDataKind::TechnicalCapabilities);
}

test(IsobusPgnDecode, processData_ddiIsLittleEndian) {
    uint8_t d[8] = { 0x02, 0x00, 0x02, 0x02, 0, 0, 0, 0 };
    assertEqual((int)ProcessDataDdi(d), 514);
}

// --- DecodeLegacyXteTrimble ------------------------------------------------------

test(IsobusPgnDecode, legacyXteTrimble_validFrame_decodesFloatXte) {
    // d[1..4] = IEEE-754 big-endian bits of 1.5f (0x3FC00000) -> xte = 150.
    uint8_t d[8] = { 2, 0x3F, 0xC0, 0x00, 0x00, 7, 0, 0 };
    auto r = DecodeLegacyXteTrimble(0xAA, d, 8);
    assertTrue(r.valid);
    assertTrue(r.hasQuality);
    assertEqual(r.xteHundredthsMeter, 150);
    assertEqual((int)r.quality, 4);
}

test(IsobusPgnDecode, legacyXteTrimble_wrongSourceAddress_rejected) {
    uint8_t d[8] = { 2, 0x3F, 0xC0, 0x00, 0x00, 7, 0, 0 };
    auto r = DecodeLegacyXteTrimble(0xAB, d, 8);
    assertFalse(r.valid);
}

test(IsobusPgnDecode, legacyXteTrimble_wrongFrameMarkers_rejected) {
    uint8_t d[8] = { 3, 0x3F, 0xC0, 0x00, 0x00, 7, 0, 0 };  // d[0] != 2
    auto r = DecodeLegacyXteTrimble(0xAA, d, 8);
    assertFalse(r.valid);
}

test(IsobusPgnDecode, legacyXteTrimble_wrongLength_invalid) {
    uint8_t d[7] = { 0 };
    auto r = DecodeLegacyXteTrimble(0xAA, d, 7);
    assertFalse(r.lengthOk);
    assertFalse(r.valid);
}

// --- DecodeAllImplementStop --------------------------------------------------
// state 0 must stop; 1/2/3 (Permit/Error/Not available) must NOT -- this is a
// ~1 Hz periodic broadcast, and most received frames carry state 1 (Permit).
// Getting this backwards would halt the plough on every routine broadcast.

test(IsobusPgnDecode, allImplementStop_state0_stopRequested) {
    uint8_t d[8] = { 0, 0, 0, 0, 0, 0, 0, 0x00 };
    auto r = DecodeAllImplementStop(d, 8);
    assertTrue(r.lengthOk);
    assertEqual((int)r.state, 0);
    assertTrue(r.stopRequested);
}

test(IsobusPgnDecode, allImplementStop_state1Permit_notStopRequested) {
    uint8_t d[8] = { 0, 0, 0, 0, 0, 0, 0, 0x01 };
    auto r = DecodeAllImplementStop(d, 8);
    assertEqual((int)r.state, 1);
    assertFalse(r.stopRequested);
}

test(IsobusPgnDecode, allImplementStop_state2Error_notStopRequested) {
    uint8_t d[8] = { 0, 0, 0, 0, 0, 0, 0, 0x02 };
    auto r = DecodeAllImplementStop(d, 8);
    assertEqual((int)r.state, 2);
    assertFalse(r.stopRequested);
}

test(IsobusPgnDecode, allImplementStop_state3NotAvailable_notStopRequested) {
    uint8_t d[8] = { 0, 0, 0, 0, 0, 0, 0, 0x03 };
    auto r = DecodeAllImplementStop(d, 8);
    assertEqual((int)r.state, 3);
    assertFalse(r.stopRequested);
}

test(IsobusPgnDecode, allImplementStop_shortFrame_lengthNotOk) {
    uint8_t d[7] = { 0 };
    auto r = DecodeAllImplementStop(d, 7);
    assertFalse(r.lengthOk);
    assertEqual((int)r.state, 0xFF);
}
