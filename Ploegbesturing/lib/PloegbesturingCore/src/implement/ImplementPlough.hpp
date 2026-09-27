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
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "GuidanceSource.hpp"

#include "../config/ConfigImplementPlough.hpp"

namespace triton
{

// The position sensor is read with analogRead(), a 10-bit conversion, so a
// stored calibration point outside this range cannot have come from it.
static constexpr short int kAdcMaxCount = 1023;

// Factory position curve. Named because it is needed in two places: the
// constructor's no-calibration-data path, and the rejection path in
// readCalibrationData() when what was stored cannot be used.
static constexpr short int kDefaultPositionCalibration[3] = { 600, 461, 308 };

class ImplementPlough {
private:
    //-------------
    // data members
    //-------------

    // Default offset calibration set
    short int positionCalibrationData[3];
    short int positionCalibrationPoints[3];
    short int position;
    short int lastPosition;

#ifdef ROTATION
    // Default rotation calibration set
    // engineered for lemken ploughs with rotation sensor
    short int rotationCalibrationData[3];
    short int rotationCalibrationPoints[3];
    short int rotation;
#endif

    // Update timer
    unsigned long updateAge;
    unsigned long lastXteFix;
    boolean       updateFlag;

    // Variables concerning adjust loop
    short int setpoint;
    byte      error;
    byte      maxCorrection;

    short int offset;
    byte      manPwm;
    byte      autoPwm;

    // Variables for adjusting ploughside and amount of shares
    boolean swap;
    byte    shares;

    // XTE
    short int xte;   // initialised in the constructor; see note there

    // PID variables
    byte kp;

    // Timers for end shutoff
    unsigned short int shutoffTime;
    bool                shutoffWide;
    bool                shutoffNarrow;
    unsigned long       shutoffTimer;

    // Objects
    Stream*         serialDebug;
    GuidanceSource* guidance;

    //------------------------------------------------------------
    // private member functions implemented in ImplementPlough.cpp
    //------------------------------------------------------------
#ifdef ROTATION
    short int getActualRotation();
#endif
    short int getActualPosition();

    void setSetpoint();
    void setOffset(short int correction);

    void readOffset();

    void writeInt(short int x, uint16_t addr);
    short int readInt(uint16_t addr);

    boolean readCalibrationData();
    void writeCalibrationData();

public:
    // ----------------------------------------------------------
    // public member functions implemented in ImplementPlough.cpp
    // ----------------------------------------------------------

    // Constructor
    ImplementPlough(Stream* serialDebug, GuidanceSource* guidance);

    void Update(byte mode, short int buttons);
    void Stop();
    void Narrower(byte pwm);
    void Wider(byte pwm);
    void Adjust(byte mode, short int direction);
    void PrintCalibrationData();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementPlough.hpp
    // ----------------------------------------------------------------
    inline boolean ResetCalibration() {
        return readCalibrationData();
    }

    inline void CommitCalibration() {
        writeCalibrationData();
    }

    // -------
    // Getters
    // -------
    inline bool GetSide() {
        return digitalRead(PLOUGHSIDE_PIN_2) ^ swap;
    }

    inline short int GetPosition() {
        return position;
    }

#ifdef ROTATION
    inline short int GetRotation() {
        return rotation;
    }
#endif

    inline short int GetSetpoint() {
        return setpoint;
    }

    inline short int GetOffset() {
        return offset;
    }

    inline short int GetPositionCalibrationPoint(short int i) {
        return positionCalibrationPoints[i];
    }

#ifdef ROTATION
    inline short int GetRotationCalibrationPoint(short int i) {
        return rotationCalibrationPoints[i];
    }
#endif

    inline byte GetShares() {
        return shares;
    }

    inline short int GetMaxCorrection() {
        return maxCorrection;
    }

#ifdef PID_KP
    inline short int GetKP() {
        return kp;
    }
#endif

#ifdef PWM_MAN
    inline byte GetPwmMan() {
        return manPwm;
    }
#endif

#ifdef PWM_AUTO
    inline byte GetPwmAuto() {
        return autoPwm;
    }
#endif

    inline byte GetError() {
        return error;
    }

    // -------
    // Setters
    // -------
    inline void SetPositionCalibrationData(short int i) {
        positionCalibrationData[i] = analogRead(POSITION_SENS_PIN_2);
    }

#ifdef ROTATION
    inline void SetRotationCalibrationData(short int i) {
        rotationCalibrationData[i] = analogRead(ROTATION_SENS_PIN_2);
    }
#endif

    inline void SetShares(byte value) {
        shares = value;
    }

#ifdef PID_KP
    inline void SetKP(short int value) {
        kp = value;
    }
#endif

#ifdef PWM_MAN
    inline void SetPwmMan(byte value) {
        manPwm = value;
    }
#endif

#ifdef PWM_AUTO
    inline void SetPwmAuto(byte value) {
        autoPwm = value;
    }
#endif

    inline void SetMaxCorrection(short int value) {
        maxCorrection = value;
    }

    inline void SetError(byte value) {
        error = value;
    }

    inline void SetSwap(boolean value) {
        swap = value;
    }
};

}  // namespace triton
