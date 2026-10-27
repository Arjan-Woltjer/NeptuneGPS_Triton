/*
  VehicleTractor - a library for a tractor
  Copyright (C) 2011-2026 J.A. Woltjer.
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "ConfigVehicleTractor.hpp"
#if defined(ESP32S3)
#include "TritonIo.hpp"   // Triton01: the hitch input sits behind the MCP23008
#endif

namespace triton
{
// Reads one of the tractor inputs. Triton01 goes through TritonIo (MCP23008
// for the hitch, opto polarity for both); every other board reads the GPIO
// directly and needs nothing beyond Arduino.h.
inline bool VehicleTractorReadInput(uint8_t pin) {
#if defined(ESP32S3)
    return ReadDigital(pin);
#else
    return digitalRead(pin) != 0;
#endif
}
}  // namespace triton

namespace triton
{

class VehicleTractor {
private:
    //-------------
    // data members
    //-------------

    Stream*       serialDebug;

    // speed and simspeed keep the scale of the pulse formula in Update()
    // (pulses * 40 / vConst), chosen for integer maths on the 8-bit MCUs:
    // one unit is 36 km/h, hence the * 36 and / 36 in the km/h accessors.
    // GetSpeedMs() and MINSPEED_1 use this same scale, not m/s.
    float         speed;
    float         simspeed;
    byte          simtime;
    bool          sim;
    bool          inversion;

    unsigned int  vConst;
    unsigned int  wheelspeedPulses;
    unsigned long distance;

    bool          wheelspeedPulse;
    unsigned long updateAge;

    bool readCalibrationData();
    void writeCalibrationData();

public:
    // Constructor
    // defaultInversion: the hitch inversion a board without stored tractor
    // data starts with. The stored setting (EEPROM 28, set in the calibration
    // wizard) always wins. Spuitcomputer SP passes true: its hitch signal used
    // to be inverted in code, hard-wired.
    explicit VehicleTractor(Stream* serialDebug, bool defaultInversion = false);

    void Update(byte mode);

    unsigned int CalibrateSpeed(int buttons);

    void PrintCalibrationData();

    inline bool ResetCalibration()        { return readCalibrationData(); }
    inline void CommitCalibration()       { writeCalibrationData(); }

    inline bool MinSpeed()                { return speed > MINSPEED_1; }

    inline void EnableSim()               { sim = true; }
    inline void DisableSim()              { sim = false; }

    inline void SetInversion(bool value)  { inversion = value; }
    inline void SetSimSpeedKmh(float kmh) { simspeed = kmh / 36; }
    inline void SetSimTime(byte t)        { simtime = t; }
    inline void SetVconst(unsigned int v) { vConst = v; }

    inline void ResetWheelspeedPulses()   { wheelspeedPulses = 0; }

    inline boolean GetHitch()             { return triton::VehicleTractorReadInput(HITCH_PIN_1) ^ inversion; }
    inline float   GetSpeedMs()           { return speed; }
    inline float   GetSpeedKmh()          { return speed * 36; }
    inline bool    GetSim()               { return sim; }
    inline bool    GetInversion()         { return inversion; }
    inline float   GetSimSpeedKmh()       { return simspeed * 36; }
    inline byte    GetSimTime()           { return simtime; }
    inline boolean SimSpeed()             { return speed >= simspeed; }
    inline unsigned long   GetDistance()  { return distance / vConst; }
    inline unsigned int    GetVconst()    { return vConst; }
};

}  // namespace triton