/*
  test_CalibrationScraper - Tests for the persistence side of the scraper's
  calibration wizard: the receiver rate index byte it owns since #82 is read
  at construction, an erased or corrupt byte falls back, the commit writes
  it, and the debug dump reports the live rate (NeptuneGPS_Triton#91).
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
#include "CalibrationScraper.hpp"

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

struct SilentPort : public HardwareSerial {
    int available() override { return 0; }
    int read() override      { return -1; }
};

static GuidanceSource        calGuidance;
static VehicleTractor        calTractor;
static InterfaceI2CLCD       calLcd;
static SilentPort            calPort;
static SerialGuidanceChannel calChannel(nullptr, &calPort, &calGuidance);

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    calGuidance = GuidanceSource();
}

test(CalibrationScraper, eepromSlot_isTheOneVehicleGpsUsed) {
    assertEqual(CalibrationScraper::kEepromGpsBaudIndex, 10);
}

test(CalibrationScraper, erasedEeprom_keepsIndex0) {
    resetAll();
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calTractor, &calGuidance, &calChannel, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
}

test(CalibrationScraper, storedByte_restoresTheIndex) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 5);   // 4800 x 8 = 38400
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calTractor, &calGuidance, &calChannel, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 5);
}

test(CalibrationScraper, corruptByte_wrapsModuloEight) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 9);
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calTractor, &calGuidance, &calChannel, &iface);
    assertEqual((int)cal.GetGpsBaudIndex(), 1);
    cal.SetGpsBaudIndex(200);
    assertEqual((int)cal.GetGpsBaudIndex(), 0);
    cal.SetGpsBaudIndex(7);
    assertEqual((int)cal.GetGpsBaudIndex(), 7);
}

test(CalibrationScraper, constructor_doesNotWrite_commitWritesTheByte) {
    resetAll();
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calTractor, &calGuidance, &calChannel, &iface);
    assertEqual((int)EEPROM.read(10), 255);
    cal.SetGpsBaudIndex(3);
    cal.CommitGuidanceCalibration();
    assertEqual((int)EEPROM.read(10), 3);

    CalibrationScraper again(&calLcd, &impl, &calTractor, &calGuidance, &calChannel, &iface);
    assertEqual((int)again.GetGpsBaudIndex(), 3);
}

test(CalibrationScraper, printCalibrationData_reportsTheRate) {
    resetAll();
    EEPROM.write(CalibrationScraper::kEepromGpsBaudIndex, 2);   // 4800 x 3 = 14400
    CalCaptureStream dbg;
    ImplementScraper impl(&calGuidance);
    InterfaceScraper iface(&calLcd, &impl, &calTractor, &calGuidance);
    CalibrationScraper cal(&calLcd, &impl, &calTractor, &calGuidance, &calChannel, &iface);
    cal.PrintCalibrationData(&dbg);
    assertTrue(dbg.has("Guidance source using following data:"));
    assertTrue(dbg.has("Baudrate\n14400\n"));
}
