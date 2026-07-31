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
#include "SerialGuidanceChannel.hpp"

namespace triton
{

SerialGuidanceChannel::SerialGuidanceChannel(Stream* serialDebug, HardwareSerial* serialGps, GuidanceSource* guidance)
    : serialDebug(serialDebug), serialGps(serialGps), guidance(guidance) {
#ifdef DEBUG
    serialDebug->println("-------------------------------");
    serialDebug->println("Initialising GPS");
    serialDebug->println("-------------------------------");
#endif

    parsers[0] = &nmeaParser;
    parsers[1] = &trimbleParser;
    parsers[2] = &canSerialParser;
    activeParse = nullptr;

    term[0] = '\0';
    termNumber = 0;
    termOffset = 0;
    parity = 0;
    checksum = 0;
    sum = 0;
    isChecksumTerm = false;
}

bool SerialGuidanceChannel::Update() {
    char c;
    bool validSentence = false;

    while (serialGps->available()) {
        c = serialGps->read();
#ifdef DEBUG
        serialDebug->print(c);
#endif

        switch (c) {
            // Trimble packet start -- reset sum and term state
            case 191:
                termNumber = 0;
                termOffset = 0;
                sum = 0;
                break;

            // $ = NMEA packet start, @ = Trimble packet start -- reset sum and term state
            case '$':
            case '@':
                termNumber = 0;
                termOffset = 0;
                parity = 0;
                sum += byte(c);
                activeParse = nullptr;
                isChecksumTerm = false;
                break;

            // Bitbucket characters that are ignored but still counted in the checksum
            case 20:
            case 0:
            case ' ':
                sum += byte(c);
                break;

            // Term separators -- commit the current term to the active parser
            case ',':
                // Comma is a term separator and part of the parity calculation, and part of the checksum sum (fall through)
                parity ^= c;
            case ':':
            case '*':
            case '\r':
            case '\n':
                sum += byte(c);
                term[termOffset] = '\0';
                if (dispatchTerm()) validSentence = true;
                termNumber++;
                termOffset = 0;
                isChecksumTerm = (c == '*');
                break;

            // Trimble packet end marker
            case 3:
                if (termOffset >= 4 && term[termOffset - 1] == 16 && !isChecksumTerm) {
                    sum -= byte(term[termOffset - 1]);
                    sum -= byte(term[termOffset - 2]);
                    sum -= byte(term[termOffset - 3]);
                    if (sum - byte(term[termOffset - 2])
                           - (256 * byte(term[termOffset - 3])) == 0) {
                        // Trim the 4 trailing Trimble framing bytes, dispatch the data term
                        term[termOffset - 4] = '\0';
                        dispatchTerm();
                        // Commit: TrimbleParser uses parity directly as checksum
                        if (activeParse && activeParse->useParityAsChecksum()) {
                            activeParse->commitTo(guidance);
                            validSentence = true;
                        }
                    }
                    termNumber++; termOffset = 0;
                    break;
                }
                // Not a valid Trimble end -- fall through and treat as ordinary character

            default:
                if (termOffset < sizeof(term) - 1)
                    term[termOffset++] = c;
                if (!isChecksumTerm) parity ^= c;
                sum += byte(c);
                break;
        }
    }
    return validSentence;
}

// Route the current null-terminated term to the active parser.
// Returns true when a valid sentence was just committed to state.
bool SerialGuidanceChannel::dispatchTerm() {
    if (isChecksumTerm) {
        if (!activeParse) return false;
        // TrimbleParser: outer packet already verified, so parity == parity always
        byte computed = activeParse->useParityAsChecksum()
                        ? parity
                        : (GpsParser::hexToInt(term[0]) << 4) + GpsParser::hexToInt(term[1]);
        if (computed == parity) {
            activeParse->commitTo(guidance);
            return true;
        }
        return false;
    }

    if (termNumber == 0) {
        // First term of a new sentence: find which parser claims it
        activeParse = nullptr;
        for (int i = 0; i < 3; i++) {
            if (parsers[i]->claimsSentenceType(term)) {
                activeParse = parsers[i];
                break;
            }
        }
        return false;
    }

    if (activeParse && term[0] != '\0') {
        activeParse->parseTerm(termNumber, term);
    }
    return false;
}

}  // namespace triton
