/*
  NmeaParser - standard NMEA sentence parser (GPGGA, GPVTG, GPXTE, GPTXT)
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
    else if (strcmp(header, "GPTXT") == 0) { type = TXT; }
    else if (strcmp(header, "GNTXT") == 0) { type = TXT; }
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
    newSpeedValid = false;
    newCourseValid = false;
    newTimeValid = false;
    newLatValid = false;
    newLonValid = false;
    newAltValid = false;
    newQualityValid = false;
    newXteValid = false;
    sawAntennaText = false;
    return true;
}

void NmeaParser::parseTerm(byte termNumber, const char* term) {
    switch (type) {
        case GGA:
            switch (termNumber) {
                case 1: if (term[0]) { newTime = atof(term); newTimeValid = true; } break;
                case 2: if (term[0]) { float f = atof(term); int d = int(f) / 100; newLat = d + (f - d * 100) / 60.0f; newLatValid = true; } break;
                case 3: if (term[0] == 'S') newLat = -newLat; break;
                case 4: if (term[0]) { float f = atof(term); int d = int(f) / 100; newLon = d + (f - d * 100) / 60.0f; newLonValid = true; } break;
                case 5: if (term[0] == 'W') newLon = -newLon; break;
                case 6: if (term[0]) { newQuality = atoi(term); newQualityValid = true; } break;
                case 9: if (term[0]) { newAlt = atof(term); newAltValid = true; } break;
            }
            break;
        case VTG:
            if (termNumber == 1 && term[0]) { newCourse = atof(term); newCourseValid = true; }
            if (termNumber == 5 && term[0]) { newSpeed = atof(term); newSpeedValid = true; }
            break;
        case XTE:
            if (termNumber == 3 && term[0]) { newXte = (int)(atof(term) * 100); newXteValid = true; }
            break;
        case TXT:
            // $GPTXT,msgTotal,msgNum,severity,text -- text is term 4. Only
            // the exact strings the ATGM336H uses for its antenna
            // supervisor are acted on; a version banner or any other text
            // (or another receiver's TXT sentence entirely) leaves
            // sawAntennaText false and commitTo() below touches nothing.
            if (termNumber == 4) {
                if (strcmp(term, "ANTENNA OPEN") == 0 || strcmp(term, "ANTENNA SHORT") == 0) {
                    newAntennaOk = false;
                    sawAntennaText = true;
                } else if (strcmp(term, "ANTENNA OK") == 0) {
                    newAntennaOk = true;
                    sawAntennaText = true;
                }
            }
            break;
        case NONE: break;
    }
}

void NmeaParser::commitTo(GuidanceSource* state) {
    switch (type) {
        case GGA:
            // Same meaning as the ISOBUS path (GuidanceCommit.hpp GApply):
            // the GGA timestamp is the fix age, stamped only when the
            // receiver reports a fix (quality > 0). Quality 0 is a genuine
            // "no fix" report and is committed; it just doesn't refresh the
            // fix age, so the 2000 ms interlocks see the fix go stale.
            if (newQualityValid) state->SetQuality(newQuality);
            if (newTimeValid) state->SetTime(newTime);
            if (newQualityValid && newQuality > 0) {
                state->NoteGgaFixReceived();
                if (newLatValid && newLonValid) state->SetPosition(newLat, newLon);
                if (newAltValid) state->SetAltitude(newAlt);
            }
            break;
        case VTG:
            // A blank field means the receiver has nothing to report
            // (typically antenna loss, or no course while standing still),
            // not a genuine zero -- committing it would stamp
            // GuidanceSource's fix timestamp anyway and mask a real loss of
            // signal from the guidance-timeout check.
            if (newCourseValid) state->SetCourseDeg(newCourse);
            if (newSpeedValid) state->SetSpeedKnots(newSpeed);
            break;
        case XTE:
            if (newXteValid) state->SetXte(newXte);
            break;
        case TXT:
            if (sawAntennaText) state->SetAntennaOk(newAntennaOk);
            break;
        case NONE: break;
    }
}

}  // namespace triton
