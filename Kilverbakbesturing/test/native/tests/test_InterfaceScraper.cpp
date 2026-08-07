/*
  test_InterfaceScraper - Tests for InterfaceScraper: CheckButtons() debounce/combo
  detection, and Update()'s AUTO/HOLD/MANUAL mode-selection decision.
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
#include "InterfaceScraper.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Objects under test. InterfaceScraper's constructor needs a real
// ImplementScraper (its header is pulled in directly, not mockable away),
// which in turn touches EEPROM at construction time -- so, like
// test_ImplementScraper.cpp, every test constructs fresh instances after
// resetAll() re-erases the fake EEPROM.
// ---------------------------------------------------------------------------
static VehicleGps       mockGps;
static VehicleTractor   mockTractor;
static InterfaceI2CLCD  mockLcd;

static void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();

    mockGps.ggaFixAge = 0;
    mockGps.vtgFixAge = 0;
    mockGps.minSpeedFlag = true;
    mockGps.latitude = 0;
    mockGps.longitude = 0;
    mockGps.altitudeCm = 0;

    digitalReadValue(LEFT_BUTTON_5, false);
    digitalReadValue(RIGHT_BUTTON_5, false);
    digitalReadValue(JOY_LEFT_5, false);
    digitalReadValue(JOY_RIGHT_5, false);
    // HIGH ("not manual") by default -- Update()'s manual condition is
    // !digitalRead(MODE_PIN_5) || !digitalRead(JOY_MODE_5), so tests that
    // want the auto/hold ladder need both true; manual-specific tests
    // override them back to false explicitly.
    digitalReadValue(MODE_PIN_5, true);
    digitalReadValue(JOY_MODE_5, true);
}

// ---------------------------------------------------------------------------
// CheckButtons() -- shared button1Timer/button2Timer debounce + combo detection.
// delay2 gates single-button detection; delay1*4 gates the combo (both held).
// ---------------------------------------------------------------------------

test(InterfaceScraper, checkButtons_noneHeld_returnsZero) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    assertEqual(iface.CheckButtons(0, 0), 0);
}

test(InterfaceScraper, checkButtons_leftHeld_delayZero_returnsMinusOneImmediately) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), -1);
    assertEqual(iface.GetButtons(), -1);
}

test(InterfaceScraper, checkButtons_rightHeld_delayZero_returnsOneImmediately) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(RIGHT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), 1);
}

test(InterfaceScraper, checkButtons_leftHeld_withDelay_waitsForDebounce) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_5, true);
    // Not yet at the delay2 threshold -> falls back to 0.
    assertEqual(iface.CheckButtons(0, 50), 0);

    millisValue(50);
    assertEqual(iface.CheckButtons(0, 50), -1);
}

test(InterfaceScraper, checkButtons_bothHeld_delayZero_returnsTwoImmediately) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_5, true);
    digitalReadValue(RIGHT_BUTTON_5, true);
    assertEqual(iface.CheckButtons(0, 0), 2);
}

test(InterfaceScraper, checkButtons_bothHeld_withDelay1_waitsForCombo) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(LEFT_BUTTON_5, true);
    digitalReadValue(RIGHT_BUTTON_5, true);
    // delay1=10 -> threshold is delay1*4=40ms.
    assertEqual(iface.CheckButtons(10, 0), 0);

    millisValue(40);
    assertEqual(iface.CheckButtons(10, 0), 2);
}

// ---------------------------------------------------------------------------
// Update() -- AUTO/HOLD/MANUAL mode selection, asserted via GetMode().
// ---------------------------------------------------------------------------

test(InterfaceScraper, update_modePinLow_selectsManual) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(MODE_PIN_5, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceScraper, update_joyModeLow_selectsManual) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    digitalReadValue(JOY_MODE_5, false);
    iface.Update();
    assertEqual(iface.GetMode(), (byte)2);
}

test(InterfaceScraper, update_allConditionsGood_selectsAuto) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    // All fix ages "fresh" relative to millis()=0 (0-0=0, not > 2000); not
    // manual (MODE_PIN_5/JOY_MODE_5 both true per resetAll()).
    iface.Update();
    assertEqual(iface.GetMode(), (byte)0);
}

test(InterfaceScraper, update_staleGgaFix_selectsHold) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    millisValue(2001);
    mockGps.ggaFixAge = 0;     // 2001 - 0 = 2001 > 2000 -> stale
    mockGps.vtgFixAge = 2001;  // fresh
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}

test(InterfaceScraper, update_staleVtgFix_selectsHold) {
    resetAll();
    ImplementScraper impl(&mockGps);
    InterfaceScraper iface(&mockLcd, &impl, &mockTractor, &mockGps);

    millisValue(2001);
    mockGps.ggaFixAge = 2001;  // fresh
    mockGps.vtgFixAge = 0;     // stale
    iface.Update();
    assertEqual(iface.GetMode(), (byte)1);
}
