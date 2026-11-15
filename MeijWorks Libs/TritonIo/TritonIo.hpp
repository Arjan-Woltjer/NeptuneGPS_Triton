/*
  TritonIo - input shim for the Triton boards: GPIO on every board, the
  MCP23008 I2C expander that carries most inputs on Triton01, and the
  ADS1115 that carries the analog inputs on the AgOpenShop ESP32 rate board.
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

// For a board that has no input stage for an operator input at all: a pin
// table entry of PIN_ALWAYS_ON reads energised, PIN_ALWAYS_OFF idle, on every
// board and whatever the input polarity. The AgOpenShop rate board uses them
// for the mode switch (always on: the work switch alone decides) and the
// narrow/wide buttons (the VT has its own).
#define PIN_ALWAYS_ON  0xFE
#define PIN_ALWAYS_OFF 0xFF

namespace triton
{

constexpr uint8_t kExpanderPinBase = 100;

inline bool IsFixedPin(uint8_t pin) { return pin == PIN_ALWAYS_ON || pin == PIN_ALWAYS_OFF; }
inline bool IsExpanderPin(uint8_t pin) { return pin >= kExpanderPinBase && pin < kExpanderPinBase + 8; }

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
    // No expander-backed input configured (the rate board has no MCP23008):
    // nothing to poll, and no bus traffic to an absent device every pass.
    if (!gExpanderBegun) {
        return;
    }
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
    if (IsFixedPin(pin)) {
        return;
    }
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
    if (IsFixedPin(pin)) {
        return pin == PIN_ALWAYS_ON;
    }
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

// ---------------------------------------------------------------------------
// Analog inputs. Every board but the rate board reads the MCU's own ADC; the
// rate board (-D ESP32RC) has its analog terminals on an ADS1115 at 0x48 on
// the I2C bus, addressed in the pin tables as ADS1115_PIN(ch), ch = AIN0-3.
// Either way the result is on the 0-1023 scale the calibration tables use.
// ---------------------------------------------------------------------------
#define ADS1115_PIN(ch) (120 + (ch))

#if defined(ESP32RC)

constexpr uint8_t kAdsPinBase = 120;
constexpr uint8_t kAdsAddress = 0x48;

inline bool IsAdsPin(uint8_t pin) { return pin >= kAdsPinBase && pin < kAdsPinBase + 4; }

// The channel the ADS1115 is converting continuously (-1: not configured
// yet), and the last good reading per channel, returned when the bus fails
// so a hiccup holds the position instead of inventing a jump.
inline int8_t    gAdsChannel = -1;
inline short int gAdsLast[4] = {0, 0, 0, 0};

namespace detail
{
// Config register: continuous conversion of AINch against GND, +-6.144 V
// full scale (a 5 V sensor fits), 860 samples/s, comparator off.
inline bool AdsSelectChannel(uint8_t ch) {
    const uint16_t config = static_cast<uint16_t>(((0x4u | ch) << 12) | (0x0u << 9) | (0x0u << 8) | (0x7u << 5) | 0x3u);
    Wire.beginTransmission(kAdsAddress);
    Wire.write(0x01);
    Wire.write(static_cast<uint8_t>(config >> 8));
    Wire.write(static_cast<uint8_t>(config & 0xFF));
    return Wire.endTransmission() == 0;
}
}  // namespace detail

// Continuous mode keeps a read to one register fetch (~0.4 ms at 100 kHz)
// instead of a ~1.5 ms single-shot wait in every loop pass. Only a change of
// channel (with -D ROTATION, two sensors) waits for a fresh conversion.
inline short int ReadAds(uint8_t ch) {
    if (static_cast<int8_t>(ch) != gAdsChannel) {
        if (!detail::AdsSelectChannel(ch)) {
            return gAdsLast[ch];
        }
        gAdsChannel = static_cast<int8_t>(ch);
        delay(3);   // first conversion at 860 SPS: 1.2 ms, plus wake-up
    }
    Wire.beginTransmission(kAdsAddress);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) {
        return gAdsLast[ch];
    }
    if (Wire.requestFrom(kAdsAddress, static_cast<uint8_t>(2)) != 2) {
        return gAdsLast[ch];
    }
    const uint8_t hi = static_cast<uint8_t>(Wire.read());
    const uint8_t lo = static_cast<uint8_t>(Wire.read());
    long raw = static_cast<int16_t>((hi << 8) | lo);
    if (raw < 0) {
        raw = 0;
    }
    // 0.1875 mV per count: 5 V = 26667 counts -> 1023, as on the Teensy boards.
    long scaled = raw * 1023L / 26667L;
    if (scaled > 1023) {
        scaled = 1023;
    }
    gAdsLast[ch] = static_cast<short int>(scaled);
    return gAdsLast[ch];
}

#endif  // ESP32RC

// Wire must already be running for an ADS1115 pin (the LCD's Begin() starts
// it in main.cpp, before the implement is constructed).
inline void ConfigureAnalogInput(uint8_t pin) {
#if defined(ESP32RC)
    if (IsAdsPin(pin)) {
        return;
    }
#endif
    pinMode(pin, INPUT);
    digitalWrite(pin, LOW);
}

inline short int ReadAnalog(uint8_t pin) {
#if defined(ESP32RC)
    if (IsAdsPin(pin)) {
        return ReadAds(pin - kAdsPinBase);
    }
#endif
    return analogRead(pin);
}

}  // namespace triton
