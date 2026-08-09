/*
  IsobusDebugMenu - serial-port status menu for ISOBUS/CAN bus health and guidance telemetry
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
#include "IsobusDebugMenu.hpp"

// The header is itself empty unless ISOBUS is defined (see its own comment) --
// guard the body too, so this compiles to an empty translation unit instead
// of failing on undeclared isobus:: symbols when PlatformIO's LDF pulls this
// file in on teensy41_serial anyway.
#ifdef ISOBUS

using namespace isobus;

namespace triton
{

// Only CAN channel this project ever brings up (see
// IsobusGuidanceChannel::Begin()'s set_number_of_can_channels(1)).
static constexpr std::uint8_t kCanChannel = 0;

static constexpr unsigned long kPeriodicIntervalMs = 1000UL;

// ------------------------------------------------------------------
// Constructor / Begin
// ------------------------------------------------------------------
IsobusDebugMenu::IsobusDebugMenu(Stream* serialDebug, IsobusGuidanceChannel* guidanceChannel, GuidanceSource* guidance)
    : serialDebug(serialDebug), guidanceChannel(guidanceChannel), guidance(guidance) {
}

void IsobusDebugMenu::Begin() {
    serialDebug->println("IsobusDebugMenu: send any character to open the status menu.");
}

// ------------------------------------------------------------------
// Update -- call every loop() iteration
// ------------------------------------------------------------------
void IsobusDebugMenu::Update() {
    if (periodicEnabled && state == State::IDLE) {
        unsigned long now = millis();
        if (now - lastPeriodicPrintMs >= kPeriodicIntervalMs) {
            lastPeriodicPrintMs = now;
            printPeriodicLine();
        }
    }

    while (serialDebug->available()) {
        char c = (char)serialDebug->read();
        if (c == '\r' || c == '\n') continue;

        if (state == State::IDLE) {
            state = State::MENU;
            printMenu();
            continue;
        }
        handleMenu(c);
    }
}

// ------------------------------------------------------------------
// Menu
// ------------------------------------------------------------------
void IsobusDebugMenu::printMenu() {
    serialDebug->println("=== ISOBUS DEBUG MENU ===");
    serialDebug->println("1. Full status dump");
    serialDebug->print("2. Periodic summary line (");
    serialDebug->print(periodicEnabled ? "ON" : "OFF");
    serialDebug->println(" - press to toggle)");
    serialDebug->println("3. Reset message counters");
    serialDebug->println("q. Exit");
}

void IsobusDebugMenu::handleMenu(char c) {
    switch (c) {
        case '1':
            printFullDump();
            printMenu();
            break;
        case '2':
            periodicEnabled = !periodicEnabled;
            lastPeriodicPrintMs = 0;
            serialDebug->print("Periodic summary line: ");
            serialDebug->println(periodicEnabled ? "ON" : "OFF");
            printMenu();
            break;
        case '3':
            guidanceChannel->ResetMessageCounters();
            serialDebug->println("Counters reset.");
            printMenu();
            break;
        case 'q':
        case 'Q':
            serialDebug->println("Exiting.");
            state = State::IDLE;
            break;
        default:
            serialDebug->println("Invalid choice.");
            printMenu();
            break;
    }
}

// ------------------------------------------------------------------
// Full status dump
// ------------------------------------------------------------------
void IsobusDebugMenu::printFullDump() {
    auto controlFunction = guidanceChannel->GetControlFunction();
    bool claimed = controlFunction != nullptr && controlFunction->get_address_valid();
    auto counters = guidanceChannel->GetMessageCounters();
    float busload = CANNetworkManager::CANNetwork.get_estimated_busload(kCanChannel);

    serialDebug->println("=== ISOBUS STATUS ===");

    serialDebug->print("Address claim: ");
    serialDebug->print(claimed ? "CLAIMED  address=0x" : "NOT CLAIMED");
    if (claimed) serialDebug->print(controlFunction->get_address(), HEX);
    serialDebug->println();

    serialDebug->print("Bus load (ch0): ");
    serialDebug->print(busload, 1);
    serialDebug->println(" %");

    serialDebug->println("--- Message counters ---");
    serialDebug->print("  PGN 129025 Position NMEA2000: ");
    serialDebug->println(counters.positionNmea2000);
    serialDebug->print("  PGN 129026 Speed NMEA2000:    ");
    serialDebug->println(counters.speedNmea2000);
    serialDebug->print("  PGN 129283 XTE NMEA2000:      ");
    serialDebug->println(counters.xteNmea2000);
    serialDebug->print("  PGN 65267  Position legacy:   ");
    serialDebug->println(counters.positionLegacy);
    serialDebug->print("  PGN 65256  Speed legacy:      ");
    serialDebug->print(counters.speedLegacy);
    serialDebug->print("   last SA=0x");
    serialDebug->print(counters.lastSpeedLegacySourceAddress, HEX);
    serialDebug->print(" raw=0x");
    serialDebug->println(counters.lastSpeedLegacyRaw, HEX);
    serialDebug->print("  PGN 65535  XTE JD legacy:     ");
    serialDebug->print(counters.xteJohnDeereLegacy);
    serialDebug->print("   last SA=0x");
    serialDebug->print(counters.lastXteJohnDeereLegacySourceAddress, HEX);
    serialDebug->print(" word=0x");
    serialDebug->print(counters.lastXteJohnDeereLegacyRawWord, HEX);
    serialDebug->print(" byte1=0x");
    serialDebug->println(counters.lastXteJohnDeereLegacyRawByte1, HEX);
    serialDebug->print("  PGN 60160  XTE Trimble legacy:");
    serialDebug->print(counters.xteTrimbleLegacy);
    serialDebug->print("   last SA=0x");
    serialDebug->println(counters.lastXteTrimbleLegacySourceAddress, HEX);
    serialDebug->print("  PGN 64770  AISO stop:         ");
    serialDebug->print(counters.allImplementStop);
    serialDebug->print("   last ");
    serialDebug->print(counters.allImplementStop > 0 ? (millis() - counters.lastAllImplementStopMs) : 0);
    serialDebug->println(" ms ago");
    serialDebug->print("  TOTAL:                        ");
    serialDebug->println(counters.Total());

    serialDebug->println("--- Guidance telemetry ---");
    serialDebug->print("  XTE:          ");
    serialDebug->print(guidance->GetXte() / 100.0f, 2);
    serialDebug->println(" m");
    serialDebug->print("  Speed:        ");
    serialDebug->print(guidance->GetSpeedMs(), 2);
    serialDebug->print(" m/s (");
    serialDebug->print(guidance->GetSpeed(), 2);
    serialDebug->println(" kn)");
    serialDebug->print("  Quality:      ");
    serialDebug->print(guidance->GetQuality());
    serialDebug->print("   RTK quality=");
    serialDebug->print(guidance->GetRtkQuality());
    serialDebug->print("  IsRtkQuality=");
    serialDebug->println(guidance->IsRtkQuality() ? "Y" : "N");
    serialDebug->print("  GGA fix age:  ");
    serialDebug->print(millis() - guidance->GetGgaFixAge());
    serialDebug->println(" ms");
    serialDebug->print("  VTG fix age:  ");
    serialDebug->print(millis() - guidance->GetVtgFixAge());
    serialDebug->println(" ms");
    serialDebug->print("  XTE fix age:  ");
    serialDebug->print(millis() - guidance->GetXteFixAge());
    serialDebug->println(" ms");
    serialDebug->print("  Lat/Lon/Alt/Course: ");
    serialDebug->print(guidance->GetLatitude(), 6);
    serialDebug->print(" / ");
    serialDebug->print(guidance->GetLongitude(), 6);
    serialDebug->print(" / ");
    serialDebug->print(guidance->GetAltitude(), 1);
    serialDebug->print(" m / ");
    serialDebug->print(guidance->GetCourse(), 1);
    serialDebug->println(" deg");
}

// ------------------------------------------------------------------
// Periodic one-liner
// ------------------------------------------------------------------
void IsobusDebugMenu::printPeriodicLine() {
    auto controlFunction = guidanceChannel->GetControlFunction();
    bool claimed = controlFunction != nullptr && controlFunction->get_address_valid();
    auto counters = guidanceChannel->GetMessageCounters();
    float busload = CANNetworkManager::CANNetwork.get_estimated_busload(kCanChannel);
    unsigned long now = millis();

    serialDebug->print("[ISOBUS] addr=0x");
    serialDebug->print(claimed ? controlFunction->get_address() : 0, HEX);
    serialDebug->print(" claimed=");
    serialDebug->print(claimed ? "Y" : "N");
    serialDebug->print(" load=");
    serialDebug->print(busload, 1);
    serialDebug->print("% msgs=");
    serialDebug->print(counters.Total());
    serialDebug->print(" aiso=");
    serialDebug->print(counters.allImplementStop);
    serialDebug->print("(");
    serialDebug->print(counters.allImplementStop > 0 ? (now - counters.lastAllImplementStopMs) : 0);
    serialDebug->print("ms) xte=");
    serialDebug->print(guidance->GetXte() / 100.0f, 2);
    serialDebug->print("m spd=");
    serialDebug->print(guidance->GetSpeedMs(), 2);
    serialDebug->print("m/s q=");
    serialDebug->print(guidance->GetQuality());
    serialDebug->print(" ggaAge=");
    serialDebug->print(now - guidance->GetGgaFixAge());
    serialDebug->print(" vtgAge=");
    serialDebug->print(now - guidance->GetVtgFixAge());
    serialDebug->print(" xteAge=");
    serialDebug->println(now - guidance->GetXteFixAge());
}

}  // namespace triton

#endif  // ISOBUS
