/*
  CalibrationPlough - LCD/button calibration wizard for the MeijWorks plough interface
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

#include "ImplementPlough.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlough.hpp"
#include "LanguagePlough.hpp"
#include "VehicleGps.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

// Extracted from InterfacePlough::Calibrate() so the LCD/button-driven wizard --
// which can't run without real hardware -- stays out of the native test build,
// matching CalibrationSprayer's role for Loofdoes. The caller (main.cpp) checks
// InterfacePlough::GetButtons() after each InterfacePlough::Update() and invokes
// Calibrate() itself; this class never triggers itself.
class CalibrationPlough {
public:
    CalibrationPlough(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlough* implement,
                       VehicleTractor* tractor, VehicleGps* gps, InterfacePlough* interface);

    void Calibrate();

private:
    Stream*          serialDebug;
    InterfaceI2CLCD* lcd;
    ImplementPlough* implement;
    VehicleTractor*  tractor;
    VehicleGps*      gps;
    InterfacePlough* interface;
};

}  // namespace triton

#endif  // ARDUINO
