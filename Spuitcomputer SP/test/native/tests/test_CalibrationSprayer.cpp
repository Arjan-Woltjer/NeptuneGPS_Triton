/*
  test_CalibrationSprayer - The hose-pump sprayer's calibration wizard has no
  persistence of its own (ImplementSprayer owns the EEPROM block); what can
  run natively is its construction, which must touch nothing
  (NeptuneGPS_Triton#94).
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
#include <AUnit.h>
#include "calibration/CalibrationSprayer.hpp"

using namespace aunit;
using namespace triton;

static VehicleTractor  calTractor;
static InterfaceI2CLCD calLcd;

// The wizard (Calibrate()) blocks on LCD buttons and on the timed pump
// calibration and stays out of the native run; main.cpp constructs it before
// loop() and that construction must have no side effect on the EEPROM the
// implement just read.
test(CalibrationSprayer, constructor_touchesNoEeprom) {
    millisValue(0);
    EEPROM.eepromReset();
    ImplementSprayer impl(&calTractor);
    InterfaceSprayer iface(&calLcd, &impl, &calTractor);
    const int bootCounter = EEPROM.read(0);
    CalibrationSprayer cal(&calLcd, &impl, &calTractor, &iface);
    assertEqual((int)EEPROM.read(0), bootCounter);
    for (int addr = 1; addr < 255; ++addr) {
        if (EEPROM.read(addr) != 255) { assertEqual(addr, -1); }
    }
    assertEqual(impl.GetDose(), 200);
}
