/*
  InterfaceKipper - a library for the MeijWorks kippercontrol interface
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
#include "InterfaceKipper.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfaceKipper::InterfaceKipper(InterfaceI2CLCD* lcd, ImplementKipper* implement, VehicleTractor* tractor) {
#ifdef DEBUG
    Serial.println("Initialising kipper interface");
#endif

    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON_4, INPUT);
    digitalWrite(LEFT_BUTTON_4, LOW);
    pinMode(RIGHT_BUTTON_4, INPUT);
    digitalWrite(RIGHT_BUTTON_4, LOW);
    pinMode(MODE_PIN_4, INPUT);
    digitalWrite(MODE_PIN_4, LOW);

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer. button1Timer/button2Timer were never
    // initialized here in the legacy code -- the very first CheckButtons()
    // call would measure elapsed time against garbage memory. Seeded to
    // millis(), same fix already applied to every sibling InterfaceX.
    buttons = 0;
    button1Flag = false;
    button2Flag = false;
    button1Timer = millis();
    button2Timer = millis();

    // Connected classes
    this->lcd = lcd;
    this->implement = implement;
    this->tractor = tractor;
}

// ------------------------
// Method for updating mode
// ------------------------
void InterfaceKipper::Update() {
    // ===============
    // Process buttons
    // ===============
    CheckButtons(255, 0);

    // ===============
    // Process tractor
    // ===============
    tractor->Update(mode);

    // ====================
    // Process mode changes
    // ====================

    // ------
    // Manual
    // ------
    if (!digitalRead(MODE_PIN_4)) {
        mode = 2;
    }
    // ----
    // Hold
    // ----
    // Reconstructed: the legacy source declared (and constructed) a
    // VehicleGps solely for this transition -- updateScreen()'s HOLD case
    // used gps->minSpeed()/gps->getQuality(), but update()'s mode ladder
    // never actually set mode to 1, so this safety interlock was dead code.
    // GPS itself was otherwise unused anywhere in ImplementKipper (see
    // Kipperbesturing distillation plan/commit). Reimplemented using
    // VehicleTractor::MinSpeed() (already real, already used by this
    // module's own D-term) instead of reintroducing GPS: auto-steering
    // pauses while the tractor is stationary.
    else if (!tractor->MinSpeed()) {
        mode = 1;
    }
    // ---------
    // Automatic
    // ---------
    else {
        mode = 0;
    }

    // Update implement and adjust
    implement->Update(mode);
    implement->Adjust(buttons);

    // Update screen (no rewrite) and write one character
    UpdateScreen(0);
    lcd->WriteScreen(1);
}

// --------------------------
// Method for updating screen
// --------------------------
void InterfaceKipper::UpdateScreen(boolean rewrite) {
    if (rewrite) {
        lcd->WriteBuffer(L4_OFFSET, 0);
        lcd->WriteBuffer(L4_A_STEER, 1);
        lcd->WriteBuffer(L4_SETPOINT, 2);
        lcd->WriteBuffer(L4_ANGLE, 3);

        lcd->WriteScreen(-1);
    }

    writeValue(implement->GetOffset(), 0, 16);
    writeValue(implement->GetSteer(), 1, 16);
    writeValue(implement->GetSetpoint(), 2, 16);
    writeValue(implement->GetAngle(), 3, 5);

    switch (mode) {
        case 0:  // AUTO
            lcd->WriteBuffer('A', 3, 14);
            lcd->WriteBuffer(' ', 3, 17);
            lcd->WriteBuffer(' ', 3, 18);
            break;
        case 1:  // HOLD
            lcd->WriteBuffer('H', 3, 14);
            lcd->WriteBuffer('S', 3, 17);
            lcd->WriteBuffer('!', 3, 18);
            break;
        case 2:  // MANUAL
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

void InterfaceKipper::writeValue(int value, byte row, byte col) {
    int temp = abs(value);

    if (temp > 99) {
        lcd->WriteBuffer(value < 0 ? '-' : ' ', row, col);
        lcd->WriteBuffer(temp / 100 + '0', row, col + 1);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', row, col + 2);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 3);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', row, col);
        lcd->WriteBuffer(value < 0 ? '-' : ' ', row, col + 1);
        lcd->WriteBuffer(temp / 10 + '0', row, col + 2);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, col + 3);
    }
    else {
        lcd->WriteBuffer(' ', row, col);
        lcd->WriteBuffer(' ', row, col + 1);
        lcd->WriteBuffer(value < 0 ? '-' : ' ', row, col + 2);
        lcd->WriteBuffer(temp + '0', row, col + 3);
    }
}

// ---------------------------
// Method for checking buttons
// ---------------------------
int InterfaceKipper::CheckButtons(byte delay1, byte delay2) {
    if (button1Flag) {
        button1Timer = millis();
        button1Flag = false;
    }

    if (button2Flag) {
        button2Timer = millis();
        button2Flag = false;
    }

    // Check for left/right button presses
    if (digitalRead(LEFT_BUTTON_4) && digitalRead(RIGHT_BUTTON_4)) {
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
    else if (digitalRead(LEFT_BUTTON_4)) {
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
    else if (digitalRead(RIGHT_BUTTON_4)) {
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
