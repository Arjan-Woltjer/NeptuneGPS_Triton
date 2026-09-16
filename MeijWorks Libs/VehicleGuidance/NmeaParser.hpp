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
#pragma once

#include "GpsParser.hpp"

namespace triton
{

// Handles standard NMEA sentences: GPGGA, GPVTG, GPXTE.
class NmeaParser : public GpsParser {
public:
    bool claimsSentenceType(const char* header) override;
    void parseTerm(byte termNumber, const char* term) override;
    void commitTo(GuidanceSource* state) override;

private:
    enum Type : byte { GGA, VTG, XTE, NONE } type = NONE;

    float newTime = 0, newLat = 0, newLon = 0, newAlt = 0;
    float newSpeed = 0, newCourse = 0;
    int   newXte = 0;
    byte  newQuality = 0;

    // A receiver with the antenna disconnected keeps sending VTG on schedule
    // with every field blank ("$GPVTG,,T,,M,,N,,K,N*xx") rather than falling
    // silent -- atof("") is 0, so without this the blank speed field
    // committed as a genuine "stopped" reading, GuidanceSource stamped it as
    // a fresh fix, and the 2000 ms guidance timeout (which assumes a lost
    // signal means the sentences stop arriving) never saw anything stale.
    // Set once term 5 (speed) is seen non-blank; commitTo() skips the VTG
    // commit entirely when it isn't, the same as a sentence that never
    // arrived at all.
    bool newSpeedValid = false;
};

}  // namespace triton
