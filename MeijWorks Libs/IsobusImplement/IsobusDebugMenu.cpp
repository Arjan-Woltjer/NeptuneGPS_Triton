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

// FLASHMEM runs rarely used code from flash instead of the Teensy 4's RAM1
// (#192). The Teensy 4 core defines it; the ESP32 core does not, so fill the
// gap (the plough's FlashMem.hpp does the same for its own files).
#ifndef FLASHMEM
#define FLASHMEM
#endif

// The header is itself empty unless ISOBUS is defined (see its own comment) --
// guard the body too, so this compiles to an empty translation unit instead
// of failing on undeclared isobus:: symbols when PlatformIO's LDF pulls this
// file in on teensy41_serial anyway. The includes sit inside the guard as
// well: avr-gcc (micro_serial) has no <vector> at all.
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
// The first `length` bytes as zero-padded hex, "-" for none (#201: 129283
// frames come in shorter than 8 bytes, down to 0).
static void GPrintBytesHex(Stream* out, const uint8_t* payload, uint8_t length) {
    static const char kHexDigits[] = "0123456789ABCDEF";
    if (length == 0) { out->print("-"); return; }
    for (uint8_t i = 0; i < length && i < 8; i++) {
        out->print(kHexDigits[(payload[i] >> 4) & 0x0F]);
        out->print(kHexDigits[payload[i] & 0x0F]);
    }
}

void GPrintPayloadHex(Stream* out, const uint8_t* payload) {
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
                                  IsobusVtInterface* vtInterface, DebugMenuSections* sections)
    : serialDebug(serialDebug), guidanceChannel(guidanceChannel), guidance(guidance),
      vtInterface(vtInterface), sections(sections) {
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
        // With the menu closed, a machine's console sees each byte first and
        // keeps the ones that are its own (the plough: lines starting with
        // ':', #241). Before the CR/LF skip, since a line end runs a command.
        if (state == State::IDLE && inputFilter != nullptr && inputFilter(c)) continue;
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
FLASHMEM void IsobusDebugMenu::printMenu() {
    serialDebug->println("=== ISOBUS DEBUG MENU ===");
    serialDebug->println("1. Full status dump");
    serialDebug->print("2. Periodic summary line (");
    serialDebug->print(periodicEnabled ? "ON" : "OFF");
    serialDebug->println(" - press to toggle)");
    serialDebug->println("3. Reset message counters");
    if (sections != nullptr) sections->PrintMenuItems();
    serialDebug->println("5. Dump VT object pool as hex");
    serialDebug->println("q. Exit");
}

FLASHMEM void IsobusDebugMenu::handleMenu(char c) {
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
        case '5':
            // The working set's pool as it built it (the plough: whatever
            // BuildObjectPool() settled on -- the designed pool, the generated
            // one or the test pool), so this reports what would really be
            // uploaded.
            if (vtInterface != nullptr && vtInterface->GetWorkingSet()->GetPoolData() != nullptr &&
                vtInterface->GetWorkingSet()->GetPoolSize() > 0) {
                GPrintHexBlob(serialDebug, "VTPOOL", vtInterface->GetWorkingSet()->GetPoolData(),
                              vtInterface->GetWorkingSet()->GetPoolSize());
            } else {
                serialDebug->println("VT object pool not built yet.");
            }
            printMenu();
            break;
        case 'q':
        case 'Q':
            serialDebug->println("Exiting.");
            state = State::IDLE;
            break;
        default:
            if (sections == nullptr || !sections->HandleMenuKey(c)) serialDebug->println("Invalid choice.");
            printMenu();
            break;
    }
}

