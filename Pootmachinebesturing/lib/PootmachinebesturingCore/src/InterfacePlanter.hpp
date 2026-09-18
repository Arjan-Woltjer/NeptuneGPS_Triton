/*
  InterfacePlanter - a library for the MeijWorks plantercontrol interface
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
#include "GuidanceSource.hpp"

#include "config/ConfigInterfacePlanter.hpp"
#include "implement/ImplementPlanter.hpp"
#include "config/LanguagePlanter.hpp"

namespace triton
{

class InterfacePlanter {
private:
    //-------------
    // data members
    //-------------

    // Mode
    byte mode;  // AUTO, HOLD, MANUAL, CALIBRATE

    // Button flag and timer
    int           buttons;
    bool          button1Flag;
    bool          button2Flag;
    unsigned long button1Timer;
    unsigned long button2Timer;

    // Objects
    Stream*          serialDebug;
    InterfaceI2CLCD* lcd;
    ImplementPlanter* implement;
    VehicleTractor*  tractor;
    GuidanceSource*  guidance;

public:
    // -----------------------------------------------------------
    // public member functions implemented in InterfacePlanter.cpp
    // -----------------------------------------------------------

    // Constructor
    InterfacePlanter(Stream* serialDebug,
                      InterfaceI2CLCD* lcd,
                      ImplementPlanter* implement,
                      VehicleTractor* tractor,
                      GuidanceSource* guidance);

    // Guidance-less overload. Never called by src/main.cpp today (kept for
    // API parity with the legacy library) -- Update() reads the guidance
    // source unconditionally, so this leaves `guidance` at nullptr rather
    // than the uninitialized-pointer read the legacy code had; an instance
    // built this way must not have Update() called on it.
    InterfacePlanter(Stream* serialDebug,
                      InterfaceI2CLCD* lcd,
                      ImplementPlanter* implement,
                      VehicleTractor* tractor);

    void Update();
    void UpdateScreen(boolean rewrite);
    int  CheckButtons(byte delay1, byte delay2);

    inline int GetButtons() {
        return buttons;
    };

    // Calibration wizard trigger: the caller (main.cpp) checks this after
    // Update() and invokes CalibrationPlanter::Calibrate() itself -- see
    // CalibrationPlanter.hpp for why that trigger lives outside this class.
    inline byte GetMode() {
        return mode;
    };
};

}  // namespace triton
