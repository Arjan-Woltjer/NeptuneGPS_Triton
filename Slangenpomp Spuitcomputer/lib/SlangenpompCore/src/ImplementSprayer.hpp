/*
  ImplementSprayer - a library for a slangenpomp (hose pump) sprayer
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

#include "VehicleTractor.hpp"

#include "ConfigImplementSprayer.hpp"

namespace triton
{

// The legacy GPS-dosing code path (an `ImplementSprayer(VehicleGps*)`
// constructor whose entire body was `// TODO`) never worked -- confirmed
// unimplemented, not just unused. Dropped rather than carried forward.
// Backlog: wire up real GPS-speed dosing later, mirroring Loofdoes
// Spuitcomputer's ImplementSprayer (which takes a VehicleGps* directly, no
// tractor wheel sensor at all).

// pwmCalibrationData is a flow-pulse count captured over a fixed 5-second
// window per calibratePump() below, not an ADC reading -- word()-decoded from
// EEPROM, so always in [0, 65535]. calibratePump() itself only ever writes a
// strictly increasing sequence (each point is clamped to be greater than the
// previous one), so that's the invariant readCalibrationData() re-validates.
static constexpr int kPwmCalibrationDataMax = 65535;

class ImplementSprayer {
private:
    //-------------
    // data members
    //-------------

    // Calibration sets
    byte pwmCalibrationPoints[12];
    int  pwmCalibrationData[12];  // EEPROM 100

    int flowCalibration;  // EEPROM 140
    int ccPer100Omw;      // EEPROM 150

    byte    teeth;     // EEPROM 160
    byte    pumps;     // EEPROM 161
    byte    width;     // EEPROM 162
    byte    pumpsOn;
    boolean hold;

    // Gear counters
    boolean       gearPuls;
    int           gearPulses;
    unsigned long rounds;

    // Flow counters
    boolean       flowPuls;
    int           flowPulses;
    unsigned long volume;

    // Dose
    int dose;
    int doseHist;
    int ccPer100m;

    // Flows in cc per second
    int neededFlow;
    int calculatedFlow;  // written by calculateAlarm() only -- see the note
                          // on that method below; kept for behavior parity.
    int actualFlow;

    // Pulses per second -- unused today; kept for behavior parity with the
    // legacy member layout (never assigned in the legacy source either).
    float calculatedPps;

    // Integral and differential values
    int           delta;
    int           deltaHist[10];
    long          deltaSum;
    long          deltaAvg;
    int           deltaDelta;
    byte          histCount;
    byte          histSize;

    int setpointFlow;

    // Update timer
    unsigned long updateAge;
    unsigned long updateAgeFlag;
    boolean       updateFlag;

    // Variables concerning adjust loop
    byte    setpointPwm;
    boolean alarm;

    // PID
    int P;
    int I;
    int D;

    byte kp;  // EEPROM 170
    byte ki;  // EEPROM 171
    byte kd;  // EEPROM 172

    // Objects
    VehicleTractor* tractor;

    //-------------------------------------------------------------
    // private member functions implemented in ImplementSprayer.cpp
    //-------------------------------------------------------------
    void calculateSetpointFlow(byte mode);
    void calculateAlarm(byte mode);
    void calculateSetpointPwm();

    void setDose(int correction);
    void readDose();

    bool readCalibrationData();
    void printCalibrationData();
    void writeCalibrationData();

public:
    // -----------------------------------------------------------
    // public member functions implemented in ImplementSprayer.cpp
    // -----------------------------------------------------------

    // Constructor
    ImplementSprayer(VehicleTractor* tractor);

    void Update(byte mode, int buttons);
    void Stop();
    void CalibratePump();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementSprayer.hpp
    // ----------------------------------------------------------------
    inline void ResetFlowPulses() {
        flowPulses = 0;
    }

    inline void ResetGearPulses() {
        gearPulses = 0;
    }

    inline bool ResetCalibration() {
        return readCalibrationData();
    }

    inline void CommitCalibration() {
        writeCalibrationData();
        printCalibrationData();
    }

    // -------
    // Getters
    // -------
    inline byte GetTeeth() {
        return teeth;
    }

    inline byte GetPumps() {
        return pumps;
    }

    inline byte GetPumpsOn() {
        return pumpsOn;
    }

    inline byte GetWidth() {
        return width;
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

    inline int GetFlowCalibration() {
        return flowCalibration;
    }

    inline bool GetSwitch() {
        return digitalRead(IMPLEMENT_SWITCH);
    }

    inline int GetDose() {
        return dose;
    }

    inline int GetActualDose() {
        if (tractor->GetSpeedMs() > 0.1f) {
            return (neededFlow - (deltaSum / 5)) / (width * tractor->GetSpeedMs());
        }
        return 0;
    }

    inline int GetI() {
        return I;
    }

    inline int GetP() {
        return P;
    }

    inline int GetDelta() {
        return delta;
    }

    inline float GetActualFlow() {
        return actualFlow;
    }

    inline float GetNeededFlow() {
        return neededFlow;
    }

    inline int GetSetpoint() {
        return setpointPwm;
    }

    inline int GetSetpointF() {
        return setpointFlow;
    }

    inline int GetFlowPulses() {
        return flowPulses;
    }

    inline int GetGearPulses() {
        return gearPulses;
    }

    inline bool GetAlarm() {
        return alarm;
    }

    inline bool GetFlag() {
        return updateFlag;
    }

    inline unsigned long GetVolume() {
        // centiliters (liters is / 2; ml or cc is * 500)
        return (volume * 50) / flowCalibration;
    }

    // -------
    // Setters
    // -------
    inline void SetTeeth(byte value) {
        teeth = value;
    }

    inline void SetPumps(byte value) {
        pumps = value;
    }

    inline void SetWidth(byte value) {
        width = value;
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

    inline void SetFlowCalibration(int pulsesPerLiter) {
        flowCalibration = pulsesPerLiter;
    }
};

}  // namespace triton
