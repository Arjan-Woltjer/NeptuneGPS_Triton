/*
  InterfaceMotorcontroller - roboteq-style serial protocol driving a SteeringActuator
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
#include "InterfaceMotorcontroller.hpp"

#include <stdlib.h>

#include <FlashStorage.h>

#include "ConfigMotorcontroller.hpp"

namespace triton
{
namespace
{
constexpr uint8_t kConfigElectric1 = 0x40;  // meijworks::SteeringActuator's Config::Electric1 mode-select bits
constexpr uint8_t kEnabledBit = 0x10;

constexpr int kKpDefault = 3;
constexpr int kKiDefault = 2;
constexpr int kKdDefault = 0;
constexpr int kMinPowerDefault = 50;
constexpr int kMaxPowerDefault = 190;
constexpr int kErrorFractionDefault = 25;
constexpr int kAllowedErrorDefault = 255;  // effectively disabled -- see the header's field comment

uint8_t buildMode(bool enabled)
{
    return static_cast<uint8_t>(kConfigElectric1 | (enabled ? kEnabledBit : 0));
}

// FlashStorage() expands to a global object -- kept file-scope (matching the legacy
// sketch's own layout) since the macro can't be used as a class member.
FlashStorage(gFlashKp, int);
FlashStorage(gFlashKi, int);
FlashStorage(gFlashKd, int);
FlashStorage(gFlashMinPower, int);
FlashStorage(gFlashMaxPower, int);
FlashStorage(gFlashErrorFraction, int);
FlashStorage(gFlashAllowedError, int);
}

meijworks::SteeringActuator::Pins InterfaceMotorcontroller::buildPins()
{
    meijworks::SteeringActuator::Pins pins{};
    pins.motorEnable = MOTOR_ENABLE_PIN;
    pins.motor1Cw = MOTOR1_CW_PIN;
    pins.motor1Ccw = MOTOR1_CCW_PIN;
    pins.motor1Fault = MOTOR1_FAULT_PIN;
    pins.motor2Cw = UNUSED_ANALOG_PIN;
    pins.motor2Ccw = UNUSED_ANALOG_PIN;
    pins.motor2Fault = UNUSED_ANALOG_PIN;
    pins.motor1Current = UNUSED_ANALOG_PIN;
    pins.motor2Current = UNUSED_ANALOG_PIN;
    pins.wheelPosition = WHEEL_POSITION_PIN;
    pins.pressure = UNUSED_ANALOG_PIN;
    pins.encoder1A = ENCODER1_A_PIN;
    pins.encoder1B = ENCODER1_B_PIN;
    pins.encoder2A = ENCODER2_A_PIN;
    pins.encoder2B = ENCODER2_B_PIN;
    pins.eStop = SAFETY_INTERLOCK_PIN;
    pins.operatorPresent = SAFETY_INTERLOCK_PIN;
    pins.steerLimitLeft = SAFETY_INTERLOCK_PIN;
    pins.steerLimitRight = SAFETY_INTERLOCK_PIN;
    return pins;
}

InterfaceMotorcontroller::InterfaceMotorcontroller()
{
    actuator.SetPins(buildPins());
}

void InterfaceMotorcontroller::Begin()
{
    Serial.begin(115200);
    Serial.println("Motorcontroller v2.0");

    ReadConfig();
    actuator.Begin(buildPins());
    ApplySettingsToActuator();
}

void InterfaceMotorcontroller::Update(uint32_t nowMs)
{
    ParseSerial();

    // The legacy roboteq protocol only sends "!P" when the host wants to change the
    // setpoint, not continuously -- but SteeringActuator's comms-timeout watchdog
    // (kCommsTimeoutMs) expects a steady stream of commands, matching Salacia's
    // continuously-repeating UDP Command traffic. For a directly-wired serial board,
    // this interface's own Update() running every loop() iteration already *is* the
    // "still alive" signal -- so the last-known setpoint is resubmitted here every
    // tick instead of only on a fresh "!P", keeping the watchdog satisfied as long as
    // firmware is actually running.
    actuator.SetCommand(nowMs, buildMode(enabled), 0, setpoint, 0);
    actuator.Update(nowMs);
}

void InterfaceMotorcontroller::ApplySettingsToActuator()
{
    actuator.ApplySettings(buildMode(enabled), static_cast<uint8_t>(kp), static_cast<uint8_t>(ki),
                            static_cast<uint8_t>(kd), static_cast<uint8_t>(minPower), static_cast<uint8_t>(maxPower),
                            static_cast<uint8_t>(allowedError),
                            /* overpressure */ 255, /* overcurrent */ 255);
}

