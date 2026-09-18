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

namespace triton
{

// -----------
// Constructor
// -----------
InterfaceSprayer::InterfaceSprayer(InterfaceI2CLCD* lcd, ImplementSprayer* implement, VehicleTractor* tractor) {
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
    button1Flag = false;
    button2Flag = false;
    button1Timer = 0;
    button2Timer = 0;

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
    if (!digitalRead(MODE_PIN) || !tractor->GetHitch()) {
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
    int temp = 0;
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
    temp = implement->GetDose();

    if (temp > 99) {
        lcd->WriteBuffer(temp / 100 + '0', 0, 12);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 0, 13);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 14);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 0, 12);
        lcd->WriteBuffer(temp / 10 + '0', 0, 13);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 14);
    }
    else {
        lcd->WriteBuffer(' ', 0, 12);
        lcd->WriteBuffer(' ', 0, 13);
        lcd->WriteBuffer(temp + '0', 0, 14);
    }

#ifdef TEENSYPROTO
    // Remote pump-selection panel (see ImplementSprayer::Update()'s Serial1
    // bitmask read) shows how many of the pumps are currently selected on.
    // Dead on teensy41 -- TEENSYPROTO is a historical protoboard variant.
    temp = implement->GetPumpsOn();
    lcd->WriteBuffer(temp + '0', 0, 9);
#endif

    // Regel 1
    temp = implement->GetActualDose();

    if (temp > 99) {
        lcd->WriteBuffer(temp / 100 + '0', 1, 12);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 1, 13);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 1, 14);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 1, 12);
        lcd->WriteBuffer(temp / 10 + '0', 1, 13);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 1, 14);
    }
    else {
        lcd->WriteBuffer(' ', 1, 12);
        lcd->WriteBuffer(' ', 1, 13);
        lcd->WriteBuffer(temp + '0', 1, 14);
    }

    // Regel 2
    temp2 = tractor->GetSpeedKmh();
    temp = abs(temp2);

    if (temp > 99) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 2, 10);
        }
        else {
            lcd->WriteBuffer(' ', 2, 10);
        }
        lcd->WriteBuffer(temp / 100 + '0', 2, 11);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 2, 12);
        temp = temp % 10;
        lcd->WriteBuffer('.', 2, 13);
        lcd->WriteBuffer(temp + '0', 2, 14);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 2, 10);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 2, 11);
        }
        else {
            lcd->WriteBuffer(' ', 2, 11);
        }
        lcd->WriteBuffer(temp / 10 + '0', 2, 12);
        temp = temp % 10;
        lcd->WriteBuffer('.', 2, 13);
        lcd->WriteBuffer(temp + '0', 2, 14);
    }
    else {
        lcd->WriteBuffer(' ', 2, 10);
        lcd->WriteBuffer(' ', 2, 11);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 2, 12);
        }
        else {
            lcd->WriteBuffer(' ', 2, 12);
        }
        lcd->WriteBuffer('.', 2, 13);
        lcd->WriteBuffer(temp + '0', 2, 14);
    }

    // Regel 3 -- alternates between volume sprayed and area covered every
    // 5 seconds (ImplementSprayer::updateFlag flips on that cadence).
    if (implement->GetFlag()) {
        temp2 = implement->GetVolume();
        temp = abs(temp2);
        lcd->WriteBuffer("Volume:        L    ", 3);
    }
    else {
        temp2 = (tractor->GetDistance() * implement->GetWidth()) / 10;
        temp = abs(temp2);
        lcd->WriteBuffer("HA:            H    ", 3);
    }

    if (temp2 < 0) {
        lcd->WriteBuffer('-', 3, 7);
    }
    if (temp > 9999) {
        lcd->WriteBuffer(temp / 10000 + '0', 3, 8);
        temp = temp % 10000;
        lcd->WriteBuffer(temp / 1000 + '0', 3, 9);
        temp = temp % 1000;
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else if (temp > 999) {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(temp / 1000 + '0', 3, 9);
        temp = temp % 1000;
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else if (temp > 99) {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(' ', 3, 10);
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer(temp / 10 + '0', 3, 12);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 13);
    }
    else {
        lcd->WriteBuffer(' ', 3, 8);
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(' ', 3, 10);
        lcd->WriteBuffer('.', 3, 11);
        lcd->WriteBuffer('0', 3, 12);
        lcd->WriteBuffer(temp + '0', 3, 13);
    }

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
    if (button1Flag) {
        button1Timer = millis();
        button1Flag = false;
    }

    if (button2Flag) {
        button2Timer = millis();
        button2Flag = false;
    }

    // Check for left/right button presses
    if (digitalRead(LEFT_BUTTON) && digitalRead(RIGHT_BUTTON)) {
        if (millis() - button1Timer >= delay1 * 4) {
            button1Flag = true;
            buttons = 2;
            return 2;
        }
        else {
            button2Flag = true;
            buttons = 0;
            return 0;
        }
    }
    else if (digitalRead(LEFT_BUTTON)) {
        if (millis() - button2Timer >= delay2) {
            button2Flag = true;
            buttons = -1;
            return -1;
        }
        else {
            button1Flag = true;
            buttons = 0;
            return 0;
        }
    }
    else if (digitalRead(RIGHT_BUTTON)) {
        if (millis() - button2Timer >= delay2) {
            button2Flag = true;
            buttons = 1;
            return 1;
        }
        else {
            button1Flag = true;
            buttons = 0;
            return 0;
        }
    }
    else {
        button1Flag = true;
        button2Flag = true;
        buttons = 0;
        return 0;
    }
}

}  // namespace triton
