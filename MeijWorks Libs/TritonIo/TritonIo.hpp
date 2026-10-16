/*
  TritonIo - digital-input shim for the Triton boards: GPIO on every board,
  plus the MCP23008 I2C expander that carries most inputs on Triton01.
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
#if defined(ESP32S3)
#include <Wire.h>
#endif

// Header-only on purpose: the GPIO path compiles to the plain pinMode()/
// digitalRead() the boards have always used (so the native test stubs keep
// working unchanged), and only an ESP32S3 build pulls in the expander code.
//
// Pin numbering: a value below kExpanderPinBase is a GPIO number; a value of
// kExpanderPinBase + n is GPn of the MCP23008. EXPANDER_PIN(n) is the macro
// form for the Config*.hpp pin tables.
#define EXPANDER_PIN(gp) (100 + (gp))

namespace triton
{

constexpr uint8_t kExpanderPinBase = 100;

inline bool IsExpanderPin(uint8_t pin) { return pin >= kExpanderPinBase; }

#if defined(ESP32S3)

// Every digital input on Triton01 (DIN1-8) is an opto-coupler collector with
// a pull-up: it reads LOW when the input is energised. The besturing 0.1
// input stage is non-inverting, and the firmware was written against it, so
// the reads below are inverted here and the callers keep "true = energised".
constexpr bool    kDigitalInputsActiveLow = true;

constexpr uint8_t kExpanderAddress  = 0x20;   // A0-A2 to GND on Triton01
constexpr uint8_t kExpanderRegIodir = 0x00;
constexpr uint8_t kExpanderRegGppu  = 0x06;
constexpr uint8_t kExpanderRegGpio  = 0x09;

// Last GPIO register image; all ones = every input idle (pulled up). Refreshed
// once per main-loop iteration by RefreshExpanderInputs(), so the reads are
// plain bit tests and a bus hiccup keeps the previous state instead of
// inventing a press.
inline uint8_t gExpanderInputs = 0xFF;
inline bool    gExpanderBegun  = false;

namespace detail
{
inline void ExpanderWrite(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(kExpanderAddress);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}
}  // namespace detail

inline void RefreshExpanderInputs() {
    Wire.beginTransmission(kExpanderAddress);
    Wire.write(kExpanderRegGpio);
    if (Wire.endTransmission(false) != 0) {
        return;
    }
    if (Wire.requestFrom(kExpanderAddress, static_cast<uint8_t>(1)) == 1) {
        gExpanderInputs = static_cast<uint8_t>(Wire.read());
    }
}

// Wire must already be running (InterfaceI2CLCD::Begin() does that in
// main.cpp before any input is configured). All eight GPs become inputs
// with pull-ups: the six wired ones already have a 10k on the board, the two
// unwired ones (GP6/GP7) need the internal pull-up to read idle.
inline void BeginExpander() {
    if (gExpanderBegun) {
        return;
    }
    detail::ExpanderWrite(kExpanderRegIodir, 0xFF);
    detail::ExpanderWrite(kExpanderRegGppu, 0xFF);
    gExpanderBegun = true;
    RefreshExpanderInputs();
}

#else

constexpr bool kDigitalInputsActiveLow = false;

inline void RefreshExpanderInputs() {}

#endif  // ESP32S3

// Configures one digital input. "pullup" means the board wants the MCU's
// pull-up on that pin (the Teensy boards do, the Arduino ones do not).
inline void ConfigureDigitalInput(uint8_t pin, bool pullup) {
#if defined(ESP32S3)
    if (IsExpanderPin(pin)) {
        BeginExpander();
        return;
    }
    pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
#else
    // The AVR/Teensy idiom every existing board was brought up with: an
    // INPUT pin written HIGH enables its pull-up.
    pinMode(pin, INPUT);
    digitalWrite(pin, pullup ? HIGH : LOW);
#endif
}

// Reads one digital input; true when the input is energised.
inline bool ReadDigital(uint8_t pin) {
    bool raw;
#if defined(ESP32S3)
    if (IsExpanderPin(pin)) {
        raw = ((gExpanderInputs >> (pin - kExpanderPinBase)) & 0x01) != 0;
    } else {
        raw = digitalRead(pin) != 0;
    }
#else
    raw = digitalRead(pin) != 0;
#endif
    return raw != kDigitalInputsActiveLow;
}

}  // namespace triton
