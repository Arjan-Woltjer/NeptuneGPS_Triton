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

// ---------------
// Taal NEDERLANDS
// ---------------
#if defined NEDERLANDS

#define L8_BLANK         "                    "

#define L8_MEIJWORKS     "     MeijWorks      "
#define L8_DEVICE        "Rooierbesturing v1.1"
#define L8_COPYRIGHT     "      (c) 2026      "
#define L8_AUTHOR        "  by J.A. Woltjer   "

// The legacy source only ever defined L_8_L_POS/L_8_R_POS, while
// InterfaceRooier.cpp's own UpdateScreen() referenced L_8_POS_L/L_8_POS_R --
// a naming mismatch that would have failed to compile. Renamed here
// (L8_POS_L/L8_POS_R, also dropping the legacy L_8_ underscore prefix to
// match every sibling module's L<N>_ convention) and used consistently on
// both sides.
#define L8_REFERENCE     "Ref. hoogte:       %"
#define L8_POS_L         "Hoogte links:      %"
#define L8_POS_R         "Hoogte rechts:     %"

#define L8_CAL_ACCEPT    "+ : accepteren      "
#define L8_CAL_DECLINE   "- : annuleren       "

#define L8_CAL_DONE      "voltooid            "
#define L8_CAL_DECLINED  "geannuleerd         "

#define L8_CAL_ADJUST    "+ / - : verstellen  "
#define L8_CAL_ENTER     "Beide : accepteren  "

// The legacy calibrate() fragment hardcoded raw Dutch strings for every
// wizard screen instead of using language-file constants like every sibling
// module's calibration wizard -- these are new constants (no legacy L_8_*
// counterpart existed), following the same L8_CAL_* naming shape.
#define L8_CAL_SKEW      "Verschilcalibratie  "
#define L8_CAL_SKEW_AD   "Verschil  :       cm"

#define L8_CAL_MARGIN    "Margecalibratie     "
#define L8_CAL_MARGIN_AD "Marge     :       cm"

#define L8_CAL_POS_L     "Hoogtecalibratie L  "
#define L8_CAL_POS_R     "Hoogtecalibratie R  "
#define L8_CAL_POS_AD    "Verstel naar     %  "

#define L8_CAL_KP        "PID wijzig KP       "
#define L8_CAL_KP_AD     "KP:                 "

#define L8_CAL_KI        "PID wijzig KI       "
#define L8_CAL_KI_AD     "KI:                 "

#define L8_CAL_KD        "PID wijzig KD       "
#define L8_CAL_KD_AD     "KD:                 "

#define L8_CAL_PWM_M     "Wijzig PWM handmatig"
#define L8_CAL_PWM_M_AD  "PWM handmatig:      "

#define L8_CAL_PWM_A     "Wijzig PWM automaat "
#define L8_CAL_PWM_A_AD  "PWM automaat:       "

#define L8_CAL_COMPLETE  "Calibratie afronden "
#define L8_CAL_NOSAVE    "Data NIET opgeslagen"
#define L8_CAL_DDONE     "geslaagd            "
#define L8_CAL_SAVE      "Data is opgeslagen  "

// -----------
// Taal ENGELS
// -----------
#elif defined ENGLISH

// The legacy source had no ENGLISH block at all (only #ifdef NEDERLANDS, no
// #elif) -- a real translation of the NEDERLANDS block above, matching how
// every other Triton module's equivalent gap was filled.

#define L8_BLANK         "                    "

#define L8_MEIJWORKS     "     MeijWorks      "
#define L8_DEVICE        "Windrower control v1"
#define L8_COPYRIGHT     "      (c) 2026      "
#define L8_AUTHOR        "  by J.A. Woltjer   "

#define L8_REFERENCE     "Ref. height:       %"
#define L8_POS_L         "Height left:       %"
#define L8_POS_R         "Height right:      %"

#define L8_CAL_ACCEPT    "+ : accept          "
#define L8_CAL_DECLINE   "- : cancel          "

#define L8_CAL_DONE      "complete            "
#define L8_CAL_DECLINED  "cancelled           "

#define L8_CAL_ADJUST    "+ / - : to adjust   "
#define L8_CAL_ENTER     "Both  : to accept   "

#define L8_CAL_SKEW      "Skew calibration    "
#define L8_CAL_SKEW_AD   "Skew      :       cm"

#define L8_CAL_MARGIN    "Margin calibration  "
#define L8_CAL_MARGIN_AD "Margin    :       cm"

#define L8_CAL_POS_L     "Height calibration L"
#define L8_CAL_POS_R     "Height calibration R"
#define L8_CAL_POS_AD    "Adjust to        %  "

#define L8_CAL_KP        "PID adjust KP       "
#define L8_CAL_KP_AD     "KP:                 "

#define L8_CAL_KI        "PID adjust KI       "
#define L8_CAL_KI_AD     "KI:                 "

#define L8_CAL_KD        "PID adjust KD       "
#define L8_CAL_KD_AD     "KD:                 "

#define L8_CAL_PWM_M     "PWM adjust manual   "
#define L8_CAL_PWM_M_AD  "PWM manual:         "

#define L8_CAL_PWM_A     "PWM adjust auto     "
#define L8_CAL_PWM_A_AD  "PWM auto:           "

#define L8_CAL_COMPLETE  "Finish calibration  "
#define L8_CAL_NOSAVE    "Data NOT saved      "
#define L8_CAL_DDONE     "done                "
#define L8_CAL_SAVE      "Data saved          "

#endif
