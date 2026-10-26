/*
  InterfaceScraper - a library for the MeijWorks scrapercontrol interface
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
#include "InterfaceScraper.hpp"
#include "LcdNumber.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfaceScraper::InterfaceScraper(InterfaceI2CLCD* lcd,
                                    ImplementScraper* implement,
                                    VehicleSensors* tractor,
                                    GuidanceSource* guidance) {
#ifdef DEBUG
    Serial.println(S_DIVIDE);
    Serial.println("Initialising scraper interface");
    Serial.println(S_DIVIDE);
#endif

    // Pin assignments and configuration
    // Schmitt triggered inputs
    pinMode(LEFT_BUTTON_5, INPUT);
    digitalWrite(LEFT_BUTTON_5, LOW);
    pinMode(RIGHT_BUTTON_5, INPUT);
    digitalWrite(RIGHT_BUTTON_5, LOW);
    pinMode(MODE_PIN_5, INPUT);
    digitalWrite(MODE_PIN_5, LOW);
    pinMode(JOY_LEFT_5, INPUT);
    digitalWrite(JOY_LEFT_5, LOW);
    pinMode(JOY_RIGHT_5, INPUT);
    digitalWrite(JOY_RIGHT_5, LOW);
    pinMode(JOY_MODE_5, INPUT);
    digitalWrite(JOY_MODE_5, LOW);

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer. button1Timer/button2Timer were never
    // initialized here in the legacy code (a real latent bug -- the very
    // first CheckButtons() call would measure elapsed time against garbage
    // memory); seeded to millis() now, matching the same fix already applied
    // to InterfacePlough/InterfacePlanter.
    buttons = 0;
    buttonPair.Begin(millis());

    // Connected classes
    this->lcd = lcd;
    this->implement = implement;
    this->tractor = tractor;
    this->guidance = guidance;
}

// ------------------------
// Method for updating mode
// ------------------------
void InterfaceScraper::Update() {
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

    // ------
    // Manual
    // ------
    if (!digitalRead(MODE_PIN_5) || !digitalRead(JOY_MODE_5)) {
        // set mode to manual
        mode = 2;

#ifdef DEBUG
        Serial.println("M");
#endif
    }
    // ----
    // Hold
    // ----
    else if (millis() - guidance->GetGgaTimestamp() > 2000 || millis() - guidance->GetVtgTimestamp() > 2000) {
        // set mode to hold
        mode = 1;

#ifdef DEBUG
        Serial.println("H");
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
void InterfaceScraper::UpdateScreen(boolean rewrite) {
    int temp = 0;
    int temp2 = 0;

    // Update screen
    if (rewrite) {
        // Regel 0
        lcd->WriteBuffer(L_HEIGHT_R, 0);

        // Regel 1
        lcd->WriteBuffer(L_HEIGHT, 1);

        // Regel 2
        lcd->WriteBuffer(L_DISTANCE, 2);

        // Regel 3
        lcd->WriteBuffer(L_SLOPE, 3);

        lcd->WriteScreen(-1);
    }

    // Regel 0
    LcdWriteNumber(lcd, implement->GetRefHeight(), 0, 5, 7, 2, ',');

    // Regel 0
    temp2 = implement->GetOffset();
    temp = abs(temp2);

    if (temp > 99) {
        lcd->WriteBuffer(' ', 0, 12);
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 0, 13);
        }
        else {
            lcd->WriteBuffer('+', 0, 13);
        }
        lcd->WriteBuffer(' ', 0, 14);
        lcd->WriteBuffer(temp / 100 + '0', 0, 15);
        temp = temp % 100;
        lcd->WriteBuffer(',', 0, 16);
        lcd->WriteBuffer(temp / 10 + '0', 0, 17);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 0, 18);
    }

    // Regel 1
    LcdWriteNumber(lcd, implement->GetHeight(), 1, 12, 7, 2, ',');

    // Regel 2
    LcdWriteNumber(lcd, implement->GetDistance(), 2, 14, 5);

    // Regel 3
    temp2 = implement->GetSlope();
    temp = abs(temp2);

    if (temp > 9) {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 8);
        }
        else {
            lcd->WriteBuffer(' ', 3, 8);
        }
        lcd->WriteBuffer('.', 3, 9);
        lcd->WriteBuffer(temp / 10 + '0', 3, 10);
        temp = temp % 10;
        lcd->WriteBuffer(temp + '0', 3, 11);
    }
    else {
        if (temp2 < 0) {
            lcd->WriteBuffer('-', 3, 8);
        }
        else {
            lcd->WriteBuffer(' ', 3, 8);
        }
        lcd->WriteBuffer('.', 3, 9);
        lcd->WriteBuffer('0', 3, 10);
        lcd->WriteBuffer(temp + '0', 3, 11);
    }

    switch (mode) {
        case 0:  // AUTO
            lcd->WriteBuffer('A', 3, 14);
            lcd->WriteBuffer(' ', 3, 17);
            lcd->WriteBuffer(' ', 3, 18);
            break;
        case 1:  // HOLD
            lcd->WriteBuffer('H', 3, 14);
            if (guidance->MinSpeed()) {
                lcd->WriteBuffer('G', 3, 17);
                lcd->WriteBuffer('!', 3, 18);
            }
            else {
                lcd->WriteBuffer('S', 3, 17);
                lcd->WriteBuffer('!', 3, 18);
            }
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

// ---------------------------
// Method for checking buttons
// ---------------------------
int InterfaceScraper::CheckButtons(byte delay1, byte delay2) {
    // MeijWorks Libs/ButtonPair: debounce, hold-to-repeat and both held.
    buttons = buttonPair.Check(digitalRead(LEFT_BUTTON_5), digitalRead(RIGHT_BUTTON_5), delay1, delay2, millis(),
                              digitalRead(JOY_LEFT_5), digitalRead(JOY_RIGHT_5));
    return buttons;
}

}  // namespace triton
