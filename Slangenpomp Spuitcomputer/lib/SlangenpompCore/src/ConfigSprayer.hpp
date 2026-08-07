/*
  ConfigSprayer - config file for the Slangenpomp sprayer controller
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

// The legacy sketch (Spuitcomputer_v1_40.ino) `#include`d a "config.h" that
// wasn't included in the source drop -- SERIALDATARATE and the board-select
// defines below are synthesized fresh here, mirroring ConfigPlanter.hpp's
// role for Pootmachinebesturing (a real per-module config header, not a
// missing dependency).

#define SPRAYER

// Defines for serial ports
#define SERIALDATARATE    115200    // Serial USB

// Defines for boardtype
#define TEENSY      //Teensy on productionboard
//#define TEENSYPROTO // Teensy 3.5 or 3.6 on protoboard
//#define VOORSERIE   // Arduino Micro pre-series board

//#define DEBUG
