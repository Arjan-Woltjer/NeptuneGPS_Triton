/*
  test_ImplementPlanter - Tests for ImplementPlanter: position/XTE calibration
  interpolation (incl. the degenerate-calibration divide-by-zero guard),
  Adjust()'s sensor-enabled vs sensor-disabled (shutoff-latch) decision, and
  EEPROM calibration-data validation.
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
#include "ImplementPlanter.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// ImplementPlanter's constructor reads calibration data from EEPROM (via
// readCalibrationData()), and most of the logic under test here (position,
// xte, setpoint, the end-shutoff latch) is private state with no reset hook --
// so every test below constructs its own local ImplementPlanter after
// resetAll() re-erases the fake EEPROM, guaranteeing the same "no calibration
// data found" defaults every time: positionCalibrationData=xteCalibrationData
// ={201,428,687}, positionCalibrationPoints={-6,0,6},
// xteCalibrationPoints={-10,0,10}, kp=50, ki=50, kd=1, manPwm=90, autoPwm=70,
// offset=0, gpsEnabled=sensorEnabled=pwmEnabled=onOffValve=
// invertHydraulics=invertPlantingelementSensor=false (see
// ImplementPlanter.cpp's constructor).
// ---------------------------------------------------------------------------
static GuidanceSource mockGuidance;
static VehicleTractor mockTractor;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    // Fresh source: xte 0, no fix ever (all timestamps 0), speed 0, RTK fixed.
    mockGuidance = GuidanceSource();
    mockGuidance.SetQuality(4);
    mockTractor.speed = 0;
}

// ---------------------------------------------------------------------------
// Position interpolation (getActualPosition(), exercised through Update()
// with gpsEnabled=true -- the one path that assigns
// `position = getActualPosition()` directly, with no averaging).
// ---------------------------------------------------------------------------

test(ImplementPlanter, position_ascendingCalibration_atPoint0) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.SetGpsEnabled(true);
    millisValue(1);
    mockGuidance.SetXte(0);  // stamps lastXteFix=1 > impl's internal updateAge(0) -> triggers recompute
    // Default calibration is ascending: data={201,428,687}, points={-6,0,6}.
    analogReadValue(POSITION_SENS_PIN_3, 201);
    impl.Update();
    assertEqual(impl.GetPosition(), -6);
}

test(ImplementPlanter, position_ascendingCalibration_atPoint1) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.SetGpsEnabled(true);
    millisValue(1);
    mockGuidance.SetXte(0);   // stamps lastXteFix=1 -> triggers recompute
    analogReadValue(POSITION_SENS_PIN_3, 428);
    impl.Update();
    assertEqual(impl.GetPosition(), 0);
}

test(ImplementPlanter, position_ascendingCalibration_atPoint2) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.SetGpsEnabled(true);
    millisValue(1);
    mockGuidance.SetXte(0);   // stamps lastXteFix=1 -> triggers recompute
    analogReadValue(POSITION_SENS_PIN_3, 687);
    impl.Update();
    assertEqual(impl.GetPosition(), 6);
}

test(ImplementPlanter, position_equalCalibrationPoints_doNotProduceNonFinitePosition) {
    // The wizard latches analogRead() at each of three steps without checking
    // the captures differ, so a disconnected or seized potentiometer -- which
    // rails to a constant -- records the same value three times. That put a
    // zero denominator into the interpolation, and narrowing the resulting
    // inf/NaN to int is undefined behaviour.
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);

    analogReadValue(POSITION_SENS_PIN_3, 512);
    impl.SetPositionCalibrationData(0);
    impl.SetPositionCalibrationData(1);
    impl.SetPositionCalibrationData(2);

    impl.SetGpsEnabled(true);
    millisValue(1);
    mockGuidance.SetXte(0);   // stamps lastXteFix=1 -> triggers recompute
    analogReadValue(POSITION_SENS_PIN_3, 512);
    impl.Update();
    const int position = impl.GetPosition();

    assertMoreOrEqual(position, -32000);
    assertLessOrEqual(position, 32000);
}

// ---------------------------------------------------------------------------
// XTE interpolation (getActualXte(), exercised through Update() with
// gpsEnabled=false -- `xte = getActualXte()` runs unconditionally in that
// branch, regardless of sensorEnabled. The first call only primes
// updateFlag (seeded false in the constructor -- see ImplementPlanter.cpp);
// the second actually recomputes, matching the "call twice" shape
// Ploegbesturing's own averaging tests use for a different reason.
// ---------------------------------------------------------------------------

test(ImplementPlanter, xte_ascendingCalibration_atPoint0) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    analogReadValue(XTE_SENS_PIN_3, 201);
    millisValue(250);
    impl.Update();
    impl.Update();
    assertEqual(impl.GetXte(), -10);
}

test(ImplementPlanter, xte_ascendingCalibration_atPoint1) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    analogReadValue(XTE_SENS_PIN_3, 428);
    millisValue(250);
    impl.Update();
    impl.Update();
    assertEqual(impl.GetXte(), 0);
}

test(ImplementPlanter, xte_ascendingCalibration_atPoint2) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    analogReadValue(XTE_SENS_PIN_3, 687);
    millisValue(250);
    impl.Update();
    impl.Update();
    assertEqual(impl.GetXte(), 10);
}

// ---------------------------------------------------------------------------
// Adjust() -- sensorEnabled=true branch: setpoint>0/<0/==0 selects
// Left()/Right()/Stop() directly, no shutoff latch in this branch at all.
// manPwm defaults to 90; invertHydraulics defaults to false, so Left()
// drives NARROW and Right() drives WIDE (see ImplementPlanter.cpp's
// Left()/Right()).
// ---------------------------------------------------------------------------

test(ImplementPlanter, adjust_sensorEnabled_positiveDirection_movesLeft) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.SetSensorEnabled(true);
    impl.Adjust(1, 5);  // manual mode -> setpoint=5 -> setpoint>0 -> Left(manPwm)
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 0);
}

test(ImplementPlanter, adjust_sensorEnabled_negativeDirection_movesRight) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.SetSensorEnabled(true);
    impl.Adjust(1, -5);  // setpoint=-5 -> Right(manPwm)
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 90);
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 0);
}

test(ImplementPlanter, adjust_sensorEnabled_zeroDirection_stops) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.SetSensorEnabled(true);
    impl.Adjust(1, 5);
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);

    impl.Adjust(1, 0);  // setpoint=0 -> Stop()
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 0);
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 0);
}

// ---------------------------------------------------------------------------
// Adjust() -- sensorEnabled=false branch (default): actualPosition =
// setpoint(0) - direction in manual mode (mode!=0), compared against
// setpoint(0). Unlike ImplementPlough, this branch's end-shutoff latch is
// NOT bypassed in manual mode -- there's no mode>=2 override in the legacy
// source -- so it's directly reachable without needing auto mode.
// ---------------------------------------------------------------------------

test(ImplementPlanter, adjust_sensorDisabled_manualMode_directionPositive_narrows) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.Adjust(1, 1);  // actualPosition = 0 - 1 = -1 < setpoint(0)
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 0);
}

test(ImplementPlanter, adjust_sensorDisabled_manualMode_directionNegative_widens) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.Adjust(1, -1);  // actualPosition = 0 - (-1) = 1 > setpoint(0)
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 90);
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 0);
}

test(ImplementPlanter, adjust_sensorDisabled_manualMode_directionZero_stops) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    impl.Adjust(1, 1);
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);

    impl.Adjust(1, 0);  // actualPosition = 0 - 0 = 0 == setpoint(0)
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 0);
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 0);
}

test(ImplementPlanter, adjust_sensorDisabled_endShutoff_latchesAfterShutoffTime) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);

    // direction=1 -> actualPosition = -1 every call (setpoint stays 0).
    impl.Adjust(1, 1);  // 1st call: lastPosition(0) != actualPosition(-1) -> shutoffTimer resets to millis()=0
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);

    impl.Adjust(1, 1);  // 2nd call, same millis(): actualPosition unchanged -> shutoffTimer NOT reset; still narrows
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);

    // Advance past shutoffTime (20000ms per SHUTOFF_3). This call still
    // narrows (the block runs before the shutoff check latches shutoffNarrow=true).
    millisValue(20001);
    impl.Adjust(1, 1);
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 90);

    // shutoffNarrow is now latched -> falls through to Stop(), even though
    // actualPosition is still on the "narrower" side of setpoint.
    analogWrite(OUTPUT_NARROW_3, 90);  // re-seed a nonzero value so Stop() zeroing it is observable
    millisValue(20002);
    impl.Adjust(1, 1);
    assertEqual(analogWriteValue(OUTPUT_NARROW_3), 0);
    assertEqual(analogWriteValue(OUTPUT_WIDE_3), 0);

    // AUnit's own TestRunner reads this same mocked millis() for its internal
    // run-duration/timeout bookkeeping (there's no separate real clock in this
    // native build) -- SHUTOFF_3's real 20000ms is far past AUnit's default
    // per-run timeout, so leaving the fake clock at ~20s here made every test
    // that happened to run after this one get bulk-marked "timed out" before
    // it ever executed. Restore a safe baseline before returning to the runner.
    millisValue(0);
}

// ---------------------------------------------------------------------------
// EEPROM calibration-data validation: values above the 10-bit ADC max
// (1023) or adjacent-equal points cannot have come from a working sensor.
// ---------------------------------------------------------------------------

test(ImplementPlanter, corruptEepromPositionData_fallsBackToDefaults) {
    resetAll();

    // Mark the block "written" the way readCalibrationData() detects it
    // (any of the checked addresses != 255), then store an impossible first
    // position calibration point (0xFFFE) and two in-range-but-equal ones.
    EEPROM.write(86, 0);  // kp -- marks the block as "written"
    EEPROM.write(70, 0xFF); EEPROM.write(71, 0xFE);
    EEPROM.write(72, 0x02); EEPROM.write(73, 0x00);
    EEPROM.write(74, 0x02); EEPROM.write(75, 0x00);

    ImplementPlanter corrupt(nullptr, &mockTractor, &mockGuidance);
    corrupt.SetGpsEnabled(true);
    millisValue(1);
    mockGuidance.SetXte(0);   // stamps lastXteFix=1 -> triggers recompute
    analogReadValue(POSITION_SENS_PIN_3, 300);
    corrupt.Update();
    const int fromCorrupt = corrupt.GetPosition();

    // Erased EEPROM is the known-good "no calibration data" path. Corrupt
    // data has to land on the same defaults rather than on whatever it
    // contained.
    resetAll();
    ImplementPlanter reference(nullptr, &mockTractor, &mockGuidance);
    reference.SetGpsEnabled(true);
    millisValue(1);
    mockGuidance.SetXte(0);   // stamps lastXteFix=1 -> triggers recompute
    analogReadValue(POSITION_SENS_PIN_3, 300);
    reference.Update();

    assertEqual(fromCorrupt, reference.GetPosition());
}

test(ImplementPlanter, offset_defaultsToZero) {
    resetAll();
    ImplementPlanter impl(nullptr, &mockTractor, &mockGuidance);
    assertEqual(impl.GetOffset(), 0);
}
