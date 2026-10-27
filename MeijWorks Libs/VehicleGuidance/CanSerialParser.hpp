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
// Every frame is rebuilt into its eight payload bytes and decoded by the same
// IsobusPgnDecode functions a directly read bus uses, so a bridged bus
// commits exactly what a direct one would, J1939 "not available" values and
// plausibility checks per field included:
//  - legacy position 65267, speed 65256 and Trimble XTE (1CEBACAA): the
//    DecodeLegacy*() decoders, called directly so the Arduino Micro build
//    (GUIDANCE_LEAN) links only these;
//  - NMEA2000 single-frame PGNs (129025 position, 129026 COG/SOG, 129283
//    XTE): CanFrameGuidanceChannel::Decode(), not under GUIDANCE_LEAN.
// The John Deere XTE frame (0CFFFF2A) keeps its own decode: the CAN path's
// decoder also checks the message selector in byte 0, which is unverified
// on the serial bridges the Micro boards use.
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
    // become a plausible but invented fix (checked with GpsParser::termIsHex()).
    // Which fields inside a complete frame are usable is the decoders' call.
    bool fieldsValid = false;

    // The 29-bit identifier from the header (NMEA2000 lines) and the payload
    // bytes from the term, decoded in commitTo().
    uint32_t frameId = 0;
    uint8_t  frameData[8] = {0};

    // John Deere XTE only (see above).
    int   newXte = 0;
    byte  newQuality = 0;
};

}  // namespace triton
