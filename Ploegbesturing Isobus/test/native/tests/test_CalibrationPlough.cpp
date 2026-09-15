/*
  test_CalibrationPlough - Tests for the persistence side of the plough's
  calibration wizard: the RTK quality byte it owns since #82 is read at
  construction, an erased or corrupt byte falls back, and the debug dump
  reports the live value (NeptuneGPS_Triton#88).
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
#include <string>

#include <AUnit.h>
#include "calibration/CalibrationPlough.hpp"

using namespace aunit;
using namespace triton;

// The wizard itself (Calibrate()) blocks on LCD buttons and stays out of
// these tests; what runs here is what main.cpp runs before loop(): the
// constructor's EEPROM read, and the serial dump.

struct CalCaptureStream : public Stream {
    std::string out;
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
};

static GuidanceSource  calGuidance;
static VehicleTractor  calTractor;
static InterfaceI2CLCD calLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    calGuidance = GuidanceSource();   // rtkQuality back to its default, 4
}

test(CalibrationPlough, eepromSlot_isTheOneVehicleGpsUsed) {
    assertEqual(CalibrationPlough::kEepromRtkQuality, 11);
}

test(CalibrationPlough, erasedEeprom_keepsGuidanceDefaultRtkFixed) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)calGuidance.GetRtkQuality(), 4);
    calGuidance.SetQuality(4);
    assertTrue(calGuidance.IsRtkQuality());
}

test(CalibrationPlough, storedByte11_2_restoresDgpsAsRtkEquivalent) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromRtkQuality, 2);
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)calGuidance.GetRtkQuality(), 2);
    // The HOLD interlock now accepts a DGPS fix and refuses RTK-fixed 4.
    calGuidance.SetQuality(2);
    assertTrue(calGuidance.IsRtkQuality());
    calGuidance.SetQuality(4);
    assertFalse(calGuidance.IsRtkQuality());
}

test(CalibrationPlough, storedByte11_corrupt_clampsBackToRtkFixed) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromRtkQuality, 9);
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)calGuidance.GetRtkQuality(), 4);
}

test(CalibrationPlough, constructor_doesNotWriteEeprom) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)EEPROM.read(CalibrationPlough::kEepromRtkQuality), 255);
}

test(CalibrationPlough, printCalibrationData_reportsTheLiveRtkQuality) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromRtkQuality, 2);
    CalCaptureStream dbg;
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(&dbg, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    cal.PrintCalibrationData();
    assertTrue(dbg.has("Guidance source using following data:"));
    assertTrue(dbg.has("RTK Quality\n2\n"));
}
