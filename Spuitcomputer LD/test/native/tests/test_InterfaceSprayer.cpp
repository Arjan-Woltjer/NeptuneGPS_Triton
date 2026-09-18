/*
  test_InterfaceSprayer - Tests for InterfaceSprayer: button debounce, digital-input state, and analog-input smoothing.
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
#include "InterfaceSprayer.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Pin table — maps the button index used throughout this file to the real
// pin number, for driving the shared digitalRead()/millis() mock in
// support/Arduino.h (digitalReadValue()/millisValue()). Every test_*.cpp now
// links into one combined binary (see ../SpuitcomputerLdNativeTests.cpp), so this
// file can no longer supply its own private digitalRead()/analogRead()/
// millis() definitions without hitting a duplicate-symbol link error.
// ---------------------------------------------------------------------------
static const uint8_t kDigitalPins[NUM_DIGITAL_IN] = { IN1, IN2, IN3, IN4 };

// ---------------------------------------------------------------------------
// Helper — reset hardware fakes and the sprayer's button state
// ---------------------------------------------------------------------------
static InterfaceSprayer sprayer;

static void resetAll() {
    millisValue(0);
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) digitalReadValue(kDigitalPins[i], true);  // HIGH = released (pull-up)
    analogReadValue(ANALOG_IN1, 0);

    // Reset internal DigitalInputState (flags true = ready, timers 0, states false)
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        sprayer.buttons[i].flag  = true;
        sprayer.buttons[i].timer = 0;
        sprayer.buttons[i].state = false;
    }
    for (int i = 0; i < NUM_ANALOG_IN; ++i) {
        sprayer.analogInputs[i].value = 0;
    }
}

// ---------------------------------------------------------------------------
// Happy path
// ---------------------------------------------------------------------------

test(InterfaceSprayer, noButtonsPressed_statesAllFalse) {
    resetAll();
    sprayer.CheckDigitalInputs(50);
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        assertFalse(sprayer.buttons[i].state);
    }
}

test(InterfaceSprayer, button1_heldLongEnough_stateTrue) {
    resetAll();
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(100);                          // well past any debounce

    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(InterfaceSprayer, button2_heldLongEnough_stateTrue) {
    resetAll();
    digitalReadValue(kDigitalPins[1], false);  // LOW = pressed
    millisValue(100);

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertTrue(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(InterfaceSprayer, button3_heldLongEnough_stateTrue) {
    resetAll();
    digitalReadValue(kDigitalPins[2], false);  // LOW = pressed
    millisValue(100);

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertTrue(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(InterfaceSprayer, button4_heldLongEnough_stateTrue) {
    resetAll();
    digitalReadValue(kDigitalPins[3], false);  // LOW = pressed
    millisValue(100);

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertTrue(sprayer.buttons[3].state);
}

test(InterfaceSprayer, allButtons_heldLongEnough_allStatesTrue) {
    resetAll();
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) digitalReadValue(kDigitalPins[i], false);  // LOW = pressed
    millisValue(100);

    sprayer.CheckDigitalInputs(50);
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        assertTrue(sprayer.buttons[i].state);
    }
}

// ---------------------------------------------------------------------------
// Debounce edge cases
// ---------------------------------------------------------------------------

test(InterfaceSprayer, button_heldExactlyDelay_isDetected) {
    resetAll();
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(50);                           // exactly at the threshold

    // timer was set to 0 at reset, now - timer = 50 >= 50
    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
}

test(InterfaceSprayer, button_heldJustUnderDelay_notDetected) {
    resetAll();
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(49);                           // 49 < 50, should not fire

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
}

test(InterfaceSprayer, button_releasedBeforeDelay_notDetected) {
    resetAll();
    // Press briefly
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(20);
    sprayer.CheckDigitalInputs(50);

    // Release before debounce expires
    digitalReadValue(kDigitalPins[0], true);  // HIGH = released
    millisValue(30);
    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
}

test(InterfaceSprayer, button_pressedReleasedThenPressedAgain_detectedOnSecondPress) {
    resetAll();

    // First press — too short
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(10);
    sprayer.CheckDigitalInputs(50);

    // Release
    digitalReadValue(kDigitalPins[0], true);  // HIGH = released
    millisValue(20);
    sprayer.CheckDigitalInputs(50);

    // Second press — held long enough
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(80);
    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
}

// ---------------------------------------------------------------------------
// millis() rollover (~49.7 day wraparound)
// ---------------------------------------------------------------------------

test(InterfaceSprayer, millis_rollover_doesNotFalselyBlock) {
    resetAll();
    // Timer recorded just before rollover
    sprayer.buttons[0].flag  = false;
    sprayer.buttons[0].timer = 0xFFFFFFFF - 10;  // 10ms before overflow

    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed
    millisValue(40);   // after rollover; unsigned subtraction: 40 - (2^32-10) = 50

    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
}

// ---------------------------------------------------------------------------
// Multiple simultaneous buttons
// ---------------------------------------------------------------------------

test(InterfaceSprayer, buttons1and3_statesCorrect) {
    resetAll();
    digitalReadValue(kDigitalPins[0], false);  // LOW = pressed (button 1)
    digitalReadValue(kDigitalPins[2], false);  // LOW = pressed (button 3)
    millisValue(100);

    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertTrue(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(InterfaceSprayer, buttons2and4_statesCorrect) {
    resetAll();
    digitalReadValue(kDigitalPins[1], false);  // LOW = pressed (button 2)
    digitalReadValue(kDigitalPins[3], false);  // LOW = pressed (button 4)
    millisValue(100);

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertTrue(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertTrue(sprayer.buttons[3].state);
}

// ---------------------------------------------------------------------------
// Analog input tests
// ---------------------------------------------------------------------------

test(InterfaceSprayer, analogInput_initialValue_isZero) {
    resetAll();
    assertEqual(sprayer.analogInputs[0].value, 0);
}

test(InterfaceSprayer, analogInput_singleRead_halvesWithZeroStart) {
    resetAll();
    analogReadValue(ANALOG_IN1, 100);

    sprayer.CheckAnalogInputs();
    // (0 + 100) / 2 = 50
    assertEqual(sprayer.analogInputs[0].value, 50);
}

test(InterfaceSprayer, analogInput_multipleReads_converge) {
    resetAll();
    analogReadValue(ANALOG_IN1, 1000);

    // After 11 reads: 0→500→750→875→937→968→984→992→996→998→999→999
    for (int i = 0; i < 11; ++i) sprayer.CheckAnalogInputs();
    assertEqual(sprayer.analogInputs[0].value, 999);
}

test(InterfaceSprayer, analogInput_zeroReading_decaysNonZeroStart) {
    resetAll();
    sprayer.analogInputs[0].value = 100;
    analogReadValue(ANALOG_IN1, 0);

    sprayer.CheckAnalogInputs();
    // (100 + 0) / 2 = 50
    assertEqual(sprayer.analogInputs[0].value, 50);
}
