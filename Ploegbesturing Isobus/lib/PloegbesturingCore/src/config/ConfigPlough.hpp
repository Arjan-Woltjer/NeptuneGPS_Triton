/*
  ConfigPlough - config file for ploegbesturing
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

#define PLOUGH

// Defines for serial ports
#define SERIALDATARATE    115200    // Serial USB

// Defines for boardtype. TEENSY40 is selected from platformio.ini
// ([env:teensy40_isobus] passes -D TEENSY40) rather than edited in here, so
// one tree builds both Teensy boards. Without any board flag the Teensy 4.1
// production board (besturing 0.1) is the default, exactly as before.
#if !defined(TEENSY40) && !defined(TEENSYPROTO) && !defined(MICRO) && !defined(VOORSERIE)
#define TEENSY      //Teensy 4.1 on productionboard (besturing 0.1)
#endif
//#define TEENSY40    // Teensy 4.0 board: CAN on FLEXCAN1 (22/23), see ConfigImplementPlough.hpp
//#define TEENSYPROTO // Teensy 3.5 or 3.6 on protoboard
//#define MICRO       // Arduino Micro
//#define VOORSERIE   // Arduino Uno pre-series board

// ISOBUS CAN peripheral per board, as an AgIsoStack FlexCANT4Plugin channel
// number: 0 = FLEXCAN1 (pins 22 TX / 23 RX), 1 = FLEXCAN2 (1 / 0),
// 2 = FLEXCAN3 (31 / 30). Consumed by main.cpp.
#ifdef TEENSY40
#define ISOBUS_CAN_CHANNEL  0   // FLEXCAN1: pins 22 TX / 23 RX
#else
#define ISOBUS_CAN_CHANNEL  2   // FLEXCAN3: pins 31 TX / 30 RX, bodge-wired on besturing 0.1
#endif

//#define ROTATION
//#define PWM_MAN
//#define PWM_AUTO
//#define PID_KP
//#define SPEED_L
//#define DEBUG
