/*
  ConfigImplementPlough - config file for the MeijWorks plough implement library
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

#include "ConfigPlough.hpp"

/*
_2 are defines for plough
_3 are defines for planter
_4 are defines for kipper
_5 are defines for scraper
_6 are defines for sprayer
_7 are defines for sower
*/

#define SHUTOFF_2             3000 // 3 seconds

#ifdef TEENSY

// Digital inputs 12V -> 5V conversion
#define PLOUGHSIDE_PIN_2      2
//#define xxx_2                 5

// Digital outputs
#define OUTPUT_LED_2          13

// FET OUTPUTS
#define OUTPUT_NARROW_2       20
#define OUTPUT_WIDE_2         21
#define OUTPUT_BYPASS_2       22
//#define xxx_2                 23

// Analog input
#define POSITION_SENS_PIN_2   A0    // for potmeter input (input 1 connector)
#define ROTATION_SENS_PIN_2   A1    // (input 2 connector)

#else

#ifdef TEENSYPROTO
// Digital inputs 12V -> 5V conversion
#define PLOUGHSIDE_PIN_2      24
//#define xxx_2                 25

// Digital outputs
#define OUTPUT_LED_2          13

// FET OUTPUTS
#define OUTPUT_NARROW_2       30
#define OUTPUT_WIDE_2         31
#define OUTPUT_BYPASS_2       32
//#define xxx_2                 29

// Analog input
#define POSITION_SENS_PIN_2   A0    // for potmeter input (input 1 connector)
#define ROTATION_SENS_PIN_2   A1    // (input 2 connector)

#else

#ifdef MICRO
// Digital inputs 12V -> 5V conversion
#define PLOUGHSIDE_PIN_2      A2    // for hall position sensor (input 1 connector)
//#define xxx_2               A3    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_2          17

// FET outputs (PWM)
#define OUTPUT_NARROW_2       9
#define OUTPUT_WIDE_2         10
#define OUTPUT_BYPASS_2       11
//#define xxx_2                 12

// Analog input
#define POSITION_SENS_PIN_2   A0    // for potmeter input (input 1 connector)
#define ROTATION_SENS_PIN_2   A1    // (input 2 connector)

#else

#ifdef VOORSERIE

#define PLOUGHSIDE_PIN_2      10    // for hall position sensor (input 1 connector)
//#define xxx_2                 11    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_2          13

// FET outputs (not PWM)
#define OUTPUT_NARROW_2       A2
#define OUTPUT_WIDE_2         A1
#define OUTPUT_BYPASS_2       A0
//#define xxx_2                 A5

// Analog input
#define POSITION_SENS_PIN_2   A3    // for potmeter input (input 1 connector)
//#define ROTATION_SENS_PIN_2   A4    // (input 2 connector) (NOT ON UNO!!)

#else

#error "no board defined"

#endif
#endif
#endif
#endif
