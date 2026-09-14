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

#include "IsobusPgnDecode.hpp"

namespace triton
{

namespace {

// The same commit rules IsobusGuidanceChannel's On*() callbacks apply,
// without that class's per-PGN diagnostic counters.
bool GApply(const PositionResult& r, GuidanceSource* g) {
    if (!r.fixPresent) return false;
    // The fix-age timer drives the plough's HOLD interlock and must keep its
    // meaning even when the coordinates fail the plausibility check.
    g->NoteGgaFixReceived();
    if (r.hasCoordinates) g->SetPosition(r.latitude, r.longitude);
    return true;
}

bool GApply(const SpeedResult& r, GuidanceSource* g) {
    if (r.valid)       g->SetSpeedKnots(r.speedKnots);
    if (r.hasCourse)   g->SetCourseDeg(r.courseDeg);
    if (r.hasAltitude) g->SetAltitude(r.altitudeMeters);
    return r.valid || r.hasCourse || r.hasAltitude;
}

bool GApply(const XteResult& r, GuidanceSource* g) {
    if (!r.valid) return false;
    if (r.hasQuality) g->SetXte(r.xteHundredthsMeter, r.quality);
    else              g->SetXte(r.xteHundredthsMeter);
    return true;
}

}  // namespace

bool CanFrameGuidanceChannel::Update(uint32_t id, const uint8_t* data, uint8_t length) {
    if (data == nullptr) return false;

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
