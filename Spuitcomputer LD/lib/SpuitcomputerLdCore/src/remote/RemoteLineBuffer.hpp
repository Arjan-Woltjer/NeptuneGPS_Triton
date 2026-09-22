/*
  RemoteLineBuffer - byte queue that hands out complete lines, between a link and RemoteSprayer
  Copyright (C) 2011-2026 J.A. Woltjer.

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

#include <stdint.h>

namespace triton
{

// Fixed-size byte ring between whoever receives command bytes (the BLE
// stack, on its own task) and RemoteSprayer (on the loop task). Bytes go in
// however they arrive -- a command split over several writes, or several
// commands in one -- and come out as whole lines without CR/LF.
//
// Locking is the caller's business: BleSprayer wraps Push() and PopLine() in
// a critical section. This class has no platform dependencies so the line
// assembly can be tested natively.
class RemoteLineBuffer {
public:
    static constexpr int kCapacity = 256;   // bytes queued at most
    static constexpr int kMaxLine  = 96;    // matches RemoteSprayer::kMaxLineLength

    RemoteLineBuffer() : head(0), tail(0), count(0), truncated(false) {}

    // Producer side. Returns how many bytes were accepted. Once the ring is
    // full the rest is dropped and the line being assembled is marked, so the
    // consumer throws it away instead of handing over a cut command.
    int Push(const uint8_t* data, int len) {
        int accepted = 0;
        for (int i = 0; i < len; ++i) {
            if (count >= kCapacity) {
                truncated = true;
                break;
            }
            buf[head] = data[i];
            head = (head + 1) % kCapacity;
            ++count;
            ++accepted;
        }
        return accepted;
    }

    // Consumer side. Copies the next complete line into `out`, NUL-terminated,
    // and returns true. Empty lines, lines that do not fit `out`, and a line
    // that was cut by an overflow are skipped.
    bool PopLine(char* out, int outSize) {
        for (;;) {
            int newlineAt = -1;
            for (int i = 0; i < count; ++i) {
                if (buf[(tail + i) % kCapacity] == '\n') { newlineAt = i; break; }
            }
            if (newlineAt < 0) {
                // A full ring with no newline in it can never complete: it is
                // one over-long or cut line. Drop it and start afresh with
                // whatever arrives next.
                if (truncated && count >= kCapacity) {
                    tail      = head;
                    count     = 0;
                    truncated = false;
                }
                return false;
            }

            int  n  = 0;
            bool ok = true;
            for (int i = 0; i < newlineAt; ++i) {
                const char c = (char)buf[(tail + i) % kCapacity];
                if (c == '\r') continue;
                if (n < outSize - 1) out[n++] = c;
                else                 ok = false;
            }
            tail   = (tail + newlineAt + 1) % kCapacity;
            count -= newlineAt + 1;

            if (truncated) { truncated = false; continue; }   // the cut line
            if (!ok || n == 0) continue;                        // too long, or empty
            out[n] = 0;
            return true;
        }
    }

    int Available() const { return count; }

private:
    uint8_t buf[kCapacity] = {};
    int     head;
    int     tail;
    int     count;
    bool    truncated;
};

}  // namespace triton
