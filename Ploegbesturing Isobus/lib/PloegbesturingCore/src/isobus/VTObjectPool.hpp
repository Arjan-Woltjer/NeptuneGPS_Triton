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
    // live sensor and a status line, with Smaller / Breder / Vastleggen /
    // Terug.
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
};

// Soft key positions per mask, for the owner's layout on a 2 x 6 key VT
// (#282). The bench VT (AgISOVirtualTerminal) fills column 2 top to bottom,
// then column 1: positions 0-5 are column 2 rows 1-6, 6-11 column 1. A
// one-column VT takes the positions in order, six to a page, so column 1
// lands on its second page.
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
};

// Populated by BuildObjectPool(); passed to VirtualTerminalClient::set_object_pool().
extern const uint8_t* VT3PoolData;
extern uint32_t       VT3PoolSize;

// Call once in setup(), before VT client initialization.
void BuildObjectPool();

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
