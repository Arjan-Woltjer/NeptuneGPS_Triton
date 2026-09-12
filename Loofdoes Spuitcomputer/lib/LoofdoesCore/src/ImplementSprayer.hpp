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

#include "InterfaceSprayer.hpp"
#include "VehicleGps.hpp"

namespace triton
{

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

    float speed;
    float speedBuf[SPEED_AVG_SAMPLES];
    float speedSum;
    int   speedBufIdx;
    float width;

    // Guidance older than this counts as no guidance at all. Same threshold
    // InterfacePlough already applies on the plough side.
    static constexpr unsigned long kGuidanceTimeoutMs = 2000;

    void updateInputs();
    void updateSpeed();
    bool guidanceStale() const;

    void calculateDoseLHA();
    void calculateDoseLM();
    void calculatePWMValues(byte outputIndex);

    void updateOutputs();
    void setOutputDuty(const OutputState& out, uint32_t duty);

    // Inverse of calculateDoseLM(): a pump flow back to l/ha at the current
    // speed and width, so a clamped duty can be expressed in the operator's
    // own unit.
    float flowToLHA(float flowMlMin) const;

    // Deviation flag with a hold in both directions; drives OUT4.
    bool          deviationPending   = false;
    unsigned long deviationChangedAt = 0;
    void updateDeviation();

public:
    // Exposed for testing; use GetOutputs() in production code
    float       doseLHA = 0.0f;
    float       doseLM  = 0.0f;

    // The dose the pump can actually deliver, in l/ha, derived from the duty
    // that really reaches it: equal to doseLHA inside the calibrated curve,
    // 0 when the demand is below the lowest calibrated flow and the pump is
    // cut, lower than doseLHA when the duty saturates at PWM_MAX_DUTY.
    // kActualDoseUndefined when there is nothing to compare against: stale
    // guidance, standing still, or an unusable calibration table.
    static constexpr float kActualDoseUndefined = -1.0f;
    float actualLHA = kActualDoseUndefined;

    // True once actualLHA has been outside kDoseTolerance of doseLHA for
    // kDeviationHoldMs while the pump output is on; drives the OUT4 buzzer
    // and is reported to the companion app so both agree.
    static constexpr float         kDoseTolerance   = 0.05f;
    static constexpr unsigned long kDeviationHoldMs = 1000;
    bool doseDeviation = false;

    // Set while the serial wizard (CalibrationSprayer, a friend) drives the
    // outputs directly; also exposed so tests can cover that hand-over.
    bool calibrationMode = false;

    // Also exposed for testing. CalibrationSprayer is already a friend and
    // writes these directly, so this widens who can reach them rather than
    // breaking an invariant -- and the fail-closed paths that guard duplicate
    // and short calibration tables are only reachable by setting them.
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

}  // namespace triton