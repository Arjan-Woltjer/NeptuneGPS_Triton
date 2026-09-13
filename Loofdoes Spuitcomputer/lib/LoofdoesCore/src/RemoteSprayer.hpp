/*
  RemoteSprayer - transport-independent command and telemetry layer for the MeijWorks loofdoes
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
#pragma once

#include "ConfigSprayer.hpp"
#include "ImplementSprayer.hpp"

namespace triton
{

// Where RemoteSprayer's output lines go: one call per line, no terminator.
// The BLE link notifies each line; a test captures them.
class RemoteSink {
public:
    virtual ~RemoteSink() {}
    virtual void WriteLine(const char* line) = 0;
};

// The companion app's view of the sprayer (NeptuneGPS_Triton#46/#47). Takes
// one ASCII command line at a time and answers with ASCII lines; emits
// periodic status while asked to. Knows nothing about BLE -- the link feeds
// HandleLine() and calls OnConnect()/OnDisconnect().
//
// Commands (app -> board)             Replies (board -> app)
//   PING                                OK
//   INFO                                V:<fw>,<proto>  OK
//   CAL GET                             C:D,<i>,<analog>,<dose> x3  C:P,<i>,<pwm>,<flow> xn  OK
//   CAL MODE 1|0                        OK | BUSY          (1 takes calibration, 0 gives it back)
//   CAL DOSE <i> <analog> <lha>         OK | ERR:...       (staged until CAL SAVE)
//   CAL PWM <i> <pwm> <flow>            OK | ERR:...       (staged)
//   CAL PWMN <n>                        OK | ERR:...       (number of pwm points, staged)
//   CAL SAVE                            OK | ERR:...       (validate, sort, apply, persist)
//   PWM SET <duty>                      OK | ERR:...       (pump duty while calibrating)
//   PWM RUN <duty> [seconds]            OK then R:<s>...R:0 (firmware-timed, 60 s max)
//   PWM STOP                            OK
//   CFG GET                             K:<key>,<value> x5  OK
//   CFG SET <key> <value>               OK | ERR:...       (persisted at once)
//   TELEM S 1|0 / TELEM G 1|0 / TELEM N 1|0   OK
//
// Periodic (board -> app), while enabled
//   S:<speed>,<req>,<act>,<flow>,<raw>,<mixer>,<vern>,<pump>,<pumpPwm>,<dev>,<cal>,<in1..4>,<out1..4>
//     (protocol 2 appended the last two: four input bits IN1..IN4 and four
//      output bits OUT1..OUT4, each as a 4-character field of 0/1)
//   G:<quality>,<lat>,<lon>,<fixAgeMs>
//   N:<sentence>            each GPS sentence as received, while TELEM N is on
//
// Every command answers OK, BUSY or ERR:<reason>; reason is one word:
// args, range, mode, key, running, unknown.
//
// The board stays fully standalone: OnDisconnect() only gives calibration
// back (which ends any pump run, because normal output logic resumes) and
// stops telemetry. Nothing else changes when the app goes away.
class RemoteSprayer {
public:
    static constexpr uint8_t       kProtocolVersion   = 2;
    static constexpr unsigned long kNmeaMinIntervalMs = 50;   // at most 20 sentences/s over the link
    static constexpr unsigned long kStatusIntervalMs  = 200;
    static constexpr unsigned long kGpsIntervalMs     = 1000;
    static constexpr unsigned long kDefaultRunMs      = ImplementSprayer::kCalibrationRunMaxMs;
    static constexpr int           kMaxLineLength     = 96;

    RemoteSprayer(ImplementSprayer* impl, ConfigSprayer* config, RemoteSink* sink);

    // One command without its line terminator. Safe to call with anything.
    void HandleLine(const char* line);

    // Call every loop iteration after ImplementSprayer::Update().
    void Update();

    void OnConnect();
    void OnDisconnect();

    bool StatusEnabled() const { return statusEnabled; }
    bool GpsEnabled() const    { return gpsEnabled; }
    bool NmeaEnabled() const   { return nmeaEnabled; }

private:
    ImplementSprayer* impl;
    ConfigSprayer*    config;
    RemoteSink*       sink;

    bool          statusEnabled;
    bool          gpsEnabled;
    bool          nmeaEnabled;
    unsigned long lastStatusAt;
    unsigned long lastGpsAt;
    unsigned long lastNmeaAt;
    uint32_t      lastNmeaSeq;

    // Countdown bookkeeping so R: lines only go out when the second changes.
    bool          runWasActive;
    unsigned long lastReportedSeconds;

    // Calibration edits are staged here from CAL MODE 1 until CAL SAVE, so a
    // half-finished wizard that loses the link leaves the live tables alone.
    DoseCalibrationPoint stagedDose[NUM_DOSE_CAL_POINTS];
    PwmCalibrationPoint  stagedPwm[MAX_PWM_CAL_POINTS];
    uint8_t              stagedPwmCount;

    bool ownsCalibration() const;
    void stageFromLive();

    void handleCal(int argc, const char* const argv[]);
    void handlePwm(int argc, const char* const argv[]);
    void handleCfg(int argc, const char* const argv[]);
    void handleTelem(int argc, const char* const argv[]);

    void sendInfo();
    void sendCalibration();
    void sendConfig();
    void sendStatus();
    void sendGps();
    void sendNmea();
    void serviceRunCountdown();

    void reply(const char* line) { sink->WriteLine(line); }
    void ok()                    { reply("OK"); }
    void err(const char* reason);

    static bool parseInt(const char* s, long* out);
};

}  // namespace triton
