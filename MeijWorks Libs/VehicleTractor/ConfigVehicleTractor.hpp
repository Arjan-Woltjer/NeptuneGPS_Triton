/*
  ConfigVehicleTractor - config file for the VehicleTractor library
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

// Board selection. Guarded so a project-level build flag (-D TEENSY40, the
// Ploegbesturing Isobus Teensy 4.0 env) is honoured no matter which config
// header a translation unit includes first; without any flag the Teensy 4.1
// production board applies, exactly as before.
#if !defined(TEENSY40) && !defined(TEENSYPROTO) && !defined(MICRO) && !defined(VOORSERIE)
#define TEENSY
#endif
//#define TEENSY40
//#define TEENSYPROTO
//#define MICRO
//#define VOORSERIE
//#define DEBUG

#define MINSPEED_1            0.5f

#ifdef TEENSY
// Digital inputs 12V -> 5V conversion
#define WHEEL_SPEED_PIN_1     6
#define HITCH_PIN_1           7

#else

#ifdef TEENSY40
// Teensy 4.0 board: identical to the Teensy 4.1 production board.
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