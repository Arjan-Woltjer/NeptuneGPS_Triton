/*
  InterfaceKipper - a library for the MeijWorks kippercontrol interface
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

#include "InterfaceI2CLCD.hpp"
#include "implement/ImplementKipper.hpp"
#include "VehicleTractor.hpp"
#include "config/ConfigInterfaceKipper.hpp"
#include "config/LanguageKipper.hpp"

namespace triton
{

class InterfaceKipper {
private:
    //-------------
    // data members
    //-------------

    // Mode
    byte mode;  // AUTO, HOLD, MANUAL

    // Button flag and timer
    int           buttons;
    bool          button1Flag;
    bool          button2Flag;
    unsigned long button1Timer;
    unsigned long button2Timer;

    // Objects
    InterfaceI2CLCD* lcd;
    ImplementKipper*  implement;
    VehicleTractor*   tractor;

    void writeValue(int value, byte row, byte col);

public:
    // -------------------------------------------------------------
    // public member functions implemented in InterfaceKipper.cpp
    // -------------------------------------------------------------

    // Constructor
    InterfaceKipper(InterfaceI2CLCD* lcd, ImplementKipper* implement, VehicleTractor* tractor);

    void Update();
    void UpdateScreen(boolean rewrite);
    int  CheckButtons(byte delay1, byte delay2);

    inline int GetButtons() {
        return buttons;
    }

    // Calibration wizard trigger: the caller (main.cpp) checks this after
    // Update() and invokes CalibrationKipper::Calibrate() itself -- see
    // CalibrationKipper.hpp for why that trigger lives outside this class.
    inline byte GetMode() {
        return mode;
    }
};

}  // namespace triton
