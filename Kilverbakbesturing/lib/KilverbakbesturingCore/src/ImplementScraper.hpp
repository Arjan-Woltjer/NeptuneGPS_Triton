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
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "VehicleGps.hpp"

#include "ConfigImplementScraper.hpp"

namespace triton
{

// The position sensor (when NOSENS is undefined -- not the case on any board
// built today) is read with analogRead(), a 10-bit conversion.
static constexpr int kAdcMaxCount = 1023;

class ImplementScraper {
private:
    //-------------
    // data members
    //-------------

    // Default offset calibration set
    int positionCalibrationData[3];
    int positionCalibrationPoints[3];
    int position;
    int lastPosition;

    // Update timer
    unsigned long updateAge;
    unsigned long lastGgaFix;

    // Variables concerning adjust loop
    int  setpoint;
    byte error;
    int  maxCorrection;

    // offset/slope/height* are all persisted through readInt()/writeInt()'s
    // 2-byte EEPROM slots (see the .cpp) -- short int here, not int, so the
    // in-RAM type actually matches what's stored. The legacy source used
    // plain `int` (4 bytes on this ARM target) with a `union { byte b[2]; int
    // i; }` that only ever wrote the low 2 bytes, leaving the high 2 bytes as
    // uninitialized stack garbage on every read -- a real bug, not just a
    // style choice.
    short int offset;
    short int slope;
    byte      manPwm;
    byte      autoPwm;

    // Lat, long and height
    float     latitude;
    float     longitude;
    short int height;

    // Reference A
    float     latRa;
    float     longRa;
    short int heightRa;

    // Reference B
    float     latRb;
    float     longRb;
    short int heightRb;

    // Length of vector AB. (Vector AB's own lat/long components are computed
    // as local variables inside calculateDistances() rather than stored here
    // -- the legacy source declared same-named members that were never
    // actually assigned, since a local variable of the same name shadowed
    // them in the one method that computed them, and nothing else ever read
    // the members. Not carried forward as dead state.)
    float lAb;

    // Reference point on the line through A and B, for XTE
    short int heightRef;

    // Distance along AB from A, and shortest distance to that line
    float dAb;
    float xAb;

    // PID variables
    byte kp;

    // Timers for end shutoff
    unsigned int  shutoffTime;
    bool          shutoffWide;
    bool          shutoffNarrow;
    unsigned long shutoffTimer;

    // Objects
    VehicleGps* gps;

    //------------------------------------------------------------
    // private member functions implemented in ImplementScraper.cpp
    //------------------------------------------------------------
    int getActualPosition();

    void setSetpoint();
    void setOffset(int correction);

    void readOffset();
    void readRefA();
    void readRefB();
    void readRef(float* lat, float* lon, short int* height, byte addr);
    void setRef(float* lat, float* lon, short int* height, byte addr);

    void calculateDistances();

    void writeFloat(float x, byte addr);
    float readFloat(byte addr);
    void writeInt(short int x, byte addr);
    short int readInt(byte addr);

    bool readCalibrationData();
    void writeCalibrationData();
    void wipeCalibrationData();

public:
    // ----------------------------------------------------------
    // public member functions implemented in ImplementScraper.cpp
    // ----------------------------------------------------------

    // Constructor
    ImplementScraper(VehicleGps* gps);

    void Update(byte mode, int buttons);
    void Stop();
    void Adjust(byte mode, int direction);
    void SetRefA();
    void SetRefB();
    void PrintCalibrationData();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementScraper.hpp
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
    inline short int GetHeight() {
        return height;
    }

    inline short int GetRefHeight() {
        return heightRef;
    }

    inline short int GetOffset() {
        return offset;
    }

    inline int GetDistance() {
        return int(dAb);
    }

    inline int GetXTE() {
        return int(xAb);
    }

    inline short int GetSlope() {
        return slope;
    }

    inline int GetPositionCalibrationPoint(int i) {
        return positionCalibrationPoints[i];
    }

    inline int GetPosition() {
        return position;
    }

#ifdef PID_KP
    inline byte GetKP() {
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

    inline int GetMaxCorrection() {
        return maxCorrection;
    }

    // -------
    // Setters
    // -------
#ifndef NOSENS
    inline void SetPositionCalibrationData(int i) {
        positionCalibrationData[i] = analogRead(POSITION_SENS_PIN_5);
    }
#endif

#ifdef PID_KP
    inline void SetKP(byte value) {
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

    inline void SetError(byte value) {
        error = value;
    }

    inline void SetMaxCorrection(int value) {
        maxCorrection = value;
    }

    inline void SetSlope(short int value) {
        slope = value;
    }
};

}  // namespace triton
