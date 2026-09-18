/*
  ConfigImplementSprayer - config file for the MeijWorks sprayer implement library
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

#include "ConfigSprayer.hpp"

#ifdef TEENSYPROTO

#define FLOW_SENS_PIN       24    // (input 1 connector)
#define GEAR_SENS_PIN       25    // (input 2 connector)

// Digital outputs
#define OUTPUT_LED          13

// FET outputs (PWM)
#define OUTPUT_FET          29
#define OUTPUT_FET2         30
//#define xxx               31
//#define xxx               32

// Analog input
#define IMPLEMENT_SWITCH    A1

#else

#ifdef TEENSY

#define FLOW_SENS_PIN       2     // (input 1 connector)
#define GEAR_SENS_PIN       5     // (input 2 connector)

// Digital outputs
#define OUTPUT_LED          A14

// FET outputs (PWM)
#define OUTPUT_FET          20
#define OUTPUT_FET2         21
//#define xxx               22
//#define xxx               23

// Analog input
#define IMPLEMENT_SWITCH    A1

#else

#ifdef MICRO

// Digital inputs 12V -> 5V conversion
#define GEAR_SENS_PIN       A2    // (input 1 connector)
#define FLOW_SENS_PIN       A3    // (input 2 connector)
// A4 speed
// A5 hitch

// Digital outputs
#define OUTPUT_LED          17

// FET outputs (PWM)
#define OUTPUT_FET          9
#define OUTPUT_FET2         10
//#define xxx               11
//#define xxx               12

// Analog input
#define IMPLEMENT_SWITCH    A1    // for potmeter input (input 2 connector)

#else

#ifdef VOORSERIE

// Digital inputs 12V -> 5V conversion
#define GEAR_SENS_PIN       10    // (input 1 connector)
#define FLOW_SENS_PIN       12    // (input 2 connector)

// FET outputs (PWM)
#define OUTPUT_FET          11
#define OUTPUT_LED          13
#define ALARM               A0

#define IMPLEMENT_SWITCH    8

#else

#error "no board defined"

#endif
#endif
#endif
#endif
