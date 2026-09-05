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
#include "isobus/IsobusPgnDecode.hpp"

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

// Bytes match known-good CanSerialParser fixture "0CFFFF2A,001000007D000000"
// (known_good_sentences.txt) -- CAN ID 0x0CFFFF2A encodes source address
// 0x2A directly, so this doubles as cross-validation against already-tested
// CanSerialParser math for the same legacy protocol.
test(IsobusPgnDecode, legacyXteJohnDeere_johnDeereAddress_acceptedZeroXte) {
    uint8_t d[8] = { 0x00, 0x10, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
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
    uint8_t d[8] = { 0x00, 0x10, 0x00, 0x40, 0x7D, 0x00, 0x00, 0x00 };
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
    uint8_t d[8] = { 0x00, 0x1F, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    auto r = DecodeLegacyXteJohnDeere(0x2A, d, 8);
    assertTrue(r.valid);
    assertEqual((int)r.quality, 4);
}

test(IsobusPgnDecode, legacyXteJohnDeere_qualityNibbleMismatch_zero) {
    uint8_t d[8] = { 0x00, 0x20, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
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
