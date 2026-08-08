#pragma once
// Minimal Arduino stubs for compiling AUnit + InterfaceMotorcontroller/SteeringActuator
// on native/MSVC. Not a real EpoxyDuino dependency -- EpoxyDuino's own
// cores/epoxy/main.cpp requires <termios.h>, which doesn't exist on Windows/MinGW.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string>
#include "Print.h"
#include "Stream.h"
#include "WString.h"

typedef uint8_t  byte;
typedef unsigned int word_t;
typedef bool boolean;

#define HIGH 1
#define LOW  0
#define INPUT  0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define RISING  3
#define FALLING 2
#define CHANGE  1

#define A0 0
#define A1 1
#define A2 2
#define A3 3
#define A4 4
#define A5 5
#define A6 6
#define A7 7
#define A8 8
#define A9 9
#define A10 10

#ifndef FLASH_STRING_HELPER_DEFINED
#define FLASH_STRING_HELPER_DEFINED
class __FlashStringHelper {};
#endif

#ifndef PROGMEM
#  define PROGMEM
#endif
#ifndef PSTR
#  define PSTR(x)         (x)
#endif
#ifndef F
#  define F(x)            ((const __FlashStringHelper*)(x))
#endif
#ifndef pgm_read_byte
#  define pgm_read_byte(p)  (*(const uint8_t*)(p))
#endif
#ifndef pgm_read_word
#  define pgm_read_word(p)  (*(const uint16_t*)(p))
#endif
#ifndef strlen_P
#  define strlen_P          strlen
#  define strcmp_P          strcmp
#  define strncmp_P         strncmp
#  define strcasecmp_P      strcasecmp
#  define memcpy_P          memcpy
#endif

#ifndef SERIAL_PORT_MONITOR
#define SERIAL_PORT_MONITOR Serial
#endif

#ifdef _WIN32
#  include <string.h>
#  ifndef strcasecmp
#    define strcasecmp  _stricmp
#    define strncasecmp _strnicmp
#  endif
#endif

namespace internal_arduino_stub
{
inline uint32_t g_digitalReadPinValues = 0;
inline uint32_t g_digitalWritePinValues = 0;
inline int      g_analogReadPinValues[64] = { 0 };
inline uint32_t g_pwmPinValues[64] = { 0 };
}

inline bool digitalRead(uint8_t pin) {
    if (pin >= 32) return false;
    return (internal_arduino_stub::g_digitalReadPinValues & (uint32_t(1) << pin)) != 0;
}

inline void digitalReadValue(uint8_t pin, bool val) {
    if (pin >= 32) return;
    if (!val) internal_arduino_stub::g_digitalReadPinValues &= ~(uint32_t(1) << pin);
    else       internal_arduino_stub::g_digitalReadPinValues |=  (uint32_t(1) << pin);
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin >= 32) return;
    if (!val) internal_arduino_stub::g_digitalWritePinValues &= ~(uint32_t(1) << pin);
    else       internal_arduino_stub::g_digitalWritePinValues |=  (uint32_t(1) << pin);
}

inline uint8_t digitalWriteValue(uint8_t pin) {
    if (pin >= 32) return 0;
    return (internal_arduino_stub::g_digitalWritePinValues & (uint32_t(1) << pin)) != 0;
}

inline int analogRead(uint8_t pin) {
    if (pin >= 64) return 0;
    return internal_arduino_stub::g_analogReadPinValues[pin];
}

inline void analogReadValue(uint8_t pin, int value) {
    if (pin >= 64) return;
    internal_arduino_stub::g_analogReadPinValues[pin] = value;
}

inline void pinMode(uint8_t, uint8_t) {}
inline void delay(unsigned long) {}
inline void analogReference(uint8_t) {}
inline void analogWrite(uint8_t, int) {}
inline void analogWriteFrequency(uint8_t, uint32_t) {}

// Seeeduino/Adafruit SAMD core's combined frequency+duty PWM call (see
// SteeringActuator.cpp's writePwm()) -- this project's real target (seeed_xiao) always
// takes this branch, unlike Salacia's Teensy targets.
inline void pwm(uint8_t pin, uint32_t /*freq*/, uint32_t value) {
    if (pin >= 64) return;
    internal_arduino_stub::g_pwmPinValues[pin] = value;
}

inline uint32_t pwmValue(uint8_t pin) {
    if (pin >= 64) return 0;
    return internal_arduino_stub::g_pwmPinValues[pin];
}

inline void attachInterrupt(uint8_t, void (*)(void), int) {}
inline void detachInterrupt(uint8_t) {}
inline uint8_t digitalPinToInterrupt(uint8_t pin) { return pin; }

inline unsigned long millis() { return 0; }

#ifndef min
#  define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#  define max(a, b) ((a) > (b) ? (a) : (b))
#endif

// HardwareSerial-shaped stub with a settable RX queue -- InterfaceMotorcontroller's
// ParseSerial() reads real Serial.available()/Serial.read() to decode incoming roboteq
// commands, unlike every other Triton module (which never parses live serial input this
// way), so this needs an injectable input buffer, not just an output sink.
class NativeSerial : public Stream {
  public:
    void begin(unsigned long) {}
    void end() {}
    operator bool() { return true; }

    int available() override { return static_cast<int>(rxBuffer.size()); }

    int read() override {
        if (rxBuffer.empty()) return -1;
        int c = static_cast<unsigned char>(rxBuffer.front());
        rxBuffer.erase(rxBuffer.begin());
        return c;
    }

    int peek() override {
        if (rxBuffer.empty()) return -1;
        return static_cast<unsigned char>(rxBuffer.front());
    }

    // Test-control hook: appends text to the simulated incoming serial stream.
    void inject(const char* text) { rxBuffer += text; }

    // Test-control hooks for what this side has sent back -- InterfaceMotorcontroller's
    // SendAnswer()/SendAllVars() write here via the overridden write() below. Also still
    // writes to real stdout (Print::write()'s own default behavior) so AUnit's own
    // TestRunner progress output stays visible instead of being silently captured too.
    size_t write(uint8_t c) override {
        txBuffer += static_cast<char>(c);
        return Print::write(c);
    }
    const std::string& sent() const { return txBuffer; }
    void clearSent() { txBuffer.clear(); }

  private:
    std::string rxBuffer;
    std::string txBuffer;
};

extern NativeSerial Serial;

// EpoxyDuino globals — defined in native_main.cpp, read by AUnit's TestRunner
extern int epoxy_argc;
extern const char* const* epoxy_argv;

// Print::print(String) needs the complete String type from WString.h
#include "PrintImpl.h"
