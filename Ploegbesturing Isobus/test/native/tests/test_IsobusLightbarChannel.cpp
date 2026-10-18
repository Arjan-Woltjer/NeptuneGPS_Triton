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
#include <isobus/isobus/can_identifier.hpp>
#include <isobus/isobus/can_message.hpp>
#include <isobus/isobus/can_NAME.hpp>
#include <isobus/isobus/can_NAME_filter.hpp>
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
    static bool SoftwareIdRequest(IsobusLightbarChannel& channel, bool& ack, isobus::AcknowledgementType& type) {
        return IsobusLightbarChannel::OnSoftwareIdRequest(0xFEDA, nullptr, ack, type, &channel);
    }
    static bool IgnoredRequest(IsobusLightbarChannel& channel, std::uint32_t pgn, bool& ack, isobus::AcknowledgementType& type) {
        return IsobusLightbarChannel::OnIgnoredRequest(pgn, nullptr, ack, type, &channel);
    }
};
}  // namespace triton

namespace {

// 29-bit identifiers as they appeared in card log 42, channel 2.
constexpr std::uint32_t kIdXte            = 0x18FFB6F5;   // PGN 65462 from the display's 0xF5
constexpr std::uint32_t kIdDiagnosticToUs = 0x18DADCF5;   // PGN 0xDA00, 0xF5 -> 0xDC
constexpr std::uint32_t kIdDiagnosticToOther = 0x18DA26F5;// 0xF5 -> 0x26
constexpr std::uint32_t kIdProprietaryAToUs = 0x18EFDCF5;

// NAMEs from session 13 (van Mastwijk), wire order B16E200C28800020 read as
// the little-endian 64-bit value AgIsoStack's NAME takes.
constexpr std::uint64_t kNameDisplayF5 = 0x200080280C206EB1ULL;   // InCommand display function
constexpr std::uint64_t kNameE9        = 0x800080000C206EB1ULL;   // same unit, function 128, instance 0
constexpr std::uint64_t kName2B        = 0x800081000C206EB1ULL;   // same unit, function 129
constexpr std::uint64_t kNameVt80      = 0x80001D000C206EB1ULL;   // InCommand VT
constexpr std::uint64_t kNameTcF7      = 0xA00082000C206EB1ULL;   // InCommand TC
constexpr std::uint64_t kNameCnhVt26   = 0xA0001D080BC23101ULL;   // CNH VT

// A partner binds when every filter matches (each parameter appears once).
bool DisplayFiltersMatch(std::uint64_t rawName) {
    const isobus::NAME name(rawName);
    for (const auto& filter : IsobusLightbarChannel::DisplayNameFilters()) {
        if (!filter.check_name_matches_filter(name)) return false;
    }
    return true;
}

}  // namespace

// Session 16: the prune took 0xF5 offline for 6 s, so the display becomes the
// channel's partner. The filter must pick the display function and nothing
// else the InCommand (or the CNH) claims on that bus.
test(IsobusLightbarChannel, displayPartnerFilter_matchesOnlyTheDisplayFunction) {
    assertEqual(isobus::NAME(kNameDisplayF5).get_function_instance(), (std::uint8_t)5);   // decoding sanity
    assertEqual(isobus::NAME(kNameDisplayF5).get_manufacturer_code(), (std::uint16_t)97);
    assertTrue(DisplayFiltersMatch(kNameDisplayF5));
    assertFalse(DisplayFiltersMatch(kNameE9));
    assertFalse(DisplayFiltersMatch(kName2B));
    assertFalse(DisplayFiltersMatch(kNameVt80));
    assertFalse(DisplayFiltersMatch(kNameTcF7));
    assertFalse(DisplayFiltersMatch(kNameCnhVt26));
}

// Never begun: no partner, and the dump says so.
test(IsobusLightbarChannel, displayPartner_unboundBeforeBegin) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    assertFalse(channel.IsDisplayPartnerBound());
    assertEqual((int)channel.GetDisplayPartnerAddress(), 0xFF);
}

