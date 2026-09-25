/*
  CanErrorMonitor - watches a FlexCAN controller's error state from its raw
  registers, so a CAN fault episode shows in the debug dump
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

#include <stdint.h>

namespace triton
{

// Why this exists (NeptuneGPS_Triton#149): in session 11 the board stopped
// transmitting and receiving on CAN for 3.19 s while loop() kept running, and
// the VT dropped us for it. Nothing in the firmware read the CAN controller's
// error state, so no dump could say whether the controller went error-passive
// or bus-off. This watches it.
//
// Pure logic over two raw register values, so it is testable off-target; the
// Teensy-only register read lives in IsobusGuidanceChannel::Update().
enum class CanFaultState : uint8_t {
    ErrorActive  = 0,
    ErrorPassive = 1,
    BusOff       = 2,
};

class CanErrorMonitor {
public:
    // Register layout, per the i.MX RT1060 reference manual: ECR[TXERRCNT] is
    // bits 7:0 and ECR[RXERRCNT] bits 15:8; ESR1[FLTCONF] is bits 5:4 --
    // 00 error active, 01 error passive, 1x bus off.
    //
    // Deliberately not FlexCAN_T4's own error() decoder: it tests
    // (ESR1 & 0x30) == 0x1, which can never be true, so it reports every
    // error-passive controller as "Bus off".
    static inline uint8_t TxErrors(uint32_t ecr) { return static_cast<uint8_t>(ecr & 0xFF); }
    static inline uint8_t RxErrors(uint32_t ecr) { return static_cast<uint8_t>((ecr >> 8) & 0xFF); }
    static inline CanFaultState Fault(uint32_t esr1) {
        const uint32_t fltconf = (esr1 >> 4) & 0x3;
        if (fltconf == 0) return CanFaultState::ErrorActive;
        if (fltconf == 1) return CanFaultState::ErrorPassive;
        return CanFaultState::BusOff;
    }

    // One reading of ECR and ESR1, taken at nowMs.
    void Sample(uint32_t ecr, uint32_t esr1, unsigned long nowMs) {
        txErrors = TxErrors(ecr);
        rxErrors = RxErrors(ecr);
        if (txErrors > peakTxErrors) peakTxErrors = txErrors;
        if (rxErrors > peakRxErrors) peakRxErrors = rxErrors;

        const CanFaultState now = Fault(esr1);
        if (now != state) {
            if (now == CanFaultState::ErrorPassive) { errorPassiveEntries++; lastErrorPassiveMs = nowMs; }
            if (now == CanFaultState::BusOff)       { busOffEntries++;       lastBusOffMs       = nowMs; }
            // An episode is the time spent away from error-active, however it
            // moved between passive and bus-off in the middle.
            if (state == CanFaultState::ErrorActive) episodeStartMs = nowMs;
            if (now == CanFaultState::ErrorActive) {
                const unsigned long length = nowMs - episodeStartMs;
                if (length > longestEpisodeMs) longestEpisodeMs = length;
            }
            state = now;
        }
        samples++;
    }

    void Reset() { *this = CanErrorMonitor(); }

    CanFaultState GetState() const            { return state; }
    uint8_t       GetTxErrors() const         { return txErrors; }
    uint8_t       GetRxErrors() const         { return rxErrors; }
    uint8_t       GetPeakTxErrors() const     { return peakTxErrors; }
    uint8_t       GetPeakRxErrors() const     { return peakRxErrors; }
    uint32_t      GetErrorPassiveEntries() const { return errorPassiveEntries; }
    uint32_t      GetBusOffEntries() const    { return busOffEntries; }
    unsigned long GetLastErrorPassiveMs() const { return lastErrorPassiveMs; }
    unsigned long GetLastBusOffMs() const     { return lastBusOffMs; }
    unsigned long GetLongestEpisodeMs() const { return longestEpisodeMs; }
    uint32_t      GetSamples() const          { return samples; }

    static const char* StateName(CanFaultState s) {
        switch (s) {
            case CanFaultState::ErrorActive:  return "error-active";
            case CanFaultState::ErrorPassive: return "error-PASSIVE";
            case CanFaultState::BusOff:       return "BUS-OFF";
        }
        return "?";
    }

private:
    CanFaultState state = CanFaultState::ErrorActive;
    uint8_t  txErrors = 0, rxErrors = 0;
    uint8_t  peakTxErrors = 0, peakRxErrors = 0;
    uint32_t errorPassiveEntries = 0, busOffEntries = 0;
    unsigned long lastErrorPassiveMs = 0, lastBusOffMs = 0;
    unsigned long episodeStartMs = 0, longestEpisodeMs = 0;
    uint32_t samples = 0;
};

}  // namespace triton
