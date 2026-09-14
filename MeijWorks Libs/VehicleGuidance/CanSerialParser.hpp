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
// The sentence "type" is the CAN arbitration ID in hex (e.g. "0CFEF31C").
class CanSerialParser : public GpsParser {
public:
    bool claimsSentenceType(const char* header) override;
    void parseTerm(byte termNumber, const char* term) override;
    void commitTo(GuidanceSource* state) override;

private:
    enum Type : byte { CAN_POS, CAN_SPD, CAN_XTE, CAN_XTE2, NONE } type = NONE;

    float newLat = 0, newLon = 0;
    float newSpeed = 0, newCourse = 0, newAlt = 0;
    int   newXte = 0;
    byte  newQuality = 0;
};

}  // namespace triton
