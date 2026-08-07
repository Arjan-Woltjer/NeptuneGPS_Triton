/*
  ConfigImplementScraper - config file for the MeijWorks scraper implement library
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

#include "ConfigScraper.hpp"

/*
_2 are defines for plough
_3 are defines for planter
_4 are defines for kipper
_5 are defines for scraper
_6 are defines for sprayer
_7 are defines for sower
*/

#define SHUTOFF_5             3000 // 3 seconds

#ifdef TEENSYPROTO

// Digital outputs
#define OUTPUT_LED_5          13

// FET outputs (PWM)
#define OUTPUT_WIDE_5         30
#define OUTPUT_NARROW_5       31
#define OUTPUT_BYPASS_5       32
//#define xxx_5                 29

// Analog input
#define POSITION_SENS_PIN_5   A0    // for potmeter input (input 1 connector) -- unused, NOSENS

#else

#ifdef TEENSY

// Digital outputs
#define OUTPUT_LED_5          A14

// FET outputs (PWM)
#define OUTPUT_WIDE_5         20
#define OUTPUT_NARROW_5       21
#define OUTPUT_BYPASS_5       22
//#define xxx_5                 23

// Analog input
#define POSITION_SENS_PIN_5   A0    // for potmeter input (input 1 connector) -- unused, NOSENS

#else

#ifdef MICRO

// Digital outputs
#define OUTPUT_LED_5          17

// FET outputs (PWM)
#define OUTPUT_WIDE_5         9
#define OUTPUT_NARROW_5       10
#define OUTPUT_BYPASS_5       11
//#define xxx_5               12

// Analog input
#define POSITION_SENS_PIN_5   A0    // for potmeter input (input 1 connector) -- unused, NOSENS

#else

#ifdef VOORSERIE

// Digital outputs
#define OUTPUT_LED_5          13

// FET outputs (PWM)
#define OUTPUT_WIDE_5         A1
#define OUTPUT_NARROW_5       A2
#define OUTPUT_BYPASS_5       A3
//#define xxx_5                 12

// Analog input
#define POSITION_SENS_PIN_5   A0    // for potmeter input (input 1 connector) -- unused, NOSENS

#else

#error "no board defined"

#endif
#endif
#endif
#endif
