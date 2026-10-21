/*
  EepromStore - typed encode/decode of the values the Triton projects keep in
  EEPROM, at the address the caller passes in
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
#include <string.h>

#include <EEPROM.h>

namespace triton
{

// One copy of the byte layouts every implement used to write out by hand
// (NeptuneGPS_Triton#177): readInt/writeInt in six projects, readFloat/
// writeFloat, wipeCalibrationData in three, and 22 inline word()/highByte()
// pairs. It owns no addresses -- those stay with the caller and the EEPROM
// memory map (#78, #178) -- and it keeps every byte where it already is:
//
//   Int16Le   int16, low byte first: what readInt()/writeInt() stored
//   Uint16Be  uint16, high byte first: what word(EEPROM.read(a),
//             EEPROM.read(a + 1)) read and highByte()/lowByte() wrote
//   Float     IEEE 754 single, low byte first: what readFloat()/writeFloat()
//             stored (all Triton targets are little-endian)
//
// The map's decision to use little-endian everywhere is a separate step: a
// block's byte order changes together with its layout-version check, never
// before it (#177's constraints). Header-only, so it builds for AVR (the
// Arduino Micro) as well as the Teensy and ESP32 boards, with no source file
// to add to any build.
struct EepromStore {
    static uint8_t ReadUint8(uint16_t addr) {
        return EEPROM.read(addr);
    }

    static void WriteUint8(uint16_t addr, uint8_t value) {
        EEPROM.write(addr, value);
    }

    static int16_t ReadInt16Le(uint16_t addr) {
        return static_cast<int16_t>(static_cast<uint16_t>(EEPROM.read(addr)) |
                                    static_cast<uint16_t>(EEPROM.read(addr + 1)) << 8);
    }

    static void WriteInt16Le(uint16_t addr, int16_t value) {
        const uint16_t bits = static_cast<uint16_t>(value);
        EEPROM.write(addr, static_cast<uint8_t>(bits & 0xFF));
        EEPROM.write(addr + 1, static_cast<uint8_t>(bits >> 8));
    }

    static uint16_t ReadUint16Be(uint16_t addr) {
        return static_cast<uint16_t>(static_cast<uint16_t>(EEPROM.read(addr)) << 8 |
                                     EEPROM.read(addr + 1));
    }

    static void WriteUint16Be(uint16_t addr, uint16_t value) {
        EEPROM.write(addr, static_cast<uint8_t>(value >> 8));
        EEPROM.write(addr + 1, static_cast<uint8_t>(value & 0xFF));
    }

    static float ReadFloat(uint16_t addr) {
        uint8_t bytes[sizeof(float)];
        for (uint8_t i = 0; i < sizeof(float); i++) {
            bytes[i] = EEPROM.read(addr + i);
        }
        float value;
        memcpy(&value, bytes, sizeof(float));
        return value;
    }

    static void WriteFloat(uint16_t addr, float value) {
        uint8_t bytes[sizeof(float)];
        memcpy(bytes, &value, sizeof(float));
        for (uint8_t i = 0; i < sizeof(float); i++) {
            EEPROM.write(addr + i, bytes[i]);
        }
    }

    // Every byte from `from` up to, not including, `to` set to `value`.
    static void Fill(uint16_t from, uint16_t to, uint8_t value) {
        for (uint16_t addr = from; addr < to; addr++) {
            EEPROM.write(addr, value);
        }
    }
};

}  // namespace triton
