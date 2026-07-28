#pragma once
// Minimal VehicleGps stub for native/MSVC unit tests.
// Only exposes the interface used by ImplementSprayer.
class Stream;
class HardwareSerial;

class VehicleGps {
public:
    float speed = 0.0f;
    VehicleGps() {}
    float GetSpeedMs() { return speed; }
};
