/*
  CalibrationSprayer - serial-port calibration tool for the MeijWorks loofdoes
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

#ifdef ARDUINO

#include <Arduino.h>

#include "ImplementSprayer.hpp"

namespace triton
{

// Call Process() every loop iteration. Normal operation (ImplementSprayer::Update)
// continues in the background. Any serial character while idle opens the menu.
class CalibrationSprayer {
public:
    CalibrationSprayer(Stream* serial, ImplementSprayer* impl);
    void Process();

private:
    enum class State {
        IDLE, MENU,
        ANALOG_CAPTURE, ANALOG_DOSE,
        PWM_ARM, PWM_FIND, PWM_STEP, PWM_TIMED_RUN, PWM_MEASURE,
        EDIT_PWM_SELECT, EDIT_PWM_VALUE
    };

    Stream*           serial;
    ImplementSprayer* impl;
    State             state;

    int                  analogPointIdx;
    DoseCalibrationPoint newDosePoints[NUM_DOSE_CAL_POINTS];

    // PWM calibration: analog knob finds start threshold, then NUM_PWM_STEPS
    // equally-spaced points are auto-generated. Each point runs the pump for
    // exactly 60 seconds; volume collected (l) equals flow in l/min.
    int                 editPointIdx;
    int                 currentPWM;
    int                 pwmSteps[NUM_PWM_STEPS];
    int                 pwmStepIdx;
    PwmCalibrationPoint newPwmPoints[NUM_PWM_STEPS];
    unsigned long       runStartTime;
    unsigned long       lastCountdown;

    bool          doseOutputEnabled;
    bool          pumpOutputEnabled;
    bool          gpsOutputEnabled;
    unsigned long lastPeriodicPrintTime;

    char buf[32];
    int  bufLen;

    void processLine();
    void printMenu();
    void handleMenu();
    void startAnalogPoint();
    void handleAnalogCapture();
    void handleAnalogDose();
    void handlePwmArm();
    void handlePwmFind();
    void startPwmStep();
    void handlePwmStep();
    void handlePwmMeasure();
    void finishAnalogCal();
    void finishPwmCal();
    void printCurrentCalibration();
    void handleEditPwmSelect();
    void handleEditPwmValue();
    void printDoseData();
    void printPumpData();
    void printGpsData();

    bool parseFloat(float* out);
    bool parseInt(int* out);
};

}  // namespace triton

#endif  // ARDUINO
