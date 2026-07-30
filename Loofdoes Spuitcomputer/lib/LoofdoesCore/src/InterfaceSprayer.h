/*
  InterfaceSprayer - a library for the MeijWorks loofdoes interface
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

#if defined(ARDUINO) || defined(EPOXY_DUINO)
#include <Arduino.h>
#else
#include <stdint.h>
typedef uint8_t byte;
class Stream;
unsigned long millis();
bool digitalRead(uint8_t pin);
int  analogRead(uint8_t pin);
#endif

#define INTERFACE_VERSION 0.2

#define NUM_DIGITAL_IN 4

#define IN1 16
#define IN2 17
#define IN3 18
#define IN4 19

#define NUM_ANALOG_IN 1

#define ANALOG_IN1 15

struct DigitalInputState {
    uint8_t       pin;
    bool          state;
    bool          flag;
    unsigned long timer;
};

struct AnalogInputState {
    uint8_t pin;
    int     value;
};

class InterfaceSprayer {
private:
    Stream* serialDebug;

public:
    // Exposed for testing; use GetDigitalInputs() in production code
    DigitalInputState buttons[NUM_DIGITAL_IN] = {
        { IN1, false, false, 0 },
        { IN2, false, false, 0 },
        { IN3, false, false, 0 },
        { IN4, false, false, 0 },
    };

    AnalogInputState analogInputs[NUM_ANALOG_IN] = {
        { ANALOG_IN1, 0 },
    };

    InterfaceSprayer(Stream* serialDebug = nullptr);

    void Update();
    void CheckDigitalInputs(byte delay);
    void CheckAnalogInputs();

    inline DigitalInputState* GetDigitalInputs() { return buttons; }
    inline AnalogInputState*  GetAnalogInputs()  { return analogInputs; }
};