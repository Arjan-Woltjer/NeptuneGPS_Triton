#pragma once
// Minimal GuidanceSource stub for native/MSVC unit tests. Exposes the subset
// of the real triton::GuidanceSource API that ImplementPlough/InterfacePlough
// need as directly-settable public fields (matching Loofdoes' fake VehicleGps
// precedent, and this project's own prior fake before this rename), plus
// real Set*() methods matching the real class's signatures so
// test_GpsParsers.cpp's ported parser tests -- which call setters and read
// back through getters, rather than poking fields directly -- behave
// identically whether they resolve to this fake or the real class. The
// setters here are plain field assignments with no timestamp side effects;
// the ported parser tests never assert on fix-ages, only on decoded values.
#include <stdint.h>

class Stream;

class GuidanceSource {
  public:
    int           xte = 0;
    unsigned long lastXteFix = 0;  // matches the real GuidanceSource's private field name
    unsigned long ggaFixAge = 0;
    unsigned long vtgFixAge = 0;
    bool          rtkQuality = true;
    bool          minSpeed = true;

    float         latitude = 0.0f;
    float         longitude = 0.0f;
    float         altitude = 0.0f;
    float         course = 0.0f;
    float         speed = 0.0f;
    float         time = 0.0f;
    unsigned long date = 0;
    uint8_t       quality = 0;

    GuidanceSource() {}

    int           GetXte()          { return xte; }
    unsigned long GetXteTimestamp() { return lastXteFix; }
    unsigned long GetGgaFixAge()  { return ggaFixAge; }
    unsigned long GetVtgFixAge()  { return vtgFixAge; }
    bool          IsRtkQuality()  { return rtkQuality; }
    bool          MinSpeed()      { return minSpeed; }

    float   GetLatitude()  { return latitude; }
    float   GetLongitude() { return longitude; }
    float   GetAltitude()  { return altitude; }
    float   GetCourse()    { return course; }
    float   GetSpeed()     { return speed; }
    uint8_t GetQuality()   { return quality; }

    void GetDatetime(unsigned long* outdate, unsigned long* outtime) {
        if (outdate) *outdate = date;
        if (outtime) *outtime = (unsigned long)time;
    }

    void NoteGgaFixReceived()              {}
    void SetPosition(float lat, float lon) { latitude = lat; longitude = lon; }
    void SetAltitude(float alt)            { altitude = alt; }
    void SetSpeedKnots(float knots)        { speed = knots; }
    void SetCourseDeg(float degrees)       { course = degrees; }
    void SetTime(float t)                  { time = t; }
    void SetDate(unsigned long d)          { date = d; }
    void SetQuality(uint8_t q)             { quality = q; }
    void SetXte(int hundredthsM)           { xte = hundredthsM; }
    void SetXte(int hundredthsM, uint8_t q) { xte = hundredthsM; quality = q; }
};
