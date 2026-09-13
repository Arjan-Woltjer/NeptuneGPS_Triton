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
#include "RemoteSprayer.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SPRAYER_VERSION is a bare numeric token (0.2); two-step stringify.
#define RS_STR2(x) #x
#define RS_STR(x)  RS_STR2(x)

namespace triton
{

namespace {
constexpr int kMaxArgs      = 6;
constexpr int kMaxDoseLHA   = 10000;   // sanity cap on an entered dose
constexpr int kMaxFlowMlMin = 4000;    // same bound the serial wizard enforces

constexpr const char* kKeyWidth = "width_cm";
constexpr const char* kKeyGuid  = "guid_ms";
constexpr const char* kKeyBaud  = "gps_baud";
constexpr const char* kKeyMinQ  = "gps_minq";

// Fixed-size scratch for one reply line. Long enough for the widest line
// (the status line at full-scale values) with room to spare.
constexpr int kReplyLength = 96;
}  // namespace

RemoteSprayer::RemoteSprayer(ImplementSprayer* impl, ConfigSprayer* config, RemoteSink* sink)
    : impl(impl), config(config), sink(sink),
      statusEnabled(false), gpsEnabled(false), nmeaEnabled(false),
      lastStatusAt(0), lastGpsAt(0), lastNmeaAt(0), lastNmeaSeq(0),
      runWasActive(false), lastReportedSeconds(0), stagedPwmCount(0) {
    stageFromLive();
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void RemoteSprayer::HandleLine(const char* line) {
    if (line == nullptr) { err("unknown"); return; }

    // Tokenise a private copy; the caller's buffer is left alone.
    char  buf[kMaxLineLength];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char* argv[kMaxArgs];
    int   argc = 0;
    char* p    = buf;
    while (*p != 0 && argc < kMaxArgs) {
        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (*p == 0) break;
        argv[argc++] = p;
        while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\r') ++p;
        if (*p != 0) *p++ = 0;
    }
    if (argc == 0) { err("unknown"); return; }

    const char* cmd = argv[0];
    if      (strcmp(cmd, "PING")  == 0) ok();
    else if (strcmp(cmd, "INFO")  == 0) { sendInfo(); ok(); }
    else if (strcmp(cmd, "CAL")   == 0) handleCal(argc, argv);
    else if (strcmp(cmd, "PWM")   == 0) handlePwm(argc, argv);
    else if (strcmp(cmd, "CFG")   == 0) handleCfg(argc, argv);
    else if (strcmp(cmd, "TELEM") == 0) handleTelem(argc, argv);
    else err("unknown");
}

void RemoteSprayer::Update() {
    serviceRunCountdown();

    const unsigned long now = millis();
    if (statusEnabled && now - lastStatusAt >= kStatusIntervalMs) {
        lastStatusAt = now;
        sendStatus();
    }
    if (gpsEnabled && now - lastGpsAt >= kGpsIntervalMs) {
        lastGpsAt = now;
        sendGps();
    }
    if (nmeaEnabled && impl->gps->GetSentenceSeq() != lastNmeaSeq && now - lastNmeaAt >= kNmeaMinIntervalMs) {
        lastNmeaAt  = now;
        lastNmeaSeq = impl->gps->GetSentenceSeq();
        sendNmea();
    }
}

void RemoteSprayer::OnConnect() {
    sendInfo();
}

// The board is standalone; the app only adds a GUI. Losing it gives
// calibration back (which ends any pump run, because the button logic takes
// the outputs again on the next cycle) and stops telemetry. Nothing else.
void RemoteSprayer::OnDisconnect() {
    impl->ReleaseCalibration(CalibrationOwner::Remote);
    statusEnabled = false;
    gpsEnabled    = false;
    nmeaEnabled   = false;
    runWasActive  = false;   // no R:0 into a dead link
    lastReportedSeconds = 0;
}

// ---------------------------------------------------------------------------
// CAL
// ---------------------------------------------------------------------------

bool RemoteSprayer::ownsCalibration() const {
    return impl->GetCalibrationOwner() == CalibrationOwner::Remote;
}

void RemoteSprayer::stageFromLive() {
    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) stagedDose[i] = impl->doseCalibrationPoints[i];
    for (int i = 0; i < MAX_PWM_CAL_POINTS; ++i)  stagedPwm[i]  = impl->pwmCalibrationPoints[i];
    stagedPwmCount = impl->numPwmCalibrationPoints;
}

void RemoteSprayer::handleCal(int argc, const char* const argv[]) {
    if (argc < 2) { err("args"); return; }
    const char* sub = argv[1];

    if (strcmp(sub, "GET") == 0) {
        sendCalibration();
        ok();
        return;
    }

    if (strcmp(sub, "MODE") == 0) {
        if (argc < 3) { err("args"); return; }
        if (strcmp(argv[2], "1") == 0) {
            if (ownsCalibration()) { ok(); return; }   // already ours: keep staged edits
            if (!impl->AcquireCalibration(CalibrationOwner::Remote)) { reply("BUSY"); return; }
            stageFromLive();
            ok();
        } else if (strcmp(argv[2], "0") == 0) {
            impl->ReleaseCalibration(CalibrationOwner::Remote);
            ok();
        } else {
            err("args");
        }
        return;
    }

    // Everything below edits the staged tables or commits them.
    if (!ownsCalibration()) { err("mode"); return; }

    if (strcmp(sub, "DOSE") == 0) {
        long i, analog, dose;
        if (argc < 5 || !parseInt(argv[2], &i) || !parseInt(argv[3], &analog) || !parseInt(argv[4], &dose)) {
            err("args"); return;
        }
        if (i < 0 || i >= NUM_DOSE_CAL_POINTS || analog < 0 || analog > PWM_MAX_DUTY
                || dose <= 0 || dose > kMaxDoseLHA) {
            err("range"); return;
        }
        stagedDose[i].analogValue = (int)analog;
        stagedDose[i].dose        = (int)dose;
        ok();
        return;
    }

    if (strcmp(sub, "PWM") == 0) {
        long i, pwm, flow;
        if (argc < 5 || !parseInt(argv[2], &i) || !parseInt(argv[3], &pwm) || !parseInt(argv[4], &flow)) {
            err("args"); return;
        }
        // Flow 0 is legitimate: the default table starts at {0 ml/min, duty 0}
        // and the wizard's first captured point can be the pump's start.
        if (i < 0 || i >= MAX_PWM_CAL_POINTS || pwm < 0 || pwm > PWM_MAX_DUTY
                || flow < 0 || flow > kMaxFlowMlMin) {
            err("range"); return;
        }
        stagedPwm[i].pwm       = (int)pwm;
        stagedPwm[i].flowMlMin = (int)flow;
        ok();
        return;
    }

    if (strcmp(sub, "PWMN") == 0) {
        long n;
        if (argc < 3 || !parseInt(argv[2], &n)) { err("args"); return; }
        if (n < 2 || n > MAX_PWM_CAL_POINTS) { err("range"); return; }
        stagedPwmCount = (uint8_t)n;
        ok();
        return;
    }

    if (strcmp(sub, "SAVE") == 0) {
        // Validate the whole staged set before touching the live tables.
        for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
            if (stagedDose[i].dose <= 0) { err("range"); return; }
        }
        if (stagedPwmCount < 2) { err("range"); return; }
        for (int i = 0; i < stagedPwmCount; ++i) {
            if (stagedPwm[i].flowMlMin < 0 || stagedPwm[i].flowMlMin > kMaxFlowMlMin
                    || stagedPwm[i].pwm < 0 || stagedPwm[i].pwm > PWM_MAX_DUTY) {
                err("range"); return;
            }
        }

        // Same ordering the serial wizard produces: dose ascending by analog
        // value (interpolation handles either direction, but the wizard sorts
        // and so does this), pump curve ascending by flow (assumed by
        // calculatePWMValues' segment search).
        for (int i = 0; i < NUM_DOSE_CAL_POINTS - 1; ++i) {
            for (int j = i + 1; j < NUM_DOSE_CAL_POINTS; ++j) {
                if (stagedDose[j].analogValue < stagedDose[i].analogValue) {
                    DoseCalibrationPoint t = stagedDose[i]; stagedDose[i] = stagedDose[j]; stagedDose[j] = t;
                }
            }
        }
        for (int i = 0; i < stagedPwmCount - 1; ++i) {
            for (int j = i + 1; j < stagedPwmCount; ++j) {
                if (stagedPwm[j].flowMlMin < stagedPwm[i].flowMlMin) {
                    PwmCalibrationPoint t = stagedPwm[i]; stagedPwm[i] = stagedPwm[j]; stagedPwm[j] = t;
                }
            }
        }

        for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) impl->doseCalibrationPoints[i] = stagedDose[i];
        for (int i = 0; i < stagedPwmCount; ++i)      impl->pwmCalibrationPoints[i]  = stagedPwm[i];
        impl->numPwmCalibrationPoints = stagedPwmCount;
        impl->SaveCalibration();
        ok();
        return;
    }

    err("unknown");
}

