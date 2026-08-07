#pragma once
// Minimal VehicleTractor stub for native/MSVC unit tests. InterfaceRooier::
// Update() calls Update() and GetHitch() on it -- CalibrateSpeed()/
// ResetWheelspeedPulses()/ResetCalibration()/CommitCalibration() aren't used
// by this module at all (Rooierbesturing has no speed-calibration wizard
// step), so this stub doesn't need them.
#include <stdint.h>

class Stream;

class VehicleTractor {
  public:
    VehicleTractor() : hitch(false) {}

    void Update(uint8_t) {}

    bool GetHitch() { return hitch; }

    bool hitch;
};
