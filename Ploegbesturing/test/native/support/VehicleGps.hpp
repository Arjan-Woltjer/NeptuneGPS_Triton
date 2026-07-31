#pragma once
// Minimal VehicleGps stub for native/MSVC unit tests. Only exposes the
// subset of the real triton::VehicleGps API that ImplementPlough/
// InterfacePlough actually call, each backed by a directly-settable public
// field for test control (matching Loofdoes' fake VehicleGps precedent).
#include <stdint.h>

class Stream;
class HardwareSerial;

class VehicleGps {
  public:
    int           xte = 0;
    unsigned long xteFixAge = 0;
    unsigned long ggaFixAge = 0;
    unsigned long vtgFixAge = 0;
    bool          rtkQuality = true;
    bool          minSpeed = true;

    VehicleGps() {}

    void Update() {}

    int           GetXte()        { return xte; }
    unsigned long GetXteFixAge()  { return xteFixAge; }
    unsigned long GetGgaFixAge()  { return ggaFixAge; }
    unsigned long GetVtgFixAge()  { return vtgFixAge; }
    bool          IsRtkQuality()  { return rtkQuality; }
    bool          MinSpeed()      { return minSpeed; }
};
