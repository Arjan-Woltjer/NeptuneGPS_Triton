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

#include <EEPROM.h>

namespace triton
{

// Reads and writes typed values in EEPROM at an address the caller passes in.
// It holds no state and owns no addresses: where each value lives is up to
// the caller and the EEPROM memory map. Byte layouts:
//
//   Uint8     one byte
//   Int16Le   int16, low byte first
//   Uint16Be  uint16, high byte first
//   Float     IEEE 754 single, low byte first
//   Fill      sets a range of bytes to one value (e.g. wiping a block)
//
// Changing a block's byte order changes what is already stored on the
// device, so do it only together with that block's layout-version check.
// Header-only, so it builds for AVR (Arduino Micro), Teensy and ESP32
// without adding a source file to any build.
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
                                     static_cast<uint16_t>(EEPROM.read(addr + 1)));
    }

    static void WriteUint16Be(uint16_t addr, uint16_t value) {
        EEPROM.write(addr, static_cast<uint8_t>(value >> 8));
        EEPROM.write(addr + 1, static_cast<uint8_t>(value & 0xFF));
    }

    static float ReadFloat(uint16_t addr) {
        union {
            float value;
            uint8_t bytes[sizeof(float)];
        } u;

        for (uint8_t i = 0; i < sizeof(u.bytes); i++) {
            u.bytes[i] = EEPROM.read(addr + i);
        }
        return u.value;
    }

    static void WriteFloat(uint16_t addr, float value) {
        union {
            float value;
            uint8_t bytes[sizeof(float)];
        } u;

        u.value = value;
        for (uint8_t i = 0; i < sizeof(u.bytes); i++) {
            EEPROM.write(addr + i, u.bytes[i]);
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
