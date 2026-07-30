/*
  test_ImplementSprayer - Tests for ImplementSprayer: cascading output interlocks, dose (l/ha and
  l/min) calculation, and PWM calibration.
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
// Objects under test
//
// digitalRead()/analogRead() aren't exercised here -- every test drives
// iface.buttons[]/analogInputs[] directly -- and millis() is now the shared
// mock in support/Arduino.h (millisValue()), since every test_*.cpp links
// into one combined binary (see ../LoofdoesNativeTests.cpp) and this file can
// no longer supply its own private millis() definition without hitting a
// duplicate-symbol link error.
// ---------------------------------------------------------------------------
static InterfaceSprayer iface;
static VehicleGps       mockGps;
static ImplementSprayer impl(nullptr, &mockGps, &iface);

static void resetAll() {
    millisValue(0);
    mockGps.speed = 0.0f;

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
    iface.analogInputs[0].value = 0;
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
    mockGps.speed = 1.0f;
    // Update() SPEED_AVG_SAMPLES times so the rolling speed average fully
    // converges to mockGps.speed instead of only weighting it 1/SPEED_AVG_SAMPLES.
    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) impl.Update();
    assertNear(impl.doseLM, 1.8f, 0.01f);
}

test(ImplementSprayer, doseLM_zero_when_stationary) {
    // speed=0 → doseLM=0 regardless of dose
    resetAll();
    iface.analogInputs[0].value = 4095;
    mockGps.speed = 0.0f;
    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) impl.Update();
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
    mockGps.speed = 1.0f;
    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) impl.Update();
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
    mockGps.speed = 10.0f;

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
    mockGps.speed = 1.0f;

    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)1843);
}

test(ImplementSprayer, pwm_enabled_upperSegment_output3685) {
    // analog=4095 → doseLHA=200, speed=1.0 → doseLM=200*1*300*60/1000000=3.6 l/min → 3600 ml/min
    // PWM calibration {0,0},{2000,2048},{4000,4095}, segment [2000,4000]:
    // ((3600-2000)/2000)*(4095-2048) + 2048 = 1637.6 + 2048 = 3685.6 → 3685
    resetAll();
    impl.outputs[2].pwm = true;
    iface.analogInputs[0].value = 4095;
    mockGps.speed = 1.0f;

    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) impl.Update();
    assertEqual(impl.outputs[2].value, (unsigned int)3685);
}
