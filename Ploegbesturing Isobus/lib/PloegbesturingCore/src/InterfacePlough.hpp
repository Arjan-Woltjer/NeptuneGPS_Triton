/*
  InterfacePlough - a library for the MeijWorks plough interface
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
#include "ButtonPair.hpp"

#include "config/ConfigInterfacePlough.hpp"
#include "implement/ImplementPlough.hpp"
#include "config/LanguagePlough.hpp"

namespace triton
{

class InterfacePlough {
private:
    //-------------
    // data members
    //-------------

    // Mode
    byte mode; // AUTO, SIM, MANUAL, CALIBRATE

    // Button flag and timer
    short int     buttons;
    ButtonPair    buttonPair;

    // Objects
    Stream*          serialDebug;
    InterfaceI2CLCD* lcd;
    ImplementPlough* implement;
    VehicleTractor*        tractor;
    GuidanceSource*  guidance;

public:
    // ----------------------------------------------------
    // public member functions implemented in InterfacePlough.cpp
    // ----------------------------------------------------

    // Constructor
    InterfacePlough(Stream* serialDebug,
                    InterfaceI2CLCD* lcd,
                    ImplementPlough* implement,
                    VehicleTractor* tractor,
                    GuidanceSource* guidance);

    // vtWiderPressed/vtNarrowerPressed/vtCalibratePressed: an IsobusVtInterface's
    // VT soft keys (see that class's header comment). Wider and Narrower are
    // held levels (#245): Narrower goes through the LEFT (-, -1) and Wider
    // through the RIGHT (+, +1) button's
    // own hold-to-repeat; Calibrate is a one-call tap that gives 2, the
    // calibration trigger main.cpp acts on, without the both-held combo's hold.
    // Defaulted so non-ISOBUS builds and any other caller can omit them --
    // this class stays framework-agnostic, no AgIsoStack/ISOBUS dependency.
    void Update(bool vtWiderPressed = false, bool vtNarrowerPressed = false, bool vtCalibratePressed = false);
    void UpdateScreen(bool rewrite);
    short int CheckButtons(byte delay1, byte delay2,
        bool vtWiderPressed = false, bool vtNarrowerPressed = false, bool vtCalibratePressed = false);

    inline short int GetButtons() {
        return buttons;
    };

    // Calibration wizard trigger: the caller (main.cpp) checks this after
    // Update() and calls CalibrationPlough::Start() itself -- see
    // CalibrationPlough.hpp for why that trigger lives outside this class.
    inline byte GetMode() {
        return mode;
    };
};

}  // namespace triton
