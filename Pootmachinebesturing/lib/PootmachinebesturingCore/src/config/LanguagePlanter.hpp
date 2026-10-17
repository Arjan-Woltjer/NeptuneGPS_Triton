/*
  LanguagePlanter - LCD language string tables for pootmachinebesturing
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
#define S_DEVICE        "Plantercontrol v 1.4"
#define S_COPYRIGHT     "(c) 2011 - 2026 by J.A. Woltjer"
#define S_TIMES         "Times started: "
#define S_DIVIDE        "-------------------------------"

#define NEDERLANDS

#include "TritonText.hpp"

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L_DEVICE        " Pootm.best.  v 1.4 "

#define L_OFFSET        "Offset:             "

#define L_CAL_POS       "Breedte calibratie  "
#define L_CAL_POS_AD    "Verstel naar     cm "

#define L_CAL_XTE       "XTE calibratie      "
#define L_CAL_XTE_AD    "Verstel naar     cm "

#define L_CAL_OFFSET    "Wijzig offset       "
#define L_CAL_OFFSET_AD "Offset :          cm"

#define L_CAL_GPS_EN    "Enable GPS          "
#define L_CAL_GPS_EN_AD "Enabled:            "

#define L_CAL_SEN_EN    "Enable XTE Sensor   "
#define L_CAL_SEN_EN_AD "Enabled:            "

#define L_CAL_PWM_EN    "Enable PWM on valve "
#define L_CAL_PWM_EN_AD "Enabled:            "

#define L_CAL_1_0_EN    "Choose ON/OFF valve "
#define L_CAL_1_0_EN_AD "Selected:           "

#define L_CAL_INV_HY    "Inverteer hydrauliek"
#define L_CAL_INV_HY_AD "Inversie:           "

#define L_CAL_INV_HI    "Inverteer hefsignaal"
#define L_CAL_INV_HI_AD "Inversie:           "

#define L_CAL_INV_SE    "Inverteer pootsensor"
#define L_CAL_INV_SE_AD "Inversie:           "

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

#define L_DEVICE        " Plantercontrol 1.4 "

#define L_OFFSET        "Offset:             "

#define L_CAL_POS       "Width calibration   "
#define L_CAL_POS_AD    "Adjust to        cm "

#define L_CAL_XTE       "XTE calibration     "
#define L_CAL_XTE_AD    "Adjust to        cm "

#define L_CAL_OFFSET    "Adjust offset       "
#define L_CAL_OFFSET_AD "Offset :          cm"

#define L_CAL_GPS_EN    "Enable GPS          "
#define L_CAL_GPS_EN_AD "Enabled:            "

#define L_CAL_SEN_EN    "Enable XTE Sensor   "
#define L_CAL_SEN_EN_AD "Enabled:            "

#define L_CAL_PWM_EN    "Enable PWM on valve "
#define L_CAL_PWM_EN_AD "Enabled:            "

#define L_CAL_1_0_EN    "Choose ON/OFF valve "
#define L_CAL_1_0_EN_AD "Selected:           "

#define L_CAL_INV_HY    "Invert hydraulics   "
#define L_CAL_INV_HY_AD "Inverted:           "

#define L_CAL_INV_HI    "Invert hitch signal "
#define L_CAL_INV_HI_AD "Inverted:           "

#define L_CAL_INV_SE    "Invert planter sens."
#define L_CAL_INV_SE_AD "Inverted:           "

#define L_CAL_SPEED_AD  "Accelerate to 10kph "

#define L_CAL_PROG      "Selecteer programma "
#define L_CAL_PROG_AD2  "Ploegbesturing      "
#define L_CAL_PROG_AD3  "Pootmachinebesturing"
#define L_CAL_PROG_AD4  "Kieperbesturing     "
#define L_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L_CAL_PROG_AD6  "Spuitcomputer       "
#define L_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L_CAL_PROG_AD8  "Rooierbesturing     "

#define L_CAL_COMPLETE  "Finish calibration  "

#endif
