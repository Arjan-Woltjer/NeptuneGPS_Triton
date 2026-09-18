/*
  ConfigInterfacePlanter - config file for the MeijWorks plantercontrol interface library
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
_8 are defines for windrower
*/

#ifdef TEENSY
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN_3          8
#define LEFT_BUTTON_3       9
#define RIGHT_BUTTON_3      10
#define JOY_MODE_3          11
#define JOY_LEFT_3          12
#define JOY_RIGHT_3         13

#else

#ifdef TEENSYPROTO
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN_3          5
#define LEFT_BUTTON_3       6
#define RIGHT_BUTTON_3      7
#define JOY_MODE_3          8
#define JOY_LEFT_3          9
#define JOY_RIGHT_3         10

#else

#error "no board defined"

#endif
#endif
