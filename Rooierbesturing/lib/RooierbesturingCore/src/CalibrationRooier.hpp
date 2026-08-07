/*
  CalibrationRooier - LCD/button calibration wizard for the MeijWorks windrower interface
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

#include "ImplementRooier.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceRooier.hpp"
#include "LanguageRooier.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

// Extracted from the legacy source's free-standing calibrate() function (it
// hardcoded raw Dutch strings and referenced an undefined updateButtons()
// instead of going through InterfaceRooier::CheckButtons() at all) so the
// LCD/button-driven wizard -- which can't run without real hardware -- stays
// out of the native test build, matching CalibrationPlough's/
// CalibrationPlanter's/CalibrationScraper's role. The caller (main.cpp)
// checks InterfaceRooier::GetButtons() after each InterfaceRooier::Update()
// and invokes Calibrate() itself; this class never triggers itself.
class CalibrationRooier {
public:
    CalibrationRooier(InterfaceI2CLCD* lcd, ImplementRooier* implement,
                       VehicleTractor* tractor, InterfaceRooier* interface);

    void Calibrate();

private:
    InterfaceI2CLCD*  lcd;
    ImplementRooier*  implement;
    VehicleTractor*   tractor;
    InterfaceRooier*  interface;

    // Shared shape for every "accept/decline, then adjust a byte/int value
    // by +/-1 per button press until both are held" wizard step -- every
    // one of the legacy fragment's steps (skew, error margin, KP, KI, KD,
    // PWM manual, PWM auto) follows this exact pattern, so it's factored
    // into one helper instead of duplicated seven times (matching how
    // writeValue() was factored out of InterfaceRooier::UpdateScreen()).
    // Returns false if the step was declined (value left unchanged).
    bool adjustValue(const char* title, const char* prompt, int* value, int minValue, int maxValue);
};

}  // namespace triton

#endif  // ARDUINO
