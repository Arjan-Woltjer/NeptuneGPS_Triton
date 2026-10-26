/*
  ConfigVehicleSensors - config file for the VehicleSensors library
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

// Board type. Every board but the default comes from a build flag in the
// project's platformio.ini (-D TEENSY40, -D TEENSY32, -D ESP32S3 or -D MICRO),
// so it is the same in every header whichever one a file includes first.
// Without a board flag the Teensy 4.1 production board is the default.
#if !defined(TEENSY40) && !defined(TEENSY32) && !defined(ESP32S3) && !defined(TEENSYPROTO) && !defined(MICRO) && !defined(VOORSERIE)
#define TEENSY      // Teensy 4.1 on the production board (besturing 0.1)
#endif
//#define TEENSY40    // Teensy 4.0 board
//#define TEENSY32    // Teensy 3.2 board
//#define ESP32S3     // Triton01: ESP32-S3, inputs on the MCP23008
//#define TEENSYPROTO // Teensy 3.5 or 3.6 on protoboard
//#define MICRO       // Arduino Micro
//#define VOORSERIE   // Arduino Uno pre-series board
//#define DEBUG       // Boot prints in VehicleSensors.cpp

// MinSpeed() is true above this speed, in the VehicleSensors speed scale (one
// unit is 36 km/h, see VehicleSensors.hpp).
#define MINSPEED_1            0.5f

#ifdef TEENSY
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     6
#define HITCH_PIN_1           7

#else

#ifdef TEENSY40
// Teensy 4.0 board: the same pins as the Teensy 4.1 board.
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     6
#define HITCH_PIN_1           7

#else

#ifdef ESP32S3
// Triton01: the wheel-speed pulse on a direct GPIO (DIN8), the hitch behind
// the MCP23008 (DIN5); EXPANDER_PIN() and the reads come from TritonIo.hpp.
#include "TritonIo.hpp"
#define WHEEL_SPEED_PIN_1     48                // DIN8
#define HITCH_PIN_1           EXPANDER_PIN(4)   // DIN5

#else

#ifdef TEENSY32
// Teensy 3.2 board: the same pins as the Teensy 4.1 board.
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     6
#define HITCH_PIN_1           7

#else

#ifdef TEENSYPROTO
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     26
#define HITCH_PIN_1           27

#else

#ifdef MICRO
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     A4
#define HITCH_PIN_1           A5

#else

#ifdef VOORSERIE
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     8
#define HITCH_PIN_1           9

#else

#error "no board defined"

#endif
#endif
#endif
#endif
#endif
#endif
#endif