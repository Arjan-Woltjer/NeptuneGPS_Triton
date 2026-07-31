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

    // Shared hex utility, available to all parsers via GpsParser::hexToInt().
    static byte hexToInt(char c) {
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return c - '0';
    }
};

}  // namespace triton
