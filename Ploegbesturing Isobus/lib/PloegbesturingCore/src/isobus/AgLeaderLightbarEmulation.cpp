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
#include "AgLeaderLightbarEmulation.hpp"

namespace triton
{

namespace {

// ISO 15765-2 protocol control information, high nibble of byte 0.
constexpr std::uint8_t kSingleFrame      = 0x0;
constexpr std::uint8_t kFirstFrame       = 0x1;
constexpr std::uint8_t kConsecutiveFrame = 0x2;
constexpr std::uint8_t kFlowControl      = 0x3;

// UDS service identifiers and negative response codes.
constexpr std::uint8_t kReadDataByIdentifier         = 0x22;
constexpr std::uint8_t kPositiveResponseOffset       = 0x40;
constexpr std::uint8_t kNegativeResponse             = 0x7F;
constexpr std::uint8_t kNrcServiceNotSupported       = 0x11;
constexpr std::uint8_t kNrcRequestOutOfRange         = 0x31;

// The strings the L160 returns sit in 16-byte fields, zero padded.
constexpr std::uint8_t kStringFieldLength = 16;

// 0x01 = one field, then the field itself, '*' terminated. 52 bytes.
const std::uint8_t kSoftwareIdentification[] = {
    0x01,
    'A', 'L', 'T', 'E', 'C', 'H', ',', 'A', 'L', ' ', 'L', '1', '6', '0', ';',
    '0', '1', '.', '0', '0', '.', '0', '0', '.', '0', '0', ';',
    'L', '1', '6', '0', '_', 'U', 'P', '_', 'F', 'W', ';',
    '0', '1', '.', '0', '5', '.', '0', '0', '.', '0', '0', ';', '*'
};

void GCopyStringField(const char* text, std::uint8_t* out) {
    std::uint8_t i = 0;
    for (; i < kStringFieldLength && text[i] != '\0'; i++) out[i] = static_cast<std::uint8_t>(text[i]);
    for (; i < kStringFieldLength; i++) out[i] = 0x00;
}

}  // namespace

AgLeaderLightbarEmulation::AgLeaderLightbarEmulation() : identity() {
}

AgLeaderLightbarEmulation::AgLeaderLightbarEmulation(const Identity& identity) : identity(identity) {
}

const std::uint8_t* AgLeaderLightbarEmulation::SoftwareIdentification(std::uint8_t& length) {
    length = static_cast<std::uint8_t>(sizeof(kSoftwareIdentification));
    return kSoftwareIdentification;
}

bool AgLeaderLightbarEmulation::Enqueue(const Outgoing& o) {
    if (queueCount >= kQueueSize) return false;
    queue[(queueHead + queueCount) % kQueueSize] = o;
    queueCount++;
    return true;
}

// The response is 0x62, the identifier big-endian as UDS writes it, then the
// value: 4, 2, 16, 16, 1, 16 and 1 bytes for the seven identifiers.
std::uint8_t AgLeaderLightbarEmulation::BuildResponse(std::uint16_t did, std::uint8_t* payload) const {
    payload[0] = kReadDataByIdentifier + kPositiveResponseOffset;
    payload[1] = static_cast<std::uint8_t>(did >> 8);
    payload[2] = static_cast<std::uint8_t>(did & 0xFF);
    std::uint8_t* value = payload + 3;
    switch (did) {
        case 0x8007: for (std::uint8_t i = 0; i < 4; i++) value[i] = identity.did8007[i]; return 3 + 4;
        case 0x8003: value[0] = identity.did8003[0]; value[1] = identity.did8003[1]; return 3 + 2;
        case 0x8006: GCopyStringField(identity.did8006, value); return 3 + kStringFieldLength;
        case 0x8008: GCopyStringField(identity.did8008, value); return 3 + kStringFieldLength;
        case 0x8009: value[0] = identity.did8009; value[1] = 0; value[2] = 0; value[3] = 0; return 3 + 4;
        case 0x8014: GCopyStringField(identity.did8014, value); return 3 + kStringFieldLength;
        case 0x8015: value[0] = identity.did8015; value[1] = 0; value[2] = 0; value[3] = 0; return 3 + 4;
        default:     return 0;
    }
}

// Single frame: byte 0 is the payload length (high nibble 0), then the
// payload, padded with 0xFF to eight bytes as the L160 does.
void AgLeaderLightbarEmulation::QueueSingleFrame(std::uint8_t destination, const std::uint8_t* payload, std::uint8_t length) {
    Outgoing o;
    o.pgn = kPgnDiagnostic;
    o.destination = destination;
    o.frame.length = 8;
    o.frame.data[0] = length;
    for (std::uint8_t i = 0; i < 7; i++) o.frame.data[1 + i] = (i < length) ? payload[i] : 0xFF;
    Enqueue(o);
}

// First frame: 0x1L LL then the first six payload bytes; the rest waits for the
// display's flow control and then goes out as consecutive frames 0x21, 0x22...
// of up to seven bytes, the last one as short as what is left (DLC 7 in the
// capture, no padding).
void AgLeaderLightbarEmulation::StartMultiFrame(std::uint8_t destination, const std::uint8_t* payload, std::uint8_t length) {
    Outgoing o;
    o.pgn = kPgnDiagnostic;
    o.destination = destination;
    o.frame.length = 8;
    o.frame.data[0] = static_cast<std::uint8_t>((kFirstFrame << 4) | ((length >> 8) & 0x0F));
    o.frame.data[1] = static_cast<std::uint8_t>(length & 0xFF);
    for (std::uint8_t i = 0; i < 6; i++) o.frame.data[2 + i] = payload[i];
    Enqueue(o);

    for (std::uint8_t i = 0; i < length && i < sizeof(pending); i++) pending[i] = payload[i];
    pendingLength = length;
    pendingSent = 6;
    pendingSequence = 1;
    pendingDestination = destination;
    pendingWaitingForFlowControl = true;
}

bool AgLeaderLightbarEmulation::OnDiagnosticFrame(std::uint8_t sourceAddress, const std::uint8_t* data, std::uint8_t length,
                                                  unsigned long nowMs) {
    if (length < 3) return false;
    const std::uint8_t pci = data[0] >> 4;

    if (pci == kFlowControl) {
        // 0x30 = continue to send, then block size and STmin. Only the STmin
        // matters here: the three string answers are 19 bytes, two
        // consecutive frames, well inside any block size.
        if (!pendingWaitingForFlowControl || sourceAddress != pendingDestination) return false;
        if ((data[0] & 0x0F) != 0) return true;   // wait / overflow: keep waiting
        stMinMs = (length >= 3 && data[2] <= 0x7F) ? data[2] : 0;
        pendingWaitingForFlowControl = false;
        lastConsecutiveMs = nowMs;
        return true;
    }

    if (pci != kSingleFrame) return false;
    const std::uint8_t payloadLength = data[0] & 0x0F;
    if (payloadLength < 1 || payloadLength + 1 > length) return false;

    requests++;
    lastRequestMs = nowMs;
    partnerAddress = sourceAddress;

    // The L160 sends its Proprietary A frame to the display before its first
    // answer; keep that order.
    if (!helloSent) {
        Outgoing hello;
        hello.pgn = kPgnProprietaryA;
        hello.destination = sourceAddress;
        hello.frame.length = 8;
        const std::uint8_t helloData[8] = { 0x00, 0x02, 0x00, 0x64, 0x00, 0xFF, 0xFF, 0xFF };
        for (std::uint8_t i = 0; i < 8; i++) hello.frame.data[i] = helloData[i];
        Enqueue(hello);
        helloSent = true;
    }

    const std::uint8_t service = data[1];
    if (service != kReadDataByIdentifier || payloadLength < 3) {
        unknownRequests++;
        const std::uint8_t nrc[3] = { kNegativeResponse, service, kNrcServiceNotSupported };
        QueueSingleFrame(sourceAddress, nrc, 3);
        return true;
    }

    const std::uint16_t did = static_cast<std::uint16_t>((data[2] << 8) | data[3]);
    lastDid = did;
    std::uint8_t payload[3 + kStringFieldLength];
    const std::uint8_t responseLength = BuildResponse(did, payload);
    if (responseLength == 0) {
        unknownRequests++;
        const std::uint8_t nrc[3] = { kNegativeResponse, service, kNrcRequestOutOfRange };
        QueueSingleFrame(sourceAddress, nrc, 3);
        return true;
    }

    responses++;
    if (responseLength <= 7) QueueSingleFrame(sourceAddress, payload, responseLength);
    else                     StartMultiFrame(sourceAddress, payload, responseLength);
    return true;
}

void AgLeaderLightbarEmulation::OnProprietaryA(std::uint8_t sourceAddress) {
    proprietaryAReceived++;
    if (partnerAddress == 0xFF) partnerAddress = sourceAddress;
}

bool AgLeaderLightbarEmulation::PopFrame(Outgoing& out, unsigned long nowMs) {
    if (queueCount > 0) {
        out = queue[queueHead];
        queueHead = static_cast<std::uint8_t>((queueHead + 1) % kQueueSize);
        queueCount--;
        return true;
    }

    // Consecutive frames, paced by STmin, once the flow control came in.
    if (pendingLength > 0 && !pendingWaitingForFlowControl && pendingSent < pendingLength) {
        if (nowMs - lastConsecutiveMs < stMinMs) return false;
        const std::uint8_t remaining = static_cast<std::uint8_t>(pendingLength - pendingSent);
        const std::uint8_t chunk = remaining < 7 ? remaining : 7;
        out.pgn = kPgnDiagnostic;
        out.destination = pendingDestination;
        out.frame.length = static_cast<std::uint8_t>(1 + chunk);
        out.frame.data[0] = static_cast<std::uint8_t>((kConsecutiveFrame << 4) | (pendingSequence & 0x0F));
        for (std::uint8_t i = 0; i < chunk; i++) out.frame.data[1 + i] = pending[pendingSent + i];
        for (std::uint8_t i = static_cast<std::uint8_t>(1 + chunk); i < 8; i++) out.frame.data[i] = 0;
        pendingSent = static_cast<std::uint8_t>(pendingSent + chunk);
        pendingSequence++;
        lastConsecutiveMs = nowMs;
        if (pendingSent >= pendingLength) pendingLength = 0;
        return true;
    }
    return false;
}

bool AgLeaderLightbarEmulation::HeartbeatDue(unsigned long nowMs) {
    if (!heartbeatStarted || nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
        heartbeatStarted = true;
        lastHeartbeatMs = nowMs;
        return true;
    }
    return false;
}

}  // namespace triton
