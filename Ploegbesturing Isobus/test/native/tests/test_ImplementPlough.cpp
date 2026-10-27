/*
  test_ImplementPlough - Tests for ImplementPlough: position calibration interpolation,
  Adjust()'s wide/narrow/stop decision and end-shutoff latch, offset EEPROM clamping,
  and setpoint proportional-gain/max-correction clamping.
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <string> uses std::min/max with three arguments.
#include <string>

#include <AUnit.h>
#include "implement/ImplementPlough.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// ImplementPlough's constructor reads calibration data from EEPROM (via
// readCalibrationData()/readOffset()), and most of the logic under test here
// (position, setpoint, offset, the end-shutoff latch) is private state with no
// reset hook -- so, unlike InterfaceSprayer's single shared static instance,
// every test below constructs its own local ImplementPlough after resetAll()
// re-erases the fake EEPROM, guaranteeing the exact same "no calibration data
// found" defaults every time (positionCalibrationData={600,461,308},
// positionCalibrationPoints={34,42,50}, offset=160, shares=4, deadband=2,
// max correction 50 mm per share (20 cm on 4 shares), no bypass -- see
// ImplementPlough.cpp's constructor).
// ---------------------------------------------------------------------------
static GuidanceSource mockGuidance;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    mockGuidance.SetXte(0);  // xte=0, lastXteFix=millis()=0
    digitalReadValue(PLOUGHSIDE_PIN_2, false);
}

// ---------------------------------------------------------------------------
// Position interpolation (GetActualPosition(), exercised through Update())
//
// Update()'s position averaging is a one-pole IIR filter seeded from 0
// (position = (position + getActualPosition()) / 2), and only runs on every
// *other* call (updateFlag alternates, starting false -- the first call is a
// warm-up that discards its reading). So after exactly 2 Update() calls from
// a fresh instance, GetPosition() == (0 + expectedReading) / 2, computed with
// plain integer division -- that's what every case below asserts against,
// not the raw calibration reading itself.
// ---------------------------------------------------------------------------

test(ImplementPlough, position_descendingCalibration_atPoint0) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    // Default calibration is descending: data={600,461,308}, points={34,42,50}, shares=4.
    // At raw=600 (point 0): actualPosition=34cm exactly -> reading=34*4=136.
    analogReadValue(POSITION_SENS_PIN_2, 600);
    impl.Update(0, 0);
    impl.Update(0, 0);
    assertEqual(impl.GetPosition(), (short int)((0 + 136) / 2));
}

test(ImplementPlough, position_descendingCalibration_atPoint1) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    // At raw=461 (point 1): actualPosition=42cm exactly -> reading=42*4=168.
    analogReadValue(POSITION_SENS_PIN_2, 461);
    impl.Update(0, 0);
    impl.Update(0, 0);
    assertEqual(impl.GetPosition(), (short int)((0 + 168) / 2));
}

test(ImplementPlough, position_descendingCalibration_atPoint2) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    // At raw=308 (point 2): actualPosition=50cm exactly -> reading=50*4=200.
    analogReadValue(POSITION_SENS_PIN_2, 308);
    impl.Update(0, 0);
    impl.Update(0, 0);
    assertEqual(impl.GetPosition(), (short int)((0 + 200) / 2));
}

test(ImplementPlough, position_ascendingCalibration_atPoint1) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    // Reprogram calibration data to ascending order via the public setter,
    // exercising GetActualPosition()'s other branch (data[0] < data[1]).
    analogReadValue(POSITION_SENS_PIN_2, 0);
    impl.SetPositionCalibrationData(0);
    analogReadValue(POSITION_SENS_PIN_2, 100);
    impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 200);
    impl.SetPositionCalibrationData(2);

    // At raw=100 (== data[1], points[1]=42cm): actualPosition=42cm exactly -> 42*4=168.
    analogReadValue(POSITION_SENS_PIN_2, 100);
    impl.Update(0, 0);
    impl.Update(0, 0);
    assertEqual(impl.GetPosition(), (short int)((0 + 168) / 2));
}

// ---------------------------------------------------------------------------
// Adjust() -- manual mode (2/3): actualPosition = setpoint - direction*(error+1).
// setpoint is left at its untouched default (0); error defaults to 2, so
// actualPosition = -3*direction. No time passes in these, so the end-shutoff
// latch (which manual mode respects since #244, see below) never trips and
// they exercise pure wide/narrow/stop selection. the valves run at kValvePwm, 255.
// ---------------------------------------------------------------------------

test(ImplementPlough, adjust_manualMode_directionPositive_narrows) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.Adjust(2, 1);  // actualPosition = 0 - 1*3 = -3 < setpoint(0)-error(2)=-2
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), LOW);
}

test(ImplementPlough, adjust_manualMode_directionNegative_widens) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.Adjust(2, -1);  // actualPosition = 0 - (-1)*3 = 3 > setpoint(0)+error(2)=2
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), HIGH);
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), LOW);
}

test(ImplementPlough, adjust_manualMode_directionZero_stops) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    // Leave a nonzero value in place first so Stop() zeroing it is observable.
    impl.Adjust(2, 1);
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);

    impl.Adjust(2, 0);  // actualPosition = 0 - 0*3 = 0, within [setpoint-error, setpoint+error]
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), LOW);
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), LOW);
}

// ---------------------------------------------------------------------------
// Adjust() -- end-shutoff latch (auto mode, mode=0; manual mode latches the
// same way since #244, tests after this one). Auto mode reads the private
// `position` member -- reached indirectly by
// extrapolating GetActualPosition() well past the calibrated range (the
// interpolation formula is unclamped) to land on a deeply negative reading,
// then averaging it in via two Update() calls, same as the position tests above.
// ---------------------------------------------------------------------------

test(ImplementPlough, adjust_autoMode_endShutoff_latchesAfterShutoffTime) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);

    // Ascending calibration [0,100,200] with the default points [34,42,50]:
    // extrapolating far below data[0] (raw=-1000) gives an exact, deeply
    // negative reading: a=-1000, b=100, c=8, d=34 -> (-1000*8)/100=-80.0 exact
    // -> +34=-46.0 -> *4=-184.0 exact.
    analogReadValue(POSITION_SENS_PIN_2, 0);
    impl.SetPositionCalibrationData(0);
    analogReadValue(POSITION_SENS_PIN_2, 100);
    impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 200);
    impl.SetPositionCalibrationData(2);

    analogReadValue(POSITION_SENS_PIN_2, -1000);
    impl.Update(0, 0);
    impl.Update(0, 0);
    assertEqual(impl.GetPosition(), (short int)((0 + (-184)) / 2));  // -92

    // setpoint stays at its untouched default (0); error=2 -> position(-92) is
    // far below setpoint-error(-2), so every call below takes the "Narrower" branch.
    impl.Adjust(0, 0);  // 1st call: -92 is far from the shutoff position (0) -> shutoffTimer resets to millis()=0
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);

    impl.Adjust(0, 0);  // 2nd call, same millis(): actualPosition unchanged -> shutoffTimer NOT reset; still narrows
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);

    // Advance past shutoffTime (3000 ms per SHUTOFF_2). This call still narrows
    // (the block runs before the shutoff check latches shutoffNarrow=true).
    millisValue(3001);
    impl.Adjust(0, 0);
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);

    // shutoffNarrow is now latched. In auto mode (mode<2), the latch's
    // (!shutoffNarrow || mode>=2) guard is now false -> falls through to Stop(),
    // even though position is still far past the setpoint+error window.
    analogWrite(OUTPUT_NARROW_2, 255);  // re-seed a nonzero value so Stop() zeroing it is observable
    millisValue(3002);
    impl.Adjust(0, 0);
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), LOW);
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), LOW);
}

// Manual mode latches the same way as auto (#244, owner): the end shutoff is
// there to protect the hydraulics, so holding the button against an end stop
// must cut the valve too. The check uses the measured position, as in auto.
// Holds the measured position at -92 (the auto test above explains the
// numbers) so it never changes while a test holds a button.
static void holdPositionStill(ImplementPlough& impl) {
    analogReadValue(POSITION_SENS_PIN_2, 0);
    impl.SetPositionCalibrationData(0);
    analogReadValue(POSITION_SENS_PIN_2, 100);
    impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 200);
    impl.SetPositionCalibrationData(2);
    analogReadValue(POSITION_SENS_PIN_2, -1000);
    impl.Update(2, 0);
    impl.Update(2, 0);
}

// Holds Narrower in manual for just over the shutoff time with the position
// standing still, so the narrower direction latches.
static void latchNarrowerInManual(ImplementPlough& impl) {
    holdPositionStill(impl);
    millisValue(0);
    impl.Adjust(2, 1);
    millisValue(3001);
    impl.Adjust(2, 1);
}

test(ImplementPlough, adjust_manualMode_endShutoff_latchesAfterShutoffTime) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    holdPositionStill(impl);
    millisValue(0);
    impl.Adjust(2, 1);                                   // Narrower held
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
    millisValue(3001);
    impl.Adjust(2, 1);                                   // past the shutoff time: still drives, then latches
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
    millisValue(3002);
    impl.Adjust(2, 1);                                   // still held: the valve is cut
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), LOW);
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), LOW);
}

test(ImplementPlough, adjust_manualMode_endShutoff_releaseAndPressAgainStaysOff) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    latchNarrowerInManual(impl);
    millisValue(3100);
    impl.Adjust(2, 0);                                   // button released
    millisValue(3200);
    impl.Adjust(2, 1);                                   // pressed again the same way
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), LOW);
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), LOW);
}

test(ImplementPlough, adjust_manualMode_endShutoff_otherWayClearsTheLatch) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    latchNarrowerInManual(impl);
    millisValue(3100);
    impl.Adjust(2, -1);                                  // Wider: allowed, and clears the latch
    assertEqual(digitalWriteValue(OUTPUT_WIDE_2), HIGH);
    millisValue(3200);
    impl.Adjust(2, 1);                                   // Narrower drives again
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
}

test(ImplementPlough, adjust_manualMode_endShutoff_movingPloughNeverLatches) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    holdPositionStill(impl);
    for (unsigned long t = 0; t <= 6000; t += 500) {
        millisValue(t);
        analogReadValue(POSITION_SENS_PIN_2, -1000 + (int)t / 10);   // the plough moves
        impl.Update(2, 0);
        impl.Update(2, 0);
        impl.Adjust(2, 1);
    }
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
}

// Calibration [0,100,200] -> [34,42,50] cm per share, 4 shares: the width is
// 136 + 0.32 * raw cm, truncated. Raw 97 reads 167 and raw 100 reads 168.
static void calibrateLinear(ImplementPlough& impl) {
    analogReadValue(POSITION_SENS_PIN_2, 0);
    impl.SetPositionCalibrationData(0);
    analogReadValue(POSITION_SENS_PIN_2, 100);
    impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 200);
    impl.SetPositionCalibrationData(2);
}

// Session 21 B3 (#279): at the end stop the sensor wiggles a few counts and the
// filtered width flips by 1 cm. That is not movement: the valve is still cut.
test(ImplementPlough, adjust_manualMode_endShutoff_sensorJitterStillLatches) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    calibrateLinear(impl);
    bool cut = false;
    for (unsigned long t = 0; t <= 4000; t += 100) {
        millisValue(t);
        analogReadValue(POSITION_SENS_PIN_2, (t / 100) % 2 ? 97 : 100);   // 167 / 168 cm
        impl.Update(2, 0);
        impl.Update(2, 0);
        impl.Adjust(2, 1);                               // Narrower held against the stop
        if (digitalWriteValue(OUTPUT_NARROW_2) == LOW) {
            assertMore(t, (unsigned long)SHUTOFF_2);     // not before the shutoff time
            cut = true;
            break;
        }
    }
    assertTrue(cut);
}

// A slow real move (about 1 cm/s) passes kShutoffMoveCm well inside the
// shutoff time, so it keeps driving.
test(ImplementPlough, adjust_manualMode_endShutoff_slowMoveNeverLatches) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    calibrateLinear(impl);
    for (unsigned long t = 0; t <= 8000; t += 100) {
        millisValue(t);
        analogReadValue(POSITION_SENS_PIN_2, 50 + (int)(t / 333));   // +1 count per 333 ms
        impl.Update(2, 0);
        impl.Update(2, 0);
        impl.Adjust(2, 1);
        assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
    }
}

// ---------------------------------------------------------------------------
// SetOffset (private, exercised through Update()) -- EEPROM-backed offset,
// which is the plough's total working width in cm. Default offset=160
// (shares*40), valid range [shares*20, shares*60] = [80, 240] with shares=4,
// clamped at both ends (#152). Update() only calls setOffset() when (a) at
// least 200ms elapsed since construction/last update and (b) mode < 2.
// ---------------------------------------------------------------------------

test(ImplementPlough, setOffset_withinRange_adds) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(0, 50);  // 160 + 50 = 210, within (80,240]
    assertEqual(impl.GetOffset(), (short int)210);
}

// Past a limit the width stops at the limit. It used to jump to the middle
// (shares*40), which these two tests pinned as if it were intended (#152).
test(ImplementPlough, setOffset_aboveUpperBound_clampsAtMaximum) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(0, 100);  // 160 + 100 = 260 > 240 -> clamped to shares*60=240
    assertEqual(impl.GetOffset(), (short int)240);
}

test(ImplementPlough, setOffset_belowLowerBound_clampsAtMinimum) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(0, -100);  // 160 - 100 = 60 < 80 -> clamped to shares*20=80
    assertEqual(impl.GetOffset(), (short int)80);
}

// The operator's actual case: holding Wider at the widest setting. Every
// further press must leave it there, not swing it to the middle.
test(ImplementPlough, setOffset_atMaximum_furtherPressesStayThere) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(0, 80);   // 160 + 80 = 240, exactly the maximum
    assertEqual(impl.GetOffset(), (short int)240);
    millisValue(400);
    impl.Update(0, 1);    // one more press
    assertEqual(impl.GetOffset(), (short int)240);
    millisValue(600);
    impl.Update(0, 1);
    assertEqual(impl.GetOffset(), (short int)240);
}

// The clamped value is what reaches the EEPROM, so it survives a restart
// instead of reading back as the middle.
test(ImplementPlough, setOffset_clampedValueIsPersisted) {
    resetAll();
    {
        ImplementPlough impl(nullptr, &mockGuidance);
        millisValue(200);
        impl.Update(0, -100);  // clamped to 80
    }
    ImplementPlough restarted(nullptr, &mockGuidance);
    assertEqual(restarted.GetOffset(), (short int)80);
}

test(ImplementPlough, setOffset_zeroCorrection_isNoOp) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(0, 0);
    assertEqual(impl.GetOffset(), (short int)160);
}

test(ImplementPlough, setOffset_manualMode_neverCalled) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(2, 90);  // mode>=2 -> Update()'s "mode < 2" guard skips setOffset entirely
    assertEqual(impl.GetOffset(), (short int)160);
}

// The width's range follows the number of shares (shares*20 .. shares*60). A
// width the new range doesn't hold goes to the middle at once, the same as
// readOffset() at boot, instead of only after a reboot (#238).
test(ImplementPlough, setShares_widthBelowNewRange_goesToTheMiddle) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);       // 4 shares, width 160
    impl.SetShares(10);                                  // range 200-600
    assertEqual(impl.GetOffset(), (short int)400);
}

test(ImplementPlough, setShares_widthAboveNewRange_goesToTheMiddle) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(200);
    impl.Update(0, 80);                                  // 160 + 80 = 240, the maximum on 4 shares
    assertEqual(impl.GetOffset(), (short int)240);
    impl.SetShares(2);                                   // range 40-120
    assertEqual(impl.GetOffset(), (short int)80);
}

test(ImplementPlough, setShares_widthInsideNewRange_isKept) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.SetShares(5);                                   // range 100-300 holds 160
    assertEqual(impl.GetOffset(), (short int)160);
}

// The wizard's store saves the width the share change set, and its decline
// restores the stored one: what the main screen shows is what comes back after
// a restart. Bench 2026-10-20 (W5): width 180, shares 4 -> 10 -> 4 showed 160,
// but the restart brought back 180 from EEPROM, valid again on 4 shares (#238).
test(ImplementPlough, setShares_storedWidthComesBackAfterARestart) {
    resetAll();
    {
        ImplementPlough impl(nullptr, &mockGuidance);
        millisValue(200);
        impl.Update(0, 20);                              // 160 + 20 = 180, saved by the press
        assertEqual(impl.GetOffset(), (short int)180);
        impl.SetShares(10);                              // 400
        impl.SetShares(4);                               // 160
        assertEqual(impl.GetOffset(), (short int)160);
        impl.CommitCalibration();                        // the wizard's store
    }
    ImplementPlough restarted(nullptr, &mockGuidance);
    assertEqual(restarted.GetOffset(), (short int)160);
}

test(ImplementPlough, setShares_declinedWizardRestoresTheStoredWidth) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.CommitCalibration();                            // a stored calibration with 4 shares
    millisValue(200);
    impl.Update(0, 20);                                  // 180, saved
    impl.SetShares(10);                                  // 400 in RAM
    impl.ResetCalibration();                             // the wizard's decline: shares back to 4
    assertEqual((int)impl.GetShares(), 4);
    assertEqual(impl.GetOffset(), (short int)180);
}

// ---------------------------------------------------------------------------
// SetSetpoint (private, exercised through Update()) -- the correction is the
// XTE itself (bang-bang, gain 1) and max-correction clamping (50 mm per share
// on 4 shares = 20 cm default, #163). setpoint = offset - pe (GetSide() reads false by default, so the
// sign multiplier (GetSide()*2-1) is -1); offset defaults to 160.
// ---------------------------------------------------------------------------

test(ImplementPlough, setSetpoint_withinMaxCorrection) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(1);  // > impl's internal lastXteFix(0) -> SetXte's timestamp triggers recompute
    mockGuidance.SetXte(15);
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - 15));
}

test(ImplementPlough, setSetpoint_clampsAboveMaxCorrection) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(1);
    mockGuidance.SetXte(100);  // pe=100 > max correction (20 cm) -> clamped to 20
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - 20));
}

test(ImplementPlough, setSetpoint_clampsBelowNegativeMaxCorrection) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(1);
    mockGuidance.SetXte(-100);  // pe=-100 <= -max correction (-20 cm) -> clamped to -20
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - (-20)));
}

// The setpoint is in whole cm, so a limit with a half centimetre rounds down:
// 55 mm per share on 3 shares is 16.5 cm, which allows 16, never 17 (#163).
test(ImplementPlough, setSetpoint_halfCentimetreLimit_roundsDown) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.SetShares(3);
    impl.SetMaxCorrectionPerShare(55);
    millisValue(1);
    mockGuidance.SetXte(30);
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - 16));
    millisValue(2);
    mockGuidance.SetXte(-30);
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 + 16));
}

// ---------------------------------------------------------------------------
// Degenerate calibration: equal points make the interpolation divide by zero
// ---------------------------------------------------------------------------

test(ImplementPlough, equalCalibrationPoints_doNotProduceNonFinitePosition) {
    // The wizard latches analogRead() at each of three steps without checking
    // the captures differ, so a disconnected or seized potentiometer -- which
    // rails to a constant -- records the same value three times. That put a
    // zero denominator into the interpolation, and narrowing the resulting inf
    // or NaN to short int is undefined behaviour.
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);

    analogReadValue(POSITION_SENS_PIN_2, 512);
    impl.SetPositionCalibrationData(0);
    impl.SetPositionCalibrationData(1);
    impl.SetPositionCalibrationData(2);

    analogReadValue(POSITION_SENS_PIN_2, 512);
    impl.Update(2, 0);
    impl.Update(2, 0);
    const short int position = impl.GetPosition();

    // Any finite, in-range value is acceptable; the point is that the result is
    // a number the caller can act on rather than garbage from a bad conversion.
    assertMoreOrEqual(position, (short int)-32000);
    assertLessOrEqual(position, (short int)32000);
}

test(ImplementPlough, partiallyEqualCalibrationPoints_doNotProduceNonFinitePosition) {
    // Only two of the three captures coinciding is enough: the segment the
    // reading falls into is the one that divides by zero.
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);

    analogReadValue(POSITION_SENS_PIN_2, 300);
    impl.SetPositionCalibrationData(0);
    impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 700);
    impl.SetPositionCalibrationData(2);

    analogReadValue(POSITION_SENS_PIN_2, 300);
    impl.Update(2, 0);
    impl.Update(2, 0);
    const short int position = impl.GetPosition();

    assertMoreOrEqual(position, (short int)-32000);
    assertLessOrEqual(position, (short int)32000);
}

test(ImplementPlough, corruptEepromPositionData_fallsBackToDefaults) {
    // The stored points are 16-bit but the sensor is a 10-bit ADC, so values
    // above 1023 cannot have come from it. They were previously loaded as-is
    // and fed straight into the divisor.
    resetAll();

    // Mark the block "written" the way readCalibrationData() detects it, then
    // store an impossible first calibration point (0xFFFE) and two equal ones.
    EEPROM.write(52, 0);
    EEPROM.write(40, 0xFF); EEPROM.write(41, 0xFE);
    EEPROM.write(42, 0x02); EEPROM.write(43, 0x00);
    EEPROM.write(44, 0x02); EEPROM.write(45, 0x00);

    ImplementPlough corrupt(nullptr, &mockGuidance);

    analogReadValue(POSITION_SENS_PIN_2, 600);
    corrupt.Update(2, 0);
    corrupt.Update(2, 0);
    const short int fromCorrupt = corrupt.GetPosition();

    // Erased EEPROM is the known-good "no calibration data" path. Corrupt data
    // has to land on the same defaults rather than on whatever it contained.
    resetAll();
    ImplementPlough reference(nullptr, &mockGuidance);
    analogReadValue(POSITION_SENS_PIN_2, 600);
    reference.Update(2, 0);
    reference.Update(2, 0);

    assertEqual(fromCorrupt, reference.GetPosition());
}

// ---------------------------------------------------------------------------
// EEPROM persistence (NeptuneGPS_Triton#88): the write/read round trip, the
// range checks on every stored byte, and the serial dump.
// ---------------------------------------------------------------------------

struct PloughCaptureStream : public Stream {
    std::string out;
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
};

test(ImplementPlough, resetCalibration_erasedEeprom_reportsNoData) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    assertFalse(impl.ResetCalibration());
    assertEqual((int)impl.GetShares(), 4);
    assertEqual((int)impl.GetDeadband(), 2);
    assertEqual((int)impl.GetMaxCorrectionPerShare(), 50);
    assertEqual(impl.GetMaxCorrection(), (short int)200);
    assertFalse(impl.GetSide());
}

test(ImplementPlough, commitThenFreshInstance_roundTripsEverySetting) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    analogReadValue(POSITION_SENS_PIN_2, 700); impl.SetPositionCalibrationData(0);
    analogReadValue(POSITION_SENS_PIN_2, 500); impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 300); impl.SetPositionCalibrationData(2);
    impl.SetShares(6);
    impl.SetDeadband(7);
    impl.SetMaxCorrectionPerShare(35);
    impl.SetSwap(true);
    impl.CommitCalibration();

    PloughCaptureStream dbg;
    ImplementPlough fresh(&dbg, &mockGuidance);
    assertTrue(fresh.ResetCalibration());
    assertEqual((int)fresh.GetShares(), 6);
    assertEqual((int)fresh.GetDeadband(), 7);
    assertEqual((int)fresh.GetMaxCorrectionPerShare(), 35);
    assertEqual(fresh.GetMaxCorrection(), (short int)210);
    assertTrue(fresh.GetSide());                 // swap=1 with the side pin low

    fresh.PrintCalibrationData();
    assertTrue(dbg.has("Offset calibration data\n700, 34\n500, 42\n300, 50\n"));
    assertTrue(dbg.has("Number of shares\n6\n"));
    assertTrue(dbg.has("Deadband (cm)\n7\n"));
    assertTrue(dbg.has("error ploughside\n1\n"));
    assertTrue(dbg.has("Maximum correction per share (mm)\n35\n"));
    assertTrue(dbg.has("Maximum correction (mm)\n210\n"));
}

test(ImplementPlough, outOfRangeStoredBytes_fallBackToDefaults) {
    resetAll();
    // Valid, distinct position data so the block counts as present ...
    EEPROM.write(40, 0x02); EEPROM.write(41, 0xBC);   // 700
    EEPROM.write(42, 0x01); EEPROM.write(43, 0xF4);   // 500
    EEPROM.write(44, 0x01); EEPROM.write(45, 0x2C);   // 300
    // ... and every scalar out of its accepted range.
    EEPROM.write(58, 200);   // deadband above 10
    EEPROM.write(60, 200);   // max correction above 100 mm per share
    EEPROM.write(62, 9);     // swap not 0/1
    EEPROM.write(64, 99);    // shares above 10
    ImplementPlough impl(nullptr, &mockGuidance);
    assertEqual((int)impl.GetDeadband(), 2);
    assertEqual((int)impl.GetMaxCorrectionPerShare(), 50);
    assertEqual((int)impl.GetShares(), 4);
    assertFalse(impl.GetSide());
}

test(ImplementPlough, printCalibrationData_onErasedEeprom_printsTheDefaults) {
    resetAll();
    PloughCaptureStream dbg;
    ImplementPlough impl(&dbg, &mockGuidance);
    impl.PrintCalibrationData();
    assertTrue(dbg.has("Times started\n255\n"));
    assertTrue(dbg.has("Offset calibration data\n600, 34\n461, 42\n308, 50\n"));
    assertTrue(dbg.has("Number of shares\n4\n"));
    assertTrue(dbg.has("Deadband (cm)\n2\n"));
    assertTrue(dbg.has("error ploughside\n0\n"));
    assertTrue(dbg.has("Maximum correction per share (mm)\n50\n"));
    assertTrue(dbg.has("Maximum correction (mm)\n200\n"));
}

// ---------------------------------------------------------------------------
// Max correction (#163): kept per share, in mm, in one EEPROM byte; the limit
// is per share times the shares. 25 to 100 mm per share, default 50. The load
// used to accept only values under 10 (then the total, in cm), so the 20 cm
// the owner sets was reset to 50 cm at every boot.
// ---------------------------------------------------------------------------

// A stored scalar block with this share count and max correction per share;
// the position data is left erased, which only resets it to its defaults.
static void storeSharesAndMaxCorrectionPerShare(byte shares, byte perShareMm) {
    resetAll();
    EEPROM.write(64, shares);
    EEPROM.write(60, perShareMm);
}

// The #163 regression: a saved setting survives a boot.
test(ImplementPlough, maxCorrection_savedPerShare_survivesABoot) {
    storeSharesAndMaxCorrectionPerShare(4, 60);
    ImplementPlough impl(nullptr, &mockGuidance);
    assertEqual((int)impl.GetMaxCorrectionPerShare(), 60);
    assertEqual(impl.GetMaxCorrection(), (short int)240);
}

// 25 to 100 mm per share, both ends included.
test(ImplementPlough, maxCorrection_perShareRangeIsTwentyFiveToHundredMm) {
    storeSharesAndMaxCorrectionPerShare(4, 25);
    assertEqual((int)ImplementPlough(nullptr, &mockGuidance).GetMaxCorrectionPerShare(), 25);
    storeSharesAndMaxCorrectionPerShare(4, 100);
    assertEqual((int)ImplementPlough(nullptr, &mockGuidance).GetMaxCorrectionPerShare(), 100);
    storeSharesAndMaxCorrectionPerShare(4, 24);
    assertEqual((int)ImplementPlough(nullptr, &mockGuidance).GetMaxCorrectionPerShare(), 50);
    storeSharesAndMaxCorrectionPerShare(4, 101);
    assertEqual((int)ImplementPlough(nullptr, &mockGuidance).GetMaxCorrectionPerShare(), 50);
}

// The limit is per share times the stored share count.
test(ImplementPlough, maxCorrection_isPerShareTimesTheShares) {
    storeSharesAndMaxCorrectionPerShare(5, 50);
    assertEqual(ImplementPlough(nullptr, &mockGuidance).GetMaxCorrection(), (short int)250);
    storeSharesAndMaxCorrectionPerShare(3, 25);
    assertEqual(ImplementPlough(nullptr, &mockGuidance).GetMaxCorrection(), (short int)75);
}

// A 4+1 plough: a four-share plough that takes an extra share. The limit
// sizes up with the share, and keeps the operator's per-share setting.
test(ImplementPlough, maxCorrection_fourPlusOne_sizesWithTheExtraShare) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    assertEqual(impl.GetMaxCorrection(), (short int)200);
    impl.SetMaxCorrectionPerShare(60);
    impl.SetShares(5);                                    // the extra share on
    assertEqual(impl.GetMaxCorrection(), (short int)300);
    impl.CommitCalibration();
    ImplementPlough fresh(nullptr, &mockGuidance);
    assertEqual(fresh.GetMaxCorrection(), (short int)300);
    fresh.SetShares(4);                                   // and off again
    assertEqual(fresh.GetMaxCorrection(), (short int)240);
}

// A board that saved the owner's usual 20 (cm, the old format) reads 20 mm
// per share now: below the range, so it gets the default -- which is 20 cm on
// 4 shares.
test(ImplementPlough, maxCorrection_oldTwentyCentimetres_becomesTheDefault) {
    storeSharesAndMaxCorrectionPerShare(4, 20);
    ImplementPlough impl(nullptr, &mockGuidance);
    assertEqual((int)impl.GetMaxCorrectionPerShare(), 50);
    assertEqual(impl.GetMaxCorrection(), (short int)200);
}

// ---------------------------------------------------------------------------
// Shares and deadband: the boot load accepts exactly what the calibration
// wizard can set, 2 to 10 both, so nothing the operator saves is reset at
// the next boot (the #163 trap: shares and the deadband used to stop at 9).
// ---------------------------------------------------------------------------

static byte loadedShares(byte stored) {
    resetAll();
    EEPROM.write(64, stored);
    return ImplementPlough(nullptr, &mockGuidance).GetShares();
}

static byte loadedDeadband(byte stored) {
    resetAll();
    EEPROM.write(58, stored);
    return ImplementPlough(nullptr, &mockGuidance).GetDeadband();
}

test(ImplementPlough, shares_loadAcceptsTwoToTen) {
    assertEqual((int)loadedShares(2), 2);
    assertEqual((int)loadedShares(10), 10);
    assertEqual((int)loadedShares(1), 4);
    assertEqual((int)loadedShares(11), 4);
}

test(ImplementPlough, deadband_loadAcceptsTwoToTen) {
    assertEqual((int)loadedDeadband(2), 2);
    assertEqual((int)loadedDeadband(10), 10);
    assertEqual((int)loadedDeadband(1), 2);
    assertEqual((int)loadedDeadband(11), 2);
}

// ---------------------------------------------------------------------------
// Bypass valve (EEPROM 68, 1 = fitted). Fitted: driven with the narrower and
// wider valve, closed at Stop. Not fitted, the default: the output stays off.
// ---------------------------------------------------------------------------

static bool loadedBypass(byte stored) {
    resetAll();
    EEPROM.write(68, stored);
    return ImplementPlough(nullptr, &mockGuidance).GetBypass();
}

test(ImplementPlough, bypass_storedAsOneOrZero) {
    assertTrue(loadedBypass(1));
    assertFalse(loadedBypass(0));
    assertFalse(loadedBypass(7));    // not 0/1: no
    resetAll();
    assertFalse(ImplementPlough(nullptr, &mockGuidance).GetBypass());   // erased: no
}

test(ImplementPlough, bypass_survivesCommitAndBoot) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.SetBypass(true);
    impl.CommitCalibration();
    assertEqual((int)EEPROM.read(68), 1);
    assertTrue(ImplementPlough(nullptr, &mockGuidance).GetBypass());
}

test(ImplementPlough, bypass_fitted_drivenWithTheValvesAndClosedAtStop) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.SetBypass(true);
    impl.Adjust(2, 1);                                   // narrower
    assertEqual(analogWriteValue(OUTPUT_BYPASS_2), 255);
    impl.Adjust(2, 0);                                   // within the deadband: stop
    assertEqual(analogWriteValue(OUTPUT_BYPASS_2), 0);
    impl.Adjust(2, -1);                                  // wider
    assertEqual(analogWriteValue(OUTPUT_BYPASS_2), 255);
    impl.Stop();
    assertEqual(analogWriteValue(OUTPUT_BYPASS_2), 0);
}

test(ImplementPlough, bypass_notFitted_outputStaysOff) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.Adjust(2, 1);
    assertEqual(digitalWriteValue(OUTPUT_NARROW_2), HIGH);
    assertEqual(analogWriteValue(OUTPUT_BYPASS_2), 0);
    impl.Adjust(2, -1);
    assertEqual(analogWriteValue(OUTPUT_BYPASS_2), 0);
}
