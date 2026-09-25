/*
  test_CanErrorMonitor - native tests for the FlexCAN error-state monitor
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
#include <AUnit.h>
#include "isobus/CanErrorMonitor.hpp"

using namespace aunit;
using namespace triton;

// The register reads themselves are Teensy-only (IsobusGuidanceChannel); these
// pin the decode and the episode bookkeeping over raw ECR/ESR1 values
// (NeptuneGPS_Triton#149).

test(CanErrorMonitor, registers_decodeCountersAndFaultState) {
    // ECR: TX error counter in bits 7:0, RX in bits 15:8.
    assertEqual((int)CanErrorMonitor::TxErrors(0x00008040UL), 0x40);
    assertEqual((int)CanErrorMonitor::RxErrors(0x00008040UL), 0x80);
    // ESR1[FLTCONF], bits 5:4.
    assertTrue(CanErrorMonitor::Fault(0x00) == CanFaultState::ErrorActive);
    assertTrue(CanErrorMonitor::Fault(0x10) == CanFaultState::ErrorPassive);
    assertTrue(CanErrorMonitor::Fault(0x20) == CanFaultState::BusOff);
    assertTrue(CanErrorMonitor::Fault(0x30) == CanFaultState::BusOff);
}

// FlexCAN_T4's own decoder reports this exact state as "Bus off" -- it tests
// (ESR1 & 0x30) == 0x1, which can never hold. Other ESR1 bits are set here
// too (SYNCH, IDLE), as they would be on a live bus.
test(CanErrorMonitor, errorPassiveIsNotMistakenForBusOff) {
    assertTrue(CanErrorMonitor::Fault(0x40090UL) == CanFaultState::ErrorPassive);
}

// A session-11-shaped episode: passive, then bus-off, recovered 3.2 s later.
test(CanErrorMonitor, episode_countsEntriesAndLongestTimeAwayFromActive) {
    CanErrorMonitor m;
    m.Sample(0x0000, 0x00, 0);       // error-active
    m.Sample(0x0090, 0x10, 100);     // error-passive, TX errors 144
    m.Sample(0x00FF, 0x20, 150);     // bus-off
    m.Sample(0x0000, 0x00, 3350);    // recovered
    assertEqual(m.GetErrorPassiveEntries(), (uint32_t)1);
    assertEqual(m.GetBusOffEntries(), (uint32_t)1);
    assertEqual(m.GetLastErrorPassiveMs(), (unsigned long)100);
    assertEqual(m.GetLastBusOffMs(), (unsigned long)150);
    assertEqual(m.GetLongestEpisodeMs(), (unsigned long)3250);
    assertTrue(m.GetState() == CanFaultState::ErrorActive);
    assertEqual((int)m.GetTxErrors(), 0);         // live counter is back to 0...
    assertEqual((int)m.GetPeakTxErrors(), 0xFF);  // ...the peak is what survives
    assertEqual(m.GetSamples(), (uint32_t)4);
}

// A state held across many samples is one entry, not one per sample.
test(CanErrorMonitor, steadyStates_countOnce) {
    CanErrorMonitor m;
    for (int t = 0; t < 10; t++) m.Sample(0x0000, 0x00, t * 10UL);
    assertEqual(m.GetErrorPassiveEntries(), (uint32_t)0);
    assertEqual(m.GetBusOffEntries(), (uint32_t)0);
    assertEqual(m.GetLongestEpisodeMs(), (unsigned long)0);
    for (int t = 0; t < 5; t++) m.Sample(0x0080, 0x10, 200 + t * 10UL);
    assertEqual(m.GetErrorPassiveEntries(), (uint32_t)1);
    assertTrue(m.GetState() == CanFaultState::ErrorPassive);
}

// The longest episode is kept, not the latest.
test(CanErrorMonitor, longestEpisode_isKeptAcrossShorterOnes) {
    CanErrorMonitor m;
    m.Sample(0x80, 0x10, 1000);  m.Sample(0x00, 0x00, 1500);   // 500 ms
    m.Sample(0x80, 0x10, 2000);  m.Sample(0x00, 0x00, 2100);   // 100 ms
    assertEqual(m.GetLongestEpisodeMs(), (unsigned long)500);
    assertEqual(m.GetErrorPassiveEntries(), (uint32_t)2);
}

test(CanErrorMonitor, reset_clearsEverything) {
    CanErrorMonitor m;
    m.Sample(0x00FF, 0x20, 100);
    m.Reset();
    assertTrue(m.GetState() == CanFaultState::ErrorActive);
    assertEqual((int)m.GetPeakTxErrors(), 0);
    assertEqual(m.GetBusOffEntries(), (uint32_t)0);
    assertEqual(m.GetSamples(), (uint32_t)0);
}
