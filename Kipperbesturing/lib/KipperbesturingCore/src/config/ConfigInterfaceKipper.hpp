/*
  ConfigInterfaceKipper - config file for the MeijWorks kippercontrol interface library
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

// This module's legacy source never had joystick inputs (JOY_MODE/JOY_LEFT/
// JOY_RIGHT), unlike most siblings -- only a mode switch and two buttons.
// Not invented here; CheckButtons() below only ever reads the pins declared.

#ifdef TEENSY
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN_4          8
#define LEFT_BUTTON_4       9
#define RIGHT_BUTTON_4      10

#else

#ifdef VOORSERIE
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN_4          6
#define LEFT_BUTTON_4       5
#define RIGHT_BUTTON_4      4

#else

#error "no board defined"

#endif
#endif
