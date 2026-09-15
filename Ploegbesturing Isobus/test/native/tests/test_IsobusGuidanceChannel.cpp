/*
  test_IsobusGuidanceChannel - Tests for the ISOBUS guidance gateway: what each
  PGN callback commits to the GuidanceSource, what it counts, what it refuses,
  and the All Implement Stop path that stops the plough
  (NeptuneGPS_Triton#98).
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

// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <vector> uses std::min/max with three arguments.
#include <vector>

#include <AUnit.h>

// Same reason, for the library headers this file reaches directly.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <can_identifier.hpp>
#include <can_message.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "isobus/IsobusGuidanceChannel.hpp"

using namespace aunit;
using namespace triton;

namespace triton
{
// The hook IsobusGuidanceChannel declares a friend. Each helper builds the
// CANMessage AgIsoStack would have handed the callback and calls it directly:
// the source control function is null on purpose, because every callback
// reads the sender's address off the identifier rather than through
// get_source_control_function() -- that accessor was permanently null for
// these legacy senders on real hardware (2026-08-08), which is why the code
// stopped using it.
struct IsobusGuidanceChannelTestAccess {
    using Callback = void (*)(const isobus::CANMessage&, void*);

    static void Deliver(Callback callback, IsobusGuidanceChannel& channel,
                        std::uint32_t identifier, const std::uint8_t* data, std::uint8_t length) {
        const isobus::CANMessage message(isobus::CANMessage::Type::Receive,
                                         isobus::CANIdentifier(identifier),
                                         data, length, nullptr, nullptr, 0);
        callback(message, &channel);
    }

    static Callback PositionNmea2000()  { return IsobusGuidanceChannel::OnPositionNmea2000; }
    static Callback SpeedNmea2000()     { return IsobusGuidanceChannel::OnSpeedNmea2000; }
    static Callback XteNmea2000()       { return IsobusGuidanceChannel::OnXteNmea2000; }
    static Callback LegacyPosition()    { return IsobusGuidanceChannel::OnLegacyPosition; }
    static Callback LegacySpeed()       { return IsobusGuidanceChannel::OnLegacySpeed; }
    static Callback LegacyXteJohnDeere(){ return IsobusGuidanceChannel::OnLegacyXteJohnDeere; }
    static Callback LegacyXteTrimble()  { return IsobusGuidanceChannel::OnLegacyXteTrimble; }
    static Callback AllImplementStop()  { return IsobusGuidanceChannel::OnAllImplementStop; }
};
}  // namespace triton

namespace {

// A CAN identifier carrying the source address in its low byte, which is all
// these callbacks read out of it. The PGN half does not matter: AgIsoStack
// dispatched on it already to pick the callback.
std::uint32_t IdFrom(std::uint8_t sourceAddress) {
    return 0x0CFF0000u | sourceAddress;
}

GuidanceSource   gcGuidance;
ImplementPlough* gcImplement = nullptr;
IsobusGuidanceChannel* gcChannel = nullptr;

IsobusGuidanceChannel& Channel() {
    if (gcChannel == nullptr) {
        EEPROM.eepromReset();
        gcImplement = new ImplementPlough(nullptr, &gcGuidance);
        // Begin() is not called: it claims an address and registers callbacks
        // with the network manager, neither of which these tests need, and
        // the claim would cost real wall-clock time.
        gcChannel = new IsobusGuidanceChannel(nullptr, nullptr, &gcGuidance, gcImplement);
    }
    return *gcChannel;
}

void Reset() {
    millisValue(0);
    gcGuidance = GuidanceSource();
    Channel().ResetMessageCounters();
}

void Deliver(IsobusGuidanceChannelTestAccess::Callback callback,
             std::uint8_t sourceAddress, const std::vector<std::uint8_t>& data) {
    IsobusGuidanceChannelTestAccess::Deliver(callback, Channel(), IdFrom(sourceAddress),
                                             data.data(), static_cast<std::uint8_t>(data.size()));
}

}  // namespace

// ---------------------------------------------------------------------------
// Standard NMEA2000
// ---------------------------------------------------------------------------

test(IsobusGuidanceChannel, nmea2000Position_commitsFixAndCoordinates) {
    Reset();
    millisValue(1234);
    // 52.0 N, 5.0 E as two int32 LE in 1e-7 degree units.
    Deliver(IsobusGuidanceChannelTestAccess::PositionNmea2000(), 0x1C,
            { 0x00, 0x92, 0xFE, 0x1E, 0x80, 0xF0, 0xFA, 0x02 });
    assertEqual(gcGuidance.GetGgaTimestamp(), (unsigned long)1234);
    assertNear(gcGuidance.GetLatitude(), 52.0f, 1e-4f);
    assertNear(gcGuidance.GetLongitude(), 5.0f, 1e-4f);
    assertEqual(Channel().GetMessageCounters().positionNmea2000, (uint32_t)1);
    millisValue(0);
}

// The fix-age timer drives the plough's HOLD interlock, so an implausible
// coordinate must never cost us a fix: the timestamp still moves, the
// position does not.
test(IsobusGuidanceChannel, nmea2000Position_implausibleCoordinatesStillCountAsAFix) {
    Reset();
    gcGuidance.SetPosition(52.0f, 5.0f);
    millisValue(2000);
    // Latitude far outside +-90 degrees, longitude valid.
    Deliver(IsobusGuidanceChannelTestAccess::PositionNmea2000(), 0x1C,
            { 0x00, 0xE4, 0x0B, 0x54, 0x80, 0xF0, 0xFA, 0x02 });
    assertEqual(gcGuidance.GetGgaTimestamp(), (unsigned long)2000);
    assertNear(gcGuidance.GetLatitude(), 52.0f, 1e-4f);   // untouched
    millisValue(0);
}

test(IsobusGuidanceChannel, nmea2000Position_notAvailableIsNoFixAtAll) {
    Reset();
    millisValue(3000);
    Deliver(IsobusGuidanceChannelTestAccess::PositionNmea2000(), 0x1C,
            { 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F });
    assertEqual(gcGuidance.GetGgaTimestamp(), (unsigned long)0);
    assertEqual(Channel().GetMessageCounters().positionNmea2000, (uint32_t)1);   // still counted
    millisValue(0);
}

// COG travels in the same frame as SOG. Each field commits on its own flag,
// so an unavailable speed must not cost us a good course (#37).
test(IsobusGuidanceChannel, nmea2000Speed_commitsCourseEvenWhenSpeedIsUnavailable) {
    Reset();
    // COG 15708 x 0.0001 rad = 90 degrees; SOG 0xFFFF = not available.
    Deliver(IsobusGuidanceChannelTestAccess::SpeedNmea2000(), 0x1C,
            { 0x00, 0x00, 0x5C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF });
    assertNear(gcGuidance.GetCourse(), 90.0f, 0.1f);
    assertEqual(gcGuidance.GetVtgTimestamp(), (unsigned long)0);   // speed never committed
    assertEqual(Channel().GetMessageCounters().speedNmea2000, (uint32_t)1);
}

test(IsobusGuidanceChannel, nmea2000Speed_commitsBothWhenBothArePresent) {
    Reset();
    millisValue(500);
    // COG 90 degrees, SOG 103 x 0.01 m/s = about 2 knots.
    Deliver(IsobusGuidanceChannelTestAccess::SpeedNmea2000(), 0x1C,
            { 0x00, 0x00, 0x5C, 0x3D, 0x67, 0x00, 0xFF, 0xFF });
    assertNear(gcGuidance.GetCourse(), 90.0f, 0.1f);
    assertNear(gcGuidance.GetSpeed(), 2.0f, 0.05f);
    assertEqual(gcGuidance.GetVtgTimestamp(), (unsigned long)500);
    millisValue(0);
}

test(IsobusGuidanceChannel, nmea2000Xte_commitsHundredthsOfAMetre) {
    Reset();
    // int32 LE, 0.01 m units: 123 -> 1.23 m.
    Deliver(IsobusGuidanceChannelTestAccess::XteNmea2000(), 0x1C,
            { 0x00, 0x00, 0x7B, 0x00, 0x00, 0x00, 0xFF, 0xFF });
    assertEqual(gcGuidance.GetXte(), 123);
    assertEqual(Channel().GetMessageCounters().xteNmea2000, (uint32_t)1);
}

// Bit 6 of byte 1 is Navigation Terminated: the sender has stopped navigating
// this leg, so whatever number rides along is stale.
test(IsobusGuidanceChannel, nmea2000Xte_navigationTerminatedCommitsNothing) {
    Reset();
    gcGuidance.SetXte(999);
    Deliver(IsobusGuidanceChannelTestAccess::XteNmea2000(), 0x1C,
            { 0x00, 0x40, 0x7B, 0x00, 0x00, 0x00, 0xFF, 0xFF });
    assertEqual(gcGuidance.GetXte(), 999);
    assertEqual(Channel().GetMessageCounters().xteNmea2000, (uint32_t)1);
}

// ---------------------------------------------------------------------------
// Legacy proprietary
// ---------------------------------------------------------------------------

test(IsobusGuidanceChannel, legacySpeed_skipsNotAvailableFieldsAndKeepsTheRest) {
    Reset();
    // Course 0x2D00/128 = 90 degrees, speed 0xFFFF not available, altitude
    // 0x4F80/8 - 2500 = 44 m.
    Deliver(IsobusGuidanceChannelTestAccess::LegacySpeed(), 0x1C,
            { 0x00, 0x2D, 0xFF, 0xFF, 0x00, 0x00, 0x80, 0x4F });
    assertNear(gcGuidance.GetCourse(), 90.0f, 0.1f);
    assertEqual(gcGuidance.GetVtgTimestamp(), (unsigned long)0);
    assertNear(gcGuidance.GetAltitude(), 44.0f, 0.1f);
    assertEqual(Channel().GetMessageCounters().speedLegacy, (uint32_t)1);
    assertEqual((int)Channel().GetMessageCounters().lastSpeedLegacySourceAddress, 0x1C);
}

test(IsobusGuidanceChannel, legacySpeed_shortFrameIsCountedButNotDecoded) {
    Reset();
    Deliver(IsobusGuidanceChannelTestAccess::LegacySpeed(), 0x1C, { 0x00, 0x2D, 0x00, 0x02 });
    assertEqual(gcGuidance.GetVtgTimestamp(), (unsigned long)0);
    assertNear(gcGuidance.GetCourse(), 0.0f, 1e-6f);
    assertEqual(Channel().GetMessageCounters().speedLegacy, (uint32_t)1);
}

// PGN 0xFFFF is shared by several manufacturers and several messages. Only
// source address 0x2A carrying selector 0x77 is cross-track error; issues #20
// and #30 are both this filter. A frame from Ag Leader/Raven (0x80) must not
// commit anything, but its raw bytes must still reach the debug readout,
// because deriving that vendor's real layout needs them.
test(IsobusGuidanceChannel, johnDeereXte_commitsOnlyFromSourceAddress0x2A) {
    Reset();
    const std::vector<std::uint8_t> frame = { 0x77, 0x15, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    Deliver(IsobusGuidanceChannelTestAccess::LegacyXteJohnDeere(), 0x80, frame);
    assertEqual(gcGuidance.GetXte(), 0);                                   // nothing committed
    assertEqual(Channel().GetMessageCounters().xteJohnDeereLegacy, (uint32_t)1);
    assertEqual((int)Channel().GetMessageCounters().lastXteJohnDeereLegacySourceAddress, 0x80);
    assertEqual((int)Channel().GetMessageCounters().lastXteJohnDeereLegacyPayload[0], 0x77);  // diagnostics kept

    Deliver(IsobusGuidanceChannelTestAccess::LegacyXteJohnDeere(), 0x2A, frame);
    assertEqual((int)Channel().GetMessageCounters().lastXteJohnDeereLegacySourceAddress, 0x2A);
    assertEqual(Channel().GetMessageCounters().xteJohnDeereLegacy, (uint32_t)2);
}

// Source address 0x2A also sends other messages under this PGN, 0x92 at 1 Hz
// among them. Running those through the XTE decode is what produced a
// +162.87 m error on a rig (#30).
test(IsobusGuidanceChannel, johnDeereXte_otherSelectorsFromTheSameSenderAreIgnored) {
    Reset();
    gcGuidance.SetXte(42, 4);
    Deliver(IsobusGuidanceChannelTestAccess::LegacyXteJohnDeere(), 0x2A,
            { 0x92, 0x15, 0x00, 0x00, 0xFF, 0x7F, 0x00, 0x00 });
    assertEqual(gcGuidance.GetXte(), 42);   // untouched
    assertEqual(Channel().GetMessageCounters().xteJohnDeereLegacy, (uint32_t)1);
    assertEqual((int)Channel().GetMessageCounters().lastXteJohnDeereLegacyPayload[0], 0x92);
}

test(IsobusGuidanceChannel, johnDeereXte_shortFrameIsCountedAndNothingElse) {
    Reset();
    Deliver(IsobusGuidanceChannelTestAccess::LegacyXteJohnDeere(), 0x2A, { 0x77, 0x15, 0x00 });
    assertEqual(Channel().GetMessageCounters().xteJohnDeereLegacy, (uint32_t)1);
    assertEqual(Channel().GetMessageCounters().lastXteJohnDeereLegacyPayloadMs, (uint32_t)0);
}

test(IsobusGuidanceChannel, trimbleXte_recordsItsSourceAddress) {
    Reset();
    Deliver(IsobusGuidanceChannelTestAccess::LegacyXteTrimble(), 0xAA,
            { 0x02, 0x00, 0x00, 0x80, 0x3F, 0x07, 0x00, 0x00 });
    assertEqual(Channel().GetMessageCounters().xteTrimbleLegacy, (uint32_t)1);
    assertEqual((int)Channel().GetMessageCounters().lastXteTrimbleLegacySourceAddress, 0xAA);
}

// ---------------------------------------------------------------------------
// All Implement Stop -- the safety path
// ---------------------------------------------------------------------------

// The state is two bits in byte 7, not a byte of its own: 00 stop, 01 permit,
// 10 error, 11 not available. The rest of the frame is padding.
namespace {
std::vector<std::uint8_t> AisoFrame(std::uint8_t state) {
    return { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, static_cast<std::uint8_t>(0xFC | (state & 0x03)) };
}
}  // namespace

test(IsobusGuidanceChannel, allImplementStop_stopRequestedStopsThePlough) {
    Reset();
    // Drive the plough first, so Stop() has something to undo.
    gcImplement->Wider(200);
    assertMore(analogWriteValue(OUTPUT_WIDE_2), 0);

    millisValue(4000);
    Deliver(IsobusGuidanceChannelTestAccess::AllImplementStop(), 0xF0, AisoFrame(0));
    assertEqual(analogWriteValue(OUTPUT_WIDE_2), 0);      // the safety path ran
    assertEqual(analogWriteValue(OUTPUT_NARROW_2), 0);
    assertEqual(Channel().GetMessageCounters().allImplementStop, (uint32_t)1);
    assertEqual(Channel().GetMessageCounters().lastAllImplementStopMs, (unsigned long)4000);
    assertEqual((int)Channel().GetMessageCounters().lastAllImplementStopState, 0);
    millisValue(0);
}

// AISO is a roughly 1 Hz broadcast and most frames carry Permit. Treating any
// state but 00 as a stop would halt the plough on routine traffic.
test(IsobusGuidanceChannel, allImplementStop_permittedStateIsRecordedButStopsNothing) {
    Reset();
    gcImplement->Wider(200);
    millisValue(5000);
    Deliver(IsobusGuidanceChannelTestAccess::AllImplementStop(), 0xF0, AisoFrame(1));
    assertMore(analogWriteValue(OUTPUT_WIDE_2), 0);       // still driving
    assertEqual(Channel().GetMessageCounters().allImplementStop, (uint32_t)1);
    assertEqual((int)Channel().GetMessageCounters().lastAllImplementStopState, 1);
    assertEqual(Channel().GetMessageCounters().lastAllImplementStopMs, (unsigned long)0);
    gcImplement->Stop();
    millisValue(0);
}

test(IsobusGuidanceChannel, allImplementStop_notAvailableStopsNothingEither) {
    Reset();
    gcImplement->Wider(200);
    millisValue(6000);
    Deliver(IsobusGuidanceChannelTestAccess::AllImplementStop(), 0xF0, AisoFrame(3));
    assertMore(analogWriteValue(OUTPUT_WIDE_2), 0);
    assertEqual((int)Channel().GetMessageCounters().lastAllImplementStopState, 3);
    assertEqual(Channel().GetMessageCounters().lastAllImplementStopMs, (unsigned long)0);
    gcImplement->Stop();
    millisValue(0);
}

test(IsobusGuidanceChannel, allImplementStop_shortFrameRecordsNoState) {
    Reset();
    Deliver(IsobusGuidanceChannelTestAccess::AllImplementStop(), 0xF0, {});
    assertEqual(Channel().GetMessageCounters().allImplementStop, (uint32_t)1);
    assertEqual((int)Channel().GetMessageCounters().lastAllImplementStopState, 0xFF);
}

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

test(IsobusGuidanceChannel, counters_totalAddsUpAndResets) {
    Reset();
    Deliver(IsobusGuidanceChannelTestAccess::XteNmea2000(), 0x1C,
            { 0x00, 0x00, 0x7B, 0x00, 0x00, 0x00, 0xFF, 0xFF });
    Deliver(IsobusGuidanceChannelTestAccess::SpeedNmea2000(), 0x1C,
            { 0x00, 0x00, 0x5C, 0x3D, 0x67, 0x00, 0xFF, 0xFF });
    Deliver(IsobusGuidanceChannelTestAccess::AllImplementStop(), 0xF0, AisoFrame(1));
    assertEqual(Channel().GetMessageCounters().Total(), (uint32_t)3);

    Channel().ResetMessageCounters();
    assertEqual(Channel().GetMessageCounters().Total(), (uint32_t)0);
    assertEqual((int)Channel().GetMessageCounters().lastAllImplementStopState, 0xFF);
}

#endif  // ISOBUS
