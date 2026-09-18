/*
  ImplementSprayer - a library for a slangenpomp (hose pump) sprayer
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
#include "../config/LanguageSprayer.hpp"

namespace triton
{

//------------
// Constructor
//------------
ImplementSprayer::ImplementSprayer(VehicleTractor* tractor) {
    Serial.println(S_DIVIDE);
    Serial.println("Initialising sprayer implement");
    Serial.println(S_DIVIDE);

#if defined(TEENSYPROTO) || defined(TEENSY)
    Serial1.begin(115200);
#endif

    // Pin configuration
    // Outputs
    pinMode(OUTPUT_FET, OUTPUT);
    pinMode(OUTPUT_FET2, OUTPUT);
    pinMode(OUTPUT_LED, OUTPUT);

    digitalWrite(OUTPUT_FET, LOW);
    digitalWrite(OUTPUT_FET2, LOW);
    digitalWrite(OUTPUT_LED, LOW);

    // Inputs
    pinMode(GEAR_SENS_PIN, INPUT);
    pinMode(FLOW_SENS_PIN, INPUT);
    pinMode(IMPLEMENT_SWITCH, INPUT);

#ifdef TEENSYPROTO
    digitalWrite(GEAR_SENS_PIN, HIGH);
    digitalWrite(FLOW_SENS_PIN, HIGH);
    digitalWrite(IMPLEMENT_SWITCH, HIGH);
#else
#ifdef TEENSY
    digitalWrite(GEAR_SENS_PIN, HIGH);
    digitalWrite(FLOW_SENS_PIN, HIGH);
    digitalWrite(IMPLEMENT_SWITCH, HIGH);
#else
    digitalWrite(GEAR_SENS_PIN, LOW);
    digitalWrite(FLOW_SENS_PIN, LOW);
    digitalWrite(IMPLEMENT_SWITCH, HIGH);
#endif
#endif

    // Get calibration data from EEPROM otherwise use defaults
    if (!readCalibrationData()) {
        // Default pwm calibration set
        pwmCalibrationData[0] = 1;  // 100 - 123
        pwmCalibrationData[1] = 47;
        pwmCalibrationData[2] = 65;
        pwmCalibrationData[3] = 79;
        pwmCalibrationData[4] = 90;
        pwmCalibrationData[5] = 129;
        pwmCalibrationData[6] = 150;
        pwmCalibrationData[7] = 170;
        pwmCalibrationData[8] = 187;
        pwmCalibrationData[9] = 192;
        pwmCalibrationData[10] = 196;
        pwmCalibrationData[11] = 203;

        // Default flow calibration set
        flowCalibration = 940;  // 140 - 141

        ccPer100Omw = 1000 * pumps;  // 150 - 151, for the whole pump

        teeth = 20;  // 160
        pumps = 8;   // 161
        width = 30;  // 162, in decimeters

        kp = 60;  // 170
        ki = 4;   // 171
        kd = 10;  // 172

#ifdef DEBUG
        Serial.println("No calibration data found");
        Serial.println("Using defaults");
#endif
    }

    // Get other initialisation values
    readDose();  // 190
    doseHist = 0;

#ifdef TEENSYPROTO
    // Calibration points for pwm and flow
    // TEENSYPROTO
    pwmCalibrationPoints[0] = 45;
    pwmCalibrationPoints[1] = 50;
    pwmCalibrationPoints[2] = 55;
    pwmCalibrationPoints[3] = 60;
    pwmCalibrationPoints[4] = 65;
    pwmCalibrationPoints[5] = 70;
    pwmCalibrationPoints[6] = 75;
    pwmCalibrationPoints[7] = 80;
    pwmCalibrationPoints[8] = 85;
    pwmCalibrationPoints[9] = 90;
    pwmCalibrationPoints[10] = 95;
    pwmCalibrationPoints[11] = 100;
#else
#ifdef TEENSY
    // Calibration points for pwm and flow
    // TEENSY
    pwmCalibrationPoints[0] = 45;
    pwmCalibrationPoints[1] = 50;
    pwmCalibrationPoints[2] = 55;
    pwmCalibrationPoints[3] = 60;
    pwmCalibrationPoints[4] = 65;
    pwmCalibrationPoints[5] = 70;
    pwmCalibrationPoints[6] = 75;
    pwmCalibrationPoints[7] = 80;
    pwmCalibrationPoints[8] = 85;
    pwmCalibrationPoints[9] = 90;
    pwmCalibrationPoints[10] = 95;
    pwmCalibrationPoints[11] = 100;
#else
#ifdef MICRO
    // Calibration points for pwm and flow
    // PRODUCTIE Arduino MICRO
    pwmCalibrationPoints[0] = 45;
    pwmCalibrationPoints[1] = 50;
    pwmCalibrationPoints[2] = 55;
    pwmCalibrationPoints[3] = 60;
    pwmCalibrationPoints[4] = 65;
    pwmCalibrationPoints[5] = 70;
    pwmCalibrationPoints[6] = 75;
    pwmCalibrationPoints[7] = 80;
    pwmCalibrationPoints[8] = 85;
    pwmCalibrationPoints[9] = 90;
    pwmCalibrationPoints[10] = 95;
    pwmCalibrationPoints[11] = 100;
#else
    // Calibration points for pwm and flow
    // VOORSERIE
    pwmCalibrationPoints[0] = 50;
    pwmCalibrationPoints[1] = 60;
    pwmCalibrationPoints[2] = 70;
    pwmCalibrationPoints[3] = 80;
    pwmCalibrationPoints[4] = 90;
    pwmCalibrationPoints[5] = 100;
    pwmCalibrationPoints[6] = 110;
    pwmCalibrationPoints[7] = 120;
    pwmCalibrationPoints[8] = 130;
    pwmCalibrationPoints[9] = 140;
    pwmCalibrationPoints[10] = 150;
    pwmCalibrationPoints[11] = 160;
#endif
#endif
#endif

    // Reset timer, pulse counters, setpoints and flags
    updateAge = millis();
    updateAgeFlag = millis();

    gearPulses = 0;
    rounds = 0;
    pumpsOn = 8;
    hold = false;

    flowPulses = 0;
    volume = 0;

    setpointPwm = 0;
    histCount = 5;
    histSize = 5;

    updateFlag = false;
    gearPuls = false;
    flowPuls = false;
    alarm = false;

    // PID
    for (int i = 0; i < histSize; i++) {
        deltaHist[i] = 0;
    }
    deltaSum = 0;
    deltaAvg = 0;
    deltaDelta = 0;

    P = 0;
    I = 0;
    D = 0;

    // Print calibration data
    printCalibrationData();

    this->tractor = tractor;
}

// ----------------------------------
// Method for updating implement data
// ----------------------------------
void ImplementSprayer::Update(byte mode, int buttons) {
#ifdef TEENSYPROTO
    if (Serial1.available()) {
        byte inputButtons = Serial1.read();
        byte newPumpsOn = 0;

        while (inputButtons) {
            if (inputButtons % 2) {
                newPumpsOn++;
            }
            inputButtons = inputButtons >> 1;
        }

        if (newPumpsOn != pumpsOn) {
            pumpsOn = newPumpsOn;
            hold = true;
        }
    }

    if (pumpsOn == 0) {
        mode = 2;
    }
#endif

    // Count pulses for gear and flow
    if (gearPuls != digitalRead(GEAR_SENS_PIN)) {
        gearPulses++;
        gearPuls = !gearPuls;
    }

    if (flowPuls != digitalRead(FLOW_SENS_PIN)) {
        flowPulses++;
        volume++;
        flowPuls = !flowPuls;
        digitalWrite(OUTPUT_LED, flowPuls);
    }

    // Set flag for volume / surface indication
    if (millis() - updateAgeFlag >= 5000) {
        updateFlag = !updateFlag;
        updateAgeFlag = millis();
    }

    // update calculations every second, reset counters
    if (millis() - updateAge >= 1000) {
        setDose(buttons);

        // Mode auto or sim
        if ((mode == 0 || mode == 4) && !hold) {
            calculateSetpointFlow(mode);
            // calculateAlarm() is intentionally not called here -- inert in
            // the legacy source too (its call site was already commented
            // out), kept implemented but unused for behavior parity.
            calculateSetpointPwm();

            analogWrite(OUTPUT_FET, setpointPwm);
            analogWrite(OUTPUT_FET2, setpointPwm);
        }
        // Mode hold
        else if (mode == 1 || mode == 2 || mode == 3) {
            Stop();
        }

        // Reset timer and pulse counters
        updateAge = millis();
        gearPulses = 0;
        flowPulses = 0;
        hold = false;
    }
}

void ImplementSprayer::calculateSetpointFlow(byte mode) {
    // Calculate needed flow for current speed in cc per 100 seconds +
    // correction for amount of pumps on
#if defined(TEENSYPROTO) || defined(TEENSY)
    neededFlow = ccPer100m * tractor->GetSpeedMs() * (float(pumpsOn) / float(pumps));
#else
    neededFlow = ccPer100m * tractor->GetSpeedMs();
#endif
    (void)mode;

    // Calculate actual flow in cc per 100 seconds according to flowmeter
    // flow calibration is in pulses per l (puls * 100sec * 1000cc / calibration / 2)
    actualFlow = flowPulses * (50000.0f / flowCalibration);

    // Calculate delta flow
    delta = neededFlow - actualFlow;

    // Calculate integral and differential values
    byte previousHistCount = histCount - 1;

    if (histCount >= histSize) {
        histCount = 0;
    }

    deltaSum = deltaSum - deltaHist[histCount] + delta;
    deltaAvg = deltaSum / histSize;
    deltaHist[histCount] = delta;
    deltaDelta = deltaHist[histCount] - deltaHist[previousHistCount];

    histCount++;

    // Calculate initial rounds per second:
    P = neededFlow + (float(kp) / 100) * delta;  // needed flow: 1 m/s, 200l/ha == 6000cc/100s

    if (ki == 0) {
        I = 0;
    }
    else {
        I = I + (float(ki) / 100) * deltaSum;
        if (I > 10000) {
            I = 10000;
        }
        else if (I < -10000) {
            I = -10000;
        }
    }

    if (kd == 0) {
        D = 0;
    }
    else {
        if (deltaDelta == 0) {
            deltaDelta = 1;
        }
        D = (float(kd) / 100) * (delta / deltaDelta);
    }

    setpointFlow = P + I + D;
}

void ImplementSprayer::calculateAlarm(byte mode) {
    if (mode == 0 || mode == 4) {
        // delta flow is greater than 25 percent
        if (abs(long(neededFlow - calculatedFlow) * 100 / neededFlow) > 25) {
            alarm = true;
        }
        else {
            alarm = false;
        }
    }
}

// ---------------------------------
// Method for calculating needed PWM
// ---------------------------------
void ImplementSprayer::calculateSetpointPwm() {
    int readRaw = (flowCalibration / 500.0f) * (setpointFlow / 40);  // cc per 100s
    int i = 0;
    int setpoint = 0;

    // Loop through calibrationdata
    while (i < 11 && readRaw > pwmCalibrationData[i]) {
        i++;
    }

    if (i == 0) {
        i++;
    }

    // Interpolate calibrationdata
    float a = readRaw - pwmCalibrationData[i - 1];
    float b = pwmCalibrationData[i] - pwmCalibrationData[i - 1];
    float c = pwmCalibrationPoints[i] - pwmCalibrationPoints[i - 1];
    float d = pwmCalibrationPoints[i - 1];

    // b is zero when two calibration points captured the same flow-pulse
    // count (the pump maxed out before the 12-point sweep finished, or
    // corrupt EEPROM) -- same class of guard already added to
    // ImplementPlough/ImplementPlanter's position interpolation. Falling
    // back to the segment's own start point avoids narrowing an inf/NaN to
    // int (undefined behaviour).
    if (b == 0.0f) {
        setpoint = d;
    }
    else {
        setpoint = (((a * c) / b) + d);
    }

    if (setpoint > 255) {
        setpointPwm = 255;
    }
    else if (setpoint < 0) {
        setpointPwm = 0;
    }
    else {
        setpointPwm = setpoint;
    }
}

// ------------------------------
// Method for adjusting implement
// ------------------------------
void ImplementSprayer::Stop() {
    analogWrite(OUTPUT_FET, 0);
    analogWrite(OUTPUT_FET2, 0);
}

// ---------------------------------------------
// Method for setting dose and writing to EEPROM
// ----------------------------------------------
void ImplementSprayer::setDose(int correction) {
    if (correction) {
        doseHist++;

        if (doseHist > 10) {
            dose += 20 * correction;
        }
        else if (doseHist > 5) {
            dose += 10 * correction;
        }
        else {
            dose += correction;
        }

        // limit dose
        if (dose > 500) {
            dose = 500;
        }
        else if (dose < 50) {
            dose = 50;
        }
        // dose is in l/ha == cc/10m2
        // dose * width(in dm) -> cc/100m
        ccPer100m = dose * width;

        // Write each byte separately to the memory
        EEPROM.write(190, highByte(dose));
        EEPROM.write(191, lowByte(dose));
    }
    else {
        doseHist = 0;
    }
}

// ---------------------------------------------
// Method for reading implement dose from EEPROM
// ---------------------------------------------
void ImplementSprayer::readDose() {
    if (EEPROM.read(190) < 255) {
        // Read dose (2 bytes)
        dose = word(EEPROM.read(190), EEPROM.read(191));
        if (dose > 500) {
            dose = 500;
        }
        else if (dose < 50) {
            dose = 50;
        }
    }
    else {
        dose = 200;
    }

    ccPer100m = dose * width;
}

// ----------------------------
// Methods for calibrating pump
// ----------------------------
void ImplementSprayer::CalibratePump() {
    gearPulses = 0;
    flowPulses = 0;
    volume = 0;
    rounds = 0;

    for (int i = 0; i < 12; i++) {
        analogWrite(OUTPUT_FET, pwmCalibrationPoints[i]);
        analogWrite(OUTPUT_FET2, pwmCalibrationPoints[i]);

        updateAge = millis();

        while (millis() - updateAge <= 5000) {
            if (millis() - updateAge <= 2500) {
                gearPulses = 0;
                flowPulses = 0;
            }
            else {
                // Count flow pulses
                if (flowPuls != digitalRead(FLOW_SENS_PIN)) {
                    flowPulses++;
                    volume++;
                    flowPuls = !flowPuls;
                    digitalWrite(OUTPUT_LED, flowPuls);
                }
                // Count gear pulses
                if (gearPuls != digitalRead(GEAR_SENS_PIN)) {
                    gearPulses++;
                    rounds++;
                    gearPuls = !gearPuls;
                }
            }
        }
        if (i - 1 > 0) {
            if (pwmCalibrationData[i - 1] < flowPulses) {
                pwmCalibrationData[i] = flowPulses;
            }
            else {
                pwmCalibrationData[i] = pwmCalibrationData[i - 1] + 1;
            }
        }
        else {
            pwmCalibrationData[i] = flowPulses;
        }
    }
    // Stop pump
    Stop();

    flowCalibration = 940;
}

// ----------------------------------------------
// Method for reading calibrationdata from EEPROM
// ----------------------------------------------
bool ImplementSprayer::readCalibrationData() {
    // Read amount of startups and add 1
    EEPROM.write(0, EEPROM.read(0) + 1);

    // Read offset and angle calibration data
    if (EEPROM.read(100) != 255 || EEPROM.read(140) != 255 ||
        EEPROM.read(150) != 255 || EEPROM.read(160) != 255 ||
        EEPROM.read(161) != 255 || EEPROM.read(162) != 255 ||
        EEPROM.read(170) != 255 || EEPROM.read(171) != 255 ||
        EEPROM.read(172) != 255) {

        // Read from eeprom highbyte, then lowbyte, and combine into words
        int storedPwm[12];
        for (int i = 0; i < 12; i++) {
            // 100 - 123
            storedPwm[i] = word(EEPROM.read(i * 2 + 100), EEPROM.read(i * 2 + 101));
        }

        // calibratePump() only ever writes a strictly increasing sequence
        // (each point is clamped to be greater than the previous one) -- a
        // stored table that isn't strictly increasing cannot have come from
        // a real calibration run. Previously accepted as-is and fed straight
        // into calculateSetpointPwm()'s divisor.
        bool pwmValid = true;
        for (int i = 0; i < 12; i++) {
            if (storedPwm[i] < 0 || storedPwm[i] > kPwmCalibrationDataMax) {
                pwmValid = false;
            }
            if (i > 0 && storedPwm[i] <= storedPwm[i - 1]) {
                pwmValid = false;
            }
        }

        if (pwmValid) {
            for (int i = 0; i < 12; i++) {
                pwmCalibrationData[i] = storedPwm[i];
            }
        }
        else {
            // The legacy defaults (see the constructor's "no calibration
            // data" branch) are the nearest defensible fallback -- this
            // function otherwise only skips writing pwmCalibrationData when
            // it returns false entirely, which isn't the case here (other
            // fields below are still valid).
            pwmCalibrationData[0] = 1;
            pwmCalibrationData[1] = 47;
            pwmCalibrationData[2] = 65;
            pwmCalibrationData[3] = 79;
            pwmCalibrationData[4] = 90;
            pwmCalibrationData[5] = 129;
            pwmCalibrationData[6] = 150;
            pwmCalibrationData[7] = 170;
            pwmCalibrationData[8] = 187;
            pwmCalibrationData[9] = 192;
            pwmCalibrationData[10] = 196;
            pwmCalibrationData[11] = 203;
        }

        flowCalibration = word(EEPROM.read(140), EEPROM.read(141));

        pumps = EEPROM.read(161);

        ccPer100Omw = pumps * int(word(EEPROM.read(150), EEPROM.read(151)));

        teeth = EEPROM.read(160);
        width = EEPROM.read(162);

        kp = EEPROM.read(170);
        ki = EEPROM.read(171);
        kd = EEPROM.read(172);
    }
    else {
        return false;
    }
    return true;
}

//---------------------------------------------------
//Method for printing calibration data to serial port
//---------------------------------------------------
void ImplementSprayer::printCalibrationData() {
    // Printing calibration data to serial port
    Serial.println("-------------------------------");
    Serial.println("Implement using following data:");
    Serial.println("-------------------------------");
    Serial.println("Gear calibration data RPM->PWM");
    for (int i = 0; i < 12; i++) {
        Serial.print(pwmCalibrationData[i]);
        Serial.print(", ");
        Serial.println(pwmCalibrationPoints[i]);
    }
    Serial.println("-------------------------------");

    Serial.println("Flow calibration");
    Serial.println(flowCalibration);
    Serial.println("-------------------------------");

    Serial.println("Teeth");
    Serial.println(teeth);
    Serial.println("-------------------------------");

    Serial.println("Pumps");
    Serial.println(pumps);
    Serial.println("-------------------------------");

    Serial.println("Width in dm");
    Serial.println(width);
    Serial.println("-------------------------------");

    Serial.println("CC per 100 rounds");
    Serial.println(ccPer100Omw);
    Serial.println("-------------------------------");

    Serial.println("PID values KP/KI/KD");
    Serial.println(kp);
    Serial.println(ki);
    Serial.println(kd);
    Serial.println("-------------------------------");
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementSprayer::writeCalibrationData() {
    // Write each byte separately to the memory first the data then the points
    for (int i = 0; i < 12; i++) {
        EEPROM.write(i * 2 + 100, highByte(pwmCalibrationData[i]));  // 100
        EEPROM.write(i * 2 + 101, lowByte(pwmCalibrationData[i]));
    }

    EEPROM.write(140, highByte(flowCalibration));  // 140
    EEPROM.write(141, lowByte(flowCalibration));   // 141

    EEPROM.write(150, highByte(ccPer100Omw / pumps));  // 150
    EEPROM.write(151, lowByte(ccPer100Omw / pumps));   // 151

    EEPROM.write(160, teeth);  // 160
    EEPROM.write(161, pumps);  // 161
    EEPROM.write(162, width);  // 162

    EEPROM.write(170, kp);  // 170
    EEPROM.write(171, ki);  // 171
    EEPROM.write(172, kd);  // 172

#ifdef DEBUG
    Serial.println("Calibration data written");
#endif
}

}  // namespace triton
