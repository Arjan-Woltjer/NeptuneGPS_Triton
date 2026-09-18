/*
  test_ImplementSprayer - Tests for ImplementSprayer: gear/flow pulse counting,
  dose set/clamp, the auto-mode PWM setpoint pipeline (calculateSetpointFlow ->
  calculateSetpointPwm, including the interpolation divide-by-zero guard's
  effect end-to-end), and EEPROM calibration-data validation.
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
#include "ImplementSprayer.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// ImplementSprayer's constructor reads calibration data from EEPROM (via
// readCalibrationData()/readDose()), and most of the logic under test here is
// private state with no reset hook -- so every test constructs its own local
// ImplementSprayer after resetAll() re-erases the fake EEPROM, guaranteeing
// the same "no calibration data found" defaults every time:
// pwmCalibrationData={1,47,65,79,90,129,150,170,187,192,196,203},
// pwmCalibrationPoints={45,50,55,60,65,70,75,80,85,90,95,100} (TEENSY board),
// flowCalibration=940, teeth=20, pumps=8, width=30, kp=60, ki=4, kd=10,
// dose=200 (readDose()'s own "no dose stored" default), histCount=histSize=5
// (see ImplementSprayer.cpp's constructor).
// ---------------------------------------------------------------------------
static VehicleTractor mockTractor;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    mockTractor.speed = 0;
    mockTractor.hitch = false;
    digitalReadValue(GEAR_SENS_PIN, false);
    digitalReadValue(FLOW_SENS_PIN, false);
}

// ---------------------------------------------------------------------------
// Pulse counting -- runs unconditionally every Update() call, before the
// once-per-second calculation block, so two calls within the same second are
// enough to observe it without triggering setDose()/calculateSetpointFlow().
// ---------------------------------------------------------------------------

test(ImplementSprayer, update_gearAndFlowPulses_countOnEachTransition) {
    resetAll();
    ImplementSprayer impl(&mockTractor);

    impl.Update(2, 0);  // mode=2 (Off) so setDose()'s correction path doesn't fire either
    assertEqual(impl.GetGearPulses(), 0);
    assertEqual(impl.GetFlowPulses(), 0);

    digitalReadValue(GEAR_SENS_PIN, true);
    digitalReadValue(FLOW_SENS_PIN, true);
    impl.Update(2, 0);
    assertEqual(impl.GetGearPulses(), 1);
    assertEqual(impl.GetFlowPulses(), 1);

    digitalReadValue(GEAR_SENS_PIN, false);
    digitalReadValue(FLOW_SENS_PIN, false);
    impl.Update(2, 0);
    assertEqual(impl.GetGearPulses(), 2);
    assertEqual(impl.GetFlowPulses(), 2);
}

// ---------------------------------------------------------------------------
// GetActualDose() -- guarded at 0.1 m/s in ImplementSprayer.hpp.
// ---------------------------------------------------------------------------

test(ImplementSprayer, getActualDose_belowMinSpeed_returnsZero) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    mockTractor.speed = 0.05f;
    assertEqual(impl.GetActualDose(), 0);
}

// ---------------------------------------------------------------------------
// setDose()/readDose() -- dose defaults to 200 (l/ha) with no stored value,
// and is clamped to [50, 500]. Only runs inside Update()'s once-per-second
// block, so each step below advances millis() by >=1000ms first.
// ---------------------------------------------------------------------------

test(ImplementSprayer, dose_defaultsTo200) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    assertEqual(impl.GetDose(), 200);
}

test(ImplementSprayer, setDose_repeatedIncrease_clampsAt500) {
    resetAll();
    ImplementSprayer impl(&mockTractor);

    unsigned long t = 0;
    for (int i = 0; i < 40; i++) {
        t += 1000;
        millisValue(t);
        impl.Update(2, 1);  // mode=2 (Off) -> setDose() still runs, Adjust doesn't
    }
    assertEqual(impl.GetDose(), 500);

    // AUnit's own TestRunner reads this same mocked millis() for its internal
    // run-duration/timeout bookkeeping (there's no separate real clock in
    // this native build) -- 40 seconds of simulated time is far past AUnit's
    // default per-run timeout, so leaving the fake clock there made every
    // test that happened to run after this one get bulk-marked "timed out"
    // before it ever executed. Restore a safe baseline before returning.
    millisValue(0);
}

test(ImplementSprayer, setDose_repeatedDecrease_clampsAt50) {
    resetAll();
    ImplementSprayer impl(&mockTractor);

    unsigned long t = 0;
    for (int i = 0; i < 40; i++) {
        t += 1000;
        millisValue(t);
        impl.Update(2, -1);
    }
    assertEqual(impl.GetDose(), 50);

    // Same AUnit-watchdog trap as setDose_repeatedIncrease_clampsAt500 above.
    millisValue(0);
}

test(ImplementSprayer, setDose_zeroCorrection_leavesDoseUnchanged) {
    resetAll();
    ImplementSprayer impl(&mockTractor);

    millisValue(1000);
    impl.Update(2, 0);
    assertEqual(impl.GetDose(), 200);
}

// ---------------------------------------------------------------------------
// Auto-mode PWM setpoint pipeline: calculateSetpointFlow() ->
// calculateSetpointPwm() -> analogWrite(OUTPUT_FET, setpointPwm), only runs
// when mode is 0 (Auto) or 4 (Sim) and not held.
// ---------------------------------------------------------------------------

test(ImplementSprayer, update_autoMode_zeroSpeed_computesDeterministicPwm) {
    resetAll();
    ImplementSprayer impl(&mockTractor);

    millisValue(1000);
    impl.Update(0, 0);  // mode=0 (Auto), zero tractor speed -> neededFlow=0

    // neededFlow=0, actualFlow=0 (no flow pulses) -> setpointFlow=0 ->
    // calculateSetpointPwm() interpolates readRaw=0 against the default
    // pwmCalibrationData/-Points' first segment (data={1,47,...},
    // points={45,50,...}): a=0-1=-1, b=47-1=46, c=50-45=5, d=45 ->
    // 45 + (-1*5)/46 = 45 - 0.1087 = 44 (truncated).
    assertEqual(analogWriteValue(OUTPUT_FET), 44);
}

test(ImplementSprayer, update_holdMode_stopsOutputs) {
    resetAll();
    ImplementSprayer impl(&mockTractor);

    millisValue(1000);
    impl.Update(0, 0);
    assertMore(analogWriteValue(OUTPUT_FET), 0);

    millisValue(2000);
    impl.Update(1, 0);  // mode=1 (Start/hold) -> Stop()
    assertEqual(analogWriteValue(OUTPUT_FET), 0);
    assertEqual(analogWriteValue(OUTPUT_FET2), 0);
}

// ---------------------------------------------------------------------------
// EEPROM calibration-data validation: calibratePump() only ever writes a
// strictly increasing 12-point table (each point clamped to be greater than
// the previous one), so a stored table that isn't strictly increasing cannot
// have come from a real calibration run.
// ---------------------------------------------------------------------------

test(ImplementSprayer, corruptEepromPwmData_fallsBackToDefaults) {
    resetAll();

    // Mark the block "written" the way readCalibrationData() detects it
    // (any of the checked addresses != 255), then store a non-increasing
    // pwm calibration table (all 12 points identical).
    EEPROM.write(160, 20);  // teeth -- marks the block as "written"
    for (int i = 0; i < 12; i++) {
        EEPROM.write(i * 2 + 100, 0);
        EEPROM.write(i * 2 + 101, 50);  // every point decodes to 50
    }

    ImplementSprayer corrupt(&mockTractor);
    millisValue(1000);
    corrupt.Update(0, 0);
    const int fromCorrupt = analogWriteValue(OUTPUT_FET);

    // Erased EEPROM is the known-good "no calibration data" path -- corrupt
    // data has to land on the same defaults rather than on whatever it
    // contained (which would otherwise feed a same-value pair straight into
    // calculateSetpointPwm()'s interpolation divisor).
    resetAll();
    ImplementSprayer reference(&mockTractor);
    millisValue(1000);
    reference.Update(0, 0);
    const int fromReference = analogWriteValue(OUTPUT_FET);

    assertEqual(fromCorrupt, fromReference);
}

// ---------------------------------------------------------------------------
// EEPROM persistence and the paths a moving sprayer takes (NeptuneGPS_Triton#94):
// the write/read round trip, the stored dose and its clamps, the actual-dose
// and volume readouts, and setpoint flow/PWM with speed on the wheel.
// CalibratePump() spins on millis() for a minute per point and stays out;
// calculateAlarm() is deliberately not called from Update() (see there).
// ---------------------------------------------------------------------------

test(ImplementSprayer, resetCalibration_erasedEeprom_reportsNoData) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    assertFalse(impl.ResetCalibration());
    assertEqual(impl.GetFlowCalibration(), 940);
    assertEqual(impl.GetDose(), 200);
}

test(ImplementSprayer, commitThenFreshInstance_roundTripsEveryField) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    impl.SetTeeth(7);
    impl.SetPumps(2);
    impl.SetWidth(30);
    impl.SetKP(12); impl.SetKI(3); impl.SetKD(4);
    impl.SetFlowCalibration(1234);
    impl.CommitCalibration();                 // writes, then dumps to Serial
    assertNotEqual((int)EEPROM.read(100), 255);

    ImplementSprayer fresh(&mockTractor);
    assertTrue(fresh.ResetCalibration());    // the default PWM table round-trips as a valid one
    assertEqual((int)fresh.GetTeeth(), 7);
    assertEqual((int)fresh.GetPumps(), 2);
    assertEqual((int)fresh.GetWidth(), 30);
    assertEqual((int)fresh.GetKP(), 12);
    assertEqual((int)fresh.GetKI(), 3);
    assertEqual((int)fresh.GetKD(), 4);
    assertEqual(fresh.GetFlowCalibration(), 1234);
}

test(ImplementSprayer, storedDose_isReadAndClampedTo50to500) {
    resetAll();
    EEPROM.write(190, 0x01); EEPROM.write(191, 0x2C);   // 300
    ImplementSprayer a(&mockTractor);
    assertEqual(a.GetDose(), 300);
    EEPROM.write(190, 0x02); EEPROM.write(191, 0xBC);   // 700 -> 500
    ImplementSprayer b(&mockTractor);
    assertEqual(b.GetDose(), 500);
    EEPROM.write(190, 0x00); EEPROM.write(191, 10);     // 10 -> 50
    ImplementSprayer c(&mockTractor);
    assertEqual(c.GetDose(), 50);
}

test(ImplementSprayer, movingInAuto_producesABoundedPumpDutyAndReadouts) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    mockTractor.speed = 2.0f;                 // m/s
    digitalReadValue(IMPLEMENT_SWITCH, true);
    for (int s = 1; s <= 4; ++s) {
        millisValue(1000UL * s);
        impl.Update(0, 0);
        // A flow pulse each cycle so the actual flow is not stuck at zero.
        digitalReadValue(FLOW_SENS_PIN, (s % 2) == 1);
        digitalReadValue(GEAR_SENS_PIN, (s % 2) == 1);
        impl.Update(0, 0);
    }
    assertMoreOrEqual(analogWriteValue(OUTPUT_FET), 0);
    assertLessOrEqual(analogWriteValue(OUTPUT_FET), 255);
    assertTrue(impl.GetNeededFlow() > 0.0f);
    (void)impl.GetActualDose();               // the moving branch of the readout
    (void)impl.GetActualFlow();
    (void)impl.GetFlag();
    assertMoreOrEqual((long)impl.GetVolume(), 0L);
    digitalReadValue(IMPLEMENT_SWITCH, false);
    digitalReadValue(FLOW_SENS_PIN, false);
    digitalReadValue(GEAR_SENS_PIN, false);
    millisValue(0);
}

test(ImplementSprayer, movingInSim_takesTheSimulatedSpeedPath) {
    resetAll();
    ImplementSprayer impl(&mockTractor);
    mockTractor.speed = 1.5f;
    digitalReadValue(IMPLEMENT_SWITCH, true);
    millisValue(1000);
    impl.Update(4, 0);
    millisValue(2000);
    impl.Update(4, 1);                        // a dose nudge while simulating
    millisValue(3000);
    impl.Update(4, -1);
    assertMoreOrEqual(analogWriteValue(OUTPUT_FET), 0);
    assertLessOrEqual(analogWriteValue(OUTPUT_FET), 255);
    digitalReadValue(IMPLEMENT_SWITCH, false);
    millisValue(0);
}
