/*
  LanguageScraper - LCD language string tables for kilverbakbesturing
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

#define L5_BLANK         "                    "

#define L5_MEIJWORKS     "     MeijWorks      "
#define L5_DEVICE        "Kilverbesturing v1.1"
#define L5_COPYRIGHT     "      (c) 2026      "
#define L5_AUTHOR        "  by J.A. Woltjer   "

#define L5_HEIGHT_R      "Ref:               m"
#define L5_HEIGHT        "Actueel:           m"
#define L5_DISTANCE      "Afstand:           m"
#define L5_SLOPE         "Afschot:    %       "

#define L5_CAL_ACCEPT    "+ : accepteren      "
#define L5_CAL_DECLINE   "- : annuleren       "

#define L5_CAL_DONE      "voltooid            "
#define L5_CAL_DECLINED  "geannuleerd         "

#define L5_CAL_ADJUST    "+ / - : verstellen  "
#define L5_CAL_ENTER     "Beide : accepteren  "

#define L5_CAL_ON        "                 aan"
#define L5_CAL_OFF       "                 uit"

#define L5_CHOICE        "Keuzemenu           "
#define L5_REF           "+ : referentie inst."
#define L5_CAL           "- : calibreren      "

#define L5_CAL_REF       "Stel referentie A in"
#define L5_CAL_REF_AD    "Referentie opgesl.  "

#define L5_CAL_SLOPE     "Wijzig helling      "
#define L5_CAL_SLOPE_AD  "In cm/100m:       cm"

#define L5_CAL_POS       "Hoogte calibratie   "
#define L5_CAL_POS_AD    "Verstel naar     cm "

#define L5_CAL_KP        "PID wijzig KP       "
#define L5_CAL_KP_AD     "KP:                 "

#define L5_CAL_PWM_M     "Wijzig PWM handmatig"
#define L5_CAL_PWM_M_AD  "PWM handmatig:      "

#define L5_CAL_PWM_A     "Wijzig PWM automaat "
#define L5_CAL_PWM_A_AD  "PWM automaat:       "

#define L5_CAL_MARGIN    "Wijzig foutmarge    "
#define L5_CAL_MARGIN_AD "Foutmarge :       cm"

#define L5_CAL_MAXCOR    "Wijzig max correctie"
#define L5_CAL_MAXCOR_AD "Max. corr.:       cm"

#define L5_CAL_QUAL      "Corrigeer RTK ident."
#define L5_CAL_QUAL_AD   "Quality:            "

#define L5_CAL_DEUTZ     "Inverteer hefsignaal"
#define L5_CAL_DEUTZ_AD  "Inversie:           "

#define L5_CAL_SPEED     "Snelheids calibratie"
#define L5_CAL_SPEED_AD  "Accelereer tot 10kmh"

#define L5_CAL_PROG      "Selecteer programma "
#define L5_CAL_PROG_AD2  "Ploegbesturing      "
#define L5_CAL_PROG_AD3  "Pootmachinebesturing"
#define L5_CAL_PROG_AD4  "Kieperbesturing     "
#define L5_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L5_CAL_PROG_AD6  "Spuitcomputer       "
#define L5_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L5_CAL_PROG_AD8  "Rooierbesturing     "

#define L5_CAL_GPS       "GPS autodetect      "
#define L5_CAL_GPS_DONE  "geslaagd            "
#define L5_CAL_GPS_FAIL  "mislukt...          "
#define L5_CAL_GPS_M1    "Check kabels en     "
#define L5_CAL_GPS_M2    "nmea output         "

#define L5_CAL_COMPLETE  "Calibratie afronden "
#define L5_CAL_NOSAVE    "Data NIET opgeslagen"
#define L5_CAL_DDONE     "geslaagd            "
#define L5_CAL_SAVE      "Data is opgeslagen  "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy source's ENGLISH block described a different screen layout
// entirely (L5_POS/L5_A_POS/L5_XTE/L5_ROTATION -- a position/XTE main
// screen) instead of translating the NEDERLANDS block's actual height/
// slope-leveling strings, and was missing L5_CHOICE/L5_REF/L5_CAL/
// L5_CAL_REF/L5_CAL_REF_AD/L5_CAL_SLOPE/L5_CAL_SLOPE_AD entirely --
// InterfaceScraper::UpdateScreen() and CalibrationScraper::Calibrate()
// reference these unconditionally, so the file would have failed to compile
// at all had ENGLISH ever been selected. Never compiled in practice
// (NEDERLANDS is always the one #define'd above). Replaced with a real
// translation of the NEDERLANDS block above, matching how LanguagePlanter.hpp's
// and LanguageSprayer.hpp's equivalent gaps were filled.

#define L5_BLANK         "                    "

#define L5_MEIJWORKS     "     MeijWorks      "
#define L5_DEVICE        "Scrapercontrol  v1.1"
#define L5_COPYRIGHT     "      (c) 2026      "
#define L5_AUTHOR        "  by J.A. Woltjer   "

#define L5_HEIGHT_R      "Ref:               m"
#define L5_HEIGHT        "Current:           m"
#define L5_DISTANCE      "Distance:          m"
#define L5_SLOPE         "Slope:      %       "

#define L5_CAL_ACCEPT    "+ : accept          "
#define L5_CAL_DECLINE   "- : cancel          "

#define L5_CAL_DONE      "complete            "
#define L5_CAL_DECLINED  "cancelled           "

#define L5_CAL_ADJUST    "+ / - : to adjust   "
#define L5_CAL_ENTER     "Both  : to accept   "

#define L5_CAL_ON        "                  on"
#define L5_CAL_OFF       "                 off"

#define L5_CHOICE        "Choice menu         "
#define L5_REF           "+ : set reference   "
#define L5_CAL           "- : calibrate       "

#define L5_CAL_REF       "Set reference A     "
#define L5_CAL_REF_AD    "Reference saved     "

#define L5_CAL_SLOPE     "Adjust slope        "
#define L5_CAL_SLOPE_AD  "In cm/100m:       cm"

#define L5_CAL_POS       "Height calibration  "
#define L5_CAL_POS_AD    "Adjust to        cm "

#define L5_CAL_KP        "PID adjust KP       "
#define L5_CAL_KP_AD     "KP:                 "

#define L5_CAL_PWM_M     "PWM adjust manual   "
#define L5_CAL_PWM_M_AD  "PWM manual:         "

#define L5_CAL_PWM_A     "PWM adjust auto     "
#define L5_CAL_PWM_A_AD  "PWM auto:           "

#define L5_CAL_MARGIN    "Adjust error margin "
#define L5_CAL_MARGIN_AD "Margin :          cm"

#define L5_CAL_MAXCOR    "Adj. max. correction"
#define L5_CAL_MAXCOR_AD "Max. corr.:       cm"

#define L5_CAL_QUAL      "Correct RTK ident.  "
#define L5_CAL_QUAL_AD   "Quality:            "

#define L5_CAL_DEUTZ     "Invert hitch signal "
#define L5_CAL_DEUTZ_AD  "Inverted:           "

#define L5_CAL_SPEED     "Speed calibration   "
#define L5_CAL_SPEED_AD  "Accelerate to 10kph "

#define L5_CAL_PROG      "Selecteer programma "
#define L5_CAL_PROG_AD2  "Ploegbesturing      "
#define L5_CAL_PROG_AD3  "Pootmachinebesturing"
#define L5_CAL_PROG_AD4  "Kieperbesturing     "
#define L5_CAL_PROG_AD5  "Kilverbakbesturing  "
#define L5_CAL_PROG_AD6  "Spuitcomputer       "
#define L5_CAL_PROG_AD7  "Zaaimachinebesturing"
#define L5_CAL_PROG_AD8  "Rooierbesturing     "

#define L5_CAL_GPS       "GPS autodetect      "
#define L5_CAL_GPS_DONE  "passed              "
#define L5_CAL_GPS_FAIL  "failed...           "
#define L5_CAL_GPS_M1    "Check cabling and   "
#define L5_CAL_GPS_M2    "nmea output         "

#define L5_CAL_COMPLETE  "Finish calibration  "
#define L5_CAL_NOSAVE    "Data NOT saved      "
#define L5_CAL_DDONE     "done                "
#define L5_CAL_SAVE      "Data saved          "

#endif
