#pragma once
// Minimal VehicleGps stub for native/MSVC unit tests.
// Only exposes the interface used by ImplementSprayer.
#include <Arduino.h>
#include <string.h>

class Stream;
class HardwareSerial;

class VehicleGps {
public:
    float speed = 0.0f;
    // The real class only ever writes `speed` when a valid message arrives, and
    // stamps lastVtgFix at that same moment. SetSpeed() mirrors that pairing so
    // a test cannot express "moving, but no fix has ever arrived" -- a state a
    // real receiver cannot produce, and one the staleness check would treat as
    // stale.
    unsigned long vtgFix = 0;
    // GGA fix quality as the real class reports it. 1 (plain GPS) by default so
    // the tests that predate the minimum-quality rule keep dosing.
    uint8_t quality = 1;
    float latitude  = 0.0f;
    float longitude = 0.0f;
    unsigned long ggaFix = 0;

    VehicleGps() {}

    void SetSpeed(float speedMs) {
        speed  = speedMs;
        vtgFix = millis();
    }

    float GetSpeedMs() { return speed; }

    // Named "age" but returns an absolute timestamp, matching the real getter.
    unsigned long GetVtgFixAge() { return vtgFix; }

    uint8_t GetQuality() { return quality; }

    // Position side, same absolute-timestamp convention as vtgFix.
    void SetPosition() { ggaFix = millis(); }
    void GetPosition(float* lat, float* lon) {
        if (lat) *lat = latitude;
        if (lon) *lon = longitude;
    }
    unsigned long GetGgaFixAge() { return ggaFix; }

    // Sentence tap as on the real class; FeedSentence() is the test helper.
    char     lastSentence[91] = "";
    uint32_t sentenceSeq = 0;
    void FeedSentence(const char* s) {
        size_t n = strlen(s); if (n > 90) n = 90;
        memcpy(lastSentence, s, n); lastSentence[n] = 0; sentenceSeq++;
    }
    long appliedBaud = 0;                     // what ApplyBaudrate() was last given
    void ApplyBaudrate(long baud) { appliedBaud = baud; }

    const char* GetLastSentence() const { return lastSentence; }
    uint32_t    GetSentenceSeq() const  { return sentenceSeq; }
};