// 233.7 s: engaged, 1 cm right on the 1200 -> -0.01 m committed (#252), counted as such.
test(IsobusLightbarChannel, xte_engagedFrame_commits) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 1, 0x00, 0x02, 0x00, 121, 0x00, 0x93, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);

    assertEqual(guidance.GetXte(), -1);
    const auto& c = channel.GetCounters();
    assertEqual(c.xteFrames, (std::uint32_t)1);
    assertEqual(c.xteCommitted, (std::uint32_t)1);
    assertEqual((int)c.lastXteSourceAddress, 0xF5);
    assertTrue(c.lastXte.valid);
    assertTrue(c.lastXte.rightOfLine);
    assertEqual((int)c.lastXte.rawPayload[6], 0x93);
}

// Log 42, 181.9 s: 148 cm right with byte 2 = 1 -- the AB line (session 17),
// not "no solution" -> committed.
test(IsobusLightbarChannel, xte_abLine_commits) {
    GuidanceSource guidance;
    guidance.SetXte(-250);
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 0x94, 0x00, 0x01, 0x00, 127, 0x00, 0xA1, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);

    assertEqual(guidance.GetXte(), -148);
    const auto& c = channel.GetCounters();
    assertEqual(c.xteFrames, (std::uint32_t)1);
    assertEqual(c.xteCommitted, (std::uint32_t)1);
    assertFalse(c.lastXte.engaged);
}

// Session 17, 09:21:25.650: the display up without a line yet (byte 7 = 1) ->
// counted, raw kept, nothing committed; GuidanceSource keeps what it had.
test(IsobusLightbarChannel, xte_noLine_countedNotCommitted) {
    GuidanceSource guidance;
    guidance.SetXte(-250);
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x22, 0x01 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);

    assertEqual(guidance.GetXte(), -250);
    const auto& c = channel.GetCounters();
    assertEqual(c.xteFrames, (std::uint32_t)1);
    assertEqual(c.xteCommitted, (std::uint32_t)0);
    assertEqual((int)c.lastXte.rawPayload[7], 0x01);
}

// 220 s: 34 cm left on the 1200 -> +0.34 m (#252).
test(IsobusLightbarChannel, xte_displayLeft_positive) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 34, 0x00, 0x02, 0x00, 115, 0x00, 0x53, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);
    assertEqual(guidance.GetXte(), 34);
}

// Session 16, 20:57:30: 90 cm right, state 3, autosteer NOT engaged -> committed
// (the XTE is followed whether or not the tractor steers itself).
test(IsobusLightbarChannel, xte_state3NotEngaged_commits) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    const std::uint8_t d[8] = { 0x5A, 0x00, 0x03, 0x00, 0x1B, 0x01, 0x91, 0x04 };
    IsobusLightbarChannelTestAccess::Deliver(IsobusLightbarChannelTestAccess::Xte(), channel, kIdXte, d, 8);
    assertEqual(guidance.GetXte(), -90);
    assertEqual(channel.GetCounters().xteCommitted, (std::uint32_t)1);
    assertFalse(channel.GetCounters().lastXte.engaged);
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

// Session 15: the display's requests for 65242, 64965, 64653 and our own claim
// must never be NACKed. The callbacks report them handled with no
// acknowledgement; 65242 schedules the software ID BAM.
test(IsobusLightbarChannel, requestCallbacks_handledWithoutAcknowledgement) {
    GuidanceSource guidance;
    IsobusLightbarChannel channel(nullptr, &guidance);
    bool ack = true;
    isobus::AcknowledgementType type = isobus::AcknowledgementType::Positive;

    assertTrue(IsobusLightbarChannelTestAccess::SoftwareIdRequest(channel, ack, type));
    assertFalse(ack);
    assertEqual(channel.GetEmulation().GetSoftwareIdRequests(), (std::uint32_t)1);

    ack = true;
    assertTrue(IsobusLightbarChannelTestAccess::IgnoredRequest(channel, 0xFDC5, ack, type));
    assertFalse(ack);
    ack = true;
    assertTrue(IsobusLightbarChannelTestAccess::IgnoredRequest(channel, 0xEE00, ack, type));
    assertFalse(ack);
    assertEqual(channel.GetEmulation().GetIgnoredRequests(), (std::uint32_t)2);
    assertEqual(channel.GetEmulation().GetLastIgnoredPgn(), (std::uint32_t)0xEE00);
}
#endif  // ISOBUS
