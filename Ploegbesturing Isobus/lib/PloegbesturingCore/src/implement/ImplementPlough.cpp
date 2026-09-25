/*
  ImplementPlough - a library for controlling a plough
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
#include "ImplementPlough.hpp"

namespace triton
{

//------------
// Constructor
//------------
ImplementPlough::ImplementPlough(Stream* serialDebug, GuidanceSource* guidance) {
    // Pin configuration
    // Inputs
    pinMode(PLOUGHSIDE_PIN_2, INPUT);
#ifdef TEENSY
    digitalWrite(PLOUGHSIDE_PIN_2, HIGH);
#else
#ifdef TEENSYPROTO
    digitalWrite(PLOUGHSIDE_PIN_2, HIGH);
#else
    digitalWrite(PLOUGHSIDE_PIN_2, LOW);
#endif
#endif

    // Outputs
    pinMode(OUTPUT_WIDE_2, OUTPUT);
    pinMode(OUTPUT_NARROW_2, OUTPUT);
    pinMode(OUTPUT_BYPASS_2, OUTPUT);
    pinMode(OUTPUT_LED_2, OUTPUT);

    // Analog IO
    // DEFAULT is an AVR/Teensy-3.x analogReference() constant; Teensy 4.1's
    // core doesn't define it at all (a real build-breaking gap found while
    // verifying this port against real hardware headers) -- guarded so the
    // call still fires on boards where it's meaningful, and is simply skipped
    // on Teensy 4.1, where the ADC's default reference already applies.
#ifdef DEFAULT
    analogReference(DEFAULT);
#endif
    pinMode(POSITION_SENS_PIN_2, INPUT);
    digitalWrite(POSITION_SENS_PIN_2, LOW);
#ifdef ROTATION
    pinMode(ROTATION_SENS_PIN_2, INPUT);
    digitalWrite(ROTATION_SENS_PIN_2, LOW);
#endif

    // Calibration points for position and xte
    positionCalibrationPoints[0] = 34;
    positionCalibrationPoints[1] = 42;
    positionCalibrationPoints[2] = 50;
    position = 0;
    lastPosition = 0;

#ifdef ROTATION
    rotationCalibrationPoints[0] = 0;
    rotationCalibrationPoints[1] = 45;
    rotationCalibrationPoints[2] = 90;
    rotation = 0;
#endif

    // Setpoint of adjust loop
    setpoint = 0;

    // End shutoff timers
    shutoffTime = SHUTOFF_2;
    shutoffNarrow = false;
    shutoffWide = false;
    shutoffTimer = millis();

    // Update timer
    updateAge = millis();
    lastXteFix = 0;
    xte = 0;
    updateFlag = false;

    // Connected classes
    this->guidance = guidance;
    this->serialDebug = serialDebug;

    // Get calibration data from EEPROM otherwise use defaults
    if (!readCalibrationData()) {
        // Defaults
        // offset calibration
        for (int i = 0; i < 3; i++) {
            positionCalibrationData[i] = kDefaultPositionCalibration[i];
        }

#ifdef ROTATION
        // rotation calibration
        rotationCalibrationData[0] = 200;
        rotationCalibrationData[1] = 450;
        rotationCalibrationData[2] = 700;
#endif

        // PID constant
        kp = 100;

        // pwm values for manual and auto
#ifdef PWM_MAN
        manPwm = 90;
#else
        manPwm = 255;
#endif

#ifdef PWM_AUTO
        autoPwm = 70;
#else
        autoPwm = 255;
#endif

        // error margin
        error = 2;

        // side_swap
        swap = false;

        // amount of shares
        shares = 4;

        // maximum correction
        maxCorrection = 50;

#ifdef DEBUG
        this->serialDebug->println("No calibration data found");
#endif
    }

    // Get latest offset from EEPROM
    readOffset();

#ifdef DEBUG
    this->serialDebug->println("Done...");
#endif
}

// --------------------------------------------
// Method for updating implement data using guidance
// --------------------------------------------
void ImplementPlough::Update(byte mode, short int buttons) {
    // Update offset, xte, rotation and setpoint
    if (millis() - updateAge >= 200) {
        if (mode < 2) {
            // Use buttons to set offset while in automatic or hold mode
            setOffset(buttons);
        }
        updateAge = millis();
    }

    // Update XTE every XTE fix
    if (guidance->GetXteTimestamp() - lastXteFix > 0) {
        // Update xte, rotation and setpoint
        xte = guidance->GetXte();

        setSetpoint();

        lastXteFix = guidance->GetXteTimestamp();
    }

    // Update analog inputs
    if (updateFlag) {
        updateFlag = false;

        position = (position + getActualPosition()) / 2;
#ifdef ROTATION
        getActualRotation();
#endif
    }
    else {
        updateFlag = true;

#ifdef ROTATION
        // Renamed from a leading-underscore member during the PascalCase/camelCase
        // migration -- this used to reference an undeclared local `rotation` instead
        // of the member (a real bug, but inert since ROTATION is never defined). Now
        // that the member itself is named `rotation`, this resolves correctly.
        rotation = (rotation + getActualRotation()) / 2;
#endif
        getActualPosition();
    }
}

// ---------------------------
// Method for setting setpoint
// ---------------------------
void ImplementPlough::setSetpoint() {
    // calculate proportional gain (kp should be 1)
    int pe = xte * (float(kp) / 100);

    // maximise correction to set maximum
    if (pe <= -maxCorrection) {
        pe = -maxCorrection;
    }
    else if (pe >= maxCorrection) {
        pe = maxCorrection;
    }

    // calculate setpoint
    setpoint = offset + ((GetSide() * 2) - 1) * pe;
}

// ------------------------------
// Method for adjusting implement
// ------------------------------
void ImplementPlough::Adjust(byte mode, short int direction) {
    short int actualPosition = 0;
    byte pwm = 0;

    switch (mode) {
        case 0:
            actualPosition = position;
            pwm = autoPwm;
            break;
        case 1:
            actualPosition = setpoint;
            pwm = autoPwm;
            break;
        case 2:
        case 3:
            actualPosition = setpoint - (direction * (error + 1));
            lastPosition = setpoint; // to counter shutoff when in manual mode
            pwm = manPwm;
    }

    // ---------------------------
    // Adjust tree including error
    // ---------------------------
    if (actualPosition < setpoint - error && (!shutoffNarrow || (mode >= 2))) {
        // Setpoint < actual position
        Narrower(pwm);

        // End shutoff
        if (actualPosition != lastPosition) {
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
    else if (actualPosition > setpoint + error && (!shutoffWide || (mode >= 2))) {
        // Setpoint < actual position
        Wider(pwm);

        // End shutoff
        if (actualPosition != lastPosition) {
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
    else {
        // Setpoint reached
        Stop();

        // Reset shutoff timer
        shutoffTimer = millis();
    }
    lastPosition = actualPosition;
}

// ------------------------------------
// Method for moving implement narrower
// ------------------------------------
void ImplementPlough::Narrower(byte pwm) {
    analogWrite(OUTPUT_WIDE_2, 0);
    analogWrite(OUTPUT_NARROW_2, pwm);
    //analogWrite(OUTPUT_BYPASS_2, pwm);
    digitalWrite(OUTPUT_LED_2, HIGH);
}

// ---------------------------------
// Method for moving implement wider
// ---------------------------------
void ImplementPlough::Wider(byte pwm) {
    analogWrite(OUTPUT_WIDE_2, pwm);
    analogWrite(OUTPUT_NARROW_2, 0);
    //analogWrite(OUTPUT_BYPASS_2, pwm);
    digitalWrite(OUTPUT_LED_2, HIGH);
}

// ------------------------------
// Method for stopping implement
// ------------------------------
void ImplementPlough::Stop() {
    analogWrite(OUTPUT_WIDE_2, 0);
    analogWrite(OUTPUT_NARROW_2, 0);
    //analogWrite(OUTPUT_BYPASS_2, 0);
    digitalWrite(OUTPUT_LED_2, LOW);
}

// --------------------------------------------
// Method for measuring actual implement offset
// --------------------------------------------
short int ImplementPlough::getActualPosition() {
    // Read analog input
    short int readRaw = analogRead(POSITION_SENS_PIN_2);
    float actualPosition;
    short int i = 0;

    if (positionCalibrationData[0] < positionCalibrationData[1]) {
        if (readRaw < positionCalibrationData[1]) {
            i = 1;
        }
        else {
            i = 2;
        }
    }
    else {
        if (readRaw < positionCalibrationData[1]) {
            i = 2;
        }
        else {
            i = 1;
        }
    }

    // Interpolate calibration data
    float a = readRaw - positionCalibrationData[i - 1];
    float b = positionCalibrationData[i] - positionCalibrationData[i - 1];
    float c = positionCalibrationPoints[i] - positionCalibrationPoints[i - 1];
    float d = positionCalibrationPoints[i - 1];

    // b is zero when two calibration points hold the same reading, which a
    // disconnected or seized position potentiometer produces directly: the
    // wizard latches analogRead() at each step without checking the captures
    // differ. The result would be inf or NaN, and narrowing either to short int
    // below is undefined behaviour -- so this cannot be left to the caller.
    // Fall back to the segment's own start point, the nearest defensible value.
    if (b == 0.0f) {
        actualPosition = d;
    }
    else {
        actualPosition = (((a * c) / b) + d);
    }

    return actualPosition * shares;
}

#ifdef ROTATION
//------------------------------------------
// Method for measuring actual implement XTE
//------------------------------------------
short int ImplementPlough::getActualRotation() {
    // Read analog input
    short int readRaw = analogRead(ROTATION_SENS_PIN_2);
    short int actualRotation;
    short int i = 0;

    if (rotationCalibrationData[0] < rotationCalibrationData[1]) {
        if (readRaw < rotationCalibrationData[1]) {
            i = 1;
        }
        else {
            i = 2;
        }
    }
    else {
        if (readRaw < rotationCalibrationData[1]) {
            i = 2;
        }
        else {
            i = 1;
        }
    }

    // Interpolate calibration data
    float a = readRaw - rotationCalibrationData[i - 1];
    float b = rotationCalibrationData[i] - rotationCalibrationData[i - 1];
    float c = rotationCalibrationPoints[i] - rotationCalibrationPoints[i - 1];
    float d = rotationCalibrationPoints[i - 1];

    // Calculate actual implement offset
    actualRotation = (((a * c) / b) + d);

    return actualRotation;
}
#endif

// ---------------------------------------------------------
// Method for setting implement offset and writing to EEPROM
// ---------------------------------------------------------
void ImplementPlough::setOffset(short int correction) {
    if (correction) {
        // Clamp at the limits (#152). offset is the plough's total working
        // width in cm, so an operator holding Wider at the widest setting
        // must stay there; it used to jump to the middle (shares * 40) instead.
        // readOffset()'s reset to the middle is a different case -- a blank or
        // corrupt EEPROM -- and stays as it is.
        const int lowest  = shares * 20;
        const int highest = shares * 60;
        int wanted = offset + correction;
        if (wanted > highest) wanted = highest;
        if (wanted < lowest)  wanted = lowest;

        // A press at a limit changes nothing, so it costs no EEPROM write.
        if (wanted != offset) {
            offset = static_cast<short int>(wanted);
            writeInt(offset, 66);
        }
    }
}

// -----------------------------------------------
// Method for reading implement offset from EEPROM
// -----------------------------------------------
void ImplementPlough::readOffset() {
    offset = readInt(66);
    if (offset == -1 || offset > shares * 60 || offset < shares * 20) {
        offset = shares * 40;
    }
}

// ----------------------------------------------
// Method for reading calibrationdata from EEPROM
// ----------------------------------------------
boolean ImplementPlough::readCalibrationData() {
    // Read amount of startups and add 1
    EEPROM.write(0, EEPROM.read(0));

    // Read offset and XTE calibration data
    if (EEPROM.read(40) != 255 ||
#ifdef ROTATION
        EEPROM.read(46) != 255 ||
#endif
        EEPROM.read(52) != 255 || EEPROM.read(54) != 255 ||
        EEPROM.read(56) != 255 || EEPROM.read(58) != 255 ||
        EEPROM.read(60) != 255 || EEPROM.read(62) != 255 ||
        EEPROM.read(64) != 255) {

        // Read from eeprom highbyte, then lowbyte, and combine into words
        short int storedPosition[3];
        for (int i = 0; i < 3; i++) {
            int k = 2 * i;
            // 100 - 101 and 110 - 111
            storedPosition[i] = word(EEPROM.read(k + 40), EEPROM.read(k + 41));
#ifdef ROTATION
            rotationCalibrationData[i] = word(EEPROM.read(k + 46), EEPROM.read(k + 47));
#endif
        }

        // These three feed the divisor in getActualPosition(), and were
        // previously accepted as-is -- an arbitrary 16-bit value from EEPROM,
        // even though the sensor is a 10-bit ADC so anything above 1023 is
        // physically impossible. Every other persisted field here is range
        // checked; these were not. Adjacent points also have to differ, or the
        // interpolation divides by zero.
        boolean positionValid = true;
        for (int i = 0; i < 3; i++) {
            if (storedPosition[i] < 0 || storedPosition[i] > kAdcMaxCount) {
                positionValid = false;
            }
        }
        if (storedPosition[0] == storedPosition[1] ||
            storedPosition[1] == storedPosition[2]) {
            positionValid = false;
        }

        // The defaults are otherwise only applied when this function returns
        // false, so they have to be written explicitly here -- leaving the
        // members untouched would leave them uninitialised.
        for (int i = 0; i < 3; i++) {
            positionCalibrationData[i] = positionValid ? storedPosition[i]
                                                       : kDefaultPositionCalibration[i];
        }


#ifdef PWM_MAN
        manPwm = EEPROM.read(52);
#else
        manPwm = 254;
#endif

#ifdef PWM_AUTO
        autoPwm = EEPROM.read(54);
#else
        autoPwm = 254;
#endif

#ifdef PID_KP
        kp = EEPROM.read(56);
#else
        kp = 100;
#endif

        //Read error max == 10 cm
        if (EEPROM.read(58) < 10) {
            // Read error
            error = EEPROM.read(58);
        }
        else {
            error = 2;  //default to 2
        }


        //Read maximum correction
        if (EEPROM.read(60) < 10) {
            // Read maximum correction
            maxCorrection = EEPROM.read(60);
        }
        else {
            maxCorrection = 50;  //default to 4
        }

        //Read swap 1 or 0
        if (EEPROM.read(62) < 2) {
            // Read swap
            swap = EEPROM.read(62);
        }
        else {
            swap = false;  //default to 0
        }

        //Read number of shares
        if (EEPROM.read(64) < 10) {
            // Read number of shares
            shares = EEPROM.read(64);
        }
        else {
            shares = 4;  //default to 4
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
void ImplementPlough::PrintCalibrationData() {
    // Print amount of times started
    serialDebug->println("Times started");
    serialDebug->println(EEPROM.read(0));
    serialDebug->println("--------------------------");

    // Printing calibration data to serial port
    serialDebug->println("Using following data:");
    serialDebug->println("--------------------------");
    serialDebug->println("Offset calibration data");
    for (int i = 0; i < 3; i++) {
        serialDebug->print(positionCalibrationData[i]);
        serialDebug->print(", ");
        serialDebug->println(positionCalibrationPoints[i]);
    }
    serialDebug->println("--------------------------");

#ifdef ROTATION
    serialDebug->println("Rotation calibration data");
    for (int i = 0; i < 3; i++) {
        serialDebug->print(rotationCalibrationData[i]);
        serialDebug->print(", ");
        serialDebug->println(rotationCalibrationPoints[i]);
    }
    serialDebug->println("--------------------------");
#endif

    serialDebug->println("Number of shares");
    serialDebug->println(shares);
    serialDebug->println("--------------------------");

#ifdef PID_KP
    serialDebug->println("kp");
    serialDebug->println(-kp);
    serialDebug->println("--------------------------");
#endif

#ifdef PWM_AUTO
    serialDebug->println("PWM auto");
    serialDebug->println(autoPwm);
    serialDebug->println("--------------------------");
#endif

#ifdef PWM_MAN
    serialDebug->println("PWM manual");
    serialDebug->println(manPwm);
    serialDebug->println("--------------------------");
#endif

    serialDebug->println("error margin");
    serialDebug->println(error);
    serialDebug->println("--------------------------");

    serialDebug->println("error ploughside");
    serialDebug->println(swap);
    serialDebug->println("--------------------------");

    serialDebug->println("Maximum correction");
    serialDebug->println(maxCorrection);
    serialDebug->println("--------------------------");
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementPlough::writeCalibrationData() {
    // Write each byte separately to the memory first the data then the points
    for (int i = 0; i < 3; i++) {
        int k = 2 * i;
        EEPROM.write(k + 40, highByte(positionCalibrationData[i])); // 40, 42, 44
        EEPROM.write(k + 41, lowByte(positionCalibrationData[i])); // 41, 42, 45
#ifdef ROTATION
        EEPROM.write(k + 46, highByte(rotationCalibrationData[i])); // 46, 48, 50
        EEPROM.write(k + 47, lowByte(rotationCalibrationData[i])); // 47, 49, 51
#endif
    }

    EEPROM.write(52, manPwm);  //52
    EEPROM.write(54, autoPwm); //54
    EEPROM.write(56, kp);       //56
    EEPROM.write(58, error);    //58
    EEPROM.write(60, maxCorrection);   //60
    EEPROM.write(62, swap);     //62
    EEPROM.write(64, shares);   //64

#ifdef DEBUG
    serialDebug->println("Calibration data written");
#endif
}

// ---------------------------------------
// Method for reading int data from EEPROM
// ---------------------------------------
short int ImplementPlough::readInt(byte addr) {
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
void ImplementPlough::writeInt(short int x, byte addr) {
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
