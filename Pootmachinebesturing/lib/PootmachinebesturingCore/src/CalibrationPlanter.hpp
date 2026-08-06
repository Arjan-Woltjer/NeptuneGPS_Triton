/*
  CalibrationPlanter - LCD/button calibration wizard for the MeijWorks planter interface
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

#include "ImplementPlanter.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfacePlanter.hpp"
#include "LanguagePlanter.hpp"
#include "VehicleGps.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

// Extracted from InterfacePlanter::Calibrate() so the LCD/button-driven wizard --
// which can't run without real hardware -- stays out of the native test build,
// matching CalibrationPlough's role for Ploegbesturing. The caller (main.cpp)
// checks InterfacePlanter::GetButtons() after each InterfacePlanter::Update()
// and invokes Calibrate() itself; this class never triggers itself.
class CalibrationPlanter {
public:
    CalibrationPlanter(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlanter* implement,
                        VehicleTractor* tractor, VehicleGps* gps, InterfacePlanter* interface);

    void Calibrate();

private:
    Stream*            serialDebug;
    InterfaceI2CLCD*   lcd;
    ImplementPlanter*  implement;
    VehicleTractor*    tractor;
    VehicleGps*        gps;
    InterfacePlanter*  interface;
};

}  // namespace triton

#endif  // ARDUINO
