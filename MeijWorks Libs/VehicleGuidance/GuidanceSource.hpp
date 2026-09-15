/*
  GuidanceSource - shared guidance data model (position, speed, course, XTE)
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

namespace triton
{

// Guarded: VehicleGps defines the same two names, and a project halfway
// through migrating from it may include both headers in one translation unit.
#ifndef GPS_MS_PER_KNOT
#define GPS_MS_PER_KNOT 0.51444444f
#endif
#ifndef MINSPEED
#define MINSPEED        0.5f
#endif

// The successor to VehicleGps's decoded state, on its own: a plain,
// transport-agnostic data model with no serial port, no CAN bus and no
// storage behind it. Whatever acquires guidance data feeds it through the
// setters -- SerialGuidanceChannel's GpsParser family for a receiver on a
// UART, an ISOBUS/CAN channel's PGN callbacks, or a test -- and the
// implement, interface and calibration classes of a project only ever read
// the getters. That split (data model vs. hardware/protocol adapter) mirrors
// Salacia's Source/Channel convention.
//
// Persistence is deliberately not in here. rtkQuality is the one value an
// operator calibrates; the project that owns the calibration menu decides
// where it is stored (EEPROM on the Teensy projects, NVS on the ESP32 ones,
// see NeptuneGPS_Triton#78) and hands it back through SetRtkQuality() at
// boot. A shared library that hard-codes a storage address would force every
// consumer onto one layout.
class GuidanceSource {
public:
    GuidanceSource() = default;

    // ------------------------------------------------------------
    // Setters -- called by whichever channel feeds this instance
    // ------------------------------------------------------------
    inline void NoteGgaFixReceived()            { lastGgaFix = millis(); }
    inline void SetPosition(float lat, float lon) { latitude = lat; longitude = lon; lastGgaFix = millis(); }
    inline void SetAltitude(float alt)          { altitude = alt; }
    inline void SetSpeedKnots(float knots)      { speed = knots; lastVtgFix = millis(); }
    inline void SetCourseDeg(float degrees)     { course = degrees; lastVtgFix = millis(); }
    inline void SetTime(float t)                { time = t; }
    inline void SetDate(unsigned long d)        { date = d; }
    inline void SetQuality(byte q)              { quality = q; }
    inline void SetXte(int hundredthsM)         { xte = hundredthsM; lastXteFix = millis(); }
    inline void SetXte(int hundredthsM, byte q) { xte = hundredthsM; quality = q; lastXteFix = millis(); }

    // ------------------------------------------------------------
    // Getters
    // ------------------------------------------------------------
    inline int           GetXte()          { return xte; }
    // Named Timestamp, not "FixAge" -- these return the millis() value the
    // sentence was received at, not an elapsed age. Callers compute the age
    // themselves (millis() - GetXteTimestamp()); 0 means never received.
    // XTE renamed 2026-08-09, GGA and VTG followed 2026-09-15 when the
    // library became shared; the old names invited a future bug (see
    // Triton_TC_Client_Design.md P5).
    inline unsigned long GetXteTimestamp() { return lastXteFix; }
    inline unsigned long GetGgaTimestamp() { return lastGgaFix; }
    inline unsigned long GetVtgTimestamp() { return lastVtgFix; }
    inline bool          IsRtkQuality()  { return quality == rtkQuality; }
    inline bool          MinSpeed()      { return GetSpeedMs() >= MINSPEED; }
    inline float         GetSpeedMs()    { return GPS_MS_PER_KNOT * speed; }

    inline float  GetLatitude()  { return latitude; }
    inline float  GetLongitude() { return longitude; }
    inline float  GetAltitude()  { return altitude; }
    inline float  GetCourse()    { return course; }
    inline float  GetSpeed()     { return speed; }
    inline byte   GetQuality()   { return quality; }

    inline void GetDatetime(unsigned long* outdate, unsigned long* outtime) {
        if (outdate) *outdate = date;
        if (outtime) *outtime = (unsigned long)time;
    }

    // The GGA fix quality IsRtkQuality() compares against: 4 (RTK fixed) or
    // 2 (DGPS); anything else falls back to 4, as VehicleGps always did.
    inline void SetRtkQuality(byte q) { rtkQuality = (q == 4 || q == 2) ? q : 4; }
    inline byte GetRtkQuality()       { return rtkQuality; }

private:
    float         latitude = 0.0f;
    float         longitude = 0.0f;
    float         altitude = 0.0f;
    float         course = 0.0f;
    float         time = 0.0f;
    unsigned long date = 0;
    int           xte = 0;
    byte          quality = 0;
    float         speed = 0.0f;
    byte          rtkQuality = 4;
    unsigned long lastGgaFix = 0;
    unsigned long lastVtgFix = 0;
    unsigned long lastXteFix = 0;
};

}  // namespace triton
