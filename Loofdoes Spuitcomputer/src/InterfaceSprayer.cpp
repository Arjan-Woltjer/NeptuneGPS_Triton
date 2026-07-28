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
#include "InterfaceSprayer.h"

InterfaceSprayer::InterfaceSprayer(Stream* serialDebug)
    : serialDebug(serialDebug) {
#ifdef DEBUG
    if (serialDebug) {
        serialDebug->println("------------------------------");
        serialDebug->println("Initialising sprayer interface");
        serialDebug->println("------------------------------");
    }
#endif

    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        DigitalInputState& button = buttons[i];
#if defined(ARDUINO) || defined(EPOXY_DUINO)
        pinMode(button.pin, INPUT_PULLUP);
#endif
        button.flag  = true;
        button.state = false;
        button.timer = 0;
    }

    for (int i = 0; i < NUM_ANALOG_IN; ++i) {
        AnalogInputState& analogInput = analogInputs[i];
#if defined(ARDUINO) || defined(EPOXY_DUINO)
        pinMode(analogInput.pin, INPUT);
#endif
        analogInput.value = 0;
    }
}

void InterfaceSprayer::Update() {
    CheckDigitalInputs(25);
    CheckAnalogInputs();
}

// Sets button.state = true for each button held >= delay ms, false otherwise.
// Timer resets when a button is released, so each new press is timed fresh.
void InterfaceSprayer::CheckDigitalInputs(byte delay) {
    const unsigned long now = millis();

    for (int i = 0; i < NUM_DIGITAL_IN; ++i) {
        DigitalInputState& button = buttons[i];

        if (digitalRead(button.pin)) {
            if (!button.flag && serialDebug) {  // rising edge
                serialDebug->print("Button pin ");
                serialDebug->print(button.pin);
                serialDebug->println(": HIGH");
            }
            button.flag  = true;
            button.timer = now;
            button.state = false;
        }
        else {
            if (button.flag) {  // falling edge
                if (serialDebug) {
                    serialDebug->print("Button pin ");
                    serialDebug->print(button.pin);
                    serialDebug->println(": LOW");
                }
                button.flag = false;
            }
            button.state = (now - button.timer) >= (unsigned long)delay;
        }
    }
}

void InterfaceSprayer::CheckAnalogInputs() {
    for (int i = 0; i < NUM_ANALOG_IN; ++i) {
        AnalogInputState& analogInput = analogInputs[i];
        analogInput.value = (analogInput.value + analogRead(analogInput.pin)) / 2;
    }
}