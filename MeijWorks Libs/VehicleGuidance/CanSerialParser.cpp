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
#include "CanSerialParser.hpp"

#include <string.h>

#include "CanFrameGuidanceChannel.hpp"
#include "IsobusPgnDecode.hpp"

namespace triton
{

bool CanSerialParser::termIsHex(const char* term, byte need) {
    for (byte i = 0; i < need; i++) {
        const char c = term[i];
        const bool isHex = (c >= '0' && c <= '9') ||
                           (c >= 'A' && c <= 'F') ||
                           (c >= 'a' && c <= 'f');
        if (!isHex) return false;   // the terminator of a short term included
    }
    return true;
}

bool CanSerialParser::claimsSentenceType(const char* header) {
    fieldsValid = false;

    // Standard NMEA2000, matched on the PGN inside the identifier so any
    // source address qualifies, as on the CAN paths. Only the single-frame
    // PGNs: 129029 GNSS Position Data (1DF805xx, VehicleGps' CAN_POS_TERM3)
    // is a fast-packet message spread over seven frames, and one bridge line
    // holds only its first fragment, so it is left unclaimed rather than
    // mis-decoded; the same receivers broadcast 129025 for position.
    if (termIsHex(header, 8) && header[8] == '\0') {
        uint32_t id = 0;
        for (byte i = 0; i < 8; i++) id = (id << 4) | hexToInt(header[i]);
        const uint32_t pgn = CanFrameGuidanceChannel::PgnFromId(id);
        if (pgn == kPgnPositionNmea2000 || pgn == kPgnSpeedNmea2000 || pgn == kPgnXteNmea2000) {
            type = NMEA2000;
            frameId = id;
            return true;
        }
    }

    // Legacy JD proprietary frames, by exact identifier as VehicleGps did.
    if (strcmp(header, "0CFEF31C") == 0 || strcmp(header, "18FEF31C") == 0) {
        type = CAN_POS;
    } else if (strcmp(header, "0CFEE81C") == 0 || strcmp(header, "18FEE81C") == 0) {
        type = CAN_SPD;
    } else if (strcmp(header, "0CFFFF2A") == 0) {
        type = CAN_XTE;
    } else if (strcmp(header, "1CEBACAA") == 0) {
        type = CAN_XTE2;
    } else {
        return false;
    }

    newLat = newLon = newSpeed = newCourse = newAlt = 0;
    newXte = 0; newQuality = 0;
    return true;
}

void CanSerialParser::parseTerm(byte termNumber, const char* term) {
    if (termNumber != 1) return;

    unsigned long val = 0;

    switch (type) {
        case CAN_POS:
            if (!termIsHex(term, 16)) return;
            fieldsValid = true;
            for (int i = 7; i >= 0; i -= 2)
                val = (val << 8) + (hexToInt(term[i - 1]) << 4) + hexToInt(term[i]);
            newLat = float(long(val - 2100000000)) / 10000000;

            val = 0;
            for (int i = 15; i >= 8; i -= 2)
                val = (val << 8) + (hexToInt(term[i - 1]) << 4) + hexToInt(term[i]);
            newLon = float(long(val - 2100000000)) / 10000000;
            break;

        case CAN_SPD:
            if (!termIsHex(term, 16)) return;
            fieldsValid = true;
            val = ((unsigned long)hexToInt(term[2]) << 12) | ((unsigned long)hexToInt(term[3]) << 8)
                | ((unsigned long)hexToInt(term[0]) << 4) | hexToInt(term[1]);
            newCourse = float(val) / 128;

            val = ((unsigned long)hexToInt(term[6]) << 12) | ((unsigned long)hexToInt(term[7]) << 8)
                | ((unsigned long)hexToInt(term[4]) << 4) | hexToInt(term[5]);
            newSpeed = float(val) / 256;

            val = ((unsigned long)hexToInt(term[14]) << 12) | ((unsigned long)hexToInt(term[15]) << 8)
                | ((unsigned long)hexToInt(term[12]) << 4) | hexToInt(term[13]);
            newAlt = float(val) / 8 - 2500;
            break;

        case CAN_XTE: {
            if (!termIsHex(term, 10)) return;
            fieldsValid = true;
            val = ((unsigned long)hexToInt(term[8]) << 12) | ((unsigned long)hexToInt(term[9]) << 8)
                | ((unsigned long)hexToInt(term[6]) << 4) | hexToInt(term[7]);
            newXte = int(val - 32000) >> 1;
            newQuality = (term[2] == '1') ? 4 : 0;
            break;
        }

        case CAN_XTE2: {
            if (!termIsHex(term, 12)) return;
            fieldsValid = true;
            int temp = (hexToInt(term[0]) << 4) + hexToInt(term[1]);
            int temp2 = (hexToInt(term[10]) << 4) + hexToInt(term[11]);
            if (temp == 2 && temp2 == 7) {
                union { unsigned long a; float b; } tofloat;
                tofloat.a = ((unsigned long)hexToInt(term[2]) << 28)
                          | ((unsigned long)hexToInt(term[3]) << 24)
                          | ((unsigned long)hexToInt(term[4]) << 20)
                          | ((unsigned long)hexToInt(term[5]) << 16)
                          | ((unsigned long)hexToInt(term[6]) << 12)
                          | ((unsigned long)hexToInt(term[7]) << 8)
                          | ((unsigned long)hexToInt(term[8]) << 4)
                          | hexToInt(term[9]);
                newXte = (int)(tofloat.b * 100);
            }
            newQuality = 4;  // TODO: parse actual quality flag
            break;
        }

        case NMEA2000:
            // Sixteen hex digits back into the eight payload bytes; the
            // layout is the decoder's business, not this parser's.
            if (!termIsHex(term, 16)) return;
            for (byte i = 0; i < 8; i++)
                frameData[i] = uint8_t((hexToInt(term[2 * i]) << 4) | hexToInt(term[2 * i + 1]));
            fieldsValid = true;
            break;

        case NONE: break;
    }
}

void CanSerialParser::commitTo(GuidanceSource* state) {
    if (!fieldsValid) return;   // short or non-hex payload: nothing to commit

    switch (type) {
        case NMEA2000:
            CanFrameGuidanceChannel::Decode(frameId, frameData, sizeof(frameData), state);
            break;
        case CAN_POS:
            state->SetPosition(newLat, newLon);
            break;
        case CAN_SPD:
            state->SetCourseDeg(newCourse);
            state->SetSpeedKnots(newSpeed);
            state->SetAltitude(newAlt);
            break;
        case CAN_XTE:
        case CAN_XTE2:
            state->SetXte(newXte);
            state->SetQuality(newQuality);
            break;
        case NONE: break;
    }
}

}  // namespace triton
