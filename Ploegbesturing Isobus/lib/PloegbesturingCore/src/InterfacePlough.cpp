/*
  InterfacePlough - a library for the MeijWorks plough interface
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
#include "InterfacePlough.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfacePlough::InterfacePlough(Stream* serialDebug,
                                  InterfaceI2CLCD* lcd,
                                  ImplementPlough* implement,
                                  VehicleTractor* tractor,
                                  IsobusGuidanceSource* guidance) {
    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON_2, INPUT);
    digitalWrite(LEFT_BUTTON_2, LOW);
    pinMode(RIGHT_BUTTON_2, INPUT);
    digitalWrite(RIGHT_BUTTON_2, LOW);
    pinMode(MODE_PIN_2, INPUT);
    digitalWrite(MODE_PIN_2, LOW);
    pinMode(JOY_LEFT_2, INPUT);
    digitalWrite(JOY_LEFT_2, LOW);
    pinMode(JOY_RIGHT_2, INPUT);
    digitalWrite(JOY_RIGHT_2, LOW);
    pinMode(JOY_MODE_2, INPUT);
    digitalWrite(JOY_MODE_2, LOW);

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer. button1Timer/button2Timer were never initialized
    // here in the legacy code (a real latent bug -- CheckButtons()'s very
    // first call would measure elapsed time against garbage memory); seeded
    // to millis() now, matching how ImplementPlough seeds its own timers.
    buttons = 0;
    button1Flag = false;
    button2Flag = false;
    button1Timer = millis();
    button2Timer = millis();

    // Connected classes
    this->serialDebug = serialDebug;
    this->lcd = lcd;
    this->implement = implement;
    this->tractor = tractor;
    this->guidance = guidance;
}

// ------------------------
// Method for updating mode
// ------------------------
void InterfacePlough::Update() {
    // Check buttons
    CheckButtons(255, 0);

    // ============================
    // Process tractor
    // ============================
    // (Guidance data now arrives asynchronously via IsobusGuidanceChannel's
    // PGN callbacks, pumped from main.cpp's loop() -- there is no serial
    // parser left to poll here.)
    tractor->Update(mode);

    // ====================
    // Process mode changes
    // ====================

    // ------
    // Manual
    // ------
    if (!digitalRead(MODE_PIN_2) ||
        //digitalRead(JOY_MODE_2) ||
        tractor->GetHitch()) {
        // set mode to manual
        mode = 2;

#ifdef DEBUG
        serialDebug->println("M");
#endif
    }

    // ----
    // Hold
    // ----
    else if (millis() - guidance->GetGgaFixAge() > 2000 ||
             millis() - guidance->GetVtgFixAge() > 2000 ||
             millis() - guidance->GetXteFixAge() > 2000 ||
             !guidance->IsRtkQuality() ||
             !guidance->MinSpeed()
            ) {
        // set mode to hold
        mode = 1;

#ifdef DEBUG
        serialDebug->println("H");
#endif
    }

    // ---------
    // Automatic
    // ---------
    else {
        // set mode to automatic
        mode = 0;

#ifdef DEBUG
        serialDebug->println("A");
#endif
    }

    // Update implement and adjust
    implement->Update(mode, buttons);
    implement->Adjust(mode, buttons);

    // Update screen (no rewrite) and write one character
    UpdateScreen(0);
    lcd->WriteScreen(1);
}

// --------------------------
// Method for updating screen
// --------------------------
void InterfacePlough::UpdateScreen(boolean rewrite) {
    short int temp = 0;
    short int temp2 = 0;

    // Update screen
    if (rewrite) {
        // Regel 0
        lcd->WriteBuffer(L2_POS, 0);

        // Regel 1
        lcd->WriteBuffer(L2_A_POS, 1);

        // Regel 2
        lcd->WriteBuffer(L2_XTE, 2);

#ifdef ROTATION
        // Regel 3
        lcd->WriteBuffer(L2_ROTATION, 3);
#else
        lcd->WriteBuffer(L2_BLANK, 3);
#endif

        lcd->WriteScreen(-1);
    }

    // Regel 0
    temp2 = implement->GetOffset();
    temp = abs(temp2);

    if (temp > 99) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 0, 16);
        }
        else {
            lcd->WriteBuffer(' ', 0, 16);
        }
        lcd->WriteBuffer(temp / 100 + '0', 0, 17);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 0, 18);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 19);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 0, 16);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 0, 17);
        }
        else {
            lcd->WriteBuffer(' ', 0, 17);
        }
        lcd->WriteBuffer(temp / 10 + '0', 0, 18);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 19);
    }
    else {
        lcd->WriteBuffer(' ', 0, 16);
        lcd->WriteBuffer(' ', 0, 17);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 0, 18);
        }
        else {
            lcd->WriteBuffer(' ', 0, 18);
        }
        lcd->WriteBuffer(temp + '0', 0, 19);
    }

    // Regel 1
    temp2 = implement->GetPosition();
    temp = abs(temp2);

    if (temp > 99) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 1, 16);
        }
        else {
            lcd->WriteBuffer(' ', 1, 16);
        }
        lcd->WriteBuffer(temp / 100 + '0', 1, 17);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 1, 18);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 1, 19);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 1, 16);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 1, 17);
        }
        else {
            lcd->WriteBuffer(' ', 1, 17);
        }
        lcd->WriteBuffer(temp / 10 + '0', 1, 18);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 1, 19);
    }
    else {
        lcd->WriteBuffer(' ', 1, 16);
        lcd->WriteBuffer(' ', 1, 17);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 1, 18);
        }
        else {
            lcd->WriteBuffer(' ', 1, 18);
        }
        lcd->WriteBuffer(temp + '0', 1, 19);
    }


    // Regel 2
    temp2 = guidance->GetXte();
    temp = abs(temp2);

    if (temp > 99) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 2, 16);
        }
        else {
            lcd->WriteBuffer(' ', 2, 16);
        }
        lcd->WriteBuffer(temp / 100 + '0', 2, 17);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 2, 18);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 2, 19);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 2, 16);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 2, 17);
        }
        else {
            lcd->WriteBuffer(' ', 2, 17);
        }
        lcd->WriteBuffer(temp / 10 + '0', 2, 18);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 2, 19);
    }
    else {
        lcd->WriteBuffer(' ', 2, 16);
        lcd->WriteBuffer(' ', 2, 17);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 2, 18);
        }
        else {
            lcd->WriteBuffer(' ', 2, 18);
        }
        lcd->WriteBuffer(temp + '0', 2, 19);
    }

#ifdef ROTATION
    // Regel  3
    temp2 = implement->GetRotation();
    temp = abs(temp2);

    if (temp > 99) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 9);
        }
        else {
            lcd->WriteBuffer(' ', 3, 9);
        }
        lcd->WriteBuffer(temp / 100 + '0', 3, 10);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 3, 11);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 12);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 3, 9);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 10);
        }
        else {
            lcd->WriteBuffer(' ', 3, 10);
        }
        lcd->WriteBuffer(temp / 10 + '0', 3, 11);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 12);
    }
    else {
        lcd->WriteBuffer(' ', 3, 9);
        lcd->WriteBuffer(' ', 3, 10);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 11);
        }
        else {
            lcd->WriteBuffer(' ', 3, 11);
        }
        lcd->WriteBuffer(temp + '0', 3, 12);
    }
#endif

    if (implement->GetSide()) {
        lcd->WriteBuffer('L', 3, 15);
    }
    else {
        lcd->WriteBuffer('R', 3, 15);
    }

    switch (mode) {
        case 0: // AUTO
            lcd->WriteBuffer('A', 3, 14);
            lcd->WriteBuffer(' ', 3, 17);
            lcd->WriteBuffer(' ', 3, 18);
            break;
        case 1: // HOLD
            lcd->WriteBuffer('H', 3, 14);
            if (!guidance->MinSpeed() && millis() - guidance->GetVtgFixAge() < 2000) {
                lcd->WriteBuffer('S', 3, 17);
                lcd->WriteBuffer('!', 3, 18);
            }
            else {
                lcd->WriteBuffer('G', 3, 17);
                lcd->WriteBuffer('!', 3, 18);
            }
            break;
        case 2: // MANUAL
            lcd->WriteBuffer('M', 3, 14);

            if (buttons == -1) {
                lcd->WriteBuffer('<', 3, 17);
                lcd->WriteBuffer(' ', 3, 18);
            }
            else if (buttons == 1) {
                lcd->WriteBuffer(' ', 3, 17);
                lcd->WriteBuffer('>', 3, 18);
            }
            else {
                lcd->WriteBuffer(' ', 3, 17);
                lcd->WriteBuffer(' ', 3, 18);
            }
            break;
    }
}

// ---------------------------
// Method for checking buttons
// ---------------------------
short int InterfacePlough::CheckButtons(byte delay1, byte delay2) {
    if (button1Flag) {
        button1Timer = millis();
        button1Flag = false;
    }

    if (button2Flag) {
        button2Timer = millis();
        button2Flag = false;
    }

    // Check for left/right button presses
    if (digitalRead(LEFT_BUTTON_2) && digitalRead(RIGHT_BUTTON_2)) {
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
    else if (digitalRead(LEFT_BUTTON_2)) { // || digitalRead(JOY_LEFT_2)){
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
    else if (digitalRead(RIGHT_BUTTON_2)) { // || digitalRead(JOY_RIGHT_2)){
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
