/*
  CalibrationScraper - LCD/button calibration wizard for the MeijWorks scraper interface
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

#include "../implement/ImplementScraper.hpp"
#include "InterfaceI2CLCD.hpp"
#include "../InterfaceScraper.hpp"
#include "GuidanceSource.hpp"
#include "../config/LanguageScraper.hpp"
#include "VehicleSensors.hpp"

// <functional> reaches the STL's three-argument std::min/std::max, which
// Arduino.h's min()/max() macros mangle under GCC.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <functional>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace triton
{

// Extracted from InterfaceScraper::calibrate() so the LCD/button-driven wizard --
// which can't run without real hardware -- stays out of the native test build,
// matching CalibrationPlough's/CalibrationPlanter's/CalibrationSprayer's role.
// The caller (main.cpp) checks InterfaceScraper::GetButtons() after each
// InterfaceScraper::Update() and invokes Calibrate() itself; this class never
// triggers itself.
class CalibrationScraper {
public:
    // pumpGuidance runs inside the wizard's wait-for-a-button loops so the
    // reference points are taken from a live position, as the wizard did
    // through VehicleGps::Update(). main.cpp passes whatever feeds the
    // GuidanceSource on its build: the receiver on the UART, or the ISOBUS
    // stack (which also keeps our address claim alive while the wizard
    // blocks). Until the wizard stops blocking (NeptuneGPS_Triton#289).
    CalibrationScraper(InterfaceI2CLCD* lcd, ImplementScraper* implement, VehicleSensors* tractor,
                        GuidanceSource* guidance, std::function<void()> pumpGuidance, InterfaceScraper* interface);

    void Calibrate();

    // The one guidance value this board persists: the receiver rate index the
    // boot autodetect (InterfaceGuidance) finds. GuidanceSource is a shared
    // data model with no storage of its own (NeptuneGPS_Triton#78), so this
    // class owns the byte, at the slot VehicleGps kept it. The scraper has no
    // RTK quality menu, so byte 11 is left alone.
    static constexpr int kEepromGpsBaudIndex = 10;

    byte GetGpsBaudIndex() const   { return gpsBaudIndex; }
    void SetGpsBaudIndex(byte idx) { gpsBaudIndex = idx % 8; }
    void CommitGuidanceCalibration();

    void PrintCalibrationData(Stream* serial);

private:
    InterfaceI2CLCD*  lcd;
    ImplementScraper* implement;
    VehicleSensors*        tractor;
    GuidanceSource*        guidance;
    std::function<void()>  pumpGuidance;
    InterfaceScraper*      interface;

    byte gpsBaudIndex = 0;   // 4800 x {1,2,3,4,6,8,12,24}; 0 until stored or detected

    bool loadGuidanceCalibration();
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
