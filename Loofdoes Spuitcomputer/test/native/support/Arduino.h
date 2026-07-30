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

// HAL declarations — the test file provides the definitions for millis/digitalRead/analogRead
unsigned long millis();
bool digitalRead(uint8_t pin);
int  analogRead(uint8_t pin);

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
class HardwareSerial : public Stream {
  public:
    void begin(unsigned long) {}
    void end() {}
    operator bool() { return true; }
};

extern HardwareSerial Serial;

// EpoxyDuino globals — defined in native_main.cpp, read by AUnit's TestRunner
extern int epoxy_argc;
extern const char* const* epoxy_argv;

// Print::print(String) needs the complete String type from WString.h
#include "PrintImpl.h"
