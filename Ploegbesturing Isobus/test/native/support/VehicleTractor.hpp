#pragma once
// Minimal VehicleTractor stub for native/MSVC unit tests. Only exposes the
// two members InterfacePlough::Update() calls.
#include <stdint.h>

class Stream;

class VehicleTractor {
  public:
    bool hitch = false;

    VehicleTractor() {}

    void Update(uint8_t) {}
    bool GetHitch() { return hitch; }
};