// ----------------
// SERIAL PARSER
// ----------------
void InterfaceMotorcontroller::ParseSerial()
{
    while (Serial.available())
    {
        const char c = static_cast<char>(Serial.read());
        if (echo)
        {
            Serial.write(c);
        }

        // Start decoding, split sentence into terms separated by " ", "\r", or "\n".
        switch (c)
        {
            case '^':
            case '!':
            case '?':
                termNumber = 0;
                termOffset = 0;
                term[termOffset++] = c;
                break;
            case ' ':
                term[termOffset] = '\0';
                ParseTerm();
                termNumber++;
                termOffset = 0;
                break;
            case '\r':
            case '\n':
                term[termOffset] = '\0';
                ParseTerm();
                termNumber = 0;
                termOffset = 0;
                break;
            default:
                if (termOffset < sizeof(term) - 1)
                {
                    term[termOffset++] = c;
                }
        }
    }
}

void InterfaceMotorcontroller::ParseTerm()
{
    if (termNumber == 0)
    {
        if (StrComp(term, "?ALLVARS"))
        {
            sentenceType = SentenceType::ReadAllVars;
            SendAllVars();
        }
        else if (StrComp(term, "?TRN"))
        {
            sentenceType = SentenceType::ReadControl;
            SendAnswer("TRN=SDC2XXX:SDC2160S");
        }
        else if (StrComp(term, "!EX"))
        {
            sentenceType = SentenceType::EmergencyShutdown;
            enabled = false;
            ApplySettingsToActuator();
            SendAnswer("+");
        }
        else if (StrComp(term, "!MG"))
        {
            sentenceType = SentenceType::ReleaseShutdown;
            enabled = true;
            ApplySettingsToActuator();
            SendAnswer("+");
        }
        else if (StrComp(term, "?FF"))
        {
            sentenceType = SentenceType::Fault;
            if (enabled)
            {
                SendAnswer("FF=0");
            }
            else
            {
                SendAnswer("FF=16");  // matches the roboteq protocol's "Emergency stop" code
            }
        }
        else if (StrComp(term, "^ECHOF"))
        {
            sentenceType = SentenceType::EchoOff;
        }
        else if (StrComp(term, "?C"))
        {
            sentenceType = SentenceType::ReadEncoder;
        }
        else if (StrComp(term, "!C"))
        {
            sentenceType = SentenceType::SetEncoder;
        }
        else if (StrComp(term, "!VAR"))
        {
            sentenceType = SentenceType::SetUserVar;
        }
        else if (StrComp(term, "!P"))
        {
            sentenceType = SentenceType::Goto;
        }
        else if (StrComp(term, "^ATRIG"))
        {
            sentenceType = SentenceType::SetAdjustTrigger;
        }
        else if (StrComp(term, "~ATRIG"))
        {
            sentenceType = SentenceType::ReadAdjustTrigger;
        }
    }
    else if (termNumber == 1)
    {
        parameter1 = ParseInteger(term);
        switch (sentenceType)
        {
            case SentenceType::EchoOff:
                if (parameter1 == 1)
                {
                    echo = false;
                    SendAnswer("+");
                }
                else if (parameter1 == 0)
                {
                    echo = true;
                    SendAnswer("+");
                }
                break;
            case SentenceType::ReadEncoder:
                if (parameter1 == 1)
                {
                    SendAnswer("C=", static_cast<int>(actuator.EncoderCount1()));
                }
                break;
            case SentenceType::ReadAdjustTrigger:
                if (parameter1 == 1)
                {
                    SendAnswer("~ATRIG=", errorFraction);
                }
                break;
            default:
                break;
        }
    }
    else if (termNumber == 2)
    {
        parameter2 = ParseInteger(term);
        switch (sentenceType)
        {
            case SentenceType::SetEncoder:
                if (parameter1 == 1)
                {
                    actuator.SetEncoderCount1(parameter2);
                    SendAnswer("+");
                }
                break;
            case SentenceType::SetUserVar:
                if (parameter1 == 0)
                {
                    uservar = parameter2;
                    SendAnswer("+");
                }
                else if (parameter1 == 1)
                {
                    kp = parameter2;
                    if (gFlashKp.read() != kp)
                    {
                        gFlashKp.write(kp);
                    }
                    ApplySettingsToActuator();
                    SendAnswer("+");
                }
                else if (parameter1 == 2)
                {
                    ki = parameter2;
                    if (gFlashKi.read() != ki)
                    {
                        gFlashKi.write(ki);
                    }
                    ApplySettingsToActuator();
                    SendAnswer("+");
                }
                else if (parameter1 == 3)
                {
                    kd = parameter2;
                    if (gFlashKd.read() != kd)
                    {
                        gFlashKd.write(kd);
                    }
                    ApplySettingsToActuator();
                    SendAnswer("+");
                }
                else if (parameter1 == 5)
                {
                    minPower = parameter2;
                    if (gFlashMinPower.read() != minPower)
                    {
                        gFlashMinPower.write(minPower);
                    }
                    ApplySettingsToActuator();
                    SendAnswer("+");
                }
                else if (parameter1 == 6)
                {
                    maxPower = parameter2;
                    if (gFlashMaxPower.read() != maxPower)
                    {
                        gFlashMaxPower.write(maxPower);
                    }
                    ApplySettingsToActuator();
                    SendAnswer("+");
                }
                else if (parameter1 == 7)
                {
                    errorFraction = parameter2;
                    if (gFlashErrorFraction.read() != errorFraction)
                    {
                        gFlashErrorFraction.write(errorFraction);
                    }
                    SendAnswer("+");
                }
                else if (parameter1 == 8)
                {
                    allowedError = parameter2;
                    if (gFlashAllowedError.read() != allowedError)
                    {
                        gFlashAllowedError.write(allowedError);
                    }
                    ApplySettingsToActuator();
                    SendAnswer("+");
                }
                break;
            case SentenceType::Goto:
                if (parameter1 == 1)
                {
                    setpoint = parameter2;
                    SendAnswer("+");
                }
                break;
            case SentenceType::SetAdjustTrigger:
                if (parameter1 == 1)
                {
                    SendAnswer("+");
                }
                break;
            default:
                break;
        }
    }
}

