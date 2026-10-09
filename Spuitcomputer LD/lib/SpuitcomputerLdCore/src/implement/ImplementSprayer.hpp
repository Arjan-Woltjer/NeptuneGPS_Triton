/*
  ImplementSprayer - a library for the MeijWorks loofdoes implement
  Copyright (C) 2011-2026 J.A. Woltjer.

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

#include "../config/ConfigSprayer.hpp"
#include "../InterfaceSprayer.hpp"
#include "GuidanceSource.hpp"

namespace triton
{

#define SPRAYER_VERSION 0.3

#define NUM_OUTPUTS 4

#define OUT1 27
#define OUT2 26
#define OUT3 25
#define OUT4 23

#define MAX_PWM_CAL_POINTS  10
#define NUM_DOSE_CAL_POINTS 3
#define NUM_PWM_STEPS       5
#define PWM_MAX_DUTY        4095
#define PWM_FREQ_HZ         1000
#define SPEED_AVG_SAMPLES   5

struct OutputState {
    uint8_t       pin;
    uint8_t       ledcChannel;
    bool          state;
    bool          pwm;
    unsigned int  value;
    unsigned long timer;
};

struct PwmCalibrationPoint {
    int flowMlMin;  // measured flow in ml/min (= ml collected in 1-minute run)
    int pwm;        // PWM value 0-4095
};

struct DoseCalibrationPoint {
    int dose;        // dose in l/ha
    int analogValue; // raw ADC value 0-4095
};

// Who is driving the outputs directly instead of the button logic: the
// serial wizard (CalibrationSprayer) or the companion app (RemoteSprayer).
// Only one at a time; the other gets refused until it is given back.
enum class CalibrationOwner : uint8_t { None = 0, Serial = 1, Remote = 2 };

class CalibrationSprayer;  // forward declarations for friend access
class RemoteSprayer;

class ImplementSprayer {
    friend class CalibrationSprayer;
    friend class RemoteSprayer;

private:
    Stream*           serialDebug;
    GuidanceSource*   guidance;
    InterfaceSprayer* interface;
    ConfigSprayer*    config;

    DigitalInputState* buttons[NUM_DIGITAL_IN];
    AnalogInputState*  inputAnalog[NUM_ANALOG_IN];

    float speed;
    float speedBuf[SPEED_AVG_SAMPLES];
    int   speedBufIdx;
    // The VTG timestamp last folded into speedBuf. Update() runs free-running
    // (no fixed timestep), so a receiver's fix arrives far less often than
    // this is called; without this guard the "5-sample average" was actually
    // 5 copies of whatever the last fix was, resampled hundreds of times.
    unsigned long lastVtgSeen;
    float width;

    void updateInputs();
    void updateSpeed();
    // Stale (older than the configured timeout, or never received) or of
    // a lower fix quality than the operator allows; either way, no dosing.
    bool guidanceStale() const;

    void calculateDoseLHA();
    void calculateDoseLM();
    void calculatePWMValues(byte outputIndex);

    void updateOutputs();
    void setOutputDuty(const OutputState& out, uint32_t duty);

    // Inverse of calculateDoseLM(): a pump flow back to l/ha at the current
    // speed and width, so a clamped duty can be expressed in the operator's
    // own unit.
    float flowToLHA(float flowMlMin) const;

    CalibrationOwner calibrationOwner = CalibrationOwner::None;
    int              calibrationDuty  = 0;   // last duty applied through SetCalibrationPWM(2, ...)

    // Timed pump run, ended by this class, never by whoever started it.
    bool          runActive     = false;
    unsigned long runStartedAt  = 0;
    unsigned long runDurationMs = 0;
    void serviceCalibrationRun();

    // Last call's timestamp for updateDeviation()'s time-in-state
    // accumulator (deviationAccumMs, declared with doseDeviation below).
    // Not reset alongside it: the elapsed-time computation clamps its own
    // delta to at most kDeviationHoldMs, so a stale value left over from
    // whatever this object was doing before self-corrects in one call.
    unsigned long lastDeviationUpdateAt = 0;
    void updateDeviation();

public:
    // Exposed for testing; use GetOutputs() in production code
    float       doseLHA = 0.0f;
    float       doseLM  = 0.0f;

    // The dose the pump can actually deliver, in l/ha, derived from the duty
    // that really reaches it: equal to doseLHA inside the calibrated curve,
    // 0 when the demand is below the lowest calibrated flow and the pump is
    // cut, lower than doseLHA when the duty saturates at PWM_MAX_DUTY.
    // kActualDoseUndefined when there is nothing to compare against: stale
    // guidance, standing still, or an unusable calibration table.
    static constexpr float kActualDoseUndefined = -1.0f;
    float actualLHA = kActualDoseUndefined;

    // True once actualLHA has been outside kDoseTolerance of doseLHA for
    // kDeviationHoldMs while the pump output is on; drives the OUT4 buzzer
    // and is reported to the companion app so both agree.
    // Below this the machine counts as standing still: a receiver with a fix
    // reports a few tenths of a km/h of creep while parked, which would
    // otherwise read as "too slow to dose" and sound the deviation alarm
    // (NeptuneGPS_Triton#74).
    //
    // Lowered from the shared 0.5 m/s GuidanceSource::MinSpeed() default
    // (#61 field test asked for 0.3) to widen the speed band the pump can
    // actually dose in with a low-flow calibration. Not taken all the way to
    // 0.3: the same field test's parked stretch logged GPS creep up to
    // 0.28 m/s on this receiver, which would leave only 0.02 m/s of margin
    // and risk reopening #74. 0.35 keeps clearance over that measurement;
    // revisit if a quieter receiver or averaging change lowers the creep.
    static constexpr float         kStandstillSpeedMs = 0.35f;
    static constexpr float         kDoseTolerance   = 0.05f;
    static constexpr unsigned long kDeviationHoldMs = 1000;
    bool doseDeviation = false;

    // No output may switch on within this of any other output switching on
    // (NeptuneGPS_Triton#208): one load at a time, so their inrush currents
    // never add up. Switch-off needs no spacing. This is the same second
    // the pump has always waited after the vernevelaar; since the mixer
    // left that chain it applies between the mixer and the chain as well.
    static constexpr unsigned long kSwitchOnSpacingMs = 1000;

    // millis() of the most recent output switch-on, whichever output it
    // was. Starts one spacing in the past so the first switch-on after
    // boot is not held back (unsigned wrap-around is intended).
    unsigned long lastSwitchOnAt = 0UL - kSwitchOnSpacingMs;

    // Priming (NeptuneGPS_Triton#210): the aux switch (button 3) asks for
    // the vernevelaar and the pump at full duty, standing still or not, so
    // the lines can be filled before a run. Recomputed every Update(); false
    // while calibration owns the outputs.
    bool priming = false;

    // updateDeviation()'s time-in-state accumulator, clamped to
    // [0, kDeviationHoldMs]: counts up while the raw (un-held) condition
    // holds and down while it doesn't, and doseDeviation follows once it
    // saturates in either direction. A single last-flip timestamp (the
    // previous design) got reset to zero by one noisy sample right at the
    // tolerance boundary, so a deviation that held on balance but chattered
    // under kDeviationHoldMs could delay the alarm indefinitely -- exactly
    // the fluctuation the hold exists to absorb. deviationPending tracks
    // the condition's last observed value purely to detect the instant it
    // changes; that tick contributes nothing to either direction (the
    // preceding interval was spent in the old state), which keeps a single
    // clean transition timed identically to the previous design.
    // Exposed for testing: resetting doseDeviation alone does not reset
    // these, and a test fixture shared across scenarios needs to.
    unsigned long deviationAccumMs = 0;
    bool          deviationPending = false;

    // Set while the serial wizard (CalibrationSprayer, a friend) drives the
    // outputs directly; also exposed so tests can cover that hand-over.
    bool calibrationMode = false;

    // Also exposed for testing. CalibrationSprayer is already a friend and
    // writes these directly, so this widens who can reach them rather than
    // breaking an invariant -- and the fail-closed paths that guard duplicate
    // and short calibration tables are only reachable by setting them.
    PwmCalibrationPoint pwmCalibrationPoints[MAX_PWM_CAL_POINTS] = {
        { 0, 0 },
        { 2000, 2048 },
        { 4000, 4095 }
    };
    uint8_t numPwmCalibrationPoints = 3;

    DoseCalibrationPoint doseCalibrationPoints[NUM_DOSE_CAL_POINTS] = {
        { 50, 0 },
        { 100, 2048 },
        { 200, 4095 }
    };

    OutputState outputs[NUM_OUTPUTS] = {
        { OUT1, 0, false, false, 0, 0 },
        { OUT2, 1, false, false, 0, 0 },
        { OUT3, 2, false, true,  0, 0 },  // pump — driven by calibrated dose PWM, not a plain relay
        { OUT4, 3, false, false, 0, 0 },
    };

    ImplementSprayer(Stream* serialDebug, GuidanceSource* guidance, InterfaceSprayer* interface,
                     ConfigSprayer* config);

    void Update();
    void LoadCalibration();
    void SaveCalibration();
    void SetCalibrationPWM(byte outputIndex, int pwmValue);
    int  GetCalibrationDuty() const { return calibrationDuty; }

    // Calibration ownership. Acquire succeeds when free or already held by
    // the same owner; Release is a no-op for anyone else. Releasing also
    // ends a pump run and lets the button logic take the outputs back on
    // the next Update() -- that is the whole "disconnect" behaviour.
    bool             AcquireCalibration(CalibrationOwner who);
    void             ReleaseCalibration(CalibrationOwner who);
    CalibrationOwner GetCalibrationOwner() const { return calibrationOwner; }

    // Firmware-timed pump run for the PWM calibration: the pump goes to
    // `duty` now and back to 0 after `durationMs`, capped at
    // kCalibrationRunMaxMs, whatever the caller does in the meantime.
    // Refused unless calibration is held.
    static constexpr unsigned long kCalibrationRunMaxMs = 60000;
    bool          StartCalibrationRun(int duty, unsigned long durationMs);
    void          StopCalibrationRun();
    bool          CalibrationRunActive() const { return runActive; }
    unsigned long CalibrationRunRemainingMs() const;

    inline OutputState* GetOutputs() { return outputs; }
};

}  // namespace triton