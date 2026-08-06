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

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L3_BLANK         "                    "

#define L3_MEIJWORKS     "     MeijWorks      "
#define L3_DEVICE        " Pootm.best.  v 1.4 "
#define L3_COPYRIGHT     "      (c) 2026      "
#define L3_AUTHOR        "  by J.A. Woltjer   "

#define L3_OFFSET        "Offset:             "
#define L3_A_POS         "Actuele positie:    "
#define L3_SETPOINT      "Setpoint:           "
#define L3_XTE           "XTE:                "

#define L3_CAL_ACCEPT    "+ : accepteren      "
#define L3_CAL_DECLINE   "- : annuleren       "

#define L3_CAL_DONE      "voltooid            "
#define L3_CAL_DECLINED  "geannuleerd         "

#define L3_CAL_ADJUST    "+ / - : verstellen  "
#define L3_CAL_ENTER     "Beide : accepteren  "

#define L3_CAL_ON        "                 aan"
#define L3_CAL_OFF       "                 uit"

#define L3_CAL_POS       "Breedte calibratie  "
#define L3_CAL_POS_AD    "Verstel naar     cm "

#define L3_CAL_XTE       "XTE calibratie      "
#define L3_CAL_XTE_AD    "Verstel naar     cm "

#define L3_CAL_KP        "PID wijzig KP       "
#define L3_CAL_KP_AD     "KP:                 "

#define L3_CAL_KI        "PID wijzig KI       "
#define L3_CAL_KI_AD     "KI:                 "

#define L3_CAL_KD        "PID wijzig KD       "
#define L3_CAL_KD_AD     "KD:                 "

#define L3_CAL_PWM_M     "Wijzig PWM handmatig"
#define L3_CAL_PWM_M_AD  "PWM handmatig:      "

#define L3_CAL_PWM_A     "Wijzig PWM automaat "
#define L3_CAL_PWM_A_AD  "PWM automaat:       "

#define L3_CAL_OFFSET    "Wijzig offset       "
#define L3_CAL_OFFSET_AD "Offset :          cm"

#define L3_CAL_QUAL      "Corrigeer RTK ident."
#define L3_CAL_QUAL_AD   "Quality:            "

#define L3_CAL_GPS_EN    "Enable GPS          "
#define L3_CAL_GPS_EN_AD "Enabled:            "

#define L3_CAL_SEN_EN    "Enable XTE Sensor   "
#define L3_CAL_SEN_EN_AD "Enabled:            "

#define L3_CAL_PWM_EN    "Enable PWM on valve "
#define L3_CAL_PWM_EN_AD "Enabled:            "

#define L3_CAL_1_0_EN    "Choose ON/OFF valve "
#define L3_CAL_1_0_EN_AD "Selected:           "

#define L3_CAL_INV_HY    "Inverteer hydrauliek"
#define L3_CAL_INV_HY_AD "Inversie:           "

#define L3_CAL_INV_HI    "Inverteer hefsignaal"
#define L3_CAL_INV_HI_AD "Inversie:           "

#define L3_CAL_INV_SE    "Inverteer pootsensor"
#define L3_CAL_INV_SE_AD "Inversie:           "

#define L3_CAL_SPEED     "Snelheids calibratie"
#define L3_CAL_SPEED_AD  "Accelereer tot 10kmh"

#define L3_CAL_PROG      "Selecteer programma "
#define L3_CAL_PROG_AD2  "Ploegbesturing      "
#define L3_CAL_PROG_AD3  "Pootmachinebesturing"
#define L3_CAL_PROG_AD4  "Kieperbesturing     "
#define L3_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L3_CAL_PROG_AD6  "Spuitcomputer       "
#define L3_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L3_CAL_PROG_AD8  "Rooierbesturing     "

#define L3_CAL_GPS       "GPS autodetect      "
#define L3_CAL_GPS_DONE  "geslaagd            "
#define L3_CAL_GPS_FAIL  "mislukt...          "
#define L3_CAL_GPS_M1    "Check kabels en     "
#define L3_CAL_GPS_M2    "nmea output         "

#define L3_CAL_COMPLETE  "Calibratie afronden "
#define L3_CAL_NOSAVE    "Data NIET opgeslagen"
#define L3_CAL_DDONE     "geslaagd            "
#define L3_CAL_SAVE      "Data is opgeslagen  "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

