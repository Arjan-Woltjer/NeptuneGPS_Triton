/*
  ConfigInterfaceSprayer - config file for the MeijWorks sprayer interface library
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
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN          5
#define LEFT_BUTTON        6
#define RIGHT_BUTTON       7
//#define xxx              8
//#define xxx              9
//#define xxx              10

#else

#ifdef TEENSY
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN          8
#define LEFT_BUTTON        9
#define RIGHT_BUTTON       10
//#define xxx              11
//#define xxx              12
//#define xxx              13

#else

#ifdef MICRO
// Defines for io ports
// Digital debounced inputs
#define MODE_PIN          4
#define LEFT_BUTTON        5
#define RIGHT_BUTTON       6
//#define xxx              7
//#define xxx              8
//#define xxx              13

#else

#ifdef VOORSERIE
#define MODE_PIN          6
#define LEFT_BUTTON        5
#define RIGHT_BUTTON       4

#else

#error "no board defined"

#endif
#endif
#endif
#endif
