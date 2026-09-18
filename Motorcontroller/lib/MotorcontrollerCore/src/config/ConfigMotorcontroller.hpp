/*
  ConfigMotorcontroller - pin assignments for the MeijWorks Motorcontroller board
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
#pragma once

#include <Arduino.h>

// Target board: Seeedstudio XIAO SAMD21 -- 11 GPIO pins total (D0-D10, all also usable
// as A0-A10), a hard physical ceiling meaningfully smaller than either of
// meijworks::SteeringActuator's real Teensy-based consumers (Salacia's Jupiter/Neptune
// 0.1 boards). SteeringActuator's Pins struct has 18 distinct roles (dual motor,
// current/pressure sensing, dual encoder, 4 independent safety interlocks); this board
// can't wire all of them independently, so this is confirmed as a **single-motor-only**
// board (Config::Electric1 only -- never Config::Electric2/Hydraulics/HydraulicBypass,
// which are the only configs that ever touch motor2's pins or check `pressure`):
//
// - motor1Cw/motor1Ccw/motorEnable/motor1Fault: the four pins this board's single motor
//   actually needs. motor1Cw/motor1Ccw reuse legacy Motorcontroller_v2.0gui's own
//   MOTOR_CW_1(5)/MOTOR_CCW_1(4) pins; motorEnable/motor1Fault repurpose the legacy
//   sketch's MOTOR_CW_2(9)/MOTOR_CCW_2(10) -- those drove a second, redundant PWM
//   channel for the same direction (a dual-half-bridge driver quirk SteeringActuator's
//   single-pin-per-direction model doesn't support), which is a real function this
//   board's driver needs anyway, not present in the legacy sketch at all.
// - encoder1A/encoder1B reuse the legacy sketch's own ENCODER_A(3)/ENCODER_B(8).
// - wheelPosition is new (A0/pin 0) -- SteeringActuator's PID always reads this for
//   real, unlike the legacy sketch's own encoder-position PID; this board has no prior
//   wiring for it.
// - The four safety interlocks (e-stop/operator-present/steer-limit-left/-right) share
//   ONE pin (1) -- confirmed with the user, matching Salacia's own AutosteerPins.hpp
//   precedent that DIN-style interlock inputs are electrically interchangeable. Real
//   loss of independent-interlock granularity versus Salacia's boards; see this
//   project's test/README "Known gaps".
// - Everything else this board doesn't have real hardware for but SteeringActuator
//   still needs *some* valid pin for (motor2Cw/motor2Ccw/motor2Fault/motor1Current/
//   motor2Current/pressure) shares ONE idle pin (2) -- harmless: motor2's pins are
//   never written outside a HydraulicBypass config this board never selects, and
//   motor1Current/motor2Current/pressure are read every tick but this project's
//   default Settings apply overcurrentThreshold=overpressureThreshold=255 (see
//   InterfaceMotorcontroller.cpp), which an 8-bit ADC reading can never exceed --
//   a deliberate null-effect, not an oversight.
// - encoder2A/encoder2B each get their OWN idle pin (6, 7) rather than sharing pin 2
//   with the rest: SteeringActuator::Begin() unconditionally attachInterrupt()s
//   encoder2A regardless of config, so sharing it with an actively-toggling pin (the
//   safety loop, or anything on the shared idle pin 2) would fire spurious encoder2
//   edges. Two more genuinely idle pins avoids that, even though EncoderCount2() is
//   never read by this board's serial protocol either.
//
// Uses all 11 of the board's pins (0-10).

#define MOTOR1_CW_PIN            5
#define MOTOR1_CCW_PIN           4
#define MOTOR_ENABLE_PIN         9
#define MOTOR1_FAULT_PIN         10
#define ENCODER1_A_PIN           3
#define ENCODER1_B_PIN           8
#define WHEEL_POSITION_PIN       0

#define SAFETY_INTERLOCK_PIN     1  // e-stop / operator-present / steer-limit-left / steer-limit-right, shared

#define UNUSED_ANALOG_PIN        2  // motor2Cw/Ccw/Fault, motor1/2Current, pressure

#define ENCODER2_A_PIN           6  // idle -- see the class comment above
#define ENCODER2_B_PIN           7  // idle -- see the class comment above
