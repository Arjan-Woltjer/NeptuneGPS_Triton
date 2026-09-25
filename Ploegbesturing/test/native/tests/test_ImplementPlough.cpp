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
// positionCalibrationPoints={34,42,50}, offset=160, shares=4, error=2,
// maxCorrection=50, kp=100 -- see ImplementPlough.cpp's constructor).
// ---------------------------------------------------------------------------
static GuidanceSource mockGuidance;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    mockGuidance = GuidanceSource();   // xte 0, no fix ever (all timestamps 0)
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
// actualPosition = -3*direction. Manual mode (mode>=2) always bypasses the
// end-shutoff latch's blocking effect (see the shutoff-latch test below for
// why), so this exercises pure wide/narrow/stop selection without shutoff
// interference. manPwm defaults to 255 (PWM_MAN undefined).
// ---------------------------------------------------------------------------

test(ImplementPlough, adjust_manualMode_directionPositive_narrows) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.Adjust(2, 1);  // actualPosition = 0 - 1*3 = -3 < setpoint(0)-error(2)=-2
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 255);
    assertEqual(analogWriteValue(OUTPUT_WIDE_2), 0);
}

test(ImplementPlough, adjust_manualMode_directionNegative_widens) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    impl.Adjust(2, -1);  // actualPosition = 0 - (-1)*3 = 3 > setpoint(0)+error(2)=2
    assertEqual(analogWriteValue(OUTPUT_WIDE_2), 255);
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 0);
}

test(ImplementPlough, adjust_manualMode_directionZero_stops) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    // Leave a nonzero value in place first so Stop() zeroing it is observable.
    impl.Adjust(2, 1);
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 255);

    impl.Adjust(2, 0);  // actualPosition = 0 - 0*3 = 0, within [setpoint-error, setpoint+error]
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 0);
    assertEqual(analogWriteValue(OUTPUT_WIDE_2), 0);
}

// ---------------------------------------------------------------------------
// Adjust() -- end-shutoff latch (auto mode, mode=0). Manual mode's mode>=2
// always overrides the latch's blocking effect, so this needs auto mode,
// which reads the private `position` member -- reached indirectly by
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
    impl.Adjust(0, 0);  // 1st call: lastPosition(0) != actualPosition(-92) -> shutoffTimer resets to millis()=0
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 255);

    impl.Adjust(0, 0);  // 2nd call, same millis(): actualPosition unchanged -> shutoffTimer NOT reset; still narrows
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 255);

    // Advance past shutoffTime (3000 ms per SHUTOFF_2). This call still narrows
    // (the block runs before the shutoff check latches shutoffNarrow=true).
    millisValue(3001);
    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 255);

    // shutoffNarrow is now latched. In auto mode (mode<2), the latch's
    // (!shutoffNarrow || mode>=2) guard is now false -> falls through to Stop(),
    // even though position is still far past the setpoint+error window.
    analogWrite(OUTPUT_NARROW_2, 255);  // re-seed a nonzero value so Stop() zeroing it is observable
    millisValue(3002);
    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 0);
    assertEqual(analogWriteValue(OUTPUT_WIDE_2), 0);
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

// ---------------------------------------------------------------------------
// SetSetpoint (private, exercised through Update()) -- proportional-gain
// (kp=100 default -> gain 1.0) and max-correction clamping (maxCorrection=50
// default). setpoint = offset - pe (GetSide() reads false by default, so the
// sign multiplier (GetSide()*2-1) is -1); offset defaults to 160.
// ---------------------------------------------------------------------------

test(ImplementPlough, setSetpoint_withinMaxCorrection) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(1);
    mockGuidance.SetXte(30);   // stamps lastXteFix=1 > impl's internal lastXteFix(0) -> triggers recompute
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - 30));
}

test(ImplementPlough, setSetpoint_clampsAboveMaxCorrection) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(1);
    mockGuidance.SetXte(100);  // pe=100 > maxCorrection(50) -> clamped to 50
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - 50));
}

test(ImplementPlough, setSetpoint_clampsBelowNegativeMaxCorrection) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    millisValue(1);
    mockGuidance.SetXte(-100);  // pe=-100 <= -maxCorrection(-50) -> clamped to -50
    impl.Update(0, 0);
    assertEqual(impl.GetSetpoint(), (short int)(160 - (-50)));
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
// EEPROM persistence (NeptuneGPS_Triton#89): the write/read round trip, the
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
    assertEqual((int)impl.GetError(), 2);
    assertEqual(impl.GetMaxCorrection(), (short int)50);
    assertFalse(impl.GetSide());
}

test(ImplementPlough, commitThenFreshInstance_roundTripsEverySetting) {
    resetAll();
    ImplementPlough impl(nullptr, &mockGuidance);
    analogReadValue(POSITION_SENS_PIN_2, 700); impl.SetPositionCalibrationData(0);
    analogReadValue(POSITION_SENS_PIN_2, 500); impl.SetPositionCalibrationData(1);
    analogReadValue(POSITION_SENS_PIN_2, 300); impl.SetPositionCalibrationData(2);
    impl.SetShares(6);
    impl.SetError(7);
    impl.SetMaxCorrection(5);
    impl.SetSwap(true);
    impl.CommitCalibration();

    PloughCaptureStream dbg;
    ImplementPlough fresh(&dbg, &mockGuidance);
    assertTrue(fresh.ResetCalibration());
    assertEqual((int)fresh.GetShares(), 6);
    assertEqual((int)fresh.GetError(), 7);
    assertEqual(fresh.GetMaxCorrection(), (short int)5);
    assertTrue(fresh.GetSide());                 // swap=1 with the side pin low

    fresh.PrintCalibrationData();
    assertTrue(dbg.has("Offset calibration data\n700, 34\n500, 42\n300, 50\n"));
    assertTrue(dbg.has("Number of shares\n6\n"));
    assertTrue(dbg.has("error margin\n7\n"));
    assertTrue(dbg.has("error ploughside\n1\n"));
    assertTrue(dbg.has("Maximum correction\n5\n"));
}

test(ImplementPlough, outOfRangeStoredBytes_fallBackToDefaults) {
    resetAll();
    // Valid, distinct position data so the block counts as present ...
    EEPROM.write(40, 0x02); EEPROM.write(41, 0xBC);   // 700
    EEPROM.write(42, 0x01); EEPROM.write(43, 0xF4);   // 500
    EEPROM.write(44, 0x01); EEPROM.write(45, 0x2C);   // 300
    // ... and every scalar out of its accepted range.
    EEPROM.write(58, 200);   // error margin >= 10
    EEPROM.write(60, 200);   // max correction >= 10
    EEPROM.write(62, 9);     // swap not 0/1
    EEPROM.write(64, 99);    // shares >= 10
    ImplementPlough impl(nullptr, &mockGuidance);
    assertEqual((int)impl.GetError(), 2);
    assertEqual(impl.GetMaxCorrection(), (short int)50);
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
    assertTrue(dbg.has("error margin\n2\n"));
    assertTrue(dbg.has("error ploughside\n0\n"));
    assertTrue(dbg.has("Maximum correction\n50\n"));
}
