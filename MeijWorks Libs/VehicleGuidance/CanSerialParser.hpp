/*
  CanSerialParser - CAN-over-serial ASCII hex GPS sentence parser
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

#include "GpsParser.hpp"

namespace triton
{

// Parses CAN frames forwarded as ASCII hex strings on the NMEA serial port.
// The sentence "type" is the CAN arbitration ID in hex (e.g. "0CFEF31C"),
// the single data term the frame's eight payload bytes as sixteen hex digits.
//
// Two families arrive this way. The legacy JD proprietary frames keep the
// layouts VehicleGps decoded. The standard NMEA2000 single-frame PGNs the
// same receivers broadcast (129025 position, 129026 COG/SOG, 129283 XTE) are
// rebuilt into bytes and handed to CanFrameGuidanceChannel::Decode(), so a
// bridged bus commits exactly what a directly read bus would.
class CanSerialParser : public GpsParser {
public:
    bool claimsSentenceType(const char* header) override;
    void parseTerm(byte termNumber, const char* term) override;
    void commitTo(GuidanceSource* state) override;

private:
    enum Type : byte { CAN_POS, CAN_SPD, CAN_XTE, CAN_XTE2, NMEA2000, NONE } type = NONE;

    // Set by parseTerm() only when the payload term had the full hex length
    // the layout needs; commitTo() leaves the source untouched otherwise. A
    // frame with DLC < 8, or a line cut short by a baudrate change, must not
    // become a plausible but invented fix (VehicleGps guarded this with
    // termIsHex()).
    bool fieldsValid = false;
    static bool termIsHex(const char* term, byte need);

    // NMEA2000 lines: the 29-bit identifier from the header and the payload
    // bytes from the term, decoded in commitTo().
    uint32_t frameId = 0;
    uint8_t  frameData[8] = {0};

    float newLat = 0, newLon = 0;
    float newSpeed = 0, newCourse = 0, newAlt = 0;
    int   newXte = 0;
    byte  newQuality = 0;
};

}  // namespace triton
