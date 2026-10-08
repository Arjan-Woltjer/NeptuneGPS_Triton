/*
  GuidanceCommit - the rules for committing decoded guidance messages to a
  GuidanceSource, shared by every CAN-side guidance path
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

#include "GuidanceSource.hpp"
#include "IsobusPgnDecode.hpp"

namespace triton
{

// What a decoded guidance message may write to a GuidanceSource, and when.
// IsobusPgnDecode does the byte math; these decide what reaches the plough.
// Both CAN-side paths commit through here, so the rules exist once:
//
//   - CanFrameGuidanceChannel -- raw frames on a directly attached bus, and
//     through it CanSerialParser's bridged NMEA2000 lines;
//   - IsobusGuidanceChannel   -- the ISOBUS build's per-PGN callbacks, which
//     keep their own counters and diagnostics and hand the commit to these.
//
// Until NeptuneGPS_Triton#98 (phase 3) IsobusGuidanceChannel carried its own
// copy of each rule, and two copies of a safety-path rule can drift apart.
// Each returns true when something was committed.

// A position message counts as a fix whenever the sender says it has one, even
// when its coordinates fail the plausibility check. The fix age drives
// InterfacePlough's HOLD interlock and must keep exactly that meaning; the
// coordinates are diagnostics only, so an implausible one must never cost us a
// fix. SetPosition() stamps the same timestamp, so publishing coordinates
// cannot shorten or extend the fix age either.
inline bool GApply(const PositionResult& r, GuidanceSource* g) {
    if (!r.fixPresent) return false;
    g->NoteGgaFixReceived();
    if (r.hasCoordinates) g->SetPosition(r.latitude, r.longitude);
    return true;
}

// Speed, course and altitude each have their own "not available" sentinel and
// so their own flag: an unavailable speed must not cost us a good course
// (NeptuneGPS_Triton#37).
inline bool GApply(const SpeedResult& r, GuidanceSource* g) {
    if (r.valid)       g->SetSpeedKnots(r.speedKnots);
    if (r.hasCourse)   g->SetCourseDeg(r.courseDeg);
    if (r.hasAltitude) g->SetAltitude(r.altitudeMeters);
    return r.valid || r.hasCourse || r.hasAltitude;
}

// A decoder that knows the fix quality (the John Deere and Trimble legacy
// cross-track messages) commits it together with the value; one that does not
// (NMEA2000 PGN 129283) leaves the stored quality alone rather than writing
// one it never received.
inline bool GApply(const XteResult& r, GuidanceSource* g) {
    if (!r.valid) return false;
    if (r.hasQuality) g->SetXte(r.xteHundredthsMeter, r.quality);
    else              g->SetXte(r.xteHundredthsMeter);
    return true;
}

// The InCommand's lightbar message carries no fix quality either; and it is
// only valid while the display is engaged on a line (see the decoder).
inline bool GApply(const LightbarXteResult& r, GuidanceSource* g) {
    if (!r.valid) return false;
    g->SetXte(r.xteHundredthsMeter);
    return true;
}

}  // namespace triton
