#pragma once
// Minimal InterfaceI2CLCD stub for native/MSVC unit tests -- no-op LCD writes.
// InterfacePlough.cpp's UpdateScreen() (rendering) isn't asserted on by any
// test, but the file must still compile since CheckButtons()/Update() (which
// are tested) live in the same translation unit.
#include <stdint.h>

class TwoWire;

class InterfaceI2CLCD {
  public:
    void WriteBuffer(const char*, uint8_t) {}
    void WriteBuffer(char, uint8_t, uint8_t) {}
    void WriteScreen(uint8_t) {}
};
