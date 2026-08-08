#pragma once
// Minimal pgmspace.h stub for native/MSVC builds (no AVR flash memory)
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#  ifndef strcasecmp
#    define strcasecmp  _stricmp
#    define strncasecmp _strnicmp
#  endif
#endif

// __FlashStringHelper is used as a tagged pointer type; forward declare if not yet defined
#ifndef FLASH_STRING_HELPER_DEFINED
#define FLASH_STRING_HELPER_DEFINED
class __FlashStringHelper;
#endif

#ifndef PROGMEM
#  define PROGMEM
#endif
#ifndef PSTR
#  define PSTR(s) (s)
#endif
#ifndef F
#  define F(x) ((const __FlashStringHelper*)(x))
#endif
#ifndef pgm_read_byte
#  define pgm_read_byte(p)  (*(const uint8_t*)(p))
#endif
#ifndef pgm_read_word
#  define pgm_read_word(p)  (*(const uint16_t*)(p))
#endif
#ifndef pgm_read_dword
#  define pgm_read_dword(p) (*(const uint32_t*)(p))
#endif
#ifndef pgm_read_ptr
#  define pgm_read_ptr(p)   (*(const void* const*)(p))
#endif
#ifndef strlen_P
#  define strlen_P    strlen
#  define strcmp_P    strcmp
#  define strncmp_P   strncmp
#  define strcasecmp_P strcasecmp
#  define memcpy_P    memcpy
#  define strcpy_P    strcpy
#  define strncpy_P   strncpy
#  define strstr_P    strstr
#endif
