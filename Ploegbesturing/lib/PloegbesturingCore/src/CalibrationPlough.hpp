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
#include "GuidanceSource.hpp"
#include "LanguagePlough.hpp"
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
                       VehicleTractor* tractor, GuidanceSource* guidance, InterfacePlough* interface);

    void Calibrate();

    // The two guidance values this board persists. GuidanceSource is a shared
    // data model with no storage of its own (NeptuneGPS_Triton#78), so this
    // class owns their EEPROM bytes: the RTK quality the wizard sets, and the
    // receiver rate index the boot autodetect (InterfaceGuidance) finds. Both
    // sit where VehicleGps kept them, so a board coming from that firmware
    // keeps its settings. Read at construction, written by the wizard's Save
    // step and by main.cpp after a successful detect.
    static constexpr int kEepromGpsBaudIndex = 10;
    static constexpr int kEepromRtkQuality   = 11;

    byte GetGpsBaudIndex() const   { return gpsBaudIndex; }
    void SetGpsBaudIndex(byte idx) { gpsBaudIndex = idx % 8; }
    void CommitGuidanceCalibration();

    void PrintCalibrationData();

private:
    Stream*          serialDebug;
    InterfaceI2CLCD* lcd;
    ImplementPlough* implement;
    VehicleTractor*  tractor;
    GuidanceSource*  guidance;
    InterfacePlough* interface;

    // 4800 x {1,2,3,4,6,8,12,24}; index 0 (4800, the common NMEA default)
    // until a detect or a stored byte says otherwise.
    byte gpsBaudIndex = 0;

    bool loadGuidanceCalibration();
};

}  // namespace triton

#endif  // ARDUINO
