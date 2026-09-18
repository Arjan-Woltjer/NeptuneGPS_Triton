/*
  ImplementKipper - a library for the MeijWorks steering-axle implement
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
#include "ImplementKipper.hpp"

namespace triton
{

//------------
// Constructor
//------------
ImplementKipper::ImplementKipper(VehicleTractor* tractor) {
    // Pin configuration
    // Outputs
    pinMode(OUTPUT_WIDE_4, OUTPUT);
    pinMode(OUTPUT_NARROW_4, OUTPUT);
    pinMode(OUTPUT_BYPASS_4, OUTPUT);
    pinMode(OUTPUT_LED_4, OUTPUT);

    // Analog IO
    // DEFAULT is an AVR/Teensy-3.x analogReference() constant; Teensy 4.1's
    // core doesn't define it at all -- guarded so the call still fires on
    // boards where it's meaningful, and is simply skipped on Teensy 4.1,
    // matching the same fix already applied to every sibling ImplementX.
#ifdef DEFAULT
    analogReference(DEFAULT);
#endif
    pinMode(STEER_SENS_PIN_4, INPUT);
    digitalWrite(STEER_SENS_PIN_4, LOW);

    pinMode(ANGLE_SENS_PIN_4, INPUT);
    digitalWrite(ANGLE_SENS_PIN_4, LOW);

    // Get calibration data from EEPROM otherwise use defaults
    if (!readCalibrationData()) {
        // Default steer calibration set
        steerCalibrationData[0] = 201;
        steerCalibrationData[1] = 428;
        steerCalibrationData[2] = 687;

        // Default angle calibration set
        angleCalibrationData[0] = 201;
        angleCalibrationData[1] = 428;
        angleCalibrationData[2] = 687;

        // PID constants
        kp = 50;
        ki = 50;
        kd = 0;

        // offset
        offset = 0;

#ifdef DEBUG
        Serial.println("No calibration data found");
#endif
    }

    // Calibration points for steer and angle
    steerCalibrationPoints[0] = -30;
    steerCalibrationPoints[1] = 0;
    steerCalibrationPoints[2] = 30;
    steer = 0;
    lastSteer = 0;

    angleCalibrationPoints[0] = -45;
    angleCalibrationPoints[1] = 0;
    angleCalibrationPoints[2] = 45;
    angle = 0;

    // Setpoint of adjust loop
    setpoint = 0;

    // mode is never assigned by any setter on this class outside Update() --
    // the legacy source read it uninitialized if Adjust() were ever called
    // before the first Update() (e.g. from a calibration wizard step run
    // right after construction). Seeded to 0, same fix already applied to
    // ImplementPlanter's own equivalent member.
    mode = 0;

    // PID integration and differentiation intervals
    for (int i = 0; i < 50; i++) {
        angleHist[i] = 0;
    }
    angleSum = 0;  // Running sum of angleHist
    angleAvg = 0;  // Average of sum

    dangle = 0;  // Delta angle

    histCount = 0;   // Counter of sum
    histTime = 25;   // Integration time (seconds * 5) == 5 in this case

    // PID variables
    P = 0;
    I = 0;
    D = 0;

    // End shutoff timers
    shutoffTime = SHUTOFF_4;
    shutoffWide = false;
    shutoffNarrow = false;
    shutoffTimer = millis();

    // Update timer
    updateAge = millis();

    this->tractor = tractor;

#ifdef DEBUG
    PrintCalibrationData();
#endif
}

// ----------------------------------
// Method for updating implement data
// ----------------------------------
void ImplementKipper::Update(byte mode) {
    this->mode = mode;

    // Update angle, steer and setpoint every 25ms. The legacy source
    // alternated which sensor it stored each tick (while still reading --
    // but discarding -- the other channel, presumably to let the ADC mux
    // settle between reads); that discarded-read nuance can't be verified
    // against real hardware here, so both are simply read and stored every
    // tick, matching the same simplification already applied to
    // Rooierbesturing's ImplementRooier::Update().
    if (millis() - updateAge >= 25) {
        angle = getActualAngle();
        steer = getActualSteer();
        speed = tractor->GetSpeedKmh();

        setSetpoint();
        updateAge = millis();
    }
}

// ------------------------------
// Method for adjusting implement
// ------------------------------
void ImplementKipper::Adjust(int direction) {
    int actualSteer;

    if (mode == 0) {  // Auto
        actualSteer = steer;
    }
    else {  // Manual, Hold, or Calibration
        actualSteer = setpoint - direction;
    }

    // Adjust tree
    //---------------------------
    // Actual steer < setpoint
    //---------------------------
    if (actualSteer < setpoint && !shutoffNarrow) {
        digitalWrite(OUTPUT_WIDE_4, LOW);
        digitalWrite(OUTPUT_NARROW_4, HIGH);

        digitalWrite(OUTPUT_LED_4, HIGH);

        // End shutoff
        if (steer != lastSteer) {
            shutoffTimer = millis();
        }

        if (shutoffWide) {
            shutoffNarrow = false;
            shutoffWide = false;
        }

        if (millis() - shutoffTimer > shutoffTime) {
            shutoffNarrow = true;
            shutoffWide = false;
        }
    }
    //---------------------------
    // Actual steer > setpoint
    //---------------------------
    else if (actualSteer > setpoint && !shutoffWide) {
        digitalWrite(OUTPUT_WIDE_4, HIGH);
        digitalWrite(OUTPUT_NARROW_4, LOW);

        digitalWrite(OUTPUT_LED_4, HIGH);

        // End shutoff
        if (steer != lastSteer) {
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
    //-----------------
    // Setpoint reached
    //-----------------
    else {
        digitalWrite(OUTPUT_WIDE_4, LOW);
        digitalWrite(OUTPUT_NARROW_4, LOW);

        digitalWrite(OUTPUT_BYPASS_4, LOW);

        digitalWrite(OUTPUT_LED_4, LOW);
        shutoffTimer = millis();
    }
    lastSteer = steer;
}

// --------------------------------------------
// Method for measuring actual steering-axle angle
// --------------------------------------------
int ImplementKipper::getActualSteer() {
    // Read analog input
    int readRaw = analogRead(STEER_SENS_PIN_4);
    int i = 0;

    // Loop through calibrationdata
    while (i < 2 && readRaw > steerCalibrationData[i]) {
        i++;
    }

    if (i == 0) {
        i++;
    }

    // Interpolate calibrationdata
    float a = readRaw - steerCalibrationData[i - 1];
    float b = steerCalibrationData[i] - steerCalibrationData[i - 1];
    float c = steerCalibrationPoints[i] - steerCalibrationPoints[i - 1];
    float d = steerCalibrationPoints[i - 1];

    // b is zero when two calibration points hold the same reading -- same
    // degenerate-calibration guard already established in every sibling
    // getActualX(). Not present in the legacy source.
    if (b == 0.0f) {
        return int(d);
    }
    return int(((a * c) / b) + d);
}

//------------------------------------------
// Method for measuring actual hitch/drawbar angle
//------------------------------------------
int ImplementKipper::getActualAngle() {
    // Read analog input
    int readRaw = analogRead(ANGLE_SENS_PIN_4);
    int i = 0;

    // Loop through calibrationdata
    while (i < 2 && readRaw > angleCalibrationData[i]) {
        i++;
    }

    if (i == 0) {
        i++;
    }

    // Interpolate calibrationdata
    float a = readRaw - angleCalibrationData[i - 1];
    float b = angleCalibrationData[i] - angleCalibrationData[i - 1];
    float c = angleCalibrationPoints[i] - angleCalibrationPoints[i - 1];
    float d = angleCalibrationPoints[i - 1];

    if (b == 0.0f) {
        return int(d);
    }
    return int(((a * c) / b) + d);
}

// ---------------------------
// Method for setting setpoint
// ---------------------------
void ImplementKipper::setSetpoint() {
    int previousHistCount;
    int angleAvgLocal;
    int dangleLocal;
    int dangleCalc;
    int dFactor;
    int angleCor = angle + offset;

    // histCount/previousHistCount index a histTime(25)-long ring within the
    // 50-slot angleHist array. histCount is a byte, so on the very first
    // call (histCount==0) "previousHistCount = histCount - 1" wraps to 255
    // -- both the wrong ring position (should be histTime-1=24) and past
    // the end of angleHist[50] entirely, an out-of-bounds read. Handle the
    // wrap explicitly instead of relying on unsigned underflow, same fix
    // already applied to ImplementPlanter::SetSetpoint().
    if (histCount == 0) {
        previousHistCount = histTime - 1;
    }
    else {
        previousHistCount = histCount - 1;
    }

    if (histCount >= histTime) {
        histCount = 0;
        previousHistCount = histTime - 1;
    }

    // Set angle sum, delta and averages
    angleSum = angleSum - angleHist[histCount] + angleCor;
    angleAvgLocal = angleSum / histTime;
    dangleLocal = angleCor - angleHist[previousHistCount];
    dangleCalc = angleCor;  // sqrt(abs(angle))
    dFactor = dangleLocal - dangleCalc;

    // Update angle history
    angleHist[histCount] = angleCor;

    P = float(angleCor) * kp / 100.0f;
    I = float(angleAvgLocal) * ki / 100.0f;
    D = D - (float(dFactor) * kd / 100.0f) * speed;

    // Restrict D
    if (D > 15) {
        D = 15;
    }
    else if (D < -15) {
        D = -15;
    }

    // Reset D
    if (mode) {
        D = 0;
    }

    setpoint = int(P + I + D);

    histCount++;

    // angleAvg/dangle are exposed via no getter today but kept in step with
    // the legacy shape (previously named angle_avg/dangle) for calibration/
    // debug parity if a getter is added later.
    angleAvg = angleAvgLocal;
    dangle = dangleLocal;
}

// ----------------------------------------------
// Method for reading calibrationdata from EEPROM
// ----------------------------------------------
bool ImplementKipper::readCalibrationData() {
    // Read amount of startups and add 1
    EEPROM.write(0, EEPROM.read(0) + 1);

    // Read offset and angle calibration data
    if (EEPROM.read(100) != 255 || EEPROM.read(110) != 255 ||
        EEPROM.read(120) != 255 || EEPROM.read(130) != 255 ||
        EEPROM.read(140) != 255 || EEPROM.read(180) != 255) {

        // Read from eeprom highbyte, then lowbyte, and combine into words
        for (int i = 0; i < 3; i++) {
            int k = 2 * i;
            steerCalibrationData[i] = word(EEPROM.read(k + 100), EEPROM.read(k + 101));
            angleCalibrationData[i] = word(EEPROM.read(k + 110), EEPROM.read(k + 111));
        }

        kp = EEPROM.read(120);
        ki = EEPROM.read(130);
        kd = EEPROM.read(140);

        if (EEPROM.read(180) < 255 || EEPROM.read(181) < 255) {
            // Read offset (2 bytes). Signed, so it goes through readInt()
            // rather than word(), which is unsigned and turned every stored
            // negative offset into a large positive number that failed the
            // range check below and reset the offset to 0 on every boot
            // (NeptuneGPS_Triton#100). Same helper ImplementPlough uses.
            offset = readInt(180);
            if (offset > 20 || offset < -20) {
                offset = 0;
            }
        }
        else {
            offset = 0;
        }
    }
    else {
        return false;
    }
    return true;
}

//---------------------------------------------------
//Method for printing calibration data to serial port
//---------------------------------------------------
void ImplementKipper::PrintCalibrationData() {
    Serial.println("Times started");
    Serial.println(EEPROM.read(0));
    Serial.println("-------------------------------");

    Serial.println("Using following data:");
    Serial.println("-------------------------------");
    Serial.println("Steer calibration data");
    for (int i = 0; i < 3; i++) {
        Serial.print(steerCalibrationData[i]);
        Serial.print(", ");
        Serial.println(steerCalibrationPoints[i]);
    }
    Serial.println("-------------------------------");

    Serial.println("Angle calibration data");
    for (int i = 0; i < 3; i++) {
        Serial.print(angleCalibrationData[i]);
        Serial.print(", ");
        Serial.println(angleCalibrationPoints[i]);
    }
    Serial.println("-------------------------------");

    Serial.println("KP");
    Serial.println(kp);
    Serial.println("-------------------------------");

    Serial.println("KI");
    Serial.println(ki);
    Serial.println("-------------------------------");

    Serial.println("KD");
    Serial.println(kd);
    Serial.println("-------------------------------");

    Serial.println("Offset");
    Serial.println(offset);
    Serial.println("-------------------------------");
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementKipper::writeCalibrationData() {
    // Write each byte separately to the memory first the data then the points
    for (int i = 0; i < 3; i++) {
        int k = 2 * i;
        EEPROM.write(k + 100, highByte(steerCalibrationData[i]));
        EEPROM.write(k + 101, lowByte(steerCalibrationData[i]));
        EEPROM.write(k + 110, highByte(angleCalibrationData[i]));
        EEPROM.write(k + 111, lowByte(angleCalibrationData[i]));
    }

    EEPROM.write(120, kp);
    EEPROM.write(130, ki);
    EEPROM.write(140, kd);

    writeInt(offset, 180);  // 180 - 181, signed; see readCalibrationData()

#ifdef DEBUG
    Serial.println("Calibration data written");
#endif
}

// ---------------------------------------------
// Method for wiping calibrationdata from EEPROM
// ---------------------------------------------
void ImplementKipper::wipeCalibrationData() {
    // Write 255 into all memory registers
    for (int i = 1; i < 255; i++) {
        EEPROM.write(i, 255);
    }

#ifdef DEBUG
    Serial.println("Calibration data wiped");
#endif
}

// ---------------------------------------
// Method for reading int data from EEPROM
// ---------------------------------------
// Signed 16-bit, byte for byte in host order. Every signed value this class
// persists goes through here and writeInt() below rather than through
// word()/highByte(), which are unsigned and silently lose the sign
// (NeptuneGPS_Triton#100). Copied from ImplementPlough, which has always
// stored its own offset this way.
short int ImplementKipper::readInt(byte addr) {
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
void ImplementKipper::writeInt(short int x, byte addr) {
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
