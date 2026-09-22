/*
  test_RemoteSprayer - Tests for RemoteSprayer: command parsing and replies, staged calibration
  edits, the firmware-timed pump run, settings, telemetry lines and the disconnect rule.
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max
// as macros, and GCC's <string>/<vector> use std::min/max with three
// arguments, which the macros then break. MSVC's headers happened not to.
#include <string>
#include <vector>

#include <AUnit.h>
#include "config/ConfigSprayer.hpp"
#include "implement/ImplementSprayer.hpp"
#include "remote/RemoteSprayer.hpp"
#include "SerialGuidanceChannel.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. Same shared-static pattern as test_ImplementSprayer.cpp;
// the sink records every line so a test can assert on exact replies.
// ---------------------------------------------------------------------------

struct CaptureSink : public RemoteSink {
    std::vector<std::string> lines;
    void WriteLine(const char* line) override { lines.push_back(line); }
    void clear() { lines.clear(); }
    size_t count() const { return lines.size(); }
    const std::string& last() const { static std::string none; return lines.empty() ? none : lines.back(); }
    const std::string& at(size_t i) const { static std::string none; return i < lines.size() ? lines[i] : none; }
    bool has(const char* line) const {
        for (const auto& l : lines) if (l == line) return true;
        return false;
    }
};

// The receiver port, as the real SerialGuidanceChannel sees it: a byte
// queue the test fills with whole lines. No stub for the channel or the
// data model -- the tap, the baudrate change and the fix bookkeeping under
// test are the real ones.
struct FakeGpsSerial : public HardwareSerial {
    std::string bytes;
    size_t      pos = 0;
    void Feed(const char* line) { bytes += line; }
    void clear() { bytes.clear(); pos = 0; }
    int available() override { return int(bytes.size() - pos); }
    int read() override      { return pos < bytes.size() ? (uint8_t)bytes[pos++] : -1; }
};

static InterfaceSprayer      rIface;
static GuidanceSource        rGps;
static FakeGpsSerial         rSerial;
static SerialGuidanceChannel rChannel(nullptr, &rSerial, &rGps);
static ConfigSprayer         rCfg;
static ImplementSprayer      rImpl(nullptr, &rGps, &rIface, &rCfg);
static CaptureSink           sink;
static RemoteSprayer         remote(&rImpl, &rChannel, &rCfg, &sink);

// Speed in m/s, as the tests think of it; GuidanceSource stores knots and
// stamps the VTG fix like a real receiver's message would.
static void gpsSpeed(float speedMs) { rGps.SetSpeedKnots(speedMs / GPS_MS_PER_KNOT); }

static void rReset() {
    millisValue(0);
    rGps = GuidanceSource();   // no fix ever, speed 0, position 0
    rGps.SetQuality(1);
    rSerial.clear();
    rSerial.begunBaud = 0;
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) { rIface.buttons[i].state = false; rIface.buttons[i].flag = true; rIface.buttons[i].timer = 0; }
    rIface.analogInputs[0].value = 0;
    for (int i = 0; i < NUM_OUTPUTS; ++i) { rImpl.outputs[i].state = false; rImpl.outputs[i].pwm = false; rImpl.outputs[i].value = 0; rImpl.outputs[i].timer = 0; }
    rImpl.doseLHA = 0.0f; rImpl.doseLM = 0.0f;
    rImpl.actualLHA = ImplementSprayer::kActualDoseUndefined;
    rImpl.doseDeviation   = false;
    rImpl.deviationAccumMs = 0;
    rImpl.deviationPending = false;
    rImpl.doseCalibrationPoints[0] = { 50,  0 };
    rImpl.doseCalibrationPoints[1] = { 100, 2048 };
    rImpl.doseCalibrationPoints[2] = { 200, 4095 };
    rImpl.pwmCalibrationPoints[0] = { 0,    0 };
    rImpl.pwmCalibrationPoints[1] = { 2000, 2048 };
    rImpl.pwmCalibrationPoints[2] = { 4000, 4095 };
    rImpl.numPwmCalibrationPoints = 3;
    rCfg = ConfigSprayer();
    // Whatever a previous test left behind: give calibration back from both
    // sides and drop telemetry, the way a link drop would.
    rImpl.ReleaseCalibration(CalibrationOwner::Serial);
    remote.OnDisconnect();
    // One cycle with no fix ever seen: the staleness rule drains the speed
    // moving average, so a previous test's samples cannot leak into this one.
    rImpl.Update();
    sink.clear();
}

// One main-loop iteration: time, a fresh speed message, implement, remote.
static void tick(unsigned long ms, float speedMs) {
    millisValue(ms);
    rChannel.Update();          // whatever the test fed the port arrives now
    gpsSpeed(speedMs);
    rImpl.Update();
    remote.Update();
}

// ---------------------------------------------------------------------------
// Basics
// ---------------------------------------------------------------------------

test(RemoteSprayer, ping_repliesOk) {
    rReset();
    remote.HandleLine("PING");
    assertEqual(sink.count(), (size_t)1);
    assertEqual(sink.last().c_str(), "OK");
}

test(RemoteSprayer, unknownCommand_errUnknown) {
    rReset();
    remote.HandleLine("FROBNICATE");
    assertEqual(sink.last().c_str(), "ERR:unknown");
    remote.HandleLine("");
    assertEqual(sink.last().c_str(), "ERR:unknown");
}

test(RemoteSprayer, info_versionLineThenOk) {
    rReset();
    remote.HandleLine("INFO");
    assertEqual(sink.count(), (size_t)2);
    assertEqual(sink.at(0).c_str(), "V:0.2,2");
    assertEqual(sink.at(1).c_str(), "OK");
}

test(RemoteSprayer, onConnect_sendsVersion) {
    rReset();
    remote.OnConnect();
    assertEqual(sink.at(0).c_str(), "V:0.2,2");
}

// ---------------------------------------------------------------------------
// Calibration tables
// ---------------------------------------------------------------------------

test(RemoteSprayer, untrustedChannel_readOnlyCommandsPass) {
    rReset();
    remote.HandleLine("PING", false);
    assertEqual(sink.last().c_str(), "OK");
    remote.HandleLine("CAL GET", false);
    assertEqual(sink.last().c_str(), "OK");
    remote.HandleLine("CFG GET", false);
    assertEqual(sink.last().c_str(), "OK");
    remote.HandleLine("TELEM S 1", false);
    assertEqual(sink.last().c_str(), "OK");
    remote.HandleLine("INFO", false);
    assertEqual(sink.last().c_str(), "OK");
}

test(RemoteSprayer, untrustedChannel_protectedCommandsRefused) {
    // Nothing that moves an output or persists may come in unauthenticated
    // (NeptuneGPS_Triton#53), and nothing changes when it is tried.
    rReset();
    const char* protectedLines[] = {
        "CAL MODE 1", "CAL DOSE 0 100 50", "CAL PWM 0 100 50", "CAL PWMN 3", "CAL SAVE",
        "PWM SET 100", "PWM RUN 100 5", "PWM STOP", "CFG SET width_cm 400",
    };
    for (const char* l : protectedLines) {
        remote.HandleLine(l, false);
        assertEqual(sink.last().c_str(), "ERR:auth");
    }
    assertTrue(rImpl.GetCalibrationOwner() == CalibrationOwner::None);
    assertEqual(rCfg.Get().widthCm, 300);
    assertFalse(rImpl.CalibrationRunActive());
}

test(RemoteSprayer, isProtected_classification) {
    assertFalse(RemoteSprayer::IsProtected("PING"));
    assertFalse(RemoteSprayer::IsProtected("CAL GET"));
    assertFalse(RemoteSprayer::IsProtected("CFG GET"));
    assertFalse(RemoteSprayer::IsProtected("TELEM N 1"));
    assertTrue(RemoteSprayer::IsProtected("CAL MODE 1"));
    assertTrue(RemoteSprayer::IsProtected("CAL SAVE"));
    assertTrue(RemoteSprayer::IsProtected("PWM RUN 1000 60"));
    assertTrue(RemoteSprayer::IsProtected("CFG SET buzzer 0"));
    assertFalse(RemoteSprayer::IsProtected("CALX"));      // not the CAL command
    assertFalse(RemoteSprayer::IsProtected(nullptr));
}

test(RemoteSprayer, calGet_listsBothTablesThenOk) {
    rReset();
    remote.HandleLine("CAL GET");
    assertEqual(sink.count(), (size_t)7);
    assertEqual(sink.at(0).c_str(), "C:D,0,0,50");
    assertEqual(sink.at(1).c_str(), "C:D,1,2048,100");
    assertEqual(sink.at(2).c_str(), "C:D,2,4095,200");
    assertEqual(sink.at(3).c_str(), "C:P,0,0,0");
    assertEqual(sink.at(4).c_str(), "C:P,1,2048,2000");
    assertEqual(sink.at(5).c_str(), "C:P,2,4095,4000");
    assertEqual(sink.at(6).c_str(), "OK");
}

test(RemoteSprayer, calMode_takesAndReturnsCalibration) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    assertEqual(sink.last().c_str(), "OK");
    assertTrue(rImpl.GetCalibrationOwner() == CalibrationOwner::Remote);
    assertTrue(rImpl.calibrationMode);

    remote.HandleLine("CAL MODE 0");
    assertEqual(sink.last().c_str(), "OK");
    assertTrue(rImpl.GetCalibrationOwner() == CalibrationOwner::None);
    assertFalse(rImpl.calibrationMode);
}

test(RemoteSprayer, calMode_busyWhileSerialWizardHoldsIt) {
    rReset();
    assertTrue(rImpl.AcquireCalibration(CalibrationOwner::Serial));
    remote.HandleLine("CAL MODE 1");
    assertEqual(sink.last().c_str(), "BUSY");
    assertTrue(rImpl.GetCalibrationOwner() == CalibrationOwner::Serial);
}

test(RemoteSprayer, calEdits_requireCalibrationMode) {
    rReset();
    remote.HandleLine("CAL DOSE 0 10 40");
    assertEqual(sink.last().c_str(), "ERR:mode");
    remote.HandleLine("CAL PWM 0 100 50");
    assertEqual(sink.last().c_str(), "ERR:mode");
    remote.HandleLine("CAL SAVE");
    assertEqual(sink.last().c_str(), "ERR:mode");
    remote.HandleLine("PWM SET 100");
    assertEqual(sink.last().c_str(), "ERR:mode");
    remote.HandleLine("PWM RUN 100");
    assertEqual(sink.last().c_str(), "ERR:mode");
}

test(RemoteSprayer, calDose_stagedThenSaved_sortedByAnalog) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    // Entered out of order on purpose: the live table must come out ascending
    // by analog value, exactly as the serial wizard's finishAnalogCal() does.
    remote.HandleLine("CAL DOSE 0 4000 300");
    remote.HandleLine("CAL DOSE 1 100 60");
    remote.HandleLine("CAL DOSE 2 2000 150");
    assertEqual(sink.last().c_str(), "OK");
    // Not applied yet
    assertEqual(rImpl.doseCalibrationPoints[0].dose, 50);

    remote.HandleLine("CAL SAVE");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rImpl.doseCalibrationPoints[0].analogValue, 100);
    assertEqual(rImpl.doseCalibrationPoints[0].dose, 60);
    assertEqual(rImpl.doseCalibrationPoints[1].analogValue, 2000);
    assertEqual(rImpl.doseCalibrationPoints[1].dose, 150);
    assertEqual(rImpl.doseCalibrationPoints[2].analogValue, 4000);
    assertEqual(rImpl.doseCalibrationPoints[2].dose, 300);
}

test(RemoteSprayer, calPwm_stagedThenSaved_sortedByFlow) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("CAL PWMN 4");
    remote.HandleLine("CAL PWM 0 1000 500");
    remote.HandleLine("CAL PWM 1 4095 3900");
    remote.HandleLine("CAL PWM 2 2000 1500");
    remote.HandleLine("CAL PWM 3 3000 2600");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rImpl.numPwmCalibrationPoints, (uint8_t)3);   // untouched until save

    remote.HandleLine("CAL SAVE");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rImpl.numPwmCalibrationPoints, (uint8_t)4);
    assertEqual(rImpl.pwmCalibrationPoints[0].flowMlMin, 500);
    assertEqual(rImpl.pwmCalibrationPoints[1].flowMlMin, 1500);
    assertEqual(rImpl.pwmCalibrationPoints[2].flowMlMin, 2600);
    assertEqual(rImpl.pwmCalibrationPoints[3].flowMlMin, 3900);
    assertEqual(rImpl.pwmCalibrationPoints[3].pwm, 4095);
}

test(RemoteSprayer, calEdits_rangeChecked) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("CAL DOSE 3 100 50");        // index
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL DOSE 0 5000 50");       // analog > 4095
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL DOSE 0 100 0");         // dose must be positive
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL PWM 0 5000 100");       // pwm > 4095
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL PWM 0 100 5000");       // flow > 4000
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL PWMN 1");               // fewer than two points is no curve
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL PWMN 11");              // more than MAX_PWM_CAL_POINTS
    assertEqual(sink.last().c_str(), "ERR:range");
    remote.HandleLine("CAL DOSE 0 x 50");          // not a number
    assertEqual(sink.last().c_str(), "ERR:args");
    remote.HandleLine("CAL DOSE 0");               // too few arguments
    assertEqual(sink.last().c_str(), "ERR:args");
}

test(RemoteSprayer, calMode0_discardsStagedEdits) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("CAL DOSE 0 10 999");
    remote.HandleLine("CAL MODE 0");
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("CAL SAVE");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rImpl.doseCalibrationPoints[0].dose, 50);   // live table re-staged, edit gone
}

// ---------------------------------------------------------------------------
// Pump control while calibrating
// ---------------------------------------------------------------------------

test(RemoteSprayer, pwmSet_appliesDuty) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("PWM SET 1234");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rImpl.GetCalibrationDuty(), 1234);
    remote.HandleLine("PWM SET 4096");
    assertEqual(sink.last().c_str(), "ERR:range");
    assertEqual(rImpl.GetCalibrationDuty(), 1234);
}

test(RemoteSprayer, pwmRun_countsDownAndStopsOnTheBoard) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    millisValue(1000);
    remote.HandleLine("PWM RUN 2000 3");
    assertEqual(sink.at(sink.count() - 2).c_str(), "OK");
    assertEqual(sink.last().c_str(), "R:3");
    assertEqual(rImpl.GetCalibrationDuty(), 2000);
    assertTrue(rImpl.CalibrationRunActive());

    sink.clear();
    for (unsigned long t = 1100; t <= 4000; t += 100) tick(t, 0.0f);
    // One line per second boundary, nothing in between, R:0 when the board ends it.
    assertEqual(sink.count(), (size_t)3);
    assertEqual(sink.at(0).c_str(), "R:2");
    assertEqual(sink.at(1).c_str(), "R:1");
    assertEqual(sink.at(2).c_str(), "R:0");
    assertFalse(rImpl.CalibrationRunActive());
    assertEqual(rImpl.GetCalibrationDuty(), 0);
}

test(RemoteSprayer, pwmRun_defaultsToMaxAndRejectsLonger) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("PWM RUN 2000 61");
    assertEqual(sink.last().c_str(), "ERR:range");
    assertFalse(rImpl.CalibrationRunActive());

    millisValue(1000);
    remote.HandleLine("PWM RUN 2000");
    assertEqual(sink.last().c_str(), "R:60");
    assertEqual(rImpl.CalibrationRunRemainingMs(), ImplementSprayer::kCalibrationRunMaxMs);
}

test(RemoteSprayer, pwmSet_refusedWhileRunning) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("PWM RUN 2000 10");
    remote.HandleLine("PWM SET 100");
    assertEqual(sink.last().c_str(), "ERR:running");
    assertEqual(rImpl.GetCalibrationDuty(), 2000);
}

test(RemoteSprayer, pwmStop_endsRunWithRZero) {
    rReset();
    remote.HandleLine("CAL MODE 1");
    millisValue(1000);
    remote.HandleLine("PWM RUN 2000 10");
    sink.clear();
    millisValue(1500);
    remote.HandleLine("PWM STOP");
    assertTrue(sink.has("R:0"));
    assertEqual(sink.last().c_str(), "OK");
    assertFalse(rImpl.CalibrationRunActive());
    assertEqual(rImpl.GetCalibrationDuty(), 0);
}

test(RemoteSprayer, disconnect_givesCalibrationBack_normalOperationResumes) {
    // The board is standalone; the app is only a GUI. Losing it must not
    // leave the wizard's pump duty in place, and must not need a dedicated
    // stop path either: giving calibration back is what ends the run.
    rReset();
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("PWM RUN 2000 30");
    remote.HandleLine("TELEM S 1");
    assertTrue(rImpl.CalibrationRunActive());

    remote.OnDisconnect();
    assertTrue(rImpl.GetCalibrationOwner() == CalibrationOwner::None);
    assertFalse(rImpl.calibrationMode);
    assertFalse(rImpl.CalibrationRunActive());
    assertEqual(rImpl.GetCalibrationDuty(), 0);
    assertFalse(remote.StatusEnabled());

    // Buttons up, so normal logic turns every output off on the next cycle.
    tick(100, 0.0f);
    assertFalse(rImpl.outputs[2].state);
}

test(RemoteSprayer, disconnect_leavesSerialWizardAlone) {
    rReset();
    rImpl.AcquireCalibration(CalibrationOwner::Serial);
    remote.OnDisconnect();
    assertTrue(rImpl.GetCalibrationOwner() == CalibrationOwner::Serial);
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

test(RemoteSprayer, cfgGet_listsEveryKeyThenOk) {
    rReset();
    remote.HandleLine("CFG GET");
    assertEqual(sink.count(), (size_t)6);
    assertEqual(sink.at(0).c_str(), "K:width_cm,300");
    assertEqual(sink.at(1).c_str(), "K:guid_ms,2000");
    assertEqual(sink.at(2).c_str(), "K:gps_baud,7");
    assertEqual(sink.at(3).c_str(), "K:gps_minq,0");
    assertEqual(sink.at(4).c_str(), "K:buzzer,1");
    assertEqual(sink.at(5).c_str(), "OK");
}

test(RemoteSprayer, cfgSet_appliesValidatedValue) {
    rReset();
    remote.HandleLine("CFG SET width_cm 450");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rCfg.Get().widthCm, 450);
    remote.HandleLine("CFG SET gps_minq 4");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rCfg.Get().gpsMinQuality, (uint8_t)4);
    remote.HandleLine("CFG SET guid_ms 1500");
    assertEqual(rCfg.Get().guidanceTimeoutMs, (unsigned long)1500);
    remote.HandleLine("CFG SET gps_baud 3");
    assertEqual(rCfg.Get().gpsBaudIndex, (uint8_t)3);
}

test(RemoteSprayer, cfgSet_gpsBaud_reopensThePortAtOnce) {
    rReset();
    remote.HandleLine("CFG SET gps_baud 1");
    assertEqual(sink.last().c_str(), "OK");
    assertEqual(rSerial.begunBaud, (unsigned long)9600);
    remote.HandleLine("CFG SET gps_baud 9");      // refused: nothing reopened
    assertEqual(sink.last().c_str(), "ERR:range");
    assertEqual(rSerial.begunBaud, (unsigned long)9600);
    remote.HandleLine("CFG SET width_cm 400");    // other keys leave the port alone
    assertEqual(rSerial.begunBaud, (unsigned long)9600);
}

test(RemoteSprayer, cfgSet_buzzer) {
    rReset();
    remote.HandleLine("CFG SET buzzer 0");
    assertEqual(sink.last().c_str(), "OK");
    assertFalse(rCfg.Get().buzzerEnabled);
    remote.HandleLine("CFG SET buzzer 2");
    assertEqual(sink.last().c_str(), "ERR:range");
    assertFalse(rCfg.Get().buzzerEnabled);
    remote.HandleLine("CFG SET buzzer 1");
    assertTrue(rCfg.Get().buzzerEnabled);
}

test(RemoteSprayer, cfgSet_rejectsBadInput) {
    rReset();
    remote.HandleLine("CFG SET width_cm 10");
    assertEqual(sink.last().c_str(), "ERR:range");
    assertEqual(rCfg.Get().widthCm, 300);
    remote.HandleLine("CFG SET colour blue");
    assertEqual(sink.last().c_str(), "ERR:key");
    remote.HandleLine("CFG SET width_cm");
    assertEqual(sink.last().c_str(), "ERR:args");
    remote.HandleLine("CFG SET width_cm wide");
    assertEqual(sink.last().c_str(), "ERR:args");
}

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------

test(RemoteSprayer, telemetry_offByDefault) {
    rReset();
    for (unsigned long t = 100; t <= 2000; t += 100) tick(t, 1.0f);
    assertEqual(sink.count(), (size_t)0);
}

test(RemoteSprayer, status_lineFormatAndRate) {
    rReset();
    rImpl.outputs[2].pwm = true;
    rIface.analogInputs[0].value = 2048;
    remote.HandleLine("TELEM S 1");
    assertEqual(sink.last().c_str(), "OK");
    sink.clear();

    // 100 ms ticks for one second at 1.0 m/s: 5 lines at 200 ms spacing.
    for (unsigned long t = 100; t <= 1000; t += 100) tick(t, 1.0f);
    assertEqual(sink.count(), (size_t)5);
    // speed, req, act, flow, raw, mixer, vern, pump, pumpPwm, dev, cal, inputs, outputs
    assertEqual(sink.last().c_str(), "S:1.00,100.0,100.0,1800.0,2048,0,0,0,1843,0,0,0000,0000");

    remote.HandleLine("TELEM S 0");
    sink.clear();
    for (unsigned long t = 1100; t <= 2000; t += 100) tick(t, 1.0f);
    assertEqual(sink.count(), (size_t)0);
}

test(RemoteSprayer, status_showsUndefinedActualAndCalibrationOwner) {
    rReset();
    rImpl.outputs[2].pwm = true;
    remote.HandleLine("TELEM S 1");
    remote.HandleLine("CAL MODE 1");
    remote.HandleLine("PWM SET 777");
    sink.clear();
    tick(200, 0.0f);   // standing still: actual undefined, pump pwm is the calibration duty
    assertEqual(sink.count(), (size_t)1);
    assertEqual(sink.last().c_str(), "S:0.00,50.0,-1.0,0.0,0,0,0,0,777,0,2,0000,0000");
}

test(RemoteSprayer, status_inputAndOutputBits) {
    // IN1 (mixer switch) and IN4 (aux) held: inputs 1001; the mixer output
    // follows its switch at once, nothing else is on yet: outputs 1000.
    rReset();
    rIface.buttons[0].state = true;
    rIface.buttons[3].state = true;
    remote.HandleLine("TELEM S 1");
    sink.clear();
    tick(200, 0.0f);
    assertEqual(sink.count(), (size_t)1);
    const std::string& s = sink.last();
    assertTrue(s.size() > 10 && s.compare(s.size() - 10, 10, ",1001,1000") == 0);
}

test(RemoteSprayer, nmea_offByDefault_onDemand_rateLimited) {
    rReset();
    rSerial.Feed("$GPGGA,123519,4807.038,N,01131.000,E,0,00,,,M,,M,,*47\r\n");
    tick(100, 0.0f);
    assertEqual(sink.count(), (size_t)0);          // nothing without TELEM N

    remote.HandleLine("TELEM N 1");
    assertEqual(sink.last().c_str(), "OK");
    sink.clear();
    tick(200, 0.0f);
    assertEqual(sink.count(), (size_t)0);          // the sentence before TELEM N is not replayed

    rSerial.Feed("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\r\n");
    tick(300, 0.0f);
    assertEqual(sink.count(), (size_t)1);
    assertEqual(sink.last().c_str(), "N:$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48");

    // Two sentences 20 ms apart: the second waits for the 50 ms slot.
    rSerial.Feed("$GPGGA,1,*01\r\n");
    tick(320, 0.0f);
    assertEqual(sink.count(), (size_t)1);
    tick(350, 0.0f);
    assertEqual(sink.count(), (size_t)2);
    assertEqual(sink.last().c_str(), "N:$GPGGA,1,*01");

    remote.HandleLine("TELEM N 0");
    sink.clear();
    rSerial.Feed("$GPGGA,2,*02\r\n");
    tick(500, 0.0f);
    assertEqual(sink.count(), (size_t)0);
}

// The whole path a real receiver takes: bytes on the port, the channel's
// checksum and parser, GuidanceSource, ImplementSprayer's speed average, a
// pump duty. The stub this suite used before could only assert the last
// two steps.
test(RemoteSprayer, vtgSentence_onThePort_drivesTheDose) {
    rReset();
    rImpl.outputs[2].pwm = true;
    rIface.analogInputs[0].value = 2048;
    remote.HandleLine("TELEM N 1");
    sink.clear();

    // Prime the rolling speed average with the same fix repeated at
    // SPEED_AVG_SAMPLES distinct earlier timestamps: updateSpeed() only
    // folds a sample in on a genuinely new VTG fix (real receiver
    // behaviour -- see ImplementSprayer::updateSpeed()), so the single
    // sentence fed below would otherwise only fill one of five slots.
    for (unsigned long ms = 1000 - (SPEED_AVG_SAMPLES - 1); ms < 1000; ++ms) {
        millisValue(ms);
        rSerial.Feed("$GPVTG,213.4,T,,M,002.91,N,005.39,K*61\r\n");
        rChannel.Update();
        rImpl.Update();
    }

    millisValue(1000);
    rSerial.Feed("$GPVTG,213.4,T,,M,002.91,N,005.39,K*61\r\n");   // 2.91 kt = 1.497 m/s
    rChannel.Update();
    assertNear(rGps.GetSpeedMs(), 1.497f, 0.001f);
    assertEqual(rGps.GetVtgTimestamp(), (unsigned long)1000);
    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) rImpl.Update();
    assertMore(rImpl.outputs[2].value, (unsigned int)0);

    // The same sentence also reaches the app through the tap.
    remote.Update();
    assertEqual(sink.last().c_str(), "N:$GPVTG,213.4,T,,M,002.91,N,005.39,K*61");

    // A corrupted copy changes nothing: no fix stamp, no speed.
    millisValue(1100);
    rSerial.Feed("$GPVTG,213.4,T,,M,009.99,N,005.39,K*61\r\n");
    rChannel.Update();
    assertNear(rGps.GetSpeedMs(), 1.497f, 0.001f);
    assertEqual(rGps.GetVtgTimestamp(), (unsigned long)1000);
}

test(RemoteSprayer, nmea_offAfterDisconnect) {
    rReset();
    remote.HandleLine("TELEM N 1");
    assertTrue(remote.NmeaEnabled());
    remote.OnDisconnect();
    assertFalse(remote.NmeaEnabled());
}

test(RemoteSprayer, gps_lineFormatAndRate) {
    rReset();
    rGps.SetQuality(4);
    remote.HandleLine("TELEM G 1");
    sink.clear();

    millisValue(400);
    rGps.SetPosition(52.5f, 6.25f);   // stamps the GGA fix at 400
    for (unsigned long t = 500; t <= 2000; t += 100) tick(t, 1.0f);
    assertEqual(sink.count(), (size_t)2);              // at 1000 and 2000
    assertEqual(sink.at(0).c_str(), "G:4,52.500000,6.250000,600");
    assertEqual(sink.at(1).c_str(), "G:4,52.500000,6.250000,1600");
}

test(RemoteSprayer, gps_noFixEver_agesMinusOne) {
    rReset();
    remote.HandleLine("TELEM G 1");
    sink.clear();
    tick(1000, 0.0f);
    assertEqual(sink.last().c_str(), "G:1,0.000000,0.000000,-1");
}

test(RemoteSprayer, telem_argsChecked) {
    rReset();
    remote.HandleLine("TELEM X 1");
    assertEqual(sink.last().c_str(), "ERR:args");
    remote.HandleLine("TELEM S 2");
    assertEqual(sink.last().c_str(), "ERR:args");
    assertFalse(remote.StatusEnabled());
}
