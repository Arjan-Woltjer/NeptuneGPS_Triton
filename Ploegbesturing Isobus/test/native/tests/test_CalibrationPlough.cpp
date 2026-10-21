/*
  test_CalibrationPlough - Tests for the persistence side of the plough's
  calibration wizard: the RTK quality byte it owns since #82 and the receiver
  rate index the serial builds' boot autodetect stores are read at
  construction, an erased or corrupt byte falls back, CommitGuidanceCalibration
  writes both, and the debug dump reports the live values (NeptuneGPS_Triton#88).
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
    assertEqual(CalibrationPlough::kEepromGpsBaudIndex, 10);
    assertEqual(CalibrationPlough::kEepromRtkQuality, 11);
}

test(CalibrationPlough, erasedEeprom_keepsRateIndex0) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
}

test(CalibrationPlough, storedByte10_restoresRateIndex) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromGpsBaudIndex, 7);   // 4800 x 24 = 115200
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 7);
}

test(CalibrationPlough, storedByte10_corrupt_wrapsModuloEight) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromGpsBaudIndex, 9);   // wraps to 1
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 1);
    cal.SetGpsBaudIndex(200);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
}

test(CalibrationPlough, commit_writesRateIndexAndQuality_andBootReadsThemBack) {
    resetAll();
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(nullptr, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    cal.SetGpsBaudIndex(7);
    calGuidance.SetRtkQuality(2);
    cal.CommitGuidanceCalibration();
    assertEqual((int)EEPROM.read(CalibrationPlough::kEepromGpsBaudIndex), 7);
    assertEqual((int)EEPROM.read(CalibrationPlough::kEepromRtkQuality), 2);

    // A board coming back up reads exactly what was committed.
    calGuidance = GuidanceSource();
    ImplementPlough impl2(nullptr, &calGuidance);
    InterfacePlough iface2(nullptr, &calLcd, &impl2, &calTractor, &calGuidance);
    CalibrationPlough again(nullptr, &calLcd, &impl2, &calTractor, &calGuidance, &iface2);
    assertEqual((int)again.GetGpsBaudIndex(), 7);
    assertEqual((int)calGuidance.GetRtkQuality(), 2);
}

test(CalibrationPlough, printCalibrationData_reportsTheLiveRate) {
    resetAll();
    EEPROM.write(CalibrationPlough::kEepromGpsBaudIndex, 7);
    CalCaptureStream dbg;
    ImplementPlough impl(nullptr, &calGuidance);
    InterfacePlough iface(nullptr, &calLcd, &impl, &calTractor, &calGuidance);
    CalibrationPlough cal(&dbg, &calLcd, &impl, &calTractor, &calGuidance, &iface);
    cal.PrintCalibrationData();
    assertTrue(dbg.has("Baudrate\n115200\n"));
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
    assertEqual((int)EEPROM.read(CalibrationPlough::kEepromGpsBaudIndex), 255);
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

// The shares and deadband screens step within the bounds the boot load
// accepts (2-10), so a saved value is never thrown away at the next boot.
test(CalibrationPlough, stepWithin_staysInsideTheLoadBounds) {
    assertEqual(CalibrationPlough::StepWithin(4, 1, 1, kLowestShares, kHighestShares), 5);
    assertEqual(CalibrationPlough::StepWithin(4, -1, 1, kLowestShares, kHighestShares), 3);
    assertEqual(CalibrationPlough::StepWithin(kHighestShares, 1, 1, kLowestShares, kHighestShares), 10);
    assertEqual(CalibrationPlough::StepWithin(kLowestShares, -1, 1, kLowestShares, kHighestShares), 2);
    assertEqual(CalibrationPlough::StepWithin(kHighestDeadband, 1, 1, kLowestDeadband, kHighestDeadband), 10);
    assertEqual(CalibrationPlough::StepWithin(kLowestDeadband, -1, 1, kLowestDeadband, kHighestDeadband), 2);
    assertEqual(CalibrationPlough::StepWithin(6, 0, 1, kLowestDeadband, kHighestDeadband), 6);   // no press
}
