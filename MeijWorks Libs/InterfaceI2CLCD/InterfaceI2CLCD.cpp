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
    : lcdWire(lcdWire), sdaPin(sdaPin), sclPin(sclPin),
      deviceAddress(addr),
      displayfunction(LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS),
      displaycontrol(0), displaymode(0),
      // buffer and screen are fixed char[4][21]; constructing with dimensions
      // larger than that would have overflowed both arrays inside the
      // constructor itself, before any caller could get it wrong.
      numlines(rows > 4 ? 4 : rows),
      cols(cols > 20 ? 20 : cols),
      rows(rows > 4 ? 4 : rows),
      backlightval(LCD_BACKLIGHT),
      screencounter(0), cursorRow(0), cursorCol(0) {
    if (rows > 1) {
        displayfunction |= LCD_2LINE;
    }
    if (charsize != 0 && rows == 1) {
        displayfunction |= LCD_5x10DOTS;
    }
    // Clear the whole arrays, not just the configured area: column index 20 is
    // the terminator slot and was never initialised, leaving each row an
    // unterminated char[21] of indeterminate content until the first write.
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

    delay(500);

    expanderWrite(backlightval);
    delay(1000);

    write4bits(0x03 << 4);
    delayMicroseconds(4500);
    write4bits(0x03 << 4);
    delayMicroseconds(4500);
    write4bits(0x03 << 4);
    delayMicroseconds(150);
    write4bits(0x02 << 4);

    Command(LCD_FUNCTIONSET | displayfunction);

    displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    Display();
    Clear();

    displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    Command(LCD_ENTRYMODESET | displaymode);

    Home();
}

void InterfaceI2CLCD::Command(uint8_t value) {
    send(value, 0);
}

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
    return 0;
}

void InterfaceI2CLCD::WriteDirect(uint8_t value) {
    send(value, RS);
}

void InterfaceI2CLCD::Clear() {
    Command(LCD_CLEARDISPLAY);
    delayMicroseconds(2000);
}

void InterfaceI2CLCD::Home() {
    Command(LCD_RETURNHOME);
    delayMicroseconds(2000);
}

void InterfaceI2CLCD::NoDisplay() {
    displaycontrol &= ~LCD_DISPLAYON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::Display() {
    displaycontrol |= LCD_DISPLAYON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::NoCursor() {
    displaycontrol &= ~LCD_CURSORON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::Cursor() {
    displaycontrol |= LCD_CURSORON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::NoBlink() {
    displaycontrol &= ~LCD_BLINKON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::Blink() {
    displaycontrol |= LCD_BLINKON;
    Command(LCD_DISPLAYCONTROL | displaycontrol);
}

void InterfaceI2CLCD::NoBacklight() {
    backlightval = LCD_NOBACKLIGHT;
    expanderWrite(0);
}

void InterfaceI2CLCD::Backlight() {
    backlightval = LCD_BACKLIGHT;
    expanderWrite(0);
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

void InterfaceI2CLCD::Autoscroll() {
    displaymode |= LCD_ENTRYSHIFTINCREMENT;
    Command(LCD_ENTRYMODESET | displaymode);
}

void InterfaceI2CLCD::NoAutoscroll() {
    displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    Command(LCD_ENTRYMODESET | displaymode);
}

void InterfaceI2CLCD::CreateChar(uint8_t location, uint8_t charmap[]) {
    location &= 0x7;
    Command(LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++) {
        write(charmap[i]);
    }
}

void InterfaceI2CLCD::SetCursor(uint8_t col, uint8_t row) {
    int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };

    // Clamp before use, not after. The bound was `row > numlines`, so with the
    // usual numlines of 4 a row of exactly 4 passed and indexed row_offsets[4],
    // one past the end -- and cursorRow was assigned the unclamped value either
    // way. row_offsets has four entries whatever numlines says, so bound by both.
    const uint8_t maxRow = (numlines < 4 ? numlines : 4);
    if (maxRow == 0) {
        return;
    }
    if (row >= maxRow) {
        row = maxRow - 1;
    }
    if (cols > 0 && col >= cols) {
        col = cols - 1;
    }

    cursorCol = col;
    cursorRow = row;
    Command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
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

void InterfaceI2CLCD::WriteBuffer(const char line[], uint8_t lineNo) {
    if (lineNo >= rows || line == nullptr) {
        return;
    }

    // The row index was checked and the string length was not. buffer is
    // char[4][21] with screen[4][21] immediately after it in the class layout,
    // so an over-long line ran into the next row and, from row 3, into screen.
    // Every caller currently passes exactly 20 characters -- the full width,
    // with no margin -- so one added character to any language string was
    // enough. Truncate instead: a clipped line is a display fault, not memory
    // corruption.
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