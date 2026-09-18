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

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>
#include <EEPROM.h>

#include "../implement/ImplementPlough.hpp"
#include "InterfaceI2CLCD.hpp"
#include "../InterfacePlough.hpp"
#include "GuidanceSource.hpp"
#include "../config/LanguagePlough.hpp"
#include "VehicleTractor.hpp"

namespace triton
{

// Extracted from InterfacePlough::Calibrate() so the LCD/button-driven wizard --
// which can't run without real hardware -- stays out of the native test build,
// matching CalibrationSprayer's role for Spuitcomputer LD. The caller (main.cpp) checks
// InterfacePlough::GetButtons() after each InterfacePlough::Update() and invokes
// Calibrate() itself; this class never triggers itself.
class CalibrationPlough {
public:
    CalibrationPlough(Stream* serialDebug, InterfaceI2CLCD* lcd, ImplementPlough* implement,
                       VehicleTractor* tractor, GuidanceSource* guidance, InterfacePlough* interface);

    void Calibrate();

    // The RTK quality the operator picked in the wizard is the one
    // GuidanceSource value that persists. GuidanceSource is a shared data
    // model with no storage of its own (NeptuneGPS_Triton#78), so this class
    // owns its EEPROM byte: read into the source at construction, written
    // back by the wizard's Save step. Byte 11 is the slot VehicleGps used for
    // the same value, so a board coming from that firmware keeps its setting.
    static constexpr int kEepromRtkQuality = 11;

    void PrintCalibrationData();

private:
    bool loadGuidanceCalibration();
    void commitGuidanceCalibration();

    Stream*          serialDebug;
    InterfaceI2CLCD* lcd;
    ImplementPlough* implement;
    VehicleTractor*        tractor;
    GuidanceSource*  guidance;
    InterfacePlough*       interface;
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
