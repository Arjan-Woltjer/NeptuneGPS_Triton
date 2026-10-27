#pragma once
// Minimal VehicleSensors stub for native/MSVC unit tests. Only exposes the
// two members InterfacePlough::Update() calls.
#include <stdint.h>
#include <EEPROM.h>

class Stream;

class VehicleSensors {
  public:
    bool hitch = false;

    VehicleSensors() {}

    void Update(uint8_t) {}
    bool GetHitch() { return hitch; }
    // The calibration wizard (CalibrationPlough, in the native build since
    // NeptuneGPS_Triton#89) toggles the speed-pulse inversion and commits the
    // tractor block; nothing here is asserted on, the wizard is never run.
    bool inversion = false;
    bool GetInversion() { return inversion; }
    void SetInversion(bool value) { inversion = value; }
    bool ResetCalibration() { return false; }
    // Writes the one byte of the real tractor block the plough's settings
    // table edits: the hitch inversion at 28, as VehicleTractor.cpp does.
    void CommitCalibration() { EEPROM.write(28, inversion); }
};
