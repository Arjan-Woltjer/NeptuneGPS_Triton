/*
  IsobusGuidanceSource - shared guidance data model for the plough ISOBUS controller
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

namespace triton
{

#define GPS_MS_PER_KNOT 0.51444444f
#define MINSPEED        0.5f

// Replaces VehicleGps for this project: a plain data model fed exclusively by
// IsobusGuidanceChannel's PGN callbacks (both the standard NMEA2000 messages
// and the legacy JD/Trimble/CNH proprietary ones), read by ImplementPlough/
// InterfacePlough/CalibrationPlough. No NMEA-serial parsing, no CAN/AgIsoStack
// dependency here -- that split (data model vs. hardware/protocol adapter)
// mirrors Salacia's Source/Channel convention.
class IsobusGuidanceSource {
public:
    inline explicit IsobusGuidanceSource(Stream* serialDebug) : serialDebug(serialDebug) {
        readCalibrationData();
    }

    // ------------------------------------------------------------
    // Setters -- called only from IsobusGuidanceChannel's PGN callbacks
    // ------------------------------------------------------------
    inline void NoteGgaFixReceived()            { lastGgaFix = millis(); }
    inline void SetSpeedKnots(float knots)      { speed = knots; lastVtgFix = millis(); }
    inline void SetXte(int hundredthsM, byte q) { xte = hundredthsM; quality = q; lastXteFix = millis(); }

    // ------------------------------------------------------------
    // Getters -- exact mirror of what ImplementPlough/InterfacePlough/
    // CalibrationPlough called on VehicleGps before this port
    // ------------------------------------------------------------
    inline int           GetXte()        { return xte; }
    inline unsigned long GetXteFixAge()  { return lastXteFix; }
    inline unsigned long GetGgaFixAge()  { return lastGgaFix; }
    inline unsigned long GetVtgFixAge()  { return lastVtgFix; }
    inline bool          IsRtkQuality()  { return quality == rtkQuality; }
    inline boolean       MinSpeed()      { return GetSpeedMs() >= MINSPEED; }
    inline float         GetSpeedMs()    { return GPS_MS_PER_KNOT * speed; }

    inline void SetRtkQuality(byte q) { rtkQuality = (q == 4 || q == 2) ? q : 4; }
    inline byte GetRtkQuality()       { return rtkQuality; }

    inline void CommitCalibration() { writeCalibrationData(); }

    void PrintCalibrationData();

private:
    // Only the rtkQuality byte survives from VehicleGps's calibration data --
    // the baudrate byte it also stored has no meaning left in a CAN-only build.
    inline bool readCalibrationData() {
        if (EEPROM.read(11) != 255) {
            rtkQuality = EEPROM.read(11);
            if (rtkQuality != 4 && rtkQuality != 2) rtkQuality = 4;
            return true;
        }
        return false;
    }

    inline void writeCalibrationData() { EEPROM.write(11, rtkQuality); }

    Stream* serialDebug;

    int           xte = 0;
    byte          quality = 0;
    float         speed = 0.0f;
    byte          rtkQuality = 4;
    unsigned long lastGgaFix = 0;
    unsigned long lastVtgFix = 0;
    unsigned long lastXteFix = 0;
};

inline void IsobusGuidanceSource::PrintCalibrationData() {
    serialDebug->println("===============================");
    serialDebug->println("Guidance source using following data:");
    serialDebug->println("===============================");
    serialDebug->println("RTK Quality");
    serialDebug->println(rtkQuality);
    serialDebug->println("-------------------------------");
}

}  // namespace triton