void InterfaceMotorcontroller::SendAllVars()
{
    Serial.print("VARS=");
    Serial.print(kp);
    Serial.print(",");
    Serial.print(ki);
    Serial.print(",");
    Serial.print(kd);
    Serial.print(",");
    Serial.print(minPower);
    Serial.print(",");
    Serial.print(maxPower);
    Serial.print(",");
    Serial.print(errorFraction);
    Serial.print(",");
    Serial.print(allowedError);
    Serial.print('\r');
}

void InterfaceMotorcontroller::SendAnswer(const char* reply)
{
    Serial.print(reply);
    Serial.print('\r');
}

void InterfaceMotorcontroller::SendAnswer(const char* reply, int value)
{
    Serial.print(reply);
    Serial.print(value);
    Serial.print('\r');
}

int InterfaceMotorcontroller::ParseInteger(const char* text)
{
    return atoi(text);
}

bool InterfaceMotorcontroller::StrComp(const char* a, const char* b)
{
    while (*a == *b)
    {
        if (!*a)
        {
            return true;
        }
        a++, b++;
    }
    return false;
}

void InterfaceMotorcontroller::ReadConfig()
{
    kp = gFlashKp.read();
    ki = gFlashKi.read();
    kd = gFlashKd.read();
    minPower = gFlashMinPower.read();
    maxPower = gFlashMaxPower.read();
    errorFraction = gFlashErrorFraction.read();
    allowedError = gFlashAllowedError.read();

    // Validate and fall back to defaults -- matches the legacy sketch's own
    // "0 or -1 (erased flash)" check.
    if (kp == 0 || kp == -1) kp = kKpDefault;
    if (ki == 0 || ki == -1) ki = kKiDefault;
    if (kd == 0 || kd == -1) kd = kKdDefault;
    if (minPower == 0 || minPower == -1) minPower = kMinPowerDefault;
    if (maxPower == 0 || maxPower == -1) maxPower = kMaxPowerDefault;
    if (errorFraction == 0 || errorFraction == -1) errorFraction = kErrorFractionDefault;
    if (allowedError == 0 || allowedError == -1) allowedError = kAllowedErrorDefault;
}
}
