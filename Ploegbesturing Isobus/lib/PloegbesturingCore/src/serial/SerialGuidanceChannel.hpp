/*
  SerialGuidanceChannel - NMEA/Trimble/CAN-over-serial guidance data acquisition
  Based on work by Maarten Lamers and Mikal Hart.
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

#include "CanSerialParser.hpp"
#include "GpsParser.hpp"
#include "GuidanceSource.hpp"
#include "NmeaParser.hpp"
#include "TrimbleParser.hpp"

namespace triton
{

// Character-level NMEA dispatcher -- the non-ISOBUS counterpart to
// IsobusGuidanceChannel, selected at compile time when ISOBUS is undefined
// (see ConfigPlough.hpp/platformio.ini). Reads bytes from a HardwareSerial
// port, splits them into terms, verifies checksums, and delegates sentence
// parsing to a set of GpsParser subclasses. Trimble binary packet framing
// (byte 191 / byte 3 / byte 16) is handled here.
class SerialGuidanceChannel {
public:
    SerialGuidanceChannel(Stream* serialDebug, HardwareSerial* serialGps, GuidanceSource* guidance);

    // Read available characters from the serial port; returns true when a
    // valid sentence was fully decoded this call. Call every loop() iteration.
    bool Update();

private:
    Stream*          serialDebug;
    HardwareSerial*  serialGps;
    GuidanceSource*  guidance;

    // Sub-parsers stored as value members -- no heap allocation needed
    NmeaParser      nmeaParser;
    TrimbleParser   trimbleParser;
    CanSerialParser canSerialParser;

    GpsParser* parsers[3];  // pointers into the three value members above
    GpsParser* activeParse; // parser that claimed the current sentence

    // Low-level parsing state
    char         term[20];
    byte         termNumber;
    byte         termOffset;
    byte         parity;
    byte         checksum;
    unsigned int sum;
    bool         isChecksumTerm;

    // Route the current term to the active parser or find a new active parser.
    // Returns true when a valid sentence was just committed.
    bool dispatchTerm();
};

}  // namespace triton
