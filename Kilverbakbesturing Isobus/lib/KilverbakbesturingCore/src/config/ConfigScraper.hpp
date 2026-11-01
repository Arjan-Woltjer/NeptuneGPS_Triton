/*
  ConfigScraper - config file for kilverbakbesturing
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

#define SCRAPER

// Defines for serial ports
#define SERIALDATARATE    115200    // Serial USB

// Defines for boardtype. As in Ploegbesturing Isobus, every board but the
// default is selected from platformio.ini, so one tree builds them all:
// [env:teensy40] and [env:teensy40_isobus] pass -D TEENSY40, the board the
// kilverbak units run (the Ploegbesturing Teensy 4.0 board, owner
// 2026-11-01). Without a board flag the Teensy 4.1 production board is the
// default, exactly as in Kilverbakbesturing.
#if !defined(TEENSY40) && !defined(TEENSYPROTO) && !defined(MICRO) && !defined(VOORSERIE)
#define TEENSY      //Teensy 4.1 on productionboard
#endif
//#define TEENSY40    // Teensy 4.0 board: CAN on FLEXCAN1 (22/23), see ConfigImplementScraper.hpp
//#define TEENSYPROTO // Teensy 3.5 or 3.6 on protoboard
//#define MICRO       // Arduino Micro
//#define VOORSERIE   // Arduino Uno/Leonardo pre-series board

// CAN peripheral per board, as an AgIsoStack FlexCANT4Plugin / ACAN_T4
// channel number: 0 = FLEXCAN1 (pins 22 TX / 23 RX), 2 = FLEXCAN3 (31 / 30).
// The same choice as ConfigPlough.hpp: the Teensy 4.0 board has its
// transceiver on FLEXCAN1, the Teensy 4.1 production board (besturing 0.1)
// on FLEXCAN3. Kilverbakbesturing reads FLEXCAN1 on every board.
#ifdef TEENSY40
#define ISOBUS_CAN_CHANNEL  0   // FLEXCAN1: pins 22 TX / 23 RX
#else
#define ISOBUS_CAN_CHANNEL  2   // FLEXCAN3: pins 31 TX / 30 RX, bodge-wired on besturing 0.1
#endif

// Defines for output and PID type
#define NOSENS      // No position sensor on this scraper -- blind on/off control
//#define PID_KP
//#define PWM_MAN
//#define PWM_AUTO

//#define DEBUG
