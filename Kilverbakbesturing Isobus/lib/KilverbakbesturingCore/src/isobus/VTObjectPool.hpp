/*
  VTObjectPool - ISOBUS VT3 object pool for the MeijWorks scraper (kilverbak) controller
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

// The ISOBUS builds only, on the board and in [env:native]: a serial build
// has no VT, and PlatformIO's LDF compiles every .cpp of this library on it.
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

namespace triton
{

// Every VT object id of the kilverbak's pool (NeptuneGPS_Triton#291), laid out
// as the plough's (Ploegbesturing Isobus' VTObjectPool.hpp): once a pool has
// been on a VT, an id keeps its meaning. Add new objects at the end of a
// block, never renumber, and bump the label (ScraperVtWorkingSet.cpp).
enum ScraperVtObjectId : uint16_t {
    // Top-level structure
    Scraper_WorkingSet  = 0,
    Scraper_DataMask    = 1,
    Scraper_SoftKeyMask = 2,

    // Soft keys: + and - as the cab buttons, and the two reference captures.
    Key_Plus  = 3,
    Key_Minus = 4,
    Key_RefA  = 5,
    Key_RefB  = 6,

    // Font attributes. Font_White_Small and Font_White_Large are used on soft
    // keys only, so AgIsoStack scales them with the keys (see the plough's
    // pool); Font_White_Unit is the 8x8 off the keys.
    Font_White_Medium = 7,   // 12x16, the values and their labels
    Font_White_Small  = 8,   // 8x8, the key labels
    Font_White_Unit   = 9,   // 8x8, "km/h"
    Font_White_Large  = 10,  // 24x32, the + and - keys

    // Soft key labels
    Label_Plus  = 11,
    Label_Minus = 12,
    Label_RefA  = 13,
    Label_RefB  = 14,

    // The app-switcher icon, the WorkingSet's one child (see the plough's
    // pool for why a WorkingSet needs one).
    Icon_Scraper = 15,

    // The side view (VTImages.hpp): the blade on, above or below the plane,
    // through a pointer ScraperVtWorkingSet switches.
    Img_OnPlane     = 16,
    Img_AbovePlane  = 17,
    Img_BelowPlane  = 18,
    Ptr_ScraperImage = 19,

    // GPS and speed indicators, as the plough's: a green dot when the gate is
    // met (ScraperGates.hpp), a red triangle when not; and the speed in km/h.
    Img_StatusOk    = 20,
    Img_StatusWarn  = 21,
    Ptr_GpsStatus   = 22,
    Ptr_SpeedStatus = 23,
    Label_Gps       = 24,
    Out_Speed       = 25,
    Var_Speed       = 26,  // 0.1 km/h
    Label_SpeedUnit = 27,

    // The mode, as the LCD shows it: AUTO, HAND, or H with the hold reason.
    Ptr_Mode      = 28,
    Str_ModeAuto  = 29,
    Str_ModeHand  = 30,
    Str_HoldNoFix = 31,  // H G!
    Str_HoldNoRtk = 32,  // H R!
    Str_HoldSpeed = 33,  // H S!

    // The six rows below the picture: label, value, variable. The variables
    // are biased so the uint32 stays non-negative; each OutputNumber's offset
    // takes the bias off again (VTObjectPool.cpp).
    Label_Deviation = 34,
    Out_Deviation   = 35,
    Var_Deviation   = 36,  // cm + kSmallBias
    Label_Offset    = 37,
    Out_Offset      = 38,
    Var_Offset      = 39,  // cm + kSmallBias
    Label_Reference = 40,
    Out_Reference   = 41,
    Var_Reference   = 42,  // cm + kLargeBias, shown in m
    Label_Height    = 43,
    Out_Height      = 44,
    Var_Height      = 45,  // cm + kLargeBias, shown in m
    Label_Distance  = 46,
    Out_Distance    = 47,
    Var_Distance    = 48,  // m + kLargeBias
    Label_Slope     = 49,
    Out_Slope       = 50,
    Var_Slope       = 51,  // cm per 100 m + kSmallBias, shown in %

    // Units after the values
    Str_UnitM       = 52,
    Str_UnitCm      = 53,
    Str_UnitPercent = 54,

    // The deviation as a vertical Linear Bar Graph next to the picture: a
    // value line against a target line at 0, kBarRangeCm each way.
    Bar_Deviation    = 55,
    Var_DeviationBar = 56,  // 0 .. 2 * kBarRangeCm, kBarRangeCm = on the plane

    // Blank soft key positions: NULL object pointers, Ptr_KeyBlank0 + i for
    // position i of any soft key mask.
    Ptr_KeyBlank0 = 60,  // .. 71

    // ---- The reference capture (#291) ----
    // REF A / REF B open this page; OPSLAAN takes the point, ANNUL. goes back.
    // The page shows which reference, the height it would take and the GPS
    // dot, and a status line when OPSLAAN was refused (no RTK fixed fix).
    Scraper_RefMask        = 80,
    Scraper_RefSoftKeyMask = 81,
    Key_RefSave            = 82,
    Key_RefCancel          = 83,
    Label_RefSave          = 84,
    Label_RefCancel        = 85,
    Ptr_RefTitle           = 86,
    Str_RefTitleA          = 87,
    Str_RefTitleB          = 88,
    Label_RefQuestion      = 89,
    Ptr_RefStatus          = 90,
    Str_RefBlank           = 91,
    Str_RefNoRtk           = 92,
};

// The variables' biases (see the rows above).
constexpr int32_t kSmallBias = 1000;     // cm or cm/100 m: -999 .. 999
constexpr int32_t kLargeBias = 100000;   // cm of height (int16) or m of distance
// The bar's range each way, in cm.
constexpr int32_t kBarRangeCm = 10;

// Soft key positions per mask, as the plough's (#282): on a 2 x 6 VT that
// fills column 2 top to bottom, then column 1, positions 0-5 are column 2
// rows 1-6; a one-column VT takes them in order.
constexpr uint8_t kSoftKeyRows  = 6;
constexpr uint8_t kSoftKeySlots = 2 * kSoftKeyRows;
constexpr uint8_t SoftKeySlot(uint8_t row, uint8_t column) {
    return uint8_t((column == 1 ? kSoftKeyRows : 0) + (row - 1));
}

// Key codes embedded in Key objects; reported back in VTKeyEvent::keyNumber.
enum ScraperKeyCode : uint8_t {
    KeyCode_Plus      = 1,
    KeyCode_Minus     = 2,
    KeyCode_RefA      = 3,
    KeyCode_RefB      = 4,
    KeyCode_RefSave   = 5,
    KeyCode_RefCancel = 6,
};

// Populated by BuildObjectPool(); passed to VirtualTerminalClient::set_object_pool().
extern const uint8_t* VT3PoolData;
extern uint32_t       VT3PoolSize;

// Call once in setup(), before VT client initialization.
void BuildObjectPool();

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
