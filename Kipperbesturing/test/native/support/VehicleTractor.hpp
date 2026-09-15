#pragma once
// Minimal VehicleTractor stub for native/MSVC unit tests. InterfaceKipper::
// Update() calls Update()/MinSpeed() on it, and ImplementKipper's D-term
// uses GetSpeedKmh() -- CalibrateSpeed()/ResetWheelspeedPulses()/
// ResetCalibration()/CommitCalibration() aren't used by this module at all
// (Kipperbesturing has no speed-calibration wizard step), so this stub
// doesn't need them.
#include <stdint.h>

class Stream;

class VehicleTractor {
  public:
    VehicleTractor() : minSpeedFlag(true), speedKmh(0.0f) {}

    void Update(uint8_t) {}

    bool  MinSpeed() { return minSpeedFlag; }
    float GetSpeedKmh() { return speedKmh; }

    bool  minSpeedFlag;
    float speedKmh;
    // The calibration wizard (CalibrationKipper, in the native build since
    // NeptuneGPS_Triton#92) commits the tractor block; nothing here is asserted
    // on, the wizard is never run.
    bool ResetCalibration() { return false; }
    void CommitCalibration() {}
};
