/*
  test_ImplementRooier - Tests for ImplementRooier: per-side height
  interpolation (incl. its divide-by-zero guard), the reconstructed PID
  target computation, the per-side shutoff-latch Adjust(), and the manual
  setpoint nudge.
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
#include "ImplementRooier.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// ImplementRooier's constructor reads calibration data from EEPROM, and most
// of the logic under test here is private state with no reset hook -- so
// every test constructs its own local ImplementRooier after resetAll()
// re-erases the fake EEPROM, guaranteeing the same "no calibration data
// found" defaults every time: positionCalibrationData{L,R}={300,650,1000},
// positionCalibrationPoints={0,50,100}, setpoint=50, offset=0, skew=0,
// error=2, kp=ki=kd=0, manPwm=autoPwm=254 (see ImplementRooier.cpp's
// constructor).
// ---------------------------------------------------------------------------
static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    analogReadValue(HEIGHT_SENS_PIN_L_8, 0);
    analogReadValue(HEIGHT_SENS_PIN_R_8, 0);
}

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

test(ImplementRooier, construct_defaultsMatchErasedEeprom) {
    resetAll();
    ImplementRooier impl;

    assertEqual(impl.GetSetpoint(), 50);
    assertEqual(impl.GetOffset(), 0);
    assertEqual(impl.GetSkew(), 0);
    assertEqual(impl.GetError(), (byte)2);
    assertEqual(impl.GetKP(), (byte)0);
    assertEqual(impl.GetKI(), (byte)0);
    assertEqual(impl.GetKD(), (byte)0);
    assertEqual(impl.GetPwmMan(), (byte)254);
    assertEqual(impl.GetPwmAuto(), (byte)254);
}

// ---------------------------------------------------------------------------
// getActualHeight() (private, exercised through Update()) -- 3-point
// interpolation against the default ascending calibration set.
// ---------------------------------------------------------------------------

test(ImplementRooier, update_midCalibrationReading_interpolatesToMidpoint) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 650);
    analogReadValue(HEIGHT_SENS_PIN_R_8, 650);
    millisValue(25);
    impl.Update(0, 0);

    assertEqual(impl.GetHeightL(), 50);
    assertEqual(impl.GetHeightR(), 50);
}

test(ImplementRooier, update_topCalibrationReading_interpolatesToTop) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 1000);
    millisValue(25);
    impl.Update(0, 0);

    assertEqual(impl.GetHeightL(), 100);
}

// ---------------------------------------------------------------------------
// getActualHeight()'s divide-by-zero guard: two calibration points holding
// the same reading. SetPositionCalibrationDataL() reads a *live*
// analogRead(), matching the ImplementScraper/ImplementPlanter precedent.
// ---------------------------------------------------------------------------

test(ImplementRooier, update_degenerateCalibration_fallsBackToFirstPoint) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 500);
    impl.SetPositionCalibrationDataL(0);
    impl.SetPositionCalibrationDataL(1);

    millisValue(25);
    impl.Update(0, 0);

    assertEqual(impl.GetHeightL(), 0);
}

// ---------------------------------------------------------------------------
// Manual setpoint nudge (AdjustSetpoint(), private, exercised through
// Update()) -- only takes effect in Auto/Hold mode (mode < 2), matching
// ImplementScraper::Update()'s setOffset(buttons) split.
// ---------------------------------------------------------------------------

test(ImplementRooier, update_autoModeButtons_nudgesSetpoint) {
    resetAll();
    ImplementRooier impl;

    impl.Update(0, 1);
    assertEqual(impl.GetSetpoint(), 51);

    impl.Update(0, -1);
    assertEqual(impl.GetSetpoint(), 50);
}

test(ImplementRooier, update_manualModeButtons_leavesSetpointUntouched) {
    resetAll();
    ImplementRooier impl;

    impl.Update(2, 1);
    assertEqual(impl.GetSetpoint(), 50);
}

test(ImplementRooier, update_setpointNudge_clampsToRange) {
    resetAll();
    ImplementRooier impl;

    for (int i = 0; i < 60; i++) {
        impl.Update(0, 1);
    }
    assertEqual(impl.GetSetpoint(), 99);

    for (int i = 0; i < 200; i++) {
        impl.Update(0, -1);
    }
    assertEqual(impl.GetSetpoint(), 1);
}

// ---------------------------------------------------------------------------
// Adjust() -- shutoff-latch bang-bang, Auto mode. kp=ki=kd=0 by default, so
// the PID-computed target equals setpoint+offset (50) exactly, isolating the
// bang-bang decision from the (separately tested) PID math.
// ---------------------------------------------------------------------------

test(ImplementRooier, adjust_autoMode_belowTarget_drivesUp) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 300);  // height=0, target=50
    analogReadValue(HEIGHT_SENS_PIN_R_8, 300);
    millisValue(25);
    impl.Update(0, 0);

    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 254);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 0);
}

test(ImplementRooier, adjust_autoMode_aboveTarget_drivesDown) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 1000);  // height=100, target=50
    analogReadValue(HEIGHT_SENS_PIN_R_8, 1000);
    millisValue(25);
    impl.Update(0, 0);

    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 254);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 0);
}

test(ImplementRooier, adjust_autoMode_atTarget_stops) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 650);  // height=50 == target
    analogReadValue(HEIGHT_SENS_PIN_R_8, 650);
    millisValue(25);
    impl.Update(0, 0);

    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 0);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 0);
}

// ---------------------------------------------------------------------------
// Adjust() -- manual jog (mode >= 2): actual is synthesized from the last
// computed target offset by direction*(error+1), so the sign of direction
// alone selects Up/Down/Stop, matching ImplementScraper's manual branch.
// ---------------------------------------------------------------------------

// actualL = targetL - (direction*(error+1)); with targetL still at its
// constructor default (0, since Update() hasn't run) and error=2, a
// positive direction makes actualL negative -- i.e. below targetL-error --
// which is the "drive Up" branch (matching ImplementScraper's identical
// actualPosition = setpoint - (direction*(error+1)) shape and sign).

test(ImplementRooier, adjust_manualMode_positiveDirection_drivesUp) {
    resetAll();
    ImplementRooier impl;

    impl.Adjust(2, 1);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 254);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 0);
}

test(ImplementRooier, adjust_manualMode_negativeDirection_drivesDown) {
    resetAll();
    ImplementRooier impl;

    impl.Adjust(2, -1);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 254);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 0);
}

test(ImplementRooier, adjust_manualMode_zeroDirection_stops) {
    resetAll();
    ImplementRooier impl;

    impl.Adjust(2, 1);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 254);

    impl.Adjust(2, 0);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 0);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 0);
}

// ---------------------------------------------------------------------------
// Shutoff latch -- once a side has been actively driving one direction for
// longer than SHUTOFF_8 without the reading changing, it latches out of that
// direction until the other direction (or Stop) resets it, matching
// ImplementScraper's/ImplementPlough's shutoff-latch behaviour.
// ---------------------------------------------------------------------------

test(ImplementRooier, adjust_autoMode_longUnchangedDrive_latchesShutoff) {
    resetAll();
    ImplementRooier impl;

    analogReadValue(HEIGHT_SENS_PIN_L_8, 300);  // height=0, target=50 -> drives Up
    analogReadValue(HEIGHT_SENS_PIN_R_8, 300);
    millisValue(25);
    impl.Update(0, 0);

    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 254);

    // SHUTOFF_8 is 20000ms; the reading never changes (still height=0), so
    // the shutoff timer keeps aging. This call crosses the threshold and
    // sets the latch, but still drives Up itself (the latch is checked
    // *after* the drive decision) -- the next call is the first to observe
    // the latched Stop.
    millisValue(20001);
    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 254);

    impl.Adjust(0, 0);
    assertEqual(analogWriteValue(OUTPUT_UP_L_8), 0);
    assertEqual(analogWriteValue(OUTPUT_DOWN_L_8), 0);

    // AUnit's TestRunner reads this same mocked millis() clock for its own
    // run-duration bookkeeping -- leaving it advanced this far would bulk-
    // timeout every test that runs after this one. Reset before returning
    // (same fix already applied in Pootmachinebesturing's/Slangenpomp
    // Spuitcomputer's equivalent tests).
    millisValue(0);
}
