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
};

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

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
