/*
  test_IsobusLightbarChannel - what the lightbar channel's callbacks commit,
  count and refuse, driven with the CANMessage AgIsoStack would hand them.
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
#ifdef ISOBUS
#include <vector>
#include <AUnit.h>
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <can_identifier.hpp>
#include <can_message.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "isobus/IsobusLightbarChannel.hpp"

using namespace aunit;
using namespace triton;

namespace triton
{
struct IsobusLightbarChannelTestAccess {
    using Callback = void (*)(const isobus::CANMessage&, void*);
    static void Deliver(Callback callback, IsobusLightbarChannel& channel,
                        std::uint32_t identifier, const std::uint8_t* data, std::uint8_t length) {
        const isobus::CANMessage message(isobus::CANMessage::Type::Receive,
                                         isobus::CANIdentifier(identifier),
                                         data, length, nullptr, nullptr, 0);
        callback(message, &channel);
    }
    static Callback Diagnostic()   { return IsobusLightbarChannel::OnDiagnostic; }
    static Callback ProprietaryA() { return IsobusLightbarChannel::OnProprietaryA; }
    static Callback Xte()          { return IsobusLightbarChannel::OnXte; }
    static void SetClaimedAddress(IsobusLightbarChannel& channel, std::uint8_t address) { channel.claimedAddress = address; }
};
}  // namespace triton

namespace {

// 29-bit identifiers as they appeared in card log 42, channel 2.
constexpr std::uint32_t kIdXte            = 0x18FFB6F5;   // PGN 65462 from the display's 0xF5
constexpr std::uint32_t kIdDiagnosticToUs = 0x18DADCF5;   // PGN 0xDA00, 0xF5 -> 0xDC
constexpr std::uint32_t kIdDiagnosticToOther = 0x18DA26F5;// 0xF5 -> 0x26
constexpr std::uint32_t kIdProprietaryAToUs = 0x18EFDCF5;

}  // namespace

// 233.7 s: engaged, 1 cm right -> +0.01 m committed, counted as such.
test(IsobusLightbarChannel, xte_engagedFrame_commits) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 1, 0x00, 0x02, 0x00, 121, 0x00, 0x93, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);

    assertEqual(guidance.GetXte(), 1);
    const auto& c = channel.GetCounters();
    assertEqual(c.xteFrames, (std::uint32_t)1);
    assertEqual(c.xteCommitted, (std::uint32_t)1);
    assertEqual((int)c.lastXteSourceAddress, 0xF5);
    assertTrue(c.lastXte.valid);
    assertTrue(c.lastXte.rightOfLine);
    assertEqual((int)c.lastXte.rawPayload[6], 0x93);
}

// 181.9 s: 148 cm right but not engaged -> counted, raw kept, nothing
// committed; GuidanceSource keeps what it had.
test(IsobusLightbarChannel, xte_notEngaged_countedNotCommitted) {
    GuidanceSource guidance;
    guidance.SetXte(-250);
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 0x94, 0x00, 0x01, 0x00, 127, 0x00, 0xA1, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);

    assertEqual(guidance.GetXte(), -250);
    const auto& c = channel.GetCounters();
    assertEqual(c.xteFrames, (std::uint32_t)1);
    assertEqual(c.xteCommitted, (std::uint32_t)0);
    assertEqual((int)c.lastXte.magnitudeCm, 148);
    assertFalse(c.lastXte.engaged);
}

// 220 s: 34 cm left -> -0.34 m.
test(IsobusLightbarChannel, xte_leftFrame_negative) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 34, 0x00, 0x02, 0x00, 115, 0x00, 0x53, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);
    assertEqual(guidance.GetXte(), -34);
}

// A diagnostic request addressed to our claimed address reaches the
// emulation; one addressed elsewhere does not. Before the claim nothing is
// ours.
test(IsobusLightbarChannel, diagnostic_onlyWhenAddressedToUs) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t request[6] = { 0x05, 0x22, 0x80, 0x03, 0x01, 0x01 };

    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Diagnostic(), channel, kIdDiagnosticToUs, request, 6);
    assertEqual(channel.GetCounters().diagnosticFrames, (std::uint32_t)0);   // not claimed yet
    assertEqual(channel.GetEmulation().GetRequests(), (std::uint32_t)0);

    IsobusLightbarChannelTestAccess::SetClaimedAddress(channel, 0xDC);
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Diagnostic(), channel, kIdDiagnosticToOther, request, 6);
    assertEqual(channel.GetCounters().diagnosticFrames, (std::uint32_t)0);

    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Diagnostic(), channel, kIdDiagnosticToUs, request, 6);
    assertEqual(channel.GetCounters().diagnosticFrames, (std::uint32_t)1);
    assertEqual(channel.GetEmulation().GetRequests(), (std::uint32_t)1);
    assertEqual((int)channel.GetEmulation().GetPartnerAddress(), 0xF5);
    assertEqual((int)channel.GetEmulation().GetLastDid(), 0x8003);
}

test(IsobusLightbarChannel, proprietaryA_countedWhenAddressedToUs) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    IsobusLightbarChannelTestAccess::SetClaimedAddress(channel, 0xDC);
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::ProprietaryA(), channel, kIdProprietaryAToUs, d, 8);
    assertEqual(channel.GetEmulation().GetProprietaryAReceived(), (std::uint32_t)1);
}

// Not claimed, not configured: the channel reports no address.
test(IsobusLightbarChannel, unclaimed_reportsNoAddress) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    assertFalse(channel.IsClaimed());
    assertEqual((int)channel.GetAddress(), 0xFF);
}
#endif  // ISOBUS
