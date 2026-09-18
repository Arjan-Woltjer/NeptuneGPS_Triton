/*
  ConfigImplementRooier - config file for the MeijWorks windrower implement library
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

#include "ConfigRooier.hpp"

/*
_2 are defines for plough
_3 are defines for planter
_4 are defines for kipper
_5 are defines for scraper
_6 are defines for sprayer
_7 are defines for sower
_8 are defines for windrower
*/

#define SHUTOFF_8             20000 // 20 seconds

#ifdef TEENSY

// Digital inputs 12V -> 5V conversion
//#define xxx_8                 2    // for hall position sensor (input 1 connector)
//#define xxx_8                 5    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_8          A14

// FET outputs (PWM)
#define OUTPUT_UP_L_8         20
#define OUTPUT_DOWN_L_8       21
#define OUTPUT_UP_R_8         22
#define OUTPUT_DOWN_R_8       23

// Analog input
#define HEIGHT_SENS_PIN_L_8   A0
#define HEIGHT_SENS_PIN_R_8   A1

#else

#ifdef TEENSYPROTO

// Digital inputs 12V -> 5V conversion
//#define xxx_8                 24    // for hall position sensor (input 1 connector)
//#define xxx_8                 25    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_8          13

// FET outputs (PWM)
#define OUTPUT_UP_L_8         29
#define OUTPUT_DOWN_L_8       30
#define OUTPUT_UP_R_8         31
#define OUTPUT_DOWN_R_8       32

// Analog input
#define HEIGHT_SENS_PIN_L_8   A0
#define HEIGHT_SENS_PIN_R_8   A1

#else

#ifdef MICRO
// Digital inputs 12V -> 5V conversion
//#define xxx_8                 A2    // for hall position sensor (input 1 connector)
//#define xxx_8                 A3    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_8          17

// FET outputs (PWM)
#define OUTPUT_UP_L_8         9
#define OUTPUT_DOWN_L_8       10
#define OUTPUT_UP_R_8         11
#define OUTPUT_DOWN_R_8       12

// Analog input
#define HEIGHT_SENS_PIN_L_8   A0
#define HEIGHT_SENS_PIN_R_8   A1

#else

#ifdef VOORSERIE
// Digital inputs 12V -> 5V conversion
//#define xxx_8                 10    // for hall position sensor (input 1 connector)
//#define xxx_8                 11    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_8          13

// FET outputs (PWM)
#define OUTPUT_UP_L_8         A0
#define OUTPUT_DOWN_L_8       A1
#define OUTPUT_UP_R_8         A2
#define OUTPUT_DOWN_R_8       12

// Analog input
#define HEIGHT_SENS_PIN_L_8   A3
#define HEIGHT_SENS_PIN_R_8   A4

#else

#error "no board defined"

#endif
#endif
#endif
#endif
