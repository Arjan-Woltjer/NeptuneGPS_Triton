/*
  InterfaceI2CLCD - I2C LCD driver with buffered screen output
  Copyright (C) 2011-2026 J.A. Woltjer.
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "InterfaceI2CLCD.hpp"

namespace triton
{

InterfaceI2CLCD::InterfaceI2CLCD(TwoWire* lcdWire, uint8_t addr,
                                 uint8_t cols, uint8_t rows,
                                 uint8_t sdaPin, uint8_t sclPin,
                                 uint8_t charsize)
    // Same order as the declarations in the header (-Wreorder). cols/rows are
    // clamped to the fixed char[4][21] buffer and screen arrays.
    : lcdWire(lcdWire),
      sdaPin(sdaPin),
      sclPin(sclPin),
      deviceAddress(addr),
      cols(cols > 20 ? 20 : cols),
      rows(rows > 4 ? 4 : rows)
    {
    // Initialize display control and mode, screencounter and cursor positions.
    displaycontrol = 0;
    displaymode = 0;
    screencounter = 0;
    cursorRow = 0;
    cursorCol = 0;

    // Set the display function to 4-bit mode, 1 line, 5x8 dots by default.
    displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;

    // Adjust the display function based on the number of rows and character size.
    if (rows > 1) {
        displayfunction |= LCD_2LINE;
    }
    if (charsize != 0 && rows == 1) {
        displayfunction |= LCD_5x10DOTS;
    }

    // Set the backlight to the initial value (ON).
    Backlight();

    // Clear the whole arrays, not just the configured area, so column 20 (the
    // terminator slot) is set too and every row is a valid empty string.
    for (size_t r = 0; r < sizeof(buffer) / sizeof(buffer[0]); r++) {
        for (size_t c = 0; c < sizeof(buffer[0]); c++) {
            buffer[r][c] = 0;
            screen[r][c] = 0;
        }
    }
}

void InterfaceI2CLCD::Begin() {
#if defined(ARDUINO_ARCH_ESP32)
    lcdWire->begin(sdaPin, sclPin);
#else
    lcdWire->begin();
#endif
    delay(50);  // HD44780 needs > 40 ms after power-up before the first command

    enterFourBitMode();

    // displayfunction is already set in the constructor based on rows and charsize.
    Command(LCD_FUNCTIONSET | displayfunction);

    // Set the display on, cursor off, and blink off by default.
    // displaycontrol is set according to the display flags.
    Display();
    NoCursor();
    NoBlink();

    // Set the entry mode to left-to-right and no autoscroll by default.
    // displaymode is set according to the entry mode flags.
    LeftToRight();
    NoAutoscroll();

    Clear(); // Clear the display at the beginning
    Home(); // Move the cursor to the home position at the beginning
}

void InterfaceI2CLCD::Command(uint8_t value) {
    send(value, 0);
}

void InterfaceI2CLCD::Clear() {
    Command(LCD_CLEARDISPLAY);
    delayMicroseconds(2000);
}

void InterfaceI2CLCD::Home() {
    Command(LCD_RETURNHOME);
    delayMicroseconds(2000);
}

void InterfaceI2CLCD::Display() {
    displaycontrol |= LCD_DISPLAYON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::NoDisplay() {
    displaycontrol &= ~LCD_DISPLAYON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::Cursor() {
    displaycontrol |= LCD_CURSORON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::NoCursor() {
    displaycontrol &= ~LCD_CURSORON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::Blink() {
    displaycontrol |= LCD_BLINKON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::NoBlink() {
    displaycontrol &= ~LCD_BLINKON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

// The backlight is a PCF8574 pin, not an LCD command.
// Backlight() and NoBacklight() just set the backlightval variable;
// the actual pin state is updated during expanderWrite() calls.
void InterfaceI2CLCD::Backlight() {
    backlightval = LCD_BACKLIGHT;
}

void InterfaceI2CLCD::NoBacklight() {
    backlightval = LCD_NOBACKLIGHT;
}

void InterfaceI2CLCD::Autoscroll() {
    displaymode |= LCD_ENTRYSHIFTINCREMENT;
    Command(LCD_ENTRYMODESET | displaymode);
}

void InterfaceI2CLCD::NoAutoscroll() {
    displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    Command(LCD_ENTRYMODESET | displaymode);
}

void InterfaceI2CLCD::ScrollDisplayLeft() {
    Command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void InterfaceI2CLCD::ScrollDisplayRight() {
    Command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void InterfaceI2CLCD::LeftToRight() {
    displaymode |= LCD_ENTRYLEFT;
    Command(LCD_ENTRYMODESET | displaymode);
}

void InterfaceI2CLCD::RightToLeft() {
    displaymode &= ~LCD_ENTRYLEFT;
    Command(LCD_ENTRYMODESET | displaymode);
}

// Stores a 5x8 custom character (8 rows of 5 bits) in CGRAM slot 0-7; print
// it afterwards as character code `location`. The pattern bytes go straight to
// the LCD with WriteDirect(): write() only fills the screen buffer.
void InterfaceI2CLCD::CreateChar(uint8_t location, uint8_t charmap[]) {
    location &= 0x7;
    Command(LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++) {
        WriteDirect(charmap[i]);
    }
    // The address counter now points into CGRAM; point it back at the screen
    // (DDRAM), at the cursor this class already tracks.
    SetCursor(cursorCol, cursorRow);
}

// One data byte straight to the LCD at its current address, bypassing the
// screen buffer. Used by CreateChar() for CGRAM; on the screen itself it
// leaves buffer and screen out of step, so text goes through WriteBuffer().
void InterfaceI2CLCD::WriteDirect(uint8_t value) {
    send(value, RS);
}

void InterfaceI2CLCD::SetCursor(uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };

    // Clamp before use. rows is at most 4 (constructor), the size of row_offsets.
    if (rows == 0) {
        return;
    }
    if (row >= rows) {
        row = rows - 1;
    }
    if (cols > 0 && col >= cols) {
        col = cols - 1;
    }

    cursorCol = col;
    cursorRow = row;
    Command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// write() overrides the Print::write method to write a character to the buffer at the current cursor position.
size_t InterfaceI2CLCD::write(uint8_t value) {
    WriteBuffer(char(value), cursorRow, cursorCol);
    cursorCol++;
    if (cursorCol == cols) {
        cursorCol = 0;
        cursorRow++;
    }
    if (cursorRow == rows) {
        cursorRow = 0;
    }
    return 1;
}

// WriteScreen(n): flushes n changed characters per call.
// Pass 0xFF to rewrite the entire screen in one call.
void InterfaceI2CLCD::WriteScreen(uint8_t n) {
    if (n == 0xFF) {
        screencounter = 0;
    }

    // n-- below would wrap 0 to 255 and write a whole screen's worth of
    // characters for a call that asked for none.
    if (n == 0) {
        return;
    }

    while (true) {
        uint8_t r = screencounter / cols;
        if (r == rows) {
            screencounter = 0;
            return;
        }
        uint8_t c = screencounter % cols;

        if (screen[r][c] != buffer[r][c]) {
            SetCursor(c, r);
            send(buffer[r][c], RS);
            screen[r][c] = buffer[r][c];
            n--;
            if (n == 0) {
                return;
            }
        }
        screencounter++;
    }
}

// WriteBuffer(line, lineNo): writes a string to the buffer at the specified line number.
void InterfaceI2CLCD::WriteBuffer(const char line[], uint8_t lineNo) {
    if (lineNo >= rows || line == nullptr) {
        return;
    }

    // Truncate to the row's 20 characters: an over-long line would otherwise
    // run into the next row and, from row 3, into screen. Callers pass the
    // full 20-character width, so a clipped line is a display fault, not
    // memory corruption.
    const size_t capacity = sizeof(buffer[0]) - 1;
    size_t i = 0;
    while (i < capacity && line[i] != '\0') {
        buffer[lineNo][i] = line[i];
        i++;
    }
    buffer[lineNo][i] = '\0';
}

void InterfaceI2CLCD::WriteBuffer(char character, uint8_t lineNo, uint8_t colNo) {
    if (lineNo < rows && colNo < cols) {
        buffer[lineNo][colNo] = character;
    }
}

// ----------------------------------------
// Private functions
// ----------------------------------------

// HD44780 power-on handshake. The controller starts in 8-bit mode and only
// sees the top nibble, so this must be single write4bits() calls: send()
// would split each byte into two nibbles, which the controller does not
// understand until the last step has put it in 4-bit mode.
void InterfaceI2CLCD::enterFourBitMode() {
    // The PCF8574 powers up with every pin high, so EN, RS and RW start high.
    // Pull them all low first (backlight as set): otherwise the first
    // write4bits() drops EN while the data lines change in the same write,
    // and the LCD may latch a garbage first nibble.
    expanderWrite(0);

    write4bits(0x03 << 4);
    delayMicroseconds(4500);
    write4bits(0x03 << 4);
    delayMicroseconds(4500);
    write4bits(0x03 << 4);
    delayMicroseconds(150);
    write4bits(0x02 << 4);
}

void InterfaceI2CLCD::send(uint8_t value, uint8_t mode) {
    uint8_t highnib = value & 0xf0;
    uint8_t lownib  = (value << 4) & 0xf0;
    write4bits(highnib | mode);
    write4bits(lownib  | mode);
}

void InterfaceI2CLCD::write4bits(uint8_t value) {
    expanderWrite(value);
    pulseEnable(value);
}

void InterfaceI2CLCD::expanderWrite(uint8_t data) {
    lcdWire->beginTransmission(deviceAddress);
    lcdWire->write((int)(data) | backlightval);
    lcdWire->endTransmission();
}

void InterfaceI2CLCD::pulseEnable(uint8_t data) {
    expanderWrite(data | EN);
    delayMicroseconds(1);
    expanderWrite(data & ~EN);
    delayMicroseconds(50);
}

}  // namespace triton