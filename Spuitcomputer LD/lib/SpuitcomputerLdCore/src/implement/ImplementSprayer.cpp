/*
  ImplementSprayer - a library for the MeijWorks loofdoes implement
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
#include "ImplementSprayer.hpp"

#ifdef ARDUINO
#include <Preferences.h>
#include "driver/ledc.h"
#endif

namespace triton
{

namespace {
// NaN is the case that matters here: it compares false against everything,
// itself included, so it slips through range clamps instead of being caught by
// them. Written out rather than using isfinite() from <math.h>, which is not
// available in the native test build's Arduino stubs and whose header collides
// with Arduino.h's min/max macros.
inline bool GIsFinite(float v) {
    return (v == v) && (v < 3.0e38f) && (v > -3.0e38f);
}
}  // namespace


ImplementSprayer::ImplementSprayer(Stream* serialDebug, GuidanceSource* guidance,
                                   InterfaceSprayer* interface, ConfigSprayer* config)
    : serialDebug(serialDebug), guidance(guidance), interface(interface), config(config),
      speed(0), width((float)config->Get().widthCm), doseLHA(0.0f), doseLM(0.0f) {
#ifdef DEBUG
    serialDebug->println(S_DIVIDE);
    serialDebug->println("Initialising sprayer implement");
    serialDebug->println(S_DIVIDE);
#endif

    speedBufIdx = 0;
    lastVtgSeen = 0;
    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) speedBuf[i] = 0.0f;

    // outputs[] is already fully initialized by the in-class member initializer
    // in ImplementSprayer.h (including which channels are calibration-driven
    // via .pwm) — do not reset it here, that would stomp that configuration.

#ifdef ARDUINO
    // One shared timer for all output channels at 1 kHz, 12-bit resolution (0-4095).
    ledc_timer_config_t timerCfg = {};
    timerCfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    timerCfg.duty_resolution = LEDC_TIMER_12_BIT;
    timerCfg.timer_num       = LEDC_TIMER_0;
    timerCfg.freq_hz         = PWM_FREQ_HZ;
    timerCfg.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&timerCfg);

    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        ledc_channel_config_t chCfg = {};
        chCfg.gpio_num   = outputs[i].pin;
        chCfg.speed_mode = LEDC_LOW_SPEED_MODE;
        chCfg.channel    = (ledc_channel_t)outputs[i].ledcChannel;
        chCfg.intr_type  = LEDC_INTR_DISABLE;
        chCfg.timer_sel  = LEDC_TIMER_0;
        chCfg.duty       = PWM_MAX_DUTY;  // Active-low: start high = output off
        chCfg.hpoint     = 0;
        ledc_channel_config(&chCfg);
    }
#endif
}

void ImplementSprayer::Update() {
    // Settings can change at runtime (serial CLI, companion app), so the
    // geometry is re-read every cycle rather than captured once.
    width = (float)config->Get().widthCm;

    serviceCalibrationRun();
    updateInputs();
    updateSpeed();

    calculateDoseLHA();
    calculateDoseLM();
    calculatePWMValues(2);  // pump output

    updateOutputs();
    updateDeviation();
}

void ImplementSprayer::updateInputs() {
    // Set each button pointer to the corresponding slot in the interface's button array
    DigitalInputState* digitalBase = interface->GetDigitalInputs();
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        buttons[i] = &digitalBase[i];
    }

    AnalogInputState* analogBase = interface->GetAnalogInputs();
    for (int i = 0; i < NUM_ANALOG_IN; ++i) {
        inputAnalog[i] = &analogBase[i];
    }
}

// GuidanceSource's speed is only overwritten when a fresh, checksum-valid
// message arrives -- it is never invalidated. Without this check, losing the antenna or
// the fix left the last known speed latched forever, and the sprayer went on
// dosing from it. Stopping the tractor at that point kept the pump injecting
// onto one stationary spot.
//
// Note the getter returns an absolute timestamp, not an age, despite the name.
// It also starts at 0, so "no message since boot" has to be distinguished from
// a genuine fix rather than read as a very recent one.
//
// The fix-quality floor lives here too: a fix the operator has said not to
// trust is treated exactly like no fix, so every fail-closed path that
// already handles stale guidance covers it without a second set of checks.
bool ImplementSprayer::guidanceStale() const {
    const unsigned long lastFix = guidance->GetVtgTimestamp();
    if (lastFix == 0) {
        return true;
    }
    if ((millis() - lastFix) > config->Get().guidanceTimeoutMs) {
        return true;
    }
    // NeptuneGPS_Triton#61 field test: with the antenna pulled, the ATGM336H
    // kept reporting plausible-looking GGA/VTG fixes -- quality 1, a
    // real-looking Doppler speed -- almost the whole time, so neither check
    // above caught it. Its own antenna-supervisor message (NmeaParser ->
    // GuidanceSource::SetAntennaOk()) tracked the actual antenna state
    // reliably where the fix itself didn't.
    if (!guidance->GetAntennaOk()) {
        return true;
    }
    return !config->GuidanceQualityOk(guidance->GetQuality());
}

void ImplementSprayer::updateSpeed() {
    if (guidanceStale()) {
        // Drain the moving average too, so speed does not creep back up from
        // stale samples when a fix returns.
        for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) {
            speedBuf[i] = 0.0f;
        }
        lastVtgSeen = 0;
        speed       = 0.0f;
        return;
    }

    // Fold in a new sample only once a genuinely new VTG fix has arrived.
    // Update() runs free-running (main.cpp's loop() has no fixed timestep),
    // so calling it several hundred times between a 1 Hz receiver's fixes
    // used to shift the *same* speed into every slot long before the next
    // fix showed up -- "5-sample average" was really 5 copies of whatever
    // the last fix said, not 5 fixes spread over roughly a second.
    const unsigned long fixAt = guidance->GetVtgTimestamp();
    if (fixAt != lastVtgSeen) {
        lastVtgSeen = fixAt;
        speedBuf[speedBufIdx] = guidance->GetSpeedMs();
        speedBufIdx = (speedBufIdx + 1) % SPEED_AVG_SAMPLES;
    }

    // Recomputed from the buffer every call rather than kept as a running
    // sum: an incremental sum of repeated zeros drifted to a small non-zero
    // (occasionally negative, printing as "-0.00" on the status line) as
    // float rounding accumulated over millions of updates.
    float sum = 0.0f;
    for (int i = 0; i < SPEED_AVG_SAMPLES; ++i) sum += speedBuf[i];
    speed = sum / SPEED_AVG_SAMPLES;
}

void ImplementSprayer::calculateDoseLHA() {
    // Example: dose = speed (m/s) * width (cm) * 0.1 (calibration factor)
    inputAnalog[0]->value = min(max(inputAnalog[0]->value, 0), 4095); // Ensure raw value is within expected range

    byte i = 0;

    // determine which calibration points to use for interpolation based on the input analog value
    if (doseCalibrationPoints[0].analogValue < doseCalibrationPoints[1].analogValue) {
        if (inputAnalog[0]->value < doseCalibrationPoints[1].analogValue) {
            i = 1;
        }
        else {
            i = 2;
        }
    }
    else {
        if (inputAnalog[0]->value < doseCalibrationPoints[1].analogValue) {
            i = 2;
        }
        else {
            i = 1;
        }
    }

    // Interpolate calibration data
    float a = inputAnalog[0]->value            - doseCalibrationPoints[i - 1].analogValue;
    float b = doseCalibrationPoints[i].analogValue - doseCalibrationPoints[i - 1].analogValue;
    float c = doseCalibrationPoints[i].dose        - doseCalibrationPoints[i - 1].dose;
    float d = doseCalibrationPoints[i - 1].dose;

    // b is zero whenever two adjacent calibration points share an analogValue,
    // which a seized or disconnected potentiometer produces directly: the
    // capture step records whatever the ADC reads without checking the points
    // differ. With a zero too, that is 0.0f/0.0f -- NaN, which then defeats
    // every comparison downstream rather than being caught by them.
    if (b == 0.0f) {
        doseLHA = d;
    } else {
        doseLHA = ((a * c) / b) + d;
    }

    if (!GIsFinite(doseLHA)) {
        doseLHA = 0.0f;
    }

    // For this example, we will just print the calculated dose to the debug stream
#ifdef DEBUG
    if (serialDebug) {
        serialDebug->print("Calculated dose: ");
        serialDebug->println(doseLHA);
    }
#endif
}

void ImplementSprayer::calculateDoseLM() {
    doseLM = (doseLHA * speed * width * 60) / (100 * 10000); // Convert from l/ha to l/min based on speed (m/s) and width (cm)
}

float ImplementSprayer::flowToLHA(float flowMlMin) const {
    // doseLM [l/min] = doseLHA * speed * width * 60 / 1e6, and flow is doseLM
    // in ml. Callers guarantee speed > 0; width is a fixed positive geometry.
    const float perLHA = speed * width * 60.0f / 1000.0f;  // ml/min per l/ha
    if (perLHA <= 0.0f) return kActualDoseUndefined;
    return flowMlMin / perLHA;
}

void ImplementSprayer::calculatePWMValues(byte outputIndex) {
    // Recomputed from scratch every cycle; every early return below is a
    // case where the pump is not dosing, so there is no actual dose either.
    actualLHA = kActualDoseUndefined;

    if (!outputs[outputIndex].pwm) return;

    // Fewer than two points means no usable curve. Returning here left the pump
    // at whatever duty was last computed, and updateOutputs() goes on reapplying
    // that value every cycle -- so a bad restore from NVS ran the pump at a
    // fixed rate with dose control silently switched off. Fail closed instead.
    //
    // Stale guidance lands in the same place: without a speed there is no dose
    // to compute, so the pump stops rather than coasting on the last figure.
    if (numPwmCalibrationPoints < 2 || guidanceStale()) {
        outputs[outputIndex].value = 0;
        return;
    }

    float doseMlMin = doseLM * 1000.0f;

    // NaN fails the shutoff comparison below and both clamps further down, since
    // every comparison against it is false. Catch it before any of them.
    if (!GIsFinite(doseMlMin)) {
        outputs[outputIndex].value = 0;
        return;
    }

    // Standing still: no demand, so the pump is off and there is no dose to
    // compare against. Left undefined rather than 0 on purpose -- 0 is what
    // the app shows for "pump cut while moving", which needs the driver to
    // react, and this does not. GPS creep while parked lands here too.
    if (speed < kStandstillSpeedMs) {
        outputs[outputIndex].value = 0;
        return;
    }

    // Requested dose is below the pump's lowest calibrated flow point -- stop it
    // rather than extrapolating below the calibrated range (which could produce
    // a bogus, even negative, PWM value). The actual dose is then 0; the
    // deviation flag (updateDeviation) turns that into the OUT4 warning while
    // spraying, so the driver knows to speed up.
    if (doseMlMin < (float)pwmCalibrationPoints[0].flowMlMin) {
        outputs[outputIndex].value = 0;
        actualLHA = 0.0f;
        return;
    }

    // Find the right interpolation segment (points assumed ascending by flowMlMin)
    uint8_t i = 1;
    for (uint8_t j = 1; j < numPwmCalibrationPoints - 1; ++j) {
        if (doseMlMin >= (float)pwmCalibrationPoints[j].flowMlMin) i = j + 1;
    }

    // Interpolate calibration data
    float a = doseMlMin                                    - (float)pwmCalibrationPoints[i - 1].flowMlMin;
    float b = (float)pwmCalibrationPoints[i].flowMlMin    - (float)pwmCalibrationPoints[i - 1].flowMlMin;
    float c = (float)(pwmCalibrationPoints[i].pwm         - pwmCalibrationPoints[i - 1].pwm);
    float d = (float)pwmCalibrationPoints[i - 1].pwm;

    // Calculate actual implement offset, clamped to the valid duty range — dose
    // demand above the highest calibrated point must not extrapolate past the
    // hardware's maximum duty cycle.
    if (b != 0.0f) {
        float computed = ((a * c) / b) + d;
        // isfinite() first: NaN passes both clamps untouched, and the resulting
        // (unsigned int)NaN is undefined behaviour that reaches ledc_set_duty()
        // through the unsigned subtraction in updateOutputs().
        if (!GIsFinite(computed)) {
            computed  = 0.0f;
            actualLHA = 0.0f;
        } else if (computed > (float)PWM_MAX_DUTY) {
            // Saturated: the pump runs flat out and delivers the top
            // calibrated flow, not what was asked. Report that as the
            // actual dose so the shortfall is visible in l/ha.
            computed  = (float)PWM_MAX_DUTY;
            actualLHA = flowToLHA((float)pwmCalibrationPoints[numPwmCalibrationPoints - 1].flowMlMin);
        } else if (computed < 0.0f) {
            computed  = 0.0f;
            actualLHA = 0.0f;
        } else {
            actualLHA = doseLHA;
        }
        outputs[outputIndex].value = (unsigned int)computed;
    } else {
        // Duplicate calibration points. Previously fell through leaving the
        // previous duty in place.
        outputs[outputIndex].value = 0;
        actualLHA = 0.0f;
    }

#ifdef DEBUG
    if (serialDebug) {
        serialDebug->print("Calculated PWM for output ");
        serialDebug->print(outputIndex);
        serialDebug->print(": ");
        serialDebug->println(outputs[outputIndex].value);
    }
#endif
}

void ImplementSprayer::updateOutputs() {
    if (calibrationMode) return;  // CalibrationSprayer controls outputs directly

    unsigned long now = millis();

    // ------------------------------------------------------------------------------------------------------------------------
    // Output control logic:
    // - Output 0 (mixer) turns on immediately when button 0 is held, off when released
    // - Output 1 (vernevelaar) turns on when button 1 is held and output 0 has been on for at least 1000 ms, off when released
    // - Output 2 (pump) turns on when button 2 is held and output 1 has been on for at least 1000 ms, off when released
    // - Output 3 is not button-driven; it's the dose-deviation buzzer, driven by updateDeviation()
    // ------------------------------------------------------------------------------------------------------------------------

    // Mixer control
    if (buttons[0]->state && !outputs[0].state) {
        // Mixer on
        outputs[0].state = true;
        outputs[0].timer = now;
        setOutputDuty(outputs[0], outputs[0].pwm ? PWM_MAX_DUTY - outputs[0].value : 0);
    }
    else if (!buttons[0]->state) {
        // Mixer off, ensure vernevelaar and pump are also off
        outputs[0].state = false;
        outputs[1].state = false; // Ensure vernevelaar is off if mixer is off
        outputs[2].state = false; // Ensure pump is off if mixer is off
        setOutputDuty(outputs[0], PWM_MAX_DUTY);
        setOutputDuty(outputs[1], PWM_MAX_DUTY);
        setOutputDuty(outputs[2], PWM_MAX_DUTY);
    }

    // Vernevelaar control
    if (buttons[1]->state && !outputs[1].state
            && outputs[0].state && now - outputs[0].timer >= 1000) { // Vernevelaar on after mixer has been on for 1000 ms
        // Vernevelaar on
        outputs[1].state = true;
        outputs[1].timer = now;
        setOutputDuty(outputs[1], outputs[1].pwm ? PWM_MAX_DUTY - outputs[1].value : 0);
    }
    else if (!buttons[1]->state) {
        // Vernevelaar off if button released or mixer is off
        outputs[1].state = false;
        outputs[2].state = false; // Ensure pump is off if vernevelaar is off
        setOutputDuty(outputs[1], PWM_MAX_DUTY);
        setOutputDuty(outputs[2], PWM_MAX_DUTY);
    }

    // Pump control
    if (buttons[2]->state && !outputs[2].state
            && outputs[1].state && now - outputs[1].timer >= 1000) { // Pump on after vernevelaar has been on for 1000 ms
        // Pump on
        outputs[2].state = true;
        outputs[2].timer = now;
    }
    else if (!buttons[2]->state) {
        // Pump off if button released
        outputs[2].state = false;
    }

    // Re-applied every loop (not just on the on-transition) so a live-updated
    // outputs[2].value — e.g. calculatePWMValues() clamping to 0 as GPS speed
    // drops — actually reaches the hardware instead of latching at whatever
    // duty was current the instant the button was pressed.
    if (outputs[2].state) {
        setOutputDuty(outputs[2], outputs[2].pwm ? PWM_MAX_DUTY - outputs[2].value : 0);
    } else {
        setOutputDuty(outputs[2], PWM_MAX_DUTY);
    }
}

// Outside kDoseTolerance of the requested dose, only while the pump output is
// actually on (mixer, vernevelaar and pump engaged -- no alarm on the headland
// or on the way to the field), and only after the condition has held for
// kDeviationHoldMs: speed is a moving average, so the boundary flickers during
// accelerations and a bare comparison would chatter the buzzer. The same hold
// applies to clearing. OUT4 follows the flag; the companion app reads the
// same flag from the status line so board and phone never disagree.
void ImplementSprayer::updateDeviation() {
    bool outside = false;
    if (outputs[2].state && !calibrationMode
            && doseLHA > 0.0f && actualLHA != kActualDoseUndefined) {
        float diff = actualLHA - doseLHA;
        if (diff < 0.0f) diff = -diff;
        outside = diff > kDoseTolerance * doseLHA;
    }

    const unsigned long now = millis();
    unsigned long deltaMs = now - lastDeviationUpdateAt;
    if (deltaMs > kDeviationHoldMs) deltaMs = kDeviationHoldMs;
    lastDeviationUpdateAt = now;

    if (outside != deviationPending) {
        // The instant of the flip: credit nothing yet, so a clean single
        // transition still needs a full kDeviationHoldMs after it, exactly
        // like the previous design.
        deviationPending = outside;
        deltaMs = 0;
    }

    if (outside) {
        deviationAccumMs += deltaMs;
        if (deviationAccumMs > kDeviationHoldMs) deviationAccumMs = kDeviationHoldMs;
    } else {
        deviationAccumMs = (deviationAccumMs > deltaMs) ? deviationAccumMs - deltaMs : 0;
    }

    if (deviationAccumMs >= kDeviationHoldMs) {
        doseDeviation = true;
    } else if (deviationAccumMs == 0) {
        doseDeviation = false;
    }
    // Between 0 and kDeviationHoldMs, doseDeviation keeps whatever it was.

    if (!outputs[2].state) {
        // The pump output is off -- the operator released one of the three
        // switches, or the interlock dropped them. Clear on the same pass
        // instead of decaying through the hold: the hold exists to stop the
        // boundary chattering while spraying, and letting it run here trails
        // the buzzer about a second onto the headland (NeptuneGPS_Triton#61
        // case 5, seen on the field log).
        doseDeviation    = false;
        deviationAccumMs = 0;
    }

    if (calibrationMode) {
        // The wizard owns the outputs; never sound over a calibration run,
        // and start the hold afresh once it hands the outputs back.
        doseDeviation    = false;
        deviationAccumMs = 0;
    }

    // The flag itself is unconditional: the app alarm and the status line
    // follow it. Only the pin obeys the operator's buzzer switch (#72), and
    // outputs[3].state shows what the pin does, not what was detected.
    const bool sound = doseDeviation && config->Get().buzzerEnabled;
    outputs[3].state = sound;
    setOutputDuty(outputs[3], sound ? 0 : PWM_MAX_DUTY);  // active-low
}

void ImplementSprayer::setOutputDuty(const OutputState& out, uint32_t duty) {
#ifdef ARDUINO
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)out.ledcChannel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)out.ledcChannel);
#else
    (void)out;
    (void)duty;
#endif
}

// ---------------------------------------------------------------------------
// Calibration ownership and the timed pump run. Both the serial wizard and
// the companion app go through these, so there is exactly one place that
// decides who may drive the outputs and exactly one timer that ends a run.
// ---------------------------------------------------------------------------

bool ImplementSprayer::AcquireCalibration(CalibrationOwner who) {
    if (who == CalibrationOwner::None) return false;
    if (calibrationOwner != CalibrationOwner::None && calibrationOwner != who) return false;
    if (calibrationOwner == CalibrationOwner::None) {
        // Calibration is about the pump alone. Mixer and vernevelaar go off
        // and their state is cleared, so the interlock cascade restarts from
        // scratch once calibration is handed back (NeptuneGPS_Triton#66).
        // Before this they simply kept whatever state they had, for the
        // whole of a five-run pump calibration.
        for (int i = 0; i < 3; ++i) {
            outputs[i].state = false;
            setOutputDuty(outputs[i], PWM_MAX_DUTY);
        }
        calibrationDuty = 0;
    }
    calibrationOwner = who;
    calibrationMode  = true;
    return true;
}

// Giving calibration back is the whole "wizard is gone" path, whether the
// serial menu finished or the app dropped off the air: the run ends, the
// pump duty is cleared, and updateOutputs() is back in charge on the next
// cycle. Nothing else changes -- the board never depended on the app.
void ImplementSprayer::ReleaseCalibration(CalibrationOwner who) {
    if (who == CalibrationOwner::None || calibrationOwner != who) return;
    StopCalibrationRun();
    SetCalibrationPWM(2, 0);
    calibrationOwner = CalibrationOwner::None;
    calibrationMode  = false;
}

bool ImplementSprayer::StartCalibrationRun(int duty, unsigned long durationMs) {
    if (calibrationOwner == CalibrationOwner::None) return false;
    if (duty < 0 || duty > PWM_MAX_DUTY || durationMs == 0) return false;
    if (durationMs > kCalibrationRunMaxMs) durationMs = kCalibrationRunMaxMs;
    runActive     = true;
    runStartedAt  = millis();
    runDurationMs = durationMs;
    SetCalibrationPWM(2, duty);
    return true;
}

void ImplementSprayer::StopCalibrationRun() {
    if (!runActive) return;
    runActive = false;
    SetCalibrationPWM(2, 0);
}

unsigned long ImplementSprayer::CalibrationRunRemainingMs() const {
    if (!runActive) return 0;
    const unsigned long elapsed = millis() - runStartedAt;
    return (elapsed >= runDurationMs) ? 0 : (runDurationMs - elapsed);
}

void ImplementSprayer::serviceCalibrationRun() {
    if (runActive && (millis() - runStartedAt) >= runDurationMs) {
        StopCalibrationRun();
    }
}

void ImplementSprayer::SetCalibrationPWM(byte outputIndex, int pwmValue) {
    if (outputIndex == 2) calibrationDuty = pwmValue;
    // Invert for active-low: stored value 0 = off, PWM_MAX_DUTY = full on
    setOutputDuty(outputs[outputIndex], PWM_MAX_DUTY - (uint32_t)pwmValue);
}

#ifdef ARDUINO
void ImplementSprayer::SaveCalibration() {
    Preferences prefs;
    prefs.begin("sprayer_cal", false);

    char key[8];
    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
        snprintf(key, sizeof(key), "da%d", i);
        prefs.putInt(key, doseCalibrationPoints[i].analogValue);
        snprintf(key, sizeof(key), "dd%d", i);
        prefs.putInt(key, doseCalibrationPoints[i].dose);
    }

    prefs.putUChar("pwm_n", numPwmCalibrationPoints);
    for (int i = 0; i < numPwmCalibrationPoints; ++i) {
        snprintf(key, sizeof(key), "pf%d", i);
        prefs.putInt(key, pwmCalibrationPoints[i].flowMlMin);
        snprintf(key, sizeof(key), "pp%d", i);
        prefs.putInt(key, pwmCalibrationPoints[i].pwm);
    }

    prefs.end();
}

void ImplementSprayer::LoadCalibration() {
    Preferences prefs;
    prefs.begin("sprayer_cal", true);

    if (!prefs.isKey("pwm_n")) {
        prefs.end();
        return;  // no saved calibration; keep defaults
    }

    char key[8];
    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
        snprintf(key, sizeof(key), "da%d", i);
        doseCalibrationPoints[i].analogValue = prefs.getInt(key, doseCalibrationPoints[i].analogValue);
        snprintf(key, sizeof(key), "dd%d", i);
        doseCalibrationPoints[i].dose = prefs.getInt(key, doseCalibrationPoints[i].dose);
    }

    numPwmCalibrationPoints = prefs.getUChar("pwm_n", numPwmCalibrationPoints);
    if (numPwmCalibrationPoints > MAX_PWM_CAL_POINTS) numPwmCalibrationPoints = MAX_PWM_CAL_POINTS;
    for (int i = 0; i < numPwmCalibrationPoints; ++i) {
        snprintf(key, sizeof(key), "pf%d", i);
        pwmCalibrationPoints[i].flowMlMin = prefs.getInt(key, pwmCalibrationPoints[i].flowMlMin);
        snprintf(key, sizeof(key), "pp%d", i);
        pwmCalibrationPoints[i].pwm = prefs.getInt(key, pwmCalibrationPoints[i].pwm);
    }

    prefs.end();
}
#else
void ImplementSprayer::SaveCalibration() {}
void ImplementSprayer::LoadCalibration() {}
#endif

}  // namespace triton