// ---------------------------------------------------------------------------
// PWM
// ---------------------------------------------------------------------------

void RemoteSprayer::handlePwm(int argc, const char* const argv[]) {
    if (argc < 2) { err("args"); return; }
    if (!ownsCalibration()) { err("mode"); return; }
    const char* sub = argv[1];

    if (strcmp(sub, "SET") == 0) {
        long duty;
        if (argc < 3 || !parseInt(argv[2], &duty)) { err("args"); return; }
        if (duty < 0 || duty > PWM_MAX_DUTY) { err("range"); return; }
        if (impl->CalibrationRunActive()) { err("running"); return; }
        impl->SetCalibrationPWM(2, (int)duty);
        ok();
        return;
    }

    if (strcmp(sub, "RUN") == 0) {
        long duty;
        long seconds = (long)(kDefaultRunMs / 1000UL);
        if (argc < 3 || !parseInt(argv[2], &duty)) { err("args"); return; }
        if (argc >= 4 && !parseInt(argv[3], &seconds)) { err("args"); return; }
        if (duty < 0 || duty > PWM_MAX_DUTY || seconds < 1
                || (unsigned long)seconds * 1000UL > ImplementSprayer::kCalibrationRunMaxMs) {
            err("range"); return;
        }
        if (impl->CalibrationRunActive()) { err("running"); return; }
        if (!impl->StartCalibrationRun((int)duty, (unsigned long)seconds * 1000UL)) { err("mode"); return; }
        ok();
        runWasActive        = false;   // force the first R: line out right away
        lastReportedSeconds = 0;
        serviceRunCountdown();
        return;
    }

    if (strcmp(sub, "STOP") == 0) {
        if (impl->CalibrationRunActive()) {
            impl->StopCalibrationRun();
            serviceRunCountdown();     // emits R:0
        }
        ok();
        return;
    }

    err("unknown");
}

