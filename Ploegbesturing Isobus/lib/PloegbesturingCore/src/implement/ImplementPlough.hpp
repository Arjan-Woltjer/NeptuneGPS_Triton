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
#include "TritonIo.hpp"

// The ESP32 rate board's narrow and wide outputs are the two inputs of one
// TB67H451 bridge (F1 / MOT9). For a proportional valve's signal this
// matters: IN1 = IN2 = low leaves the output floating (a meter read 9 V for
// 50 % on 12 V), so the signal holds IN1 high and puts the inverted duty on
// IN2 -- high/low drives F1 high, high/high brakes it low.
#if defined(ESP32RC) && !defined(OUTPUT_VALVE_BRIDGE_2)
#define OUTPUT_VALVE_BRIDGE_2
#endif

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

// The end shutoff sees the plough as moving once its width has changed by at
// least this much (cm) since the shutoff timer last restarted. The filtered
// width flips by 1 cm on a few counts of sensor noise; a real move covers
// this in well under the shutoff time.
static constexpr short int kShutoffMoveCm = 2;

// The plough's hydraulics (EEPROM 68, which held the old bypass yes/no, so
// 0 and 1 keep their meaning):
//   Normal  -- narrower / wider solenoids only.
//   Bypass  -- plus the bypass output, on while either solenoid is.
//   Danfoss -- a proportional valve with an analog signal input: the signal
//              on the narrow output (F1), Center (DanfossParam) at rest,
//              less narrower, more wider; the wide output stays low. The bypass output (F2) is the PVE's
//              supply, on all the time: it wants power, then the signal.
enum class HydraulicType : byte {
    Normal  = 0,
    Bypass  = 1,
    Danfoss = 2,
};
static constexpr byte kHighestHydraulicType = byte(HydraulicType::Danfoss);

// The hydraulics tuning, set on the VT's HYDRAULIEK page and stored at
// once, one byte each at EEPROM kEepromDanfossTuning + the parameter (30-37:
// past the vehicle sensors' 20-28, before the plough's block at 40).
// Danfoss, in % of the supply on F1:
//   Center    -- the signal at rest (neutral), 30-70.
//   Manual    -- the deflection from Center the buttons drive at, 1-45.
//   AutoMax   -- the most deflection auto drives at, 1-45.
//   AutoMin   -- the deflection just past the deadband in auto, 0-45:
//                the least that still moves the spool.
// Both:
//   FullError -- the error past the deadband (cm) where auto reaches
//                AutoMax, 1-50; larger is gentler.
// Normal and bypass, the solenoids' PWM duty in % (100 = on, as before;
// switched boards without PWM, the Teensy's 20/21, stay on / off):
//   DutyManual, DutyAutoMax -- 1-100; DutyAutoMin -- 0-100.
enum class DanfossParam : uint8_t {
    Center,
    Manual,
    AutoMax,
    AutoMin,
    FullError,
    DutyManual,
    DutyAutoMax,
    DutyAutoMin,
    Count
};
static constexpr uint8_t kDanfossParamCount = uint8_t(DanfossParam::Count);
static constexpr int     kEepromDanfossTuning = 30;
struct DanfossParamLine {
    byte lowest;
    byte highest;
    byte byDefault;
};
// Manual 15 and auto 15 / 5 over 10 cm are what the bench liked on
// 2026-11-17 (manual 35 % / 65 %).
static constexpr DanfossParamLine kDanfossParams[kDanfossParamCount] = {
    { 30, 70, 50 },   // Center
    {  1, 45, 15 },   // Manual
    {  1, 45, 15 },   // AutoMax
    {  0, 45,  5 },   // AutoMin
    {  1, 50, 10 },   // FullError
    {  1, 100, 100 }, // DutyManual: full, as the solenoids always were
    {  1, 100, 100 }, // DutyAutoMax
    {  0, 100, 100 }, // DutyAutoMin: full from the deadband on, the bang-bang control
};
static_assert(kEepromDanfossTuning + kDanfossParamCount <= 40, "the Danfoss tuning runs into the plough's block");

// The VT hydraulics page's rows after the type: KOZEP, KEZI, AUTO MAX, AUTO
// MIN, TELJES. KEZI and the AUTO rows edit the Danfoss deflection or the
// solenoids' duty, by the type (IsobusVtCalibration::HydParamFor()).
constexpr uint8_t kHydRows = 5;

// A % of the supply as a duty of 255, rounded.
static constexpr byte DanfossPctToPwm(int pct) {
    return byte(pct <= 0 ? 0 : pct >= 100 ? 255 : (pct * 255 + 50) / 100);
}
// F2 (the valve's power, through a relay) comes on this long after the signal
// was set to neutral at boot or on a change of hydraulics.
static constexpr unsigned long kDanfossPowerDelayMs = 500;

class ImplementPlough {
private:
    //-------------
    // data members
    //-------------

    // Default offset calibration set
    short int positionCalibrationData[3];
    short int positionCalibrationPoints[3];
    short int position;
    // Width where the end-shutoff timer last restarted
    short int shutoffPosition;

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

    // A width from WidthPlanning (fan-out / straightening), in cm. While
    // set, it is the setpoint instead of offset plus the XTE correction.
    boolean   plannedWidthSet;
    short int plannedWidth;

    // LIMIT, cm (SetWidthLimits()); 0 = none set.
    short int widthLowestCm  = 0;
    short int widthHighestCm = 0;

    // Variables for adjusting ploughside and amount of shares
    boolean swap;
    byte    shares;

