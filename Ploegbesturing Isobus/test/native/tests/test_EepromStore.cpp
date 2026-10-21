/*
  test_EepromStore - the shared EEPROM encode/decode (MeijWorks Libs/EepromStore)
  writes exactly the bytes the per-project copies it replaces wrote
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
#include "EepromStore.hpp"

using namespace aunit;
using namespace triton;

namespace {

// The code EepromStore replaces, verbatim apart from the names: readInt()/
// writeInt() as six implements had them, and the inline word()/highByte()
// pairs. The tests below hold the new code to these byte for byte.
short int LegacyReadInt(uint16_t addr) {
    union {
        byte b[2];
        short int i;
    } data;
    for (short int i = 0; i < 2; i++) {
        data.b[i] = EEPROM.read(addr + i);
    }
    return data.i;
}

void LegacyWriteInt(short int x, uint16_t addr) {
    union {
        byte b[2];
        short int i;
    } data;
    data.i = x;
    for (short int i = 0; i < 2; i++) {
        EEPROM.write(addr + i, data.b[i]);
    }
}

void LegacyWriteWord(uint16_t addr, short int x) {
    EEPROM.write(addr, highByte(x));
    EEPROM.write(addr + 1, lowByte(x));
}

short int LegacyReadWord(uint16_t addr) {
    return word(EEPROM.read(addr), EEPROM.read(addr + 1));
}

float LegacyReadFloat(uint16_t addr) {
    union {
        byte b[4];
        float f;
    } data;
    for (int i = 0; i < 4; i++) {
        data.b[i] = EEPROM.read(addr + i);
    }
    return data.f;
}

}  // namespace

// The byte layouts, spelled out.
test(EepromStore, int16Le_isLowByteFirst) {
    EEPROM.eepromReset();
    EepromStore::WriteInt16Le(66, 160);
    assertEqual((int)EEPROM.read(66), 160);
    assertEqual((int)EEPROM.read(67), 0);
    EepromStore::WriteInt16Le(66, -2);
    assertEqual((int)EEPROM.read(66), 0xFE);
    assertEqual((int)EEPROM.read(67), 0xFF);
    assertEqual((int)EepromStore::ReadInt16Le(66), -2);
}

test(EepromStore, uint16Be_isHighByteFirst) {
    EEPROM.eepromReset();
    EepromStore::WriteUint16Be(40, 700);
    assertEqual((int)EEPROM.read(40), 0x02);
    assertEqual((int)EEPROM.read(41), 0xBC);
    assertEqual((int)EepromStore::ReadUint16Be(40), 700);
}

test(EepromStore, float_isIeeeLowByteFirst) {
    EEPROM.eepromReset();
    EepromStore::WriteFloat(130, 3.5f);   // 0x40600000
    assertEqual((int)EEPROM.read(130), 0x00);
    assertEqual((int)EEPROM.read(131), 0x00);
    assertEqual((int)EEPROM.read(132), 0x60);
    assertEqual((int)EEPROM.read(133), 0x40);
    assertNear(EepromStore::ReadFloat(130), 3.5f, 0.0f);
    assertNear(LegacyReadFloat(130), 3.5f, 0.0f);
}

// Every 16-bit value: written by the old code, read by the new, and the
// other way round -- so a board keeps every stored value across the update.
test(EepromStore, int16Le_matchesTheOldReadIntWriteIntForEveryValue) {
    for (int32_t v = -32768; v <= 32767; v++) {
        LegacyWriteInt(static_cast<short int>(v), 10);
        if (EepromStore::ReadInt16Le(10) != v) { assertEqual((long)EepromStore::ReadInt16Le(10), (long)v); return; }
        EepromStore::WriteInt16Le(10, static_cast<int16_t>(v));
        if (LegacyReadInt(10) != v) { assertEqual((long)LegacyReadInt(10), (long)v); return; }
    }
}

test(EepromStore, uint16Be_matchesTheOldWordPairsForEveryValue) {
    for (int32_t v = -32768; v <= 32767; v++) {
        const short int x = static_cast<short int>(v);
        LegacyWriteWord(20, x);
        const uint8_t hi = EEPROM.read(20), lo = EEPROM.read(21);
        EepromStore::WriteUint16Be(20, static_cast<uint16_t>(x));
        if (EEPROM.read(20) != hi || EEPROM.read(21) != lo) { assertEqual((long)v, -99999L); return; }
        if (static_cast<short int>(EepromStore::ReadUint16Be(20)) != LegacyReadWord(20)) {
            assertEqual((long)v, -99999L);
            return;
        }
    }
}

test(EepromStore, fill_coversFromUpToButNotIncludingTo) {
    EEPROM.eepromReset();
    for (int a = 0; a < 256; a++) EEPROM.write(a, 7);
    EepromStore::Fill(1, 255, 255);   // what wipeCalibrationData() did
    assertEqual((int)EEPROM.read(0), 7);
    assertEqual((int)EEPROM.read(1), 255);
    assertEqual((int)EEPROM.read(254), 255);
    assertEqual((int)EEPROM.read(255), 7);
    EEPROM.eepromReset();
}
