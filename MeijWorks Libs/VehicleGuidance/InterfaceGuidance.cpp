/*
  InterfaceGuidance - LCD-guided receiver baudrate detection at boot
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
#include "InterfaceGuidance.hpp"

namespace triton
{

namespace {
constexpr byte kRateMultipliers[InterfaceGuidance::kRateCount] = { 1, 2, 3, 4, 6, 8, 12, 24 };
constexpr long kBaudBase = 4800;

// A sentence counts as present while its fix is younger than this. Matches
// InterfaceGps, and the 2 s InterfacePlough uses for its own staleness gate.
constexpr unsigned long kFreshMs   = 2000;
constexpr unsigned long kMinWaitMs = 2000;   // give every rate at least this long
constexpr unsigned long kMaxWaitMs = 10000;  // and no more than this, sentences or not
}  // namespace

long InterfaceGuidance::BaudFromIndex(byte index) {
    return kBaudBase * long(kRateMultipliers[index % kRateCount]);
}

InterfaceGuidance::InterfaceGuidance(InterfaceI2CLCD* lcd, SerialGuidanceChannel* channel, GuidanceSource* guidance)
    : lcd(lcd), channel(channel), guidance(guidance) {
}

byte InterfaceGuidance::DetectBaudrate(byte startIndex, byte required) {
    startIndex %= kRateCount;

    lcd->WriteBuffer("GPS: testing        ", 0);
    lcd->WriteBuffer("GGA string: --      ", 1);
    lcd->WriteBuffer("VTG string: --      ", 2);
    lcd->WriteBuffer("XTE string: --      ", 3);
    lcd->WriteScreen(0xFF);

    for (byte i = 0; i < kRateCount; i++) {
        const byte index = byte((startIndex + i) % kRateCount);
        const long baud  = BaudFromIndex(index);

        // Right-aligned rate on the top line, as InterfaceGps drew it
        long shown = baud;
        for (byte col = 19; col >= 14; col--) {
            lcd->WriteBuffer(char('0' + shown % 10), 0, col);
            shown /= 10;
        }

        if (testRate(baud, required) != 0) {
            lcd->WriteScreen(0xFF);
            return index;   // channel already listening at this rate
        }
    }

    lcd->WriteScreen(0xFF);
    channel->ApplyBaudrate(BaudFromIndex(startIndex));
    delay(1000);
    return kNotFound;
}

byte InterfaceGuidance::testRate(long baud, byte required) {
    channel->ApplyBaudrate(baud);

    const unsigned long start = millis();
    byte seen = 0;

    // Keep listening while the minimum has not passed or something is
    // arriving, until every required sentence showed up or the maximum ran
    // out. A rate that decodes nothing in two seconds is dropped at once.
    while ((millis() - start < kMinWaitMs || seen != 0)
           && millis() - start < kMaxWaitMs
           && seen != required) {
        channel->Update();

        const unsigned long now = millis();
        const bool gga = (now - guidance->GetGgaFixAge()) < kFreshMs && guidance->GetGgaFixAge() != 0;
        const bool vtg = (now - guidance->GetVtgFixAge()) < kFreshMs && guidance->GetVtgFixAge() != 0;
        const bool xte = (now - guidance->GetXteTimestamp()) < kFreshMs && guidance->GetXteTimestamp() != 0;

        seen = 0;
        if ((required & kGga) && gga) seen |= kGga;
        if ((required & kVtg) && vtg) seen |= kVtg;
        if ((required & kXte) && xte) seen |= kXte;

        if (required & kGga) showSentence(1, gga);
        if (required & kVtg) showSentence(2, vtg);
        if (required & kXte) showSentence(3, xte);
        lcd->WriteScreen(1);
    }

    lcd->WriteScreen(0xFF);
    return seen;
}

void InterfaceGuidance::showSentence(byte row, bool seen) {
    lcd->WriteBuffer(seen ? 'O' : '-', row, 12);
    lcd->WriteBuffer(seen ? 'K' : '-', row, 13);
}

}  // namespace triton
