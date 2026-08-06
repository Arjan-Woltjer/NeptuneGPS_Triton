#pragma once
// Minimal VehicleTractor stub for native/MSVC unit tests. Only exposes the
// two members ImplementPlanter::Update()/InterfacePlanter::Update() call --
// GetInversion()/SetInversion() (used by the hitch-signal calibration step)
// live in CalibrationPlanter, which is #ifdef ARDUINO-guarded and excluded
// from the native build, so this stub doesn't need them.
#include <stdint.h>

class Stream;

class VehicleTractor {
  public:
    float speed = 0;

    VehicleTractor() {}

    void  Update(uint8_t) {}
    float GetSpeedMs() { return speed; }
};
