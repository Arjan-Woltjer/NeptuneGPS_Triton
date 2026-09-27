/*
  ImplementPlanter - a library for the MeijWorks planter implement
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
#include "ImplementPlanter.hpp"

namespace triton
{

//------------
// Constructor
//------------
ImplementPlanter::ImplementPlanter(Stream* serialDebug, VehicleTractor* tractor, GuidanceSource* guidance) {
    // Connected classes
    this->serialDebug = serialDebug;
    this->guidance = guidance;
    this->tractor = tractor;

#ifdef DEBUG
    this->serialDebug->println(S_DIVIDE);
    this->serialDebug->println("Initialising planter");
    this->serialDebug->println(S_DIVIDE);
#endif

    // Pin configuration
    // Inputs
    pinMode(PLANTINGELEMENT_PIN_3, INPUT);
    digitalWrite(PLANTINGELEMENT_PIN_3, HIGH);

    // Outputs
    pinMode(OUTPUT_WIDE_3, OUTPUT);
    pinMode(OUTPUT_NARROW_3, OUTPUT);
    pinMode(OUTPUT_BYPASS_3, OUTPUT);
    pinMode(OUTPUT_LED_3, OUTPUT);

    // Analog IO
    pinMode(POSITION_SENS_PIN_3, INPUT);
    digitalWrite(POSITION_SENS_PIN_3, LOW);

    // Calibration points for position and xte
    positionCalibrationPoints[0] = -6;
    positionCalibrationPoints[1] = 0;
    positionCalibrationPoints[2] = 6;
    position = 0;
    lastPosition = 0;

    xteCalibrationPoints[0] = -10;
    xteCalibrationPoints[1] = 0;
    xteCalibrationPoints[2] = 10;
    xte = 0;

    // Setpoint of adjust loop
    setpoint = 0;

    // mode is never assigned by any setter on this class -- see the member
    // declaration comment in ImplementPlanter.hpp. Seeded to 0 rather than
    // left to read whatever the constructor's implicit zero-initialization
    // (or lack thereof) happens to leave behind.
    mode = 0;

    // PID integration and differentiation intervals
    for (int i = 0; i < 50; i++) {
        xteHist[i] = 0;
    }
    xteSum = 0;  // Running sum of xteHist
    xteAvg = 0;  // Average of sum

    dxte = 0;  // DXTE

    histCount = 0;   // Counter of sum
    histTime = 25;   // Integration time (seconds * 5) == 5 in this case

    // PID variables
    P = 0;
    I = 0;
    D = 0;

    // End shutoff timers
    shutoffTime = SHUTOFF_3;
    shutoffWide = false;
    shutoffNarrow = false;
    shutoffTimer = millis();

    // Update timer. updateFlag was never initialized here in the legacy
    // code either -- Update()'s "at least 250ms elapsed AND updateFlag"
    // gate would read garbage memory on the very first call. Seeded to
    // false, same treatment ImplementPlough gives its own updateFlag.
    updateAge = millis();
    updateFlag = false;

    // Get calibration data from EEPROM otherwise use defaults
    if (!readCalibrationData()) {
        // Default offset calibration set 100, 101
        // Default xte calibration set 110, 111
        for (int i = 0; i < 3; i++) {
            positionCalibrationData[i] = kDefaultCalibrationData[i];
            xteCalibrationData[i] = kDefaultCalibrationData[i];
        }

        // PID constants 120, 121, 130, 131, 140, 141
        kp = 50;
        ki = 50;
        kd = 1;

        // pwm values for manual (150) and auto (160)
        manPwm = 90;
        autoPwm = 70;

        // offset 180
        offset = 0;

        // Settings byte 94's six flags -- gpsEnabled/sensorEnabled/pwmEnabled/
        // onOffValve/invertHydraulics/invertPlantingelementSensor -- were only
        // ever set inside readCalibrationData()'s "found data" branch in the
        // legacy code, leaving them uninitialized on a first boot (erased
        // EEPROM). Same class of bug as the button timers in InterfacePlanter;
        // default to the safest state (everything off, XTE sensor over GPS).
        gpsEnabled = false;
        sensorEnabled = false;
        pwmEnabled = false;
        onOffValve = false;
        invertHydraulics = false;
        invertPlantingelementSensor = false;

#ifdef DEBUG
        this->serialDebug->println("No calibration data found, using defaults");
#endif
    }
#ifdef DEBUG
    this->serialDebug->println("Done...");
#endif
}

// ----------------------------------
// Method for updating implement data
// ----------------------------------
void ImplementPlanter::Update() {
    // When using GPS for XTE measurement
    if (gpsEnabled) {
        // update offset, xte, position and setpoint
        if (guidance->GetXteTimestamp() - updateAge > 0) {
            // Update xte, position and setpoint
            xte = guidance->GetXte();
            speed = guidance->GetSpeedMs();
            position = getActualPosition();

            setSetpoint();

            updateAge = guidance->GetXteTimestamp();
        }
    }
    // When GPS is not used as XTE sensor
    else {
        // update offset, xte and setpoint
        if (millis() - updateAge >= 250 && updateFlag) {
            // Update xte and setpoint
            xte = getActualXte();
            speed = tractor->GetSpeedMs();

            if (!sensorEnabled) {
                position = 0;
            }
            else {
                // reset ad converter to position input
                getActualPosition();
                // let input settle
                delay(25);
                // read position, reset ad converter to xte input
                position = getActualPosition();
                getActualXte();
            }

            setSetpoint();
            updateAge = millis();
            updateFlag = false;
        }
        else {
            updateFlag = true;
        }
    }
}

// --------------------------------------------
// Method for measuring actual implement offset
// --------------------------------------------
int ImplementPlanter::getActualPosition() {
    // Read analog input
    int readRaw = analogRead(POSITION_SENS_PIN_3);
    int actualPosition;
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

    // b is zero when two calibration points hold the same reading, which a
    // disconnected or seized position potentiometer produces directly: the
    // wizard latches analogRead() at each step without checking the captures
    // differ. The result would be inf or NaN, and narrowing either to int
    // below is undefined behaviour -- so this cannot be left to the caller.
    // Fall back to the segment's own start point, the nearest defensible value.
    if (b == 0.0f) {
        actualPosition = d;
    }
    else {
        actualPosition = (((a * c) / b) + d);
    }

    return actualPosition;
}

//------------------------------------------
// Method for measuring actual implement XTE
//------------------------------------------
int ImplementPlanter::getActualXte() {
    // Read analog input
    int readRaw = analogRead(XTE_SENS_PIN_3);
    int actualXte;
    int i = 0;

    // Loop through calibrationdata
    while (i < 2 && readRaw > xteCalibrationData[i]) {
        i++;
    }

    if (i == 0) {
        i++;
    }

    // Interpolate calibrationdata
    float a = readRaw - xteCalibrationData[i - 1];
    float b = xteCalibrationData[i] - xteCalibrationData[i - 1];
    float c = xteCalibrationPoints[i] - xteCalibrationPoints[i - 1];
    float d = xteCalibrationPoints[i - 1];

    // Same degenerate-calibration guard as getActualPosition() above.
    if (b == 0.0f) {
        actualXte = d;
    }
    else {
        actualXte = (((a * c) / b) + d);
    }

    return actualXte;
}

// ---------------------------
// Method for setting setpoint
// ---------------------------
void ImplementPlanter::setSetpoint() {
    int xteCor = xte + offset;

    int previousHistCount;
    int xteAvgLocal;
    int dxteLocal;
    int dxteCalc;
    int dFactor;

    // histCount/previousHistCount index a histTime(25)-long ring within the
    // 50-slot xteHist array. histCount is a byte, so on the very first call
    // (histCount==0) the legacy "previousHistCount = histCount - 1" wraps to
    // 255 -- both the wrong ring position (should be histTime-1=24) and past
    // the end of xteHist[50] entirely, an out-of-bounds read. Handle the
    // wrap explicitly instead of relying on unsigned underflow.
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

    // Set xte sum, delta and averages
    xteSum = xteSum - xteHist[histCount] + xteCor;
    xteAvgLocal = xteSum / histTime;
    dxteLocal = xteCor - xteHist[previousHistCount];
    dxteCalc = xteCor;  // sqrt(abs(xte))
    dFactor = dxteLocal - dxteCalc;

    // Update XTE history
    xteHist[histCount] = xteCor;

    P = float(xteCor) * kp / 100.0f;
    I = float(xteAvgLocal) * ki / 100.0f;
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

    histCount++;

    setpoint = getActualPosition() + P + I + D;

    // xteAvg/dxte are exposed via no getter today but kept in step with the
    // legacy shape (previously named xte_avg/dxte) for calibration/debug
    // parity if a getter is added later.
    xteAvg = xteAvgLocal;
    dxte = dxteLocal;
}

// ------------------------------
// Method for adjusting implement
// ------------------------------
void ImplementPlanter::Adjust(byte mode, int direction) {
    byte pwm;

    if (sensorEnabled) {
        unsigned int absSetpoint = abs(setpoint);
        unsigned int settime = 0;
        unsigned int inputtime = 0;

        if (mode == 0) {  // Automatic
            pwm = autoPwm;

            if (absSetpoint > 0) {
                settime = absSetpoint * 150 + 100;
            }
            else {
                settime = 0;
            }

            inputtime = millis() - updateAge;
        }
        else {  // Manual or Calibration
            pwm = manPwm;

            setpoint = direction;
            settime = 20000;
            inputtime = 0;
        }

        // Adjust tree including error
        //---------------------------
        // Setpoint < actual position
        //---------------------------
        if (setpoint > 0 && inputtime < settime) {
            Left(pwm);
        }
        //---------------------------
        // Setpoint > actual position
        //---------------------------
        else if (setpoint < 0 && inputtime < settime) {
            Right(pwm);
        }
        //-----------------
        // Setpoint reached
        //-----------------
        else {
            Stop();
        }
    }
    else {
        int actualPosition;

        if (mode == 0) {  // Automatic
            pwm = autoPwm;

            actualPosition = position;
        }
        else {  // Manual or Calibration
            pwm = manPwm;

            actualPosition = setpoint - direction;
        }

        //---------------------------
        // Setpoint < actual position
        //---------------------------
        if (actualPosition < setpoint && !shutoffNarrow) {
            Left(pwm);

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
        else if (actualPosition > setpoint && !shutoffWide) {
            Right(pwm);

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

            shutoffTimer = millis();
        }
        lastPosition = actualPosition;
    }
}

// ---------------------------------------
// Method for moving implement to the left
// ---------------------------------------
void ImplementPlanter::Left(short int pwm) {
    if (onOffValve && pwmEnabled) {
        digitalWrite(OUTPUT_WIDE_3, LOW ^ invertHydraulics);
        digitalWrite(OUTPUT_NARROW_3, HIGH ^ invertHydraulics);
        analogWrite(OUTPUT_BYPASS_3, pwm);
    }
    else if (onOffValve && !pwmEnabled) {
        digitalWrite(OUTPUT_WIDE_3, LOW ^ invertHydraulics);
        digitalWrite(OUTPUT_NARROW_3, HIGH ^ invertHydraulics);
        digitalWrite(OUTPUT_BYPASS_3, HIGH);
    }
    else {
        analogWrite(OUTPUT_WIDE_3, pwm * invertHydraulics);
        analogWrite(OUTPUT_NARROW_3, pwm * !invertHydraulics);
        digitalWrite(OUTPUT_BYPASS_3, HIGH);
    }
    digitalWrite(OUTPUT_LED_3, HIGH);
}

// ----------------------------------------
// Method for moving implement to the right
// ----------------------------------------
void ImplementPlanter::Right(short int pwm) {
    if (onOffValve && pwmEnabled) {
        digitalWrite(OUTPUT_WIDE_3, HIGH ^ invertHydraulics);
        digitalWrite(OUTPUT_NARROW_3, LOW ^ invertHydraulics);
        analogWrite(OUTPUT_BYPASS_3, pwm);
    }
    else if (onOffValve && !pwmEnabled) {
        digitalWrite(OUTPUT_WIDE_3, HIGH ^ invertHydraulics);
        digitalWrite(OUTPUT_NARROW_3, LOW ^ invertHydraulics);
        digitalWrite(OUTPUT_BYPASS_3, HIGH);
    }
    else {
        analogWrite(OUTPUT_WIDE_3, pwm * !invertHydraulics);
        analogWrite(OUTPUT_NARROW_3, pwm * invertHydraulics);
        digitalWrite(OUTPUT_BYPASS_3, HIGH);
    }
    digitalWrite(OUTPUT_LED_3, HIGH);
}

// ------------------------------
// Method for stopping implement
// ------------------------------
void ImplementPlanter::Stop() {
    analogWrite(OUTPUT_WIDE_3, 0);
    analogWrite(OUTPUT_NARROW_3, 0);
    analogWrite(OUTPUT_BYPASS_3, 0);

    digitalWrite(OUTPUT_LED_3, LOW);
}

// ----------------------------------------------
// Method for reading calibrationdata from EEPROM
// ----------------------------------------------
bool ImplementPlanter::readCalibrationData() {
    // Read amount of startups and add 1
    EEPROM.write(0, EEPROM.read(0) + 1);

    // Read offset and XTE calibration data
    if (EEPROM.read(70) != 255 || EEPROM.read(76) != 255 ||
        EEPROM.read(82) != 255 || EEPROM.read(84) != 255 ||
        EEPROM.read(86) != 255 || EEPROM.read(88) != 255 ||
        EEPROM.read(90) != 255 || EEPROM.read(92) != 255) {

        // Read from eeprom highbyte, then lowbyte, and combine into words
        int storedPosition[3];
        int storedXte[3];
        for (int i = 0; i < 3; i++) {
            int k = 2 * i;
            // 100 - 101 and 110 - 111
            storedPosition[i] = word(EEPROM.read(k + 70), EEPROM.read(k + 71));
            storedXte[i] = word(EEPROM.read(k + 76), EEPROM.read(k + 77));
        }

        // These feed the divisor in getActualPosition()/getActualXte(), and
        // were previously accepted as-is -- an arbitrary 16-bit value from
        // EEPROM, even though both sensors are 10-bit ADCs so anything above
        // 1023 is physically impossible. Adjacent points also have to differ,
        // or the interpolation divides by zero (see getActualPosition()'s
        // b==0 guard, which only protects against a degenerate *reading*, not
        // degenerate *stored calibration*).
        bool positionValid = true;
        bool xteValid = true;
        for (int i = 0; i < 3; i++) {
            if (storedPosition[i] < 0 || storedPosition[i] > kAdcMaxCount) {
                positionValid = false;
            }
            if (storedXte[i] < 0 || storedXte[i] > kAdcMaxCount) {
                xteValid = false;
            }
        }
        if (storedPosition[0] == storedPosition[1] || storedPosition[1] == storedPosition[2]) {
            positionValid = false;
        }
        if (storedXte[0] == storedXte[1] || storedXte[1] == storedXte[2]) {
            xteValid = false;
        }

        // The defaults are otherwise only applied when this function returns
        // false, so they have to be written explicitly here -- leaving the
        // members untouched would leave them uninitialised.
        for (int i = 0; i < 3; i++) {
            positionCalibrationData[i] = positionValid ? storedPosition[i] : kDefaultCalibrationData[i];
            xteCalibrationData[i] = xteValid ? storedXte[i] : kDefaultCalibrationData[i];
        }

        manPwm = EEPROM.read(82);
        autoPwm = EEPROM.read(84);

        kp = EEPROM.read(86);
        ki = EEPROM.read(88);
        kd = EEPROM.read(90);

        if (EEPROM.read(92) < 255 || EEPROM.read(93) < 255) {
            // Read offset (2 bytes). Signed, so it goes through readInt()
            // rather than word(), which is unsigned and turned every stored
            // negative offset into a large positive number that failed the
            // range check below and reset the offset to 0 on every boot
            // (NeptuneGPS_Triton#100). Same helper ImplementPlough uses.
            offset = readInt(92);
            if (offset > 20 || offset < -20) {
                offset = 0;
            }
        }
        else {
            offset = 0;
        }

#ifdef DEBUG
        serialDebug->println(EEPROM.read(94));
#endif

        gpsEnabled = (EEPROM.read(94) & 0B00100000) > 0;
        sensorEnabled = (EEPROM.read(94) & 0B00010000) > 0;
        pwmEnabled = (EEPROM.read(94) & 0B00001000) > 0;
        onOffValve = (EEPROM.read(94) & 0B00000100) > 0;
        invertHydraulics = (EEPROM.read(94) & 0B00000010) > 0;
        invertPlantingelementSensor = (EEPROM.read(94) & 0B00000001) > 0;
    }
    else {
        return false;
    }
    return true;
}

// --------------------------------------------
// Method for writing calibrationdata to EEPROM
// --------------------------------------------
void ImplementPlanter::writeCalibrationData() {
    // Write each byte separately to the memory first the data then the points
    for (int i = 0; i < 3; i++) {
        int k = 2 * i;
        EEPROM.write(k + 70, highByte(positionCalibrationData[i]));  // 70, 72, 74
        EEPROM.write(k + 71, lowByte(positionCalibrationData[i]));   // 71, 73, 75
        EEPROM.write(k + 76, highByte(xteCalibrationData[i]));       // 76, 78, 80
        EEPROM.write(k + 77, lowByte(xteCalibrationData[i]));        // 77, 79, 81
    }

    EEPROM.write(82, manPwm);   // 82
    EEPROM.write(84, autoPwm);  // 84

    EEPROM.write(86, kp);  // 86
    EEPROM.write(88, ki);  // 88
    EEPROM.write(90, kd);  // 90

    writeInt(offset, 92);  // 92 - 93, signed; see readCalibrationData()

    byte settings = 0;

    settings += 0B00100000 * gpsEnabled;
    settings += 0B00010000 * sensorEnabled;
    settings += 0B00001000 * pwmEnabled;
    settings += 0B00000100 * onOffValve;
    settings += 0B00000010 * invertHydraulics;
    settings += 0B00000001 * invertPlantingelementSensor;

#ifdef DEBUG
    serialDebug->println(settings);
#endif

    EEPROM.write(94, settings);

#ifdef DEBUG
    serialDebug->println("Calibration data written");
#endif
}

//---------------------------------------------------
//Method for printing calibration data to serial port
//---------------------------------------------------
void ImplementPlanter::PrintCalibrationData() {
    // Printing calibration data to serial port
    serialDebug->println("===============================");
    serialDebug->println("Planter using following data:");
    serialDebug->println("===============================");
    serialDebug->println("Offset calibration data");
    for (int i = 0; i < 3; i++) {
        serialDebug->print(positionCalibrationData[i]);
        serialDebug->print(", ");
        serialDebug->println(positionCalibrationPoints[i]);
    }
    serialDebug->println("-------------------------------");

    serialDebug->println("XTE calibration data");
    for (int i = 0; i < 3; i++) {
        serialDebug->print(xteCalibrationData[i]);
        serialDebug->print(", ");
        serialDebug->println(xteCalibrationPoints[i]);
    }
    serialDebug->println("-------------------------------");

    serialDebug->println("KP");
    serialDebug->println(kp);
    serialDebug->println("-------------------------------");

    serialDebug->println("KI");
    serialDebug->println(ki);
    serialDebug->println("-------------------------------");

    serialDebug->println("KD");
    serialDebug->println(kd);
    serialDebug->println("-------------------------------");

    serialDebug->println("PWM auto");
    serialDebug->println(autoPwm);
    serialDebug->println("-------------------------------");

    serialDebug->println("PWM manual");
    serialDebug->println(manPwm);
    serialDebug->println("-------------------------------");

    serialDebug->println("GPS Enabled");
    serialDebug->println(gpsEnabled);
    serialDebug->println("-------------------------------");

    serialDebug->println("XTE Sensor enabled");
    serialDebug->println(sensorEnabled);
    serialDebug->println("-------------------------------");

    serialDebug->println("PWM Enabled");
    serialDebug->println(pwmEnabled);
    serialDebug->println("-------------------------------");

    serialDebug->println("On/Off Valve");
    serialDebug->println(onOffValve);
    serialDebug->println("-------------------------------");

    serialDebug->println("Hydraulics Inverted");
    serialDebug->println(invertHydraulics);
    serialDebug->println("-------------------------------");

    serialDebug->println("Plantingelement Sensor Inverted");
    serialDebug->println(invertPlantingelementSensor);
    serialDebug->println("-------------------------------");
}

// ---------------------------------------
// Method for reading int data from EEPROM
// ---------------------------------------
// Signed 16-bit, byte for byte in host order. Every signed value this class
// persists goes through here and writeInt() below rather than through
// word()/highByte(), which are unsigned and silently lose the sign
// (NeptuneGPS_Triton#100). Copied from ImplementPlough, which has always
// stored its own offset this way.
short int ImplementPlanter::readInt(uint16_t addr) {
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
void ImplementPlanter::writeInt(short int x, uint16_t addr) {
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
