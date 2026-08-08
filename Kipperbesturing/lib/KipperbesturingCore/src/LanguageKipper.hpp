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

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L4_BLANK         "                    "

#define L4_MEIJWORKS     "     MeijWorks      "
#define L4_DEVICE        " Kipperbest.  v 1.00"
#define L4_COPYRIGHT     "      (c) 2026      "
#define L4_AUTHOR        "  by J.A. Woltjer   "

#define L4_OFFSET        "Offset:             "
#define L4_A_STEER       "Stand wielen:       "
#define L4_SETPOINT      "Setpoint:           "
#define L4_ANGLE         "Hoek:               "

#define L4_CAL_ACCEPT    "+ : accepteren      "
#define L4_CAL_DECLINE   "- : annuleren       "

#define L4_CAL_DONE      "voltooid            "
#define L4_CAL_DECLINED  "geannuleerd         "

#define L4_CAL_ADJUST    "+ / - : verstellen  "
#define L4_CAL_ENTER     "Beide : accepteren  "

#define L4_CAL_STEER     "Stuur calibratie    "
#define L4_CAL_STEER_AD  "Verstel naar     grd"

#define L4_CAL_ANGLE     "Hoek calibratie     "
#define L4_CAL_ANGLE_AD  "Verstel naar     grd"

#define L4_CAL_KP        "PID wijzig KP       "
#define L4_CAL_KP_AD     "KP:                 "

#define L4_CAL_KI        "PID wijzig KI       "
#define L4_CAL_KI_AD     "KI:                 "

#define L4_CAL_KD        "PID wijzig KD       "
#define L4_CAL_KD_AD     "KD:                 "

#define L4_CAL_OFFSET    "Wijzig offset       "
#define L4_CAL_OFFSET_AD "Offset :         grd"

#define L4_CAL_COMPLETE  "Calibratie afronden "
#define L4_CAL_NOSAVE    "Data NIET opgeslagen"
#define L4_CAL_DDONE     "geslaagd            "
#define L4_CAL_SAVE      "Data is opgeslagen  "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy ENGLISH block's L_DEVICE said "Plantercontrol 1.00" -- a
// copy-paste from Pootmachinebesturing's own language file, never updated
// for Kipper (every other string in this block was correctly Kipper-
// specific). Fixed here to match this module's own identity.

#define L4_BLANK         "                    "

#define L4_MEIJWORKS     "     MeijWorks      "
#define L4_DEVICE        " Kipper control 1.00"
#define L4_COPYRIGHT     "      (c) 2026      "
#define L4_AUTHOR        "  by J.A. Woltjer   "

#define L4_OFFSET        "Offset:             "
#define L4_A_STEER       "Actual position:    "
#define L4_SETPOINT      "Setpoint:           "
#define L4_ANGLE         "Angle:              "

#define L4_CAL_ACCEPT    "+ : accept          "
#define L4_CAL_DECLINE   "- : cancel          "

#define L4_CAL_DONE      "complete            "
#define L4_CAL_DECLINED  "cancelled           "

#define L4_CAL_ADJUST    "+ / - : to adjust   "
#define L4_CAL_ENTER     "Both  : to accept   "

#define L4_CAL_STEER     "Steer calibration   "
#define L4_CAL_STEER_AD  "Adjust to        deg"

#define L4_CAL_ANGLE     "Angle calibration   "
#define L4_CAL_ANGLE_AD  "Adjust to        deg"

#define L4_CAL_KP        "PID adjust KP       "
#define L4_CAL_KP_AD     "KP:                 "

#define L4_CAL_KI        "PID adjust KI       "
#define L4_CAL_KI_AD     "KI:                 "

#define L4_CAL_KD        "PID adjust KD       "
#define L4_CAL_KD_AD     "KD:                 "

#define L4_CAL_OFFSET    "Adjust offset       "
#define L4_CAL_OFFSET_AD "Offset :         deg"

#define L4_CAL_COMPLETE  "Finish calibration  "
#define L4_CAL_NOSAVE    "Data NOT saved      "
#define L4_CAL_DDONE     "done                "
#define L4_CAL_SAVE      "Data saved          "

#endif
