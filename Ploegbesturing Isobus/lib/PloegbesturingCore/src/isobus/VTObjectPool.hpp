/*
  VTObjectPool - ISOBUS VT3 object pool for the MeijWorks plough controller
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

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>

namespace triton
{

// All VT object IDs used in the plough working screen.
enum PloughVtObjectId : uint16_t {
    // Top-level structure
    Plough_WorkingSet  = 0,
    Plough_DataMask    = 1,
    Plough_SoftKeyMask = 2,

    // Soft key objects. 5 was the AUTO key and 14 its label until #282: never
    // reused, so a VT's cached pool can't mistake a new object for them.
    Key_Wider    = 3,
    Key_Narrower = 4,

    // Font attribute objects
    Font_White_Medium = 6,  // 12x16 for data fields
    Font_White_Small  = 7,  // 8x8 for soft key labels

    // Static label strings
    Label_Position = 8,
    Label_Setpoint = 9,
    Label_XTE      = 10,
    Label_Offset   = 11,
    Label_Wider    = 12,
    Label_Narrower = 13,

    // Output numbers (reference the variables below)
    Out_Position = 15,
    Out_Setpoint = 16,
    Out_XTE      = 17,  // displayed in cm (variable biased +1000 cm)
    Out_Offset   = 18,

    // NumberVariables -- updated from loop() via send_change_numeric_value()
    Var_Position = 19,
    Var_Setpoint = 20,
    Var_XTE      = 21,  // stored as (xte_cm + 1000); OutputNumber subtracts 1000
    Var_Offset   = 22,

    // App-switcher icon -- see VTObjectPool.cpp's appendPictureGraphic() call
    // site. Declared as the WorkingSet's sole child object reference, the
    // same pattern AgIsoStack's own reference pool uses for its "avatar"
    // icon (confirmed by decoding that pool's WorkingSet bytes by hand,
    // 2026-08-10) -- this is what a VT's implement/app-switcher list reads
    // to show something other than a blank/generic entry for Triton.
    Icon_Plough = 23,

    // Calibration soft key -- appended after Icon_Plough rather than slotted
    // in next to the other Key_/Label_ objects on purpose: renumbering an
    // existing object ID would silently invalidate every ID already baked
    // into IsobusVtInterface's send_change_numeric_value() calls and into any
    // pool a VT has cached under this working set's version label.
    Key_Calibrate   = 24,
    Label_Calibrate = 25,

    // Top-view picture of the tractor and plough (VTImages.hpp), one per
    // ploughing side, shown through an ObjectPointer that IsobusVtInterface
    // points at the side ImplementPlough::GetSide() reports.
    Img_PloughLeft  = 26,
    Img_PloughRight = 27,
    Ptr_PloughImage = 28,

    // GPS and speed indicators: a green dot when the control's own gate is
    // met (PloughGates.hpp), a red triangle when it is not, each through a
    // pointer IsobusVtInterface switches; plus the speed itself in km/h.
    Img_StatusOk     = 29,
    Img_StatusWarn   = 30,
    Ptr_GpsStatus    = 31,
    Ptr_SpeedStatus  = 32,
    Label_Gps        = 33,
    Out_Speed        = 34,
    Var_Speed        = 35,  // 0.1 km/h
    Label_SpeedUnit  = 36,
    // Its own 8x8 font, not Font_White_Small: a font shared with the soft key
    // labels would make AgIsoStack scale those labels' font with the data
    // mask factor again (neptune-main a9453ff keeps shared fonts there).
    Font_White_Unit  = 37,

    // The + and - key labels' 24x32 font (#282), used on keys only, so it
    // scales with the soft keys like Font_White_Small.
    Font_White_Large = 38,
    // Blank soft key positions: NULL object pointers, Ptr_KeyBlank0 + i for
    // position i of any soft key mask (VTObjectPool.cpp, appendSoftKeys()).
    Ptr_KeyBlank0    = 39,  // .. 50

    // ---- Calibration page (#240) ----
    // A block of its own from 100, so no existing id moves. One label and
    // one input per PloughSettings line, in table order: Label_Cal0 + i and
    // Input_Cal0 + i for line i.
    Plough_CalDataMask    = 100,
    Plough_CalSoftKeyMask = 101,
    Key_CalSave           = 102,
    Key_CalCancel         = 103,
    Label_CalSave         = 104,
    Label_CalCancel       = 105,
    Key_CalWidth          = 106,  // opens the width procedure
    Label_CalWidth        = 107,
    Label_Cal0            = 110,  // .. 119 at most
    Input_Cal0            = 120,  // .. 129 at most: InputNumber or InputList
    // The list entries and units, shared by the inputs that use them;
    // Str_CalCm also follows the main screen's four values (#282).
    Str_CalNo             = 130,
    Str_CalYes            = 131,
    Str_CalRight          = 132,
    Str_CalLeft           = 133,
    Str_CalDgps           = 134,
    Str_CalRtk            = 135,
    Str_CalCm             = 136,
    Str_CalMm             = 137,

    // The width procedure (#240): the point being captured, its target, the
    // live sensor and a status line, with - / + / Opslaan (capture) /
    // Annul. (back).
    Plough_CalWidthMask        = 140,
    Plough_CalWidthSoftKeyMask = 141,
    Key_CalNarrower            = 142,
    Key_CalWider               = 143,
    Key_CalCapture             = 144,
    Key_CalWidthBack           = 145,
    Label_CalNarrower          = 146,
    Label_CalWider             = 147,
    Label_CalCapture           = 148,
    Label_CalWidthBack         = 149,
    Label_CalWidthTitle        = 150,
    Label_CalWidthPoint        = 151,
    Out_CalWidthPoint          = 152,
    Var_CalWidthPoint          = 153,  // 1 to 3
    Label_CalWidthOf           = 154,
    Label_CalWidthTarget       = 155,
    Out_CalWidthTarget         = 156,
    Var_CalWidthTarget         = 157,  // target cm + 1000; the output subtracts it
    Label_CalWidthSensor       = 158,
    Out_CalWidthSensor         = 159,
    Var_CalWidthSensor         = 160,  // the sensor reading (ADC)
    Ptr_CalWidthStatus         = 161,
    Str_CalBlank               = 162,
    Str_CalInvalid             = 163,

    // The lightbar setting's list entries (#272), after the width procedure's.
    Str_CalAuto                = 164,
    Str_CalOn                  = 165,
    Str_CalOff                 = 166,

    // AUX-N (#254): Breder and Smaller as Auxiliary Function Type 2 objects,
    // assignable to joystick or armrest inputs on the VT. Their designators
    // are the soft keys' own labels.
    Aux_Wider                  = 170,
    Aux_Narrower               = 171,

    // The control's state on the main screen (#318), in the LCD's letters:
    // the pointer shows one of the five strings, CAL over the others while
    // a calibration holds the control.
    Ptr_State                  = 172,
    Str_StateAuto              = 173,
    Str_StateHoldSpeed         = 174,
    Str_StateHoldGps           = 175,
    Str_StateHand              = 176,
    Str_StateCal               = 177,
    // The state strings' own 8x12 font: 9 characters in the 74 units beside
    // the picture, where the 12x16 data font holds 6. Data mask only, like
    // Font_White_Unit, so AgIsoStack scales it with the mask.
    Font_White_State           = 178,

    // ---- The width planner (fan-out / straightening) ----
    // Main screen: the PLAN key, and the plan's word under the state line,
    // in the state's 8x12 font, through a pointer; the passes left after it
    // through a second pointer (NULL when the word has no count).
    Key_Plan                   = 200,
    Label_Plan                 = 201,
    Ptr_PlanWord               = 202,
    Ptr_PlanCount              = 203,
    Out_PlanCount              = 204,
    Var_PlanMleft              = 205,   // passes left; also on the planner page
    Str_PlanWordXte            = 206,
    Str_PlanWordNoPlan         = 207,
    Str_PlanWordNoTrack        = 208,
    Str_PlanWordField          = 209,   // the plan is another field's
    Str_PlanWordPass1          = 210,
    Str_PlanWordRange          = 211,   // the plan does not fit Wmin..Wmax
    Str_PlanWordStraight       = 212,   // + the count
    Str_PlanWordFan            = 213,   // + the count
    Str_PlanWordParallel       = 214,
    Str_PlanWordDone           = 215,
    Str_PlanWordWidth          = 216,   // the main width is not the track spacing

    // The planner page: the settings as inputs, stored at once, and the
    // plan's state. Without track data the mode shows XTE and the tuning
    // rows (a container behind a pointer) are left out.
    Plough_PlanMask            = 220,
    Plough_PlanSoftKeyMask     = 221,
    Key_PlanMleftUp            = 222,
    Key_PlanMleftDown          = 223,
    Key_PlanNewField           = 224,
    Key_PlanRange              = 225,
    Key_PlanBack               = 226,
    Label_PlanUpA              = 227,   // two-line key labels: A on top, B under it
    Label_PlanUpB              = 228,
    Label_PlanDownA            = 229,
    Label_PlanDownB            = 230,
    Label_PlanNewA             = 231,
    Label_PlanNewB             = 232,
    Label_PlanRange            = 233,
    Label_PlanBack             = 234,
    Label_PlanMode             = 235,
    Label_PlanK                = 236,
    Label_PlanR                = 237,
    Label_PlanSmooth           = 238,
    Label_PlanTrack            = 239,
    Label_PlanState            = 240,
    Label_PlanMleft            = 241,
    Label_PlanM                = 242,
    Label_PlanWidth            = 243,
    Ptr_PlanMode               = 244,   // Input_PlanMode, or Str_PlanModeXte alone
    Input_PlanMode             = 245,   // index = WidthMode
    Str_PlanModeXte            = 246,
    Str_PlanModeStraight       = 247,
    Str_PlanModeFan            = 248,
    Ptr_PlanTuning             = 249,   // Cont_PlanTuning, or NULL
    Cont_PlanTuning            = 250,
    Input_PlanK                = 251,
    Input_PlanR                = 252,
    Input_PlanSmooth           = 253,
    Str_PlanMeter              = 254,
    Out_PlanTrack              = 255,
    Var_PlanTrack              = 256,   // track spacing, cm
    Out_PlanMleft              = 257,
    Out_PlanM                  = 258,
    Var_PlanM                  = 259,
    Ptr_PlanWidth              = 260,   // Out_PlanWidth, or Str_PlanDash
    Out_PlanWidth              = 261,
    Var_PlanWidth              = 262,   // the planned width, cm
    Str_PlanDash               = 263,

    // The width range page: Wmin and Wmax (MIN / MAX, stepped with + and -),
    // the main width (NOM) and the plough's width now (NU), in cm, over a
    // bar of the plough's whole range, 20 to 60 cm per share.
    Plough_RangeMask           = 270,
    Plough_RangeSoftKeyMask    = 271,
    Key_RangeUp                = 272,
    Key_RangeDown              = 273,
    Key_RangeSelect            = 274,
    Key_RangeSave              = 275,
    Key_RangeCancel            = 276,
    Label_RangeUp              = 277,
    Label_RangeDown            = 278,
    Label_RangeSelect          = 279,
    Label_RangeSave            = 280,
    Label_RangeCancel          = 281,
    Label_RangeMin             = 282,
    Label_RangeMax             = 283,
    Label_RangeNom             = 284,
    Label_RangeNow             = 285,
    Ptr_RangeMinMark           = 286,   // Str_RangeMark on the value + and - step
    Ptr_RangeMaxMark           = 287,
    Str_RangeMark              = 288,
    Str_RangeNoMark            = 289,
    Out_RangeMin               = 290,
    Out_RangeMax               = 291,
    Out_RangeNom               = 292,
    Out_RangeNow               = 293,
    Var_RangeMin               = 294,   // cm, the whole plough
    Var_RangeMax               = 295,
    Var_RangeNom               = 296,
    Var_RangeNow               = 297,
    Bar_RangeMax               = 298,   // filled to Wmax, framed: the whole range
    Bar_RangeMin               = 299,   // filled to Wmin over it, grey; NOM's line
    Bar_RangeNow               = 300,   // a line at the width now
    Var_BarMin                 = 301,   // mm per share, 200-600
    Var_BarMax                 = 302,
    Var_BarNom                 = 303,
    Var_BarNow                 = 304,
    Out_RangeLo                = 305,   // the bar's ends, cm: 20 and 60 per share
    Out_RangeHi                = 306,
    Var_RangeLo                = 307,
    Var_RangeHi                = 308,
};

// Soft key positions per mask, for the owner's layout on a 2 x 6 key VT
// (#282). The bench VT (AgISOVirtualTerminal) fills column 2 top to bottom,
// then column 1: positions 0-5 are column 2 rows 1-6, 6-11 column 1. A
// one-column VT takes the positions in order, six to a page, so column 1
// lands on its second page. Row 6 stays empty on every mask (session 23):
// the InCommand 1200 shows 2 x 5 keys and put a 6th key in the wrong place,
// so the bottom key of a page (ANNUL.) sits on row 5.
constexpr uint8_t kSoftKeyRows  = 6;
constexpr uint8_t kSoftKeySlots = 2 * kSoftKeyRows;
constexpr uint8_t SoftKeySlot(uint8_t row, uint8_t column) {
    return uint8_t((column == 1 ? kSoftKeyRows : 0) + (row - 1));
}

// Key codes embedded in Key objects; reported back in VTKeyEvent::keyNumber.
// 3 was the AUTO key's until #282.
enum PloughKeyCode : uint8_t {
    KeyCode_Wider     = 1,
    KeyCode_Narrower  = 2,
    KeyCode_Calibrate = 4,
    KeyCode_CalSave   = 5,
    KeyCode_CalCancel = 6,
    KeyCode_CalWidth     = 7,
    KeyCode_CalNarrower  = 8,
    KeyCode_CalWider     = 9,
    KeyCode_CalCapture   = 10,
    KeyCode_CalWidthBack = 11,
    KeyCode_Plan         = 12,
    KeyCode_PlanMleftUp  = 13,
    KeyCode_PlanMleftDown = 14,
    KeyCode_PlanNewField = 15,
    KeyCode_PlanRange    = 16,
    KeyCode_PlanBack     = 17,
    KeyCode_RangeUp      = 18,
    KeyCode_RangeDown    = 19,
    KeyCode_RangeSelect  = 20,
    KeyCode_RangeSave    = 21,
    KeyCode_RangeCancel  = 22,
};

// The bar on the width range page spans the plough's whole range, in mm per
// share (ImplementPlough clamps the setpoint to 20-60 cm per share).
constexpr uint16_t kRangeBarMinMm = 200;
constexpr uint16_t kRangeBarMaxMm = 600;

// Populated by BuildObjectPool(); passed to VirtualTerminalClient::set_object_pool().
extern const uint8_t* VT3PoolData;
extern uint32_t       VT3PoolSize;

// Call once in setup(), before VT client initialization.
void BuildObjectPool();

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
