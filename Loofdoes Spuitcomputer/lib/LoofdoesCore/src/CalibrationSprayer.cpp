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
#ifdef ARDUINO

#include "CalibrationSprayer.hpp"
#include <stdlib.h>

#define RUN_DURATION_MS 60000UL
#define PWM_ARM_THRESHOLD 50  // analog reading below this counts as "knob at minimum"
#define PERIODIC_PRINT_INTERVAL_MS 500UL

namespace triton
{

CalibrationSprayer::CalibrationSprayer(Stream* serial, ImplementSprayer* impl)
    : serial(serial), impl(impl), state(State::IDLE),
      analogPointIdx(0), currentPWM(0), pwmStepIdx(0),
      runStartTime(0), lastCountdown(0),
      doseOutputEnabled(false), pumpOutputEnabled(false), gpsOutputEnabled(false),
      lastPeriodicPrintTime(0), bufLen(0) {
    buf[0] = 0;
}

void CalibrationSprayer::Process() {
    // Live analog-controlled PWM during the threshold-finding phase.
    // inputAnalog[0] is refreshed by ImplementSprayer::Update() each loop cycle.
    if (state == State::PWM_FIND) {
        int newPWM = (int)((long)impl->inputAnalog[0]->value * PWM_MAX_DUTY / 4095);
        if (newPWM != currentPWM) {
            currentPWM = newPWM;
            impl->SetCalibrationPWM(2, currentPWM);
            serial->print("\rPWM: ");
            serial->print(currentPWM);
            serial->print("    ");
        }
    }

    // Non-blocking 60-second timed run with per-second countdown.
    if (state == State::PWM_TIMED_RUN) {
        unsigned long elapsed = millis() - runStartTime;
        if (elapsed >= RUN_DURATION_MS) {
            impl->SetCalibrationPWM(2, 0);
            bufLen = 0;
            buf[0] = 0;
            serial->println();
            serial->print("Run complete. Enter volume collected (ml): ");
            lastCountdown = 0;
            state = State::PWM_MEASURE;
        } else {
            unsigned long remaining = (RUN_DURATION_MS - elapsed + 999UL) / 1000UL;
            if (remaining != lastCountdown) {
                lastCountdown = remaining;
                serial->print("\r");
                serial->print(remaining);
                serial->print("s remaining    ");
            }
        }
    }

    // Periodic telemetry, only while sitting idle/at the menu so it doesn't
    // corrupt the single-line \r-updated displays used elsewhere. Each output
    // is independently toggleable; all share one timer since they print at
    // the same rate.
    if ((doseOutputEnabled || pumpOutputEnabled || gpsOutputEnabled)
            && (state == State::IDLE || state == State::MENU)) {
        unsigned long now = millis();
        if (now - lastPeriodicPrintTime >= PERIODIC_PRINT_INTERVAL_MS) {
            lastPeriodicPrintTime = now;
            if (doseOutputEnabled) printDoseData();
            if (pumpOutputEnabled) printPumpData();
            if (gpsOutputEnabled) printGpsData();
        }
    }

    // Serial input handling for menus and calibration steps. 
    //This is non-blocking and can be called repeatedly in the main loop.
    while (serial->available()) {
        char c = (char)serial->read();

        if (state == State::IDLE) {
            state  = State::MENU;
            bufLen = 0;
            buf[0] = 0;
            printMenu();
            continue;
        }

        // Discard all input while the pump is running; the timed run ends on its own
        if (state == State::PWM_TIMED_RUN) continue;

        if (c == '\r') continue;

        if (c == '\n') {
            serial->println();   // terminate the echoed input line
            buf[bufLen] = 0;
            processLine();
            bufLen = 0;
            continue;
        }

        // During the knob-search phase only ENTER matters; echoing other characters
        // would interleave with the live \r-updated PWM value on the same line
        if (state == State::PWM_FIND) continue;

        if (c == '\b' || c == 0x7F) {
            if (bufLen > 0) {
                bufLen--;
                serial->print("\b \b");
            }
        }
        else if (bufLen < (int)sizeof(buf) - 1) {
            bool valid;
            if (state == State::EDIT_PWM_SELECT || state == State::EDIT_PWM_VALUE)
                valid = (c >= '0' && c <= '9') || c == 'q' || c == 'Q';
            else if (state == State::ANALOG_DOSE || state == State::PWM_MEASURE)
                valid = (c >= '0' && c <= '9');
            else
                valid = (c >= 32 && c < 127);
            if (valid) {
                buf[bufLen++] = c;
                serial->print(c);
            }
        }
    }
}

void CalibrationSprayer::processLine() {
    switch (state) {
        case State::MENU:            handleMenu();          break;
        case State::ANALOG_CAPTURE:  handleAnalogCapture(); break;
        case State::ANALOG_DOSE:     handleAnalogDose();    break;
        case State::PWM_ARM:         handlePwmArm();        break;
        case State::PWM_FIND:        handlePwmFind();       break;
        case State::PWM_STEP:        handlePwmStep();       break;
        case State::PWM_MEASURE:     handlePwmMeasure();    break;
        case State::EDIT_PWM_SELECT: handleEditPwmSelect(); break;
        case State::EDIT_PWM_VALUE:  handleEditPwmValue();  break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

void CalibrationSprayer::printMenu() {
    serial->println();
    serial->println("=== SPRAYER CALIBRATION ===");
    serial->println("1. Analog input    (l/ha)");
    serial->println("2. PWM output      (ml/min)");
    serial->println("3. Show current calibration");
    serial->println("4. Edit PWM point");
    serial->print("5. Analog output   (raw/dose/speed/flow) (");
    serial->print(doseOutputEnabled ? "ON" : "OFF");
    serial->println(" - press to toggle)");
    serial->print("6. Pump output     (calMode/pumpBtn/pumpOn/pumpVal) (");
    serial->print(pumpOutputEnabled ? "ON" : "OFF");
    serial->println(" - press to toggle)");
    serial->print("7. GPS output      (");
    serial->print(gpsOutputEnabled ? "ON" : "OFF");
    serial->println(" - press to toggle)");
    serial->print("8. GPS raw passthrough (");
    serial->print(impl->gps->GetRawEcho() ? "ON" : "OFF");
    serial->println(" - press to toggle)");
    serial->println("q. Exit");
    serial->print("Choose: ");
}

void CalibrationSprayer::handleMenu() {
    if (bufLen == 0) { printMenu(); return; }

    switch (buf[0]) {
        case '1':
            impl->calibrationMode = true;
            analogPointIdx = 0;
            startAnalogPoint();
            break;
        case '2':
            serial->println();
            serial->println("=== PWM OUTPUT CALIBRATION ===");
            serial->println("Turn the analog knob fully to MINIMUM, then press ENTER to arm.");
            impl->calibrationMode = true;
            currentPWM = 0;
            impl->SetCalibrationPWM(2, 0);
            state = State::PWM_ARM;
            break;
        case '3':
            printCurrentCalibration();
            printMenu();
            break;
        case '4':
            printCurrentCalibration();
            serial->print("Select PWM point to edit (1-");
            serial->print(impl->numPwmCalibrationPoints);
            serial->print(", q to cancel): ");
            state = State::EDIT_PWM_SELECT;
            break;
        case '5':
            doseOutputEnabled = !doseOutputEnabled;
            lastPeriodicPrintTime = 0;
            serial->print("\nAnalog output ");
            serial->println(doseOutputEnabled ? "enabled." : "disabled.");
            printMenu();
            break;
        case '6':
            pumpOutputEnabled = !pumpOutputEnabled;
            lastPeriodicPrintTime = 0;
            serial->print("\nPump output ");
            serial->println(pumpOutputEnabled ? "enabled." : "disabled.");
            printMenu();
            break;
        case '7':
            gpsOutputEnabled = !gpsOutputEnabled;
            lastPeriodicPrintTime = 0;
            serial->print("\nGPS output ");
            serial->println(gpsOutputEnabled ? "enabled." : "disabled.");
            printMenu();
            break;
        case '8':
            impl->gps->SetRawEcho(!impl->gps->GetRawEcho());
            serial->print("\nGPS raw passthrough ");
            serial->println(impl->gps->GetRawEcho() ? "enabled." : "disabled.");
            printMenu();
            break;
        case 'q':
        case 'Q':
            serial->println("\nExiting calibration.");
            state = State::IDLE;
            break;
        default:
            serial->println("Invalid choice.");
            printMenu();
            break;
    }
}

// ---------------------------------------------------------------------------
// Analog input calibration
// ---------------------------------------------------------------------------

void CalibrationSprayer::startAnalogPoint() {
    static const char* const labels[NUM_DOSE_CAL_POINTS] = { "MINIMUM", "MIDDLE", "MAXIMUM" };
    serial->println();
    serial->print("--- Analog point ");
    serial->print(analogPointIdx + 1);
    serial->print("/");
    serial->print(NUM_DOSE_CAL_POINTS);
    serial->println(" ---");
    serial->print("Set knob to ");
    serial->print(labels[analogPointIdx]);
    serial->println(" position, then press ENTER.");
    state = State::ANALOG_CAPTURE;
}

void CalibrationSprayer::handleAnalogCapture() {
    // inputAnalog[0] is refreshed by ImplementSprayer::Update() each loop cycle
    int val = impl->inputAnalog[0]->value;
    newDosePoints[analogPointIdx].analogValue = val;
    serial->print("Analog reading: ");
    serial->println(val);
    serial->print("Enter dose for this position (l/ha): ");
    state = State::ANALOG_DOSE;
}

void CalibrationSprayer::handleAnalogDose() {
    int dose;
    if (!parseInt(&dose) || dose <= 0) {
        serial->println("Invalid — enter a positive whole number.");
        serial->print("Enter dose (l/ha): ");
        return;
    }
    newDosePoints[analogPointIdx].dose = dose;
    serial->print("Point ");
    serial->print(analogPointIdx + 1);
    serial->println(" saved.");

    if (++analogPointIdx >= NUM_DOSE_CAL_POINTS) {
        finishAnalogCal();
    } else {
        startAnalogPoint();
    }
}

void CalibrationSprayer::finishAnalogCal() {
    // Sort ascending by analogValue so interpolation works in both directions
    for (int i = 0; i < NUM_DOSE_CAL_POINTS - 1; ++i) {
        for (int j = i + 1; j < NUM_DOSE_CAL_POINTS; ++j) {
            if (newDosePoints[j].analogValue < newDosePoints[i].analogValue) {
                DoseCalibrationPoint tmp = newDosePoints[i];
                newDosePoints[i]         = newDosePoints[j];
                newDosePoints[j]         = tmp;
            }
        }
    }

    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
        impl->doseCalibrationPoints[i] = newDosePoints[i];
    }

    impl->SaveCalibration();
    serial->println("Analog calibration saved.");
    impl->calibrationMode = false;
    printMenu();
    state = State::MENU;
}

// ---------------------------------------------------------------------------
// PWM output calibration
// ---------------------------------------------------------------------------

void CalibrationSprayer::handlePwmArm() {
    // Pump stays off (state != PWM_FIND, so Process() isn't driving it yet) until
    // the knob is confirmed at minimum — otherwise the live tracking below would
    // jump straight to whatever the knob currently reads.
    int val = impl->inputAnalog[0]->value;
    if (val > PWM_ARM_THRESHOLD) {
        serial->print("Knob not at minimum (reading ");
        serial->print(val);
        serial->println("). Turn it down and press ENTER.");
        return;
    }
    serial->println("Turn the analog knob until the pump just starts flowing.");
    serial->println("Press ENTER to capture that value as the start point.");
    state = State::PWM_FIND;
}

void CalibrationSprayer::handlePwmFind() {
    // currentPWM was kept up-to-date by Process() while knob was turned
    serial->println();
    serial->print("Start PWM captured: ");
    serial->println(currentPWM);

    // Generate NUM_PWM_STEPS equally-spaced points from startPWM to PWM_MAX_DUTY
    for (int i = 0; i < NUM_PWM_STEPS; ++i) {
        pwmSteps[i] = currentPWM
                    + (int)((long)(PWM_MAX_DUTY - currentPWM) * i / (NUM_PWM_STEPS - 1));
    }

    pwmStepIdx = 0;
    startPwmStep();
}

void CalibrationSprayer::startPwmStep() {
    impl->SetCalibrationPWM(2, 0);  // stay off until the run is actually started
    serial->print("Point ");
    serial->print(pwmStepIdx + 1);
    serial->print("/");
    serial->print(NUM_PWM_STEPS);
    serial->print(": PWM = ");
    serial->print(pwmSteps[pwmStepIdx]);
    serial->println(". Press ENTER to start 1-minute run.");
    state = State::PWM_STEP;
}

void CalibrationSprayer::handlePwmStep() {
    // Any ENTER starts the timed run — buffer content is ignored.
    // The countdown itself (including the first tick) is printed by the
    // \r-based updater in Process(), so every line on this row shares the
    // same format/length and none leave stale trailing characters behind.
    impl->SetCalibrationPWM(2, pwmSteps[pwmStepIdx]);
    serial->println("Running...");
    runStartTime  = millis();
    lastCountdown = 0;
    state = State::PWM_TIMED_RUN;
}

void CalibrationSprayer::handlePwmMeasure() {
    int volumeMl;
    if (!parseInt(&volumeMl) || volumeMl <= 0 || volumeMl > 4000) {
        serial->println("Invalid — enter a whole number between 1 and 4000.");
        serial->print("Enter volume collected (ml): ");
        return;
    }

    // Run was exactly 1 minute so ml collected == ml/min
    newPwmPoints[pwmStepIdx].flowMlMin = volumeMl;
    newPwmPoints[pwmStepIdx].pwm       = pwmSteps[pwmStepIdx];

    serial->print("Saved: PWM=");
    serial->print(pwmSteps[pwmStepIdx]);
    serial->print(", flow=");
    serial->print(volumeMl);
    serial->println(" ml/min.");

    pwmStepIdx++;

    if (pwmStepIdx >= NUM_PWM_STEPS) {
        finishPwmCal();
    } else {
        startPwmStep();
    }
}

void CalibrationSprayer::finishPwmCal() {
    impl->calibrationMode = false;
    impl->SetCalibrationPWM(2, 0);

    for (int i = 0; i < NUM_PWM_STEPS; ++i) {
        impl->pwmCalibrationPoints[i] = newPwmPoints[i];
    }
    impl->numPwmCalibrationPoints = (uint8_t)NUM_PWM_STEPS;

    impl->SaveCalibration();
    serial->print("PWM calibration saved (");
    serial->print(NUM_PWM_STEPS);
    serial->println(" points).");
    printMenu();
    state = State::MENU;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void CalibrationSprayer::printCurrentCalibration() {
    serial->println();
    serial->println("=== CURRENT CALIBRATION ===");
    serial->println("Analog input (l/ha):");
    for (int i = 0; i < NUM_DOSE_CAL_POINTS; ++i) {
        serial->print("  ");
        serial->print(i + 1);
        serial->print(": analog=");
        serial->print(impl->doseCalibrationPoints[i].analogValue);
        serial->print("  dose=");
        serial->print(impl->doseCalibrationPoints[i].dose);
        serial->println(" l/ha");
    }
    serial->print("PWM output (");
    serial->print(impl->numPwmCalibrationPoints);
    serial->println(" points):");
    for (int i = 0; i < impl->numPwmCalibrationPoints; ++i) {
        serial->print("  ");
        serial->print(i + 1);
        serial->print(": pwm=");
        serial->print(impl->pwmCalibrationPoints[i].pwm);
        serial->print("  flow=");
        serial->print(impl->pwmCalibrationPoints[i].flowMlMin);
        serial->println(" ml/min");
    }
}

void CalibrationSprayer::printDoseData() {
    // Analog/dosing pipeline diagnostics — raw analog counts and the resulting
    // dose/speed/flow demand, so a wrong-dose report can be traced to the
    // exact stage (raw ADC not moving vs. calibration math). Formatted as
    // Arduino Serial Plotter "label:value,label:value" pairs — no units in
    // the values themselves, since trailing text after a number breaks the
    // plotter's parser.
    serial->print("raw:");
    serial->print(impl->inputAnalog[0]->value);
    serial->print(",dose:");
    serial->print(impl->doseLHA, 1);
    serial->print(",speed:");
    serial->print(impl->gps->GetSpeedMs(), 2);
    serial->print(",flow:");
    serial->println(impl->doseLM * 1000.0f, 1);
}

void CalibrationSprayer::printPumpData() {
    // Pump-path diagnostics — calibration mode, pump button state, and the
    // computed pump value that actually reaches the hardware.
    serial->print("calMode=");
    serial->print(impl->calibrationMode ? "Y" : "N");
    serial->print("  pumpBtn=");
    serial->print(impl->buttons[2]->state ? "1" : "0");
    serial->print("  pumpOn=");
    serial->print(impl->outputs[2].state ? "1" : "0");
    serial->print("  pumpPwmFlag=");
    serial->print(impl->outputs[2].pwm ? "1" : "0");
    serial->print("  pumpVal=");
    serial->println(impl->outputs[2].value);
}

void CalibrationSprayer::printGpsData() {
    float lat, lon;
    impl->gps->GetPosition(&lat, &lon);
    serial->print("GPS: speed=");
    serial->print(impl->gps->GetSpeedMs(), 2);
    serial->print(" m/s  lat=");
    serial->print(lat, 6);
    serial->print("  lon=");
    serial->print(lon, 6);
    serial->print("  quality=");
    serial->println(impl->gps->GetQuality());
}

// ---------------------------------------------------------------------------
// Edit PWM calibration
// ---------------------------------------------------------------------------

void CalibrationSprayer::handleEditPwmSelect() {
    if (buf[0] == 'q' || buf[0] == 'Q') {
        printMenu();
        state = State::MENU;
        return;
    }
    int idx;
    if (!parseInt(&idx) || idx < 1 || idx > (int)impl->numPwmCalibrationPoints) {
        serial->print("Invalid — enter 1 to ");
        serial->print(impl->numPwmCalibrationPoints);
        serial->print(", q to cancel: ");
        return;
    }
    editPointIdx = idx - 1;
    serial->print("Point ");
    serial->print(idx);
    serial->print(": PWM=");
    serial->print(impl->pwmCalibrationPoints[editPointIdx].pwm);
    serial->print(", flow=");
    serial->print(impl->pwmCalibrationPoints[editPointIdx].flowMlMin);
    serial->println(" ml/min");
    serial->print("New flow (1-4000 ml/min, q to cancel): ");
    state = State::EDIT_PWM_VALUE;
}

void CalibrationSprayer::handleEditPwmValue() {
    if (buf[0] == 'q' || buf[0] == 'Q') {
        serial->println("Cancelled.");
        printMenu();
        state = State::MENU;
        return;
    }
    int flowMlMin;
    if (!parseInt(&flowMlMin) || flowMlMin <= 0 || flowMlMin > 4000) {
        serial->println("Invalid — enter a whole number between 1 and 4000.");
        serial->print("New flow (1-4000 ml/min, q to cancel): ");
        return;
    }
    impl->pwmCalibrationPoints[editPointIdx].flowMlMin = flowMlMin;
    impl->SaveCalibration();
    serial->println("Saved.");
    printMenu();
    state = State::MENU;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool CalibrationSprayer::parseFloat(float* out) {
    if (bufLen == 0) return false;
    char* end;
    float val = strtof(buf, &end);
    if (end == buf) return false;
    *out = val;
    return true;
}

bool CalibrationSprayer::parseInt(int* out) {
    if (bufLen == 0) return false;
    char* end;
    long val = strtol(buf, &end, 10);
    if (end == buf) return false;
    *out = (int)val;
    return true;
}

}  // namespace triton

#endif  // ARDUINO
