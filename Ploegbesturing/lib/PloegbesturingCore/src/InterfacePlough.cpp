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
#include "LcdNumber.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfacePlough::InterfacePlough(Stream* serialDebug,
                                  InterfaceI2CLCD* lcd,
                                  ImplementPlough* implement,
                                  VehicleSensors* tractor,
                                  GuidanceSource* guidance) {
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

    // Button flag and timer. The button timers were never initialized
    // here in the legacy code (a real latent bug -- CheckButtons()'s very
    // first call would measure elapsed time against garbage memory); seeded
    // to millis() now, matching how ImplementPlough seeds its own timers.
    buttons = 0;
    buttonPair.Begin(millis());

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
    else if (millis() - guidance->GetGgaTimestamp() > 2000 ||
             millis() - guidance->GetVtgTimestamp() > 2000 ||
             millis() - guidance->GetXteTimestamp() > 2000 ||
             millis() - guidance->GetQualityTimestamp() > 2000 ||  // #261
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
    // Update screen
    if (rewrite) {
        // Regel 0
        lcd->WriteBuffer(L_POS, 0);

        // Regel 1
        lcd->WriteBuffer(L_A_POS, 1);

        // Regel 2
        lcd->WriteBuffer(L_XTE, 2);

#ifdef ROTATION
        // Regel 3
        lcd->WriteBuffer(L_ROTATION, 3);
#else
        lcd->WriteBuffer(L_BLANK, 3);
#endif

        lcd->WriteScreen(-1);
    }

    // Regel 0
    LcdWriteNumber(lcd, implement->GetOffset(), 0, 16, 4);

    // Regel 1
    LcdWriteNumber(lcd, implement->GetPosition(), 1, 16, 4);


    // Regel 2
    LcdWriteNumber(lcd, guidance->GetXte(), 2, 16, 4);

#ifdef ROTATION
    // Regel  3
    LcdWriteNumber(lcd, implement->GetRotation(), 3, 9, 4);
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
            if (!guidance->MinSpeed() && millis() - guidance->GetVtgTimestamp() < 2000) {
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
    // MeijWorks Libs/ButtonPair: debounce, hold-to-repeat and both held.
    buttons = buttonPair.Check(digitalRead(LEFT_BUTTON_2), digitalRead(RIGHT_BUTTON_2), delay1, delay2, millis());
    return buttons;
}

}  // namespace triton
