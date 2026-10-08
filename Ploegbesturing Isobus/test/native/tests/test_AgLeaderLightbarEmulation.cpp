/*
  test_AgLeaderLightbarEmulation - the L160 identification exchange replayed
  byte for byte from card log 42 (2026-10-08), plus the pacing and the
  negative responses the capture could not show.
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
#include <cstring>
#include "isobus/AgLeaderLightbarEmulation.hpp"

using namespace aunit;
using namespace triton;

namespace {

constexpr std::uint8_t kDisplay = 0xF5;

bool sameFrame(const AgLeaderLightbarEmulation::Frame& f, const std::uint8_t* expected, std::uint8_t length) {
    if (f.length != length) return false;
    return std::memcmp(f.data, expected, length) == 0;
}

// Pops everything queued at `now` into `out`, returns the count.
int drain(AgLeaderLightbarEmulation& e, AgLeaderLightbarEmulation::Outgoing* out, int max, unsigned long now) {
    int n = 0;
    while (n < max && e.PopFrame(out[n], now)) n++;
    return n;
}

}  // namespace

// 105.29 s: `05 22 80 03 01 01` -> `05 62 80 03 01 19 FF FF`. The very first
// request also produces the Proprietary A hello, before the answer.
test(AgLeaderLightbarEmulation, firstRequest_helloThenSingleFrameAnswer) {
    AgLeaderLightbarEmulation e;
    const std::uint8_t request[6] = { 0x05, 0x22, 0x80, 0x03, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, request, 6, 1000));

    AgLeaderLightbarEmulation::Outgoing out[4];
    assertEqual(drain(e, out, 4, 1000), 2);

    const std::uint8_t hello[8] = { 0x00, 0x02, 0x00, 0x64, 0x00, 0xFF, 0xFF, 0xFF };
    assertEqual(out[0].pgn, AgLeaderLightbarEmulation::kPgnProprietaryA);
    assertEqual(out[0].destination, kDisplay);
    assertTrue(sameFrame(out[0].frame, hello, 8));

    const std::uint8_t answer[8] = { 0x05, 0x62, 0x80, 0x03, 0x01, 0x19, 0xFF, 0xFF };
    assertEqual(out[1].pgn, AgLeaderLightbarEmulation::kPgnDiagnostic);
    assertEqual(out[1].destination, kDisplay);
    assertTrue(sameFrame(out[1].frame, answer, 8));

    assertEqual(e.GetPartnerAddress(), kDisplay);
    assertEqual(e.GetRequests(), (std::uint32_t)1);
    assertEqual(e.GetResponses(), (std::uint32_t)1);
    assertEqual((int)e.GetLastDid(), 0x8003);
    assertTrue(e.GetHelloSent());
}

// The hello goes once; the four single-frame identifiers answer as captured.
test(AgLeaderLightbarEmulation, singleFrameIdentifiers_matchTheCapture) {
    AgLeaderLightbarEmulation e;
    AgLeaderLightbarEmulation::Outgoing out[4];

    const std::uint8_t r8007[6] = { 0x05, 0x22, 0x80, 0x07, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, r8007, 6, 0));
    assertEqual(drain(e, out, 4, 0), 2);   // hello + answer
    const std::uint8_t a8007[8] = { 0x07, 0x62, 0x80, 0x07, 0x77, 0xCF, 0xDA, 0x0E };
    assertTrue(sameFrame(out[1].frame, a8007, 8));

    const std::uint8_t r8009[6] = { 0x05, 0x22, 0x80, 0x09, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, r8009, 6, 0));
    assertEqual(drain(e, out, 4, 0), 1);
    const std::uint8_t a8009[8] = { 0x07, 0x62, 0x80, 0x09, 0x06, 0x00, 0x00, 0x00 };
    assertTrue(sameFrame(out[0].frame, a8009, 8));

    const std::uint8_t r8015[6] = { 0x05, 0x22, 0x80, 0x15, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, r8015, 6, 0));
    assertEqual(drain(e, out, 4, 0), 1);
    const std::uint8_t a8015[8] = { 0x07, 0x62, 0x80, 0x15, 0x01, 0x00, 0x00, 0x00 };
    assertTrue(sameFrame(out[0].frame, a8015, 8));

    assertEqual(e.GetRequests(), (std::uint32_t)3);
    assertEqual(e.GetResponses(), (std::uint32_t)3);
}

// 105.37-105.61 s, identifier 0x8006: first frame `10 13 62 80 06 41 4C 20`,
// the display's flow control `30 32 01`, then `21 4C 31 36 30 20 00 00` and
// `22 00 00 00 00 00 00` (DLC 7).
test(AgLeaderLightbarEmulation, stringIdentifier_firstFrameFlowControlConsecutive) {
    AgLeaderLightbarEmulation e;
    AgLeaderLightbarEmulation::Outgoing out[4];

    const std::uint8_t request[6] = { 0x05, 0x22, 0x80, 0x06, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, request, 6, 100));
    assertEqual(drain(e, out, 4, 100), 2);   // hello + first frame
    const std::uint8_t first[8] = { 0x10, 0x13, 0x62, 0x80, 0x06, 0x41, 0x4C, 0x20 };
    assertTrue(sameFrame(out[1].frame, first, 8));

    // Nothing more until the flow control arrives.
    assertEqual(drain(e, out, 4, 150), 0);

    const std::uint8_t flow[8] = { 0x30, 0x32, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    assertTrue(e.OnDiagnosticFrame(kDisplay, flow, 8, 200));

    // STmin 1 ms: the first consecutive frame is held until 1 ms has passed.
    assertEqual(drain(e, out, 4, 200), 0);
    assertEqual(drain(e, out, 4, 201), 1);
    const std::uint8_t cf1[8] = { 0x21, 0x4C, 0x31, 0x36, 0x30, 0x20, 0x00, 0x00 };
    assertTrue(sameFrame(out[0].frame, cf1, 8));
    assertEqual(out[0].pgn, AgLeaderLightbarEmulation::kPgnDiagnostic);
    assertEqual(out[0].destination, kDisplay);

    assertEqual(drain(e, out, 4, 202), 1);
    const std::uint8_t cf2[7] = { 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    assertTrue(sameFrame(out[0].frame, cf2, 7));

    assertEqual(drain(e, out, 4, 300), 0);
}

// 0x8008 carries the part number, 0x8014 the model again without the trailing
// space -- both as 16-byte zero-padded fields, as in the capture.
test(AgLeaderLightbarEmulation, partNumberAndModel_payloads) {
    AgLeaderLightbarEmulation e;
    AgLeaderLightbarEmulation::Outgoing out[4];
    const std::uint8_t flow[8] = { 0x30, 0x32, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    const std::uint8_t r8008[6] = { 0x05, 0x22, 0x80, 0x08, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, r8008, 6, 0));
    assertEqual(drain(e, out, 4, 0), 2);
    const std::uint8_t first8008[8] = { 0x10, 0x13, 0x62, 0x80, 0x08, 0x34, 0x30, 0x30 };
    assertTrue(sameFrame(out[1].frame, first8008, 8));
    assertTrue(e.OnDiagnosticFrame(kDisplay, flow, 8, 10));
    assertEqual(drain(e, out, 4, 20), 1);
    const std::uint8_t cf8008[8] = { 0x21, 0x31, 0x35, 0x39, 0x35, 0x00, 0x00, 0x00 };
    assertTrue(sameFrame(out[0].frame, cf8008, 8));
    assertEqual(drain(e, out, 4, 30), 1);

    const std::uint8_t r8014[6] = { 0x05, 0x22, 0x80, 0x14, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, r8014, 6, 40));
    assertEqual(drain(e, out, 4, 40), 1);
    const std::uint8_t first8014[8] = { 0x10, 0x13, 0x62, 0x80, 0x14, 0x41, 0x4C, 0x20 };
    assertTrue(sameFrame(out[0].frame, first8014, 8));
    assertTrue(e.OnDiagnosticFrame(kDisplay, flow, 8, 50));
    assertEqual(drain(e, out, 4, 60), 1);
    const std::uint8_t cf8014[8] = { 0x21, 0x4C, 0x31, 0x36, 0x30, 0x00, 0x00, 0x00 };
    assertTrue(sameFrame(out[0].frame, cf8014, 8));
}

// An identifier the L160 was never asked for gets a UDS negative response
// (request out of range), and a service other than ReadDataByIdentifier gets
// service-not-supported. Neither counts as a response.
test(AgLeaderLightbarEmulation, unknownIdentifierAndService_negativeResponses) {
    AgLeaderLightbarEmulation e;
    AgLeaderLightbarEmulation::Outgoing out[4];

    const std::uint8_t unknown[6] = { 0x05, 0x22, 0x80, 0x99, 0x01, 0x01 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, unknown, 6, 0));
    assertEqual(drain(e, out, 4, 0), 2);
    const std::uint8_t nrc1[8] = { 0x03, 0x7F, 0x22, 0x31, 0xFF, 0xFF, 0xFF, 0xFF };
    assertTrue(sameFrame(out[1].frame, nrc1, 8));

    const std::uint8_t tester[3] = { 0x02, 0x3E, 0x00 };
    assertTrue(e.OnDiagnosticFrame(kDisplay, tester, 3, 0));
    assertEqual(drain(e, out, 4, 0), 1);
    const std::uint8_t nrc2[8] = { 0x03, 0x7F, 0x3E, 0x11, 0xFF, 0xFF, 0xFF, 0xFF };
    assertTrue(sameFrame(out[0].frame, nrc2, 8));

    assertEqual(e.GetRequests(), (std::uint32_t)2);
    assertEqual(e.GetResponses(), (std::uint32_t)0);
    assertEqual(e.GetUnknownRequests(), (std::uint32_t)2);
}

// A flow control from somebody else, or with nothing pending, is ignored; a
// consecutive frame addressed to us is not something this responder handles.
test(AgLeaderLightbarEmulation, strayFramesAreIgnored) {
    AgLeaderLightbarEmulation e;
    const std::uint8_t flow[8] = { 0x30, 0x32, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    assertFalse(e.OnDiagnosticFrame(kDisplay, flow, 8, 0));
    const std::uint8_t consecutive[8] = { 0x21, 0, 0, 0, 0, 0, 0, 0 };
    assertFalse(e.OnDiagnosticFrame(kDisplay, consecutive, 8, 0));
    const std::uint8_t tooShort[2] = { 0x05, 0x22 };
    assertFalse(e.OnDiagnosticFrame(kDisplay, tooShort, 2, 0));
    assertEqual(e.GetRequests(), (std::uint32_t)0);
}

// The heartbeat is due immediately and then every 1667 ms, as the L160 sends
// 65513 at ~0.6 Hz.
test(AgLeaderLightbarEmulation, heartbeatEvery1667ms) {
    AgLeaderLightbarEmulation e;
    assertTrue(e.HeartbeatDue(5000));
    assertFalse(e.HeartbeatDue(5100));
    assertFalse(e.HeartbeatDue(6666));
    assertTrue(e.HeartbeatDue(6667));
    assertFalse(e.HeartbeatDue(6668));
}

// 52 bytes: the field count and the string the L160 broadcast on PGN 65242.
test(AgLeaderLightbarEmulation, softwareIdentification_isTheCapturedString) {
    std::uint8_t length = 0;
    const std::uint8_t* id = AgLeaderLightbarEmulation::SoftwareIdentification(length);
    assertEqual((int)length, 52);
    assertEqual((int)id[0], 1);
    assertEqual(std::memcmp(id + 1, "ALTECH,AL L160;01.00.00.00;L160_UP_FW;01.05.00.00;*", 51), 0);
}

// Proprietary A from the display is counted and names the partner when no
// request has yet.
test(AgLeaderLightbarEmulation, proprietaryAFromDisplay_counted) {
    AgLeaderLightbarEmulation e;
    e.OnProprietaryA(kDisplay);
    assertEqual(e.GetProprietaryAReceived(), (std::uint32_t)1);
    assertEqual(e.GetPartnerAddress(), kDisplay);
}
