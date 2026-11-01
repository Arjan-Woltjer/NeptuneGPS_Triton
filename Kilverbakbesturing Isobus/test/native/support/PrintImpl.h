#pragma once
// Inline implementations of Print::print(String) that need WString.h complete type.
// Included at the bottom of Arduino.h, after WString.h has been included.
#include "Print.h"
#include "WString.h"

inline size_t Print::print(const String& s)   { return print(s.c_str()); }
inline size_t Print::println(const String& s) { size_t r = print(s); return r + println(); }
