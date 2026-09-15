/*
  test_ImplementKipper - Tests for ImplementKipper: the angle/steer 3-point
  calibration interpolation (incl. its divide-by-zero guard), the P/I/D
  SetSetpoint() computation (incl. the first-call histCount-wrap fix), and
  Adjust()'s shutoff-latch bang-bang.
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
#include "ImplementKipper.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// ImplementKipper's constructor reads calibration data from EEPROM, and most
// of the logic under test here is private state with no reset hook -- so
// every test constructs its own local ImplementKipper after resetAll()
// re-erases the fake EEPROM, guaranteeing the same "no calibration data
// found" defaults every time: steerCalibrationData=angleCalibrationData=
// {201,428,687}, steerCalibrationPoints={-30,0,30},
// angleCalibrationPoints={-45,0,45}, kp=50, ki=50, kd=0, offset=0 (see
// ImplementKipper.cpp's constructor).
// ---------------------------------------------------------------------------
static VehicleTractor mockTractor;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    analogReadValue(ANGLE_SENS_PIN_4, 428);  // interpolates to 0
    analogReadValue(STEER_SENS_PIN_4, 428);  // interpolates to 0
    mockTractor.speedKmh = 0.0f;
}

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

test(ImplementKipper, construct_defaultsMatchErasedEeprom) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    assertEqual(impl.GetKP(), (byte)50);
    assertEqual(impl.GetKI(), (byte)50);
    assertEqual(impl.GetKD(), (byte)0);
    assertEqual(impl.GetOffset(), 0);
}

// ---------------------------------------------------------------------------
// getActualAngle()/getActualSteer() (private, exercised through Update()) --
// 3-point interpolation against the default calibration set.
// ---------------------------------------------------------------------------

test(ImplementKipper, update_midCalibrationReading_interpolatesToZero) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    millisValue(25);
    impl.Update(0);

    assertEqual(impl.GetAngle(), 0);
    assertEqual(impl.GetSteer(), 0);
}

test(ImplementKipper, update_topCalibrationReading_interpolatesToTop) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    analogReadValue(ANGLE_SENS_PIN_4, 687);
    analogReadValue(STEER_SENS_PIN_4, 687);
    millisValue(25);
    impl.Update(0);

    assertEqual(impl.GetAngle(), 45);
    assertEqual(impl.GetSteer(), 30);
}

test(ImplementKipper, update_bottomCalibrationReading_interpolatesToBottom) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    analogReadValue(ANGLE_SENS_PIN_4, 201);
    analogReadValue(STEER_SENS_PIN_4, 201);
    millisValue(25);
    impl.Update(0);

    assertEqual(impl.GetAngle(), -45);
    assertEqual(impl.GetSteer(), -30);
}

// ---------------------------------------------------------------------------
// getActualAngle()/getActualSteer()'s divide-by-zero guard: two calibration
// points holding the same reading. SetAngleCalibrationData()/
// SetSteerCalibrationData() read a *live* analogRead(), matching the
// ImplementScraper/ImplementPlanter/ImplementRooier precedent.
// ---------------------------------------------------------------------------

test(ImplementKipper, update_degenerateAngleCalibration_fallsBackToFirstPoint) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    analogReadValue(ANGLE_SENS_PIN_4, 300);
    impl.SetAngleCalibrationData(0);
    impl.SetAngleCalibrationData(1);

    millisValue(25);
    impl.Update(0);

    assertEqual(impl.GetAngle(), -45);
}

// ---------------------------------------------------------------------------
// SetSetpoint() (private, exercised through Update()) -- P/I/D with the
// default kp=50/ki=50/kd=0. angleCor = angle + offset = 45 on the first
// (histCount==0) call: P = 45*50/100 = 22.5; angleAvg = angleSum/25 =
// 45/25 = 1 (int division) -> I = 1*50/100 = 0.5; D stays 0 (dFactor is 0
// on the very first call, and kd=0 regardless). setpoint = int(22.5+0.5) = 23.
// ---------------------------------------------------------------------------

test(ImplementKipper, update_firstCall_computesSetpointFromPID) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    analogReadValue(ANGLE_SENS_PIN_4, 687);  // angle=45
    millisValue(25);
    impl.Update(0);

    assertEqual(impl.GetSetpoint(), 23);
}

// ---------------------------------------------------------------------------
// Adjust() -- shutoff-latch bang-bang, Auto mode (mode==0 -> actualSteer is
// the live sensor reading).
// ---------------------------------------------------------------------------

test(ImplementKipper, adjust_autoMode_belowSetpoint_drivesNarrow) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    analogReadValue(ANGLE_SENS_PIN_4, 687);  // angle=45 -> setpoint=23
    analogReadValue(STEER_SENS_PIN_4, 428);  // steer=0 -> below setpoint
    millisValue(25);
    impl.Update(0);

    impl.Adjust(0);
    assertTrue(digitalWriteValue(OUTPUT_NARROW_4));
    assertFalse(digitalWriteValue(OUTPUT_WIDE_4));
}

test(ImplementKipper, adjust_autoMode_aboveSetpoint_drivesWide) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    analogReadValue(ANGLE_SENS_PIN_4, 201);  // angle=-45 -> setpoint negative
    analogReadValue(STEER_SENS_PIN_4, 687);  // steer=30 -> above setpoint
    millisValue(25);
    impl.Update(0);

    impl.Adjust(0);
    assertTrue(digitalWriteValue(OUTPUT_WIDE_4));
    assertFalse(digitalWriteValue(OUTPUT_NARROW_4));
}

// ---------------------------------------------------------------------------
// Adjust() -- Manual/Hold/Calibration jog (mode != 0): actualSteer =
// setpoint - direction, so direction 0 always reaches the setpoint exactly
// (the "reached" branch), regardless of what setpoint currently is.
// ---------------------------------------------------------------------------

test(ImplementKipper, adjust_manualMode_zeroDirection_stops) {
    resetAll();
    ImplementKipper impl(&mockTractor);

    millisValue(25);
    impl.Update(2);  // mode=2 (Manual) -- also seeds setpoint via SetSetpoint()

    impl.Adjust(0);
    assertFalse(digitalWriteValue(OUTPUT_WIDE_4));
    assertFalse(digitalWriteValue(OUTPUT_NARROW_4));
}

// ---------------------------------------------------------------------------
// EEPROM persistence (NeptuneGPS_Triton#92): the write/read round trip, the
// offset window and its known sign loss (#100), what the wipe takes with it,
// and the serial dump.
// ---------------------------------------------------------------------------

test(ImplementKipper, resetCalibration_erasedEeprom_reportsNoData) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    assertFalse(impl.ResetCalibration());
    assertEqual((int)impl.GetKP(), 50);
    assertEqual((int)impl.GetKI(), 50);
    assertEqual((int)impl.GetKD(), 0);
    assertEqual(impl.GetOffset(), 0);
}

test(ImplementKipper, commitThenFreshInstance_roundTripsEveryField) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    analogReadValue(STEER_SENS_PIN_4, 100); impl.SetSteerCalibrationData(0);
    analogReadValue(STEER_SENS_PIN_4, 500); impl.SetSteerCalibrationData(1);
    analogReadValue(STEER_SENS_PIN_4, 900); impl.SetSteerCalibrationData(2);
    analogReadValue(ANGLE_SENS_PIN_4, 150); impl.SetAngleCalibrationData(0);
    analogReadValue(ANGLE_SENS_PIN_4, 450); impl.SetAngleCalibrationData(1);
    analogReadValue(ANGLE_SENS_PIN_4, 750); impl.SetAngleCalibrationData(2);
    impl.SetKP(12); impl.SetKI(3); impl.SetKD(4);
    impl.SetOffset(7);
    impl.CommitCalibration();

    ImplementKipper fresh(&mockTractor);
    assertTrue(fresh.ResetCalibration());
    assertEqual((int)fresh.GetKP(), 12);
    assertEqual((int)fresh.GetKI(), 3);
    assertEqual((int)fresh.GetKD(), 4);
    assertEqual(fresh.GetOffset(), 7);

    // The persisted curves are what the interpolation runs on: a reading at
    // the stored top point lands on the top calibration point.
    analogReadValue(STEER_SENS_PIN_4, 900);
    analogReadValue(ANGLE_SENS_PIN_4, 750);
    millisValue(25);
    fresh.Update(0);
    assertEqual(fresh.GetSteer(), fresh.GetSteerCalibrationPoint(2));
    assertEqual(fresh.GetAngle(), fresh.GetAngleCalibrationPoint(2));
}

test(ImplementKipper, storedOffsetOutOfRange_fallsBackToZero) {
    resetAll();
    ImplementKipper seed(&mockTractor);
    seed.SetOffset(5);
    seed.CommitCalibration();
    // Bytes in the order writeInt() uses: low at the address, high after it.
    EEPROM.write(180, 100); EEPROM.write(181, 0x00);   // +100, beyond the +-20 window
    ImplementKipper impl(&mockTractor);
    assertEqual(impl.GetOffset(), 0);
    EEPROM.write(180, 20); EEPROM.write(181, 0x00);    // +20, the edge, kept
    ImplementKipper edge(&mockTractor);
    assertEqual(edge.GetOffset(), 20);
}

// A negative offset survives a reboot (NeptuneGPS_Triton#100): it is stored
// and rebuilt with the signed writeInt()/readInt() helpers, as in
// ImplementPlough. The unsigned word() used before turned a stored -7 into
// 65529, which failed the +-20 window and reset the offset to 0 every boot.
test(ImplementKipper, storedOffset_negative_survivesAReboot) {
    resetAll();
    ImplementKipper seed(&mockTractor);
    seed.SetOffset(-7);
    seed.CommitCalibration();
    // writeInt() stores the two bytes in host order, low byte first.
    assertEqual((int)EEPROM.read(180), 0xF9);
    assertEqual((int)EEPROM.read(181), 0xFF);
    ImplementKipper impl(&mockTractor);
    assertEqual(impl.GetOffset(), -7);
}

// CommitCalibration() wipes bytes 1..254 before writing its own block, so
// anything else stored on the board (the tractor block at 20..28) goes with
// it. Pinned so a change to it is deliberate.
test(ImplementKipper, commitCalibration_wipesEverythingOutsideItsOwnBlock) {
    resetAll();
    EEPROM.write(20, 42);
    ImplementKipper impl(&mockTractor);
    impl.CommitCalibration();
    assertEqual((int)EEPROM.read(20), 255);
    assertNotEqual((int)EEPROM.read(120), 255);   // its own block is back
}

test(ImplementKipper, printCalibrationData_runs) {
    resetAll();
    ImplementKipper impl(&mockTractor);
    impl.PrintCalibrationData();   // writes to Serial (stdout in the stub)
    assertEqual((int)impl.GetKP(), 50);
}