// ---------------------------------------------------------------------------
// CFG
// ---------------------------------------------------------------------------

void RemoteSprayer::handleCfg(int argc, const char* const argv[]) {
    if (argc < 2) { err("args"); return; }
    const char* sub = argv[1];

    if (strcmp(sub, "GET") == 0) {
        sendConfig();
        ok();
        return;
    }

    if (strcmp(sub, "SET") == 0) {
        if (argc < 3) { err("args"); return; }
        const char* key = argv[2];
        const bool known = strcmp(key, kKeyWidth) == 0 || strcmp(key, kKeyGuid) == 0
                        || strcmp(key, kKeyBaud)  == 0 || strcmp(key, kKeyMinQ) == 0;
        if (!known) { err("key"); return; }

        long value;
        if (argc < 4 || !parseInt(argv[3], &value)) { err("args"); return; }

        bool applied = false;
        if      (strcmp(key, kKeyWidth) == 0) applied = config->SetWidthCm((int)value);
        else if (strcmp(key, kKeyGuid)  == 0) applied = value >= 0 && config->SetGuidanceTimeoutMs((unsigned long)value);
        else if (strcmp(key, kKeyBaud)  == 0) applied = value >= 0 && value <= 255 && config->SetGpsBaudIndex((uint8_t)value);
        else if (strcmp(key, kKeyMinQ)  == 0) applied = value >= 0 && value <= 255 && config->SetGpsMinQuality((uint8_t)value);

        if (!applied) { err("range"); return; }
        config->Save();
        ok();
        return;
    }

    err("unknown");
}

// ---------------------------------------------------------------------------
// TELEM
// ---------------------------------------------------------------------------

void RemoteSprayer::handleTelem(int argc, const char* const argv[]) {
    if (argc < 3) { err("args"); return; }
    bool on;
    if      (strcmp(argv[2], "1") == 0) on = true;
    else if (strcmp(argv[2], "0") == 0) on = false;
    else { err("args"); return; }

    const unsigned long now = millis();
    if (strcmp(argv[1], "S") == 0) {
        statusEnabled = on;
        lastStatusAt  = now;
    } else if (strcmp(argv[1], "G") == 0) {
        gpsEnabled = on;
        lastGpsAt  = now;
    } else if (strcmp(argv[1], "N") == 0) {
        nmeaEnabled = on;
        lastNmeaSeq = impl->gps->GetSentenceSeq();   // only sentences from now on
        lastNmeaAt  = now;
    } else {
        err("args");
        return;
    }
    ok();
}

// ---------------------------------------------------------------------------
// Output lines
// ---------------------------------------------------------------------------

void RemoteSprayer::sendInfo() {
    char line[kReplyLength];
    snprintf(line, sizeof(line), "V:" RS_STR(SPRAYER_VERSION) ",%d", (int)kProtocolVersion);
    reply(line);
}

void RemoteSprayer::sendCalibration() {
    char line[kReplyLength];
    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
        snprintf(line, sizeof(line), "C:D,%d,%d,%d", i,
                 impl->doseCalibrationPoints[i].analogValue, impl->doseCalibrationPoints[i].dose);
        reply(line);
    }
    for (int i = 0; i < impl->numPwmCalibrationPoints; ++i) {
        snprintf(line, sizeof(line), "C:P,%d,%d,%d", i,
                 impl->pwmCalibrationPoints[i].pwm, impl->pwmCalibrationPoints[i].flowMlMin);
        reply(line);
    }
}

