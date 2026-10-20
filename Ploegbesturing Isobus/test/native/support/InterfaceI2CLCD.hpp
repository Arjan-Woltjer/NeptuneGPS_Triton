#pragma once
// Minimal InterfaceI2CLCD stub for native/MSVC unit tests. It keeps the four
// 20-character rows the real class buffers, so the calibration wizard's tests
// (test_CalibrationWizard.cpp) can read the screen; WriteScreen() writes
// nothing. InterfacePlough.cpp's UpdateScreen() renders into it too, but no
// test asserts on that.
#include <stdint.h>
#include <string>

class TwoWire;

class InterfaceI2CLCD {
  public:
    InterfaceI2CLCD() { Clear(); }

    void WriteBuffer(const char* line, uint8_t lineNo) {
        if (lineNo >= 4) return;
        for (uint8_t i = 0; i < 20 && line[i] != '\0'; i++) rows[lineNo][i] = line[i];
    }
    void WriteBuffer(char c, uint8_t lineNo, uint8_t colNo) {
        if (lineNo < 4 && colNo < 20) rows[lineNo][colNo] = c;
    }
    void WriteScreen(uint8_t) {}

    std::string Row(uint8_t lineNo) const { return std::string(rows[lineNo], 20); }
    void Clear() {
        for (uint8_t r = 0; r < 4; r++) {
            for (uint8_t c = 0; c < 20; c++) rows[r][c] = ' ';
            rows[r][20] = '\0';
        }
    }

  private:
    char rows[4][21];
};
