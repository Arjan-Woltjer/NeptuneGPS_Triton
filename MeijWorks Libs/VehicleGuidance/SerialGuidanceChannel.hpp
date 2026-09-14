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

// Character-level dispatcher for a receiver on a UART -- the successor to
// VehicleGps::Update(), and the non-CAN counterpart to a project's ISOBUS
// guidance channel. Reads bytes from a HardwareSerial port, splits them into
// terms, verifies checksums, and delegates sentence parsing to a set of
// GpsParser subclasses that commit into a GuidanceSource. Trimble binary
// packet framing (byte 191 / byte 3 / byte 16) is handled here.
class SerialGuidanceChannel {
public:
    SerialGuidanceChannel(Stream* serialDebug, HardwareSerial* serialGps, GuidanceSource* guidance);

    // Read available characters from the serial port; returns true when a
    // valid sentence was fully decoded this call. Call every loop() iteration.
    bool Update();

    // Reopen the receiver port at another rate without a reboot (the Loofdoes
    // companion app changes it at runtime, NeptuneGPS_Triton#68). A partial
    // sentence read at the old rate is discarded.
    void ApplyBaudrate(long baud);

    // When enabled, every byte read from serialGps in Update() is echoed
    // verbatim to serialDebug -- raw NMEA/CAN passthrough for diagnostics
    // (NeptuneGPS_Triton#62). Off by default.
    inline void SetRawEcho(bool enable) { rawEcho = enable; }
    inline bool GetRawEcho() const      { return rawEcho; }

    // Sentence tap: the most recent complete line as received, printable
    // characters only (a Trimble binary frame shows up as its printable
    // remains), capped at the NMEA maximum, plus a counter that increments
    // once per line so a poller can notice a new one. Lines that arrive
    // between two reads are overwritten; a debugging aid, not a logger
    // (NeptuneGPS_Triton#62, #67).
    inline const char* GetLastSentence() const { return lastSentence; }
    inline uint32_t    GetSentenceSeq() const  { return sentenceSeq; }

private:
    static constexpr uint8_t kMaxSentence = 90;

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
    char         term[20] = {};
    byte         termNumber = 0;
    byte         termOffset = 0;
    byte         parity = 0;
    byte         checksum = 0;
    unsigned int sum = 0;
    bool         isChecksumTerm = false;

    // Diagnostics: raw echo and the sentence tap, independent of the parser
    bool     rawEcho = false;
    char     rawSentence[kMaxSentence + 1] = {};
    uint8_t  rawLen = 0;
    char     lastSentence[kMaxSentence + 1] = {};
    uint32_t sentenceSeq = 0;

    // Route the current term to the active parser or find a new active parser.
    // Returns true when a valid sentence was just committed.
    bool dispatchTerm();
};

}  // namespace triton
