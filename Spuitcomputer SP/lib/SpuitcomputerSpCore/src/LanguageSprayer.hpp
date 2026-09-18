/*
  LanguageSprayer - LCD language string tables for the Spuitcomputer SP sprayer controller
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
#define S_DEVICE        "Sprayercontrol v 1.4"
#define S_COPYRIGHT     "(c) 2011 - 2026 by J.A. Woltjer"
#define S_TIMES         "Times started: "
#define S_DIVIDE        "-------------------------------"

#define NEDERLANDS

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L_BLANK         "                    "

#define L_MEIJWORKS     "     MeijWorks      "
#define L_DEVICE        "Sprayercontrol  v1.4"
#define L_COPYRIGHT     "      (c) 2026      "
#define L_AUTHOR        "  by J.A. Woltjer   "

#define L_FLOW          "Afgifte:        L/Ha"
#define L_MEASURE       "Gemeten:        L/Ha"
#define L_SPEED         "Snelheid:       KM/h"
#define L_SETPOINT      "Setpoint:           "

#define L_CAL_ACCEPT    "+ : accepteren      "
#define L_CAL_DECLINE   "- : annuleren       "

#define L_CAL_DONE      "voltooid            "
#define L_CAL_DECLINED  "geannuleerd         "

#define L_CAL_ADJUST    "+ / - : verstellen  "
#define L_CAL_ENTER     "Beide : accepteren  "

#define L_CAL_ON        "                 aan"
#define L_CAL_OFF       "                 uit"

#define L_MEN_SIM       "Simulatie menu      "
#define L_MEN_SPEED     "Snelheidscalibratie "
#define L_MEN_FLOW      "Flowcalibratie      "
#define L_MEN_SYSTEM    "Systeeminstellingen "
#define L_MEN_PID       "Instellingen PID    "

#define L_CAL_SIM       "Wijzig sim mode     "

#define L_CAL_SIMS      "Wijzig sim snelheid "
#define L_CAL_SIMS_AD   "Huidig:          kmh"

#define L_CAL_SIMT      "Wijzig sim tijd     "
#define L_CAL_SIMT_AD   "Huidig:          sec"

#define L_CAL_SPEED     "Snelheids calibratie"
#define L_CAL_SPEED_AD  "Pulsen:             "

#define L_CAL_SPEED_100 "100m uitgemeten     "
#define L_CAL_SPEED_MAN "Opgeven aant. pulsen"

#define L_CAL_TEETH     "Wijzig aantal tanden"
#define L_CAL_TEETH_AD  "Huidig aantal:      "

#define L_CAL_PUMPS     "Wijzig aantal pompen"
#define L_CAL_PUMPS_AD  "Huidig aantal:      "

#define L_CAL_WIDTH     "Wijzig werkbreedte  "
#define L_CAL_WIDTH_AD  "Huidige breedte:    "

#define L_CAL_FLOW      "Flow calibratie     "
#define L_CAL_FLOW_AD   "Huidig:       puls/L"

#define L_CAL_PWM       "PWM calibratie      "
#define L_CAL_PWM_AD    "Volume/omw:       cc"

#define L_CAL_KP        "PID wijzig KP       "
#define L_CAL_KP_AD     "KP:                 "

#define L_CAL_KI        "PID wijzig KI       "
#define L_CAL_KI_AD     "KI:                 "

#define L_CAL_KD        "PID wijzig KD       "
#define L_CAL_KD_AD     "KD:                 "

#define L_CAL_QUAL      "Corrigeer RTK ident."
#define L_CAL_QUAL_AD   "Quality:            "

#define L_CAL_GPS       "GPS autodetect      "
#define L_CAL_GPS_DONE  "geslaagd            "
#define L_CAL_GPS_FAIL  "mislukt...          "
#define L_CAL_GPS_M1    "Check kabels en     "
#define L_CAL_GPS_M2    "nmea output         "

#define L_CAL_COMPLETE  "Calibratie opslaan  "
#define L_CAL_NOSAVE    "Data NIET opgeslagen"
#define L_CAL_DDONE     "geslaagd            "
#define L_CAL_SAVE      "Data is opgeslagen  "

#define L_CAL_EXIT      "Menu verlaten       "
#define L_CAL_BYE       "BYE                 "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy source's ENGLISH block was copy-pasted from an unrelated
// steering/angle-sensor module (L_CAL_STEER/L_ANGLE/L_A_STEER -- nothing to
// do with a sprayer) and never actually translated. Never compiled in
// practice (NEDERLANDS is always the one #define'd above), but
// CalibrationSprayer references these sprayer-specific strings
// unconditionally, so it would have failed to compile at all had ENGLISH
// ever been selected. Replaced with a real translation of the NEDERLANDS
// block above, matching how LanguagePlanter.hpp's equivalent gap was filled.

#define L_BLANK         "                    "

#define L_MEIJWORKS     "     MeijWorks      "
#define L_DEVICE        "Sprayercontrol  v1.4"
#define L_COPYRIGHT     "      (c) 2026      "
#define L_AUTHOR        "  by J.A. Woltjer   "

#define L_FLOW          "Output:         L/Ha"
#define L_MEASURE       "Measured:       L/Ha"
#define L_SPEED         "Speed:          KM/h"
#define L_SETPOINT      "Setpoint:           "

#define L_CAL_ACCEPT    "+ : accept          "
#define L_CAL_DECLINE   "- : cancel          "

#define L_CAL_DONE      "complete            "
#define L_CAL_DECLINED  "cancelled           "

#define L_CAL_ADJUST    "+ / - : to adjust   "
#define L_CAL_ENTER     "Both  : to accept   "

#define L_CAL_ON        "                  on"
#define L_CAL_OFF       "                 off"

#define L_MEN_SIM       "Simulation menu     "
#define L_MEN_SPEED     "Speed calibration   "
#define L_MEN_FLOW      "Flow calibration    "
#define L_MEN_SYSTEM    "System settings     "
#define L_MEN_PID       "PID settings        "

#define L_CAL_SIM       "Adjust sim mode     "

#define L_CAL_SIMS      "Adjust sim speed    "
#define L_CAL_SIMS_AD   "Current:         kmh"

#define L_CAL_SIMT      "Adjust sim time     "
#define L_CAL_SIMT_AD   "Current:         sec"

#define L_CAL_SPEED     "Speed calibration   "
#define L_CAL_SPEED_AD  "Pulses:             "

#define L_CAL_SPEED_100 "100m measured out   "
#define L_CAL_SPEED_MAN "Enter pulse count   "

#define L_CAL_TEETH     "Adjust amt. of teeth"
#define L_CAL_TEETH_AD  "Current amount:     "

#define L_CAL_PUMPS     "Adjust amt. of pumps"
#define L_CAL_PUMPS_AD  "Current amount:     "

#define L_CAL_WIDTH     "Adjust work width   "
#define L_CAL_WIDTH_AD  "Current width:      "

#define L_CAL_FLOW      "Flow calibration    "
#define L_CAL_FLOW_AD   "Current:      puls/L"

#define L_CAL_PWM       "PWM calibration     "
#define L_CAL_PWM_AD    "Volume/rev:       cc"

#define L_CAL_KP        "PID adjust KP       "
#define L_CAL_KP_AD     "KP:                 "

#define L_CAL_KI        "PID adjust KI       "
#define L_CAL_KI_AD     "KI:                 "

#define L_CAL_KD        "PID adjust KD       "
#define L_CAL_KD_AD     "KD:                 "

#define L_CAL_QUAL      "Correct RTK ident.  "
#define L_CAL_QUAL_AD   "Quality:            "

#define L_CAL_GPS       "GPS autodetect      "
#define L_CAL_GPS_DONE  "passed              "
#define L_CAL_GPS_FAIL  "failed...           "
#define L_CAL_GPS_M1    "Check cabling and   "
#define L_CAL_GPS_M2    "nmea output         "

#define L_CAL_COMPLETE  "Save calibration    "
#define L_CAL_NOSAVE    "Data NOT saved      "
#define L_CAL_DDONE     "done                "
#define L_CAL_SAVE      "Data saved          "

#define L_CAL_EXIT      "Exit menu           "
#define L_CAL_BYE       "BYE                 "

#endif
