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
