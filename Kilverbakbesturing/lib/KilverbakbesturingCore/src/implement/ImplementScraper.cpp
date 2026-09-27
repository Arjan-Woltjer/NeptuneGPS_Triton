/*
  ImplementScraper - a library for the MeijWorks scraper (leveling bucket) implement
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
#include "ImplementScraper.hpp"

namespace triton
{

//------------
// Constructor
//------------
ImplementScraper::ImplementScraper(GuidanceSource* guidance) {
#ifdef DEBUG
    Serial.println(S_DIVIDE);
    Serial.println("Initialising scraper");
    Serial.println(S_DIVIDE);
#endif

    // Pin configuration
    // Outputs
    pinMode(OUTPUT_WIDE_5, OUTPUT);
    pinMode(OUTPUT_NARROW_5, OUTPUT);
    pinMode(OUTPUT_BYPASS_5, OUTPUT);
    pinMode(OUTPUT_LED_5, OUTPUT);

    // Analog IO
    // DEFAULT is an AVR/Teensy-3.x analogReference() constant; Teensy 4.1's
    // core doesn't define it at all -- guarded so the call still fires on
    // boards where it's meaningful, and is simply skipped on Teensy 4.1,
    // where the ADC's default reference already applies (same fix already
    // applied to ImplementPlough).
#ifdef DEFAULT
    analogReference(DEFAULT);
#endif
    pinMode(POSITION_SENS_PIN_5, INPUT);
    digitalWrite(POSITION_SENS_PIN_5, LOW);

    // Get latest offset from EEPROM
    readOffset();

    // Calibration points for position and xte
    positionCalibrationPoints[0] = 0;
    positionCalibrationPoints[1] = 10;
    positionCalibrationPoints[2] = 20;
    position = 0;
    lastPosition = 0;

    // Setpoint of adjust loop
    setpoint = 0;

    // End shutoff timers
    shutoffTime = SHUTOFF_5;
    shutoffNarrow = false;
    shutoffWide = false;
    shutoffTimer = millis();

    // Update timer
    updateAge = millis();
    lastGgaFix = 0;

    // Connected classes
    this->guidance = guidance;

    // Get calibration data from EEPROM otherwise use defaults
    if (!readCalibrationData()) {
        // Defaults
        // offset calibration
        positionCalibrationData[0] = 200;
        positionCalibrationData[1] = 450;
        positionCalibrationData[2] = 700;

        // PID constant
        kp = 100;

        // pwm values for manual and auto
#ifdef PWM_MAN
        manPwm = 90;
#else
        manPwm = 254;
#endif

#ifdef PWM_AUTO
        autoPwm = 70;
#else
        autoPwm = 254;
#endif

        // error margin
        error = 2;

        // maximum correction
        maxCorrection = 5;

        // slope
        slope = 0;

        // Reference A/B -- readRefA()/readRefB() are only called from
        // readCalibrationData()'s "found data" branch, so on a genuinely
        // fresh board (this branch) they were never assigned at all in the
        // legacy source: a real uninitialized-memory bug, caught by this
        // module's own tests (calculateDistances() narrowing whatever
        // garbage lAb worked out to into heightRef, an integer member, is
        // undefined behaviour). Both references default to the same null
        // point -- lAb (their distance apart) comes out exactly 0, which
        // calculateDistances()'s own guard below already handles.
        latRa = 0;
        longRa = 0;
        heightRa = 0;
        latRb = 0;
        longRb = 0;
        heightRb = 0;

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
void ImplementScraper::Update(byte mode, int buttons) {
    // Update offset every half a second
    if (millis() - updateAge >= 500) {
        if (mode < 2) {
            // Use buttons to set offset while in automatic or hold mode
            setOffset(buttons);
        }
        updateAge = millis();
    }

    // Update gps height, position, distance to refpoint and setpoint once
    // every GGA fix
    if (guidance->GetGgaTimestamp() - lastGgaFix > 0) {
        // Update altitude and position
        height = altitudeCm();
        latitude  = guidance->GetLatitude();
        longitude = guidance->GetLongitude();

        calculateDistances();

        setSetpoint();

        // Register time of last GGA fix
        lastGgaFix = guidance->GetGgaTimestamp();
    }
}

// ---------------------------
// Method for setting setpoint
// ---------------------------
void ImplementScraper::setSetpoint() {
    int delta = height - heightRef + offset;

#ifdef NOSENS
    setpoint = -delta;
#else
    position = getActualPosition();

    // calculate proportional gain (kp should be 1)
    setpoint = position - delta * (float(kp) / 100);

    // maximise correction to set maximum
    if (setpoint <= -maxCorrection) {
        setpoint = -maxCorrection;
    }
#endif
}

// ------------------------------
// Method for adjusting implement
// ------------------------------
void ImplementScraper::Adjust(byte mode, int direction) {
#ifdef NOSENS
    byte pwm = 255;

    unsigned int absSetpoint = abs(setpoint);
    unsigned int settime = 1;
    unsigned int inputtime = 0;

    if (mode == 0) {
        if (absSetpoint > 0) {
            settime = absSetpoint * 150 + 100;
        }
        else {
            settime = 0;
        }
        inputtime = millis() - guidance->GetGgaTimestamp();
    }
    else {
        setpoint = direction;
        offset = heightRef - height;
    }

    // Adjust tree including error
    //---------------------------
    // Setpoint < actual position
    //---------------------------
    if (setpoint > 0 && inputtime < settime) {
        analogWrite(OUTPUT_WIDE_5, 0);
        analogWrite(OUTPUT_NARROW_5, pwm);
        digitalWrite(OUTPUT_LED_5, HIGH);
    }
    //---------------------------
    // Setpoint > actual position
    //---------------------------
    else if (setpoint < 0 && inputtime < settime) {
        analogWrite(OUTPUT_WIDE_5, pwm);
        analogWrite(OUTPUT_NARROW_5, 0);
        digitalWrite(OUTPUT_LED_5, HIGH);
    }
    //-----------------
    // Setpoint reached
    //-----------------
    else {
        Stop();
    }

#else
    int  actualPosition;
    byte pwm;

    if (mode < 2) {  // Auto or hold
        actualPosition = getActualPosition();
        pwm = autoPwm;
    }
    else {
        actualPosition = setpoint - (direction * (error + 1));
        pwm = manPwm;
    }

    // Adjust tree including error
    //---------------------------
    // Setpoint < actual position
    //---------------------------
    if (actualPosition < setpoint - error && !shutoffNarrow) {
        analogWrite(OUTPUT_WIDE_5, 0);
        analogWrite(OUTPUT_NARROW_5, 255);
        analogWrite(OUTPUT_BYPASS_5, pwm);
        digitalWrite(OUTPUT_LED_5, HIGH);

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
    //---------------------------
    // Setpoint > actual position
    //---------------------------
    else if (actualPosition > setpoint + error && !shutoffWide) {
        analogWrite(OUTPUT_WIDE_5, 255);
        analogWrite(OUTPUT_NARROW_5, 0);
        analogWrite(OUTPUT_BYPASS_5, pwm);
        digitalWrite(OUTPUT_LED_5, HIGH);

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
    //-----------------
    // Setpoint reached
    //-----------------
    else {
        Stop();

        // Reset shutoff timer
        shutoffTimer = millis();
    }
    lastPosition = actualPosition;
#endif
}

// -----------------------------
// Method for stopping implement
// -----------------------------
void ImplementScraper::Stop() {
    analogWrite(OUTPUT_WIDE_5, 0);
    analogWrite(OUTPUT_NARROW_5, 0);
    analogWrite(OUTPUT_BYPASS_5, 0);
    digitalWrite(OUTPUT_LED_5, LOW);
}

// ----------------------------------------------
// Method for measuring actual implement position
// ----------------------------------------------
int ImplementScraper::getActualPosition() {
    // Read analog input
    int readRaw = analogRead(POSITION_SENS_PIN_5);
    float actualPosition;
    int i = 0;

    // Loop through calibrationdata
    while (i < 2 && readRaw > positionCalibrationData[i]) {
        i++;
    }

    if (i == 0) {
        i++;
    }

    // Interpolate calibrationdata
    float a = readRaw - positionCalibrationData[i - 1];
    float b = positionCalibrationData[i] - positionCalibrationData[i - 1];
    float c = positionCalibrationPoints[i] - positionCalibrationPoints[i - 1];
    float d = positionCalibrationPoints[i - 1];

    // b is zero when two calibration points hold the same reading -- same
    // degenerate-calibration guard already added to ImplementPlough/
    // ImplementPlanter/ImplementSprayer. Dead today (NOSENS is always
    // defined, so this function is never called), but this class of bug
    // isn't worth leaving unguarded for whenever a sensored variant is
    // revived.
    if (b == 0.0f) {
        actualPosition = d;
    }
    else {
        actualPosition = (((a * c) / b) + d);
    }

    return actualPosition;
}

// ---------------------------------------------
// Method for calculating XTE and line distances
// ---------------------------------------------
void ImplementScraper::calculateDistances() {
    // Vector AB
    float latAb = DistanceBetween(latRa, longRa, latRa, longRb);
    float longAb = DistanceBetween(latRa, longRa, latRb, longRa);

    if (latRa > latRb) {
        latAb = -latAb;
    }

    if (longRa > longRb) {
        longAb = -longAb;
    }

    // Length of vector AB
    lAb = DistanceBetween(latRa, longRa, latRb, longRb);

    // Vector AC
    float latAc = DistanceBetween(latRa, longRa, latRa, longitude);
    float longAc = DistanceBetween(latRa, longRa, latitude, longRa);

    if (latRa > latitude) {
        latAc = -latAc;
    }

    if (longRa > longitude) {
        longAc = -longAc;
    }

    // lAb is zero before Reference A/B are ever set (both default to (0,0),
    // an erased-EEPROM read -- see readRefA()/readRefB()) or if they're
    // accidentally set to the same point. The legacy source divided by lAb
    // unconditionally here: 0/0 is NaN, and narrowing a NaN float to
    // heightRef (an integer type) below is undefined behaviour -- the same
    // class of bug already guarded against in getActualPosition() above,
    // just reached a different way (a distance of zero rather than a
    // calibration read). Falls back to "on the line, no slope offset" until
    // both references are real, distinct points.
    if (lAb == 0.0f) {
        dAb = 0;
        xAb = 0;
    }
    else {
        // distance along line AB and across to line AB, using inner product
        // with vector AB (x,y) and inner product with vector (y, -x)
        dAb = ((longAb * longAc) + (latAb * latAc)) / lAb;
        xAb = ((latAb * longAc) - (longAb * latAc)) / lAb;
    }

    // calculate reference height
    heightRef = heightRa + (dAb * slope) / 100;
}

// ---------------------------------------------------------
// Method for setting implement offset and writing to EEPROM
// ---------------------------------------------------------
void ImplementScraper::setOffset(int correction) {
    if (correction) {
        if (correction != 2) {
            offset += correction;
        }
        else {
            offset = 0;
        }
    }
}

// -----------------------------------------------
// Method for reading implement offset from EEPROM
// -----------------------------------------------
void ImplementScraper::readOffset() {
    if (EEPROM.read(148) < 255) {
        // Read offset (2 bytes)
        offset = readInt(148);
    }
    else {
        offset = 0;
    }
}

// ------------------------------
// Method for setting Reference A
// ------------------------------
void ImplementScraper::SetRefA() {
    // To EEPROM block from address 150
    setRef(&latRa, &longRa, &heightRa, 150);
}

// ------------------------------
// Method for setting Reference B
// ------------------------------
void ImplementScraper::SetRefB() {
    // To EEPROM block from address 160
    setRef(&latRb, &longRb, &heightRb, 160);

    PrintCalibrationData();
}

// ------------------------------
// Method for reading Reference A
// ------------------------------
void ImplementScraper::readRefA() {
    latRa = readFloat(150);
    longRa = readFloat(154);
    heightRa = readInt(158);
}

// ------------------------------
// Method for reading Reference B
// ------------------------------
void ImplementScraper::readRefB() {
    latRb = readFloat(160);
    longRb = readFloat(164);
    heightRb = readInt(168);
}

// ------------------------------
// Method for setting a Reference
// ------------------------------
void ImplementScraper::setRef(float* lat, float* lon, short int* height, uint16_t addr) {
    *lat = guidance->GetLatitude();
    *lon = guidance->GetLongitude();
    *height = altitudeCm();

    writeFloat(*lat, addr);
    writeFloat(*lon, addr + 4);
    writeInt(*height, addr + 8);

    // Set offset to 0
    setOffset(2);
}

// ------------------------------
// Method for reading a Reference
// ------------------------------
// ----------------------------------------------
// Method for reading calibrationdata from EEPROM
// ----------------------------------------------
bool ImplementScraper::readCalibrationData() {
    // Read amount of startups and add 1
    EEPROM.write(0, EEPROM.read(0) + 1);

    // Read offset and XTE calibration data
    if (EEPROM.read(130) != 255 ||
#ifdef PWM_MAN
        EEPROM.read(136) != 255 ||
#endif
#ifdef PWM_AUTO
        EEPROM.read(138) != 255 ||
#endif
#ifdef PID_KP
        EEPROM.read(140) != 255 ||
#endif
        EEPROM.read(142) != 255 ||
#ifndef NOSENS
        EEPROM.read(144) != 255 ||
#endif
        EEPROM.read(146) != 255) {

        // Read from eeprom highbyte, then lowbyte, and combine into words
        for (int i = 0; i < 3; i++) {
            positionCalibrationData[i] = readInt(130 + i * 2);
        }

#ifdef PWM_MAN
        manPwm = EEPROM.read(136);
#else
        manPwm = 254;
#endif

#ifdef PWM_AUTO
        autoPwm = EEPROM.read(138);
#else
        autoPwm = 254;
#endif

#ifdef PID_KP
        kp = EEPROM.read(140);
#else
        kp = 100;
#endif
        // Read error max == 10 cm
        error = EEPROM.read(142);
        if (error > 10) {
            error = 2;  // default to 2
        }

#ifndef NOSENS
        // Read maximum correction
        maxCorrection = EEPROM.read(144);
        if (maxCorrection > 10) {
            maxCorrection = 5;
        }
#else
        maxCorrection = 30;
#endif

        // Read slope
        slope = readInt(146);
        if (slope > 99 || slope < -99) {
            slope = 0;
        }

        readRefA();
        readRefB();
    }
    else {
        return false;
    }
    return true;
}

//---------------------------------------------------
//Method for printing calibration data to serial port
//---------------------------------------------------
void ImplementScraper::PrintCalibrationData() {
    // Printing calibration data to serial port
    Serial.println("Using following data:");
    Serial.println("--------------------------");
    Serial.println("Offset calibration data");
    for (int i = 0; i < 3; i++) {
        Serial.print(positionCalibrationData[i]);
        Serial.print(", ");
        Serial.println(positionCalibrationPoints[i]);
    }
    Serial.println("--------------------------");

#ifdef PWM_AUTO
    Serial.println("PWM auto");
    Serial.println(autoPwm);
    Serial.println("--------------------------");
#endif

#ifdef PWM_MAN
    Serial.println("PWM manual");
    Serial.println(manPwm);
    Serial.println("--------------------------");
#endif

#ifdef PID_KP
    Serial.println("KP");
    Serial.println(kp);
    Serial.println("--------------------------");
#endif

    Serial.println("Error margin");
    Serial.println(error);
    Serial.println("--------------------------");

#ifndef NOSENS
    Serial.println("Maximum correction");
    Serial.println(maxCorrection);
    Serial.println("--------------------------");
#endif

    Serial.println("Reference A");
    Serial.println(latRa);
    Serial.println(longRa);
    Serial.println(heightRa);
    Serial.println("--------------------------");

    Serial.println("Reference B");
    Serial.println(latRb);
    Serial.println(longRb);
    Serial.println(heightRb);
    Serial.println("--------------------------");

    Serial.println("Slope");
    Serial.println(slope);
    Serial.println("--------------------------");
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementScraper::writeCalibrationData() {
    // Write each byte separately to the memory first the data then the points
    for (int i = 0; i < 3; i++) {
        writeInt(positionCalibrationData[i], 130 + i * 2);
    }

    EEPROM.write(136, manPwm);   // 136
    EEPROM.write(138, autoPwm);  // 138
    EEPROM.write(140, kp);       // 140

    EEPROM.write(142, error);  // 142
#ifndef NOSENS
    EEPROM.write(144, maxCorrection);  // 144
#endif
    writeInt(slope, 146);  // 146

#ifdef DEBUG
    Serial.println("Calibration data written");
#endif
}

// ---------------------------------------------
// Method for wiping calibrationdata from EEPROM
// ---------------------------------------------
void ImplementScraper::wipeCalibrationData() {
    // Wipe calibration data
    for (int i = 1; i < 255; i++) {
        EEPROM.write(i, 255);
    }

#ifdef DEBUG
    Serial.println("Calibration data wiped");
#endif
}

// -----------------------------------------
// Method for reading float data from EEPROM
// -----------------------------------------
float ImplementScraper::readFloat(uint16_t addr) {
    union {
        byte b[4];
        float f;
    } data;
    for (int i = 0; i < 4; i++) {
        data.b[i] = EEPROM.read(addr + i);
    }
    return data.f;
}

// ---------------------------------------
// Method for writing float data to EEPROM
// ---------------------------------------
void ImplementScraper::writeFloat(float x, uint16_t addr) {
    union {
        byte b[4];
        float f;
    } data;
    data.f = x;
    for (int i = 0; i < 4; i++) {
        EEPROM.write(addr + i, data.b[i]);
    }
}

// ---------------------------------------
// Method for reading int data from EEPROM
// ---------------------------------------
short int ImplementScraper::readInt(uint16_t addr) {
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
void ImplementScraper::writeInt(short int x, uint16_t addr) {
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
