/*
  ImplementRooier - a library for the MeijWorks windrower implement
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
#include "ImplementRooier.hpp"

namespace triton
{

//------------
// Constructor
//------------
ImplementRooier::ImplementRooier() {
#ifdef DEBUG
    Serial.println("Initialising rooier");
#endif

    // Pin configuration
    // Outputs
    pinMode(OUTPUT_UP_L_8, OUTPUT);
    pinMode(OUTPUT_DOWN_L_8, OUTPUT);
    pinMode(OUTPUT_UP_R_8, OUTPUT);
    pinMode(OUTPUT_DOWN_R_8, OUTPUT);
    pinMode(OUTPUT_LED_8, OUTPUT);

    // Analog IO
    // DEFAULT is an AVR/Teensy-3.x analogReference() constant; Teensy 4.1's
    // core doesn't define it at all -- guarded so the call still fires on
    // boards where it's meaningful and is simply skipped on Teensy 4.1,
    // matching the same fix already applied to ImplementPlough/ImplementScraper.
#ifdef DEFAULT
    analogReference(DEFAULT);
#endif
    pinMode(HEIGHT_SENS_PIN_L_8, INPUT);
    pinMode(HEIGHT_SENS_PIN_R_8, INPUT);

    // Calibration points, shared by both sides
    positionCalibrationPoints[0] = 0;
    positionCalibrationPoints[1] = 50;
    positionCalibrationPoints[2] = 100;

    heightL = 0;
    heightR = 0;
    lastHeightL = 0;
    lastHeightR = 0;

    targetL = 0;
    targetR = 0;

    integralL = 0.0f;
    integralR = 0.0f;
    lastErrorL = 0.0f;
    lastErrorR = 0.0f;

    // End shutoff timers
    shutoffTime = SHUTOFF_8;
    shutoffWideL = false;
    shutoffWideR = false;
    shutoffNarrowL = false;
    shutoffNarrowR = false;
    shutoffTimerL = millis();
    shutoffTimerR = millis();

    // Update timer
    updateAge = millis();

    // Get calibration data from EEPROM otherwise use defaults
    if (!readCalibrationData()) {
        // Default height calibration set. The legacy source's own
        // constructor-local defaults (never actually assigned to the real
        // members there -- a real bug, fixed here by assigning them for
        // real) ran descending (1000, 650, 300) against ascending
        // positionCalibrationPoints -- but getActualHeight()'s interpolation
        // loop (shared with every sibling module) walks calibrationData with
        // `while (readRaw > calibrationData[i])`, which only ever advances
        // past i=0 for *ascending* data; kept descending, the third
        // calibration point would silently be unreachable. Reordered to
        // ascending here so all three points are actually usable.
        positionCalibrationDataL[0] = 300;
        positionCalibrationDataL[1] = 650;
        positionCalibrationDataL[2] = 1000;
        positionCalibrationDataR[0] = 300;
        positionCalibrationDataR[1] = 650;
        positionCalibrationDataR[2] = 1000;

        // Operator target height (percentage) and trim
        setpoint = 50;
        offset = 0;

        // L/R balance trim and bang-bang error margin
        skew = 0;
        error = 2;

        // PID gains default to 0 -- see the class comment in
        // ImplementRooier.hpp for why this feature is real but inert by
        // default.
        kp = 0;
        ki = 0;
        kd = 0;

        // pwm values for manual and auto -- full drive strength until tuned,
        // matching ImplementScraper's PWM_MAN/PWM_AUTO-undefined fallback.
        manPwm = 254;
        autoPwm = 254;

#ifdef DEBUG
        Serial.println("No calibration data found, using defaults");
#endif
    }
#ifdef DEBUG
    Serial.println("Done...");
#endif
}

// ----------------------------------
// Method for updating implement data
// ----------------------------------
void ImplementRooier::Update(byte mode, int buttons) {
    // Buttons nudge the operator-set target height while in Auto/Hold mode --
    // matches the legacy readSetpoint()/setSetpoint(int) EEPROM fragment
    // (a byte clamped 1-99, persisted immediately on change). In Manual
    // mode, buttons instead drive Adjust()'s temporary jog below, leaving
    // the persisted setpoint untouched -- the same buttons-mean-different-
    // things-per-mode split ImplementScraper::Update()'s setOffset(buttons)
    // call already establishes.
    if (mode < 2 && buttons != 0 && buttons != 2) {
        adjustSetpoint(buttons);
    }

    // Update height every 25 milliseconds. The legacy source alternated
    // which side it stored each tick (while still reading -- but discarding
    // -- the other channel, presumably to let the ADC mux settle between
    // reads); that discarded-read nuance can't be verified against real
    // hardware here, so both sides are simply read and stored every tick,
    // which is simpler and behaviourally equivalent from this class's
    // public interface.
    if (millis() - updateAge >= 25) {
        heightL = getActualHeight(HEIGHT_SENS_PIN_L_8, positionCalibrationDataL);
        heightR = getActualHeight(HEIGHT_SENS_PIN_R_8, positionCalibrationDataR);

        computeTargets();

        updateAge = millis();
    }
}

// -----------------------------------------------
// Method for nudging and persisting the setpoint
// -----------------------------------------------
void ImplementRooier::adjustSetpoint(int correction) {
    setpoint += correction;

    if (setpoint < 1) {
        setpoint = 1;
    }
    if (setpoint > 99) {
        setpoint = 99;
    }

    if (setpoint != EEPROM.read(220)) {
        EEPROM.write(220, byte(setpoint));
    }
}

// ---------------------------------------------------
// Method for recomputing the per-side PID-adjusted targets
// ---------------------------------------------------
void ImplementRooier::computeTargets() {
    targetL = computeTarget(GetHeightL(), integralL, lastErrorL);
    targetR = computeTarget(GetHeightR(), integralR, lastErrorR);
}

int ImplementRooier::computeTarget(int actualHeight, float& integral, float& lastError) {
    float target = float(setpoint + offset);
    // Named pidError, not error: the member `error` is the bang-bang margin
    // (EEPROM-loaded, used by Adjust()), an unrelated quantity this would shadow.
    float pidError = target - float(actualHeight);

    integral += pidError;
    // Clamp the integral term to prevent windup, same class of guard as
    // ImplementPlanter's D-term clamp -- there's no legacy fragment to
    // match here (this whole PID loop is reconstructed), but an unclamped
    // integral is a real defect in any from-scratch PID design.
    if (integral > 1000.0f) {
        integral = 1000.0f;
    }
    else if (integral < -1000.0f) {
        integral = -1000.0f;
    }

    float derivative = pidError - lastError;
    lastError = pidError;

    float correction = (float(kp) / 100.0f) * pidError
                      + (float(ki) / 100.0f) * integral
                      + (float(kd) / 100.0f) * derivative;

    return int(target + correction);
}

// ------------------------------
// Method for adjusting implement
// ------------------------------
void ImplementRooier::Adjust(byte mode, int direction) {
    int actualL, actualR;
    byte pwm;

    if (mode < 2) {  // Auto or Hold
        actualL = GetHeightL();
        actualR = GetHeightR();
        pwm = autoPwm;
    }
    else {  // Manual or Calibration
        actualL = targetL - (direction * (error + 1));
        actualR = targetR - (direction * (error + 1));
        pwm = manPwm;
    }

    bool drivingL = adjustSide(actualL, targetL, pwm, OUTPUT_UP_L_8, OUTPUT_DOWN_L_8,
                                lastHeightL, shutoffWideL, shutoffNarrowL, shutoffTimerL);
    bool drivingR = adjustSide(actualR, targetR, pwm, OUTPUT_UP_R_8, OUTPUT_DOWN_R_8,
                                lastHeightR, shutoffWideR, shutoffNarrowR, shutoffTimerR);

    digitalWrite(OUTPUT_LED_8, (drivingL || drivingR) ? HIGH : LOW);
}

// ----------------------------------------------------------
// Method for driving a single side's shutoff-latch bang-bang
// ----------------------------------------------------------
bool ImplementRooier::adjustSide(int actual, int target, byte pwm, byte outputUp, byte outputDown,
                                  int& lastHeight, bool& shutoffWide, bool& shutoffNarrow, unsigned long& shutoffTimer) {
    bool driving = false;

    //---------------------------
    // Actual below target -> up
    //---------------------------
    if (actual < target - error && !shutoffNarrow) {
        analogWrite(outputUp, pwm);
        analogWrite(outputDown, 0);
        driving = true;

        // End shutoff
        if (actual != lastHeight) {
            shutoffTimer = millis();
        }

        if (shutoffWide) {
            shutoffWide = false;
            shutoffNarrow = false;
        }

        if (millis() - shutoffTimer > shutoffTime) {
            shutoffNarrow = true;
            shutoffWide = false;
        }
    }
    //-----------------------------
    // Actual above target -> down
    //-----------------------------
    else if (actual > target + error && !shutoffWide) {
        analogWrite(outputDown, pwm);
        analogWrite(outputUp, 0);
        driving = true;

        // End shutoff
        if (actual != lastHeight) {
            shutoffTimer = millis();
        }

        if (shutoffNarrow) {
            shutoffWide = false;
            shutoffNarrow = false;
        }

        if (millis() - shutoffTimer > shutoffTime) {
            shutoffNarrow = false;
            shutoffWide = true;
        }
    }
    //----------------
    // Target reached
    //----------------
    else {
        analogWrite(outputUp, 0);
        analogWrite(outputDown, 0);

        shutoffTimer = millis();
    }

    lastHeight = actual;
    return driving;
}

// -----------------------------
// Method for stopping implement
// -----------------------------
void ImplementRooier::Stop() {
    analogWrite(OUTPUT_UP_L_8, 0);
    analogWrite(OUTPUT_DOWN_L_8, 0);
    analogWrite(OUTPUT_UP_R_8, 0);
    analogWrite(OUTPUT_DOWN_R_8, 0);
    digitalWrite(OUTPUT_LED_8, LOW);
}

// ----------------------------------------------
// Method for measuring actual implement height
// ----------------------------------------------
int ImplementRooier::getActualHeight(byte pin, const int* calibrationData) {
    // Read analog input, averaged over 8 samples. The legacy source summed
    // an initial read plus 8 more (9 total) and divided by 8 -- a real
    // off-by-one averaging bug, fixed here to a plain 8-sample average.
    int sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(pin);
    }
    int readRaw = sum / 8;

    int i = 0;

    // Loop through calibrationdata
    while (i < 2 && readRaw > calibrationData[i]) {
        i++;
    }

    if (i == 0) {
        i++;
    }

    // Interpolate calibrationdata
    float a = readRaw - calibrationData[i - 1];
    float b = calibrationData[i] - calibrationData[i - 1];
    float c = positionCalibrationPoints[i] - positionCalibrationPoints[i - 1];
    float d = positionCalibrationPoints[i - 1];

    // b is zero when two calibration points hold the same reading -- same
    // degenerate-calibration guard already established in ImplementScraper/
    // ImplementPlanter/ImplementPlough.
    if (b == 0.0f) {
        return int(d);
    }
    return int(((a * c) / b) + d);
}

// ----------------------------------------------
// Method for reading calibrationdata from EEPROM
// ----------------------------------------------
bool ImplementRooier::readCalibrationData() {
    // Read amount of startups and add 1
    EEPROM.write(0, EEPROM.read(0) + 1);

    if (EEPROM.read(200) != 255 || EEPROM.read(217) != 255 || EEPROM.read(220) != 255) {
        for (int i = 0; i < 3; i++) {
            int k = 2 * i;
            positionCalibrationDataL[i] = word(EEPROM.read(k + 200), EEPROM.read(k + 201));
            positionCalibrationDataR[i] = word(EEPROM.read(k + 206), EEPROM.read(k + 207));
        }

        manPwm = EEPROM.read(212);
        autoPwm = EEPROM.read(213);

        kp = EEPROM.read(214);
        ki = EEPROM.read(215);
        kd = EEPROM.read(216);

        error = EEPROM.read(217);
        if (error > 10) {
            error = 2;
        }

        // Signed, so it goes through readInt() rather than word(), which is
        // unsigned and turned every stored negative skew into a large
        // positive number that failed the range check below and reset the
        // skew to 0 on every boot (NeptuneGPS_Triton#100). Same helper
        // ImplementPlough uses.
        skew = readInt(218);
        if (skew > 30 || skew < -30) {
            skew = 0;
        }

        setpoint = EEPROM.read(220);
        if (setpoint < 1 || setpoint > 99) {
            setpoint = 50;
        }

        offset = readInt(221);   // signed, as skew above
        if (offset > 30 || offset < -30) {
            offset = 0;
        }
    }
    else {
        return false;
    }
    return true;
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementRooier::writeCalibrationData() {
    for (int i = 0; i < 3; i++) {
        int k = 2 * i;
        EEPROM.write(k + 200, highByte(positionCalibrationDataL[i]));
        EEPROM.write(k + 201, lowByte(positionCalibrationDataL[i]));
        EEPROM.write(k + 206, highByte(positionCalibrationDataR[i]));
        EEPROM.write(k + 207, lowByte(positionCalibrationDataR[i]));
    }

    EEPROM.write(212, manPwm);
    EEPROM.write(213, autoPwm);

    EEPROM.write(214, kp);
    EEPROM.write(215, ki);
    EEPROM.write(216, kd);

    EEPROM.write(217, error);

    writeInt(skew, 218);  // 218 - 219, signed; see readCalibrationData()

    EEPROM.write(220, byte(setpoint));

    writeInt(offset, 221);  // 221 - 222, signed

#ifdef DEBUG
    Serial.println("Calibration data written");
#endif
}

// ---------------------------------------------
// Method for wiping calibrationdata from EEPROM
// ---------------------------------------------
void ImplementRooier::wipeCalibrationData() {
    for (int i = 1; i < 255; i++) {
        EEPROM.write(i, 255);
    }

#ifdef DEBUG
    Serial.println("Calibration data wiped");
#endif
}

//---------------------------------------------------
//Method for printing calibration data to serial port
//---------------------------------------------------
void ImplementRooier::PrintCalibrationData() {
    Serial.println("Using following data:");
    Serial.println("--------------------------");
    Serial.println("Height calibration data L");
    for (int i = 0; i < 3; i++) {
        Serial.print(positionCalibrationDataL[i]);
        Serial.print(", ");
        Serial.println(positionCalibrationPoints[i]);
    }
    Serial.println("--------------------------");

    Serial.println("Height calibration data R");
    for (int i = 0; i < 3; i++) {
        Serial.print(positionCalibrationDataR[i]);
        Serial.print(", ");
        Serial.println(positionCalibrationPoints[i]);
    }
    Serial.println("--------------------------");

    Serial.println("KP / KI / KD");
    Serial.println(kp);
    Serial.println(ki);
    Serial.println(kd);
    Serial.println("--------------------------");

    Serial.println("PWM auto");
    Serial.println(autoPwm);
    Serial.println("--------------------------");

    Serial.println("PWM manual");
    Serial.println(manPwm);
    Serial.println("--------------------------");

    Serial.println("Error margin");
    Serial.println(error);
    Serial.println("--------------------------");

    Serial.println("Skew");
    Serial.println(skew);
    Serial.println("--------------------------");

    Serial.println("Setpoint / offset");
    Serial.println(setpoint);
    Serial.println(offset);
    Serial.println("--------------------------");
}

// ---------------------------------------
// Method for reading int data from EEPROM
// ---------------------------------------
// Signed 16-bit, byte for byte in host order. Every signed value this class
// persists goes through here and writeInt() below rather than through
// word()/highByte(), which are unsigned and silently lose the sign
// (NeptuneGPS_Triton#100). Copied from ImplementPlough, which has always
// stored its own offset this way.
short int ImplementRooier::readInt(uint16_t addr) {
    union {
        byte b[2];
        short int i;
    } data;

    for (short int i = 0; i < 2; i++) {
        data.b[i] = EEPROM.read(addr + i);
    }
    return data.i;
}

// -------------------------------------
// Method for writing int data to EEPROM
// -------------------------------------
void ImplementRooier::writeInt(short int x, uint16_t addr) {
    union {
        byte b[2];
        short int i;
    } data;

    data.i = x;
    for (short int i = 0; i < 2; i++) {
        EEPROM.write(addr + i, data.b[i]);
    }
}

}  // namespace triton
