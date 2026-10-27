/*
  PlainCanGuidance - reading guidance from a CAN bus without an ISOBUS stack
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

// Include after <ACAN.h> (Teensy 3.x) or <ACAN_T4.h> (Teensy 4): this uses
// their ACANPrimaryFilter, kData, kExtended and CANMessage. Firmware only --
// the native tests never include it.

namespace triton
{

// One copy of the plain-CAN setup four main.cpp files wrote out
// (NeptuneGPS_Triton#177): 250 kbit/s, acceptance filters for the guidance
// messages CanFrameGuidanceChannel decodes, and the bit timing on the debug
// port. Receive only in effect: no address claim, nothing transmitted.
//
//   ACAN on a Teensy 3.2:  PlainCanBegin<ACANSettings>(ACAN::can0, ...)
//   ACAN_T4 on a Teensy 4: PlainCanBegin<ACAN_T4_Settings>(ACAN_T4::can1, ...)
//
// The filters: ploegbesturing v1.30/v1.31's four (StarFire position and
// speed, JD and Trimble/CNH XTE) plus the three standard NMEA2000 PGNs
// (129025 position, 129026 COG/SOG, 129283 XTE) from any source address, so
// a receiver that only broadcasts those is read too. Every matching frame
// goes to `handler`, which hands it to CanFrameGuidanceChannel; the loop
// calls can.dispatchReceivedMessage() to run it.
template <typename Settings, typename Can>
uint32_t PlainCanBegin(Can& can, Stream* serialDebug, void (*handler)(const CANMessage&)) {
    Settings settings(250UL * 1000UL);  // CAN bit rate 250 kbit/s

    const ACANPrimaryFilter primaryFilters[]{
        ACANPrimaryFilter(kData, kExtended, 0x00FFFFFF, 0x00FEF31C, handler), // Position
        ACANPrimaryFilter(kData, kExtended, 0x00FFFFFF, 0x00FEE81C, handler), // Direction, speed
        ACANPrimaryFilter(kData, kExtended, 0x0CFFFF2A, handler),             // JD XTE
        ACANPrimaryFilter(kData, kExtended, 0x1CEBACAA, handler),             // Trimble CNH XTE
        ACANPrimaryFilter(kData, kExtended, 0x01FFFF00, 0x01F80100, handler), // 129025 position, rapid update
        ACANPrimaryFilter(kData, kExtended, 0x01FFFF00, 0x01F80200, handler), // 129026 COG/SOG, rapid update
        ACANPrimaryFilter(kData, kExtended, 0x01FFFF00, 0x01F90300, handler)  // 129283 cross track error
    };

    const uint32_t errorCode = can.begin(settings, primaryFilters, 7);

    if (errorCode == 0) {
        serialDebug->print("Bit Rate prescaler: ");
        serialDebug->println(settings.mBitRatePrescaler);
        serialDebug->print("Propagation Segment: ");
        serialDebug->println(settings.mPropagationSegment);
        serialDebug->print("Phase segment 1: ");
        serialDebug->println(settings.mPhaseSegment1);
        serialDebug->print("Phase segment 2: ");
        serialDebug->println(settings.mPhaseSegment2);
        serialDebug->print("RJW:");
        serialDebug->println(settings.mRJW);
        serialDebug->print("Triple Sampling: ");
        serialDebug->println(settings.mTripleSampling ? "yes" : "no");
        serialDebug->print("Actual bit rate: ");
        serialDebug->print(settings.actualBitRate());
        serialDebug->println(" bit/s");
        serialDebug->print("Exact bit rate ? ");
        serialDebug->println(settings.exactBitRate() ? "yes" : "no");
        serialDebug->print("Sample point: ");
        serialDebug->print(settings.samplePointFromBitStart());
        serialDebug->println("%");
    }
    else {
        serialDebug->print("CAN error: 0x");
        serialDebug->println(errorCode, HEX);
    }
    return errorCode;
}

}  // namespace triton
