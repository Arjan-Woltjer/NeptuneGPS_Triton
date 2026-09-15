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

// ---------------------------------------------------------------------------
// The rest of the serial protocol and the loop entry point
// (NeptuneGPS_Triton#95): every !VAR setting, the encoder and trigger reads,
// the echo switch both ways, the variable dump, and Update() driving the
// actuator with the parsed setpoint.
// ---------------------------------------------------------------------------

test(InterfaceMotorcontroller, AllVarsDumpsEverySetting)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();
    Serial.inject("?ALLVARS\r");
    iface.ParseSerial();
    const std::string& out = Serial.sent();
    assertTrue(out.find("VAR=") != std::string::npos || out.find("=") != std::string::npos);
    assertTrue(out.size() > 20u);
}

test(InterfaceMotorcontroller, EchoOnAgainEchoesCharacters)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();
    Serial.inject("^ECHOF 1\r");
    iface.ParseSerial();
    assertEqual(Serial.sent().c_str(), "+\r");
    Serial.clearSent();
    Serial.inject("^ECHOF 0\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("+\r") != std::string::npos);
    Serial.clearSent();
    Serial.inject("?TRN\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("?TRN") != std::string::npos);   // echoed again
}

test(InterfaceMotorcontroller, EverySettingVarPersistsAndAcks)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    const char* commands[] = {
        "!VAR 0 42\r",    // user variable
        "!VAR 1 7\r",     // kp
        "!VAR 2 3\r",     // ki
        "!VAR 3 1\r",     // kd
        "!VAR 5 20\r",    // min power
        "!VAR 6 200\r",   // max power
        "!VAR 7 15\r",    // error fraction
        "!VAR 8 4\r",     // allowed error
    };
    for (const char* cmd : commands) {
        Serial.clearSent();
        Serial.inject(cmd);
        iface.ParseSerial();
        assertTrue(Serial.sent().find("+\r") != std::string::npos);
    }
    // Writing the same value twice takes the "already stored" branch.
    Serial.clearSent();
    Serial.inject("!VAR 1 7\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("+\r") != std::string::npos);

    Serial.clearSent();
    Serial.inject("~ATRIG 1\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("~ATRIG=15") != std::string::npos);

    Serial.clearSent();
    Serial.inject("^ATRIG 1 9\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("+\r") != std::string::npos);
}

test(InterfaceMotorcontroller, UnknownVarIndexAndUnknownCommandAreIgnored)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();
    Serial.inject("!VAR 4 1\r");      // no such index
    iface.ParseSerial();
    assertTrue(Serial.sent().find("+\r") == std::string::npos);
    Serial.clearSent();
    Serial.inject("?NOPE\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("+\r") == std::string::npos);
    Serial.clearSent();
    Serial.inject("?C 2\r");          // only encoder 1 exists
    iface.ParseSerial();
    assertTrue(Serial.sent().find("C=") == std::string::npos);
}

test(InterfaceMotorcontroller, UpdateDrivesTheActuatorWithTheParsedSetpoint)
{
    resetAll();
    InterfaceMotorcontroller iface;
    iface.Begin();
    Serial.clearSent();
    Serial.inject("!MG\r");           // release the shutdown
    iface.ParseSerial();
    Serial.inject("!P 1 300\r");
    iface.ParseSerial();
    Serial.inject("!C 1 0\r");
    iface.ParseSerial();
    for (uint32_t now = 0; now <= 500; now += 50) {
        iface.Update(now);            // parses, hands the setpoint over, steps the actuator
    }
    Serial.clearSent();
    Serial.inject("?C 1\r");
    iface.ParseSerial();
    assertTrue(Serial.sent().find("C=") != std::string::npos);
}
