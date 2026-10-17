/*
  LanguageKipper - LCD language string tables for kipperbesturing
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

#define NEDERLANDS

#include "TritonText.hpp"

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L_DEVICE        " Kipperbest.  v 1.00"

#define L_OFFSET        "Offset:             "
#define L_A_STEER       "Stand wielen:       "
#define L_ANGLE         "Hoek:               "

#define L_CAL_STEER     "Stuur calibratie    "
#define L_CAL_STEER_AD  "Verstel naar     grd"

#define L_CAL_ANGLE     "Hoek calibratie     "
#define L_CAL_ANGLE_AD  "Verstel naar     grd"

#define L_CAL_OFFSET    "Wijzig offset       "
#define L_CAL_OFFSET_AD "Offset :         grd"

#define L_CAL_COMPLETE  "Calibratie afronden "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy ENGLISH block's L_DEVICE said "Plantercontrol 1.00" -- a
// copy-paste from Pootmachinebesturing's own language file, never updated
// for Kipper (every other string in this block was correctly Kipper-
// specific). Fixed here to match this module's own identity.

#define L_DEVICE        " Kipper control 1.00"

#define L_OFFSET        "Offset:             "
#define L_A_STEER       "Actual position:    "
#define L_ANGLE         "Angle:              "

#define L_CAL_STEER     "Steer calibration   "
#define L_CAL_STEER_AD  "Adjust to        deg"

#define L_CAL_ANGLE     "Angle calibration   "
#define L_CAL_ANGLE_AD  "Adjust to        deg"

#define L_CAL_OFFSET    "Adjust offset       "
#define L_CAL_OFFSET_AD "Offset :         deg"

#define L_CAL_COMPLETE  "Finish calibration  "

#endif
