/*
  test_VehicleGps - malformed-input coverage for the shared NMEA/Trimble parser
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
#include <Arduino.h>

#include "VehicleGps.hpp"

// VehicleGps lives in MeijWorks Libs and had no coverage on any platform: the
// Loofdoes native build shadows it with a 12-line stub, and neither plough
// project compiled it. It is also the only attacker-reachable surface in the
// codebase -- everything it parses arrives from a serial line.
//
// These tests drive it through a fake Stream, byte for byte, the way a receiver
// would. They are written to fail on the pre-fix code.

namespace {

// Feeds a fixed byte sequence to VehicleGps::Update().
class FakeGpsSerial : public HardwareSerial {
public:
    FakeGpsSerial() : data(nullptr), len(0), pos(0) {}

    void Load(const uint8_t* bytes, size_t count) {
        data = bytes;
        len  = count;
        pos  = 0;
    }

    int available() override { return (pos < len) ? int(len - pos) : 0; }
    int read() override      { return (pos < len) ? int(data[pos++]) : -1; }
    int peek() override      { return (pos < len) ? int(data[pos])   : -1; }
    size_t write(uint8_t) override { return 1; }

private:
    const uint8_t* data;
    size_t         len;
    size_t         pos;
};

class NullDebug : public Stream {
public:
    size_t write(uint8_t) override { return 1; }
};

// Drives every byte of `text` through the parser.
void feed(triton::VehicleGps& gps, FakeGpsSerial& port, const char* text) {
    port.Load(reinterpret_cast<const uint8_t*>(text), strlen(text));
    gps.Update();
}

void feedBytes(triton::VehicleGps& gps, FakeGpsSerial& port,
               const uint8_t* bytes, size_t count) {
    port.Load(bytes, count);
    gps.Update();
}

}  // namespace

// --- the out-of-bounds case -------------------------------------------------
//
// termOffset is reset to 0 by five separate cases, so a 0x03 arriving straight
// after a delimiter used to evaluate term[termOffset - 1] as term[-1]. If those
// out-of-bounds bytes satisfied the checksum arithmetic, term[-4] = '\0' wrote
// into the lastXteFix member sitting immediately before the buffer.
//
// Under ASan the pre-fix code traps here. Without it, the observable damage is
// the corrupted fix timestamp, which is what these assert on.

test(VehicleGps, packetEndAtZeroOffset_doesNotCorruptFixTimestamps) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    // Establish a known-good XTE fix so a stray write is visible as a change.
    feed(gps, port, "$GPXTE,A,A,1.23,L,N*6E\r\n");
    const unsigned long before = gps.GetXteFixAge();

    // A single 0x03 immediately after a comma: termOffset is 0 here.
    const uint8_t stray[] = { ',', 0x03 };
    feedBytes(gps, port, stray, sizeof(stray));

    assertEqual(before, gps.GetXteFixAge());
}

test(VehicleGps, packetEndAfterShortTerm_doesNotCorruptFixTimestamps) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    feed(gps, port, "$GPXTE,A,A,1.23,L,N*6E\r\n");
    const unsigned long before = gps.GetXteFixAge();

    // termOffset of 1, 2 and 3 underflow the same way, just less far.
    const uint8_t one[]   = { ',', 0x10, 0x03 };
    const uint8_t two[]   = { ',', 'A', 0x10, 0x03 };
    const uint8_t three[] = { ',', 'A', 'B', 0x10, 0x03 };
    feedBytes(gps, port, one,   sizeof(one));
    feedBytes(gps, port, two,   sizeof(two));
    feedBytes(gps, port, three, sizeof(three));

    assertEqual(before, gps.GetXteFixAge());
}

// --- checksum handling ------------------------------------------------------

test(VehicleGps, emptyChecksumField_rejected) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    // Prime term[] with digits, then send a sentence whose checksum field is
    // empty. term[0] is '\0' but term[1] still held a stale byte, which used to
    // be read as half the checksum.
    feed(gps, port, "$GPXTE,A,A,1.23,L,N*6E\r\n");
    const unsigned long before = gps.GetXteFixAge();

    feed(gps, port, "$GPXTE,A,A,9.99,L,N*\r\n");

    assertEqual(before, gps.GetXteFixAge());
}

test(VehicleGps, nonHexChecksumField_rejected) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    feed(gps, port, "$GPXTE,A,A,1.23,L,N*6E\r\n");
    const unsigned long before = gps.GetXteFixAge();

    feed(gps, port, "$GPXTE,A,A,9.99,L,N*ZZ\r\n");

    assertEqual(before, gps.GetXteFixAge());
}

test(VehicleGps, validSentence_stillAccepted) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    // The guards must not break the happy path.
    feed(gps, port, "$GPXTE,A,A,1.23,L,N*6E\r\n");

    assertNotEqual(0UL, gps.GetXteFixAge());
    // Hundredths of a metre. Note this parser does not apply the L/R direction
    // field to the sign -- field 3 is taken as-is.
    assertEqual(123, gps.GetXte());
}

test(VehicleGps, ggaSentence_parsesPositionAndQuality) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    feed(gps, port, "$GPGGA,123519,4807.038,N,01131.000,E,4,08,0.9,545.4,M,46.9,M,,*42\r\n");

    assertNotEqual(0UL, gps.GetGgaFixAge());
    assertEqual(4, (int)gps.GetQuality());
}

// --- XTE2 checksum bypass ---------------------------------------------------

test(VehicleGps, bareRoxteWithoutTrimbleFrame_rejected) {
    NullDebug      dbg;
    FakeGpsSerial  port;
    // millis() is a stateful mock that starts at 0; a fix recorded at 0 would be
    // indistinguishable from no fix at all. Kept well under AUnit's own 10 s
    // timeout, which is measured with this same mock.
    millisValue(2001);
    triton::VehicleGps gps(&dbg, &port);

    feed(gps, port, "$GPXTE,A,A,1.23,L,N*6E\r\n");
    const int xteBefore = gps.GetXte();

    // XTE2 substitutes the outer Trimble frame's integrity check for its own
    // checksum. Sent bare, with a deliberately wrong checksum, there is no outer
    // frame to stand behind it, so it must not be accepted.
    feed(gps, port, "@ROXTE,9.99*6F\r\n");

    assertEqual(xteBefore, gps.GetXte());
}
