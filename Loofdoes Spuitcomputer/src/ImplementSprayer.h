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
#pragma once

#include <Arduino.h>
#include "VehicleGps.h"
#include "InterfaceSprayer.h"

#define SPRAYER_VERSION 0.2

#define WIDTH 300

#define NUM_OUTPUTS 4

#define OUT1 27
#define OUT2 26
#define OUT3 25
#define OUT4 23

#define MAX_PWM_CAL_POINTS  10
#define NUM_DOSE_CAL_POINTS 3
#define NUM_PWM_STEPS       5
#define PWM_MAX_DUTY        4095
#define PWM_FREQ_HZ         1000
#define SPEED_AVG_SAMPLES   5

struct OutputState {
    uint8_t       pin;
    uint8_t       ledcChannel;
    bool          state;
    bool          pwm;
    unsigned int  value;
    unsigned long timer;
};

struct PwmCalibrationPoint {
    int flowMlMin;  // measured flow in ml/min (= ml collected in 1-minute run)
    int pwm;        // PWM value 0-4095
};

struct DoseCalibrationPoint {
    int dose;        // dose in l/ha
    int analogValue; // raw ADC value 0-4095
};

class CalibrationSprayer;  // forward declaration for friend access

class ImplementSprayer {
    friend class CalibrationSprayer;

private:
    Stream*           serialDebug;
    VehicleGps*       gps;
    InterfaceSprayer* interface;

    DigitalInputState* buttons[NUM_DIGITAL_IN];
    AnalogInputState*  inputAnalog[NUM_ANALOG_IN];

    PwmCalibrationPoint pwmCalibrationPoints[MAX_PWM_CAL_POINTS] = {
        { 0, 0 },
        { 2000, 2048 },
        { 4000, 4095 }
    };
    uint8_t numPwmCalibrationPoints = 3;

    DoseCalibrationPoint doseCalibrationPoints[NUM_DOSE_CAL_POINTS] = {
        { 50, 0 },
        { 100, 2048 },
        { 200, 4095 }
    };

    float speed;
    float speedBuf[SPEED_AVG_SAMPLES];
    float speedSum;
    int   speedBufIdx;
    float width;
    bool  calibrationMode = false;

    void updateInputs();
    void updateSpeed();

    void calculateDoseLHA();
    void calculateDoseLM();
    void calculatePWMValues(byte outputIndex);

    void updateOutputs();
    void setOutputDuty(const OutputState& out, uint32_t duty);

public:
    // Exposed for testing; use GetOutputs() in production code
    float       doseLHA = 0.0f;
    float       doseLM  = 0.0f;
    OutputState outputs[NUM_OUTPUTS] = {
        { OUT1, 0, false, false, 0, 0 },
        { OUT2, 1, false, false, 0, 0 },
        { OUT3, 2, false, true,  0, 0 },  // pump — driven by calibrated dose PWM, not a plain relay
        { OUT4, 3, false, false, 0, 0 },
    };

    ImplementSprayer(Stream* serialDebug, VehicleGps* gps, InterfaceSprayer* interface);

    void Update();
    void LoadCalibration();
    void SaveCalibration();
    void SetCalibrationPWM(byte outputIndex, int pwmValue);

    inline OutputState* GetOutputs() { return outputs; }
};