/*
  CalibrationSprayer - serial-port calibration tool for the MeijWorks loofdoes
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
#pragma once

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub): the wizard talks to a Stream and to
// ImplementSprayer only, nothing ESP32-specific, so it is testable with a
// scripted serial port. Excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>

#include "../implement/ImplementSprayer.hpp"
#include "SerialGuidanceChannel.hpp"

namespace triton
{

// Call Process() every loop iteration. Normal operation (ImplementSprayer::Update)
// continues in the background. Any serial character while idle opens the menu.
class CalibrationSprayer {
public:
    // The channel is only for the raw-passthrough switch (menu option 8);
    // guidance values come through impl, which owns the GuidanceSource.
    CalibrationSprayer(Stream* serial, ImplementSprayer* impl, SerialGuidanceChannel* gpsChannel);
    void Process();

    // Menu option 9, "Forget paired phones", shown once a handler is set
    // (main.cpp points it at the Bluetooth link; this class stays BLE-free).
    typedef void (*ActionHandler)();
    void SetForgetPhonesHandler(ActionHandler handler) { forgetPhones = handler; }

private:
    enum class State {
        IDLE, MENU,
        ANALOG_CAPTURE, ANALOG_DOSE,
        PWM_ARM, PWM_FIND, PWM_STEP, PWM_TIMED_RUN, PWM_MEASURE,
        EDIT_PWM_SELECT, EDIT_PWM_VALUE,
        RESTORE_TABLE, RESTORE_INDEX, RESTORE_X, RESTORE_Y
    };

    Stream*                serial;
    ImplementSprayer*      impl;
    SerialGuidanceChannel* gpsChannel;
    State                  state;
    ActionHandler     forgetPhones = nullptr;

    int                  analogPointIdx;
    DoseCalibrationPoint newDosePoints[NUM_DOSE_CAL_POINTS] = {};

    // Menu option 0, restore a point from typed values (a unit record holds
    // analog/dose and PWM/flow pairs; the wizards only capture live readings
    // and option 4 only edits a flow). One table per session, point after
    // point until q.
    bool restorePwm = false;
    int  restoreIdx = 0;
    int  restoreX   = 0;

    // PWM calibration: analog knob finds start threshold, then NUM_PWM_STEPS
    // equally-spaced points are auto-generated. Each point runs the pump for
    // exactly 60 seconds; volume collected (l) equals flow in l/min.
    int                 editPointIdx = 0;
    int                 currentPWM;
    int                 pwmSteps[NUM_PWM_STEPS] = {};
    int                 pwmStepIdx;
    PwmCalibrationPoint newPwmPoints[NUM_PWM_STEPS] = {};
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
    void cancelCalibration();
    void finishAnalogCal();
    void finishPwmCal();
    void printCurrentCalibration();
    void handleEditPwmSelect();
    void handleEditPwmValue();
    void handleRestoreTable();
    void handleRestoreIndex();
    void handleRestoreX();
    void handleRestoreY();
    void promptRestoreIndex();
    void cancelRestore();
    void printDoseData();
    void printPumpData();
    void printGpsData();

    bool parseInt(int* out);
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