void RemoteSprayer::sendConfig() {
    const SprayerSettings& s = config->Get();
    char line[kReplyLength];
    snprintf(line, sizeof(line), "K:%s,%d",  kKeyWidth, s.widthCm);                    reply(line);
    snprintf(line, sizeof(line), "K:%s,%lu", kKeyGuid,  s.guidanceTimeoutMs);          reply(line);
    snprintf(line, sizeof(line), "K:%s,%u",  kKeyBaud,  (unsigned)s.gpsBaudIndex);     reply(line);
    snprintf(line, sizeof(line), "K:%s,%u",  kKeyMinQ,  (unsigned)s.gpsMinQuality);    reply(line);
}

// S:<speed>,<req>,<act>,<flow>,<raw>,<mixer>,<vern>,<pump>,<pumpPwm>,<dev>,<cal>,<in1..4>,<out1..4>
// While someone holds calibration the pump duty shown is the calibration
// duty, since that is what actually reaches the pump then.
void RemoteSprayer::sendStatus() {
    const CalibrationOwner owner = impl->GetCalibrationOwner();
    const int pumpPwm = (owner != CalibrationOwner::None)
                      ? impl->GetCalibrationDuty()
                      : (int)impl->outputs[2].value;
    const int raw = impl->interface->GetAnalogInputs()[0].value;
    const DigitalInputState* in = impl->interface->GetDigitalInputs();

    char line[kReplyLength];
    snprintf(line, sizeof(line), "S:%.2f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%d,%d,%d,%d%d%d%d,%d%d%d%d",
             (double)impl->speed,
             (double)impl->doseLHA,
             (double)impl->actualLHA,
             (double)(impl->doseLM * 1000.0f),
             raw,
             impl->outputs[0].state ? 1 : 0,
             impl->outputs[1].state ? 1 : 0,
             impl->outputs[2].state ? 1 : 0,
             pumpPwm,
             impl->doseDeviation ? 1 : 0,
             (int)owner,
             in[0].state ? 1 : 0, in[1].state ? 1 : 0, in[2].state ? 1 : 0, in[3].state ? 1 : 0,
             impl->outputs[0].state ? 1 : 0, impl->outputs[1].state ? 1 : 0,
             impl->outputs[2].state ? 1 : 0, impl->outputs[3].state ? 1 : 0);
    reply(line);
}

// N:<sentence>, the receiver's last complete line as it came in. For
// debugging the receiver from the app; the parser is not involved.
void RemoteSprayer::sendNmea() {
    char line[kReplyLength];
    snprintf(line, sizeof(line), "N:%s", impl->gps->GetLastSentence());
    reply(line);
}

// G:<quality>,<lat>,<lon>,<fixAgeMs>; age -1 until the first position fix.
void RemoteSprayer::sendGps() {
    float lat = 0.0f, lon = 0.0f;
    impl->gps->GetPosition(&lat, &lon);
    const unsigned long fixAt = impl->gps->GetGgaFixAge();
    const long ageMs = (fixAt == 0) ? -1L : (long)(millis() - fixAt);

    char line[kReplyLength];
    snprintf(line, sizeof(line), "G:%d,%.6f,%.6f,%ld",
             (int)impl->gps->GetQuality(), (double)lat, (double)lon, ageMs);
    reply(line);
}

// R:<seconds remaining> whenever the (rounded-up) second changes, R:0 once
// the board has ended the run. The board's own timer is the only clock here.
void RemoteSprayer::serviceRunCountdown() {
    const bool active = impl->CalibrationRunActive();
    if (active) {
        const unsigned long seconds = (impl->CalibrationRunRemainingMs() + 999UL) / 1000UL;
        if (!runWasActive || seconds != lastReportedSeconds) {
            char line[kReplyLength];
            snprintf(line, sizeof(line), "R:%lu", seconds);
            reply(line);
            lastReportedSeconds = seconds;
        }
        runWasActive = true;
    } else if (runWasActive) {
        reply("R:0");
        runWasActive        = false;
        lastReportedSeconds = 0;
    }
}

void RemoteSprayer::err(const char* reason) {
    char line[kReplyLength];
    snprintf(line, sizeof(line), "ERR:%s", reason);
    reply(line);
}

bool RemoteSprayer::parseInt(const char* s, long* out) {
    if (s == nullptr || *s == 0) return false;
    char* end = nullptr;
    const long v = strtol(s, &end, 10);
    if (end == s || *end != 0) return false;
    *out = v;
    return true;
}

}  // namespace triton
