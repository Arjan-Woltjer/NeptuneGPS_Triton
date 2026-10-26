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
#include "PloughGates.hpp"
#include "TritonIo.hpp"

namespace triton
{

// -----------
// Constructor
// -----------
InterfacePlough::InterfacePlough(Stream* serialDebug,
                                 InterfaceI2CLCD* lcd,
                                 ImplementPlough* implement,
                                 VehicleSensors* tractor,
                                 GuidanceSource* guidance):
                                 serialDebug(serialDebug), 
                                 lcd(lcd), 
                                 implement(implement),
                                 tractor(tractor),
                                 guidance(guidance) {
    // Pin assignments and configuration
    // Schmitt triggered inputs (GPIO on the Teensy/Arduino boards, the
    // MCP23008 expander on Triton01 -- TritonIo.hpp hides the difference)
    triton::ConfigureDigitalInput(LEFT_BUTTON_2, false);
    triton::ConfigureDigitalInput(RIGHT_BUTTON_2, false);
    triton::ConfigureDigitalInput(MODE_PIN_2, false);
    // The Micro and pre-series boards have no joystick inputs, and their
    // config headers define no JOY_ pins.
#ifdef JOY_MODE_2
    triton::ConfigureDigitalInput(JOY_LEFT_2, false);
    triton::ConfigureDigitalInput(JOY_RIGHT_2, false);
    triton::ConfigureDigitalInput(JOY_MODE_2, false);
#endif

    // Mode
    mode = 2;  // MANUAL

    // Button flag and timer. button1Timer/button2Timer were never initialized
    // here in the legacy code (a real latent bug -- CheckButtons()'s very
    // first call would measure elapsed time against garbage memory); seeded
    // to millis() now, matching how ImplementPlough seeds its own timers.
    buttons = 0;
    buttonPair.Begin(millis());
}

// ------------------------
// Method for updating mode
// ------------------------
void InterfacePlough::Update(bool vtWiderPressed, bool vtNarrowerPressed, bool vtCalibratePressed) {
    // Check buttons
    CheckButtons(255, 0, vtWiderPressed, vtNarrowerPressed, vtCalibratePressed);

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
    if (!triton::ReadDigital(MODE_PIN_2) ||
        //triton::ReadDigital(JOY_MODE_2) ||
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
    // (PloughGates.hpp: the same two checks drive the VT's indicators.)
    else if (!GpsReadyToSteer(*guidance, millis()) || !SpeedReadyToSteer(*guidance)) {
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
void InterfacePlough::UpdateScreen(bool rewrite) {
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
short int InterfacePlough::CheckButtons(byte delay1, byte delay2, bool vtWiderPressed, bool vtNarrowerPressed, bool vtCalibratePressed) {
    // MeijWorks Libs/ButtonPair: debounce, hold-to-repeat and both held.
    buttons = buttonPair.Check(triton::ReadDigital(LEFT_BUTTON_2), triton::ReadDigital(RIGHT_BUTTON_2), delay1, delay2, millis(),
                              false, false, vtCalibratePressed, vtWiderPressed, vtNarrowerPressed);
    return buttons;
}

}  // namespace triton
