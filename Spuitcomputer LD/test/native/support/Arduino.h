#pragma once
// Minimal Arduino stubs for compiling AUnit + InterfaceSprayer on native/MSVC
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "Print.h"
#include "Stream.h"
#include "WString.h"

// Basic Arduino types and macros
typedef uint8_t  byte;
typedef unsigned int word;

#define HIGH 1
#define LOW  0
#define INPUT  0
#define OUTPUT 1
#define INPUT_PULLUP 2

// __FlashStringHelper is a tagged pointer type for PROGMEM strings
#ifndef FLASH_STRING_HELPER_DEFINED
#define FLASH_STRING_HELPER_DEFINED
class __FlashStringHelper {};
#endif

// Also defined by pgmspace.h (which AUnit's own Flash.h includes separately when EPOXY_DUINO
// is set) -- guard with #ifndef so both can be included in the same translation unit without a
// harmless-but-noisy redefinition warning.
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
#ifndef pgm_read_ptr
#  define pgm_read_ptr(p)   (*(const void* const*)(p))
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

// Windows POSIX name compatibility
#ifdef _WIN32
#  include <string.h>
#  ifndef strcasecmp
#    define strcasecmp  _stricmp
#    define strncasecmp _strnicmp
#  endif
#endif

// digitalRead()/analogRead()/millis() are stateful mocks, controllable from
// any test file via the *Value() setters below. Every test_*.cpp now links
// into one combined binary (see ../SpuitcomputerLdNativeTests.cpp), so a test file
// can no longer supply its own private definition of these without hitting a
// duplicate-symbol link error -- matching Salacia's test/native/support/
// Arduino.h. C++17 inline variables (one definition across every translation
// unit that includes this header) avoid needing a separate Arduino.cpp.
namespace internal_arduino_stub
{
inline uint32_t      g_digitalReadPinValues = 0;
inline int           g_analogReadPinValues[64] = { 0 };
inline unsigned long g_millis = 0;
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

inline int analogRead(uint8_t pin) {
    if (pin >= 64) return 0;
    return internal_arduino_stub::g_analogReadPinValues[pin];
}

inline void analogReadValue(uint8_t pin, int value) {
    if (pin >= 64) return;
    internal_arduino_stub::g_analogReadPinValues[pin] = value;
}

inline unsigned long millis() { return internal_arduino_stub::g_millis; }

inline void millisValue(unsigned long ms) { internal_arduino_stub::g_millis = ms; }

// No-op stubs for hardware control not used in tests
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline void analogWrite(uint8_t, int) {}
inline void delay(unsigned long) {}

#ifndef min
#  define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#  define max(a, b) ((a) > (b) ? (a) : (b))
#endif

// HardwareSerial: a Print that writes to stdout; begin() is a no-op
// HardwareSerial: a Print that writes to stdout. begin() only records the
// rate it was given so a test can assert on SerialGuidanceChannel::
// ApplyBaudrate(); end() is a no-op.
class HardwareSerial : public Stream {
  public:
    unsigned long begunBaud = 0;
    void begin(unsigned long baud) { begunBaud = baud; }
    void end() {}
    operator bool() { return true; }
};

extern HardwareSerial Serial;

// EpoxyDuino globals — defined in native_main.cpp, read by AUnit's TestRunner
extern int epoxy_argc;
extern const char* const* epoxy_argv;

// Print::print(String) needs the complete String type from WString.h
#include "PrintImpl.h"
