#pragma once
// Minimal VehicleTractor stub for native/MSVC unit tests. Only exposes the
// subset of the real triton::VehicleTractor API that ImplementSprayer/
// InterfaceSprayer actually call (CalibrationSprayer's larger surface --
// EnableSim/DisableSim/SetVconst/CalibrateSpeed/etc. -- isn't needed here:
// it's #ifdef ARDUINO-guarded and excluded from the native build, same as
// CalibrationPlough/CalibrationPlanter).
#include <stdint.h>

class Stream;

class VehicleTractor {
  public:
    float         speed = 0;
    bool          hitch = false;
    bool          simSpeedFlag = false;
    unsigned long simTime = 0;
    bool          sim = false;
    unsigned long distance = 0;

    VehicleTractor() {}

    void  Update(uint8_t) {}
    float GetSpeedMs() { return speed; }
    // Matches the real triton::VehicleTractor::GetSpeedKmh()'s *36 factor
    // (not *3.6 -- speed is stored pre-scaled, see VehicleTractor.cpp).
    float GetSpeedKmh() { return speed * 36; }
    bool  GetHitch() { return hitch; }
    bool  SimSpeed() { return simSpeedFlag; }
    unsigned long GetSimTime() { return simTime; }
    bool  GetSim() { return sim; }
    unsigned long GetDistance() { return distance; }
    // The calibration wizard (CalibrationSprayer, in the native build since
    // NeptuneGPS_Triton#94) drives the tractor speed calibration, simulation
    // and speed-constant settings; nothing here is asserted on, the wizard is
    // never run.
    float         simSpeedKmh = 0;
    unsigned int  vconst = 0;
    unsigned int  CalibrateSpeed(int) { return 0; }
    bool          ResetCalibration() { return false; }
    void          CommitCalibration() {}
    void          ResetWheelspeedPulses() {}
    void          EnableSim() { sim = true; }
    void          DisableSim() { sim = false; }
    float         GetSimSpeedKmh() { return simSpeedKmh; }
    void          SetSimSpeedKmh(float value) { simSpeedKmh = value; }
    void          SetSimTime(unsigned long value) { simTime = value; }
    unsigned int  GetVconst() { return vconst; }
    void          SetVconst(unsigned int value) { vconst = value; }
};
