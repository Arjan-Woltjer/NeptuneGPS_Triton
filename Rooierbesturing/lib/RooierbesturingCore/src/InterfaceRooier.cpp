/*
  InterfaceRooier - a library for the MeijWorks windrowercontrol interface
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
#include "InterfaceRooier.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfaceRooier::InterfaceRooier(InterfaceI2CLCD* lcd, ImplementRooier* implement, VehicleTractor* tractor) {
#ifdef DEBUG
    Serial.println("Initialising rooier interface");
#endif

    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON_8, INPUT);
    digitalWrite(LEFT_BUTTON_8, LOW);
    pinMode(RIGHT_BUTTON_8, INPUT);
    digitalWrite(RIGHT_BUTTON_8, LOW);
    pinMode(MODE_PIN_8, INPUT);
    digitalWrite(MODE_PIN_8, LOW);
    pinMode(JOY_LEFT_8, INPUT);
    digitalWrite(JOY_LEFT_8, LOW);
    pinMode(JOY_RIGHT_8, INPUT);
    digitalWrite(JOY_RIGHT_8, LOW);
    pinMode(JOY_MODE_8, INPUT);
    digitalWrite(JOY_MODE_8, LOW);

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer. button1Timer/button2Timer were never
    // initialized in the legacy code -- the very first CheckButtons() call
    // would measure elapsed time against garbage memory. Seeded to millis(),
    // same fix already applied to every sibling InterfaceX.
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
void InterfaceRooier::Update() {
    // ===============
    // Process buttons
    // ===============
    CheckButtons(255, 0);

    // ================
    // Process tractor
    // ================
    // GPS was dropped for this pass (see the Rooierbesturing distillation
    // plan) -- the legacy source's #ifdef GPS branch never had a working
    // implementation to preserve, matching Slangenpomp Spuitcomputer's
    // precedent.
    tractor->Update(mode);

    // ====================
    // Process mode changes
    // ====================

    // ------
    // Manual
    // ------
    // The legacy source's manual condition read
    // "!implement->getPlantingelement()" -- a Planter-specific method
    // copy-pasted in that doesn't exist on ImplementRooier at all. The
    // separately abandoned updateMode() draft in the same file hinted at a
    // HITCH_PIN-based transition instead (raising the implement forces
    // Manual), which is both hardware-appropriate for a windrower and
    // already an established pattern (ImplementPlough's InterfacePlough::
    // Update() uses tractor->GetHitch() the same way) -- used here in place
    // of the dead Planter call.
    if (!digitalRead(MODE_PIN_8) || !digitalRead(JOY_MODE_8) || tractor->GetHitch()) {
        // set mode to manual
        mode = 2;

#ifdef DEBUG
        Serial.println("M");
#endif
    }
    // ---------
    // Automatic
    // ---------
    else {
        // set mode to automatic
        mode = 0;

#ifdef DEBUG
        Serial.println("A");
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
void InterfaceRooier::UpdateScreen(boolean rewrite) {
    // Update screen
    if (rewrite) {
        lcd->WriteBuffer(L8_REFERENCE, 0);
        lcd->WriteBuffer(L8_POS_L, 1);
        lcd->WriteBuffer(L8_POS_R, 2);
        lcd->WriteBuffer(L8_BLANK, 3);

        lcd->WriteScreen(-1);
    }

    // Each of these used to be its own ~30-line copy-pasted block, all three
    // (bug) writing to row 0 instead of their own row -- see writeValue()'s
    // declaration comment in the header.
    writeValue(implement->GetSetpoint(), 0);
    writeValue(implement->GetHeightL(), 1);
    writeValue(implement->GetHeightR(), 2);

    switch (mode) {
        case 0:  // AUTO
            lcd->WriteBuffer('A', 3, 17);
            lcd->WriteBuffer(' ', 3, 19);
            break;
        case 2:  // MANUAL
            lcd->WriteBuffer('M', 3, 17);

            if (buttons == -1) {
                lcd->WriteBuffer('v', 3, 19);
            }
            else if (buttons == 1) {
                lcd->WriteBuffer('^', 3, 19);
            }
            else {
                lcd->WriteBuffer(' ', 3, 19);
            }
            break;
    }
}

void InterfaceRooier::writeValue(int value, byte row) {
    int temp = abs(value);

    if (temp > 99) {
        lcd->WriteBuffer(value < 0 ? '-' : ' ', row, 15);
        lcd->WriteBuffer(temp / 100 + '0', row, 16);
        temp = temp % 100;
        lcd->WriteBuffer(temp / 10 + '0', row, 17);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, 18);
    }
    else if (temp > 9) {
        lcd->WriteBuffer(' ', row, 15);
        lcd->WriteBuffer(value < 0 ? '-' : ' ', row, 16);
        lcd->WriteBuffer(temp / 10 + '0', row, 17);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', row, 18);
    }
    else {
        lcd->WriteBuffer(' ', row, 15);
        lcd->WriteBuffer(' ', row, 16);
        lcd->WriteBuffer(value < 0 ? '-' : ' ', row, 17);
        lcd->WriteBuffer(temp + '0', row, 18);
    }
}

// ---------------------------
// Method for checking buttons
// ---------------------------
int InterfaceRooier::CheckButtons(byte delay1, byte delay2) {
    if (button1Flag) {
        button1Timer = millis();
        button1Flag = false;
    }

    if (button2Flag) {
        button2Timer = millis();
        button2Flag = false;
    }

    // Check for left/right button presses
    if (digitalRead(LEFT_BUTTON_8) && digitalRead(RIGHT_BUTTON_8)) {
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
    else if (digitalRead(LEFT_BUTTON_8) || digitalRead(JOY_LEFT_8)) {
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
    else if (digitalRead(RIGHT_BUTTON_8) || digitalRead(JOY_RIGHT_8)) {
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
