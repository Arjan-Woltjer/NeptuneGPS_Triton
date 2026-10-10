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
#define OUTPUT_LED_2          33  // LED net (A14/DAC0 on 3.5/3.6); pin 13 is
                                   // JOY_RIGHT_2, an input, not this board's LED

// FET OUTPUTS
#define OUTPUT_NARROW_2       20
#define OUTPUT_WIDE_2         21
#define OUTPUT_BYPASS_2       22

// Analog input
#define POSITION_SENS_PIN_2   A0    // for potmeter input (input 1 connector)
#define ROTATION_SENS_PIN_2   A1    // (input 2 connector)

#else

#ifdef TEENSY40
// Teensy 4.0 board. Same pinout as the Teensy 4.1 production board above,
// except that CAN lives on FLEXCAN1 (pins 22 TX / 23 RX), which were the
// bypass / FET 4 outputs: those move to pins 2 and 3, and the plough-side
// input, which was on pin 2, moves to pin 4.

// Digital inputs 12V -> 5V conversion
#define PLOUGHSIDE_PIN_2      4     // was 2 on besturing 0.1; 2 is now OUTPUT_BYPASS_2

// Digital outputs
#define OUTPUT_LED_2          33    // bottom pad on the 4.0

// FET OUTPUTS (22/23 are CAN TX/RX on this board)
#define OUTPUT_NARROW_2       20
#define OUTPUT_WIDE_2         21
#define OUTPUT_BYPASS_2       2

// Analog input
#define POSITION_SENS_PIN_2   A0    // for potmeter input (input 1 connector)
#define ROTATION_SENS_PIN_2   A1    // (input 2 connector)

#else

#ifdef ESP32S3
// Triton01 (ESP32-S3-WROOM-2). Numbers are ESP32 GPIO numbers; inputs behind
// the MCP23008 I2C expander use EXPANDER_PIN(gp) from TritonIo.hpp and are
// read through triton::ReadDigital(). Every digital input on this board is an
// opto-coupler output with a pull-up (LOW when energised); TritonIo inverts
// that so the firmware keeps the "true = energised" of the besturing 0.1
// input stage.
#include "TritonIo.hpp"

// Digital inputs (DIN1-6 on the MCP23008, DIN7/8 on GPIO 47/48)
#define PLOUGHSIDE_PIN_2      EXPANDER_PIN(3)   // DIN4

// Valve outputs: one DRV8701 H-bridge per valve, PH = direction, EN = PWM.
// Narrow and wide are the two solenoids of one valve and mutually exclusive,
// so they share bridge 1; the bypass valve has bridge 2 to itself. Bridges 3
// and 4 (GPIO 15/16 and 17/18) are unassigned.
#define OUTPUT_VALVE_PH_2     11    // bridge 1 PH, see OUTPUT_NARROW_PH_LEVEL
#define OUTPUT_VALVE_EN_2     12    // bridge 1 EN, PWM
#define OUTPUT_BYPASS_PH_2    13    // bridge 2 PH
#define OUTPUT_BYPASS_EN_2    14    // bridge 2 EN, PWM
#define OUTPUT_NARROW_PH_LEVEL LOW  // PH level that fires the narrow solenoid, wide is the
                                    // opposite; swap here or swap the two leads on the connector
// No status LED on this board: OUTPUT_LED_2 is deliberately not defined.

// DRV8701 housekeeping, shared by all four bridges
#define MOTORDRIVER_SLEEP_PIN_2 38  // nSLEEP, HIGH = bridges enabled
#define MOTORDRIVER_FAULT_PIN_2 21  // nFAULT, open drain, pulled up on the board, LOW = fault
#define VALVE_CURRENT_PIN_2    7    // bridge 1 SO through a 1k/1k divider (ADC1)
#define BYPASS_CURRENT_PIN_2   8    // bridge 2 SO through a 1k/1k divider (ADC1)

// Analog input (ADC1). The board's buffers scale a 5 V sensor to 2.27 V,
// inside the ESP32's linear range; ImplementPlough selects 10-bit reads so
// the calibration tables keep the 0-1023 scale of the Teensy boards.
#define POSITION_SENS_PIN_2   5     // ANA1 (input 1 connector)
#define ROTATION_SENS_PIN_2   6     // ANA2 (input 2 connector)

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
#endif
#endif
