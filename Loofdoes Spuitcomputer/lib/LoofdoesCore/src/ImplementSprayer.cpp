/*
  ImplementSprayer - a library for the MeijWorks loofdoes implement
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
#include "ImplementSprayer.hpp"

#ifdef ARDUINO
#include <Preferences.h>
#include "driver/ledc.h"
#endif

namespace triton
{

ImplementSprayer::ImplementSprayer(Stream* serialDebug, VehicleGps* gps,
                                   InterfaceSprayer* interface)
    : serialDebug(serialDebug), gps(gps), interface(interface),
      speed(0), width(WIDTH), doseLHA(0.0f), doseLM(0.0f) {
#ifdef DEBUG
    serialDebug->println(S_DIVIDE);
    serialDebug->println("Initialising sprayer implement");
    serialDebug->println(S_DIVIDE);
#endif

    speedSum    = 0.0f;
    speedBufIdx = 0;
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
    updateInputs();
    updateSpeed();

    calculateDoseLHA();
    calculateDoseLM();
    calculatePWMValues(2);  // pump output

    updateOutputs();
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

void ImplementSprayer::updateSpeed() {
    speedSum -= speedBuf[speedBufIdx];
    speedBuf[speedBufIdx] = gps->GetSpeedMs();
    speedSum += speedBuf[speedBufIdx];
    speedBufIdx = (speedBufIdx + 1) % SPEED_AVG_SAMPLES;
    speed = speedSum / SPEED_AVG_SAMPLES;
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

    doseLHA = ((a * c) / b) + d;

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

void ImplementSprayer::calculatePWMValues(byte outputIndex) {
    if (!outputs[outputIndex].pwm || numPwmCalibrationPoints < 2) return;

    float doseMlMin = doseLM * 1000.0f;

    // Requested dose is below the pump's lowest calibrated flow point — stop it
    // rather than extrapolating below the calibrated range (which could produce
    // a bogus, even negative, PWM value). Sound the buzzer on OUT4 to tell the
    // driver to speed up, but only while still moving; give no warning once
    // fully stopped, since stopping the pump there is expected.
    if (doseMlMin < (float)pwmCalibrationPoints[0].flowMlMin) {
        outputs[outputIndex].value = 0;
        setOutputDuty(outputs[3], (speed > 0.0f) ? 0 : PWM_MAX_DUTY);
        return;
    }
    setOutputDuty(outputs[3], PWM_MAX_DUTY);  // clear the warning buzzer

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
        if (computed > (float)PWM_MAX_DUTY) computed = (float)PWM_MAX_DUTY;
        else if (computed < 0.0f)           computed = 0.0f;
        outputs[outputIndex].value = (unsigned int)computed;
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
    // - Output 3 is not button-driven; it's the low-flow warning buzzer, driven directly by calculatePWMValues()
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

void ImplementSprayer::setOutputDuty(const OutputState& out, uint32_t duty) {
#ifdef ARDUINO
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)out.ledcChannel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)out.ledcChannel);
#else
    (void)out;
    (void)duty;
#endif
}

void ImplementSprayer::SetCalibrationPWM(byte outputIndex, int pwmValue) {
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
