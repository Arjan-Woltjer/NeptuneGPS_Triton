/*
  test_GpsParsers - Tests for NmeaParser, TrimbleParser, CanSerialParser.
  Test data matches the sentences in test/native/tests/known_good_sentences.txt.
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
#include "CanSerialParser.hpp"
#include "NmeaParser.hpp"
#include "TrimbleParser.hpp"

using namespace aunit;
using namespace triton;

// Float comparison helper
static bool near(float a, float b, float eps = 1e-4f) {
    float d = a - b;
    return d > -eps && d < eps;
}

// ---------------------------------------------------------------------------
// NmeaParser -- sentences from known_good_sentences.txt
// ---------------------------------------------------------------------------

// $GPGGA,151503.00,5326.480207,N,00645.193203,E,2,09,1.0,44.05,M,0.00,M,,*67
test(NmeaParser, gga_lat_lon_quality_alt) {
    GuidanceSource state;
    NmeaParser p;
    assertTrue(p.claimsSentenceType("GPGGA"));
    p.parseTerm(1, "151503.00");
    p.parseTerm(2, "5326.480207");
    p.parseTerm(3, "N");
    p.parseTerm(4, "00645.193203");
    p.parseTerm(5, "E");
    p.parseTerm(6, "2");
    p.parseTerm(9, "44.05");
    p.commitTo(&state);
    assertTrue(near(state.GetLatitude(), 53.4413f));
    assertTrue(near(state.GetLongitude(), 6.7532f));
    assertEqual((int)state.GetQuality(), 2);
    assertTrue(near(state.GetAltitude(), 44.05f));
}

// $GPVTG,213.4,T,,M,002.91,N,005.39,K*61
test(NmeaParser, vtg_course_speed) {
    GuidanceSource state;
    NmeaParser p;
    assertTrue(p.claimsSentenceType("GPVTG"));
    p.parseTerm(1, "213.4");
    p.parseTerm(5, "002.91");
    p.commitTo(&state);
    assertTrue(near(state.GetCourse(), 213.4f));
    assertTrue(near(state.GetSpeed(), 2.91f));
}

// $GPXTE,A,A,0.159523,L,N*67
test(NmeaParser, xte) {
    GuidanceSource state;
    NmeaParser p;
    assertTrue(p.claimsSentenceType("GPXTE"));
    p.parseTerm(3, "0.159523");
    p.commitTo(&state);
    assertEqual(state.GetXte(), 15);  // int(0.159523 * 100) = 15
}

test(NmeaParser, rejects_unknown_header) {
    NmeaParser p;
    assertFalse(p.claimsSentenceType("GPRMC"));
    assertFalse(p.claimsSentenceType("ROXTE"));
    assertFalse(p.claimsSentenceType("0CFEF31C"));
}

// ---------------------------------------------------------------------------
// TrimbleParser -- ROXTE inner sentences; binary packet framing stripped by
// SerialGuidanceChannel
// ---------------------------------------------------------------------------

test(TrimbleParser, uses_parity_as_checksum) {
    TrimbleParser p;
    assertTrue(p.useParityAsChecksum());
}

test(TrimbleParser, claims_only_roxte) {
    TrimbleParser p;
    assertTrue(p.claimsSentenceType("ROXTE"));
    assertFalse(p.claimsSentenceType("GPGGA"));
    assertFalse(p.claimsSentenceType("0CFFFF2A"));
}

// ROXTE,0.050 -- xte = 5 cm
test(TrimbleParser, xte_positive) {
    GuidanceSource state;
    TrimbleParser p;
    p.claimsSentenceType("ROXTE");
    p.parseTerm(1, "0.050");
    p.commitTo(&state);
    assertEqual(state.GetXte(), 5);
}

// ROXTE,-0.120 -- xte = -12 cm
test(TrimbleParser, xte_negative) {
    GuidanceSource state;
    TrimbleParser p;
    p.claimsSentenceType("ROXTE");
    p.parseTerm(1, "-0.120");
    p.commitTo(&state);
    assertEqual(state.GetXte(), -12);
}

// ROXTE,0.000 -- xte = 0 cm
test(TrimbleParser, xte_zero) {
    GuidanceSource state;
    TrimbleParser p;
    p.claimsSentenceType("ROXTE");
    p.parseTerm(1, "0.000");
    p.commitTo(&state);
    assertEqual(state.GetXte(), 0);
}

// ---------------------------------------------------------------------------
// CanSerialParser -- CAN frames forwarded as ASCII hex (see known_good_sentences.txt)
// ---------------------------------------------------------------------------

// $0CFEF31C,00072A9C80652680*52  lat=52.0degN  lon=5.0degE
test(CanSerialParser, pos_lat_lon) {
    GuidanceSource state;
    CanSerialParser p;
    assertTrue(p.claimsSentenceType("0CFEF31C"));
    p.parseTerm(1, "00072A9C80652680");
    p.commitTo(&state);
    assertTrue(near(state.GetLatitude(), 52.0f, 1e-5f));
    assertTrue(near(state.GetLongitude(), 5.0f, 1e-5f));
}

// $0CFEE81C,002D00020000804F*5D  course=90.0  speed=2.0 kt  alt=44.0 m
test(CanSerialParser, spd_course_speed_alt) {
    GuidanceSource state;
    CanSerialParser p;
    assertTrue(p.claimsSentenceType("0CFEE81C"));
    p.parseTerm(1, "002D00020000804F");
    p.commitTo(&state);
    assertTrue(near(state.GetCourse(), 90.0f, 0.01f));
    assertTrue(near(state.GetSpeed(), 2.0f, 0.01f));
    assertTrue(near(state.GetAltitude(), 44.0f, 0.01f));
}

// $0CFFFF2A,001000007D000000*5E  xte=0  quality=4
test(CanSerialParser, xte_and_quality) {
    GuidanceSource state;
    CanSerialParser p;
    assertTrue(p.claimsSentenceType("0CFFFF2A"));
    p.parseTerm(1, "001000007D000000");
    p.commitTo(&state);
    assertEqual(state.GetXte(), 0);
    assertEqual((int)state.GetQuality(), 4);
}

test(CanSerialParser, rejects_unknown_header) {
    CanSerialParser p;
    assertFalse(p.claimsSentenceType("DEADBEEF"));
    assertFalse(p.claimsSentenceType("GPGGA"));
    assertFalse(p.claimsSentenceType("ROXTE"));
}

// Also accepts alternate CAN IDs for the same message type
test(CanSerialParser, pos_alternate_id) {
    GuidanceSource state;
    CanSerialParser p;
    assertTrue(p.claimsSentenceType("18FEF31C"));  // CAN_POS_ID2
    p.parseTerm(1, "00072A9C80652680");
    p.commitTo(&state);
    assertTrue(near(state.GetLatitude(), 52.0f, 1e-5f));
}
