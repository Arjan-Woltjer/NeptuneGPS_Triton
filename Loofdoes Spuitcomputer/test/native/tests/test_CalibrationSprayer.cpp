/*
  test_CalibrationSprayer - Tests for the serial calibration wizard: menu,
  analog and PWM calibration flows, PWM point editing, telemetry toggles and
  the line editor, driven through a scripted Stream (NeptuneGPS_Triton#87).
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max
// as macros (see test_RemoteSprayer.cpp).
#include <string>

#include <AUnit.h>
#include "CalibrationSprayer.hpp"
#include "ConfigSprayer.hpp"
#include "ImplementSprayer.hpp"
#include "SerialGuidanceChannel.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// The operator's terminal: bytes the test types go in, everything the wizard
// prints is captured for assertions. No stub for the wizard, the implement or
// the guidance channel -- the state machine under test is the real one.
// ---------------------------------------------------------------------------

struct ScriptedSerial : public Stream {
    std::string in;
    size_t      pos = 0;
    std::string out;
    void Type(const char* s) { in += s; }
    int  available() override { return int(in.size() - pos); }
    int  read() override      { return pos < in.size() ? (uint8_t)in[pos++] : -1; }
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
    int  count(const char* s) const {
        int n = 0;
        for (size_t p = out.find(s); p != std::string::npos; p = out.find(s, p + 1)) n++;
        return n;
    }
    void clearOut() { out.clear(); }
};

struct SilentGpsSerial : public HardwareSerial {
    int available() override { return 0; }
    int read() override      { return -1; }
};

static InterfaceSprayer      cIface;
static GuidanceSource        cGps;
static SilentGpsSerial       cGpsSerial;
static SerialGuidanceChannel cChannel(nullptr, &cGpsSerial, &cGps);
static ConfigSprayer         cCfg;
static ImplementSprayer      cImpl(nullptr, &cGps, &cIface, &cCfg);
static ScriptedSerial        term;

static int forgetCalls = 0;
static void countForget() { forgetCalls++; }

static void knob(int analog) { cIface.analogInputs[0].value = analog; }

static void cReset() {
    millisValue(0);
    cGps = GuidanceSource();
    cGps.SetQuality(1);
    knob(0);
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) { cIface.buttons[i].state = false; cIface.buttons[i].flag = true; cIface.buttons[i].timer = 0; }
    for (int i = 0; i < NUM_OUTPUTS; ++i) { cImpl.outputs[i].state = false; cImpl.outputs[i].pwm = false; cImpl.outputs[i].value = 0; cImpl.outputs[i].timer = 0; }
    cImpl.doseCalibrationPoints[0] = { 50,  0 };
    cImpl.doseCalibrationPoints[1] = { 100, 2048 };
    cImpl.doseCalibrationPoints[2] = { 200, 4095 };
    cImpl.pwmCalibrationPoints[0] = { 0,    0 };
    cImpl.pwmCalibrationPoints[1] = { 2000, 2048 };
    cImpl.pwmCalibrationPoints[2] = { 4000, 4095 };
    cImpl.numPwmCalibrationPoints = 3;
    cImpl.doseLHA = 0.0f; cImpl.doseLM = 0.0f;
    cImpl.actualLHA = ImplementSprayer::kActualDoseUndefined;
    cImpl.doseDeviation = false;
    cCfg = ConfigSprayer();
    cImpl.StopCalibrationRun();
    cImpl.ReleaseCalibration(CalibrationOwner::Serial);
    cImpl.ReleaseCalibration(CalibrationOwner::Remote);
    cChannel.SetRawEcho(false);
    // The implement points its button and knob pointers at the interface on
    // its first Update(); the wizard reads the knob through those pointers.
    cImpl.Update();
    term.in.clear(); term.pos = 0; term.out.clear();
    forgetCalls = 0;
}

// Type a line and let the wizard consume it in one Process() call.
static void line(CalibrationSprayer& cal, const char* s) {
    term.Type(s);
    term.Type("\n");
    cal.Process();
}

// Idle -> menu: the first byte of any kind opens it.
static void openMenu(CalibrationSprayer& cal) {
    term.Type("x");
    cal.Process();
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

test(CalibrationSprayer, idle_anyByteOpensMenu_noForgetOptionWithoutHandler) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    cal.Process();
    assertFalse(term.has("=== SPRAYER CALIBRATION ==="));   // nothing typed, stays idle
    openMenu(cal);
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));
    assertTrue(term.has("8. GPS raw passthrough (OFF"));
    assertFalse(term.has("9. Forget paired phones"));
    assertTrue(term.has("Choose: "));
}

test(CalibrationSprayer, menu_showsForgetOption_onceHandlerSet) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    cal.SetForgetPhonesHandler(countForget);
    openMenu(cal);
    assertTrue(term.has("9. Forget paired phones (Bluetooth)"));
}

test(CalibrationSprayer, menu_q_exitsToIdle_nextByteReopens) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();
    line(cal, "q");
    assertTrue(term.has("Exiting calibration."));
    assertFalse(term.has("=== SPRAYER CALIBRATION ==="));
    term.clearOut();
    openMenu(cal);
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));
}

test(CalibrationSprayer, menu_Q_uppercase_alsoExits) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "Q");
    assertTrue(term.has("Exiting calibration."));
}

test(CalibrationSprayer, menu_invalidChoice_reprintsMenu) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();
    line(cal, "k");
    assertTrue(term.has("Invalid choice."));
    assertEqual(term.count("=== SPRAYER CALIBRATION ==="), 1);
}

test(CalibrationSprayer, menu_emptyLine_reprintsMenu) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "");
    assertEqual(term.count("=== SPRAYER CALIBRATION ==="), 2);
}

test(CalibrationSprayer, menu_carriageReturn_isIgnored) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();
    term.Type("3\r\n");
    cal.Process();
    assertEqual(term.count("=== CURRENT CALIBRATION ==="), 1);
}

test(CalibrationSprayer, menu_3_printsCalibrationAndSettings) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();
    line(cal, "3");
    assertTrue(term.has("=== CURRENT CALIBRATION ==="));
    assertTrue(term.has("  1: analog=0  dose=50 l/ha"));
    assertTrue(term.has("  3: analog=4095  dose=200 l/ha"));
    assertTrue(term.has("PWM output (3 points):"));
    assertTrue(term.has("  2: pwm=2048  flow=2000 ml/min"));
    assertTrue(term.has("Settings:"));
    assertTrue(term.has("  width="));
    assertTrue(term.has("  blePasskey="));
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));   // back at the menu
}

test(CalibrationSprayer, menu_8_togglesRawPassthroughOnTheChannel) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "8");
    assertTrue(cChannel.GetRawEcho());
    assertTrue(term.has("GPS raw passthrough enabled."));
    assertTrue(term.has("8. GPS raw passthrough (ON"));
    line(cal, "8");
    assertFalse(cChannel.GetRawEcho());
    assertTrue(term.has("GPS raw passthrough disabled."));
}

test(CalibrationSprayer, menu_9_callsForgetHandler_orIsInvalidWithoutOne) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "9");
    assertEqual(forgetCalls, 0);
    assertTrue(term.has("Invalid choice."));

    cal.SetForgetPhonesHandler(countForget);
    term.clearOut();
    line(cal, "9");
    assertEqual(forgetCalls, 1);
    assertTrue(term.has("Paired phones forgotten; the app will ask for the code again."));
}

// ---------------------------------------------------------------------------
// Telemetry toggles (5/6/7): printed every 500 ms, only at the menu or idle
// ---------------------------------------------------------------------------

test(CalibrationSprayer, telemetry_togglesPrintEvery500ms_atMenu) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "5");
    assertTrue(term.has("Analog output enabled."));
    term.clearOut();

    millisValue(100);
    cal.Process();
    assertFalse(term.has("raw:"));
    millisValue(500);
    cal.Process();
    // Knob at 0 sits on the first dose point (50 l/ha); no fix, so no flow
    // and the actual dose is undefined.
    assertTrue(term.has("raw:0,dose:50.0,speed:0.00,flow:0.0,actual:-1.0,dev:0"));

    line(cal, "6");
    assertTrue(term.has("Pump output enabled."));
    line(cal, "7");
    assertTrue(term.has("GPS output enabled."));
    term.clearOut();
    millisValue(1000);
    cal.Process();
    assertTrue(term.has("raw:"));
    assertTrue(term.has("calMode=N  pumpBtn=0  pumpOn=0  pumpPwmFlag=0  pumpVal=0"));
    assertTrue(term.has("GPS: speed=0.00 m/s  lat=0.000000  lon=0.000000  quality=1"));

    line(cal, "5");
    assertTrue(term.has("Analog output disabled."));
    term.clearOut();
    millisValue(1500);
    cal.Process();
    assertFalse(term.has("raw:"));
    assertTrue(term.has("calMode="));
}

test(CalibrationSprayer, telemetry_keepsPrintingAfterExitToIdle) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "7");
    line(cal, "q");
    term.clearOut();
    millisValue(600);
    cal.Process();
    assertTrue(term.has("GPS: speed="));
}

test(CalibrationSprayer, telemetry_silentInsideACalibrationStep) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "5");
    line(cal, "1");             // now in ANALOG_CAPTURE
    term.clearOut();
    millisValue(5000);
    cal.Process();
    assertFalse(term.has("raw:"));
}

// ---------------------------------------------------------------------------
// Analog input calibration (option 1)
// ---------------------------------------------------------------------------

test(CalibrationSprayer, analog_busyWhileAppHoldsCalibration) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    assertTrue(cImpl.AcquireCalibration(CalibrationOwner::Remote));
    openMenu(cal);
    term.clearOut();
    line(cal, "1");
    assertTrue(term.has("Busy: the app holds calibration."));
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));
    assertEqual(cImpl.doseCalibrationPoints[0].dose, 50);   // untouched
    cImpl.ReleaseCalibration(CalibrationOwner::Remote);
}

test(CalibrationSprayer, analog_threePoints_rejectsBadDose_sortsAndSaves) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();

    knob(300);
    line(cal, "1");
    assertTrue(term.has("--- Analog point 1/3 ---"));
    assertTrue(term.has("Set knob to MINIMUM position, then press ENTER."));
    line(cal, "");
    assertTrue(term.has("Analog reading: 300"));
    assertTrue(term.has("Enter dose for this position (l/ha): "));

    // Only digits reach the buffer in this step, so letters vanish and the
    // empty line is rejected; so is zero.
    line(cal, "abc");
    assertTrue(term.has("enter a positive whole number."));
    term.clearOut();
    line(cal, "0");
    assertTrue(term.has("Invalid"));
    line(cal, "150");
    assertTrue(term.has("Point 1 saved."));
    assertTrue(term.has("--- Analog point 2/3 ---"));
    assertTrue(term.has("MIDDLE"));

    knob(100);
    line(cal, "");
    line(cal, "50");
    assertTrue(term.has("Point 2 saved."));
    assertTrue(term.has("MAXIMUM"));

    knob(200);
    line(cal, "");
    line(cal, "100");
    assertTrue(term.has("Point 3 saved."));
    assertTrue(term.has("Analog calibration saved."));
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));   // back at the menu

    // Stored ascending by analog reading whatever order the knob went.
    assertEqual(cImpl.doseCalibrationPoints[0].analogValue, 100);
    assertEqual(cImpl.doseCalibrationPoints[0].dose, 50);
    assertEqual(cImpl.doseCalibrationPoints[1].analogValue, 200);
    assertEqual(cImpl.doseCalibrationPoints[1].dose, 100);
    assertEqual(cImpl.doseCalibrationPoints[2].analogValue, 300);
    assertEqual(cImpl.doseCalibrationPoints[2].dose, 150);

    // Calibration handed back: the app can take it now.
    assertTrue(cImpl.AcquireCalibration(CalibrationOwner::Remote));
    cImpl.ReleaseCalibration(CalibrationOwner::Remote);
}

// ---------------------------------------------------------------------------
// PWM output calibration (option 2)
// ---------------------------------------------------------------------------

test(CalibrationSprayer, pwm_armRequiresKnobAtMinimum) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    knob(100);
    line(cal, "2");
    assertTrue(term.has("=== PWM OUTPUT CALIBRATION ==="));
    assertTrue(term.has("Turn the analog knob fully to MINIMUM, then press ENTER to arm."));
    term.clearOut();
    line(cal, "");
    assertTrue(term.has("Knob not at minimum (reading 100). Turn it down and press ENTER."));
    assertFalse(term.has("until the pump just starts flowing"));
    knob(10);
    line(cal, "");
    assertTrue(term.has("Turn the analog knob until the pump just starts flowing."));
    cImpl.ReleaseCalibration(CalibrationOwner::Serial);
}

test(CalibrationSprayer, pwm_busyWhileAppHoldsCalibration) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    assertTrue(cImpl.AcquireCalibration(CalibrationOwner::Remote));
    openMenu(cal);
    line(cal, "2");
    assertTrue(term.has("Busy: the app holds calibration."));
    cImpl.ReleaseCalibration(CalibrationOwner::Remote);
}

// One timed run: ENTER starts it, the countdown follows millis(), input is
// discarded while it runs, and the volume prompt appears once the implement
// ends the run. AUnit's assert macros only work inside a test body, so this
// returns the first checkpoint that failed (0 = all good) for the test to
// assert on.
static int runOnePoint(CalibrationSprayer& cal, unsigned long& now, const char* volume) {
    line(cal, "");
    if (!term.has("Running...")) return 1;
    if (!cImpl.CalibrationRunActive()) return 2;
    term.clearOut();
    now += 1;
    millisValue(now);
    cal.Process();
    if (!term.has("60s remaining")) return 3;
    // Typed while running: thrown away, so it cannot become the volume below.
    term.Type("77\n");
    cal.Process();
    now += 30000;
    millisValue(now);
    cal.Process();
    if (!term.has("30s remaining")) return 4;
    now += 30000;
    millisValue(now);
    cImpl.Update();            // the implement's own timer ends the run
    if (cImpl.CalibrationRunActive()) return 5;
    term.clearOut();
    cal.Process();
    if (!term.has("Run complete. Enter volume collected (ml): ")) return 6;
    line(cal, volume);
    return 0;
}

test(CalibrationSprayer, pwm_fullRun_fiveEqualSteps_fromCapturedStart) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    knob(0);
    line(cal, "2");
    line(cal, "");                       // armed
    term.clearOut();

    // Knob search: the live PWM tracks the knob, typed text is ignored.
    knob(2048);
    cal.Process();
    assertTrue(term.has("PWM: 2048"));
    term.Type("zz");
    cal.Process();
    assertFalse(term.has("zz"));
    line(cal, "");
    assertTrue(term.has("Start PWM captured: 2048"));
    assertTrue(term.has("Point 1/5: PWM = 2048. Press ENTER to start 1-minute run."));

    unsigned long now = 0;
    // Bad volumes first: zero, above 4000, letters.
    line(cal, "");
    now += 60000; millisValue(now); cImpl.Update(); cal.Process();
    line(cal, "0");
    assertTrue(term.has("enter a whole number between 1 and 4000."));
    line(cal, "5000");
    assertEqual(term.count("Invalid"), 2);
    term.clearOut();
    line(cal, "1500");
    assertTrue(term.has("Saved: PWM=2048, flow=1500 ml/min."));
    assertTrue(term.has("Point 2/5: PWM = 2559"));

    assertEqual(runOnePoint(cal, now, "2000"), 0);
    assertTrue(term.has("Point 3/5: PWM = 3071"));
    assertEqual(runOnePoint(cal, now, "2500"), 0);
    assertTrue(term.has("Point 4/5: PWM = 3583"));
    assertEqual(runOnePoint(cal, now, "3000"), 0);
    assertTrue(term.has("Point 5/5: PWM = 4095"));
    assertEqual(runOnePoint(cal, now, "3500"), 0);
    assertTrue(term.has("PWM calibration saved (5 points)."));
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));

    assertEqual((int)cImpl.numPwmCalibrationPoints, 5);
    assertEqual(cImpl.pwmCalibrationPoints[0].pwm, 2048);
    assertEqual(cImpl.pwmCalibrationPoints[0].flowMlMin, 1500);
    assertEqual(cImpl.pwmCalibrationPoints[1].pwm, 2559);
    assertEqual(cImpl.pwmCalibrationPoints[3].flowMlMin, 3000);
    assertEqual(cImpl.pwmCalibrationPoints[4].pwm, 4095);
    assertEqual(cImpl.pwmCalibrationPoints[4].flowMlMin, 3500);
    assertFalse(cImpl.CalibrationRunActive());
    assertTrue(cImpl.AcquireCalibration(CalibrationOwner::Remote));   // released
    cImpl.ReleaseCalibration(CalibrationOwner::Remote);
    // AUnit times its tests on the same mocked millis(); five minutes of
    // simulated runs would make every later test look timed out.
    millisValue(0);
}

// ---------------------------------------------------------------------------
// Edit a PWM point (option 4)
// ---------------------------------------------------------------------------

test(CalibrationSprayer, editPwm_select_validatesIndex_andCancels) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "4");
    assertTrue(term.has("=== CURRENT CALIBRATION ==="));
    assertTrue(term.has("Select PWM point to edit (1-3, q to cancel): "));
    term.clearOut();
    line(cal, "9");
    assertTrue(term.has("enter 1 to 3, q to cancel: "));
    line(cal, "0");
    assertEqual(term.count("Invalid"), 2);
    term.clearOut();
    line(cal, "q");
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));
    assertEqual(cImpl.pwmCalibrationPoints[1].flowMlMin, 2000);
}

test(CalibrationSprayer, editPwm_value_validatesRange_cancels_andSaves) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "4");
    line(cal, "2");
    assertTrue(term.has("Point 2: PWM=2048, flow=2000 ml/min"));
    assertTrue(term.has("New flow (1-4000 ml/min, q to cancel): "));
    term.clearOut();
    line(cal, "abc");           // letters never enter the buffer here
    assertTrue(term.has("enter a whole number between 1 and 4000."));
    line(cal, "0");
    line(cal, "4001");
    assertEqual(term.count("Invalid"), 3);
    term.clearOut();
    line(cal, "Q");
    assertTrue(term.has("Cancelled."));
    assertEqual(cImpl.pwmCalibrationPoints[1].flowMlMin, 2000);

    line(cal, "4");
    line(cal, "2");
    term.clearOut();
    line(cal, "2500");
    assertTrue(term.has("Saved."));
    assertTrue(term.has("=== SPRAYER CALIBRATION ==="));
    assertEqual(cImpl.pwmCalibrationPoints[1].flowMlMin, 2500);
    assertEqual(cImpl.pwmCalibrationPoints[1].pwm, 2048);   // pwm untouched
}

// ---------------------------------------------------------------------------
// Line editor
// ---------------------------------------------------------------------------

test(CalibrationSprayer, editor_backspaceRemovesLastChar_andEchoes) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    line(cal, "4");
    line(cal, "1");
    term.clearOut();
    term.Type("25\b\b\b2600\n");          // third backspace on an empty buffer is a no-op
    cal.Process();
    assertTrue(term.has("\b \b"));
    assertTrue(term.has("Saved."));
    assertEqual(cImpl.pwmCalibrationPoints[0].flowMlMin, 2600);
}

test(CalibrationSprayer, editor_capsLineAt31Chars_withoutOverflow) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();
    std::string longLine(40, 'z');                 // 'z' appears nowhere in the menu text
    term.Type(longLine.c_str());
    term.Type("\n");
    cal.Process();
    assertEqual(term.count("z"), 31);              // echo stops at the buffer limit
    assertTrue(term.has("Invalid choice."));
}

test(CalibrationSprayer, editor_dropsControlAndNonAsciiBytes) {
    cReset();
    CalibrationSprayer cal(&term, &cImpl, &cChannel);
    openMenu(cal);
    term.clearOut();
    term.Type("\x01\x80" "3\n");
    cal.Process();
    assertEqual(term.count("=== CURRENT CALIBRATION ==="), 1);
}
