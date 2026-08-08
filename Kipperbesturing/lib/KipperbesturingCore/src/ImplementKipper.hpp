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
// A proportional-integral-derivative steering-axle controller: `angle` (a
// hitch/drawbar angle sensor) plays the same role Pootmachinebesturing's
// XTE sensor plays -- the error signal a P/I/D loop is driven to zero on --
// producing a `setpoint` that `steer` (the actual steering-axle angle
// feedback) is driven toward via Adjust()'s shutoff-latch bang-bang.
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "VehicleTractor.hpp"

#include "ConfigImplementKipper.hpp"

namespace triton
{

class ImplementKipper {
private:
    //-------------
    // data members
    //-------------

    // Default angle calibration set
    int angleCalibrationData[3];
    int angleCalibrationPoints[3];
    int angle;

    // Default steer calibration set
    int steerCalibrationData[3];
    int steerCalibrationPoints[3];
    int steer;
    int lastSteer;

    float speed;

    // Update timer
    unsigned long updateAge;

    // Variables concerning adjust loop
    byte mode;
    int  setpoint;
    int  offset;

    int angleHist[50];
    int angleSum;  // Running sum of angleHist
    int angleAvg;  // Average of sum

    int dangle;  // Delta angle

    byte histCount;  // Counter of sum
    byte histTime;   // Integration time (seconds * 5)

    // PID variables
    float P;
    byte  kp;

    float I;
    byte  ki;

    float D;
    byte  kd;

    // Timers for end shutoff
    unsigned int  shutoffTime;
    bool          shutoffWide;
    bool          shutoffNarrow;
    unsigned long shutoffTimer;

    // Objects
    VehicleTractor* tractor;

    //-------------------------------------------------------------
    // private member functions implemented in ImplementKipper.cpp
    //-------------------------------------------------------------
    int getActualAngle();
    int getActualSteer();

    void setSetpoint();

    bool readCalibrationData();
    void writeCalibrationData();
    void wipeCalibrationData();

public:
    // -----------------------------------------------------------
    // public member functions implemented in ImplementKipper.cpp
    // -----------------------------------------------------------

    // Constructor
    ImplementKipper(VehicleTractor* tractor);

    void Update(byte mode);
    void Adjust(int direction);
    void PrintCalibrationData();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementKipper.hpp
    // ----------------------------------------------------------------
    inline bool ResetCalibration() {
        return readCalibrationData();
    }

    inline void CommitCalibration() {
        wipeCalibrationData();
        writeCalibrationData();
    }

    // -------
    // Getters
    // -------
    inline int GetSteer() {
        return steer;
    }

    inline int GetAngle() {
        return angle;
    }

    inline int GetSetpoint() {
        return setpoint;
    }

    inline int GetSteerCalibrationPoint(int i) {
        return steerCalibrationPoints[i];
    }

    inline int GetAngleCalibrationPoint(int i) {
        return angleCalibrationPoints[i];
    }

    inline byte GetKP() {
        return kp;
    }

    inline byte GetKI() {
        return ki;
    }

    inline byte GetKD() {
        return kd;
    }

    inline int GetOffset() {
        return offset;
    }

    // -------
    // Setters
    // -------
    inline void SetSteerCalibrationData(int i) {
        steerCalibrationData[i] = analogRead(STEER_SENS_PIN_4);
    }

    inline void SetAngleCalibrationData(int i) {
        angleCalibrationData[i] = analogRead(ANGLE_SENS_PIN_4);
    }

    inline void SetKP(byte value) {
        kp = value;
    }

    inline void SetKI(byte value) {
        ki = value;
    }

    inline void SetKD(byte value) {
        kd = value;
    }

    inline void SetOffset(int value) {
        offset = value;
    }
};

}  // namespace triton
