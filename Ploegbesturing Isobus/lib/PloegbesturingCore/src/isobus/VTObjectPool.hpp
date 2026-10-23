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

    // Soft key objects
    Key_Wider    = 3,
    Key_Narrower = 4,
    Key_Auto     = 5,

    // Font attribute objects
    Font_White_Medium = 6,  // 8x12 for data fields
    Font_White_Small  = 7,  // 8x8 for soft key labels

    // Static label strings
    Label_Position = 8,
    Label_Setpoint = 9,
    Label_XTE      = 10,
    Label_Offset   = 11,
    Label_Wider    = 12,
    Label_Narrower = 13,
    Label_Auto     = 14,

    // Output numbers (reference the variables below)
    Out_Position = 15,
    Out_Setpoint = 16,
    Out_XTE      = 17,  // displayed in metres (variable biased +1000 cm)
    Out_Offset   = 18,

    // NumberVariables -- updated from loop() via send_change_numeric_value()
    Var_Position = 19,
    Var_Setpoint = 20,
    Var_XTE      = 21,  // stored as (xte_cm + 1000); OutputNumber subtracts 1000 and scales to m
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
};

// DIAGNOSTIC (Mookhoek 57, 2026-10-23): a John Deere VT4 rejects the pool with
// "any other error" and no faulting object, MW08 and MW09 alike. Build with
// PLATFORMIO_BUILD_FLAGS="-D VT_JD_BISECT=n" to try:
//   1  no autoscaling (the pool as drawn, 200 px)       label MWJ1
//   2  the plough pictures and status icons as the old  label MWJ2
//      1-bit 16x16 icon instead of 8-bit RLE pictures
//   3  both                                             label MWJ3
//   4  no autoscaling; the RLE pictures marked          label MWJ4
//      transparent (options 0x05, colour 0), as every
//      8-bit RLE picture in John Deere's own pools is
//   5  no autoscaling; the pictures 8-bit uncompressed  label MWJ5
//      (a ~19 kB pool, no RLE: RLE versus pool size)
//   6  the plough pictures as the 1-bit icon, the two   label MWJ6
//      status icons kept 8-bit RLE: 8-bit pictures in a
//      ~1 kB pool (one TP transfer, no ETP)
//   7  the plough pictures 1-bit, 120x76, status icons  label MWJ7
//      1-bit: a ~3 kB pool (ETP), no 8-bit at all
//   8  as 7, the plough pictures stored 240x152 (shown  label MWJ8
//      120 wide): a ~10 kB pool, no 8-bit
//   9  variant 7's pool, sent as several pools of at   label MWJ9
//      most 1 700 B (cut on object boundaries): each a
//      TP transfer, no ETP
//  10  the full MW08 content as TP-sized transfers:    label MWJ10
//      each plough picture as 8-bit RLE strips of at
//      most 1 500 B in a Container that keeps its id
//  11  the full MW08 pool, one ETP transfer, with 255   label MWJ11
//      packets per DPO instead of AgIsoStack's 16
//   6-11 keep the autoscaling (variant 2 cleared it).
// 0 (default) is the production pool, MW08. -D NO_LIGHTBAR leaves the
// Ag Leader lightbar emulation (0xDC) out; it combines with any variant.
#ifndef VT_JD_BISECT
#define VT_JD_BISECT 0
#endif

// Key codes embedded in Key objects; reported back in VTKeyEvent::keyNumber.
enum PloughKeyCode : uint8_t {
    KeyCode_Wider     = 1,
    KeyCode_Narrower  = 2,
    KeyCode_Auto      = 3,
    KeyCode_Calibrate = 4,
};

// Populated by BuildObjectPool(); passed to VirtualTerminalClient::set_object_pool().
extern const uint8_t* VT3PoolData;
extern uint32_t       VT3PoolSize;

// Call once in setup(), before VT client initialization.
void BuildObjectPool();

// DIAGNOSTIC (VT_JD_BISECT 9): cuts the built pool on object boundaries into
// pieces of at most maxBytes. Returns the number of pieces (0 if an object
// type is unknown or one object alone is larger than maxBytes).
uint8_t SplitObjectPool(uint32_t maxBytes, const uint8_t** starts, uint32_t* sizes, uint8_t maxPieces);

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
