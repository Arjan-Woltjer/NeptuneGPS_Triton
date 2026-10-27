/*
  test_VtFailoverPolicy - Tests for the decision behind the VT client's
  failover to another Virtual Terminal on the bus.
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
#include <AUnit.h>

#include "isobus/VtFailoverPolicy.hpp"
#include "isobus/PreferredVt.hpp"

using namespace aunit;
using namespace triton;

namespace {

using Action = VtFailoverPolicy::Action;

// The addresses of session 13's two VTs: the CNH VT the client was bound to,
// and the InCommand 1200's.
constexpr uint8_t kCnhVt      = 0x26;
constexpr uint8_t kInCommand  = 0x80;
constexpr unsigned long kT0   = 1000;

// The field case up to the moment the CNH VT goes quiet: bound to 0x26,
// hearing both VTs, then 0x26 stops.
VtFailoverPolicy BoundToCnhThatStopsAt(unsigned long stopMs) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    policy.OnVtStatus(kCnhVt, stopMs);
    return policy;
}

}  // namespace

// Session 15: the client said "connected" for five minutes while the bound
// VT was dead, because the other VT's status kept its timer fresh. The
// policy's own per-address record is what counts: bound VT silent for a
// minute, another VT alive -> switch, connected or not.
test(VtFailoverPolicy, connected_butPartnerSilent_actsAnyway) {
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0);
    policy.OnVtStatus(kInCommand, kT0 + 60000);
    const VtFailoverPolicy::Decision d = policy.Evaluate(kT0 + 60000, true, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kInCommand);
}

test(VtFailoverPolicy, connected_partnerHeardRecently_noAction) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    for (unsigned long t = kT0; t <= kT0 + 60000; t += 1000) {
        policy.OnVtStatus(kCnhVt, t);
        policy.OnVtStatus(kInCommand, t);
    }
    assertTrue(policy.Evaluate(kT0 + 60000, true, kCnhVt, true).action == Action::None);
    // Just inside the window still counts as heard.
    assertTrue(policy.Evaluate(kT0 + 60000 + VtFailoverPolicy::kPartnerSilentMs - 1, true, kCnhVt, true).action == Action::None);
}

// Connected with a partner that is not address-valid: nothing of its own to
// judge by, leave the client to it.
test(VtFailoverPolicy, connected_invalidPartner_noAction) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    assertTrue(policy.Evaluate(kT0 + 60000, true, VtFailoverPolicy::kNoAddress, false).action == Action::None);
}

test(VtFailoverPolicy, partnerStillTalking_noAction) {
    // Disconnected but the bound VT is broadcasting: a handshake in progress,
    // or a VT that rejected us for its own reasons. Not ours to override.
    VtFailoverPolicy policy;
    policy.Start(kT0);
    for (unsigned long t = kT0; t <= kT0 + 60000; t += 1000) {
        policy.OnVtStatus(kCnhVt, t);
        policy.OnVtStatus(kInCommand, t);
    }
    assertTrue(policy.Evaluate(kT0 + 60000, false, kCnhVt, true).action == Action::None);
}

test(VtFailoverPolicy, partnerSilentJustUnderTheLimit_noAction) {
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0 + 5000);
    policy.OnVtStatus(kInCommand, kT0 + 19000);
    const unsigned long now = kT0 + 5000 + VtFailoverPolicy::kPartnerSilentMs - 1;
    assertTrue(policy.Evaluate(now, false, kCnhVt, true).action == Action::None);
}

test(VtFailoverPolicy, partnerSilent_anotherVtAlive_switchesToIt) {
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0 + 5000);
    const unsigned long now = kT0 + 5000 + VtFailoverPolicy::kPartnerSilentMs;
    policy.OnVtStatus(kInCommand, now - 500);
    const VtFailoverPolicy::Decision d = policy.Evaluate(now, false, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kInCommand);
}

test(VtFailoverPolicy, partnerSilent_noOtherVtKnown_requestsAddressClaims) {
    // The session 13 case as it really was: the InCommand's status never
    // reached us because 0x80 had been pruned from the address table.
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0 + 5000);
    const unsigned long now = kT0 + 5000 + VtFailoverPolicy::kPartnerSilentMs;
    assertTrue(policy.Evaluate(now, false, kCnhVt, true).action == Action::RequestAddressClaims);
}

test(VtFailoverPolicy, requestsAreSpacedByTheInterval) {
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0);
    const unsigned long first = kT0 + VtFailoverPolicy::kPartnerSilentMs;
    assertTrue(policy.Evaluate(first, false, kCnhVt, true).action == Action::RequestAddressClaims);
    policy.OnRequestSent(first);
    assertTrue(policy.Evaluate(first + 1, false, kCnhVt, true).action == Action::None);
    assertTrue(policy.Evaluate(first + VtFailoverPolicy::kRequestIntervalMs - 1, false, kCnhVt, true).action ==
               Action::None);
    assertTrue(policy.Evaluate(first + VtFailoverPolicy::kRequestIntervalMs, false, kCnhVt, true).action ==
               Action::RequestAddressClaims);
    assertEqual(policy.GetRequestCount(), (unsigned int)1);
}

test(VtFailoverPolicy, aStaleCandidateIsNotAlive) {
    // A display whose VT function is off keeps its address but stops
    // broadcasting. Heard once, long ago, is not alive.
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0 + 5000);
    const unsigned long now = kT0 + 5000 + VtFailoverPolicy::kPartnerSilentMs;
    policy.OnVtStatus(kInCommand, now - VtFailoverPolicy::kCandidateFreshMs - 1);
    assertTrue(policy.Evaluate(now, false, kCnhVt, true).action == Action::RequestAddressClaims);
}

test(VtFailoverPolicy, theRequestThenTheAnswerThenTheSwitch) {
    // The whole sequence the interface runs through: silence, a request,
    // the InCommand's status arriving once it is back in the table, a switch.
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0);
    unsigned long now = kT0 + VtFailoverPolicy::kPartnerSilentMs;
    assertTrue(policy.Evaluate(now, false, kCnhVt, true).action == Action::RequestAddressClaims);
    policy.OnRequestSent(now);
    now += 1000;
    policy.OnVtStatus(kInCommand, now);
    const VtFailoverPolicy::Decision d = policy.Evaluate(now, false, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kInCommand);
}

test(VtFailoverPolicy, neverSwitchesToTheBoundVtsOwnAddress) {
    // Status from the partner's own address is the partner, however the
    // silence arithmetic comes out.
    VtFailoverPolicy policy;
    policy.Start(kT0);
    const unsigned long now = kT0 + VtFailoverPolicy::kPartnerSilentMs + 5000;
    policy.OnVtStatus(kCnhVt, now - 100);
    assertTrue(policy.Evaluate(now, false, kCnhVt, true).action != Action::SwitchTo);
}

test(VtFailoverPolicy, aPartnerNeverHeardIsJudgedFromStart) {
    // Bound at boot to a VT whose VT function is off: it claims its address
    // but never broadcasts. The grace period runs from Start().
    VtFailoverPolicy policy;
    policy.Start(kT0);
    policy.OnVtStatus(kInCommand, kT0 + VtFailoverPolicy::kPartnerSilentMs - 100);
    assertTrue(policy.Evaluate(kT0 + VtFailoverPolicy::kPartnerSilentMs - 1, false, kCnhVt, true).action ==
               Action::None);
    const VtFailoverPolicy::Decision d = policy.Evaluate(kT0 + VtFailoverPolicy::kPartnerSilentMs, false, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kInCommand);
}

test(VtFailoverPolicy, anInvalidPartnerTakesAnyLiveVt) {
    // Partner evicted (address 0xFE): there is no own address to skip.
    VtFailoverPolicy policy;
    policy.Start(kT0);
    const unsigned long now = kT0 + VtFailoverPolicy::kPartnerSilentMs;
    policy.OnVtStatus(kCnhVt, now - 10);
    const VtFailoverPolicy::Decision d = policy.Evaluate(now, false, VtFailoverPolicy::kNoAddress, false);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kCnhVt);
}

test(VtFailoverPolicy, picksTheLowestLiveAddress) {
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0);
    const unsigned long now = kT0 + VtFailoverPolicy::kPartnerSilentMs;
    policy.OnVtStatus(0xA0, now - 10);
    policy.OnVtStatus(kInCommand, now - 10);
    assertEqual(policy.Evaluate(now, false, kCnhVt, true).address, kInCommand);
}

test(VtFailoverPolicy, aSwitchStartsAFreshGracePeriod) {
    // Right after a switch the new partner has not spoken yet. It must get
    // the full grace period, or we would bounce straight back.
    VtFailoverPolicy policy = BoundToCnhThatStopsAt(kT0);
    unsigned long now = kT0 + VtFailoverPolicy::kPartnerSilentMs;
    policy.OnVtStatus(kInCommand, now);
    policy.OnSwitched(now);
    assertEqual(policy.GetSwitchCount(), (unsigned int)1);
    // The old VT comes back and talks; the new partner is still silent.
    policy.OnVtStatus(kCnhVt, now + 1000);
    assertTrue(policy.Evaluate(now + 1000, false, kInCommand, true).action == Action::None);
    assertTrue(policy.Evaluate(now + VtFailoverPolicy::kPartnerSilentMs - 1, false, kInCommand, true).action ==
               Action::None);
}

test(VtFailoverPolicy, staleStatusFromBeforeStartDoesNotCount) {
    // The partner's last status predates Start(): silence is measured from
    // Start, not from that old status (which would trigger at once).
    VtFailoverPolicy policy;
    policy.OnVtStatus(kCnhVt, kT0);
    policy.Start(kT0 + 100000);
    policy.OnVtStatus(kInCommand, kT0 + 100000 + 1000);
    assertTrue(policy.Evaluate(kT0 + 100000 + 1000, false, kCnhVt, true).action == Action::None);
}

// --- Rejoining the bus -------------------------------------------------
// Session 18's R2 (2026-10-17): the plough's ISOBUS plug was out for 63 s. On
// the rejoin the InCommand's status (0x80) arrived first, and 0.4 s later the
// plough switched to it: 0x26 had been "silent" for a minute, but only because
// we were not on the bus. Every calibration-wizard stall ended the same way.

namespace {

// Both VTs and the rest of the bus heard every second from kT0 to `untilMs`.
void OnTheBusWithBothVtsUntil(VtFailoverPolicy& policy, unsigned long untilMs) {
    for (unsigned long t = kT0; t <= untilMs; t += 1000) {
        policy.OnBusTraffic(t);
        policy.OnVtStatus(kCnhVt, t);
        policy.OnVtStatus(kInCommand, t);
    }
}

}  // namespace

test(VtFailoverPolicy, rejoin_afterTimeOffTheBus_keepsTheOldPartner) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    const unsigned long unplugged = kT0 + 30000;
    OnTheBusWithBothVtsUntil(policy, unplugged);
    const unsigned long rejoin = unplugged + 63000;
    policy.OnBusTraffic(rejoin);
    policy.OnVtStatus(kInCommand, rejoin);
    assertTrue(policy.Evaluate(rejoin + 400, false, kCnhVt, true).action == Action::None);
    assertEqual(policy.GetRejoinCount(), (unsigned int)1);
    // 0x26 speaks again within its 1 s cycle: bound to it as before.
    policy.OnVtStatus(kCnhVt, rejoin + 800);
    assertTrue(policy.Evaluate(rejoin + 1000, true, kCnhVt, true).action == Action::None);
}

// If the old partner does not come back, the switch waits for the grace
// period to end, then happens at once (it has been silent far longer than
// kPartnerSilentMs by then).
test(VtFailoverPolicy, rejoin_oldPartnerGone_switchesWhenTheGraceEnds) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    const unsigned long unplugged = kT0 + 30000;
    OnTheBusWithBothVtsUntil(policy, unplugged);
    const unsigned long rejoin = unplugged + 63000;
    for (unsigned long t = rejoin; t <= rejoin + VtFailoverPolicy::kRejoinGraceMs; t += 1000) {
        policy.OnBusTraffic(t);
        policy.OnVtStatus(kInCommand, t);
    }
    assertEqual(VtFailoverPolicy::kRejoinGraceMs, 120000UL);
    assertTrue(policy.Evaluate(rejoin + VtFailoverPolicy::kRejoinGraceMs - 1, false, kCnhVt, true).action ==
               Action::None);
    const VtFailoverPolicy::Decision d = policy.Evaluate(rejoin + VtFailoverPolicy::kRejoinGraceMs, false, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kInCommand);
}

// A loop that did not run (the calibration wizard, #31) is a gap in the
// traffic just like an unplug: nothing was received while it was open.
test(VtFailoverPolicy, rejoin_afterALoopStall_keepsTheOldPartner) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    const unsigned long stalled = kT0 + 30000;
    OnTheBusWithBothVtsUntil(policy, stalled);
    const unsigned long resumed = stalled + 30000;
    policy.OnBusTraffic(resumed);
    policy.OnVtStatus(kInCommand, resumed);
    assertTrue(policy.Evaluate(resumed, false, kCnhVt, true).action == Action::None);
}

// On the bus all along (F1/F3, and session 13's pruned VT whose status never
// reached us): no rejoin, the 15 s switch stays.
test(VtFailoverPolicy, onTheBusAllAlong_partnerSilent_switchesAfterTheUsualTime) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    OnTheBusWithBothVtsUntil(policy, kT0 + 30000);
    const unsigned long stop = kT0 + 30000;
    for (unsigned long t = stop + 1000; t <= stop + VtFailoverPolicy::kPartnerSilentMs; t += 1000) {
        policy.OnBusTraffic(t);
        policy.OnVtStatus(kInCommand, t);
    }
    const VtFailoverPolicy::Decision d = policy.Evaluate(stop + VtFailoverPolicy::kPartnerSilentMs, false, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(policy.GetRejoinCount(), (unsigned int)0);
}

// The first traffic after boot is not a rejoin: nothing was heard before it.
test(VtFailoverPolicy, firstTrafficAfterBoot_isNotARejoin) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    policy.OnBusTraffic(kT0 + 60000);
    policy.OnVtStatus(kInCommand, kT0 + 60000);
    assertTrue(policy.Evaluate(kT0 + 60000, false, kCnhVt, true).action == Action::SwitchTo);
    assertEqual(policy.GetRejoinCount(), (unsigned int)0);
}

test(VtFailoverPolicy, survivesMillisWraparound) {
    const unsigned long nearWrap = 0xFFFFFFFFUL - 5000;
    VtFailoverPolicy policy;
    policy.Start(nearWrap);
    policy.OnVtStatus(kCnhVt, nearWrap + 1000);
    const unsigned long now = nearWrap + 1000 + VtFailoverPolicy::kPartnerSilentMs;  // wrapped past zero
    policy.OnVtStatus(kInCommand, now - 50);
    const VtFailoverPolicy::Decision d = policy.Evaluate(now, false, kCnhVt, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kInCommand);
    assertTrue(policy.Evaluate(nearWrap + 1000 + VtFailoverPolicy::kPartnerSilentMs - 1, false, kCnhVt, true).action ==
               Action::None);
}

// ---------------------------------------------------------------------------
// Preferred VT (#255): the operator's choice wins once that VT is up. Owner:
// no wait at boot (bind to the first VT), and move back to the preferred one
// as soon as it is available again.
// ---------------------------------------------------------------------------
namespace {
// Both VTs broadcast their status at 1 Hz from `fromMs` to `toMs`.
void BothVtsTalk(VtFailoverPolicy& policy, unsigned long fromMs, unsigned long toMs) {
    for (unsigned long t = fromMs; t <= toMs; t += 1000) {
        policy.OnVtStatus(kCnhVt, t);
        policy.OnVtStatus(kInCommand, t);
    }
}
}  // namespace

test(VtFailoverPolicy, preferred_settled_movesFromTheOtherVt) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    BothVtsTalk(policy, kT0, kT0 + VtFailoverPolicy::kPreferredSettledMs);
    policy.SetPreferredAddress(kCnhVt);
    // Bound to the InCommand, connected and talking: only the preference moves it.
    const VtFailoverPolicy::Decision d =
        policy.Evaluate(kT0 + VtFailoverPolicy::kPreferredSettledMs, true, kInCommand, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kCnhVt);
}

test(VtFailoverPolicy, preferred_stillBooting_staysPut) {
    // The CNH VT only just started broadcasting: not settled yet.
    VtFailoverPolicy fresh;
    fresh.Start(kT0);
    for (unsigned long t = kT0; t <= kT0 + 10000; t += 1000) fresh.OnVtStatus(kInCommand, t);
    fresh.OnVtStatus(kCnhVt, kT0 + 8000);
    fresh.OnVtStatus(kCnhVt, kT0 + 9000);
    fresh.OnVtStatus(kCnhVt, kT0 + 10000);
    fresh.SetPreferredAddress(kCnhVt);
    assertTrue(fresh.Evaluate(kT0 + 10000, true, kInCommand, true).action == Action::None);
    fresh.OnVtStatus(kCnhVt, kT0 + 11000);
    fresh.OnVtStatus(kCnhVt, kT0 + 12000);
    fresh.OnVtStatus(kCnhVt, kT0 + 13000);
    assertTrue(fresh.Evaluate(kT0 + 13000, true, kInCommand, true).action == Action::SwitchTo);
}

test(VtFailoverPolicy, preferred_alreadyBound_noAction) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    BothVtsTalk(policy, kT0, kT0 + 10000);
    policy.SetPreferredAddress(kCnhVt);
    assertTrue(policy.Evaluate(kT0 + 10000, true, kCnhVt, true).action == Action::None);
}

test(VtFailoverPolicy, noPreference_staysOnTheOtherVt) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    BothVtsTalk(policy, kT0, kT0 + 10000);
    policy.SetPreferredAddress(VtFailoverPolicy::kNoAddress);
    assertTrue(policy.Evaluate(kT0 + 10000, true, kInCommand, true).action == Action::None);
}

// A gap in the preferred VT's status (it rebooted, or flaps) starts the
// settling over, so a flapping VT is not chased.
test(VtFailoverPolicy, preferred_gapRestartsTheSettling) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    for (unsigned long t = kT0; t <= kT0 + 2000; t += 1000) policy.OnVtStatus(kCnhVt, t);
    // 5 s without the CNH VT's status (longer than kCandidateFreshMs), then back from kT0 + 7000.
    for (unsigned long t = kT0; t <= kT0 + 11000; t += 1000) policy.OnVtStatus(kInCommand, t);
    for (unsigned long t = kT0 + 7000; t <= kT0 + 11000; t += 1000) policy.OnVtStatus(kCnhVt, t);
    policy.SetPreferredAddress(kCnhVt);
    assertTrue(policy.Evaluate(kT0 + 11000, true, kInCommand, true).action == Action::None);   // 4 s since it came back
    policy.OnVtStatus(kInCommand, kT0 + 12000);
    policy.OnVtStatus(kCnhVt, kT0 + 12000);
    assertTrue(policy.Evaluate(kT0 + 12000, true, kInCommand, true).action == Action::SwitchTo);
}

// Right after any switch the new binding is left alone for a while, so the
// client is never rebuilt twice in a row while its partner resolves.
test(VtFailoverPolicy, preferred_notRightAfterASwitch) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    BothVtsTalk(policy, kT0, kT0 + 24000);
    policy.SetPreferredAddress(kCnhVt);
    policy.OnSwitched(kT0 + 20000);
    // Partner not resolved yet (invalid): still no second switch inside the hold-off.
    assertTrue(policy.Evaluate(kT0 + 20000 + VtFailoverPolicy::kPreferredSettledMs - 1, true, kCnhVt, false).action ==
               Action::None);
    BothVtsTalk(policy, kT0 + 25000, kT0 + 25000);
    assertTrue(policy.Evaluate(kT0 + 20000 + VtFailoverPolicy::kPreferredSettledMs, true, kInCommand, true).action ==
               Action::SwitchTo);
}

test(VtFailoverPolicy, preferred_silent_noMove) {
    VtFailoverPolicy policy;
    policy.Start(kT0);
    BothVtsTalk(policy, kT0, kT0 + 10000);
    for (unsigned long t = kT0 + 11000; t <= kT0 + 20000; t += 1000) policy.OnVtStatus(kInCommand, t);
    policy.SetPreferredAddress(kCnhVt);                   // last heard at kT0 + 10000
    assertTrue(policy.Evaluate(kT0 + 20000, true, kInCommand, true).action == Action::None);
}

// The bound VT goes silent and the preferred one is up: the failover goes to
// the preferred VT, not to the lowest live address.
test(VtFailoverPolicy, partnerSilent_preferredUp_goesToThePreferred) {
    constexpr uint8_t kThirdVt = 0x10;
    VtFailoverPolicy policy;
    policy.Start(kT0);
    policy.OnVtStatus(kInCommand, kT0);                   // bound VT, then silent
    for (unsigned long t = kT0; t <= kT0 + 20000; t += 1000) {
        policy.OnVtStatus(kCnhVt, t);
        policy.OnVtStatus(kThirdVt, t);
    }
    policy.SetPreferredAddress(kCnhVt);
    const VtFailoverPolicy::Decision d = policy.Evaluate(kT0 + 20000, false, kInCommand, true);
    assertTrue(d.action == Action::SwitchTo);
    assertEqual(d.address, kCnhVt);
}

// The setting is the VT number as the terminals show it: function instance + 1
// (owner: the 1200, instance 0, is VT 1; the CNH VT, instance 1, is VT 2).
test(PreferredVt, matchesTheVtNumberAsTheTerminalsShowIt) {
    assertTrue(IsPreferredVt(true, 0, 1));     // the InCommand 1200
    assertTrue(IsPreferredVt(true, 1, 2));     // the CNH VT
    assertFalse(IsPreferredVt(true, 1, 1));
    assertFalse(IsPreferredVt(true, 0, 0));    // 0 = no preference
    assertFalse(IsPreferredVt(false, 1, 2));   // not a Virtual Terminal
}

// Block 1 of the EEPROM map, next to the GPS bytes and the lightbar mode.
test(PreferredVt, atByte14_inBlock1) {
    assertEqual(kEepromPreferredVt, 14);
}

// 14 is the only byte read: with 14 erased there is no preference, whatever
// another byte holds (70 belongs to the Planter block in the EEPROM map).
test(PreferredVt, onlyByte14IsRead) {
    EEPROM.eepromReset();
    EEPROM.write(70, 2);
    assertEqual((int)ReadPreferredVt(), 0);
    WritePreferredVt(1);
    assertEqual((int)ReadPreferredVt(), 1);
    assertEqual((int)EEPROM.read(70), 2);   // left alone
    EEPROM.eepromReset();
}

test(PreferredVt, storedAsTheNumber_erasedOrOutOfRangeIsNone) {
    EEPROM.eepromReset();
    assertEqual((int)ReadPreferredVt(), 0);    // erased (255)
    WritePreferredVt(2);
    assertEqual((int)EEPROM.read(kEepromPreferredVt), 2);
    assertEqual((int)ReadPreferredVt(), 2);
    EEPROM.write(kEepromPreferredVt, kHighestPreferredVt + 1);
    assertEqual((int)ReadPreferredVt(), 0);
    WritePreferredVt(kHighestPreferredVt + 1);
    assertEqual((int)EEPROM.read(kEepromPreferredVt), 0);
}
