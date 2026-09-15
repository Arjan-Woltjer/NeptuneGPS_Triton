/*
  test_ImplementScraper - Tests for ImplementScraper: the two-reference-point
  leveling calculation (calculateDistances(), incl. its divide-by-zero guard),
  Update()'s offset/setpoint pipeline, and Adjust()'s NOSENS blind on/off
  decision.
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
#include "ImplementScraper.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// ImplementScraper's constructor reads calibration data from EEPROM (via
// readCalibrationData()/readOffset()), and most of the logic under test here
// is private state with no reset hook -- so every test constructs its own
// local ImplementScraper after resetAll() re-erases the fake EEPROM,
// guaranteeing the same "no calibration data found" defaults every time:
// positionCalibrationData={200,450,700}, positionCalibrationPoints={0,10,20},
// kp=100, manPwm=autoPwm=254 (PWM_MAN/PWM_AUTO undefined), error=2,
// maxCorrection=5, slope=0, offset=0, Reference A/B both (0,0,0) -- an erased
// EEPROM read (see ImplementScraper.cpp's constructor).
// ---------------------------------------------------------------------------
static GuidanceSource mockGuidance;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    // Fresh source (no fix ever, position 0/0, altitude 0), then a speed
    // above MinSpeed(); its VTG stamp lands at millis()=0, matching the stub
    // this suite was written against.
    mockGuidance = GuidanceSource();
    mockGuidance.SetSpeedKnots(10.0f);
}

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

test(ImplementScraper, construct_defaultsMatchErasedEeprom) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    assertEqual(impl.GetOffset(), (short int)0);
    assertEqual(impl.GetSlope(), (short int)0);
    assertEqual(impl.GetError(), (byte)2);
    assertEqual(impl.GetMaxCorrection(), 5);
}

// ---------------------------------------------------------------------------
// calculateDistances() (private, exercised through Update()) -- Reference A
// and B both default to (0,0,0), so the vector AB has zero length. The
// legacy source divided by that unconditionally: 0/0 is NaN, and narrowing a
// NaN float to heightRef (an integer member) is undefined behaviour. This
// asserts the fallback ("on the line, no slope offset") instead.
// ---------------------------------------------------------------------------

test(ImplementScraper, calculateDistances_degenerateReferencePoints_fallsBackToZero) {
    resetAll();
    ImplementScraper impl(&mockGuidance);

    millisValue(1);
    mockGuidance.SetPosition(52.0f, 5.0f);   // stamps lastGgaFix=1 > impl's internal lastGgaFix(0) -> triggers recompute
    mockGuidance.SetAltitude(10.0f);         // 1000 cm

    impl.Update(0, 0);

    assertEqual(impl.GetDistance(), 0);
    assertEqual(impl.GetXTE(), 0);
    assertEqual(impl.GetRefHeight(), (short int)0);
}

// ---------------------------------------------------------------------------
// calculateDistances() with real, distinct reference points -- exercises the
// shared DistanceBetween() (GuidanceGeometry.hpp) end to end. Reference A at
// the origin, Reference B 100m due north (roughly -- at these latitudes
// 111320m per degree of latitude is the approximation both the real library
// and this stub use), both at 1000cm altitude with slope=0, so heightRef
// should track heightRa exactly regardless of position along AB.
// ---------------------------------------------------------------------------

test(ImplementScraper, calculateDistances_realReferencePoints_computesRefHeight) {
    resetAll();
    ImplementScraper impl(&mockGuidance);

    // Reference A at (52.0, 5.0), altitude 1000cm.
    mockGuidance.SetPosition(52.0f, 5.0f);
    mockGuidance.SetAltitude(10.0f);
    impl.SetRefA();

    // Reference B ~100m north of A (100m / 111320 m-per-degree).
    mockGuidance.SetPosition(52.0f + (100.0f / 111320.0f), 5.0f);
    mockGuidance.SetAltitude(12.0f);
    impl.SetRefB();

    // Current position: back at Reference A's location, stamped at 1 so
    // Update() sees a fix newer than its internal lastGgaFix(0).
    millisValue(1);
    mockGuidance.SetPosition(52.0f, 5.0f);
    mockGuidance.SetAltitude(10.0f);

    impl.Update(0, 0);

    // slope defaults to 0 -> heightRef = heightRa + (dAb*0)/100 = heightRa,
    // regardless of dAb/xAb -- the reference plane is flat until slope is set.
    assertEqual(impl.GetRefHeight(), (short int)1000);
}

// ---------------------------------------------------------------------------
// Update()'s setSetpoint() (NOSENS branch): setpoint = -(height - heightRef
// + offset). Reference A/B left at their default (0,0,0) -- heightRef stays
// 0 (the degenerate-reference fallback above) -- so this isolates the
// height/offset arithmetic itself.
// ---------------------------------------------------------------------------

test(ImplementScraper, update_setpointTracksHeightDelta) {
    resetAll();
    ImplementScraper impl(&mockGuidance);

    mockGuidance.SetAltitude(2.5f);   // height=250, heightRef=0, offset=0 -> setpoint = -(250-0+0) = -250
    millisValue(1);
    mockGuidance.NoteGgaFixReceived();   // lastGgaFix=1 -> triggers recompute

    impl.Update(0, 0);
    assertEqual(impl.GetHeight(), (short int)250);

    // Auto mode's inputtime = millis() - guidance->GetGgaTimestamp() -- advance
    // millis() past the fix age first so this doesn't wrap negative (this
    // module's own Update() always runs after a real GGA fix has aged, so
    // millis() > fix age holds in practice; only an artifact of testing
    // Adjust() immediately after seeding ggaFixAge above).
    millisValue(100);

    // setpoint isn't exposed via a getter directly, but Adjust() reads it --
    // a negative setpoint should drive "wide" (raise) via OUTPUT_WIDE_5.
    impl.Adjust(0, 0);
    assertMore(analogWriteValue(OUTPUT_WIDE_5), 0);
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 0);
}

// ---------------------------------------------------------------------------
// Adjust() -- NOSENS branch, manual mode (mode != 0): setpoint = direction
// directly, and inputtime(0) < settime(1) always holds in this branch, so
// the sign of direction alone selects Left/Right/Stop.
// ---------------------------------------------------------------------------

test(ImplementScraper, adjust_manualMode_positiveDirection_narrows) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    impl.Adjust(2, 5);
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 255);
    assertEqual(analogWriteValue(OUTPUT_WIDE_5), 0);
}

test(ImplementScraper, adjust_manualMode_negativeDirection_widens) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    impl.Adjust(2, -5);
    assertEqual(analogWriteValue(OUTPUT_WIDE_5), 255);
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 0);
}

test(ImplementScraper, adjust_manualMode_zeroDirection_stops) {
    resetAll();
    ImplementScraper impl(&mockGuidance);
    impl.Adjust(2, 5);
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 255);

    impl.Adjust(2, 0);
    assertEqual(analogWriteValue(OUTPUT_NARROW_5), 0);
    assertEqual(analogWriteValue(OUTPUT_WIDE_5), 0);
}
