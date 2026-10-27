/*
  LcdNumber - a number, right-aligned in a fixed field on the 20x4 LCD
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

#include <stdint.h>

namespace triton
{

// Writes a fixed-width number into the LCD buffer, right-aligned:
//
//   LcdWriteNumber(lcd, value, row, col, width)          "  -7", " -42", "-123"
//   LcdWriteNumber(lcd, v, row, col, 7, 2, ',')          "  -1,23", "-123,45"
//   LcdWriteNumber(lcd, v, row, col, 5, 1, '.', false)   "   .5", " 10.5"
//
// The value fills the `width` characters from (row, col) rightwards: its
// digits, the last `decimals` of them after `separator`, a '-' directly before
// the first digit when negative, blanks in front. With leadingZero false a
// value below one shows nothing before the separator (".5", as the sprayer
// always drew its speed). A value that does not fit keeps its last `width`
// characters (the copies drew ':' and other garbage there instead).
//
// A template over the LCD type, so the real InterfaceI2CLCD and the native
// tests' stub both work: all it needs is WriteBuffer(char, row, col).
template <typename Lcd>
void LcdWriteNumber(Lcd* lcd, long value, uint8_t row, uint8_t col, uint8_t width,
                    uint8_t decimals = 0, char separator = '.', bool leadingZero = true) {
    if (width > 16) {
        width = 16;   // the widest field any screen uses is 7
    }
    char text[16];
    int pos = 15;
    unsigned long magnitude = value < 0 ? 0UL - static_cast<unsigned long>(value) : static_cast<unsigned long>(value);

    for (uint8_t i = 0; i < decimals; i++) {
        text[pos--] = static_cast<char>('0' + magnitude % 10);
        magnitude /= 10;
    }
    if (decimals > 0) {
        text[pos--] = separator;
    }
    if (magnitude == 0) {
        if (leadingZero || decimals == 0) {
            text[pos--] = '0';
        }
    }
    else {
        while (magnitude > 0 && pos >= 1) {
            text[pos--] = static_cast<char>('0' + magnitude % 10);
            magnitude /= 10;
        }
    }
    if (value < 0) {
        text[pos--] = '-';
    }

    // Right-aligned: the field's last character is text[15].
    for (uint8_t i = 0; i < width; i++) {
        const int index = 16 - width + i;
        lcd->WriteBuffer(index > pos ? text[index] : ' ', row, static_cast<uint8_t>(col + i));
    }
}

}  // namespace triton
