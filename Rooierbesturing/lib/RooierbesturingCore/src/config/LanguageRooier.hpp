/*
  LanguageRooier - LCD language string tables for rooierbesturing
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

#define L_DEVICE        "Rooierbesturing v1.1"

// The legacy source only ever defined L_8_L_POS/L_8_R_POS, while
// InterfaceRooier.cpp's own UpdateScreen() referenced L_8_POS_L/L_8_POS_R --
// a naming mismatch that would have failed to compile. Renamed here
// (L_POS_L/L_POS_R, also dropping the legacy L_8_ underscore prefix to
// match every sibling module's L<N>_ convention) and used consistently on
// both sides.
#define L_REFERENCE     "Ref. hoogte:       %"
#define L_POS_L         "Hoogte links:      %"
#define L_POS_R         "Hoogte rechts:     %"

// The legacy calibrate() fragment hardcoded raw Dutch strings for every
// wizard screen instead of using language-file constants like every sibling
// module's calibration wizard -- these are new constants (no legacy L_8_*
// counterpart existed), following the same L_CAL_* naming shape.
#define L_CAL_SKEW      "Verschilcalibratie  "
#define L_CAL_SKEW_AD   "Verschil  :       cm"

#define L_CAL_MARGIN    "Margecalibratie     "
#define L_CAL_MARGIN_AD "Marge     :       cm"

#define L_CAL_POS_L     "Hoogtecalibratie L  "
#define L_CAL_POS_R     "Hoogtecalibratie R  "
#define L_CAL_POS_AD    "Verstel naar     %  "

#define L_CAL_INVERSION "Inverteer hefsignaal"
#define L_CAL_YES       "                  ja"
#define L_CAL_NO        "                 nee"

#define L_CAL_COMPLETE  "Calibratie afronden "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy source had no ENGLISH block at all (only #ifdef NEDERLANDS, no
// #elif) -- a real translation of the NEDERLANDS block above, matching how
// every other Triton module's equivalent gap was filled.

#define L_DEVICE        "Windrower control v1"

#define L_REFERENCE     "Ref. height:       %"
#define L_POS_L         "Height left:       %"
#define L_POS_R         "Height right:      %"

#define L_CAL_SKEW      "Skew calibration    "
#define L_CAL_SKEW_AD   "Skew      :       cm"

#define L_CAL_MARGIN    "Margin calibration  "
#define L_CAL_MARGIN_AD "Margin    :       cm"

#define L_CAL_POS_L     "Height calibration L"
#define L_CAL_POS_R     "Height calibration R"
#define L_CAL_POS_AD    "Adjust to        %  "

#define L_CAL_INVERSION "Invert hitch signal "
#define L_CAL_YES       "                 yes"
#define L_CAL_NO        "                  no"

#define L_CAL_COMPLETE  "Finish calibration  "

#endif
