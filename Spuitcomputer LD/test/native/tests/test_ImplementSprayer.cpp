/*
  test_ImplementSprayer - Tests for ImplementSprayer: cascading output interlocks, dose (l/ha and
  l/min) calculation, and PWM calibration.
  Copyright (C) 2011-2026 J.A. Woltjer.

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
#include "config/ConfigSprayer.hpp"
#include "implement/ImplementSprayer.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test
//
// digitalRead()/analogRead() aren't exercised here -- every test drives
// iface.buttons[]/analogInputs[] directly -- and millis() is now the shared
// mock in support/Arduino.h (millisValue()), since every test_*.cpp links
// into one combined binary (see ../SpuitcomputerLdNativeTests.cpp) and this file can
// no longer supply its own private millis() definition without hitting a
// duplicate-symbol link error.
// ---------------------------------------------------------------------------
static InterfaceSprayer iface;
static GuidanceSource   mockGps;
static ConfigSprayer    cfg;
static ImplementSprayer impl(nullptr, &mockGps, &iface, &cfg);

// Speed in m/s, as the tests think of it. GuidanceSource stores knots and
// stamps the VTG fix, the way a real receiver's message would -- so a test
// cannot express "moving, but no fix has ever arrived", a state a real
// receiver cannot produce either.
static void gpsSpeed(float speedMs) { mockGps.SetSpeedKnots(speedMs / GPS_MS_PER_KNOT); }

// Fully converge the rolling speed average to speedMs, ending exactly at
// atMs so a caller's own millis() arithmetic right after this call is
// unaffected. updateSpeed() only folds a sample in once GuidanceSource's VTG
// timestamp genuinely changes (real receiver behaviour -- see
// ImplementSprayer::updateSpeed()), so SPEED_AVG_SAMPLES calls at the same
// millis() would fold in one sample and recompute the same average
// SPEED_AVG_SAMPLES times, not converge it: each of the SPEED_AVG_SAMPLES
// fixes here lands one millisecond apart instead, each one a distinct fix.
static void primeSpeed(float speedMs, unsigned long atMs) {
    for (int i = SPEED_AVG_SAMPLES - 1; i >= 0; --i) {
        millisValue(atMs - (unsigned long)i);
        gpsSpeed(speedMs);
        impl.Update();
    }
}

static void resetAll() {
    millisValue(0);
    // Fresh source: no fix ever, speed 0. Quality 1 (plain GPS) so the tests
    // that predate the minimum-quality rule keep dosing.
    mockGps = GuidanceSource();
    mockGps.SetQuality(1);
    cfg = ConfigSprayer();   // back to the defaults every test assumes

    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        iface.buttons[i].state = false;
        iface.buttons[i].flag  = true;
        iface.buttons[i].timer = 0;
    }
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        impl.outputs[i].state = false;
        impl.outputs[i].pwm   = false;
        impl.outputs[i].value = 0;
        impl.outputs[i].timer = 0;
    }
    impl.doseLHA = 0.0f;
    impl.doseLM  = 0.0f;

    // impl is a file-static shared by every test in this file, so the tests that
    // deliberately corrupt the calibration tables would otherwise leak those
    // values into whichever tests AUnit happens to run next.
    impl.doseCalibrationPoints[0] = { 50,  0 };
    impl.doseCalibrationPoints[1] = { 100, 2048 };
    impl.doseCalibrationPoints[2] = { 200, 4095 };

    impl.pwmCalibrationPoints[0] = { 0,    0 };
    impl.pwmCalibrationPoints[1] = { 2000, 2048 };
    impl.pwmCalibrationPoints[2] = { 4000, 4095 };
    impl.numPwmCalibrationPoints = 3;
    iface.analogInputs[0].value = 0;
    impl.actualLHA     = ImplementSprayer::kActualDoseUndefined;
    impl.doseDeviation   = false;
    impl.deviationAccumMs = 0;
    impl.deviationPending = false;
    // A test that fails mid-way may leave calibration held; the next test
    // would then run with the output logic frozen and fail for the wrong reason.
    impl.ReleaseCalibration(CalibrationOwner::Serial);
    impl.ReleaseCalibration(CalibrationOwner::Remote);
    impl.calibrationMode = false;
}

static unsigned long kHoldMinus1() { return ImplementSprayer::kDeviationHoldMs - 1; }

// Advance time in `stepMs` slices up to `untilMs`, feeding a fresh speed
// message every slice like a real receiver would, so the guidance-staleness
// check never trips by accident inside a long scenario.
static void runUntil(unsigned long untilMs, float speedMs, unsigned long stepMs = 100) {
    while (millis() < untilMs) {
        unsigned long next = millis() + stepMs;
        if (next > untilMs) next = untilMs;
        millisValue(next);
        gpsSpeed(speedMs);
        impl.Update();
    }
}

// Bring mixer, vernevelaar and pump up in the interlocked order so the pump
// output is on at t = 2000 ms, moving at `speedMs` with `analog` on the knob.
static void startSpraying(float speedMs, int analog) {
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = analog;
    iface.buttons[0].state = true;
    gpsSpeed(speedMs);
    impl.Update();                          // t=0:    mixer on
    runUntil(1000, speedMs);
    iface.buttons[1].state = true;
    gpsSpeed(speedMs);
    impl.Update();                          // t=1000: vernevelaar on
    runUntil(2000, speedMs);
    iface.buttons[2].state = true;
    gpsSpeed(speedMs);
    impl.Update();                          // t=2000: pump on
}

// ---------------------------------------------------------------------------
// Basic output state tests
// ---------------------------------------------------------------------------

test(ImplementSprayer, noButtons_allOutputsOff) {
    resetAll();
    impl.Update();
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        assertFalse(impl.outputs[i].state);
    }
}

test(ImplementSprayer, button0_output0On) {
    resetAll();
    iface.buttons[0].state = true;

    impl.Update();
    assertTrue(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);
}

test(ImplementSprayer, button0Off_immediately_clears_outputs1and2) {
    resetAll();
    // Get mixer and vernevelaar running: 0 at t=0, 1 at t=1000
    iface.buttons[0].state = true;
    impl.Update();                          // t=0:    output 0 ON

    millisValue(1000);
    iface.buttons[1].state = true;
    impl.Update();                          // t=1000: output 1 ON

    millisValue(2000);
    iface.buttons[2].state = true;
    impl.Update();                          // t=2000: output 2 ON

    assertTrue(impl.outputs[0].state);
    assertTrue(impl.outputs[1].state);
    assertTrue(impl.outputs[2].state);

    // Release button 0 — all three must go off immediately
    iface.buttons[0].state = false;
    impl.Update();

    assertFalse(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);
}

// ---------------------------------------------------------------------------
// Cascading interlock — output 1 requires output 0 on for >= 1000 ms
// ---------------------------------------------------------------------------

test(ImplementSprayer, button1WithoutButton0_output1Off) {
    resetAll();
    iface.buttons[1].state = true;
    impl.Update();
    assertFalse(impl.outputs[1].state);
}

test(ImplementSprayer, buttons0and1_within1s_output1Off) {
    resetAll();
    iface.buttons[0].state = true;
    iface.buttons[1].state = true;

    millisValue(999);
    impl.Update();                          // 999 ms < 1000 ms threshold
    assertTrue(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
}

test(ImplementSprayer, buttons0and1_exactly1s_output1On) {
    resetAll();
    iface.buttons[0].state = true;
    impl.Update();                          // t=0: timer[0] = 0

    millisValue(1000);
    iface.buttons[1].state = true;
    impl.Update();                          // 1000 - 0 >= 1000 → on
    assertTrue(impl.outputs[0].state);
    assertTrue(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);
}

test(ImplementSprayer, button1_output0Off_output1StaysOff) {
    // Mixer was running for >1 s, then stopped — vernevelaar must not turn on
    resetAll();
    iface.buttons[0].state = true;
    impl.Update();                          // t=0: output 0 ON, timer[0]=0

    millisValue(1500);
    iface.buttons[0].state = false;        // mixer released
    iface.buttons[1].state = true;
    impl.Update();                          // output 0 goes off; vernevelaar must stay off
    assertFalse(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
}

// ---------------------------------------------------------------------------
// Cascading interlock — output 2 requires output 1 on for >= 1000 ms
// ---------------------------------------------------------------------------

test(ImplementSprayer, button2WithoutButton1_output2Off) {
    resetAll();
    iface.buttons[2].state = true;
    impl.Update();
    assertFalse(impl.outputs[2].state);
}

test(ImplementSprayer, buttons0and1and2_within1s_ofVernevelaar_output2Off) {
    resetAll();
    iface.buttons[0].state = true;
    impl.Update();                          // t=0: output 0 ON

    millisValue(1000);
    iface.buttons[1].state = true;
    impl.Update();                          // t=1000: output 1 ON, timer[1]=1000

    millisValue(1999);
    iface.buttons[2].state = true;
    impl.Update();                          // 999 ms since output 1 on — too soon
    assertTrue(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);
}

test(ImplementSprayer, fullCascade_after2s_output2On) {
    resetAll();
    iface.buttons[0].state = true;
    impl.Update();                          // t=0:    output 0 ON

    millisValue(1000);
    iface.buttons[1].state = true;
    impl.Update();                          // t=1000: output 1 ON

    millisValue(2000);
    iface.buttons[2].state = true;
    impl.Update();                          // t=2000: output 2 ON
    assertTrue(impl.outputs[0].state);
    assertTrue(impl.outputs[1].state);
    assertTrue(impl.outputs[2].state);
}

test(ImplementSprayer, mixer_toggled_cascade_restarts_from_scratch) {
    resetAll();

    // Step 1: bring full cascade up
    iface.buttons[0].state = true;
    impl.Update();                          // t=0:    output 0 ON, timer[0]=0

    millisValue(1000);
    iface.buttons[1].state = true;
    impl.Update();                          // t=1000: output 1 ON, timer[1]=1000

    millisValue(2000);
    iface.buttons[2].state = true;
    impl.Update();                          // t=2000: output 2 ON
    assertTrue(impl.outputs[2].state);

    // Step 2: release mixer — everything goes off
    millisValue(3000);
    iface.buttons[0].state = false;
    impl.Update();
    assertFalse(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);

    // Step 3: re-press mixer — timer resets; outputs 1 and 2 must NOT come back on yet
    millisValue(3001);
    iface.buttons[0].state = true;
    impl.Update();                          // rising edge: timer[0]=3001
    assertTrue(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);    // cascade delay not elapsed yet
    assertFalse(impl.outputs[2].state);

    // Step 4: after 1 s since mixer re-pressed — vernevelaar may come on
    millisValue(4001);
    impl.Update();                          // 4001-3001=1000 >= 1000 → output 1 ON
    assertTrue(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);    // pump delay not elapsed yet

    // Step 5: after another 1 s — pump may come on
    millisValue(5001);
    impl.Update();                          // 5001-4001=1000 >= 1000 → output 2 ON
    assertTrue(impl.outputs[2].state);
}

// ---------------------------------------------------------------------------
// CalculateDoseLHA — analog value → l/ha interpolation
// Default calibration: {dose=50, analog=0}, {dose=100, analog=2048}, {dose=200, analog=4095}
// ---------------------------------------------------------------------------

test(ImplementSprayer, doseLHA_at_P0) {
    // analogValue=0 → at first calibration point → 50 l/ha
    resetAll();
    iface.analogInputs[0].value = 0;
    impl.Update();
    assertNear(impl.doseLHA, 50.0f, 0.01f);
}

test(ImplementSprayer, doseLHA_midSegment1) {
    // analogValue=1024, midpoint of segment [0,2048]: dose = (1024/2048)*50 + 50 = 75 l/ha
    resetAll();
    iface.analogInputs[0].value = 1024;
    impl.Update();
    assertNear(impl.doseLHA, 75.0f, 0.01f);
}

test(ImplementSprayer, doseLHA_at_P1_boundary) {
    // analogValue=2048 → at second calibration point → 100 l/ha
    resetAll();
    iface.analogInputs[0].value = 2048;
    impl.Update();
    assertNear(impl.doseLHA, 100.0f, 0.01f);
}

test(ImplementSprayer, doseLHA_at_P2) {
    // analogValue=4095 → at third calibration point → 200 l/ha
    resetAll();
    iface.analogInputs[0].value = 4095;
    impl.Update();
    assertNear(impl.doseLHA, 200.0f, 0.01f);
}

test(ImplementSprayer, doseLHA_clampsAbove4095) {
    // analogValue=5000 clamped to 4095 → same result as P2: 200 l/ha
    resetAll();
    iface.analogInputs[0].value = 5000;
    impl.Update();
    assertNear(impl.doseLHA, 200.0f, 0.01f);
}

// ---------------------------------------------------------------------------
// CalculateDoseLM — l/ha × speed × width → l/min
// Formula: doseLM = (doseLHA * speed * width * 60) / (100 * 10000), width=300 cm
// ---------------------------------------------------------------------------

test(ImplementSprayer, doseLM_from_doseLHA_and_speed) {
    // analogValue=2048 → doseLHA=100, speed=1.0 m/s
    // doseLM = 100 * 1.0 * 300 * 60 / 1000000 = 1.8 l/min
    resetAll();
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertNear(impl.doseLM, 1.8f, 0.01f);
}

test(ImplementSprayer, doseLM_zero_when_stationary) {
    // speed=0 → doseLM=0 regardless of dose
    resetAll();
    iface.analogInputs[0].value = 4095;
    primeSpeed(0.0f, 1000);
    assertNear(impl.doseLM, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// End-to-end: analog → doseLHA → doseLM → PWM value
// ---------------------------------------------------------------------------

test(ImplementSprayer, endToEnd_analog1024_speed1_pwm1382) {
    // analogValue=1024 → doseLHA=75 l/ha
    // doseLM = 75 * 1.0 * 300 * 60 / 1000000 = 1.35 l/min → 1350 ml/min
    // PWM calibration {0,0},{2000,2048},{4000,4095}, segment [0,2000]:
    // pwm = (1350/2000)*2048 = 1382.4 → truncated to 1382
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 1024;
    primeSpeed(1.0f, 1000);
    assertNear(impl.doseLHA, 75.0f, 0.01f);
    assertNear(impl.doseLM, 1.35f, 0.01f);
    assertEqual(impl.outputs[2].value, (unsigned int)1382);
}

// ---------------------------------------------------------------------------
// PWM calculation (CalculatePWMValues targets output 2, the pump)
// ---------------------------------------------------------------------------

test(ImplementSprayer, pwm_disabled_valueUnchanged) {
    resetAll();
    impl.outputs[2].pwm = false;
    millisValue(1000);
    gpsSpeed(10.0f);

    impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)0);
}

test(ImplementSprayer, pwm_enabled_lowerSegment_output1843) {
    // analog=2048 → doseLHA=100, speed=1.0 → doseLM=100*1*300*60/1000000=1.8 l/min → 1800 ml/min
    // PWM calibration {0,0},{2000,2048},{4000,4095}, segment [0,2000]:
    // (1800/2000)*2048 = 1843.2 → 1843
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertEqual(impl.outputs[2].value, (unsigned int)1843);
}

test(ImplementSprayer, pwm_enabled_upperSegment_output3685) {
    // analog=4095 → doseLHA=200, speed=1.0 → doseLM=200*1*300*60/1000000=3.6 l/min → 3600 ml/min
    // PWM calibration {0,0},{2000,2048},{4000,4095}, segment [2000,4000]:
    // ((3600-2000)/2000)*(4095-2048) + 2048 = 1637.6 + 2048 = 3685.6 → 3685
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 4095;
    primeSpeed(1.0f, 1000);
    assertEqual(impl.outputs[2].value, (unsigned int)3685);
}

// ---------------------------------------------------------------------------
// Fail-safe behaviour: stale guidance and non-finite dose must stop the pump
// ---------------------------------------------------------------------------

test(ImplementSprayer, staleGuidance_stopsPump) {
    // Establish a real dose first, so a latched value would be visible.
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertMore(impl.outputs[2].value, (unsigned int)0);

    // No further messages. GuidanceSource keeps the last speed forever, so
    // before this fix the pump went on dosing from it indefinitely.
    millisValue(1000 + 2001);
    impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)0);
}

// NeptuneGPS_Triton#61 field test: the ATGM336H kept sending a fresh,
// plausible-looking fix well within the timeout while the antenna was
// actually disconnected -- neither the staleness check above nor the
// quality gate below caught that. Its own antenna-supervisor sentence
// (NmeaParser -> GuidanceSource::SetAntennaOk()) is what does.
test(ImplementSprayer, antennaNotOk_stopsPumpDespiteFreshFix) {
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertMore(impl.outputs[2].value, (unsigned int)0);

    // Antenna reported open; the fix itself keeps arriving on schedule.
    mockGps.SetAntennaOk(false);
    runUntil(1500, 1.0f);
    assertEqual(impl.outputs[2].value, (unsigned int)0);
    assertEqual(impl.actualLHA, ImplementSprayer::kActualDoseUndefined);

    // Antenna reported OK again: dosing resumes on the next fresh fix.
    mockGps.SetAntennaOk(true);
    primeSpeed(1.0f, 1600);
    assertMore(impl.outputs[2].value, (unsigned int)0);
}

test(ImplementSprayer, guidanceJustWithinTimeout_keepsDosing) {
    // The boundary must not be so tight that ordinary message jitter trips it.
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);

    millisValue(1000 + 2000);
    impl.Update();
    assertMore(impl.outputs[2].value, (unsigned int)0);
}

test(ImplementSprayer, noFixSinceBoot_doesNotDose) {
    // lastVtgFix starts at 0, so "never received" must not read as "just now".
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    // A message stamped at millis()=0 leaves lastVtgFix at 0, the very value
    // "never received" carries: the sentinel has to win over the speed it
    // brought. (The stub this test was written against could set the speed
    // without any stamp; the real GuidanceSource cannot, and this is the
    // closest state a real receiver can produce.)
    gpsSpeed(5.0f);
    millisValue(500);

    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)0);
}

test(ImplementSprayer, duplicateDoseCalibrationPoints_doNotProduceNaN) {
    // A seized or disconnected potentiometer makes the capture step record the
    // same reading three times, which put a 0/0 into the interpolation. NaN then
    // passed straight through the low-flow shutoff and both duty clamps.
    resetAll();
    impl.outputs[2].pwm = true;
    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
        impl.doseCalibrationPoints[i].analogValue = 2048;
        impl.doseCalibrationPoints[i].dose        = 100;
    }
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);

    // Self-comparison rather than isfinite(): <math.h> collides with Arduino.h's
    // min/max macros in this build, and NaN is the case that matters here.
    assertTrue(impl.doseLHA == impl.doseLHA);
    assertTrue(impl.doseLM  == impl.doseLM);
    assertLessOrEqual(impl.outputs[2].value, (unsigned int)PWM_MAX_DUTY);
}

test(ImplementSprayer, tooFewPwmCalibrationPoints_stopsPump) {
    // A bad restore from NVS used to leave the pump at its last duty with dose
    // control silently disabled, reapplied every cycle by updateOutputs().
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertMore(impl.outputs[2].value, (unsigned int)0);

    impl.numPwmCalibrationPoints = 1;
    impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)0);
}

// ---------------------------------------------------------------------------
// Actual dose -- what the pump can really deliver after clamping, in l/ha
// (NeptuneGPS_Triton#50). Requested l/ha is the knob; the duty follows speed
// so the l/ha stays constant, and "actual" says whether a duty exists for it.
// ---------------------------------------------------------------------------

test(ImplementSprayer, actual_insideCurve_equalsRequested) {
    // analog=2048 -> 100 l/ha, 1.0 m/s -> 1800 ml/min, inside {0..4000}
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    runUntil(1000, 1.0f);
    assertNear(impl.doseLHA,   100.0f, 0.01f);
    assertNear(impl.actualLHA, 100.0f, 0.01f);
}

test(ImplementSprayer, actual_belowLowestFlow_isZero) {
    // Lowest calibrated flow 500 ml/min; 50 l/ha at 0.5 m/s asks for
    // 50*0.5*300*60/1e6 = 0.45 l/min = 450 ml/min -> pump is cut, actual 0.
    resetAll();
    impl.outputs[2].pwm = true;
    impl.pwmCalibrationPoints[0] = { 500, 1000 };
    iface.analogInputs[0].value = 0;
    runUntil(1000, 0.5f);
    assertEqual(impl.outputs[2].value, (unsigned int)0);
    assertNear(impl.actualLHA, 0.0f, 0.001f);
}

test(ImplementSprayer, actual_aboveTopPoint_reportsSaturatedFlow) {
    // 200 l/ha at 2.0 m/s asks for 7200 ml/min; the curve tops out at 4000
    // ml/min at full duty, which at this speed and width is
    // 4000*1000/(2.0*300*60) = 111.1 l/ha.
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 4095;
    runUntil(1000, 2.0f);
    assertEqual(impl.outputs[2].value, (unsigned int)PWM_MAX_DUTY);
    assertNear(impl.doseLHA,   200.0f, 0.01f);
    assertNear(impl.actualLHA, 111.11f, 0.05f);
}

test(ImplementSprayer, actual_staleGuidance_isUndefined) {
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    runUntil(1000, 1.0f);
    assertNear(impl.actualLHA, 100.0f, 0.01f);

    millisValue(1000 + 2001);   // no message since t=1000
    impl.Update();
    assertEqual(impl.actualLHA, ImplementSprayer::kActualDoseUndefined);
}

test(ImplementSprayer, actual_standingStill_isUndefined) {
    // A fresh fix with zero speed: nothing to dose, so nothing to compare.
    // Must be the sentinel, not 0 -- 0 would read as "pump cut" on the app.
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    runUntil(1000, 0.0f);
    assertEqual(impl.actualLHA, ImplementSprayer::kActualDoseUndefined);
}

test(ImplementSprayer, actual_gpsCreepAtStandstill_isUndefined_noDeviation) {
    // Bench: a receiver with a fix reports 0.5 km/h while the tractor stands
    // still. That must count as standing still, not as "too slow to dose".
    resetAll();
    startSpraying(0.3f, 4095);              // 0.3 m/s = 1.1 km/h, switches on
    runUntil(2000 + ImplementSprayer::kDeviationHoldMs + 500, 0.3f);
    assertEqual(impl.outputs[2].value, (unsigned int)0);
    assertEqual(impl.actualLHA, ImplementSprayer::kActualDoseUndefined);
    assertFalse(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);
}

test(ImplementSprayer, actual_tooFewPwmPoints_isUndefined) {
    resetAll();
    impl.outputs[2].pwm = true;
    impl.numPwmCalibrationPoints = 1;
    iface.analogInputs[0].value = 2048;
    runUntil(1000, 1.0f);
    assertEqual(impl.actualLHA, ImplementSprayer::kActualDoseUndefined);
}

// ---------------------------------------------------------------------------
// Deviation flag and OUT4 buzzer: outside 5 % of requested, only while the
// pump output is on, held for kDeviationHoldMs before setting and clearing.
// ---------------------------------------------------------------------------

test(ImplementSprayer, deviation_setsAfterHold_whileSpraying) {
    resetAll();
    startSpraying(2.0f, 4095);              // saturated: 111 vs 200 l/ha, pump on at t=2000
    assertTrue(impl.outputs[2].state);
    assertFalse(impl.doseDeviation);        // condition true, hold not elapsed

    runUntil(2000 + kHoldMinus1(), 2.0f);
    assertFalse(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);

    runUntil(2000 + ImplementSprayer::kDeviationHoldMs, 2.0f);
    assertTrue(impl.doseDeviation);
    assertTrue(impl.outputs[3].state);      // board buzzer follows the flag
}

test(ImplementSprayer, deviation_buzzerOff_flagSetsButOut4StaysQuiet) {
    // NeptuneGPS_Triton#72: the operator switched the board buzzer off. The
    // deviation is still detected (app alarm, status line), OUT4 stays off.
    resetAll();
    cfg.SetBuzzerEnabled(false);
    startSpraying(2.0f, 4095);
    runUntil(2000 + ImplementSprayer::kDeviationHoldMs, 2.0f);
    assertTrue(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);

    cfg.SetBuzzerEnabled(true);             // switched back on: sounds at once
    runUntil(3200, 2.0f);
    assertTrue(impl.outputs[3].state);
}

test(ImplementSprayer, deviation_lowFlowCutoff_setsWhileSpraying) {
    resetAll();
    impl.pwmCalibrationPoints[0] = { 500, 1000 };
    startSpraying(0.5f, 0);                 // 450 ml/min asked, 500 minimum -> cut
    runUntil(2000 + ImplementSprayer::kDeviationHoldMs, 0.5f);
    assertNear(impl.actualLHA, 0.0f, 0.001f);
    assertTrue(impl.doseDeviation);
    assertTrue(impl.outputs[3].state);
}

test(ImplementSprayer, deviation_notSet_whenPumpOff) {
    // Same saturated demand, but nobody is spraying: no alarm on the way to
    // the field or on the headland.
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 4095;
    runUntil(5000, 2.0f);
    assertNear(impl.actualLHA, 111.11f, 0.05f);
    assertFalse(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);
}

test(ImplementSprayer, deviation_insideCurve_staysClear) {
    resetAll();
    startSpraying(1.0f, 2048);
    runUntil(6000, 1.0f);
    assertTrue(impl.outputs[2].state);
    assertFalse(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);
}

test(ImplementSprayer, deviation_clearsAfterHold) {
    resetAll();
    startSpraying(2.0f, 4095);
    runUntil(3000, 2.0f);
    assertTrue(impl.doseDeviation);

    // Slow down to 1.0 m/s: 200 l/ha now needs 3600 ml/min, inside the curve.
    // The rolling speed average needs SPEED_AVG_SAMPLES updates to settle, so
    // give it a moment before starting the clock on the hold.
    runUntil(3000 + SPEED_AVG_SAMPLES * 100, 1.0f);
    assertNear(impl.actualLHA, 200.0f, 0.01f);
    unsigned long inRangeSince = millis();
    assertTrue(impl.doseDeviation);          // still held

    runUntil(inRangeSince + kHoldMinus1(), 1.0f);
    assertTrue(impl.doseDeviation);
    runUntil(inRangeSince + ImplementSprayer::kDeviationHoldMs, 1.0f);
    assertFalse(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);
}

test(ImplementSprayer, deviation_fivePercentBoundary) {
    // Saturated at 4000 ml/min with 200 l/ha requested. actual =
    // 4000*1000/(v*300*60): 4 % under at v=1.157 m/s, 6 % under at v=1.182.
    resetAll();
    startSpraying(1.157f, 4095);
    runUntil(6000, 1.157f);
    assertTrue(impl.outputs[2].state);
    assertFalse(impl.doseDeviation);

    resetAll();
    startSpraying(1.182f, 4095);
    runUntil(6000, 1.182f);
    assertTrue(impl.doseDeviation);
}

// NeptuneGPS_Triton#61 case 5: releasing any of the three switches drops the
// pump output through the interlock, and the alarm has to go with it on the
// same pass. On the field log the outputs cut instantly but OUT4 kept sounding
// for about a second afterwards, because a cleared deviation still had to
// decay through kDeviationHoldMs. The hold is there to stop the boundary
// chattering while spraying, not to trail a beep onto the headland.
test(ImplementSprayer, deviation_switchReleased_out4StopsOnTheSamePass) {
    resetAll();
    startSpraying(2.0f, 4095);               // saturated -> deviation
    runUntil(3000, 2.0f);
    assertTrue(impl.doseDeviation);
    assertTrue(impl.outputs[2].state);
    assertTrue(impl.outputs[3].state);

    iface.buttons[0].state = false;          // mixer released
    gpsSpeed(2.0f);
    impl.Update();                           // same pass, no time advanced

    assertFalse(impl.outputs[2].state);      // interlock dropped the pump
    assertFalse(impl.doseDeviation);         // and the flag went with it
    assertFalse(impl.outputs[3].state);      // so the buzzer is already quiet
}

test(ImplementSprayer, deviation_clearedInCalibrationMode) {
    resetAll();
    startSpraying(2.0f, 4095);
    runUntil(3000, 2.0f);
    assertTrue(impl.doseDeviation);

    impl.calibrationMode = true;             // wizard owns the outputs now
    runUntil(3000 + ImplementSprayer::kDeviationHoldMs, 2.0f);
    assertFalse(impl.doseDeviation);
    assertFalse(impl.outputs[3].state);
    impl.calibrationMode = false;
}

// ---------------------------------------------------------------------------
// Settings from ConfigSprayer (NeptuneGPS_Triton#49): width, guidance timeout
// and the minimum fix quality all used to be compile-time constants.
// ---------------------------------------------------------------------------

test(ImplementSprayer, width_fromSettings_scalesDoseLM) {
    // 100 l/ha at 1.0 m/s: 1.8 l/min at 300 cm, 3.6 l/min at 600 cm.
    resetAll();
    cfg.SetWidthCm(600);
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertNear(impl.doseLM, 3.6f, 0.01f);
}

test(ImplementSprayer, guidanceTimeout_fromSettings) {
    // 500 ms instead of the old fixed 2000: still dosing at +500, stopped at +501.
    resetAll();
    cfg.SetGuidanceTimeoutMs(500);
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);

    millisValue(1500);
    impl.Update();
    assertMore(impl.outputs[2].value, (unsigned int)0);

    millisValue(1501);
    impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)0);
}

test(ImplementSprayer, minQualityRtk_plainGpsFix_stopsPump) {
    resetAll();
    cfg.SetGpsMinQuality(4);
    mockGps.SetQuality(1);
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertEqual(impl.outputs[2].value, (unsigned int)0);

    // Fix improves to RTK fixed: dosing resumes on the same speed. The
    // rejected quality above drained the average the same way stale
    // guidance does (guidanceStale() covers both), so it needs re-priming
    // here rather than a bare Update() loop, same as after any stale spell.
    mockGps.SetQuality(4);
    primeSpeed(1.0f, 1010);
    assertMore(impl.outputs[2].value, (unsigned int)0);
}

test(ImplementSprayer, minQualityAny_default_dosesWithoutFixQuality) {
    // Default 0 keeps today's behaviour: a speed is a speed, whatever the fix says.
    resetAll();
    mockGps.SetQuality(0);
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 2048;
    primeSpeed(1.0f, 1000);
    assertMore(impl.outputs[2].value, (unsigned int)0);
}

// ---------------------------------------------------------------------------
// Calibration ownership and the firmware-timed pump run (NeptuneGPS_Triton#47):
// shared by the serial wizard and the companion app, ended here regardless of
// what the starter does next.
// ---------------------------------------------------------------------------

test(ImplementSprayer, calibration_singleOwner) {
    resetAll();
    assertTrue(impl.AcquireCalibration(CalibrationOwner::Serial));
    assertTrue(impl.calibrationMode);
    assertTrue(impl.AcquireCalibration(CalibrationOwner::Serial));    // idempotent
    assertFalse(impl.AcquireCalibration(CalibrationOwner::Remote));   // held
    impl.ReleaseCalibration(CalibrationOwner::Remote);                // not yours: no-op
    assertTrue(impl.GetCalibrationOwner() == CalibrationOwner::Serial);
    impl.ReleaseCalibration(CalibrationOwner::Serial);
    assertTrue(impl.GetCalibrationOwner() == CalibrationOwner::None);
    assertFalse(impl.calibrationMode);
    assertTrue(impl.AcquireCalibration(CalibrationOwner::Remote));
    impl.ReleaseCalibration(CalibrationOwner::Remote);
}

test(ImplementSprayer, calibrationRun_requiresOwnerAndValidArgs) {
    resetAll();
    assertFalse(impl.StartCalibrationRun(2000, 5000));    // nobody holds calibration
    assertTrue(impl.AcquireCalibration(CalibrationOwner::Remote));
    assertFalse(impl.StartCalibrationRun(-1, 5000));
    assertFalse(impl.StartCalibrationRun(4096, 5000));
    assertFalse(impl.StartCalibrationRun(2000, 0));
    assertFalse(impl.CalibrationRunActive());
    impl.ReleaseCalibration(CalibrationOwner::Remote);
}

test(ImplementSprayer, calibrationRun_endsItselfAtDuration) {
    resetAll();
    impl.AcquireCalibration(CalibrationOwner::Remote);
    millisValue(1000);
    assertTrue(impl.StartCalibrationRun(2000, 3000));
    assertTrue(impl.CalibrationRunActive());
    assertEqual(impl.GetCalibrationDuty(), 2000);
    assertEqual(impl.CalibrationRunRemainingMs(), (unsigned long)3000);

    millisValue(3999);
    impl.Update();
    assertTrue(impl.CalibrationRunActive());
    assertEqual(impl.CalibrationRunRemainingMs(), (unsigned long)1);

    millisValue(4000);
    impl.Update();
    assertFalse(impl.CalibrationRunActive());
    assertEqual(impl.GetCalibrationDuty(), 0);
    assertEqual(impl.CalibrationRunRemainingMs(), (unsigned long)0);
    impl.ReleaseCalibration(CalibrationOwner::Remote);
}

test(ImplementSprayer, calibrationRun_cappedAtMax) {
    resetAll();
    impl.AcquireCalibration(CalibrationOwner::Remote);
    assertTrue(impl.StartCalibrationRun(2000, 90000));
    assertEqual(impl.CalibrationRunRemainingMs(), ImplementSprayer::kCalibrationRunMaxMs);
    impl.ReleaseCalibration(CalibrationOwner::Remote);
}

test(ImplementSprayer, releaseCalibration_endsRunAndDuty) {
    resetAll();
    impl.AcquireCalibration(CalibrationOwner::Serial);
    impl.StartCalibrationRun(2000, 30000);
    impl.ReleaseCalibration(CalibrationOwner::Serial);
    assertFalse(impl.CalibrationRunActive());
    assertEqual(impl.GetCalibrationDuty(), 0);
}

// ---------------------------------------------------------------------------
// Taking calibration switches the mixer and vernevelaar off (NeptuneGPS_Triton
// #66): calibration is about the pump alone, and on the bench the two stayed
// on for five one-minute runs because the output logic was merely frozen.
// ---------------------------------------------------------------------------

test(ImplementSprayer, acquireCalibration_switchesMixerAndVernevelaarOff) {
    resetAll();
    startSpraying(1.0f, 2048);              // full cascade on at t=2000
    assertTrue(impl.outputs[0].state);
    assertTrue(impl.outputs[1].state);
    assertTrue(impl.outputs[2].state);

    assertTrue(impl.AcquireCalibration(CalibrationOwner::Remote));
    assertFalse(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);
    assertEqual(impl.GetCalibrationDuty(), 0);

    // Still off while calibration is held, whatever the switches say.
    runUntil(3000, 1.0f);
    assertFalse(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
    impl.ReleaseCalibration(CalibrationOwner::Remote);
}

test(ImplementSprayer, releaseCalibration_cascadeRestartsWithItsDelays) {
    resetAll();
    startSpraying(1.0f, 2048);
    impl.AcquireCalibration(CalibrationOwner::Remote);
    runUntil(3000, 1.0f);
    impl.ReleaseCalibration(CalibrationOwner::Remote);

    // Switches are still held: mixer at once, vernevelaar 1 s later, pump 1 s after that.
    millisValue(3100); gpsSpeed(1.0f); impl.Update();
    assertTrue(impl.outputs[0].state);
    assertFalse(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);

    runUntil(4099, 1.0f);
    assertFalse(impl.outputs[1].state);
    runUntil(4100, 1.0f);
    assertTrue(impl.outputs[1].state);
    assertFalse(impl.outputs[2].state);

    runUntil(5099, 1.0f);
    assertFalse(impl.outputs[2].state);
    runUntil(5100, 1.0f);
    assertTrue(impl.outputs[2].state);
}

// ---------------------------------------------------------------------------
// calculatePWMValues() branches the earlier scenarios never reached
// (NeptuneGPS_Triton#87): standing still with a fresh fix, and a pump curve
// with more than three points so the segment search walks past the first.
// ---------------------------------------------------------------------------

test(ImplementSprayer, standstill_withFreshFix_pumpOffAndActualUndefined) {
    resetAll();
    startSpraying(0.2f, 1024);              // below kStandstillSpeedMs, fix fresh
    assertTrue(impl.outputs[2].state);      // the cascade did switch the pump on
    assertEqual(impl.outputs[2].value, (unsigned int)0);  // but standing still it does not run
    assertTrue(impl.actualLHA == ImplementSprayer::kActualDoseUndefined);
}

test(ImplementSprayer, fivePointPumpCurve_upperSegmentInterpolates) {
    resetAll();
    impl.pwmCalibrationPoints[0] = { 0,    0 };
    impl.pwmCalibrationPoints[1] = { 500,  1000 };
    impl.pwmCalibrationPoints[2] = { 1000, 2000 };
    impl.pwmCalibrationPoints[3] = { 2000, 3000 };
    impl.pwmCalibrationPoints[4] = { 4000, 4095 };
    impl.numPwmCalibrationPoints = 5;
    startSpraying(2.0f, 4095);              // top dose, brisk speed: high flow demand
    assertTrue(impl.outputs[2].value > 2000u);
    assertTrue(impl.outputs[2].value <= 4095u);
    assertTrue(impl.actualLHA > 0.0f);
}
