/*
  PloughDebugMenuSections - the plough's sections of the ISOBUS debug menu
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
#include "PloughDebugMenuSections.hpp"
#include "../FlashMem.hpp"

// The header is itself empty unless ARDUINO and ISOBUS are defined -- guard
// the body too, as IsobusDebugMenu.cpp does. avr-gcc (micro_serial) has no
// <vector> at all.
#if defined(ARDUINO) && defined(ISOBUS)

#include <vector>

namespace triton
{

// The display's pass as its own screen labels it: "0" for the AB line, then
// "1L", "1R", "2L" ... (PGN 65462 byte 2 and byte 6 bit 4, session 17).
static void GPrintLightbarPass(Stream* out, const LightbarXteResult& xte) {
    out->print(xte.pass);
    if (xte.pass > 0) out->print(xte.passLeftOfAbLine ? "L" : "R");
}

// ------------------------------------------------------------------
// The menu: item 4, the DDOP (the plough's Task Controller client)
// ------------------------------------------------------------------
FLASHMEM void PloughDebugMenuSections::PrintMenuItems() {
    serialDebug->println("4. Dump DDOP as hex (open in AgIsoDDOPGenerator)");
}

FLASHMEM bool PloughDebugMenuSections::HandleMenuKey(char c) {
    if (c != '4') return false;
    // The DDOP the firmware actually builds, not the one we think it
    // builds. A pool that fails to serialise here is a pool a
    // terminal would refuse, so the failure path is worth as much as
    // the success path.
    std::vector<std::uint8_t> pool;
    if (tcInterface != nullptr && tcInterface->GenerateDdopBinary(pool) && !pool.empty()) {
        GPrintHexBlob(serialDebug, "DDOP", pool.data(), static_cast<uint32_t>(pool.size()));
    } else {
        serialDebug->println("DDOP not available (not built yet, or AgIsoStack refused to serialise it).");
    }
    return true;
}

// ------------------------------------------------------------------
// Full status dump
// ------------------------------------------------------------------
FLASHMEM void PloughDebugMenuSections::PrintVtDump() {
    // AUX-N (#254): statuses of inputs assigned to Breder / Smaller.
    if (vtWorkingSet != nullptr) {
        serialDebug->print("  AUX-N: Breder events=");
        serialDebug->print(vtWorkingSet->GetAuxWiderEvents());
        serialDebug->print(" Smaller events=");
        serialDebug->print(vtWorkingSet->GetAuxNarrowerEvents());
        serialDebug->print(" held=");
        serialDebug->print(vtWorkingSet->IsWiderHeld() ? "B" : "-");
        serialDebug->print(vtWorkingSet->IsNarrowerHeld() ? "S" : "-");
        serialDebug->println(vtWorkingSet->IsAuxLearnMode() ? " learn mode" : "");
    }
}

FLASHMEM void PloughDebugMenuSections::PrintDump() {
    serialDebug->println("--- Lightbar emulation (Ag Leader L160, PGN 65462) ---");
    if (lightbarChannel == nullptr) {
        serialDebug->println("  (not configured)");
    } else {
        const auto& lc = lightbarChannel->GetCounters();
        const auto& em = lightbarChannel->GetEmulation();
        serialDebug->print("  Mode: ");
        serialDebug->print(LightbarModeName(lightbarChannel->GetMode()));
        serialDebug->print(lightbarChannel->IsRunning() ? "  RUNNING" : "  NOT RUNNING");
        serialDebug->println(lightbarChannel->IsAgLeaderDisplaySeen() ? "  (Ag Leader display seen)"
                                                                      : "  (no Ag Leader display seen)");
        serialDebug->print("  Address claim: ");
        serialDebug->print(lightbarChannel->IsClaimed() ? "CLAIMED  address=0x" : "NOT CLAIMED");
        if (lightbarChannel->IsClaimed()) serialDebug->print(lightbarChannel->GetAddress(), HEX);
        serialDebug->println();
        serialDebug->print("  Display partner: 0x");
        serialDebug->print(em.GetPartnerAddress(), HEX);
        serialDebug->print("  identification requests=");
        serialDebug->print(em.GetRequests());
        serialDebug->print(" answered=");
        serialDebug->print(em.GetResponses());
        serialDebug->print(" unknown=");
        serialDebug->print(em.GetUnknownRequests());
        serialDebug->print("  last DID=0x");
        serialDebug->print(em.GetLastDid(), HEX);
        serialDebug->print("  hello=");
        serialDebug->print(em.GetHelloSent() ? "Y" : "N");
        serialDebug->print("  stack partner=");
        if (lightbarChannel->IsDisplayPartnerBound()) {
            serialDebug->print("0x");
            serialDebug->println(lightbarChannel->GetDisplayPartnerAddress(), HEX);
        } else {
            serialDebug->println("N");
        }
        serialDebug->print("  Sent: frames=");
        serialDebug->print(lc.framesSent);
        serialDebug->print(" failures=");
        serialDebug->print(lc.sendFailures);
        serialDebug->print(" heartbeats=");
        serialDebug->print(lc.heartbeats);
        serialDebug->print(" softwareId=");
        serialDebug->print(lc.softwareIdSent ? "Y" : "N");
        serialDebug->print("  proprietary A from display=");
        serialDebug->println(em.GetProprietaryAReceived());
        serialDebug->print("  Requests: softwareId=");
        serialDebug->print(em.GetSoftwareIdRequests());
        serialDebug->print(" (sent ");
        serialDebug->print(lc.softwareIdSends);
        serialDebug->print("x) ignored=");
        serialDebug->print(em.GetIgnoredRequests());
        serialDebug->print(" last ignored PGN=");
        serialDebug->println(em.GetLastIgnoredPgn());
        serialDebug->print("  PGN 65462 XTE: frames=");
        serialDebug->print(lc.xteFrames);
        serialDebug->print(" committed=");
        serialDebug->print(lc.xteCommitted);
        if (lc.xteFrames > 0) {
            serialDebug->print("  last ");
            serialDebug->print(millis() - lc.lastXteMs);
            serialDebug->print(" ms ago from 0x");
            serialDebug->print(lc.lastXteSourceAddress, HEX);
            serialDebug->print("  raw=");
            GPrintPayloadHex(serialDebug, lc.lastXte.rawPayload);
            serialDebug->print("  -> ");
            serialDebug->print(lc.lastXte.magnitudeCm);
            serialDebug->print(" cm ");
            serialDebug->print(lc.lastXte.rightOfLine ? "right" : (lc.lastXte.leftOfLine ? "left" : "on the line"));
            if (lc.lastXte.hasPass) {
                serialDebug->print(", pass ");
                GPrintLightbarPass(serialDebug, lc.lastXte);
            }
            serialDebug->print(lc.lastXte.engaged ? ", autosteer engaged" : ", autosteer off");
            serialDebug->print(lc.lastXte.hasLine ? ", line" : ", no line");
            serialDebug->print(lc.lastXte.valid ? "  (committed)" : "  (not committed)");
        }
        serialDebug->println();
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
        // --- What reached our handlers, via AgIsoStack -------------------
        // Labelled as such deliberately. Session 9 read "Value commands: 0"
        // as "the TC never commanded us" when the TC was commanding us at
        // that moment; the number was not wrong, the label was.
        serialDebug->print("  [via stack] Value cmds to handler: ");
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
        serialDebug->print("  [via stack] Value req callbacks:  ");
        serialDebug->print(tcInterface->GetValueRequestCount());
        serialDebug->println("   (mostly AgIsoStack's own re-polling -- NOT bus traffic)");

        // --- What is actually on the wire, addressed to us ----------------
        serialDebug->print("  [on bus] Requests for our values: ");
        serialDebug->println(tcInterface->GetBusRequestValueCount());
        serialDebug->print("  [on bus] Set-value commands:      ");
        serialDebug->println(tcInterface->GetBusSetValueCount());
        serialDebug->print("  [on bus] Measurement commands:    ");
        serialDebug->print(tcInterface->GetBusMeasurementCount());
        if (tcInterface->GetBusMeasurementCount() > 0) {
            serialDebug->print("  last DDI=");
            serialDebug->print(tcInterface->GetLastBusMeasurementDdi());
            serialDebug->print(" type=");
            serialDebug->print(tcInterface->GetLastBusMeasurementType());
            serialDebug->print(" (");
            serialDebug->print(millis() - tcInterface->GetLastBusMeasurementMs());
            serialDebug->println(" ms ago)");
        } else {
            serialDebug->println("  (none)");
        }
        serialDebug->print("  [on bus] Other, addressed to us:  ");
        serialDebug->println(tcInterface->GetBusOtherProcessDataCount());
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
            serialDebug->println("(none yet)");
        } else {
            serialDebug->println("(not connected)");
        }
        // What the TC polls: the AGCO TC on the Fendt read 515 at ~1 Hz and
        // 506 every ~10 s without ever commanding anything (#316).
        serialDebug->print("  [on bus] Requests for DDI 506: ");
        serialDebug->print(tcInterface->GetBusRequest506Count());
        serialDebug->print("  DDI 515: ");
        serialDebug->println(tcInterface->GetBusRequest515Count());
        serialDebug->print("  Track control state (DDI 515): ");
        if (tcInterface->HasTrackControlState()) {
            serialDebug->print("commanded=");
            serialDebug->print(tcInterface->GetCommandedTrackControlState());
            serialDebug->print(" (");
            serialDebug->print(millis() - tcInterface->GetTrackControlCommandMs());
            serialDebug->print(" ms ago)");
        } else {
            serialDebug->print("never commanded");
        }
        serialDebug->print("  we answer=");
        serialDebug->print(tcInterface->GetReportedTrackControlState());
        serialDebug->println(tcInterface->IsTrackControlEchoOn() ? "  [SPIKE #316: echo ON]" : "");
        // Guidance-track DDIs 508-511. Worth reading even when 506 stays
        // silent: a terminal that populates track numbering speaks this part
        // of the protocol whether or not it completes the tramline handshake,
        // which is a different and useful answer.
        serialDebug->print("  Guidance track (DDI 507-511): ");
        if (tcInterface->HasGuidanceTrackInfo()) {
            serialDebug->print("seq=");
            serialDebug->print(tcInterface->GetTramlineSequenceNumber());
            serialDebug->print(" abLine=");
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
FLASHMEM void PloughDebugMenuSections::PrintPeriodicBeforeVt() {
    // The InCommand's lightbar XTE, raw, on the same line for the same reason
    // as xteraw: the side bit is still unconfirmed against deliberate offsets.
    if (lightbarChannel != nullptr && lightbarChannel->GetCounters().xteFrames > 0) {
        const auto& lc = lightbarChannel->GetCounters();
        serialDebug->print(" lb=");
        serialDebug->print(lc.lastXte.magnitudeCm);
        serialDebug->print(lc.lastXte.rightOfLine ? "R" : (lc.lastXte.leftOfLine ? "L" : "C"));
        serialDebug->print(lc.lastXte.engaged ? "e" : "-");
        if (lc.lastXte.hasPass) {
            serialDebug->print("p");
            GPrintLightbarPass(serialDebug, lc.lastXte);
        }
        serialDebug->print(":");
        GPrintPayloadHex(serialDebug, lc.lastXte.rawPayload);
    }
}

FLASHMEM void PloughDebugMenuSections::PrintPeriodicAfterVt() {
    if (tcInterface != nullptr) {
        serialDebug->print(" tc=");
        serialDebug->print(tcInterface->IsConnected() ? "Y" : "N");
        serialDebug->print(" drp=");
        serialDebug->print(tcInterface->GetDrpDeviationMm());
        serialDebug->print("mm tcq=");
        serialDebug->print(tcInterface->GetTcGnssQuality());
    }
}

}  // namespace triton

#endif  // ARDUINO && ISOBUS
