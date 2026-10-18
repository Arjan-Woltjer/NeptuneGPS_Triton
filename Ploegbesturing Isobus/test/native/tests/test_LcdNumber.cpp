/*
  test_LcdNumber - the shared LCD number rendering (MeijWorks Libs/LcdNumber)
  draws exactly what the interface screens' digit-by-digit copies drew
  (NeptuneGPS_Triton#177).
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <string> uses std::min/max with three arguments.
#include <string>

#include <AUnit.h>
#include "LcdNumber.hpp"

using namespace aunit;
using namespace triton;

namespace {

// A 4x20 character buffer standing in for the LCD.
struct FakeLcd {
    char cells[4][20];
    FakeLcd() { Clear('#'); }
    void Clear(char c) {
        for (auto& row : cells) for (char& cell : row) cell = c;
    }
    void WriteBuffer(char c, uint8_t row, uint8_t col) {
        if (row < 4 && col < 20) cells[row][col] = c;
    }
    std::string Row(uint8_t row, uint8_t from, uint8_t width) const {
        return std::string(&cells[row][from], width);
    }
};

// The copies, verbatim apart from the names: the signed 4-character field
// (plough, planter, kipper and rooier writeValue()), the sprayer's 3-digit
// dose, its one-decimal speed and its two-decimal volume, the scraper's
// two-decimal comma height and its 5-digit distance.
void LegacySigned4(FakeLcd* lcd, int temp2, uint8_t row, uint8_t col) {
    int temp = abs(temp2);
    if (temp > 99) {
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', row, col);
        lcd->WriteBuffer(temp / 100 + '0', row, col + 1);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', row, col + 2);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 3);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', row, col);
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', row, col + 1);
        lcd->WriteBuffer(temp / 10 + '0', row, col + 2);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 3);
    }
    else {
        lcd->WriteBuffer(' ', row, col);
        lcd->WriteBuffer(' ', row, col + 1);
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', row, col + 2);
        lcd->WriteBuffer(temp + '0', row, col + 3);
    }
}

void LegacySprayerDose(FakeLcd* lcd, int temp) {
    if (temp > 99) {
        lcd->WriteBuffer(temp / 100 + '0', 0, 12);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 0, 13);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 14);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 0, 12);
        lcd->WriteBuffer(temp / 10 + '0', 0, 13);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 14);
    }
    else {
        lcd->WriteBuffer(' ', 0, 12);
        lcd->WriteBuffer(' ', 0, 13);
        lcd->WriteBuffer(temp + '0', 0, 14);
    }
}

void LegacySprayerSpeed(FakeLcd* lcd, int temp2) {
    int temp = abs(temp2);
    if (temp > 99) {
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', 2, 10);
        lcd->WriteBuffer(temp / 100 + '0', 2, 11);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 2, 12);
        temp = temp % 10;
        lcd->WriteBuffer('.', 2, 13);
        lcd->WriteBuffer(temp + '0', 2, 14);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 2, 10);
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', 2, 11);
        lcd->WriteBuffer(temp / 10 + '0', 2, 12);
        temp = temp % 10;
        lcd->WriteBuffer('.', 2, 13);
        lcd->WriteBuffer(temp + '0', 2, 14);
    }
    else {
        lcd->WriteBuffer(' ', 2, 10);
        lcd->WriteBuffer(' ', 2, 11);
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', 2, 12);
        lcd->WriteBuffer('.', 2, 13);
        lcd->WriteBuffer(temp + '0', 2, 14);
    }
}

// Row 3 is first written with its template line, so col 7 is a blank unless
// the value is negative.
void LegacySprayerVolume(FakeLcd* lcd, int temp2) {
    int temp = abs(temp2);
    lcd->WriteBuffer(' ', 3, 7);
    if (temp2 < 0) {
        lcd->WriteBuffer('-', 3, 7);
    }
    if (temp > 9999) {
        lcd->WriteBuffer(temp / 10000 + '0', 3, 8);
        temp = temp % 10000;
        lcd->WriteBuffer(temp / 1000 + '0', 3, 9);
        temp = temp % 1000;
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else if (temp > 999) {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(temp / 1000 + '0', 3, 9);
        temp = temp % 1000;
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else if (temp > 99) {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(' ', 3, 10);
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(' ', 3, 10);
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer('0', 3, 12);
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
}

// The scraper's heights: drawn only from 1,00 up (the copy has no branch for
// smaller values), so the comparison below starts there.
void LegacyScraperHeight(FakeLcd* lcd, int temp2, uint8_t row, uint8_t col) {
    int temp = abs(temp2);
    if (temp > 9999) {
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', row, col);
        lcd->WriteBuffer(temp / 10000 + '0', row, col + 1);
        temp = temp % 10000;
        lcd->WriteBuffer(temp / 1000 + '0', row, col + 2);
        temp = temp % 1000;
        lcd->WriteBuffer(temp / 100 + '0', row, col + 3);
        temp = temp % 100;
        lcd->WriteBuffer(',', row, col + 4);
        lcd->WriteBuffer(temp / 10 + '0', row, col + 5);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 6);
    }
    else if (temp > 999) {
        lcd->WriteBuffer(' ', row, col);
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', row, col + 1);
        lcd->WriteBuffer(temp / 1000 + '0', row, col + 2);
        temp = temp % 1000;
        lcd->WriteBuffer(temp / 100 + '0', row, col + 3);
        temp = temp % 100;
        lcd->WriteBuffer(',', row, col + 4);
        lcd->WriteBuffer(temp / 10 + '0', row, col + 5);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 6);
    }
    else if (temp > 99) {
        lcd->WriteBuffer(' ', row, col);
        lcd->WriteBuffer(' ', row, col + 1);
        lcd->WriteBuffer(temp2 < 0 ? '-' : ' ', row, col + 2);
        lcd->WriteBuffer(temp / 100 + '0', row, col + 3);
        temp = temp % 100;
        lcd->WriteBuffer(',', row, col + 4);
        lcd->WriteBuffer(temp / 10 + '0', row, col + 5);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 6);
    }
}

// Reported as the expected value when a comparison loop finds a mismatch: no
// loop's range reaches it, so the assertion always fails and prints the value.
constexpr int kMismatch = 1000000;

bool Same(const FakeLcd& p, const FakeLcd& q) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 20; c++)
            if (p.cells[r][c] != q.cells[r][c]) return false;
    return true;
}

}  // namespace

test(LcdNumber, signedFourCharacterField_matchesTheCopies) {
    for (int v = -999; v <= 999; v++) {
        FakeLcd legacy, shared;
        LegacySigned4(&legacy, v, 1, 16);
        LcdWriteNumber(&shared, v, 1, 16, 4);
        if (!Same(legacy, shared)) { assertEqual(v, kMismatch); return; }
    }
}

test(LcdNumber, sprayerDose_matchesTheCopy) {
    for (int v = 0; v <= 999; v++) {
        FakeLcd legacy, shared;
        LegacySprayerDose(&legacy, v);
        LcdWriteNumber(&shared, v, 0, 12, 3);
        if (!Same(legacy, shared)) { assertEqual(v, kMismatch); return; }
    }
}

test(LcdNumber, sprayerSpeedOneDecimalNoLeadingZero_matchesTheCopy) {
    for (int v = -999; v <= 999; v++) {
        FakeLcd legacy, shared;
        LegacySprayerSpeed(&legacy, v);
        LcdWriteNumber(&shared, v, 2, 10, 5, 1, '.', false);
        if (!Same(legacy, shared)) { assertEqual(v, kMismatch); return; }
    }
}

// Volume and area are never negative; the copy put a '-' in a fixed column,
// the shared function puts it before the first digit -- compared from 0 up.
test(LcdNumber, sprayerVolumeTwoDecimals_matchesTheCopy) {
    for (int v = 0; v <= 99999; v++) {
        FakeLcd legacy, shared;
        LegacySprayerVolume(&legacy, v);
        LcdWriteNumber(&shared, v, 3, 7, 7, 2, '.', false);
        if (!Same(legacy, shared)) { assertEqual(v, kMismatch); return; }
    }
}

test(LcdNumber, scraperHeightTwoDecimalsComma_matchesTheCopyFromOneUp) {
    for (int v = -99999; v <= 99999; v++) {
        if (v > -100 && v < 100) continue;   // the copy drew nothing there
        FakeLcd legacy, shared;
        LegacyScraperHeight(&legacy, v, 1, 12);
        LcdWriteNumber(&shared, v, 1, 12, 7, 2, ',');
        if (!Same(legacy, shared)) { assertEqual(v, kMismatch); return; }
    }
}

// What the copies got wrong, and the shared function draws instead.
test(LcdNumber, belowOneAndOverflow_areDrawnSanely) {
    FakeLcd lcd;
    LcdWriteNumber(&lcd, 5, 0, 0, 7, 2, ',');            // the scraper drew nothing below 1,00
    assertTrue(lcd.Row(0, 0, 7) == "   0,05");
    LcdWriteNumber(&lcd, -5, 0, 0, 7, 2, ',');
    assertTrue(lcd.Row(0, 0, 7) == "  -0,05");
    LcdWriteNumber(&lcd, 1234, 1, 0, 4);                 // the copies drew ":34" garbage
    assertTrue(lcd.Row(1, 0, 4) == "1234");
    LcdWriteNumber(&lcd, 0, 2, 0, 4);
    assertTrue(lcd.Row(2, 0, 4) == "   0");
    assertEqual(lcd.cells[2][4], '#');                   // nothing outside the field
}
