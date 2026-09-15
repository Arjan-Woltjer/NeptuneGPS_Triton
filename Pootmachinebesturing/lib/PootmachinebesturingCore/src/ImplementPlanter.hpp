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
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "GuidanceSource.hpp"
#include "VehicleTractor.hpp"

#include "ConfigImplementPlanter.hpp"

namespace triton
{

// The position/XTE sensors are read with analogRead(), a 10-bit conversion,
// so a stored calibration point outside this range cannot have come from them.
static constexpr int kAdcMaxCount = 1023;

// Factory calibration set shared by both position and XTE (legacy constructor
// defaults). Named because it's needed in two places: the constructor's
// no-calibration-data path, and readCalibrationData()'s rejection path when
// what was stored cannot be used.
static constexpr int kDefaultCalibrationData[3] = { 201, 428, 687 };

class ImplementPlanter {
private:
    //-------------
    // data members
    //-------------

    // Default offset calibration set
    int positionCalibrationData[3];
    int positionCalibrationPoints[3];
    int position;
    int lastPosition;

    // Default xte calibration set
    int xteCalibrationData[3];
    int xteCalibrationPoints[3];

    int xte;
    int speed = 0;

    // Update timer
    unsigned long updateAge;
    bool          updateFlag;

    // Variables concerning adjust loop.
    // `mode` is never assigned by any setter here -- InterfacePlanter has its
    // own, separate mode member, and this one only ever feeds the D-reset
    // branch in setSetpoint(). Left in place (matching the legacy shape,
    // which never wired up a setter for it either) but seeded to 0 instead
    // of read uninitialized, same treatment as the button timers below.
    byte mode;
    bool gpsEnabled;
    bool sensorEnabled;
    bool pwmEnabled;
    bool onOffValve;
    bool invertHydraulics;
    bool invertPlantingelementSensor;
    int  setpoint;
    int  offset;
    byte manPwm;
    byte autoPwm;

    int xteHist[50];
    int xteSum;  // Running sum of xteHist
    int xteAvg;  // Average of sum

    int dxte;  // DXTE

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
    Stream*         serialDebug;
    VehicleTractor* tractor;
    GuidanceSource* guidance;

    //-------------------------------------------------------------
    // private member functions implemented in ImplementPlanter.cpp
    //-------------------------------------------------------------
    int getActualXte();
    int getActualPosition();

    void setSetpoint();

    void readOffset();
    bool readCalibrationData();
    void writeCalibrationData();

public:
    // -----------------------------------------------------------
    // public member functions implemented in ImplementPlanter.cpp
    // -----------------------------------------------------------

    // Constructor
    ImplementPlanter(Stream* serialDebug, VehicleTractor* tractor, GuidanceSource* guidance);

    void Update();
    void Adjust(byte mode, int direction);
    void Left(short int pwm);
    void Right(short int pwm);
    void Stop();
    void PrintCalibrationData();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementPlanter.hpp
    // ----------------------------------------------------------------
    inline bool ResetCalibration() {
        return readCalibrationData();
    }

    inline void CommitCalibration() {
        writeCalibrationData();
    }

    // -------
    // Getters
    // -------
    inline bool GetPlantingelement() {
        return digitalRead(PLANTINGELEMENT_PIN_3) ^ invertPlantingelementSensor;
    }

    inline int GetPosition() {
        return position;
    }

    inline int GetXte() {
        return xte;
    }

    inline int GetSetpoint() {
        return setpoint;
    }

    inline int GetPositionCalibrationPoint(int i) {
        return positionCalibrationPoints[i];
    }

    inline int GetXteCalibrationPoint(int i) {
        return xteCalibrationPoints[i];
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

    inline byte GetPwmMan() {
        return manPwm;
    }

    inline byte GetPwmAuto() {
        return autoPwm;
    }

    inline bool GetGpsEnabled() {
        return gpsEnabled;
    }

    inline bool GetSensorEnabled() {
        return sensorEnabled;
    }

    inline bool GetPwmEnabled() {
        return pwmEnabled;
    }

    inline bool GetOnOffValve() {
        return onOffValve;
    }

    inline bool GetInvertHydraulics() {
        return invertHydraulics;
    }

    inline bool GetInvertPlantingelementSensor() {
        return invertPlantingelementSensor;
    }

    inline int GetOffset() {
        return offset;
    }

    // -------
    // Setters
    // -------
    inline void SetPositionCalibrationData(int i) {
        positionCalibrationData[i] = analogRead(POSITION_SENS_PIN_3);
    }

    inline void SetXteCalibrationData(int i) {
        xteCalibrationData[i] = analogRead(XTE_SENS_PIN_3);
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

    inline void SetPwmMan(byte value) {
        manPwm = value;
    }

    inline void SetPwmAuto(byte value) {
        autoPwm = value;
    }

    inline void SetGpsEnabled(bool value) {
        gpsEnabled = value;
    }

    inline void SetSensorEnabled(bool value) {
        sensorEnabled = value;
    }

    inline void SetPwmEnabled(bool value) {
        pwmEnabled = value;
    }

    inline void SetOnOffValve(bool value) {
        onOffValve = value;
    }

    inline void SetInvertHydraulics(bool value) {
        invertHydraulics = value;
    }

    inline void SetInvertPlantingelementSensor(bool value) {
        invertPlantingelementSensor = value;
    }

    inline void SetOffset(int value) {
        offset = value;
    }
};

}  // namespace triton