// ------------------------------------------------------------------
// Full status dump
// ------------------------------------------------------------------
FLASHMEM void IsobusDebugMenu::printFullDump() {
    auto controlFunction = guidanceChannel->GetControlFunction();
    bool claimed = controlFunction != nullptr && controlFunction->get_address_valid();
    auto counters = guidanceChannel->GetMessageCounters();
    float busload = CANNetworkManager::CANNetwork.get_estimated_busload(kCanChannel);

    serialDebug->println("=== ISOBUS STATUS ===");

    serialDebug->print("Address claim: ");
    serialDebug->print(claimed ? "CLAIMED  address=0x" : "NOT CLAIMED");
    if (claimed) {
        serialDebug->print(controlFunction->get_address(), HEX);
        // Our NAME's identity, from the board's serial since TC08 (#45) --
        // what to look for on the bus to tell two machines apart.
        serialDebug->print("  identity=");
        serialDebug->print(controlFunction->get_NAME().get_identity_number());
    }
    serialDebug->println();

    {
        // The CAN controller's own error state (#149): the live error counters,
        // the worst they reached, and every entry into error-passive or bus-off.
        const CanErrorMonitor& ce = guidanceChannel->GetCanErrors();
        serialDebug->print("CAN controller: ");
        if (ce.GetSamples() == 0) {
            serialDebug->println("not sampled on this build");
        } else {
            serialDebug->print(CanErrorMonitor::StateName(ce.GetState()));
            serialDebug->print("  TX err ");
            serialDebug->print(ce.GetTxErrors());
            serialDebug->print(" (peak ");
            serialDebug->print(ce.GetPeakTxErrors());
            serialDebug->print(")  RX err ");
            serialDebug->print(ce.GetRxErrors());
            serialDebug->print(" (peak ");
            serialDebug->print(ce.GetPeakRxErrors());
            serialDebug->println(")");
            serialDebug->print("  error-passive entries: ");
            serialDebug->print(ce.GetErrorPassiveEntries());
            serialDebug->print("  bus-off entries: ");
            serialDebug->print(ce.GetBusOffEntries());
            serialDebug->print("  longest episode: ");
            serialDebug->print(ce.GetLongestEpisodeMs());
            serialDebug->print(" ms");
            if (ce.GetState() != CanFaultState::ErrorActive) {
                // Still in it -- the longest only counts finished episodes.
                serialDebug->print("  (current: ");
                serialDebug->print(ce.GetCurrentEpisodeMs(millis()));
                serialDebug->print(" ms)");
            }
            serialDebug->println();
            if (ce.GetBusOffEntries() > 0) {
                serialDebug->print("  last bus-off ");
                serialDebug->print(millis() - ce.GetLastBusOffMs());
                serialDebug->println(" ms ago");
            }
        }
    }
    serialDebug->print("Bus load (ch0): ");
    serialDebug->print(busload, 1);
    serialDebug->println(" %");
    // #248: a frame used to be lost whenever a loop pass outlasted a burst
    // on the bus. The driver's receive queue shows how close it came to full.
    serialDebug->print("CAN receive queue: ");
    if (receiveQueueStats == nullptr || !receiveQueueStats().available) {
        serialDebug->println("not on this build");
    }
    else {
        const CanReceiveQueueStats q = receiveQueueStats();
        serialDebug->print("high water ");
        serialDebug->print(q.highWater);
        serialDebug->print(" of ");
        serialDebug->print(q.size);
        serialDebug->print(", dropped ");
        serialDebug->println(q.overflows);
    }
    serialDebug->print("Loop pass: longest ");
    serialDebug->print(longestPassSinceDumpUs);
    serialDebug->print(" us since the last dump, ");
    serialDebug->print(longestPassUs);
    serialDebug->println(" us since boot");
    longestPassSinceDumpUs = 0;

    serialDebug->println("--- Message counters ---");
    serialDebug->print("  PGN 129025 Position NMEA2000: ");
    serialDebug->println(counters.positionNmea2000);
    serialDebug->print("  PGN 129026 Speed NMEA2000:    ");
    serialDebug->println(counters.speedNmea2000);
    serialDebug->print("  PGN 129283 XTE NMEA2000:      ");
    serialDebug->println(counters.xteNmea2000);
    if (counters.xteNmea2000 > 0) {
        serialDebug->print("             last SA=0x");
        serialDebug->print(counters.lastXteNmea2000SourceAddress, HEX);
        serialDebug->print(" len=");
        serialDebug->print(counters.lastXteNmea2000Length);
        serialDebug->print(" raw=");
        GPrintBytesHex(serialDebug, counters.lastXteNmea2000Payload, counters.lastXteNmea2000Length);
        serialDebug->print("  (");
        serialDebug->print(millis() - counters.lastXteNmea2000Ms);
        serialDebug->println(" ms ago)");
        serialDebug->print("             committed=");
        serialDebug->print(counters.xteNmea2000Outcome[kXteRejectNone]);
        serialDebug->print("  rejected: length=");
        serialDebug->print(counters.xteNmea2000Outcome[kXteRejectLength]);
        serialDebug->print(" navTerminated=");
        serialDebug->print(counters.xteNmea2000Outcome[kXteRejectNavigationTerminated]);
        serialDebug->print(" notAvailable=");
        serialDebug->print(counters.xteNmea2000Outcome[kXteRejectNotAvailable]);
        serialDebug->print(" error=");
        serialDebug->println(counters.xteNmea2000Outcome[kXteRejectError]);
    }
    serialDebug->print("  PGN 65267  Position legacy:   ");
    serialDebug->println(counters.positionLegacy);
    serialDebug->print("  PGN 65256  Speed legacy:      ");
    serialDebug->print(counters.speedLegacy);
    serialDebug->print("   last SA=0x");
    serialDebug->print(counters.lastSpeedLegacySourceAddress, HEX);
    serialDebug->print(" raw=0x");
    serialDebug->println(counters.lastSpeedLegacyRaw, HEX);
    // PGN 0xFFFF is every manufacturer's proprietary traffic, so its total
    // says nothing about the XTE -- ~104 frames/s on the John Deere rig, most
    // of it from 0x1C and 0xF0. The carrier line is the one that does (#153).
    serialDebug->print("  PGN 65535  all senders:       ");
    serialDebug->print(counters.xteJohnDeereLegacy);
    serialDebug->print("   last SA=0x");
    serialDebug->println(counters.lastXteJohnDeereLegacySourceAddress, HEX);
    serialDebug->print("  PGN 65535  XTE carrier 0x2A:  ");
    serialDebug->print(counters.xteJohnDeereCarrier);
    if (counters.xteJohnDeereCarrier > 0) {
        serialDebug->print("   (");
        serialDebug->print(millis() - counters.lastXteJohnDeereCarrierMs);
        serialDebug->println(" ms ago)");
    } else {
        serialDebug->println("   (none decoded)");
    }
    // The last frame from a sender the decoder recognises -- 0x2A, or 0x80 for
    // #20's Ag Leader layout work -- all 8 bytes. word/byte1 are the John Deere
    // layout's fields specifically, which is exactly the assumption #20 is
    // trying to replace for Ag Leader, so the payload is printed whole too.
    if (counters.lastXteJohnDeereLegacyPayloadSourceAddress != 0xFF) {
        serialDebug->print("             raw SA=0x");
        serialDebug->print(counters.lastXteJohnDeereLegacyPayloadSourceAddress, HEX);
        serialDebug->print(" word=0x");
        serialDebug->print(counters.lastXteJohnDeereLegacyRawWord, HEX);
        serialDebug->print(" byte1=0x");
        serialDebug->print(counters.lastXteJohnDeereLegacyRawByte1, HEX);
        serialDebug->print(" payload ");
        GPrintPayloadHex(serialDebug, counters.lastXteJohnDeereLegacyPayload);
        serialDebug->print("  (");
        serialDebug->print(millis() - counters.lastXteJohnDeereLegacyPayloadMs);
        serialDebug->println(" ms ago)");
    } else {
        serialDebug->println("             raw: nothing from 0x2A or 0x80 yet");
    }
    serialDebug->print("  PGN 65535  GNSS status 0x80:  ");
    serialDebug->print(counters.agLeaderGnssStatus);
    if (counters.agLeaderGnssStatus > 0) {
        serialDebug->print("   fix mode ");
        serialDebug->print(counters.lastAgLeaderGnssFixMode);
        serialDebug->print(counters.lastAgLeaderGnssFixMode == 2 ? " (RTK fixed)" : " (not RTK fixed)");
        serialDebug->print("  (");
        serialDebug->print(millis() - counters.lastAgLeaderGnssStatusMs);
        serialDebug->println(" ms ago)");
    } else {
        serialDebug->println("   (none)");
    }
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
    // PGN 129029 -- the only message on any bus captured so far that carries a
    // GNSS quality indicator. On a rig without a John Deere (0x2A) or Trimble
    // (0xAA) legacy XTE sender, this is the sole route to a non-zero quality,
    // and therefore the sole route to IsRtkQuality() ever being true.
    {
        static const char* kMethod[6] = { "0 no GNSS", "1 GNSS", "2 DGNSS",
                                          "3 precise", "4 RTK FIXED", "5 RTK float" };
        const auto& c = counters;
        serialDebug->print("  PGN 129029 GNSS position: ");
        if (c.gnssPositionData == 0) {
            serialDebug->println("(none received)");
        } else {
            serialDebug->print(c.gnssPositionData);
            serialDebug->print(" msgs, method=");
            if (c.lastGnssMethod < 6) serialDebug->print(kMethod[c.lastGnssMethod]);
            else                      serialDebug->print(c.lastGnssMethod);
            serialDebug->print("  SVs=");
            serialDebug->print(c.lastGnssSvCount);
            if (c.lastGnssHasHdop) {
                serialDebug->print("  HDOP=");
                serialDebug->print(c.lastGnssHdop, 2);
            }
            serialDebug->print("  last ");
            serialDebug->print(millis() - c.lastGnssPositionMs);
            serialDebug->println(" ms ago");
        }
        serialDebug->print("  PGN 129027 position deltas: ");
        serialDebug->println(c.positionDeltaNmea2000);
    }
    // PGN 44032 -- the standard ISO 11783-7 guidance channel. Diagnostics
    // only: it carries curvature, not cross-track error. Its worth is the
    // status fields, which say why guidance is or is not happening. Session
    // 9's CNH tractor reported LOCKED OUT for all 8420 frames and no other
    // message on that bus said so.
    {
        static const char* kState[4] = { "no/not-ready", "YES/READY", "error", "n/a" };
        const auto& c = counters;
        serialDebug->print("  Guidance machine info (PGN 44032): ");
        if (c.guidanceMachineInfo == 0) {
            serialDebug->println("(none received)");
        } else {
            serialDebug->print(c.guidanceMachineInfo);
            serialDebug->print(" msgs, last ");
            serialDebug->print(millis() - c.lastGuidanceMachineInfoMs);
            serialDebug->println(" ms ago");
            serialDebug->print("    lockout=");
            serialDebug->print(kState[c.lastGuidanceMechanicalLockout & 0x03]);
            serialDebug->print("  steeringReady=");
            serialDebug->print(kState[c.lastGuidanceSteeringReadiness & 0x03]);
            serialDebug->print("  remoteEngage=");
            serialDebug->print(kState[c.lastGuidanceRemoteEngage & 0x03]);
            serialDebug->print("  curvature=");
            if (c.lastGuidanceHasCurvature) {
                serialDebug->print(c.lastGuidanceCurvaturePerKm, 2);
                serialDebug->println(" 1/km");
            } else {
                serialDebug->println("n/a");
            }
        }
    }
    serialDebug->print("  IsRtkQuality=");
    serialDebug->println(guidance->IsRtkQuality() ? "Y" : "N");
    serialDebug->print("  GGA fix age:  ");
    serialDebug->print(millis() - guidance->GetGgaTimestamp());
    serialDebug->println(" ms");
    serialDebug->print("  VTG fix age:  ");
    serialDebug->print(millis() - guidance->GetVtgTimestamp());
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
        // VT failover (VtFailoverPolicy.hpp): switches to another VT, and the
        // address-claim requests sent while looking for one.
        serialDebug->print("  VT failover: switches=");
        serialDebug->print(vtInterface->GetVtSwitchCount());
        serialDebug->print(" claim requests=");
        serialDebug->print(vtInterface->GetVtClaimRequestCount());
        serialDebug->print(" rejoins=");
        serialDebug->print(vtInterface->GetVtRejoinCount());
        // Preferred VT (#255): the number, and where it was last seen.
        serialDebug->print(" preferred=");
        if (vtInterface->GetPreferredVt() == 0) {
            serialDebug->println("none");
        }
        else {
            serialDebug->print("VT");
            serialDebug->print(vtInterface->GetPreferredVt());
            if (vtInterface->GetPreferredVtAddress() == VtFailoverPolicy::kNoAddress) {
                serialDebug->println(" (not seen)");
            }
            else {
                serialDebug->print(" at 0x");
                serialDebug->print(vtInterface->GetPreferredVtAddress(), HEX);
                serialDebug->println(vtInterface->GetPreferredVtAddress() == vtInterface->GetPartnerAddress() ? " (bound)" : " (not bound)");
            }
        }
        if (sections != nullptr) sections->PrintVtDump();
    }

    // The machine's own sections (the plough: lightbar emulation, Task
    // Controller).
    if (sections != nullptr) sections->PrintDump();
}

