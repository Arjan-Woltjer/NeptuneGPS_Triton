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
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "ConfigImplementRooier.hpp"

namespace triton
{

// The height sensors are read with analogRead(), a 10-bit conversion.
static constexpr int kAdcMaxCount = 1023;

// This module's legacy source never showed a working implementation of the
// KP/KI/KD/manPwm/autoPwm getters/setters its header declared -- no fragment
// demonstrated how they should drive the L/R leveling loop. Reconstructed
// here as a real (if untuned-by-default) proportional-integral-derivative
// correction added on top of the operator's manually-set target height
// (`setpoint`), modeled on ImplementPlanter::setSetpoint()/Adjust()'s shape
// (the closest sibling with all three gain terms and a manual/auto PWM
// split). kp=ki=kd=0 by default -- the PID path is fully real and exercised,
// it's simply inert (target == setpoint + offset exactly) until a technician
// tunes it via CalibrationRooier's wizard, matching the "real but unused
// today" framing this feature was reconstructed under.
class ImplementRooier {
private:
    //-------------
    // data members
    //-------------

    // Height sensor calibration (independent per side)
    int positionCalibrationDataL[3];
    int positionCalibrationDataR[3];
    int positionCalibrationPoints[3];

    int heightL;
    int heightR;
    int lastHeightL;
    int lastHeightR;

    // Update timer
    unsigned long updateAge;

    // Operator-set target height (percentage, 1-99) and a calibration-wizard
    // trim on top of it -- see the .cpp's readCalibrationData()/
    // AdjustSetpoint() for why these are persisted differently.
    int setpoint;
    int offset;

    // L/R balance trim and bang-bang error margin, both calibration-wizard-set.
    int  skew;
    byte error;

    // PID gains and per-side PID state (integral/last-error), and the
    // manual/auto PWM drive strength the bang-bang below applies.
    byte kp;
    byte ki;
    byte kd;
    byte manPwm;
    byte autoPwm;

    float integralL;
    float integralR;
    float lastErrorL;
    float lastErrorR;

    // Per-side PID-adjusted targets, recomputed each Update() cycle and read
    // back by Adjust().
    int targetL;
    int targetR;

    // Timers for end shutoff (per side)
    unsigned int  shutoffTime;
    bool          shutoffWideL;
    bool          shutoffWideR;
    bool          shutoffNarrowL;
    bool          shutoffNarrowR;
    unsigned long shutoffTimerL;
    unsigned long shutoffTimerR;

    //-------------------------------------------------------------
    // private member functions implemented in ImplementRooier.cpp
    //-------------------------------------------------------------
    int getActualHeight(byte pin, int* calibrationData);

    void computeTargets();
    int  computeTarget(int actualHeight, float& integral, float& lastError);
    bool adjustSide(int actual, int target, byte pwm, byte outputUp, byte outputDown,
                     int& lastHeight, bool& shutoffWide, bool& shutoffNarrow, unsigned long& shutoffTimer);

    void adjustSetpoint(int correction);

    bool readCalibrationData();
    void writeCalibrationData();
    void wipeCalibrationData();

public:
    // -----------------------------------------------------------
    // public member functions implemented in ImplementRooier.cpp
    // -----------------------------------------------------------

    // Constructor
    ImplementRooier();

    void Update(byte mode, int buttons);
    void Adjust(byte mode, int direction);
    void Stop();
    void PrintCalibrationData();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementRooier.hpp
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
    inline int GetSetpoint() {
        return setpoint;
    }

    inline int GetHeightL() {
        return heightL + skew;
    }

    inline int GetHeightR() {
        return heightR - skew;
    }

    inline int GetPositionCalibrationPoint(int i) {
        return positionCalibrationPoints[i];
    }

    inline int GetSkew() {
        return skew;
    }

    inline byte GetError() {
        return error;
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

    inline int GetOffset() {
        return offset;
    }

    // -------
    // Setters
    // -------
    // The legacy source's SetPositionCalibrationData()/SetXteCalibrationData()
    // were unmodified copy-paste from ImplementPlanter.h (POSITION_SENS_PIN_3/
    // XTE_SENS_PIN_3 don't exist in this module at all). Replaced with real
    // per-side setters against this module's own height sensor pins.
    inline void SetPositionCalibrationDataL(int i) {
        positionCalibrationDataL[i] = analogRead(HEIGHT_SENS_PIN_L_8);
    }

    inline void SetPositionCalibrationDataR(int i) {
        positionCalibrationDataR[i] = analogRead(HEIGHT_SENS_PIN_R_8);
    }

    inline void SetSkew(int value) {
        skew = value;
    }

    inline void SetError(byte value) {
        error = value;
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

    inline void SetOffset(int value) {
        offset = value;
    }
};

}  // namespace triton
