/*
  test_RemoteLineBuffer - Tests for RemoteLineBuffer: line assembly from a byte stream that
  arrives in arbitrary pieces, and what happens on overflow or over-long lines.
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
#include <string.h>
#include "remote/RemoteLineBuffer.hpp"

using namespace aunit;
using namespace triton;

static void push(RemoteLineBuffer& b, const char* s) {
    b.Push(reinterpret_cast<const uint8_t*>(s), (int)strlen(s));
}

test(RemoteLineBuffer, singleLine_poppedWithoutTerminator) {
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    push(b, "PING\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "PING");
    assertFalse(b.PopLine(out, sizeof(out)));
}

test(RemoteLineBuffer, crlf_stripped) {
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    push(b, "CAL GET\r\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "CAL GET");
}

test(RemoteLineBuffer, partial_waitsForNewline) {
    // BLE writes arrive in whatever pieces the phone's stack makes of them.
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    push(b, "CFG S");
    assertFalse(b.PopLine(out, sizeof(out)));
    push(b, "ET width_cm 4");
    assertFalse(b.PopLine(out, sizeof(out)));
    push(b, "50\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "CFG SET width_cm 450");
}

test(RemoteLineBuffer, twoLinesInOnePush_poppedInOrder) {
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    push(b, "PING\nINFO\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "PING");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "INFO");
    assertFalse(b.PopLine(out, sizeof(out)));
}

test(RemoteLineBuffer, emptyLines_skipped) {
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    push(b, "\n\r\n\nPING\n\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "PING");
    assertFalse(b.PopLine(out, sizeof(out)));
}

test(RemoteLineBuffer, overlongLine_droppedAndNextOneSurvives) {
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    char big[RemoteLineBuffer::kMaxLine + 20];
    memset(big, 'X', sizeof(big) - 2);
    big[sizeof(big) - 2] = '\n';
    big[sizeof(big) - 1] = 0;
    push(b, big);
    push(b, "PING\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "PING");
    assertFalse(b.PopLine(out, sizeof(out)));
}

test(RemoteLineBuffer, overflow_dropsThePartialLineItCut) {
    // More bytes than fit and no newline in sight: whatever was queued is a
    // truncated line, so it must not be glued to the next command and
    // handed over as if it were one.
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    char flood[RemoteLineBuffer::kCapacity + 50];
    memset(flood, 'F', sizeof(flood) - 1);
    flood[sizeof(flood) - 1] = 0;
    int accepted = b.Push(reinterpret_cast<const uint8_t*>(flood), (int)strlen(flood));
    assertLessOrEqual(accepted, RemoteLineBuffer::kCapacity);

    push(b, "PING\n");          // may be dropped too, or arrive as the tail of garbage
    while (b.PopLine(out, sizeof(out))) {
        assertNotEqual(out, "PING");    // never as a clean command
    }
    push(b, "OK\n");
    assertTrue(b.PopLine(out, sizeof(out)));
    assertEqual(out, "OK");
}

test(RemoteLineBuffer, wraparound_manyLines) {
    RemoteLineBuffer b;
    char out[RemoteLineBuffer::kMaxLine];
    for (int i = 0; i < 200; ++i) {
        char line[32];
        snprintf(line, sizeof(line), "CMD %d\n", i);
        push(b, line);
        assertTrue(b.PopLine(out, sizeof(out)));
        char expect[32];
        snprintf(expect, sizeof(expect), "CMD %d", i);
        assertEqual(out, expect);
    }
    assertEqual(b.Available(), 0);
}

test(RemoteLineBuffer, outputTooSmall_lineDropped) {
    RemoteLineBuffer b;
    char tiny[4];
    push(b, "PING\n");
    assertFalse(b.PopLine(tiny, sizeof(tiny)));
    push(b, "OK\n");
    assertTrue(b.PopLine(tiny, sizeof(tiny)));
    assertEqual(tiny, "OK");
}
