#pragma once
// Minimal InterfaceI2CLCD stub for native/MSVC unit tests. It keeps the four
// 20-character rows the real class buffers, so the calibration wizard's tests
// (test_CalibrationWizard.cpp) can read the screen; WriteScreen() writes
// nothing. InterfacePlough.cpp's UpdateScreen() renders into it too, but no
// test asserts on that.
#include <stdint.h>
// No <string> here: this header comes after Arduino.h, whose min/max macros
// break GCC's standard library headers (CI, 2026-10-21).

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
    // A call that may write more than one character in one go: on the board
    // each changed character costs about 3 ms of I2C, so a whole screen holds
    // loop() up for ~150 ms. Counted so tests can require one per pass.
    void WriteScreen(uint8_t n) { if (n > 1) multiCharWrites++; }
    int multiCharWrites = 0;

    // The row as 20 characters, null-terminated.
    const char* Row(uint8_t lineNo) const { return rows[lineNo]; }
    void Clear() {
        multiCharWrites = 0;
        for (uint8_t r = 0; r < 4; r++) {
            for (uint8_t c = 0; c < 20; c++) rows[r][c] = ' ';
            rows[r][20] = '\0';
        }
    }

  private:
    char rows[4][21];
};
