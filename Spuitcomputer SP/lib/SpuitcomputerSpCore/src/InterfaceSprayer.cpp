/*
  InterfaceSprayer - a library for the MeijWorks sprayer interface
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
#include "InterfaceSprayer.hpp"
#include "LcdNumber.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfaceSprayer::InterfaceSprayer(InterfaceI2CLCD* lcd, ImplementSprayer* implement, VehicleSensors* tractor) {
    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON, INPUT);
    pinMode(RIGHT_BUTTON, INPUT);
    pinMode(MODE_PIN, INPUT);

    digitalWrite(LEFT_BUTTON, LOW);
    digitalWrite(RIGHT_BUTTON, LOW);
    digitalWrite(MODE_PIN, LOW);

    // Mode
    mode = 2;  // Off

    // Button flags and timers
    buttons = 0;
    buttonPair.Begin(0);

    // Connected classes
    this->lcd = lcd;
    this->implement = implement;
    this->tractor = tractor;

    simTime = millis();
}

// ------------------------
// Method for updating mode
// ------------------------
void InterfaceSprayer::Update() {
    // Check buttons
    CheckButtons(byte(255), 0);

    // Mode Auto = 0
    // Mode Start = 1 (waiting for min speed)
    // Mode Off  = 2
    // Mode Calibrate = 3 (never assigned here -- see GetMode()'s comment)
    // Mode Sim  = 4

    // ---
    // Off
    // ---
    // GetHitch() is true with the hitch up, after the inversion the
    // calibration wizard sets (VehicleSensors; inverted by default on the
    // sprayer, which used to hard-wire it here as !GetHitch()).
    if (!digitalRead(MODE_PIN) || tractor->GetHitch()) {
        mode = 2;
        simTime = millis();
    }
    else {
        //----
        // Sim
        //----
        if ((!tractor->SimSpeed() && millis() - simTime < tractor->GetSimTime() * 1000) ||
            tractor->GetSim()) {
            mode = 4;
        }
        //----------
        // Automatic
        //----------
        else {
            mode = 0;
        }
    }

    // Update tractor (speed and hitch)
    tractor->Update(mode);

    // Update implement and adjust
    implement->Update(mode, buttons);

    // Update screen (no rewrite) and write one character
    UpdateScreen(0);
    lcd->WriteScreen(1);
}

// --------------------------
// Method for updating screen
// --------------------------
void InterfaceSprayer::UpdateScreen(boolean rewrite) {
    int temp2 = 0;

    // Update screen
    if (rewrite) {
        // Regel 0
        lcd->WriteBuffer(L_FLOW, 0);

        // Regel 1
        lcd->WriteBuffer(L_MEASURE, 1);

        // Regel 2
        lcd->WriteBuffer(L_SPEED, 2);

        // Regel 3
        lcd->WriteBuffer(L_BLANK, 3);

        lcd->WriteScreen(-1);
    }

    // Regel 0
    LcdWriteNumber(lcd, implement->GetDose(), 0, 12, 3);

#ifdef TEENSYPROTO
    // Remote pump-selection panel (see ImplementSprayer::Update()'s Serial1
    // bitmask read) shows how many of the pumps are currently selected on.
    // Dead on teensy41 -- TEENSYPROTO is a historical protoboard variant.
    int temp = implement->GetPumpsOn();
    lcd->WriteBuffer(temp + '0', 0, 9);
#endif

    // Regel 1
    LcdWriteNumber(lcd, implement->GetActualDose(), 1, 12, 3);

    // Regel 2
    LcdWriteNumber(lcd, tractor->GetSpeedKmh(), 2, 10, 5, 1, '.', false);

    // Regel 3 -- alternates between volume sprayed and area covered every
    // 5 seconds (ImplementSprayer::updateFlag flips on that cadence).
    if (implement->GetFlag()) {
        temp2 = implement->GetVolume();
        lcd->WriteBuffer("Volume:        L    ", 3);
    }
    else {
        temp2 = (tractor->GetDistance() * implement->GetWidth()) / 10;
        lcd->WriteBuffer("HA:            H    ", 3);
    }
    LcdWriteNumber(lcd, temp2, 3, 7, 7, 2, '.', false);

    switch (mode) {
        case 0:  // AUTO
            lcd->WriteBuffer('A', 3, 17);
            lcd->WriteBuffer('U', 3, 18);
            lcd->WriteBuffer('T', 3, 19);
            break;
        case 1:  // START
            lcd->WriteBuffer('S', 3, 17);
            lcd->WriteBuffer(' ', 3, 18);
            lcd->WriteBuffer(' ', 3, 19);
            break;
        case 2:  // OFF
            lcd->WriteBuffer('O', 3, 17);
            lcd->WriteBuffer('F', 3, 18);
            lcd->WriteBuffer('F', 3, 19);
            break;
        case 4:  // SIM
            lcd->WriteBuffer('S', 3, 17);
            lcd->WriteBuffer('I', 3, 18);
            lcd->WriteBuffer('M', 3, 19);
            break;
    }
}

// ---------------------------
// Method for checking buttons
// ---------------------------
int InterfaceSprayer::CheckButtons(byte delay1, byte delay2) {
    // MeijWorks Libs/ButtonPair: debounce, hold-to-repeat and both held.
    buttons = buttonPair.Check(digitalRead(LEFT_BUTTON), digitalRead(RIGHT_BUTTON), delay1, delay2, millis());
    return buttons;
}

}  // namespace triton
