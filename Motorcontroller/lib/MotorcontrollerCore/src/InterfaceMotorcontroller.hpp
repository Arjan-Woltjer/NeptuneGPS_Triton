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
// Ported from legacy Motorcontroller_v2.0gui.ino's roboteq-protocol serial parser (the
// most refined of the three legacy drafts -- flash-persisted, range-validated gains,
// an "?ALLVARS" convenience read, more tunable variables than v1_0/v1_0_Jupiter), but
// driving a meijworks::SteeringActuator instead of the legacy sketch's own inline
// encoder-position PID/slowstart/watchdog globals -- see Shared Firmware Libs/
// SteeringActuator's own header for why that control algorithm now lives there,
// shared with NeptuneGPS Salacia's AutosteerSource.
//
// This board is a single-motor design (Config::Electric1, fixed) -- see
// ConfigMotorcontroller.hpp for why (an 11-GPIO SAMD21 board can't wire
// SteeringActuator's full 18-pin feature set independently).
#pragma once

#include <Arduino.h>

#include <SteeringActuator.hpp>

namespace triton
{
class InterfaceMotorcontroller
{
public:
    InterfaceMotorcontroller();

    void Begin();
    void Update(uint32_t nowMs);

    // Exposed for the native test suite; not part of the intended external API otherwise.
    void ParseSerial();

private:
    enum class SentenceType : uint8_t
    {
        ReadControl,
        EmergencyShutdown,
        ReleaseShutdown,
        Fault,
        EchoOff,
        SetUserVar,
        ReadEncoder,
        SetEncoder,
        Goto,
        SetAdjustTrigger,
        ReadAdjustTrigger,
        ReadAllVars,
    };

    void ParseTerm();
    void SendAnswer(const char* reply);
    void SendAnswer(const char* reply, int value);
    void SendAllVars();
    static int ParseInteger(const char* text);
    static bool StrComp(const char* a, const char* b);

    // Rebuilds the actuator's Settings from this interface's own tracked gains/enabled
    // state -- called whenever any of those change, and every Update() tick (see
    // Update()'s own comment for why the setpoint is also resubmitted every tick).
    void ApplySettingsToActuator();

    // Every gain-set command writes flash immediately (matching the legacy sketch's own
    // per-command write pattern), so there's no separate "commit" step/method here.
    void ReadConfig();

    static meijworks::SteeringActuator::Pins buildPins();

    meijworks::SteeringActuator actuator;

    SentenceType sentenceType = SentenceType::ReadControl;
    bool enabled = false;
    bool echo = false;
    int uservar = 0;
    int32_t setpoint = 0;

    // Config items -- flash-persisted, matching the legacy sketch's own FlashStorage
    // fields. errorFraction has no SteeringActuator equivalent (it belonged to the
    // legacy sketch's own encoder-vs-pwm-delta watchdog math, replaced entirely by
    // SteeringActuator's own overcurrent/overpressure/interlock safety system) -- kept
    // as a locally-tracked, flash-persisted, but functionally inert value so the
    // roboteq wire protocol/Python tuner GUI's variable set doesn't shrink; allowedError
    // is forwarded to ApplySettings() but is equally inert on the SteeringActuator side
    // today (parsed, never consumed -- matches the Jupiter reference's own `sensor`
    // field, see SteeringActuator::ApplySettings()'s comment).
    int kp = 0;
    int ki = 0;
    int kd = 0;
    int minPower = 0;
    int maxPower = 0;
    int errorFraction = 0;
    int allowedError = 0;

    char term[10];
    unsigned int termNumber = 0;
    unsigned int termOffset = 0;
    int parameter1 = 0;
    int parameter2 = 0;
};
}
