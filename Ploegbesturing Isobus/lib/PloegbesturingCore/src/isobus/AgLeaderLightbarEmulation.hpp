/*
  AgLeaderLightbarEmulation - answers an Ag Leader InCommand the way its L160
  lightbar does, so the display starts broadcasting cross-track error
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
#pragma once

#include <cstdint>

namespace triton
{

// The protocol half of IsobusLightbarChannel, with no CAN stack in it so the
// whole exchange is native-testable byte for byte against the capture it was
// taken from (Triton canlogs, card log 42, 2026-10-08; Documentation
// ISOBUS/research/agleader-incommand-65462-xte-2026-10-08.md).
//
// What the capture shows: an InCommand 1200 only broadcasts its cross-track
// error (PGN 65462, see IsobusPgnDecode's DecodeXteAgLeaderLightbar) once a
// lightbar has identified itself. The identification is an ISO 15765-2
// exchange on PGN 0xDA00, addressed, in which the display's control function
// (0xF5 on that rig) reads seven UDS data identifiers with ReadDataByIdentifier
// (0x22) and the bar answers each with 0x62. Short answers fit a single frame;
// the three string answers go as a first frame, a flow control from the
// display (block size 50, STmin 1 ms) and two consecutive frames. The bar also
// sends one Proprietary A frame to the display and a Proprietary B heartbeat
// (PGN 65513, all 0xFF) every ~1.7 s; the display answers the identification
// with one Proprietary A frame of its own and then starts 65461/65462 within
// 0.2 s.
//
// This class holds the identifier table and the ISO 15765-2 responder state.
// It consumes frames addressed to the lightbar control function and produces
// frames to send; the channel moves them on and off the bus.
class AgLeaderLightbarEmulation {
public:
    struct Frame {
        std::uint8_t length = 0;
        std::uint8_t data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    };

    // An outgoing frame and where it goes. pgn is one of the three below;
    // destination is the address of the display's control function.
    struct Outgoing {
        std::uint32_t pgn = 0;
        std::uint8_t  destination = 0xFF;
        Frame         frame;
    };

    static constexpr std::uint32_t kPgnDiagnostic   = 0xDA00;   // ISO 15765-2 on ISOBUS, PDU1
    static constexpr std::uint32_t kPgnProprietaryA = 0xEF00;   // PDU1
    static constexpr std::uint32_t kPgnHeartbeat    = 0xFFE9;   // 65513, proprietary B, all 0xFF
    static constexpr std::uint32_t kPgnSoftwareIdentification = 0xFEDA;  // 65242, sent once as a BAM
    static constexpr unsigned long kHeartbeatIntervalMs = 1667;

    // The values the InCommand read from the L160 on 2026-10-08, byte for
    // byte. Which of them it actually checks is unknown; the first rig test
    // runs with the captured set, and anything proven irrelevant can be made
    // our own afterwards. 0x8007 looks like a serial number, 0x8008 is the
    // L160's part number.
    struct Identity {
        std::uint8_t did8007[4] = { 0x77, 0xCF, 0xDA, 0x0E };
        std::uint8_t did8003[2] = { 0x01, 0x19 };
        const char*  did8006 = "AL L160 ";   // 16-byte field, zero padded
        const char*  did8008 = "4001595";    // 16-byte field, zero padded
        std::uint8_t did8009 = 0x06;
        const char*  did8014 = "AL L160";    // 16-byte field, zero padded
        std::uint8_t did8015 = 0x01;
    };

    // Two constructors rather than a defaulted argument: GCC refuses a default
    // argument built from a nested struct's own member initializers inside the
    // enclosing class.
    AgLeaderLightbarEmulation();
    explicit AgLeaderLightbarEmulation(const Identity& identity);

    // PGN 65242 payload the L160 broadcasts at power-up: the field count byte
    // and "ALTECH,AL L160;01.00.00.00;L160_UP_FW;01.05.00.00;*", 52 bytes.
    static const std::uint8_t* SoftwareIdentification(std::uint8_t& length);

    // A frame on PGN 0xDA00 addressed to the lightbar control function.
    // Returns true when it was a request this class answers (the answer is
    // queued) or a flow control it was waiting for.
    bool OnDiagnosticFrame(std::uint8_t sourceAddress, const std::uint8_t* data, std::uint8_t length,
                           unsigned long nowMs);

    // A Proprietary A frame addressed to the lightbar; counted only.
    void OnProprietaryA(std::uint8_t sourceAddress);

    // Next frame to put on the bus, if any. Consecutive frames come out one per
    // call, no sooner than the flow control's STmin after the previous one.
    bool PopFrame(Outgoing& out, unsigned long nowMs);

    // True once per heartbeat interval; the caller then sends PGN 65513 with
    // eight 0xFF bytes.
    bool HeartbeatDue(unsigned long nowMs);

    // Diagnostics for the debug dump.
    std::uint8_t  GetPartnerAddress() const { return partnerAddress; }
    std::uint32_t GetRequests()       const { return requests; }
    std::uint32_t GetResponses()      const { return responses; }
    std::uint32_t GetUnknownRequests() const { return unknownRequests; }
    std::uint16_t GetLastDid()        const { return lastDid; }
    bool          GetHelloSent()      const { return helloSent; }
    unsigned long GetLastRequestMs()  const { return lastRequestMs; }
    std::uint32_t GetProprietaryAReceived() const { return proprietaryAReceived; }

private:
    Identity identity;

    // A small FIFO of single frames, enough for the hello plus one answer.
    static constexpr std::uint8_t kQueueSize = 4;
    Outgoing     queue[kQueueSize];
    std::uint8_t queueHead = 0, queueCount = 0;
    bool Enqueue(const Outgoing& o);

    // The multi-frame answer in flight: payload, how much has gone, the
    // flow-control parameters, and the pacing.
    std::uint8_t  pending[24] = { 0 };
    std::uint8_t  pendingLength = 0;
    std::uint8_t  pendingSent = 0;
    std::uint8_t  pendingSequence = 0;
    bool          pendingWaitingForFlowControl = false;
    std::uint8_t  pendingDestination = 0xFF;
    std::uint8_t  stMinMs = 0;
    unsigned long lastConsecutiveMs = 0;

    std::uint8_t  partnerAddress = 0xFF;
    bool          helloSent = false;
    std::uint32_t requests = 0, responses = 0, unknownRequests = 0, proprietaryAReceived = 0;
    std::uint16_t lastDid = 0;
    unsigned long lastRequestMs = 0;
    unsigned long lastHeartbeatMs = 0;
    bool          heartbeatStarted = false;

    // Builds the UDS positive response (0x62, DID, value) for one identifier
    // into `payload`; returns its length, 0 when the identifier is unknown.
    std::uint8_t BuildResponse(std::uint16_t did, std::uint8_t* payload) const;
    void QueueSingleFrame(std::uint8_t destination, const std::uint8_t* payload, std::uint8_t length);
    void StartMultiFrame(std::uint8_t destination, const std::uint8_t* payload, std::uint8_t length);
};

}  // namespace triton
