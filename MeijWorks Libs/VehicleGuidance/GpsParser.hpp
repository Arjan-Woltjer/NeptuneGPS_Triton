/*
  GpsParser - abstract base class for GPS sentence parsers
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

#include "GuidanceSource.hpp"

namespace triton
{

// Abstract base for sentence-level GPS parsers.
// SerialGuidanceChannel owns the character loop; each GpsParser handles one
// family of sentence types and knows how to commit its parsed values to a
// GuidanceSource.
class GpsParser {
public:
    virtual ~GpsParser() = default;

    // Return true if this parser handles sentences whose first term equals
    // header. Implementations should also reset their temporaries here.
    virtual bool claimsSentenceType(const char* header) = 0;

    // Process one data term (termNumber >= 1) from the active sentence.
    virtual void parseTerm(byte termNumber, const char* term) = 0;

    // Commit the accumulated parsed values to state. Called once per valid
    // sentence immediately after the checksum passes.
    virtual void commitTo(GuidanceSource* state) = 0;

    // Return true if parity should be used as checksum directly.
    // Used by TrimbleParser: the outer Trimble packet already verified
    // integrity, so the inner ROXTE checksum is always parity itself.
    virtual bool useParityAsChecksum() const { return false; }

    // The value of one hex digit, 0 for anything else. A 0 cannot tell a bad
    // digit from a real '0', so callers check the term with termIsHex() first.
    static byte hexToInt(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return 0;
    }

    // True when the first `need` characters of term are all hex digits. A
    // term shorter than `need` fails on its terminator, so a short payload is
    // caught before hexToInt() reads past it.
    static bool termIsHex(const char* term, byte need) {
        for (byte i = 0; i < need; i++) {
            const char c = term[i];
            const bool isHex = (c >= '0' && c <= '9') ||
                               (c >= 'A' && c <= 'F') ||
                               (c >= 'a' && c <= 'f');
            if (!isHex) return false;
        }
        return true;
    }
};

}  // namespace triton
