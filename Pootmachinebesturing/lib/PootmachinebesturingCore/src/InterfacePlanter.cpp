/*
  InterfacePlanter - a library for the MeijWorks plantercontrol interface
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
#include "InterfacePlanter.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfacePlanter::InterfacePlanter(Stream* serialDebug,
                                    InterfaceI2CLCD* lcd,
                                    ImplementPlanter* implement,
                                    VehicleTractor* tractor,
                                    GuidanceSource* guidance) {
#ifdef DEBUG
    serialDebug->println(S_DIVIDE);
    serialDebug->println("Initialising planter interface");
    serialDebug->println(S_DIVIDE);
#endif

    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON_3, INPUT);
    digitalWrite(LEFT_BUTTON_3, LOW);
    pinMode(RIGHT_BUTTON_3, INPUT);
    digitalWrite(RIGHT_BUTTON_3, LOW);
    pinMode(MODE_PIN_3, INPUT);
    digitalWrite(MODE_PIN_3, LOW);
    pinMode(JOY_LEFT_3, INPUT);
    digitalWrite(JOY_LEFT_3, LOW);
    pinMode(JOY_RIGHT_3, INPUT);
    digitalWrite(JOY_RIGHT_3, LOW);
    pinMode(JOY_MODE_3, INPUT);
    digitalWrite(JOY_MODE_3, LOW);

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer. button1Timer/button2Timer were never
    // initialized here in the legacy code (a real latent bug -- the very
    // first CheckButtons() call would measure elapsed time against garbage
    // memory); seeded to millis() now, matching ImplementPlanter's own
    // timers.
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

InterfacePlanter::InterfacePlanter(Stream* serialDebug,
                                    InterfaceI2CLCD* lcd,
                                    ImplementPlanter* implement,
                                    VehicleTractor* tractor) {
    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON_3, INPUT);
    digitalWrite(LEFT_BUTTON_3, LOW);
    pinMode(RIGHT_BUTTON_3, INPUT);
    digitalWrite(RIGHT_BUTTON_3, LOW);
    pinMode(MODE_PIN_3, INPUT);
    digitalWrite(MODE_PIN_3, LOW);
    pinMode(JOY_LEFT_3, INPUT);
    digitalWrite(JOY_LEFT_3, LOW);
    pinMode(JOY_RIGHT_3, INPUT);
    digitalWrite(JOY_RIGHT_3, LOW);
    pinMode(JOY_MODE_3, INPUT);
    digitalWrite(JOY_MODE_3, LOW);

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer -- same fix as the 5-arg constructor above.
    buttons = 0;
    button1Flag = false;
    button2Flag = false;
    button1Timer = millis();
    button2Timer = millis();

    // Connected classes. No guidance source supplied by this overload -- see
    // the constructor-selection comment in InterfacePlanter.hpp; Update()
    // still requires one before it can be called safely.
    this->serialDebug = serialDebug;
    this->lcd = lcd;
    this->implement = implement;
    this->tractor = tractor;
    this->guidance = nullptr;

#ifdef DEBUG
    serialDebug->println(S_DIVIDE);
    serialDebug->println("Initialised planter interface");
    serialDebug->println(S_DIVIDE);
#endif
}

// ------------------------
// Method for updating mode
// ------------------------
void InterfacePlanter::Update() {
    // ===============
    // Process buttons
    // ===============
    CheckButtons(255, 0);

    // ===============
    // Process tractor
    // ===============
    // The receiver port and the CAN bus are pumped by main.cpp's loop();
    // this class only reads what they committed to the GuidanceSource.
    tractor->Update(mode);

    // ====================
    // Process mode changes
    // ====================

    //-------
    // Manual
    //-------
    if (!digitalRead(MODE_PIN_3) ||
        //!digitalRead(JOY_MODE_3) ||
        //tractor->getHitch() ||
        implement->GetPlantingelement()) {
        // set mode to manual
        mode = 2;

#ifdef DEBUG
        serialDebug->print("M:");
        serialDebug->println(digitalRead(MODE_PIN_3));
#endif
    }

    //-----
    // Hold
    //-----
    else if (millis() - guidance->GetGgaTimestamp() > 2000 ||
             millis() - guidance->GetVtgTimestamp() > 2000 ||
             millis() - guidance->GetXteTimestamp() > 2000 ||
             guidance->GetQuality() != 4 ||
             !guidance->MinSpeed()) {
        // set mode to hold
        mode = 1;

#ifdef DEBUG
        serialDebug->println("H");
#endif
    }

    //----------
    // Automatic
    //----------
    else {
        // set mode to automatic
        mode = 0;

#ifdef DEBUG
        serialDebug->println("A");
#endif
    }

    // Update implement and adjust
    implement->Update();
    implement->Adjust(mode, buttons);

    // Update screen (no rewrite) and write one character
    UpdateScreen(0);
    lcd->WriteScreen(1);
}

// --------------------------
// Method for updating screen
// --------------------------
void InterfacePlanter::UpdateScreen(boolean rewrite) {
    int temp = 0;
    int temp2 = 0;

    // Update screen
    if (rewrite) {
        // Regel 0 - 3
        lcd->WriteBuffer(L3_OFFSET, 0);
        lcd->WriteBuffer(L3_A_POS, 1);
        lcd->WriteBuffer(L3_SETPOINT, 2);
        lcd->WriteBuffer(L3_XTE, 3);

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
    temp2 = implement->GetSetpoint();
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

    // Regel 3
    temp2 = implement->GetXte();
    temp = abs(temp2);

    if (temp > 99) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 5);
        }
        else {
            lcd->WriteBuffer(' ', 3, 5);
        }
        lcd->WriteBuffer(temp / 100 + '0', 3, 6);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', 3, 7);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 8);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', 3, 5);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 6);
        }
        else {
            lcd->WriteBuffer(' ', 3, 6);
        }
        lcd->WriteBuffer(temp / 10 + '0', 3, 7);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 8);
    }
    else {
        lcd->WriteBuffer(' ', 3, 5);
        lcd->WriteBuffer(' ', 3, 6);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 7);
        }
        else {
            lcd->WriteBuffer(' ', 3, 7);
        }
        lcd->WriteBuffer(temp + '0', 3, 8);
    }

    switch (mode) {
        case 0:  // AUTO
            lcd->WriteBuffer('A', 3, 14);
            lcd->WriteBuffer(' ', 3, 17);
            lcd->WriteBuffer(' ', 3, 18);
            break;
        case 1:  // HOLD
            lcd->WriteBuffer('H', 3, 14);
            if (!guidance->MinSpeed()) {
                lcd->WriteBuffer('S', 3, 17);
                lcd->WriteBuffer('!', 3, 18);
            }
            else if (guidance->GetQuality() != 4) {
                lcd->WriteBuffer('Q', 3, 17);
                lcd->WriteBuffer('!', 3, 18);
            }
            else {
                lcd->WriteBuffer('G', 3, 17);
                if (millis() - guidance->GetGgaTimestamp() > 2000) {
                    lcd->WriteBuffer('G', 3, 18);
                }
                else if (millis() - guidance->GetVtgTimestamp() > 2000) {
                    lcd->WriteBuffer('V', 3, 18);
                }
                else if (millis() - guidance->GetXteTimestamp() > 2000) {
                    lcd->WriteBuffer('X', 3, 18);
                }
                else {
                    lcd->WriteBuffer('!', 3, 18);
                }
            }
            break;
        case 2:  // MANUAL
            lcd->WriteBuffer('M', 3, 14);
            lcd->WriteBuffer(' ', 3, 17);
            lcd->WriteBuffer(' ', 3, 18);

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
int InterfacePlanter::CheckButtons(byte delay1, byte delay2) {
    if (button1Flag) {
        button1Timer = millis();
        button1Flag = false;
    }

    if (button2Flag) {
        button2Timer = millis();
        button2Flag = false;
    }

    // Check for left/right button presses
    if (digitalRead(LEFT_BUTTON_3) && digitalRead(RIGHT_BUTTON_3)) {
        if (millis() - button1Timer >= delay1 * 4) {  // delay * 4 because of byte type
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
    else if (digitalRead(LEFT_BUTTON_3)) {  // || digitalRead(JOY_LEFT_3)) {
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
    else if (digitalRead(RIGHT_BUTTON_3)) {  // || digitalRead(JOY_RIGHT_3)) {
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