// ------------------------------------------------------------------
// Periodic one-liner
// ------------------------------------------------------------------
FLASHMEM void IsobusDebugMenu::printPeriodicLine() {
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
    serialDebug->print(now - guidance->GetGgaTimestamp());
    serialDebug->print(" vtgAge=");
    serialDebug->print(now - guidance->GetVtgTimestamp());
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

    // The last PGN 129283 frame as sender:length:bytes (#201), next to the
    // decoded xte= above, so a terminal's on-screen XTE can be checked against
    // the frame on one line, on any brand that sends it.
    if (counters.xteNmea2000 > 0) {
        serialDebug->print(" x129283=");
        serialDebug->print(counters.lastXteNmea2000SourceAddress, HEX);
        serialDebug->print(":");
        serialDebug->print(counters.lastXteNmea2000Length);
        serialDebug->print(":");
        GPrintBytesHex(serialDebug, counters.lastXteNmea2000Payload, counters.lastXteNmea2000Length);
    }

    if (sections != nullptr) sections->PrintPeriodicBeforeVt();

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
    if (sections != nullptr) sections->PrintPeriodicAfterVt();
    serialDebug->println();
}

FLASHMEM void GPrintHexBlob(Stream* out, const char* label, const uint8_t* data, uint32_t length) {
    static const char kHex[] = "0123456789ABCDEF";
    constexpr uint32_t kBytesPerLine = 32;

    out->print("----- BEGIN ");
    out->print(label);
    out->print(" ");
    out->print(length);
    out->println(" bytes -----");

    for (uint32_t i = 0; i < length; i++) {
        out->write(kHex[(data[i] >> 4) & 0x0F]);
        out->write(kHex[data[i] & 0x0F]);
        if (((i + 1) % kBytesPerLine) == 0) {
            out->println();
        }
    }
    if ((length % kBytesPerLine) != 0) {
        out->println();
    }

    out->print("----- END ");
    out->print(label);
    out->println(" -----");
    // Printed with the data on purpose: a hex dump nobody can turn back into
    // a file is not worth capturing.
    out->println("Strip the BEGIN/END lines, then:");
    out->print("  python -c \"import sys;open('");
    out->print(label);
    out->println(".iop','wb').write(bytes.fromhex(''.join(sys.stdin.read().split())))\" < hex.txt");
}

}  // namespace triton

#endif  // ISOBUS
