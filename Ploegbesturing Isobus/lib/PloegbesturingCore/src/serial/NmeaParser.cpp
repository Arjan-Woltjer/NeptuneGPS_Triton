/*
  NmeaParser - standard NMEA sentence parser (GPGGA, GPVTG, GPXTE)
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
#include "NmeaParser.hpp"

#include <string.h>

namespace triton
{

bool NmeaParser::claimsSentenceType(const char* header) {
    if      (strcmp(header, "GPGGA") == 0) { type = GGA; }
    else if (strcmp(header, "GNGGA") == 0) { type = GGA; }
    else if (strcmp(header, "GPVTG") == 0) { type = VTG; }
    else if (strcmp(header, "GNVTG") == 0) { type = VTG; }
    else if (strcmp(header, "GPXTE") == 0) { type = XTE; }
    else if (strcmp(header, "GNXTE") == 0) { type = XTE; }
    else { return false; }

    // Reset temporaries at sentence start
    newTime = 0;
    newLat = 0;
    newLon = 0;
    newAlt = 0;
    newSpeed = 0;
    newCourse = 0;
    newXte = 0;
    newQuality = 0;
    return true;
}

void NmeaParser::parseTerm(byte termNumber, const char* term) {
    switch (type) {
        case GGA:
            switch (termNumber) {
                case 1: newTime = atof(term); break;
                case 2: { float f = atof(term); int d = int(f) / 100; newLat = d + (f - d * 100) / 60.0f; } break;
                case 3: if (term[0] == 'S') newLat = -newLat; break;
                case 4: { float f = atof(term); int d = int(f) / 100; newLon = d + (f - d * 100) / 60.0f; } break;
                case 5: if (term[0] == 'W') newLon = -newLon; break;
                case 6: newQuality = atoi(term); break;
                case 9: newAlt = atof(term); break;
            }
            break;
        case VTG:
            if (termNumber == 1) newCourse = atof(term);
            if (termNumber == 5) newSpeed = atof(term);
            break;
        case XTE:
            if (termNumber == 3) newXte = (int)(atof(term) * 100);
            break;
        case NONE: break;
    }
}

void NmeaParser::commitTo(GuidanceSource* state) {
    switch (type) {
        case GGA:
            state->SetPosition(newLat, newLon);
            state->SetAltitude(newAlt);
            state->SetTime(newTime);
            state->SetQuality(newQuality);
            break;
        case VTG:
            state->SetCourseDeg(newCourse);
            state->SetSpeedKnots(newSpeed);
            break;
        case XTE:
            state->SetXte(newXte);
            break;
        case NONE: break;
    }
}

}  // namespace triton
