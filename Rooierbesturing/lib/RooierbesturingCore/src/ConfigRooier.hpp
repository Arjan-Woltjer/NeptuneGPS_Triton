/*
  ConfigRooier - config file for rooierbesturing
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

#define ROOIER

// Defines for serial ports
#define SERIALDATARATE    115200    // Serial USB

// Defines for boardtype
#define TEENSY      //Teensy on productionboard
//#define TEENSYPROTO // Teensy 3.5 or 3.6 on protoboard
//#define MICRO       // Arduino Micro
//#define VOORSERIE   // Arduino Uno/Leonardo pre-series board

// The legacy source's ERROR/TIME_PER_CM/FALLTIME macros belonged to a
// settle-time-based adjust scheme (adjustL()/adjustR() in the abandoned
// ImplementRooier.cpp fragment) that's been replaced with the shutoff-latch
// bang-bang scheme every other Triton implement already uses (see
// ImplementRooier::Adjust()) -- the error margin is now the persisted,
// calibration-wizard-adjustable `error` member instead of a compile-time
// constant, and SHUTOFF_8 (ConfigImplementRooier.hpp) is the only timing
// constant the new scheme needs.

//#define DEBUG
