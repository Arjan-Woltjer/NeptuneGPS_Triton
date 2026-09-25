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
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "GuidanceSource.hpp"

namespace triton
{

// The successor to VehicleGps::Update(id, data, len): a project that reads
// its CAN controller itself (ACAN_T4 on the Teensy plough, planter and
// scraper) hands every received extended frame to Update(), and the
// guidance messages among them are committed to a GuidanceSource. The same
// IsobusPgnDecode functions the ISOBUS build's PGN callbacks use do the
// byte math, and the same GuidanceCommit rules decide what is committed, so
// both paths agree on every layout, sentinel, plausibility check and commit:
// the standard NMEA2000 position/speed/XTE PGNs and the legacy JD/Trimble/CNH
// proprietary ones.
//
// Compared with VehicleGps this path is stricter in two places, both
// deliberate: the John Deere XTE on PGN 0xFFFF must come from source
// address 0x2A and carry the 0x77 message selector (NeptuneGPS_Triton#20,
// #30), and a speed, course or altitude field reading its 0xFFFF
// "not available" sentinel is skipped instead of committed as a number.
class CanFrameGuidanceChannel {
public:
    explicit CanFrameGuidanceChannel(GuidanceSource* guidance) : guidance(guidance) {}

    // One received 29-bit frame. Returns true when a guidance value was
    // committed to the source; frames on other PGNs return false and are
    // otherwise ignored, so the caller's acceptance filter can be as wide
    // as it likes.
    bool Update(uint32_t id, const uint8_t* data, uint8_t length);

    // The same decode and commit rules as Update(), for a caller that holds
    // a frame but no channel: CanSerialParser hands the NMEA2000 lines a
    // CAN-to-serial bridge forwards through here, so a bridged bus and a
    // directly read bus commit identical values to the source.
    static bool Decode(uint32_t id, const uint8_t* data, uint8_t length, GuidanceSource* guidance);

    // SAE J1939 / ISO 11783 identifier fields. PGN keeps the data page bit
    // and, for PDU2 (PF >= 240), the PS byte; for PDU1 the PS byte is a
    // destination address and is not part of the PGN.
    static inline uint32_t PgnFromId(uint32_t id) {
        const uint32_t dataPage = (id >> 24) & 0x01;
        const uint32_t pf       = (id >> 16) & 0xFF;
        const uint32_t ps       = (id >> 8) & 0xFF;
        return (dataPage << 16) | (pf << 8) | (pf >= 240 ? ps : 0);
    }
    static inline uint8_t SourceAddressFromId(uint32_t id) { return uint8_t(id & 0xFF); }

private:
    GuidanceSource* guidance;
};

}  // namespace triton
