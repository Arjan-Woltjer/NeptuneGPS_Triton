/*
  InterfaceSprayer - a library for the MeijWorks sprayer interface
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
#include "VehicleTractor.hpp"

#include "ConfigInterfaceSprayer.hpp"
#include "ImplementSprayer.hpp"
#include "LanguageSprayer.hpp"

namespace triton
{

class InterfaceSprayer {
private:
    //-------------
    // data members
    //-------------

    // Mode: 0=Auto, 1=Start (waiting for min speed), 2=Off, 3=Calibrate, 4=Sim
    byte mode;

    // Button flag and timer
    int           buttons;
    bool          button1Flag;
    bool          button2Flag;
    unsigned long button1Timer;
    unsigned long button2Timer;

    unsigned long simTime;

    // Objects
    InterfaceI2CLCD* lcd;
    ImplementSprayer* implement;
    VehicleTractor*  tractor;

public:
    // -----------------------------------------------------------
    // public member functions implemented in InterfaceSprayer.cpp
    // -----------------------------------------------------------

    // Constructor
    InterfaceSprayer(InterfaceI2CLCD* lcd, ImplementSprayer* implement, VehicleTractor* tractor);

    void Update();
    void UpdateScreen(boolean rewrite);
    int  CheckButtons(byte delay1, byte delay2);

    inline int GetButtons() {
        return buttons;
    }

    // Calibration wizard trigger: the caller (main.cpp) checks this after
    // Update() and invokes CalibrationSprayer::Calibrate() itself -- see
    // CalibrationSprayer.hpp for why that trigger lives outside this class.
    inline byte GetMode() {
        return mode;
    }
};

}  // namespace triton
