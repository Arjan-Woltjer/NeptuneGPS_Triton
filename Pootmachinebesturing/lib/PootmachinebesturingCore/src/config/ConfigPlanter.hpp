/*
  ConfigPlanter - config file for pootmachinebesturing
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

#define PLANTER

// Defines for serial ports
#define SERIALDATARATE    115200    // Serial USB

// Defines for boardtype
#define TEENSY      //Teensy on productionboard
//#define TEENSYPROTO // Teensy 3.5 or 3.6 on protoboard
//#define MICRO       // Arduino Micro
//#define VOORSERIE   // Arduino Uno pre-series board

// When defined, CalibrationPlanter skips its XTE-sensor calibration step --
// for a future GPS-only variant board with no XTE potentiometer wired up.
// Never defined today; kept for future revival, matching how ConfigPlough.hpp
// keeps ROTATION/PWM_MAN/etc. as never-defined guards.
//#define GPS
//#define DEBUG
