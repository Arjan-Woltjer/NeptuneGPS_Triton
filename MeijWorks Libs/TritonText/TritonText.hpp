/*
  TritonText - the LCD texts every Triton machine shares (20 characters each)
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

// One copy of the 36 texts that were defined identically, Dutch and English,
// in every project's Language*.hpp that used them (NeptuneGPS_Triton#177): the
// calibration prompts (accept, cancel, adjust, done, saved ...), the splash
// screen lines, the GPS autodetect and PID/PWM screens. Each project's language
// file defines NEDERLANDS or ENGLISH and then includes this; its own texts stay
// there. Every line is exactly 20 characters, the LCD's width.
#if defined NEDERLANDS
#define L_AUTHOR         "  by J.A. Woltjer   "
#define L_A_POS          "Actuele positie:    "
#define L_BLANK          "                    "
#define L_CAL_ACCEPT     "+ : accepteren      "
#define L_CAL_ADJUST     "+ / - : verstellen  "
#define L_CAL_DDONE      "geslaagd            "
#define L_CAL_DECLINE    "- : annuleren       "
#define L_CAL_DECLINED   "geannuleerd         "
#define L_CAL_DONE       "voltooid            "
#define L_CAL_ENTER      "Beide : accepteren  "
#define L_CAL_GPS        "GPS autodetect      "
#define L_CAL_GPS_DONE   "geslaagd            "
#define L_CAL_GPS_FAIL   "mislukt...          "
#define L_CAL_GPS_M1     "Check kabels en     "
#define L_CAL_GPS_M2     "nmea output         "
#define L_CAL_KD         "PID wijzig KD       "
#define L_CAL_KD_AD      "KD:                 "
#define L_CAL_KI         "PID wijzig KI       "
#define L_CAL_KI_AD      "KI:                 "
#define L_CAL_KP         "PID wijzig KP       "
#define L_CAL_KP_AD      "KP:                 "
#define L_CAL_NOSAVE     "Data NIET opgeslagen"
#define L_CAL_OFF        "                 uit"
#define L_CAL_ON         "                 aan"
#define L_CAL_PWM_A      "Wijzig PWM automaat "
#define L_CAL_PWM_A_AD   "PWM automaat:       "
#define L_CAL_PWM_M      "Wijzig PWM handmatig"
#define L_CAL_PWM_M_AD   "PWM handmatig:      "
#define L_CAL_QUAL       "Corrigeer RTK ident."
#define L_CAL_QUAL_AD    "Quality:            "
#define L_CAL_SAVE       "Data is opgeslagen  "
#define L_CAL_SPEED      "Snelheids calibratie"
#define L_COPYRIGHT      "      (c) 2026      "
#define L_MEIJWORKS      "     MeijWorks      "
#define L_SETPOINT       "Setpoint:           "
#define L_XTE            "XTE:                "

#elif defined ENGLISH
#define L_AUTHOR         "  by J.A. Woltjer   "
#define L_A_POS          "Actual position:    "
#define L_BLANK          "                    "
#define L_CAL_ACCEPT     "+ : accept          "
#define L_CAL_ADJUST     "+ / - : to adjust   "
#define L_CAL_DDONE      "done                "
#define L_CAL_DECLINE    "- : cancel          "
#define L_CAL_DECLINED   "cancelled           "
#define L_CAL_DONE       "complete            "
#define L_CAL_ENTER      "Both  : to accept   "
#define L_CAL_GPS        "GPS autodetect      "
#define L_CAL_GPS_DONE   "passed              "
#define L_CAL_GPS_FAIL   "failed...           "
#define L_CAL_GPS_M1     "Check cabling and   "
#define L_CAL_GPS_M2     "nmea output         "
#define L_CAL_KD         "PID adjust KD       "
#define L_CAL_KD_AD      "KD:                 "
#define L_CAL_KI         "PID adjust KI       "
#define L_CAL_KI_AD      "KI:                 "
#define L_CAL_KP         "PID adjust KP       "
#define L_CAL_KP_AD      "KP:                 "
#define L_CAL_NOSAVE     "Data NOT saved      "
#define L_CAL_OFF        "                 off"
#define L_CAL_ON         "                  on"
#define L_CAL_PWM_A      "PWM adjust auto     "
#define L_CAL_PWM_A_AD   "PWM auto:           "
#define L_CAL_PWM_M      "PWM adjust manual   "
#define L_CAL_PWM_M_AD   "PWM manual:         "
#define L_CAL_QUAL       "Correct RTK ident.  "
#define L_CAL_QUAL_AD    "Quality:            "
#define L_CAL_SAVE       "Data saved          "
#define L_CAL_SPEED      "Speed calibration   "
#define L_COPYRIGHT      "      (c) 2026      "
#define L_MEIJWORKS      "     MeijWorks      "
#define L_SETPOINT       "Setpoint:           "
#define L_XTE            "XTE:                "

#else
#error "Define NEDERLANDS or ENGLISH before including TritonText.hpp"
#endif
