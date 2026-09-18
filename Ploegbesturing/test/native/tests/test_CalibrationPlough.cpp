/*
  test_CalibrationPlough - Tests for the persistence side of the plough's
  calibration wizard: the receiver rate index and RTK quality bytes it owns
  since #82 are read at construction, erased or corrupt bytes fall back,
  CommitGuidanceCalibration writes both, and the debug dump reports the live
  values (NeptuneGPS_Triton#89).
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
// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <string> uses std::min/max with three arguments.
#include <string>

#include <AUnit.h>
#include "calibration/CalibrationPlough.hpp"

using namespace aunit;
using namespace triton;

// The wizard itself (Calibrate()) blocks on LCD buttons and stays out of
// these tests; what runs here is what main.cpp runs around loop(): the
// constructor's EEPROM read, the commit after a successful detect, and the
// serial dump.

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

test(CalibrationPlough, eepromSlots_areTheOnesVehicleGpsUsed) {
    assertEqual(CalibrationPlough::kEepromGpsBaudIndex, 10);
    assertEqual(CalibrationPlough::kEepromRtkQuality, 11);
}

test(CalibrationPlough, erasedEeprom_keepsIndex0_andRtkFixed) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
    assertEqual((int)calGuidance.GetRtkQuality(), 4);
}

test(CalibrationPlough, storedBytes_restoreIndexAndQuality) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromGpsBaudIndex, 5);   // 4800 x 8 = 38400
    EEPROM.write(CalibrationPlough::kEepromRtkQuality, 2);
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 5);
    assertEqual((int)calGuidance.GetRtkQuality(), 2);
    calGuidance.SetQuality(2);
    assertTrue(calGuidance.IsRtkQuality());
    calGuidance.SetQuality(4);
    assertFalse(calGuidance.IsRtkQuality());
}

test(CalibrationPlough, corruptBytes_wrapTheIndex_andClampTheQuality) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromGpsBaudIndex, 9);   // wraps to 1
    EEPROM.write(CalibrationPlough::kEepromRtkQuality, 9);     // clamps to 4
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 1);
    assertEqual((int)calGuidance.GetRtkQuality(), 4);
}

test(CalibrationPlough, setGpsBaudIndex_wrapsModuloEight) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    cal.SetGpsBaudIndex(7);
    assertEqual((int)cal.GetGpsBaudIndex(), 7);
    cal.SetGpsBaudIndex(8);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
    cal.SetGpsBaudIndex(200);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
}

test(CalibrationPlough, constructor_doesNotWrite_commitWritesBothBytes) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)EEPROM.read(10), 255);
    assertEqual((int)EEPROM.read(11), 255);

    cal.SetGpsBaudIndex(3);
    calGuidance.SetRtkQuality(2);
    cal.CommitGuidanceCalibration();
    assertEqual((int)EEPROM.read(10), 3);
    assertEqual((int)EEPROM.read(11), 2);

    // A board coming back up reads exactly what was committed.
    calGuidance = GuidanceSource();
    ImplementPlough impl2(nullptr, &calGuidance);
    InterfacePlough iface2(nullptr, &calLcd, &impl2, &calTractor, &calGuidance);
    CalibrationPlough again(nullptr, &calLcd, &impl2, &calTractor, &calGuidance, &iface2);
    assertEqual((int)again.GetGpsBaudIndex(), 3);
    assertEqual((int)calGuidance.GetRtkQuality(), 2);
}

test(CalibrationPlough, printCalibrationData_reportsRateAndQuality) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromGpsBaudIndex, 2);   // 4800 x 3 = 14400
    CalCaptureStream dbg;
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(&dbg, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    cal.PrintCalibrationData();
    assertTrue(dbg.has("Guidance source using following data:"));
    assertTrue(dbg.has("Baudrate\n14400\n"));
    assertTrue(dbg.has("RTK Quality\n4\n"));
}
