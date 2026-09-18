/*
  CalibrationSprayer - LCD/button calibration wizard for the MeijWorks sprayer interface
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
#include <EEPROM.h>

#include "../implement/ImplementSprayer.hpp"
#include "InterfaceI2CLCD.hpp"
#include "../InterfaceSprayer.hpp"
#include "../config/LanguageSprayer.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

// Extracted from InterfaceSprayer::calibrate() so the LCD/button-driven wizard
// -- which can't run without real hardware -- stays out of the native test
// build, matching CalibrationPlough's/CalibrationPlanter's role for
// Ploegbesturing/Pootmachinebesturing. The caller (main.cpp) checks
// InterfaceSprayer::GetButtons() after each InterfaceSprayer::Update() and
// invokes Calibrate() itself; this class never triggers itself.
//
// Unlike the other two modules' flat linear wizard, this one is a nested menu
// (Sim -> Speed -> Flow -> System -> PID -> Exit, each with its own
// accept/decline submenu) -- preserved exactly as the legacy source had it,
// not flattened to match the other modules' shape.
class CalibrationSprayer {
public:
    CalibrationSprayer(InterfaceI2CLCD* lcd, ImplementSprayer* implement,
                        VehicleTractor* tractor, InterfaceSprayer* interface);

    void Calibrate();

private:
    InterfaceI2CLCD*  lcd;
    ImplementSprayer* implement;
    VehicleTractor*   tractor;
    InterfaceSprayer* interface;
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
