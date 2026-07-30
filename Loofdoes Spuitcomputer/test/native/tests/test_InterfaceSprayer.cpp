#include <AUnit.h>
#include "InterfaceSprayer.h"

using namespace aunit;

// ---------------------------------------------------------------------------
// Fake Arduino HAL — controls what digitalRead() and millis() return
// ---------------------------------------------------------------------------
static bool          pinStates[NUM_DIGITAL_IN]           = { false, false, false, false };
static int           analogValues[NUM_ANALOG_IN]  = { 0 };
static unsigned long fakeMillis = 0;

// Overrides linked in test build
bool digitalRead(uint8_t pin) {
    if (pin == IN1) return pinStates[0];
    if (pin == IN2) return pinStates[1];
    if (pin == IN3) return pinStates[2];
    if (pin == IN4) return pinStates[3];
    return false;
}

int analogRead(uint8_t pin) {
    if (pin == ANALOG_IN1) return analogValues[0];
    return 0;
}

unsigned long millis() { return fakeMillis; }

// ---------------------------------------------------------------------------
// Helper — reset hardware fakes and the sprayer's button state
// ---------------------------------------------------------------------------
static InterfaceSprayer sprayer;

void resetAll() {
    fakeMillis = 0;
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) pinStates[i] = true;  // HIGH = released (pull-up)
    for (int i = 0; i < NUM_ANALOG_IN; ++i) analogValues[i] = 0;

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

test(noButtonsPressed_statesAllFalse) {
    resetAll();
    sprayer.CheckDigitalInputs(50);
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        assertFalse(sprayer.buttons[i].state);
    }
}

test(button1_heldLongEnough_stateTrue) {
    resetAll();
    pinStates[0] = false;  // LOW = pressed
    fakeMillis = 100;                          // well past any debounce

    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(button2_heldLongEnough_stateTrue) {
    resetAll();
    pinStates[1] = false;  // LOW = pressed
    fakeMillis = 100;

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertTrue(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(button3_heldLongEnough_stateTrue) {
    resetAll();
    pinStates[2] = false;  // LOW = pressed
    fakeMillis = 100;

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertTrue(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(button4_heldLongEnough_stateTrue) {
    resetAll();
    pinStates[3] = false;  // LOW = pressed
    fakeMillis = 100;

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertTrue(sprayer.buttons[3].state);
}

test(allButtons_heldLongEnough_allStatesTrue) {
    resetAll();
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) pinStates[i] = false;  // LOW = pressed
    fakeMillis = 100;

    sprayer.CheckDigitalInputs(50);
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        assertTrue(sprayer.buttons[i].state);
    }
}

// ---------------------------------------------------------------------------
// Debounce edge cases
// ---------------------------------------------------------------------------

test(button_heldExactlyDelay_isDetected) {
    resetAll();
    pinStates[0] = false;  // LOW = pressed
    fakeMillis = 50;                           // exactly at the threshold

    // timer was set to 0 at reset, now - timer = 50 >= 50
    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
}

test(button_heldJustUnderDelay_notDetected) {
    resetAll();
    pinStates[0] = false;  // LOW = pressed
    fakeMillis = 49;                           // 49 < 50, should not fire

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
}

test(button_releasedBeforeDelay_notDetected) {
    resetAll();
    // Press briefly
    pinStates[0] = false;  // LOW = pressed
    fakeMillis = 20;
    sprayer.CheckDigitalInputs(50);

    // Release before debounce expires
    pinStates[0] = true;  // HIGH = released
    fakeMillis = 30;
    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
}

test(button_pressedReleasedThenPressedAgain_detectedOnSecondPress) {
    resetAll();

    // First press — too short
    pinStates[0] = false;  // LOW = pressed
    fakeMillis = 10;
    sprayer.CheckDigitalInputs(50);

    // Release
    pinStates[0] = true;  // HIGH = released
    fakeMillis = 20;
    sprayer.CheckDigitalInputs(50);

    // Second press — held long enough
    pinStates[0] = false;  // LOW = pressed
    fakeMillis = 80;
    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
}

// ---------------------------------------------------------------------------
// millis() rollover (~49.7 day wraparound)
// ---------------------------------------------------------------------------

test(millis_rollover_doesNotFalselyBlock) {
    resetAll();
    // Timer recorded just before rollover
    sprayer.buttons[0].flag  = false;
    sprayer.buttons[0].timer = 0xFFFFFFFF - 10;  // 10ms before overflow

    pinStates[0] = false;  // LOW = pressed
    fakeMillis   = 40;   // after rollover; unsigned subtraction: 40 - (2^32-10) = 50

    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
}

// ---------------------------------------------------------------------------
// Multiple simultaneous buttons
// ---------------------------------------------------------------------------

test(buttons1and3_statesCorrect) {
    resetAll();
    pinStates[0] = false;  // LOW = pressed (button 1)
    pinStates[2] = false;  // LOW = pressed (button 3)
    fakeMillis = 100;

    sprayer.CheckDigitalInputs(50);
    assertTrue(sprayer.buttons[0].state);
    assertFalse(sprayer.buttons[1].state);
    assertTrue(sprayer.buttons[2].state);
    assertFalse(sprayer.buttons[3].state);
}

test(buttons2and4_statesCorrect) {
    resetAll();
    pinStates[1] = false;  // LOW = pressed (button 2)
    pinStates[3] = false;  // LOW = pressed (button 4)
    fakeMillis = 100;

    sprayer.CheckDigitalInputs(50);
    assertFalse(sprayer.buttons[0].state);
    assertTrue(sprayer.buttons[1].state);
    assertFalse(sprayer.buttons[2].state);
    assertTrue(sprayer.buttons[3].state);
}

// ---------------------------------------------------------------------------
// Analog input tests
// ---------------------------------------------------------------------------

test(analogInput_initialValue_isZero) {
    resetAll();
    assertEqual(sprayer.analogInputs[0].value, 0);
}

test(analogInput_singleRead_halvesWithZeroStart) {
    resetAll();
    analogValues[0] = 100;

    sprayer.CheckAnalogInputs();
    // (0 + 100) / 2 = 50
    assertEqual(sprayer.analogInputs[0].value, 50);
}

test(analogInput_multipleReads_converge) {
    resetAll();
    analogValues[0] = 1000;

    // After 11 reads: 0→500→750→875→937→968→984→992→996→998→999→999
    for (int i = 0; i < 11; ++i) sprayer.CheckAnalogInputs();
    assertEqual(sprayer.analogInputs[0].value, 999);
}

test(analogInput_zeroReading_decaysNonZeroStart) {
    resetAll();
    sprayer.analogInputs[0].value = 100;
    analogValues[0] = 0;

    sprayer.CheckAnalogInputs();
    // (100 + 0) / 2 = 50
    assertEqual(sprayer.analogInputs[0].value, 50);
}

// ---------------------------------------------------------------------------
// Arduino boilerplate
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial);
}

void loop() {
    TestRunner::run();
}
