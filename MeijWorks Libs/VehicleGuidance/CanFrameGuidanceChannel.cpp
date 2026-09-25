/*
  CanFrameGuidanceChannel - guidance data from raw CAN frames on a directly
  attached bus (no ISOBUS stack, no address claim)
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
#include "CanFrameGuidanceChannel.hpp"

#include "GuidanceCommit.hpp"
#include "IsobusPgnDecode.hpp"

namespace triton
{

bool CanFrameGuidanceChannel::Update(uint32_t id, const uint8_t* data, uint8_t length) {
    return Decode(id, data, length, guidance);
}

bool CanFrameGuidanceChannel::Decode(uint32_t id, const uint8_t* data, uint8_t length, GuidanceSource* guidance) {
    if (data == nullptr || guidance == nullptr) return false;

    const uint32_t pgn = PgnFromId(id);
    const uint8_t  sa  = SourceAddressFromId(id);

    switch (pgn) {
        case kPgnPositionNmea2000:     return GApply(DecodePositionNmea2000(data, length), guidance);
        case kPgnSpeedNmea2000:        return GApply(DecodeSpeedNmea2000(data, length), guidance);
        case kPgnXteNmea2000:          return GApply(DecodeXteNmea2000(data, length), guidance);
        case kPgnPositionLegacy:       return GApply(DecodeLegacyPosition(data, length), guidance);
        case kPgnSpeedLegacy:          return GApply(DecodeLegacySpeed(data, length), guidance);
        case kPgnXteJohnDeereLegacy:   return GApply(DecodeLegacyXteJohnDeere(sa, data, length), guidance);
        case kPgnXteTrimbleLegacy:     return GApply(DecodeLegacyXteTrimble(sa, data, length), guidance);
        default:                       return false;
    }
}

}  // namespace triton
