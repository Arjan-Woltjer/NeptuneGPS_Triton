/*
  VtFailoverPolicy - decides when the plough's VT client should give up on its
  Virtual Terminal and move to another one on the bus
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
#include "VtFailoverPolicy.hpp"

namespace triton
{

void VtFailoverPolicy::Start(unsigned long nowMs) {
    startMs   = nowMs;
    requested = false;
}

void VtFailoverPolicy::OnVtStatus(uint8_t source, unsigned long nowMs) {
    lastStatusMs[source] = nowMs;
    heard[source]        = true;
}

bool VtFailoverPolicy::HeardRecently(uint8_t address, unsigned long nowMs, unsigned long windowMs) const {
    return heard[address] && (nowMs - lastStatusMs[address]) <= windowMs;
}

VtFailoverPolicy::Decision VtFailoverPolicy::Evaluate(unsigned long nowMs, bool connected, uint8_t partnerAddress,
                                                      bool partnerValid) const {
    Decision decision;
    if (connected) return decision;

    // Silent since the later of the grace period's start and the bound VT's
    // last status. A partner that is not address-valid has no status of its
    // own to go by.
    unsigned long silentSince = startMs;
    if (partnerValid && heard[partnerAddress] && (lastStatusMs[partnerAddress] - startMs) < 0x80000000UL) {
        silentSince = lastStatusMs[partnerAddress];
    }
    if (nowMs - silentSince < kPartnerSilentMs) return decision;

    // Lowest live address that is not the bound VT's own: deterministic, and
    // the same order AgIsoStack's own first bind scans the table in.
    for (unsigned int address = 0; address < kNoAddress; address++) {
        if (partnerValid && address == partnerAddress) continue;
        if (HeardRecently(static_cast<uint8_t>(address), nowMs, kCandidateFreshMs)) {
            decision.action  = Action::SwitchTo;
            decision.address = static_cast<uint8_t>(address);
            return decision;
        }
    }

    if (!requested || nowMs - lastRequestMs >= kRequestIntervalMs) {
        decision.action = Action::RequestAddressClaims;
    }
    return decision;
}

void VtFailoverPolicy::OnRequestSent(unsigned long nowMs) {
    lastRequestMs = nowMs;
    requested     = true;
    requestCount++;
}

void VtFailoverPolicy::OnSwitched(unsigned long nowMs) {
    switchCount++;
    Start(nowMs);
}

}  // namespace triton
