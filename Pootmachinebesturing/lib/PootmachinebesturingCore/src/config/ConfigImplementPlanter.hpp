/*
  ConfigImplementPlanter - config file for the MeijWorks planter implement library
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

#include "ConfigPlanter.hpp"

/*
_2 are defines for plough
_3 are defines for planter
_4 are defines for kipper
_5 are defines for scraper
_6 are defines for sprayer
_7 are defines for sower
*/

#define SHUTOFF_3             20000 // 3 seconds

#ifdef TEENSY

// Digital inputs 12V -> 5V conversion
#define PLANTINGELEMENT_PIN_3 2    // for hall position sensor (input 1 connector)
//#define xxx_3                 5    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_3          A14

// FET outputs (PWM)
#define OUTPUT_WIDE_3         20
#define OUTPUT_NARROW_3       21
#define OUTPUT_BYPASS_3       22
//#define xxx_3                 23

// Analog input
#define POSITION_SENS_PIN_3   A0    // for potmeter input (input 1 connector)
#define XTE_SENS_PIN_3        A1    // (input 2 connector)

#else

#ifdef TEENSYPROTO

#define PLANTINGELEMENT_PIN_3 24    // (input 1 connector)
//#define xxx_3                 25    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_3          13

// FET outputs (PWM)
#define OUTPUT_WIDE_3         30
#define OUTPUT_NARROW_3       31
#define OUTPUT_BYPASS_3       32
//#define xxx_3                 29

// Analog input
#define POSITION_SENS_PIN_3   A0    // for potmeter input (input 1 connector)
#define XTE_SENS_PIN_3          A1

#else

#ifdef MICRO
// Digital inputs 12V -> 5V conversion
#define PLANTINGELEMENT_PIN_3 A2    // for hall position sensor (input 1 connector)
//#define xxx_3                 A3    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_3          17

// FET outputs (PWM)
#define OUTPUT_WIDE_3         9
#define OUTPUT_NARROW_3       10
#define OUTPUT_BYPASS_3       11
//#define xxx_3                 12

// Analog input
#define POSITION_SENS_PIN_3   A0    // for potmeter input (input 1 connector)
#define XTE_SENS_PIN_3        A1    // (input 2 connector)

#else

#ifdef VOORSERIE
// Digital inputs 12V -> 5V conversion
#define PLANTINGELEMENT_PIN_3 10    // for hall position sensor (input 1 connector)
//#define xxx_3                 11    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED_3          13

// FET outputs (geen PWM)
#define OUTPUT_WIDE_3         A1
#define OUTPUT_NARROW_3       A2
#define OUTPUT_BYPASS_3       A0
//#define xxx_3                 12

// Analog input
#define POSITION_SENS_PIN_3   A3    // for potmeter input (input 1 connector)
#define XTE_SENS_PIN_3        A4    // (input 2 connector)

#else

#error "no board defined"

#endif
#endif
#endif
#endif
