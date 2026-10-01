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
#pragma once

#include <stdint.h>

namespace triton
{

// AgIsoStack binds a VirtualTerminalClient's partner once, to the first VT on
// the bus that matches its NAME filters, and never re-binds it: the partner's
// NAME is locked after the first match (can_network_manager.cpp's
// update_new_partners() is gated on !initialized). So when that VT goes away
// while another VT stays on the bus, the client waits for it forever.
//
// Field evidence, session 13 repeat (2026-10-01, New Holland + Ag Leader): the
// CNH VT (0x26) was switched off for 4 minutes while the InCommand 1200's VT
// (0x80) broadcast its VT Status at 1 Hz the whole time. The client stayed in
// WaitForPartnerVTStatusMessage, bound to 0x26, until the plough control was
// rebooted -- after which it bound to 0x80 at once.
//
// A second trap found in the same capture: 0x80 had been pruned from
// AgIsoStack's address table at the InCommand's join (15:13:54) and never
// re-claimed, and the network manager only hands a global message to callbacks
// when its sender is in that table (can_network_manager.cpp:978-979). So the
// InCommand's VT Status never reached us at all. A failover therefore first has
// to bring every live CF back into the table -- a global request for address
// claim does that, each answer restoring its sender -- and only then can it see
// which VT is alive.
//
// This class only decides; IsobusVtInterface acts. It knows nothing of
// AgIsoStack, so the whole decision is testable without a bus.
class VtFailoverPolicy {
public:
    // How long the bound VT may be silent (no VT Status Message) while we are
    // not connected before we look elsewhere. Five times AgIsoStack's own 3 s
    // VT status timeout: a VT that is merely rebooting or busy is given time
    // to come back first.
    static constexpr unsigned long kPartnerSilentMs = 15000UL;
    // A candidate VT counts as alive only if its VT Status was seen this
    // recently. A VT broadcasts it at 1 Hz; a display whose VT function is
    // switched off keeps claiming its address but stops broadcasting, so the
    // address table alone cannot tell the two apart.
    static constexpr unsigned long kCandidateFreshMs = 3000UL;
    // Spacing between our requests for address claim while no live VT is
    // known: each one makes every CF on the bus answer, so it is rate-limited.
    static constexpr unsigned long kRequestIntervalMs = 10000UL;

    static constexpr uint8_t kNoAddress = 0xFE;

    enum class Action : uint8_t {
        None,
        RequestAddressClaims,  // send a global request for address claim (PGN 60928)
        SwitchTo,              // rebind the client to the VT at Decision::address
    };

    struct Decision {
        Action  action  = Action::None;
        uint8_t address = kNoAddress;
    };

    // Starts the bound VT's grace period: from Begin(), and again after every
    // switch, so a freshly bound VT is never judged on the old one's silence.
    void Start(unsigned long nowMs);

    // A VT Status Message (VT-to-ECU, function 0xFE) arrived from `source`.
    void OnVtStatus(uint8_t source, unsigned long nowMs);

    // What to do now. `partnerAddress` and `partnerValid` describe the bound
    // partner as AgIsoStack sees it.
    Decision Evaluate(unsigned long nowMs, bool connected, uint8_t partnerAddress, bool partnerValid) const;

    // Report back that a decision was carried out.
    void OnRequestSent(unsigned long nowMs);
    void OnSwitched(unsigned long nowMs);

    unsigned int GetSwitchCount() const { return switchCount; }
    unsigned int GetRequestCount() const { return requestCount; }

private:
    bool HeardRecently(uint8_t address, unsigned long nowMs, unsigned long windowMs) const;

    // Per source address: when its last VT Status arrived, and whether one
    // ever has (0 is a valid millis() value, so it cannot double as "never").
    unsigned long lastStatusMs[256] = {};
    bool          heard[256]        = {};

    unsigned long startMs       = 0;
    unsigned long lastRequestMs = 0;
    bool          requested     = false;
    unsigned int  switchCount   = 0;
    unsigned int  requestCount  = 0;
};

}  // namespace triton
