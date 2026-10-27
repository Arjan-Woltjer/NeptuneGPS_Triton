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
#include "GuidanceCommit.hpp"
#include "IsobusPgnDecode.hpp"

namespace triton
{

bool CanSerialParser::claimsSentenceType(const char* header) {
    fieldsValid = false;

    // Standard NMEA2000, matched on the PGN inside the identifier so any
    // source address qualifies, as on the CAN paths. Only the single-frame
    // PGNs: 129029 GNSS Position Data (1DF805xx, VehicleGps' CAN_POS_TERM3)
    // is a fast-packet message spread over seven frames, and one bridge line
    // holds only its first fragment, so it is left unclaimed rather than
    // mis-decoded; the same receivers broadcast 129025 for position.
    //
    // Not under GUIDANCE_LEAN (the Arduino Micro build): the decoders behind
    // it are 2 KB the Micro's 28 KB of flash cannot spare, and the JD and
    // CNH CAN-to-serial converters the Micro boards use forward only the
    // legacy identifiers below.
#ifndef GUIDANCE_LEAN
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
#endif

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

    newXte = 0;
    newQuality = 0;
    return true;
}

void CanSerialParser::parseTerm(byte termNumber, const char* term) {
    if (termNumber != 1) return;

    if (type == CAN_XTE) {
        // John Deere XTE: its own decode, see the header.
        if (!termIsHex(term, 10)) return;
        fieldsValid = true;
        const unsigned long val = ((unsigned long)hexToInt(term[8]) << 12) | ((unsigned long)hexToInt(term[9]) << 8)
                                | ((unsigned long)hexToInt(term[6]) << 4) | hexToInt(term[7]);
        newXte = int(val - 32000) >> 1;
        newQuality = (term[2] == '1') ? 4 : 0;
        return;
    }

    // Every other frame: sixteen hex digits back into the eight payload
    // bytes; the layout is the decoder's business, not this parser's.
    if (type == NONE || !termIsHex(term, 16)) return;
    for (byte i = 0; i < 8; i++)
        frameData[i] = uint8_t((hexToInt(term[2 * i]) << 4) | hexToInt(term[2 * i + 1]));
    fieldsValid = true;
}

void CanSerialParser::commitTo(GuidanceSource* state) {
    if (!fieldsValid) return;   // short or non-hex payload: nothing to commit

    switch (type) {
        case NMEA2000:
#ifndef GUIDANCE_LEAN
            CanFrameGuidanceChannel::Decode(frameId, frameData, sizeof(frameData), state);
#endif
            break;
        case CAN_POS:
            GApply(DecodeLegacyPosition(frameData, sizeof(frameData)), state);
            break;
        case CAN_SPD:
            GApply(DecodeLegacySpeed(frameData, sizeof(frameData)), state);
            break;
        case CAN_XTE:
            state->SetXte(newXte);
            state->SetQuality(newQuality);
            break;
        case CAN_XTE2:
            // Only the transfer's packet 2 with marker 7 carries the XTE; the
            // decoder ignores the others. Its quality is still the fixed 4
            // from the reverse-engineered layout: where the real quality
            // flag sits needs a capture of this transfer.
            GApply(DecodeLegacyXteTrimble(kSourceAddressTrimble, frameData, sizeof(frameData)), state);
            break;
        case NONE: break;
    }
}

}  // namespace triton
