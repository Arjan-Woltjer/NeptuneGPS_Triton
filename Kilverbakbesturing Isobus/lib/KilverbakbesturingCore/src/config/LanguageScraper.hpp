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

#include "TritonText.hpp"

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L_DEVICE        "Triton KB-ISO  v0.01"

#define L_HEIGHT_R      "Ref:               m"
#define L_HEIGHT        "Actueel:           m"
#define L_DISTANCE      "Afstand:           m"
#define L_SLOPE         "Afschot:    %       "

#define L_CHOICE        "Keuzemenu           "
#define L_REF           "+ : referentie inst."
#define L_CAL           "- : calibreren      "

#define L_CAL_REF       "Stel referentie A in"
#define L_CAL_REF_AD    "Referentie opgesl.  "

#define L_CAL_SLOPE     "Wijzig helling      "
#define L_CAL_SLOPE_AD  "In cm/100m:       cm"

#define L_CAL_POS       "Hoogte calibratie   "
#define L_CAL_POS_AD    "Verstel naar     cm "

#define L_CAL_MARGIN    "Wijzig foutmarge    "
#define L_CAL_MARGIN_AD "Foutmarge :       cm"

#define L_CAL_MAXCOR    "Wijzig max correctie"
#define L_CAL_MAXCOR_AD "Max. corr.:       cm"

#define L_CAL_DEUTZ     "Inverteer hefsignaal"
#define L_CAL_DEUTZ_AD  "Inversie:           "

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

// The settings NeptuneGPS_Triton#289 added to the wizard
#define L_CAL_SWAP      "Wissel op en neer   "
#define L_CAL_SPEEDGATE "Snelheid vereist    "
#define L_CAL_VT        "Wijzig voorkeurs-VT "
#define L_CAL_VT_AD     "VT (0 = geen):      "
#define L_CAL_YES       "                  ja"
#define L_CAL_NO        "                 nee"

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy source's ENGLISH block described a different screen layout
// entirely (L_POS/L_A_POS/L_XTE/L_ROTATION -- a position/XTE main
// screen) instead of translating the NEDERLANDS block's actual height/
// slope-leveling strings, and was missing L_CHOICE/L_REF/L_CAL/
// L_CAL_REF/L_CAL_REF_AD/L_CAL_SLOPE/L_CAL_SLOPE_AD entirely --
// InterfaceScraper::UpdateScreen() and CalibrationScraper::Calibrate()
// reference these unconditionally, so the file would have failed to compile
// at all had ENGLISH ever been selected. Never compiled in practice
// (NEDERLANDS is always the one #define'd above). Replaced with a real
// translation of the NEDERLANDS block above, matching how LanguagePlanter.hpp's
// and LanguageSprayer.hpp's equivalent gaps were filled.

#define L_DEVICE        "Triton KB-ISO  v0.01"

#define L_HEIGHT_R      "Ref:               m"
#define L_HEIGHT        "Current:           m"
#define L_DISTANCE      "Distance:          m"
#define L_SLOPE         "Slope:      %       "

#define L_CHOICE        "Choice menu         "
#define L_REF           "+ : set reference   "
#define L_CAL           "- : calibrate       "

#define L_CAL_REF       "Set reference A     "
#define L_CAL_REF_AD    "Reference saved     "

#define L_CAL_SLOPE     "Adjust slope        "
#define L_CAL_SLOPE_AD  "In cm/100m:       cm"

#define L_CAL_POS       "Height calibration  "
#define L_CAL_POS_AD    "Adjust to        cm "

#define L_CAL_MARGIN    "Adjust error margin "
#define L_CAL_MARGIN_AD "Margin :          cm"

#define L_CAL_MAXCOR    "Adj. max. correction"
#define L_CAL_MAXCOR_AD "Max. corr.:       cm"

#define L_CAL_DEUTZ     "Invert hitch signal "
#define L_CAL_DEUTZ_AD  "Inverted:           "

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

// The settings NeptuneGPS_Triton#289 added to the wizard
#define L_CAL_SWAP      "Swap up and down    "
#define L_CAL_SPEEDGATE "Require speed       "
#define L_CAL_VT        "Adj. preferred VT   "
#define L_CAL_VT_AD     "VT (0 = none):      "
#define L_CAL_YES       "                 yes"
#define L_CAL_NO        "                  no"

#endif
