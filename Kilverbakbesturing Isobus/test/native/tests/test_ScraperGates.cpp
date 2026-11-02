/*
  test_ScraperGates - Tests for when the kilverbak may level and which source
  it levels on (NeptuneGPS_Triton#290): the hold reasons in their order, and
  the UART winning while its GGA is fresh.
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
#include "ScraperGates.hpp"

using namespace aunit;
using namespace triton;

// A source with a fresh RTK fix stamped at `ms`.
static void rtkFix(GuidanceSource& g, unsigned long ms, float altitudeM = 10.0f) {
    millisValue(ms);
    g.SetPosition(52.0f, 5.0f);
    g.SetAltitude(altitudeM);
    g.SetQuality(4);
    g.SetSpeedKnots(10.0f);
}

// ---------------------------------------------------------------------------
// ScraperHoldReason
// ---------------------------------------------------------------------------

// At boot nothing was ever received: no fix, even within the first 2 s (a
// timestamp of 0 is "never", not "at 0 ms").
test(ScraperGates, boot_nothingReceived_isNoFix) {
    GuidanceSource g;
    assertEqual((int)ScraperHoldReason(g, true, 500), (int)ScraperHold::NoFix);
}

test(ScraperGates, freshRtkFix_levels) {
    GuidanceSource g;
    rtkFix(g, 1000);
    assertEqual((int)ScraperHoldReason(g, true, 1000), (int)ScraperHold::None);
    assertEqual((int)ScraperHoldReason(g, true, 3000), (int)ScraperHold::None);   // 2000 ms: still fresh
    assertEqual((int)ScraperHoldReason(g, true, 3001), (int)ScraperHold::NoFix);
}

// The order: a missing fix before a missing RTK before the speed.
test(ScraperGates, reasons_inTheirOrder) {
    GuidanceSource g;
    rtkFix(g, 1000);
    g.SetQuality(5);
    g.SetSpeedKnots(0.1f);
    assertEqual((int)ScraperHoldReason(g, true, 1000), (int)ScraperHold::NoRtk);
    g.SetQuality(4);
    assertEqual((int)ScraperHoldReason(g, true, 1000), (int)ScraperHold::Speed);
    assertEqual((int)ScraperHoldReason(g, false, 1000), (int)ScraperHold::None);
}

// An RTK quality that went quiet counts as none, even while positions keep
// coming (the plough's #261 rule).
test(ScraperGates, staleQuality_isNoRtk) {
    GuidanceSource g;
    rtkFix(g, 1000);
    millisValue(4000);
    g.SetPosition(52.0f, 5.0f);
    g.SetAltitude(10.0f);
    g.SetSpeedKnots(10.0f);
    assertEqual((int)ScraperHoldReason(g, true, 4000), (int)ScraperHold::NoRtk);
}

// ---------------------------------------------------------------------------
// SelectScraperGuidance
// ---------------------------------------------------------------------------

// The UART wins while its GGA is fresh (owner, #290).
test(ScraperGates, select_freshUart_wins) {
    GuidanceSource out, uart, can;
    rtkFix(can, 1000, 7.0f);
    rtkFix(uart, 1000, 5.0f);
    assertTrue(SelectScraperGuidance(out, uart, can, 1000));
    assertNear(out.GetAltitude(), 5.0f, 0.001f);
    assertEqual(out.GetGgaTimestamp(), (unsigned long)1000);
}

// A quiet UART hands over to the CAN bus; a UART that never sent anything
// does at once.
test(ScraperGates, select_quietUart_handsOverToCan) {
    GuidanceSource out, uart, can;
    rtkFix(uart, 1000, 5.0f);
    rtkFix(can, 3500, 7.0f);
    assertFalse(SelectScraperGuidance(out, uart, can, 3500));
    assertNear(out.GetAltitude(), 7.0f, 0.001f);

    GuidanceSource neverUart;
    assertFalse(SelectScraperGuidance(out, neverUart, can, 3500));
    assertNear(out.GetAltitude(), 7.0f, 0.001f);
}

// The RTK quality setting lives on `out` (the calibration sets it there) and
// survives the copy.
test(ScraperGates, select_keepsTheRtkQualitySetting) {
    GuidanceSource out, uart, can;
    out.SetRtkQuality(2);
    rtkFix(uart, 1000);
    SelectScraperGuidance(out, uart, can, 1000);
    assertEqual((int)out.GetRtkQuality(), 2);
}
