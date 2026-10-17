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
#include "EepromStore.hpp"
#include "Interpolation.hpp"

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

        // deadband
        deadband = kDefaultDeadband;

        // side_swap
        swap = false;

        // no bypass valve
        bypass = false;

        // amount of shares
        shares = kDefaultShares;

        // maximum correction, per share
        maxCorrectionPerShare = kDefaultMaxCorrectionPerShareMm;

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
// Method for updating implement data using gps
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
    // bang-bang control: the correction is the cross-track error itself
    int pe = xte;

    // maximise correction to set maximum. The limit is in mm (#163) and the
    // setpoint in whole cm, so a limit with a half centimetre rounds down --
    // never more correction than set.
    const int maxCorrectionCm = GetMaxCorrection() / 10;
    if (pe <= -maxCorrectionCm) {
        pe = -maxCorrectionCm;
    }
    else if (pe >= maxCorrectionCm) {
        pe = maxCorrectionCm;
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
            pwm = kValvePwm;
            break;
        case 1:
            actualPosition = setpoint;
            pwm = kValvePwm;
            break;
        case 2:
        case 3:
            actualPosition = setpoint - (direction * (deadband + 1));
            lastPosition = setpoint; // to counter shutoff when in manual mode
            pwm = kValvePwm;
    }

    // ------------------------------
    // Adjust tree including deadband
    // ------------------------------
    if (actualPosition < setpoint - deadband && (!shutoffNarrow || (mode >= 2))) {
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
    else if (actualPosition > setpoint + deadband && (!shutoffWide || (mode >= 2))) {
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
    driveBypass(bypass ? pwm : 0);
    analogWrite(OUTPUT_LED_2, 255);
}

// ---------------------------------
// Method for moving implement wider
// ---------------------------------
void ImplementPlough::Wider(byte pwm) {
    analogWrite(OUTPUT_WIDE_2, pwm);
    analogWrite(OUTPUT_NARROW_2, 0);
    driveBypass(bypass ? pwm : 0);
    analogWrite(OUTPUT_LED_2, 255);
}

// ------------------------------
// Method for stopping implement
// ------------------------------
void ImplementPlough::Stop() {
    analogWrite(OUTPUT_WIDE_2, 0);
    analogWrite(OUTPUT_NARROW_2, 0);
    driveBypass(0);
    analogWrite(OUTPUT_LED_2, 0);
}

// ----------------------------------------------------------------------
// Bypass valve: with one fitted it is driven with the narrower/wider
// valve and closed at Stop; without one the output stays off.
// ----------------------------------------------------------------------
void ImplementPlough::driveBypass(byte pwm) {
    analogWrite(OUTPUT_BYPASS_2, pwm);
}

// --------------------------------------------
// Method for measuring actual implement offset
// --------------------------------------------
short int ImplementPlough::getActualPosition() {
    // Read analog input
    short int readRaw = analogRead(POSITION_SENS_PIN_2);
    float actualPosition;
    const int i = Interpolation::SegmentAroundMiddle(positionCalibrationData, readRaw);
    actualPosition = Interpolation::Evaluate(positionCalibrationData, positionCalibrationPoints, i, readRaw);

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
    const int i = Interpolation::SegmentAroundMiddle(rotationCalibrationData, readRaw);
    actualRotation = Interpolation::Evaluate(rotationCalibrationData, rotationCalibrationPoints, i, readRaw);

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
            EepromStore::WriteInt16Le(66, offset);
        }
    }
}

// -----------------------------------------------
// Method for reading implement offset from EEPROM
// -----------------------------------------------
void ImplementPlough::readOffset() {
    offset = EepromStore::ReadInt16Le(66);
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
        // 52/54/56 held PWM manual/auto and KP, no longer used; a board
        // that has them written still counts as calibrated.
        EEPROM.read(52) != 255 || EEPROM.read(54) != 255 ||
        EEPROM.read(56) != 255 || EEPROM.read(58) != 255 ||
        EEPROM.read(60) != 255 || EEPROM.read(62) != 255 ||
        EEPROM.read(64) != 255 || EEPROM.read(68) != 255) {

        // Read from eeprom highbyte, then lowbyte, and combine into words
        short int storedPosition[3];
        for (int i = 0; i < 3; i++) {
            int k = 2 * i;
            // 100 - 101 and 110 - 111
            storedPosition[i] = EepromStore::ReadUint16Be(k + 40);
#ifdef ROTATION
            rotationCalibrationData[i] = EepromStore::ReadUint16Be(k + 46);
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


        // Read deadband, the same 2-10 cm the calibration wizard allows
        if (EEPROM.read(58) >= kLowestDeadband && EEPROM.read(58) <= kHighestDeadband) {
            deadband = EEPROM.read(58);
        }
        else {
            deadband = kDefaultDeadband;
        }


        // Read maximum correction per share, in mm (#163). It used to be the
        // plough's total in cm, accepted only under 10, so anything the
        // operator set from 10 cm up became 50 cm at every boot.
        if (EEPROM.read(60) >= kLowestMaxCorrectionPerShareMm &&
            EEPROM.read(60) <= kHighestMaxCorrectionPerShareMm) {
            maxCorrectionPerShare = EEPROM.read(60);
        }
        else {
            maxCorrectionPerShare = kDefaultMaxCorrectionPerShareMm;
        }

        //Read swap 1 or 0
        if (EEPROM.read(62) < 2) {
            // Read swap
            swap = EEPROM.read(62);
        }
        else {
            swap = false;  //default to 0
        }

        // Read number of shares, the same 2-10 the calibration wizard allows
        if (EEPROM.read(64) >= kLowestShares && EEPROM.read(64) <= kHighestShares) {
            shares = EEPROM.read(64);
        }
        else {
            shares = kDefaultShares;
        }

        // Read bypass valve fitted, 1 or 0
        if (EEPROM.read(68) < 2) {
            bypass = EEPROM.read(68);
        }
        else {
            bypass = false;
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

    serialDebug->println("Deadband (cm)");
    serialDebug->println(deadband);
    serialDebug->println("--------------------------");

    serialDebug->println("error ploughside");
    serialDebug->println(swap);
    serialDebug->println("--------------------------");

    serialDebug->println("Bypass valve");
    serialDebug->println(bypass);
    serialDebug->println("--------------------------");

    serialDebug->println("Maximum correction per share (mm)");
    serialDebug->println(maxCorrectionPerShare);
    serialDebug->println("Maximum correction (mm)");
    serialDebug->println(GetMaxCorrection());
    serialDebug->println("--------------------------");
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementPlough::writeCalibrationData() {
    // Write each byte separately to the memory first the data then the points
    for (int i = 0; i < 3; i++) {
        int k = 2 * i;
        EepromStore::WriteUint16Be(k + 40, positionCalibrationData[i]); // 40, 42, 44
#ifdef ROTATION
        EepromStore::WriteUint16Be(k + 46, rotationCalibrationData[i]); // 46, 48, 50
#endif
    }

    // 52, 54, 56 (PWM manual/auto, KP) are no longer written
    EEPROM.write(58, deadband); //58, cm
    EEPROM.write(60, maxCorrectionPerShare);   //60, mm per share (#163)
    EEPROM.write(62, swap);     //62
    EEPROM.write(64, shares);   //64
    EEPROM.write(68, bypass);   //68, bypass valve fitted

#ifdef DEBUG
    serialDebug->println("Calibration data written");
#endif
}

}  // namespace triton
