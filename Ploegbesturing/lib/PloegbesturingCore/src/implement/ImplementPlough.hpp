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
#include "EndShutoff.hpp"

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

// Max correction is kept per share, in mm (#163): it sizes with the plough --
// a 4+1 plough taking its extra share on gets that share's worth more -- and
// it fits one EEPROM byte. 2.5 to 10 cm per share; the default is 5 cm, which
// is 20 cm for 4 shares.
static constexpr byte kLowestMaxCorrectionPerShareMm  = 25;
static constexpr byte kHighestMaxCorrectionPerShareMm = 100;
static constexpr byte kDefaultMaxCorrectionPerShareMm = 50;

// Number of shares and deadband (cm): the calibration wizard adjusts within
// these bounds and the boot load accepts exactly the same range.
static constexpr byte kLowestShares    = 2;
static constexpr byte kHighestShares   = 10;
static constexpr byte kDefaultShares   = 4;
static constexpr byte kLowestDeadband  = 2;
static constexpr byte kHighestDeadband = 10;
static constexpr byte kDefaultDeadband = 2;

// The valves are switched, not proportional: a bang-bang control at full
// duty, in manual and automatic alike.
static constexpr byte kValvePwm = 255;

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
    byte      deadband;                // cm
    byte      maxCorrectionPerShare;   // mm (#163)

    short int offset;

    // Variables for adjusting ploughside and amount of shares
    boolean swap;
    byte    shares;

    // Bypass valve fitted: driven together with the narrower/wider valves
    boolean bypass;

    // XTE
    short int xte;   // initialised in the constructor; see note there

    // End-of-travel latch (MeijWorks Libs/EndShutoff)
    EndShutoff endShutoff;

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
    void driveBypass(byte pwm);

    void readOffset();


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

    // Per share, in mm: what the calibration menu adjusts and EEPROM holds.
    inline byte GetMaxCorrectionPerShare() {
        return maxCorrectionPerShare;
    }

    // The plough's limit on the XTE correction: per share times the shares,
    // in mm (#163).
    inline short int GetMaxCorrection() {
        return maxCorrectionPerShare * shares;
    }

    inline byte GetDeadband() {
        return deadband;
    }

    inline bool GetBypass() {
        return bypass;
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

    // The width's range depends on the number of shares, so a width the new
    // range doesn't hold goes to the middle, as readOffset() does at boot
    // (#238). Not written: readOffset() gives the same value after a reboot.
    inline void SetShares(byte value) {
        shares = value;
        if (offset > shares * 60 || offset < shares * 20) {
            offset = shares * 40;
        }
    }

    inline void SetMaxCorrectionPerShare(byte value) {
        maxCorrectionPerShare = value;
    }

    inline void SetDeadband(byte value) {
        deadband = value;
    }

    inline void SetSwap(boolean value) {
        swap = value;
    }

    inline void SetBypass(boolean value) {
        bypass = value;
    }
};

}  // namespace triton
