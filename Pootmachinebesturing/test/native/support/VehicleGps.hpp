#pragma once
// Minimal VehicleGps stub for native/MSVC unit tests. Only exposes the
// subset of the real triton::VehicleGps API that ImplementPlanter/
// InterfacePlanter actually call, each backed by a directly-settable public
// field for test control (matching Ploegbesturing's fake VehicleGps
// precedent). Planter checks the raw NMEA `quality` byte directly
// (`GetQuality() != 4`) rather than the RTK-quality-threshold API
// (IsRtkQuality()/SetRtkQuality()) Ploegbesturing's InterfacePlough uses, so
// this stub exposes `quality`/GetQuality() instead of `rtkQuality`.
#include <stdint.h>

class Stream;
class HardwareSerial;

class VehicleGps {
  public:
    int           xte = 0;
    unsigned long xteFixAge = 0;
    unsigned long ggaFixAge = 0;
    unsigned long vtgFixAge = 0;
    // uint8_t rather than the `byte` typedef -- this header can be the first
    // one in InterfacePlanter.cpp's include chain to need it (InterfacePlanter.hpp
    // pulls VehicleGps.hpp in ahead of <Arduino.h>, via ImplementPlanter.hpp),
    // so `byte` isn't guaranteed defined yet at this point.
    uint8_t       quality = 4;
    bool          minSpeed = true;
    float         speed = 0;

    VehicleGps() {}

    void Update() {}

    int           GetXte()        { return xte; }
    unsigned long GetXteFixAge()  { return xteFixAge; }
    unsigned long GetGgaFixAge()  { return ggaFixAge; }
    unsigned long GetVtgFixAge()  { return vtgFixAge; }
    uint8_t       GetQuality()    { return quality; }
    bool          MinSpeed()      { return minSpeed; }
    float         GetSpeedMs()    { return speed; }
};
