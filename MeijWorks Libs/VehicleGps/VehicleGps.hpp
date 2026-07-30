/*
  VehicleGps - a small GPS library for Arduino providing basic NMEA parsing.
  Based on work by Maarten Lamers and Mikal Hart.
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

//#define DEBUG

// Conversion constants
#define GPS_MS_PER_KNOT      0.51444444
#define GPS_KMH_PER_KNOT     1.852
#define GPS_MILES_PER_METER  0.00062137112
#define GPS_KM_PER_METER     0.001

#define GPGGA_TERM    "GPGGA"
#define GNGGA_TERM    "GNGGA"
#define GPVTG_TERM    "GPVTG"
#define GNVTG_TERM    "GNVTG"
#define GPXTE_TERM    "GPXTE"
#define ROXTE_TERM    "ROXTE"
#define CAN_POS_TERM  "0CFEF31C"
#define CAN_POS_TERM2 "18FEF31C"
#define CAN_POS_TERM3 "1DF8051C"
#define CAN_SPD_TERM  "0CFEE81C"
#define CAN_SPD_TERM2 "18FEE81C"
#define CAN_SPD_TERM3 "1DF8021C"
#define CAN_XTE_TERM  "0CFFFF2A"
#define CAN_XTE_TERM2 "1CEBACAA"
#define CAN_XTE_TERM3 "1DF9031C"

#define CAN_POS_ID  0x0CFEF31C
#define CAN_POS_ID2 0x18FEF31C
#define CAN_SPD_ID  0x0CFEE81C
#define CAN_SPD_ID2 0x18FEE81C
#define CAN_XTE_ID  0x0CFFFF2A
#define CAN_XTE_ID2 0x1CEBACAA

#define GPS_INVALID_FLOAT 999999.9
#define GPS_INVALID_LONG  0xFFFFFFFF

#define GPS_NO_STATS

#define MINSPEED 0.5f

class VehicleGps {
private:
    Stream*         serialDebug;
    HardwareSerial* serialGps;
    byte            baudrate;
    byte            rtkQuality;
    bool            rawEcho;

    float         time,      newTime;
    unsigned long date,      newDate;
    float         latitude,  newLatitude;
    float         longitude, newLongitude;
    float         altitude,  newAltitude;
    float         speed,     newSpeed;
    float         course,    newCourse;
    int           xte,       newXte;
    byte          quality,   newQuality;

    unsigned long lastGgaFix;
    unsigned long lastVtgFix;
    unsigned long lastXteFix;

    char         term[20];
    byte         termNumber;
    byte         termOffset;
    byte         parity;
    byte         checksum;
    unsigned int sum;
    bool         isChecksumTerm;

    enum types { GGA, VTG, XTE, XTE2, CAN_POS, CAN_SPD, CAN_XTE, CAN_XTE2, OTHER };
    types sentenceType;

#ifndef GPS_NO_STATS
    unsigned long encodedCharacters;
    unsigned long goodSentences;
    unsigned long failedChecksum;
    unsigned long passedChecksum;
#endif

    float parseDecimal(const char* c);
    float parseDegrees(const char* c);
    int   parseInteger(const char* c);

    bool strcmp_(const char* str1, const char* str2);
    byte hexToInt(char c);

    bool parseTerm();

    bool readCalibrationData();
    void writeCalibrationData();

public:
    VehicleGps(Stream* serialDebug, HardwareSerial* serialGps);

    bool Update();
    bool Update(long int id, const uint8_t* data, byte len);

    static float DistanceBetween(float lat1, float lon1, float lat2, float lon2);
    static float DistanceBetween(float* lat1, float* lon1, float* lat2, float* lon2);

    void PrintCalibrationData();

#ifndef GPS_NO_STATS
    void Stats(unsigned long* chars, unsigned short* sentences, unsigned short* failedCs);
#endif

    inline boolean ResetCalibration()  { return readCalibrationData(); }
    inline void    CommitCalibration() { writeCalibrationData(); }
    inline boolean MinSpeed()          { return GetSpeedMs() >= MINSPEED; }

    // When enabled, every character read from serialGps in Update() is echoed
    // verbatim to serialDebug — raw NMEA/CAN passthrough for diagnostics.
    inline void SetRawEcho(bool enable) { rawEcho = enable; }
    inline bool GetRawEcho()            { return rawEcho; }

    // Setters for values arriving via CAN / NMEA 2000 rather than serial NMEA
    inline void SetSpeedKnots(float knots)         { speed = knots;   lastVtgFix = millis(); }
    inline void SetCourseDeg(float degrees)        { course = degrees; lastVtgFix = millis(); }
    inline void SetPosition(float lat, float lon)  { latitude = lat; longitude = lon; lastGgaFix = millis(); }
    inline void SetXte(int hundredthsM)            { xte = hundredthsM; lastXteFix = millis(); }
    inline void SetQuality(byte q)                 { quality = q; }
    inline void SetBaudrate(byte b)                { baudrate = b; }

    inline void SetRtkQuality(byte q) {
        rtkQuality = (q == 4 || q == 2) ? q : 4;
    }

    inline bool IsRtkQuality() { return quality == rtkQuality; }

    // Getters
    inline HardwareSerial* GetSerial()    { return serialGps; }
    inline byte            GetBaudrate()  { return baudrate; }
    inline byte            GetRtkQuality(){ return rtkQuality; }

    // date as ddmmyy, time as hhmmsscc
    inline void GetDatetime(unsigned long* outdate, unsigned long* outtime) {
        if (outdate) *outdate = date;
        if (outtime) *outtime = time;
    }

    inline void GetDatetimeDetails(int* outyear, byte* outmonth, byte* outday,
                                   byte* outhour, byte* outminute,
                                   byte* outsecond, byte* outhundredths = 0) {
        unsigned long d, t;
        GetDatetime(&d, &t);
        if (outyear) {
            *outyear = d % 100;
            *outyear += *outyear > 80 ? 1900 : 2000;
        }
        if (outmonth)      *outmonth      = (d / 100) % 100;
        if (outday)        *outday        = d / 10000;
        if (outhour)       *outhour       = t / 1000000;
        if (outminute)     *outminute     = (t / 10000) % 100;
        if (outsecond)     *outsecond     = (t / 100) % 100;
        if (outhundredths) *outhundredths = t % 100;
    }

    inline void GetPosition(float* outlatitude, float* outlongitude) {
        if (outlatitude)  *outlatitude  = latitude;
        if (outlongitude) *outlongitude = longitude;
    }

    inline float GetAltitude()    { return altitude; }
    inline byte  GetQuality()     { return quality; }
    inline float GetCourse()      { return course; }
    inline float GetSpeed()       { return speed; }
    inline int   GetXte()         { return xte; }
    inline int   GetAltitudeCm()  { return int(altitude * 100); }
    inline float GetSpeedMs()     { return GPS_MS_PER_KNOT  * speed; }
    inline float GetSpeedKmh()    { return GPS_KMH_PER_KNOT * speed; }
    inline float GetXteM()        { return float(xte) / 100; }

    inline unsigned long GetGgaFixAge() { return lastGgaFix; }
    inline unsigned long GetVtgFixAge() { return lastVtgFix; }
    inline unsigned long GetXteFixAge() { return lastXteFix; }
};

}  // namespace triton