#define L3_BLANK         "                    "

#define L3_MEIJWORKS     "     MeijWorks      "
#define L3_DEVICE        " Plantercontrol 1.4 "
#define L3_COPYRIGHT     "      (c) 2026      "
#define L3_AUTHOR        "  by J.A. Woltjer   "

#define L3_OFFSET        "Offset:             "
#define L3_A_POS         "Actual position:    "
#define L3_SETPOINT      "Setpoint:           "
#define L3_XTE           "XTE:                "

#define L3_CAL_ACCEPT    "+ : accept          "
#define L3_CAL_DECLINE   "- : cancel          "

#define L3_CAL_DONE      "complete            "
#define L3_CAL_DECLINED  "cancelled           "

#define L3_CAL_ADJUST    "+ / - : to adjust   "
#define L3_CAL_ENTER     "Both  : to accept   "

#define L3_CAL_ON        "                  on"
#define L3_CAL_OFF       "                 off"

#define L3_CAL_POS       "Width calibration   "
#define L3_CAL_POS_AD    "Adjust to        cm "

#define L3_CAL_XTE       "XTE calibration     "
#define L3_CAL_XTE_AD    "Adjust to        cm "

#define L3_CAL_KP        "PID adjust KP       "
#define L3_CAL_KP_AD     "KP:                 "

#define L3_CAL_KI        "PID adjust KI       "
#define L3_CAL_KI_AD     "KI:                 "

#define L3_CAL_KD        "PID adjust KD       "
#define L3_CAL_KD_AD     "KD:                 "

#define L3_CAL_PWM_M     "PWM adjust manual   "
#define L3_CAL_PWM_M_AD  "PWM manual:         "

#define L3_CAL_PWM_A     "PWM adjust auto     "
#define L3_CAL_PWM_A_AD  "PWM auto:           "

#define L3_CAL_OFFSET    "Adjust offset       "
#define L3_CAL_OFFSET_AD "Offset :          cm"

#define L3_CAL_QUAL      "Correct RTK ident.  "
#define L3_CAL_QUAL_AD   "Quality:            "

#define L3_CAL_GPS_EN    "Enable GPS          "
#define L3_CAL_GPS_EN_AD "Enabled:            "

#define L3_CAL_SEN_EN    "Enable XTE Sensor   "
#define L3_CAL_SEN_EN_AD "Enabled:            "

#define L3_CAL_PWM_EN    "Enable PWM on valve "
#define L3_CAL_PWM_EN_AD "Enabled:            "

#define L3_CAL_1_0_EN    "Choose ON/OFF valve "
#define L3_CAL_1_0_EN_AD "Selected:           "

#define L3_CAL_INV_HY    "Invert hydraulics   "
#define L3_CAL_INV_HY_AD "Inverted:           "

#define L3_CAL_INV_HI    "Invert hitch signal "
#define L3_CAL_INV_HI_AD "Inverted:           "

#define L3_CAL_INV_SE    "Invert planter sens."
#define L3_CAL_INV_SE_AD "Inverted:           "

#define L3_CAL_SPEED     "Speed calibration   "
#define L3_CAL_SPEED_AD  "Accelerate to 10kph "

#define L3_CAL_PROG      "Selecteer programma "
#define L3_CAL_PROG_AD2  "Ploegbesturing      "
#define L3_CAL_PROG_AD3  "Pootmachinebesturing"
#define L3_CAL_PROG_AD4  "Kieperbesturing     "
#define L3_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L3_CAL_PROG_AD6  "Spuitcomputer       "
#define L3_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L3_CAL_PROG_AD8  "Rooierbesturing     "

#define L3_CAL_GPS       "GPS autodetect      "
#define L3_CAL_GPS_DONE  "passed              "
#define L3_CAL_GPS_FAIL  "failed...           "
#define L3_CAL_GPS_M1    "Check cabling and   "
#define L3_CAL_GPS_M2    "nmea output         "

#define L3_CAL_COMPLETE  "Finish calibration  "
#define L3_CAL_NOSAVE    "Data NOT saved      "
#define L3_CAL_DDONE     "done                "
#define L3_CAL_SAVE      "Data saved          "

#endif
