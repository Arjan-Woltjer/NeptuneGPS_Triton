/*
  LanguagePlough - LCD language string tables for ploegbesturing
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

// ---------------
// Serial messages
// ---------------
#define S_MEIJWORKS     "MeijWorks"
#define S_DEVICE        "Ploughcontrol v 1.31"
#define S_COPYRIGHT     "(c) 2011 - 2026 by J.A. Woltjer"
#define S_TIMES         "Times started: "
#define S_DIVIDE        "-------------------------------"

#define NEDERLANDS

#include "TritonText.hpp"

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L_DEVICE        "Ploegbesturing v1.31"

#define L_POS           "Ploegbreedte:       "
#define L_ROTATION      "Rotatie:            "

#define L_CAL_YES       "                  ja"
#define L_CAL_NO        "                 nee"

#define L_CAL_POS       "Breedte calibratie  "
#define L_CAL_POS_AD    "Verstel naar     cm "

#define L_CAL_ROTATION  "Rotatie calibratie  "
#define L_CAL_ROTATION_AD "Verstel naar     deg"

#define L_CAL_SHARES    "Wijzig aant. scharen"
#define L_CAL_SHARES_AD "Aantal scharen:     "

#define L_CAL_DEADBAND    "Wijzig dode band    "
#define L_CAL_DEADBAND_AD "Dode band:        cm"

#define L_CAL_MAXCOR    "Wijzig corr./schaar "
#define L_CAL_MAXCOR_AD "Corr./schaar:     mm"

#define L_CAL_SWAP      "Wijzig ploegzijde   "
#define L_CAL_SWAP_AD   "Ploegt naar:        "

#define L_CAL_BYPASS    "Bypassventiel       "

#define L_CAL_INVERSION    "Inverteer hefsignaal"
#define L_CAL_INVERSION_AD "Inversie:           "

#define L_CAL_JD        "Hefschakelaar       "
#define L_CAL_JD_AD     "                    "
#define L_CAL_SPEED_AD  "Accelereer tot 10kmh"

#define L_CAL_PROG      "Selecteer programma "
#define L_CAL_PROG_AD2  "Ploegbesturing      "
#define L_CAL_PROG_AD3  "Pootmachinebesturing"
#define L_CAL_PROG_AD4  "Kieperbesturing     "
#define L_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L_CAL_PROG_AD6  "Spuitcomputer       "
#define L_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L_CAL_PROG_AD8  "Rooierbesturing     "

#define L_CAL_COMPLETE  "Calibratie afronden "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

#define L_DEVICE        " Ploughcontrol v1.31"

#define L_POS           "Set width:          "
#define L_ROTATION      "Rotation:           "

#define L_CAL_YES       "                 yes"
#define L_CAL_NO        "                  no"

#define L_CAL_POS       "Width calibration   "
#define L_CAL_POS_AD    "Adjust to        cm "

#define L_CAL_ROTATION  "Rotation calibration"
#define L_CAL_ROTATION_AD "Adjust to        deg"

#define L_CAL_SHARES    "Adjust am. of shares"
#define L_CAL_SHARES_AD "Amount of shares:   "

#define L_CAL_DEADBAND    "Adjust deadband     "
#define L_CAL_DEADBAND_AD "Deadband :        cm"

#define L_CAL_MAXCOR    "Adj. max corr./share"
#define L_CAL_MAXCOR_AD "Corr./share:      mm"

#define L_CAL_SWAP      "Change ploughside   "
#define L_CAL_SWAP_AD   "Side:               "

#define L_CAL_BYPASS    "Bypass valve        "

#define L_CAL_INVERSION    "Invert hitch signal "
#define L_CAL_INVERSION_AD "Inversion:          "

#define L_CAL_SPEED_AD  "Accelerate to 10kph "

#define L_CAL_COMPLETE  "Finish calibration  "

#endif
