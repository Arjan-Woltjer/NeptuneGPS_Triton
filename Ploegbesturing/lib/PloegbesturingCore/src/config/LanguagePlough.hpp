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

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L2_BLANK         "                    "

#define L2_MEIJWORKS     "     MeijWorks      "
#define L2_DEVICE        "Ploegbesturing v1.31"
#define L2_COPYRIGHT     "      (c) 2026      "
#define L2_AUTHOR        "  by J.A. Woltjer   "

#define L2_POS           "Ploegbreedte:       "
#define L2_A_POS         "Actuele positie:    "
#define L2_XTE           "XTE:                "
#define L2_ROTATION      "Rotatie:            "

#define L2_CAL_ACCEPT    "+ : accepteren      "
#define L2_CAL_DECLINE   "- : annuleren       "

#define L2_CAL_DONE      "voltooid            "
#define L2_CAL_DECLINED  "geannuleerd         "

#define L2_CAL_ADJUST    "+ / - : verstellen  "
#define L2_CAL_ENTER     "Beide : accepteren  "

#define L2_CAL_ON        "                 aan"
#define L2_CAL_OFF       "                 uit"

#define L2_CAL_POS       "Breedte calibratie  "
#define L2_CAL_POS_AD    "Verstel naar     cm "

#define L2_CAL_ROTATION  "Rotatie calibratie  "
#define L2_CAL_ROTATION_AD "Verstel naar     deg"

#define L2_CAL_SHARES    "Wijzig aant. scharen"
#define L2_CAL_SHARES_AD "Aantal scharen:     "

#define L2_CAL_KP        "PID wijzig KP       "
#define L2_CAL_KP_AD     "KP:                 "

#define L2_CAL_PWM_M     "Wijzig PWM handmatig"
#define L2_CAL_PWM_M_AD  "PWM handmatig:      "

#define L2_CAL_PWM_A     "Wijzig PWM automaat "
#define L2_CAL_PWM_A_AD  "PWM automaat:       "

#define L2_CAL_MARGIN    "Wijzig foutmarge    "
#define L2_CAL_MARGIN_AD "Foutmarge :       cm"

#define L2_CAL_MAXCOR    "Wijzig max correctie"
#define L2_CAL_MAXCOR_AD "Max. corr.:       cm"

#define L2_CAL_SWAP      "Wijzig ploegzijde   "
#define L2_CAL_SWAP_AD   "Ploegt naar:        "

#define L2_CAL_QUAL      "Corrigeer RTK ident."
#define L2_CAL_QUAL_AD   "Quality:            "

#define L2_CAL_DEUTZ     "Inverteer hefsignaal"
#define L2_CAL_DEUTZ_AD  "Inversie:           "

#define L2_CAL_JD        "Hefschakelaar       "
#define L2_CAL_JD_AD     "                    "
#define L2_CAL_SPEED     "Snelheids calibratie"
#define L2_CAL_SPEED_AD  "Accelereer tot 10kmh"

#define L2_CAL_PROG      "Selecteer programma "
#define L2_CAL_PROG_AD2  "Ploegbesturing      "
#define L2_CAL_PROG_AD3  "Pootmachinebesturing"
#define L2_CAL_PROG_AD4  "Kieperbesturing     "
#define L2_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L2_CAL_PROG_AD6  "Spuitcomputer       "
#define L2_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L2_CAL_PROG_AD8  "Rooierbesturing     "

#define L2_CAL_GPS       "GPS autodetect      "
#define L2_CAL_GPS_DONE  "geslaagd            "
#define L2_CAL_GPS_FAIL  "mislukt...          "
#define L2_CAL_GPS_M1    "Check kabels en     "
#define L2_CAL_GPS_M2    "nmea output         "

#define L2_CAL_COMPLETE  "Calibratie afronden "
#define L2_CAL_NOSAVE    "Data NIET opgeslagen"
#define L2_CAL_DDONE     "geslaagd            "
#define L2_CAL_SAVE      "Data is opgeslagen  "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

#define L2_BLANK         "                    "

#define L2_MEIJWORKS     "     MeijWorks      "
#define L2_DEVICE        " Ploughcontrol v1.31"
#define L2_COPYRIGHT     "      (c) 2026      "
#define L2_AUTHOR        "  by J.A. Woltjer   "

#define L2_POS           "Set width:          "
#define L2_A_POS         "Actual position:    "
#define L2_XTE           "XTE:                "
#define L2_ROTATION      "Rotation:           "

#define L2_CAL_ACCEPT    "+ : accept          "
#define L2_CAL_DECLINE   "- : cancel          "

#define L2_CAL_DONE      "complete            "
#define L2_CAL_DECLINED  "cancelled           "

#define L2_CAL_ADJUST    "+ / - : to adjust   "
#define L2_CAL_ENTER     "Both  : to accept   "

#define L2_CAL_ON        "                  on"
#define L2_CAL_OFF       "                 off"

#define L2_CAL_POS       "Width calibration   "
#define L2_CAL_POS_AD    "Adjust to        cm "

#define L2_CAL_ROTATION  "Rotation calibration"
#define L2_CAL_ROTATION_AD "Adjust to        deg"

#define L2_CAL_SHARES    "Adjust am. of shares"
#define L2_CAL_SHARES_AD "Amount of shares:   "

#define L2_CAL_KP        "PID adjust KP       "
#define L2_CAL_KP_AD     "KP:                 "

#define L2_CAL_PWM_M     "PWM adjust manual   "
#define L2_CAL_PWM_M_AD  "PWM manual:         "

#define L2_CAL_PWM_A     "PWM adjust auto     "
#define L2_CAL_PWM_A_AD  "PWM auto:           "

#define L2_CAL_MARGIN    "Adjust error margin "
#define L2_CAL_MARGIN_AD "Margin :          cm"

#define L2_CAL_MAXCOR    "Adj. max. correction"
#define L2_CAL_MAXCOR_AD "Max. corr.:       cm"

#define L2_CAL_SWAP      "Change ploughside   "
#define L2_CAL_SWAP_AD   "Side:               "

#define L2_CAL_QUAL      "Correct RTK ident.  "
#define L2_CAL_QUAL_AD   "Quality:            "

#define L2_CAL_SPEED     "Speed calibration   "
#define L2_CAL_SPEED_AD  "Accelerate to 10kph "

#define L2_CAL_GPS       "GPS autodetect      "
#define L2_CAL_GPS_DONE  "passed              "
#define L2_CAL_GPS_FAIL  "failed...           "
#define L2_CAL_GPS_M1    "Check cabling and   "
#define L2_CAL_GPS_M2    "nmea output         "

#define L2_CAL_COMPLETE  "Finish calibration  "
#define L2_CAL_NOSAVE    "Data NOT saved      "
#define L2_CAL_DDONE     "done                "
#define L2_CAL_SAVE      "Data saved          "

#endif
