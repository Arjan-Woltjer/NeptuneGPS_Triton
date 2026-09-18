/*
  ConfigImplementKipper - config file for the MeijWorks steering-axle implement library
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

#include "ConfigKipper.hpp"

/*
_2 are defines for plough
_3 are defines for planter
_4 are defines for kipper
_5 are defines for scraper
_6 are defines for sprayer
_7 are defines for sower
_8 are defines for windrower
*/

#define SHUTOFF_4             3000 // 3 seconds

#ifdef TEENSY

// Digital outputs
#define OUTPUT_LED_4          A14

// FET outputs (PWM)
#define OUTPUT_WIDE_4         20
#define OUTPUT_NARROW_4       21
#define OUTPUT_BYPASS_4       22
//#define xxx_4                 23

// Analog input
#define ANGLE_SENS_PIN_4      A0    // hitch/drawbar angle sensor (input 1 connector)
#define STEER_SENS_PIN_4      A1    // steering-axle angle feedback (input 2 connector)

#else

#ifdef VOORSERIE

// Digital outputs
#define OUTPUT_LED_4          13

// FET outputs (no PWM)
#define OUTPUT_WIDE_4         A1
#define OUTPUT_NARROW_4       A2
#define OUTPUT_BYPASS_4       A0
//#define xxx_4                 12

// Analog input
#define ANGLE_SENS_PIN_4      A3
#define STEER_SENS_PIN_4      A4

#else

#error "no board defined"

#endif
#endif
