/*
  InterfaceRooier - a library for the MeijWorks windrowercontrol interface
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
// This header didn't exist at all in the legacy source -- only
// InterfaceRooier.cpp did, and its own file header even wrongly said
// "InterfacePlanter". Written fresh here, sized to what the .cpp's coherent
// half (its constructor/Update()/UpdateScreen()/CheckButtons() -- the
// abandoned free-function updateMode()/calibrate() state machine mixed into
// the same file is not carried forward) actually needs, matching every
// sibling InterfaceX.hpp's shape.
#pragma once

#include "InterfaceI2CLCD.hpp"
#include "implement/ImplementRooier.hpp"
#include "VehicleTractor.hpp"
#include "config/ConfigInterfaceRooier.hpp"
#include "config/LanguageRooier.hpp"

namespace triton
{

class InterfaceRooier {
private:
    //-------------
    // data members
    //-------------

    // Mode
    byte mode;  // AUTO, MANUAL

    // Button flag and timer
    int           buttons;
    bool          button1Flag;
    bool          button2Flag;
    unsigned long button1Timer;
    unsigned long button2Timer;

    // Objects
    InterfaceI2CLCD* lcd;
    ImplementRooier*  implement;
    VehicleTractor*   tractor;

    // UpdateScreen()'s three value fields (setpoint/heightL/heightR) all
    // render identically, just on different rows -- the legacy source
    // triplicated the ~30-line digit-formatting block and, in doing so, had
    // all three write to row 0 instead of their own row (a copy-paste bug).
    // Factored into one private helper instead of fixing the bug three times.
    void writeValue(int value, byte row);

public:
    // -------------------------------------------------------------
    // public member functions implemented in InterfaceRooier.cpp
    // -------------------------------------------------------------

    // Constructor
    InterfaceRooier(InterfaceI2CLCD* lcd, ImplementRooier* implement, VehicleTractor* tractor);

    void Update();
    void UpdateScreen(boolean rewrite);
    int  CheckButtons(byte delay1, byte delay2);

    inline int GetButtons() {
        return buttons;
    }

    // Calibration wizard trigger: the caller (main.cpp) checks this after
    // Update() and invokes CalibrationRooier::Calibrate() itself -- see
    // CalibrationRooier.hpp for why that trigger lives outside this class.
    inline byte GetMode() {
        return mode;
    }
};

}  // namespace triton
