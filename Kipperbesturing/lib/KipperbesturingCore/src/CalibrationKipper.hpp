/*
  CalibrationKipper - LCD/button calibration wizard for the MeijWorks kipper interface
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

#ifdef ARDUINO

#include <Arduino.h>
#include <EEPROM.h>

#include "ImplementKipper.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceKipper.hpp"
#include "LanguageKipper.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

// Extracted from InterfaceKipper::calibrate() so the LCD/button-driven
// wizard -- which can't run without real hardware -- stays out of the
// native test build, matching CalibrationPlanter's/CalibrationRooier's
// role. The caller (main.cpp) checks InterfaceKipper::GetButtons() after
// each InterfaceKipper::Update() and invokes Calibrate() itself; this class
// never triggers itself.
class CalibrationKipper {
public:
    CalibrationKipper(InterfaceI2CLCD* lcd, ImplementKipper* implement,
                       VehicleTractor* tractor, InterfaceKipper* interface);

    void Calibrate();

private:
    InterfaceI2CLCD* lcd;
    ImplementKipper*  implement;
    VehicleTractor*   tractor;
    InterfaceKipper*  interface;

    // Shared accept/decline + adjust-by-one-per-press shape every KP/KI/KD/
    // offset step in the legacy calibrate() fragment repeated four times.
    bool adjustValue(const char* title, const char* prompt, int* value, int minValue, int maxValue);
};

}  // namespace triton

#endif  // ARDUINO