    // Normal, bypass valve or Danfoss proportional valve (HydraulicType)
    HydraulicType hydraulics;
    // Danfoss: when the signal was set to neutral, before F2 may power the valve
    unsigned long danfossNeutralSinceMs;
    // Danfoss: the tuning (DanfossParam), the deflection the next Narrower /
    // Wider drives at (% of the supply; Adjust sets it, else Manual), and the
    // signal on F1 now, for the VT.
    byte danfoss[kDanfossParamCount];
    byte danfossNextPct;
    byte danfossSignalPwm;

    // XTE
    short int xte;   // initialised in the constructor; see note there

    // End-of-travel latch (MeijWorks Libs/EndShutoff)
    EndShutoff endShutoff;

    // Objects
    Stream*     serialDebug;
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
    void driveDanfoss(byte signal);
    void readDanfossTuning();

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
    // `limited` false: the width procedure, which must reach its points
    // whatever LIMIT says. Otherwise the buttons stop at the width limits.
    void Adjust(byte mode, short int direction, bool limited = true);
    void PrintCalibrationData();

    // ----------------------------------------------------------------
    // public inline member functions implemented in ImplementPlough.hpp
    // ----------------------------------------------------------------
    // Decline: everything back as stored, the width included, since a share
    // change may have reset it (#238).
    inline boolean ResetCalibration() {
        const boolean found = readCalibrationData();
        readOffset();
        return found;
    }

    inline void CommitCalibration() {
        writeCalibrationData();
    }

    // -------
    // Getters
    // -------
    inline bool GetSide() {
        return triton::ReadDigital(PLOUGHSIDE_PIN_2) ^ swap;
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

    // The width sensor reading stored for calibration point i (the serial
    // console's export, #241).
    inline short int GetPositionCalibrationData(short int i) {
        return positionCalibrationData[i];
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

    inline HydraulicType GetHydraulics() {
        return hydraulics;
    }

    // The stored swap, not the side it gives: GetSide() is the plough-side
    // input XOR this.
    inline bool GetSwap() {
        return swap;
    }

    // Width sensor readings the boot load keeps: within the 10-bit ADC, and
    // adjacent points distinct, or the interpolation divides by zero.
    static boolean IsValidPositionCalibration(const short int adc[3]) {
        for (int i = 0; i < 3; i++) {
            if (adc[i] < 0 || adc[i] > kAdcMaxCount) return false;
        }
        return adc[0] != adc[1] && adc[1] != adc[2];
    }

    // -------
    // Setters
    // -------
    inline void SetPositionCalibrationData(short int i) {
        positionCalibrationData[i] = analogRead(POSITION_SENS_PIN_2);
    }

    // The width sensor now, for the width procedure to capture.
    inline short int ReadPositionSensor() {
        return analogRead(POSITION_SENS_PIN_2);
    }

    // A reading taken earlier, for a draft applied on Save (PloughSettings).
    inline void SetPositionCalibrationValue(short int i, short int adc) {
        positionCalibrationData[i] = adc;
    }

#ifdef ROTATION
    inline void SetRotationCalibrationData(short int i) {
        rotationCalibrationData[i] = analogRead(ROTATION_SENS_PIN_2);
    }
#endif

    // The width's range depends on the number of shares, so a width the new
    // range doesn't hold goes to the middle, as readOffset() does at boot
    // (#238). The wizard's store writes it with the shares (writeCalibrationData);
    // without that, 4 -> 10 -> 4 shares came back after a restart with the old
    // width, valid again on 4 shares (bench 2026-10-20).
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

    // Stops the plough and switches the outputs over. Out of range: Normal.
    void SetHydraulics(HydraulicType value);
    // The same, stored at once (EEPROM 68): the VT's HIDR. page.
    void SetHydraulicsStored(HydraulicType value);

    // The Danfoss tuning (DanfossParam). Set refuses a value outside the
    // parameter's bounds (false) and stores the rest at once.
    static bool IsValidDanfoss(DanfossParam param, int value) {
        const DanfossParamLine& line = kDanfossParams[uint8_t(param)];
        return value >= line.lowest && value <= line.highest;
    }
    inline byte GetDanfoss(DanfossParam param) const {
        return danfoss[uint8_t(param)];
    }
    bool SetDanfoss(DanfossParam param, int value);

    // Auto's deflection (% of the supply) for an error `beyondCm` past the
    // deadband: AutoMin just past it, AutoMax from FullError on.
    byte DanfossAutoPct(int beyondCm) const;
    // The same for the solenoids' duty: DutyAutoMin to DutyAutoMax.
    byte DutyAutoPct(int beyondCm) const;

    // The duty on F1 now (of 255), while the hydraulics are Danfoss.
    inline byte GetDanfossSignalPwm() const {
        return danfossSignalPwm;
    }

    // WidthPlanning, every loop() pass: the planned width in cm, or none
    // (set = false) for the XTE control.
    inline void SetPlannedWidth(boolean set, short int widthCm) {
        plannedWidthSet = set;
        plannedWidth    = widthCm;
    }
    inline boolean HasPlannedWidth() {
        return plannedWidthSet;
    }

    // LIMIT (the planner's Wmin..Wmax, the plough's physical range), in cm,
    // every loop() pass from WidthPlanning: the setpoint stays inside it in
    // every mode, and the buttons do not drive past it. 0 / 0: none set, the
    // 20..60 cm per share the offset keeps to.
    inline void SetWidthLimits(short int lowestCm, short int highestCm) {
        widthLowestCm  = lowestCm;
        widthHighestCm = highestCm;
    }
    inline short int GetWidthLowest() const {
        return widthLowestCm > 0 ? widthLowestCm : short(shares * 20);
    }
    inline short int GetWidthHighest() const {
        return widthHighestCm > 0 ? widthHighestCm : short(shares * 60);
    }
};

}  // namespace triton
