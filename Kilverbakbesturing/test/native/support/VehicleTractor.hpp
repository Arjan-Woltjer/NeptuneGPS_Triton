#pragma once
// Minimal VehicleTractor stub for native/MSVC unit tests. InterfaceScraper::
// Update() only calls Update() on it -- CalibrateSpeed()/ResetWheelspeedPulses()/
// ResetCalibration()/CommitCalibration() (used by CalibrationScraper's
// #ifdef SPEED_L-gated speed-calibration step and the final save step) live
// in CalibrationScraper, which is #ifdef ARDUINO-guarded and excluded from
// the native build, so this stub doesn't need them.
#include <stdint.h>

class Stream;

class VehicleTractor {
  public:
    VehicleTractor() {}

    void Update(uint8_t) {}
};
