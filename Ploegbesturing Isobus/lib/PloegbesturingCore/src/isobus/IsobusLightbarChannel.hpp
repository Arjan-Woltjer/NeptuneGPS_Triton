/*
  IsobusLightbarChannel - a second ISOBUS control function that presents as
  an Ag Leader L160 lightbar and reads the InCommand's cross-track error
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

// Same guard as IsobusGuidanceChannel.hpp, for the same LDF reason.
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/isobus/can_callbacks.hpp>
#include <isobus/isobus/can_internal_control_function.hpp>
#include <isobus/isobus/can_message.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "AgLeaderLightbarEmulation.hpp"
#include "GuidanceSource.hpp"
#include "IsobusPgnDecode.hpp"

namespace triton
{

// An Ag Leader InCommand does not put its cross-track error on the ISOBUS
// for just anyone: on the 2026-10-08 capture (Triton canlogs, card log 42)
// PGN 65462 started within 0.2 s of an L160 lightbar finishing its
// identification exchange, and was absent from every earlier capture without
// one (#42). This channel claims a second control function next to
// IsobusGuidanceChannel's, with the L160's NAME, answers the display's
// identification the way the bar does (AgLeaderLightbarEmulation), keeps up
// the bar's heartbeat, and commits every valid 65462 to GuidanceSource.
//
// The identity it presents is Ag Leader's (manufacturer 97) and the answers
// are the captured L160's, part number included. That is a deliberate,
// owner-approved choice for the first rig test, not a default to ship
// unexamined; which of those values the display actually checks is unknown.
//
// The stack is already running when this is constructed: IsobusGuidanceChannel
// brought the hardware interface up and blocked on its own claim. This one's
// claim is left to the stack and finishes within Update().
class IsobusLightbarChannel {
public:
    // canPort is AgIsoStack's CAN port index, the one the guidance channel
    // assigned the hardware plugin to (0) -- not the FlexCAN channel number.
    IsobusLightbarChannel(Stream* serialDebug, GuidanceSource* guidance, std::uint8_t canPort = 0);

    // Creates the control function (its claim completes asynchronously) and
    // registers the three PGN callbacks. Call once after
    // IsobusGuidanceChannel::Begin().
    void Begin();

    // Call every loop(): sends the software identification once the address
    // is claimed, the heartbeat, and whatever the emulation has queued.
    void Update();

    inline std::shared_ptr<isobus::InternalControlFunction> GetControlFunction() { return controlFunction; }
    bool IsClaimed() const;
    std::uint8_t GetAddress() const;

    struct Counters {
        std::uint32_t xteFrames = 0;        // every PGN 65462 seen
        std::uint32_t xteCommitted = 0;     // those that reached GuidanceSource
        std::uint8_t  lastXteSourceAddress = 0xFF;
        unsigned long lastXteMs = 0;
        LightbarXteResult lastXte;          // raw payload and decode of the last frame
        std::uint32_t diagnosticFrames = 0; // PGN 0xDA00 addressed to us
        std::uint32_t framesSent = 0;
        std::uint32_t sendFailures = 0;     // stack refused, or no destination CF
        std::uint32_t heartbeats = 0;
        bool          softwareIdSent = false;
        std::uint32_t softwareIdSends = 0;   // after the claim, on request, on first contact
    };
    inline const Counters& GetCounters() const { return counters; }
    inline void ResetCounters() { counters = Counters(); }
    inline const AgLeaderLightbarEmulation& GetEmulation() const { return emulation; }

private:
    friend struct IsobusLightbarChannelTestAccess;

    Stream*         serialDebug;
    GuidanceSource* guidance;
    std::uint8_t    canPort;

    std::shared_ptr<isobus::InternalControlFunction> controlFunction;
    AgLeaderLightbarEmulation emulation;
    Counters                  counters;

    // The claimed address, cached once valid so the callbacks can tell frames
    // addressed to us from the rest of PGN 0xDA00 / 0xEF00 traffic. 0xFF
    // until claimed. The native tests set it directly.
    std::uint8_t claimedAddress = 0xFF;
    bool         claimAnnounced = false;

    bool IsAddressedToUs(const isobus::CANMessage& msg) const;
    void Send(const AgLeaderLightbarEmulation::Outgoing& outgoing);
    void SendGlobal(std::uint32_t pgn, const std::uint8_t* data, std::uint32_t length);

    // PGN request callbacks on the lightbar control function's own request
    // protocol: 65242 answered with the software identification, the rest
    // swallowed without a NACK (session 15: the stack's NACKs are what the
    // real bar never sends).
    static bool OnSoftwareIdRequest(std::uint32_t pgn, std::shared_ptr<isobus::ControlFunction> requester,
                                    bool& acknowledge, isobus::AcknowledgementType& acknowledgeType, void* context);
    static bool OnIgnoredRequest(std::uint32_t pgn, std::shared_ptr<isobus::ControlFunction> requester,
                                 bool& acknowledge, isobus::AcknowledgementType& acknowledgeType, void* context);
    void SendSoftwareIdentification();

    static void OnDiagnostic(const isobus::CANMessage& msg, void* context);
    static void OnProprietaryA(const isobus::CANMessage& msg, void* context);
    static void OnXte(const isobus::CANMessage& msg, void* context);
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
