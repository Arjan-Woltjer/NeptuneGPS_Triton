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

namespace triton
{

bool CanSerialParser::claimsSentenceType(const char* header) {
    if (strcmp(header, "0CFEF31C") == 0 || strcmp(header, "18FEF31C") == 0 || strcmp(header, "1DF8051C") == 0) {
        type = CAN_POS;
    } else if (strcmp(header, "0CFEE81C") == 0 || strcmp(header, "18FEE81C") == 0 || strcmp(header, "1DF8021C") == 0) {
        type = CAN_SPD;
    } else if (strcmp(header, "0CFFFF2A") == 0 || strcmp(header, "1DF9031C") == 0) {
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
            for (int i = 7; i >= 0; i -= 2)
                val = (val << 8) + (hexToInt(term[i - 1]) << 4) + hexToInt(term[i]);
            newLat = float(long(val - 2100000000)) / 10000000;

            val = 0;
            for (int i = 15; i >= 8; i -= 2)
                val = (val << 8) + (hexToInt(term[i - 1]) << 4) + hexToInt(term[i]);
            newLon = float(long(val - 2100000000)) / 10000000;
            break;

        case CAN_SPD:
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
            val = ((unsigned long)hexToInt(term[8]) << 12) | ((unsigned long)hexToInt(term[9]) << 8)
                | ((unsigned long)hexToInt(term[6]) << 4) | hexToInt(term[7]);
            newXte = int(val - 32000) >> 1;
            newQuality = (term[2] == '1') ? 4 : 0;
            break;
        }

        case CAN_XTE2: {
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

        case NONE: break;
    }
}

void CanSerialParser::commitTo(GuidanceSource* state) {
    switch (type) {
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
