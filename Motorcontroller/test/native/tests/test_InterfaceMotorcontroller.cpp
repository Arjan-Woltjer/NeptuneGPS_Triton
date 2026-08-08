/*
  test_InterfaceMotorcontroller - Tests for InterfaceMotorcontroller: the roboteq-style
  serial parser's term splitting and command handling.
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
// PID/config/safety behavior itself is exercised by Shared Firmware Libs/
// SteeringActuator's own test suite, not duplicated here -- see that library's
// test/README for why. These tests only cover the roboteq-style serial protocol layer:
// term splitting/command dispatch, and that each command reaches (or correctly doesn't
// reach) the underlying SteeringActuator.
#include <AUnit.h>

#include "InterfaceMotorcontroller.hpp"

using namespace aunit;
using namespace triton;

namespace
{
void resetAll()
{
    Serial.clearSent();
}
}

test(InterfaceMotorcontroller, ReadControlReportsTrn)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();  // discard Begin()'s own startup banner

    Serial.inject("?TRN\r");
    iface.ParseSerial();

    assertEqual(Serial.sent().c_str(), "TRN=SDC2XXX:SDC2160S\r");
}

test(InterfaceMotorcontroller, EmergencyShutdownThenReleaseAcks)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();

    Serial.inject("!EX\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");

    Serial.clearSent();
    Serial.inject("!MG\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");
}

test(InterfaceMotorcontroller, FaultReadReflectsShutdownState)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();

    // Freshly Begin()'d: not yet released via "!MG", so still in the emergency-stop state.
    Serial.clearSent();
    Serial.inject("?FF\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "FF=16\r");

    Serial.clearSent();
    Serial.inject("!MG\r");
    iface.ParseSerial();
    Serial.clearSent();
    Serial.inject("?FF\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "FF=0\r");
}

test(InterfaceMotorcontroller, SetGainPersistsAndAcks)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();

    Serial.inject("!VAR 1 100\r");  // kp = 100
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");

    Serial.clearSent();
    Serial.inject("?ALLVARS\r");
    iface.ParseSerial();
    // kp is the first field in "VARS=kp,ki,kd,minPower,maxPower,errorFraction,allowedError".
    assertEqual(Serial.sent().rfind("VARS=100,", 0), static_cast<size_t>(0));
}

test(InterfaceMotorcontroller, GotoSetpointAcks)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();

    Serial.inject("!P 1 500\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");
}

test(InterfaceMotorcontroller, SetEncoderAcksAndReadEncoderReportsIt)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();

    Serial.inject("!C 1 42\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");

    Serial.clearSent();
    Serial.inject("?C 1\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "C=42\r");
}

test(InterfaceMotorcontroller, EchoOffSuppressesCharacterEcho)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();

    // Default (echo=false, matching legacy v2.0gui's own default): no per-character echo.
    Serial.inject("?TRN\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "TRN=SDC2XXX:SDC2160S\r");

    Serial.clearSent();
    Serial.inject("^ECHOF 1\r");  // explicitly request echo off -> still no ack, matches legacy shape
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");
}

test(InterfaceMotorcontroller, TermsSplitOnSpaceAndCr)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();

    // Byte-at-a-time delivery must parse identically to all-at-once.
    const char* command = "!P 1 250\r";
    for (const char* p = command; *p; ++p)
    {
        char one[2] = { *p, '\0' };
        Serial.inject(one);
        iface.ParseSerial();
    }

    assertEqual(Serial.sent().c_str(), "+\r");
}
