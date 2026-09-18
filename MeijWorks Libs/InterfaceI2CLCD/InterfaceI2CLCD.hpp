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
#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace triton
{

// HD44780 command bytes
#define LCD_CLEARDISPLAY        0x01
#define LCD_RETURNHOME          0x02
#define LCD_ENTRYMODESET        0x04
#define LCD_DISPLAYCONTROL      0x08
#define LCD_CURSORSHIFT         0x10
#define LCD_FUNCTIONSET         0x20
#define LCD_SETCGRAMADDR        0x40
#define LCD_SETDDRAMADDR        0x80

// Entry mode flags
#define LCD_ENTRYRIGHT          0x00
#define LCD_ENTRYLEFT           0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Display on/off flags
#define LCD_DISPLAYON           0x04
#define LCD_DISPLAYOFF          0x00
#define LCD_CURSORON            0x02
#define LCD_CURSOROFF           0x00
#define LCD_BLINKON             0x01
#define LCD_BLINKOFF            0x00

// Display/cursor shift flags
#define LCD_DISPLAYMOVE         0x08
#define LCD_CURSORMOVE          0x00
#define LCD_MOVERIGHT           0x04
#define LCD_MOVELEFT            0x00

// Function set flags
#define LCD_8BITMODE            0x10
#define LCD_4BITMODE            0x00
#define LCD_2LINE               0x08
#define LCD_1LINE               0x00
#define LCD_5x10DOTS            0x04
#define LCD_5x8DOTS             0x00

// Backlight flags
#define LCD_BACKLIGHT           B00001000
#define LCD_NOBACKLIGHT         B00000000

// PCF8574 bit positions
#define EN                      B00000100  // Enable bit
#define RW                      B00000010  // Read/Write bit
#define RS                      B00000001  // Register select bit

class InterfaceI2CLCD : public Print {
private:
    TwoWire* lcdWire;
    uint8_t  sdaPin;
    uint8_t  sclPin;
    uint8_t  deviceAddress;
    uint8_t  displayfunction;
    uint8_t  displaycontrol;
    uint8_t  displaymode;
    uint8_t  numlines;
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  backlightval;

    uint8_t screencounter;
    uint8_t cursorRow;
    uint8_t cursorCol;
    char    buffer[4][21];
    char    screen[4][21];

    void send(uint8_t, uint8_t);
    void write4bits(uint8_t);
    void expanderWrite(uint8_t);
    void pulseEnable(uint8_t);

public:
    InterfaceI2CLCD(TwoWire* lcdWire, uint8_t addr,
                    uint8_t cols, uint8_t rows,
                    uint8_t sdaPin = 0, uint8_t sclPin = 0,
                    uint8_t charsize = LCD_5x8DOTS);

    void Begin();

    // write() keeps its Arduino-standard lowercase name — virtual override of Print::write
    size_t write(uint8_t value) override;

    void Command(uint8_t value);
    void WriteDirect(uint8_t value);

    void Clear();
    void Home();
    void NoDisplay();
    void Display();
    void NoCursor();
    void Cursor();
    void NoBlink();
    void Blink();
    void NoBacklight();
    void Backlight();
    void ScrollDisplayLeft();
    void ScrollDisplayRight();
    void LeftToRight();
    void RightToLeft();
    void PrintLeft();
    void PrintRight();
    void Autoscroll();
    void NoAutoscroll();

    void CreateChar(uint8_t location, uint8_t charmap[]);
    void SetCursor(uint8_t col, uint8_t row);

    // Buffered screen output — WriteScreen(n) flushes row n; -1 flushes all rows
    void WriteScreen(uint8_t n);
    void WriteBuffer(const char line[], uint8_t lineNo);
    void WriteBuffer(char character, uint8_t lineNo, uint8_t colNo);
};

}  // namespace triton