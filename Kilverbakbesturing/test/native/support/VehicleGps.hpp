#pragma once
// Minimal VehicleGps stub for native/MSVC unit tests. Only exposes the
// subset of the real triton::VehicleGps API that ImplementScraper/
// InterfaceScraper actually call, each backed by a directly-settable public
// field for test control (matching Ploegbesturing's/Pootmachinebesturing's
// fake VehicleGps precedent). Unlike those two, this module reads real
// lat/long/altitude (for the two-reference-point leveling calculation), so
// this stub also provides GetPosition()/GetAltitudeCm() and the static
// DistanceBetween() helper, computed the same way the real library computes
// it (flat local-tangent-plane approximation), rather than a fixed value --
// ImplementScraper's own tests need real relative distances between
// deliberately-chosen lat/long pairs.
#include <math.h>
#include <stdint.h>

class Stream;
class HardwareSerial;

class VehicleGps {
  public:
    unsigned long ggaFixAge = 0;
    unsigned long vtgFixAge = 0;
    bool          minSpeedFlag = true;
    float         latitude = 0;
    float         longitude = 0;
    int           altitudeCm = 0;

    VehicleGps() {}

    void Update() {}

    unsigned long GetGgaFixAge()  { return ggaFixAge; }
    unsigned long GetVtgFixAge()  { return vtgFixAge; }
    bool          MinSpeed()      { return minSpeedFlag; }
    int           GetAltitudeCm() { return altitudeCm; }

    void GetPosition(float* outLatitude, float* outLongitude) {
        if (outLatitude) *outLatitude = latitude;
        if (outLongitude) *outLongitude = longitude;
    }

    // Same flat-earth degrees-to-meters approximation the real
    // triton::VehicleGps::DistanceBetween() uses, close enough at field
    // scale for deterministic test assertions.
    static float DistanceBetween(float* lat1, float* lon1, float* lat2, float* lon2) {
        constexpr float kMetersPerDegreeLat = 111320.0f;
        float latMeters = (*lat2 - *lat1) * kMetersPerDegreeLat;
        float lonMeters = (*lon2 - *lon1) * kMetersPerDegreeLat * cosf(*lat1 * 0.0174533f);
        return sqrtf(latMeters * latMeters + lonMeters * lonMeters);
    }
};
