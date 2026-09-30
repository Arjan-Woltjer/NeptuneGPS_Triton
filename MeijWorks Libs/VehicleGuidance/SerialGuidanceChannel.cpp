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

#include <string.h>

namespace triton
{

SerialGuidanceChannel::SerialGuidanceChannel(Stream* serialDebug, HardwareSerial* serialGps, GuidanceSource* guidance)
    : serialDebug(serialDebug), serialGps(serialGps), guidance(guidance) {
#ifdef DEBUG
    serialDebug->println("-------------------------------");
    serialDebug->println("Initialising GPS");
    serialDebug->println("-------------------------------");
    rawEcho = true;
#endif

    parsers[0] = &nmeaParser;
    parsers[1] = &trimbleParser;
    parsers[2] = &canSerialParser;
    activeParse = nullptr;
    // term/termNumber/termOffset/parity/checksum/sum/isChecksumTerm and the
    // sentence-tap buffers carry default member initializers in the header
    // instead of being assigned here.
}

bool SerialGuidanceChannel::Update() {
    // Unsigned deliberately. Dispatching on a plain char made `case 191` (the
    // Trimble packet-start byte) unreachable wherever char is signed, so the
    // parser behaved differently on the host than on the target -- and host
    // tests could never have exercised the Trimble path at all.
    uint8_t c;
    bool validSentence = false;

    while (serialGps->available()) {
        c = uint8_t(serialGps->read());
        if (rawEcho && serialDebug) serialDebug->write(c);

        // Sentence tap, independent of the parser below.
        if (c == '$' || c == '@' || c == 191) {
            rawLen = 0;
        }
        if (c == '\n' || c == '\r') {
            if (rawLen > 0) {
                memcpy(lastSentence, rawSentence, rawLen);
                lastSentence[rawLen] = '\0';
                sentenceSeq++;
                rawLen = 0;
                if (sentenceTap) sentenceTap(sentenceTapContext, lastSentence);
            }
        } else if (c >= 32 && c < 127 && rawLen < kMaxSentence) {
            rawSentence[rawLen++] = (char)c;
        }

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
                sum += byte(c);
                break;

            // A space is an ordinary character inside an NMEA sentence: it
            // counts toward the '*XX' checksum and belongs in the term. The
            // ATGM336H's "$GPTXT,01,01,01,ANTENNA OPEN" is the first sentence
            // handled here that contains one, and bitbucketing it broke that
            // sentence twice over -- the space never reached parity, so the
            // real checksum never matched and the sentence was dropped whole
            // before commitTo(), and it never reached term[], so the text
            // arrived as "ANTENNAOPEN". NeptuneGPS_Triton#61 case 7: the pump
            // kept running through a real antenna pull because of this.
            //
            // Trimble frames deliberately keep the old behaviour. TrimbleParser
            // uses this same parity as its own checksum (useParityAsChecksum()),
            // and that decode is already proven on the rig, so widening the
            // change to it would need its own hardware proof.
            case ' ':
                if (activeParse && !activeParse->useParityAsChecksum()) {
                    if (termOffset < sizeof(term) - 1)
                        term[termOffset++] = c;
                    if (!isChecksumTerm) parity ^= c;
                }
                sum += byte(c);
                break;

            // Term separators -- commit the current term to the active parser
            case ',':
                // Comma is a term separator and part of the parity calculation, and part of the checksum sum (fall through)
                parity ^= c;
                // fall through
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

            // Trimble packet end marker: byte 3 preceded by byte 16.
            //
            // termOffset is reset to 0 by five other cases above, so without
            // the length guard a single 0x03 arriving after any delimiter
            // read term[-1] -- and if the bytes there happened to satisfy the
            // checksum arithmetic, term[-4] = '\0' wrote outside the buffer.
            // Four bytes are the minimum this block indexes.
            case 3:
                if (termOffset >= 4 && term[termOffset - 1] == 16 && !isChecksumTerm) {
                    sum -= byte(term[termOffset - 1]);
                    sum -= byte(term[termOffset - 2]);
                    sum -= byte(term[termOffset - 3]);
                    // Checksum verification of the Trimble outer packet
                    if (sum - byte(term[termOffset - 2])
                           - (256 * byte(term[termOffset - 3])) == 0) {
                        // Trim the 4 trailing Trimble framing bytes, dispatch the data term
                        term[termOffset - 4] = '\0';
                        // dispatchTerm() doesn't commit: isChecksumTerm is
                        // never set for the Trimble outer packet.
                        dispatchTerm();
                        // Commit here, and only here, for a parser that relies
                        // on the outer frame instead of an NMEA checksum. This
                        // branch is the sole path that commits without a
                        // verified '*XX' term, and it is only reached once the
                        // frame's own checksum has just passed -- the guarantee
                        // VehicleGps tracked as trimbleFrameVerified.
                        if (activeParse && activeParse->useParityAsChecksum()) {
                            activeParse->commitTo(guidance);
                            validSentence = true;
                        }
                    }
                    termNumber++; termOffset = 0;
                    break;
                }
                // Not a valid Trimble end -- fall through and treat as ordinary character
                // fall through

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

void SerialGuidanceChannel::ApplyBaudrate(long baud) {
    if (serialGps == nullptr || baud <= 0) return;
#if defined(ESP32)
    // Keeps the pins main.cpp assigned; end()/begin() would need them again.
    serialGps->updateBaudRate((unsigned long)baud);
#else
    serialGps->end();
    serialGps->begin((unsigned long)baud);
#endif
    // A partial sentence read at the old rate is garbage; start clean.
    termNumber  = 0;
    termOffset  = 0;
    rawLen      = 0;
    activeParse = nullptr;
    isChecksumTerm = false;
}

// Route the current null-terminated term to the active parser.
// Returns true when a valid sentence was just committed to state.
bool SerialGuidanceChannel::dispatchTerm() {
    if (isChecksumTerm) {
        if (!activeParse) {
            return false;
        }

        if ((GpsParser::hexToInt(term[0]) << 4) + GpsParser::hexToInt(term[1]) == parity) {
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
