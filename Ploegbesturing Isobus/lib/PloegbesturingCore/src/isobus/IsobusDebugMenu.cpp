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

// Print an 8-byte CAN payload as 16 zero-padded hex characters, no
// separators. Arduino's print(x, HEX) drops leading zeros, which would make
// the field variable-width and ruin offline column alignment when deriving a
// payload layout from a capture (GitHub issue #20) -- 0x03 must read as "03",
// not "3", or byte boundaries shift.
static void GPrintPayloadHex(Stream* out, const uint8_t* payload) {
    static const char kHexDigits[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < 8; i++) {
        out->print(kHexDigits[(payload[i] >> 4) & 0x0F]);
        out->print(kHexDigits[payload[i] & 0x0F]);
    }
}

// ------------------------------------------------------------------
// Constructor / Begin
// ------------------------------------------------------------------
IsobusDebugMenu::IsobusDebugMenu(Stream* serialDebug, IsobusGuidanceChannel* guidanceChannel, GuidanceSource* guidance,
                                  IsobusTcInterface* tcInterface, IsobusVtInterface* vtInterface)
    : serialDebug(serialDebug), guidanceChannel(guidanceChannel), guidance(guidance),
      tcInterface(tcInterface), vtInterface(vtInterface) {
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
    // All 8 bytes -- the word/byte1 fields above are the John Deere layout's
    // fields specifically, which is exactly the assumption GitHub issue #20
    // is trying to replace for Ag Leader. Deriving that layout needs the
    // whole payload, so print it whole.
    serialDebug->print("             full payload:     ");
    GPrintPayloadHex(serialDebug, counters.lastXteJohnDeereLegacyPayload);
    serialDebug->print("  (");
    serialDebug->print(counters.xteJohnDeereLegacy > 0
                           ? (millis() - counters.lastXteJohnDeereLegacyPayloadMs)
                           : 0);
    serialDebug->println(" ms ago)");
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
    serialDebug->print(millis() - guidance->GetXteTimestamp());
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

    serialDebug->println("--- Virtual Terminal ---");
    if (vtInterface == nullptr) {
        serialDebug->println("  (not configured)");
    } else {
        serialDebug->print("  Connected:    ");
        serialDebug->println(vtInterface->IsConnected() ? "Y" : "N");
        serialDebug->print("  State:        ");
        serialDebug->print(vtInterface->GetStateStep());
        serialDebug->print("/");
        serialDebug->print(vtInterface->GetStateTotalSteps());
        serialDebug->print("  ");
        serialDebug->println(vtInterface->GetStateName());
        serialDebug->print("  VT version:  ");
        serialDebug->println(vtInterface->GetVtVersionName());
        serialDebug->print("  VT status msgs: ");
        serialDebug->print(vtInterface->GetVtStatusMessageCount());
        serialDebug->print("   last ");
        serialDebug->print(vtInterface->GetVtStatusMessageAgeMs());
        serialDebug->println(" ms ago");
        // Partner address/validity: the thing that silently went false in
        // #17 and was readable nowhere at the time. If this reads valid=N
        // while the terminal is plainly alive on screen, that is the
        // control-function eviction, not the terminal.
        serialDebug->print("  Partner: addr=0x");
        serialDebug->print(vtInterface->GetPartnerAddress(), HEX);
        serialDebug->print(" valid=");
        serialDebug->print(vtInterface->IsPartnerAddressValid() ? "Y" : "N");
        serialDebug->print("  reconnect attempts=");
        serialDebug->println(vtInterface->GetReconnectAttemptCount());
    }

    serialDebug->println("--- Task Controller ---");
    if (tcInterface == nullptr) {
        serialDebug->println("  (not configured)");
    } else {
        serialDebug->print("  Connected:    ");
        serialDebug->println(tcInterface->IsConnected() ? "Y" : "N");
        serialDebug->print("  TC-GEO (with/without pos): ");
        if (tcInterface->IsConnected()) {
            serialDebug->print(tcInterface->SupportsTcGeoWithPosition() ? "Y" : "N");
            serialDebug->print("/");
            serialDebug->println(tcInterface->SupportsTcGeoWithoutPosition() ? "Y" : "N");
        } else {
            serialDebug->println("(not connected)");
        }
        serialDebug->print("  Task active:  ");
        // Advisory only -- see IsobusTcInterface::IsTaskActive()'s comment.
        serialDebug->println(tcInterface->IsTaskActive() ? "Y" : "N");
        serialDebug->print("  DRP deviation (DDI 513): ");
        serialDebug->print(tcInterface->GetDrpDeviationMm());
        serialDebug->print(" mm, last ");
        serialDebug->print(millis() - tcInterface->GetDrpTimestamp());
        serialDebug->println(" ms ago");
        serialDebug->print("  GNSS quality (DDI 514):  ");
        serialDebug->print(tcInterface->GetTcGnssQuality());
        serialDebug->print(", last ");
        serialDebug->print(millis() - tcInterface->GetQualityTimestamp());
        serialDebug->println(" ms ago");
        serialDebug->print("  Value commands (any DDI): ");
        serialDebug->print(tcInterface->GetValueCommandCount());
        serialDebug->print("  last DDI=");
        if (tcInterface->GetValueCommandCount() > 0) {
            serialDebug->print(tcInterface->GetLastValueCommandDdi());
            serialDebug->print(" (");
            serialDebug->print(millis() - tcInterface->GetLastValueCommandMs());
            serialDebug->println(" ms ago)");
        } else {
            serialDebug->println("(none)");
        }
        serialDebug->print("  Value requests (any DDI): ");
        serialDebug->println(tcInterface->GetValueRequestCount());
        // Partner address/validity: the thing that silently went false in
        // both #17 and #19 and was readable nowhere at the time.
        serialDebug->print("  Partner: addr=0x");
        serialDebug->print(tcInterface->GetPartnerAddress(), HEX);
        serialDebug->print(" valid=");
        serialDebug->print(tcInterface->IsPartnerAddressValid() ? "Y" : "N");
        serialDebug->print("  reconnect attempts=");
        serialDebug->println(tcInterface->GetReconnectAttemptCount());
        // Tramline Control probe (GitHub issue #21) -- the arrival is the
        // result, not the value; see IsobusTcInterface::HasTramlineSetpoint().
        serialDebug->print("  Tramline setpoint (DDI 506): ");
        if (tcInterface->HasTramlineSetpoint()) {
            serialDebug->print(tcInterface->GetTramlineSetpointLevel());
            serialDebug->print("  -> terminal DOES implement Tramline Control (");
            serialDebug->print(millis() - tcInterface->GetTramlineSetpointMs());
            serialDebug->println(" ms ago)");
        } else if (tcInterface->IsConnected()) {
            serialDebug->println("(none yet -- no Tramline Control seen on this terminal)");
        } else {
            serialDebug->println("(not connected)");
        }
        // Guidance-track DDIs 508-511. Worth reading even when 506 stays
        // silent: a terminal that populates track numbering speaks this part
        // of the protocol whether or not it completes the tramline handshake,
        // which is a different and useful answer.
        serialDebug->print("  Guidance track (DDI 508-511): ");
        if (tcInterface->HasGuidanceTrackInfo()) {
            serialDebug->print("abLine=");
            serialDebug->print(tcInterface->GetAbLineId());
            serialDebug->print(" track=");
            serialDebug->print(tcInterface->GetActualTrackNumber());
            serialDebug->print(" right=");
            serialDebug->print(tcInterface->GetTrackNumberRight());
            serialDebug->print(" left=");
            serialDebug->print(tcInterface->GetTrackNumberLeft());
            serialDebug->print("  (");
            serialDebug->print(tcInterface->GetGuidanceTrackAgeMs());
            serialDebug->println(" ms ago)");
        } else if (tcInterface->IsConnected()) {
            serialDebug->println("(none yet)");
        } else {
            serialDebug->println("(not connected)");
        }
    }
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
    serialDebug->print(now - guidance->GetXteTimestamp());

    // Raw PGN 65535 payload, on the periodic line specifically so a serial
    // capture is time-correlated: deriving Ag Leader's layout (GitHub issue
    // #20) means matching these bytes against XTE values an operator reads
    // aloud off the terminal, which only works if each sample carries the
    // same timestamp as everything else on the line. Sampled at the periodic
    // rate rather than per message -- the underlying PGN arrives ~10 Hz, but
    // real XTE moves on the scale of seconds, so 1 Hz is ample and keeps the
    // log readable. Source address is included because the whole point is
    // that this PGN is shared between vendors with different layouts.
    if (counters.xteJohnDeereLegacy > 0) {
        serialDebug->print(" xteraw=");
        serialDebug->print(counters.lastXteJohnDeereLegacySourceAddress, HEX);
        serialDebug->print(":");
        GPrintPayloadHex(serialDebug, counters.lastXteJohnDeereLegacyPayload);
    }

    if (vtInterface != nullptr) {
        serialDebug->print(" vt=");
        serialDebug->print(vtInterface->IsConnected() ? "Y" : "N");
        serialDebug->print("(");
        serialDebug->print(vtInterface->GetStateStep());
        serialDebug->print("/");
        serialDebug->print(vtInterface->GetStateTotalSteps());
        serialDebug->print(") vtstat=");
        serialDebug->print(vtInterface->GetVtStatusMessageCount());
        serialDebug->print("/");
        serialDebug->print(vtInterface->GetVtStatusMessageAgeMs());
        serialDebug->print("ms");
    }
    if (tcInterface != nullptr) {
        serialDebug->print(" tc=");
        serialDebug->print(tcInterface->IsConnected() ? "Y" : "N");
        serialDebug->print(" drp=");
        serialDebug->print(tcInterface->GetDrpDeviationMm());
        serialDebug->print("mm tcq=");
        serialDebug->print(tcInterface->GetTcGnssQuality());
    }
    serialDebug->println();
}

}  // namespace triton

#endif  // ISOBUS
