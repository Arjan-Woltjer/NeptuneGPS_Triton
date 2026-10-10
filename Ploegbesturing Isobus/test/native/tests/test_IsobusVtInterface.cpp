/*
  test_IsobusVtInterface - Tests for the Virtual Terminal client half of the
  plough controller against the real AgIsoStack running off-target: the raw
  VT Status Message counter, the soft-key handler, the connect-state reporting
  and the post-connection reconnect watchdog (NeptuneGPS_Triton#98).
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
// macros, and GCC's <string>/<vector> use std::min/max with three arguments.
#include <cstring>
#include <string>
#include <vector>

#include <AUnit.h>

// AUnit.h has pulled in the Arduino stub by now, so its min()/max() macros
// are live. Same sandwich as test_IsobusTcInterface.cpp's.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <can_network_manager.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "isobus/IsobusVtInterface.hpp"
#include "isobus/VTObjectPool.hpp"

using namespace aunit;
using namespace triton;

namespace triton
{
// The hook IsobusVtInterface declares a friend. Everything reached below is
// private because only AgIsoStack itself is meant to call it: the global PGN
// callback, the key-event listener the two event dispatchers hold, and the
// watchdog Update() drives. A real VT server is the one thing a host test
// cannot stand up, so the tests call them directly instead.
struct IsobusVtInterfaceTestAccess {
    static void DeliverVtToEcu(IsobusVtInterface& vt, const std::uint8_t* data, std::uint8_t length) {
        const isobus::CANMessage message(isobus::CANMessage::Type::Receive,
                                         isobus::CANIdentifier(0x1CE6FF1Cu),
                                         data, length, nullptr, nullptr, 0);
        IsobusVtInterface::OnVtToEcuMessage(message, &vt);
    }

    // The same frame with no parent: this callback is registered with the
    // network manager, which is free to outlive us, so it has to survive it.
    static void DeliverVtToEcuWithoutParent(const std::uint8_t* data, std::uint8_t length) {
        const isobus::CANMessage message(isobus::CANMessage::Type::Receive,
                                         isobus::CANIdentifier(0x1CE6FF1Cu),
                                         data, length, nullptr, nullptr, 0);
        IsobusVtInterface::OnVtToEcuMessage(message, nullptr);
    }

    static void Key(IsobusVtInterface& vt, std::uint16_t objectId,
                    isobus::VirtualTerminalClient::KeyActivationCode code) {
        isobus::VirtualTerminalClient::VTKeyEvent event{};
        event.parentPointer  = nullptr;
        event.objectID       = objectId;
        event.parentObjectID = 0;
        event.keyNumber      = 0;
        event.keyEvent       = code;
        vt.onVtKeyEvent(event);
    }

    static void Watchdog(IsobusVtInterface& vt) { vt.updateReconnectWatchdog(); }

    // The watchdog is deliberately inert until a first successful connection
    // (see the header's reconnect fields), and get_is_connected() is driven
    // by a private state machine no host test can push to Connected. So the
    // "we have been connected once" history is set here instead.
    static void SetHasEverConnected(IsobusVtInterface& vt, bool value) { vt.hasEverConnected = value; }

    static void ResetWatchdog(IsobusVtInterface& vt) {
        vt.hasEverConnected    = false;
        vt.disconnectedSinceMs = 0;
        vt.lastReconnectTryMs  = 0;
        vt.reconnectAttempts   = 0;
    }

    static unsigned long DisconnectedSinceMs(const IsobusVtInterface& vt) { return vt.disconnectedSinceMs; }

    static void ResetVtStatusCounters(IsobusVtInterface& vt) {
        vt.vtStatusMessageCount  = 0;
        vt.lastVtStatusMessageMs = 0;
    }

    static bool ClientIsInitialized(const IsobusVtInterface& vt) {
        return vt.vtClient && vt.vtClient->get_is_initialized();
    }

    // A VT Status Message from a given sender: the failover policy keys on
    // the identifier's source address.
    static void DeliverVtStatusFrom(IsobusVtInterface& vt, std::uint8_t source) {
        const std::uint8_t data[8] = {
            static_cast<std::uint8_t>(isobus::VirtualTerminalClient::Function::VTStatusMessage),
            0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
        const isobus::CANMessage message(isobus::CANMessage::Type::Receive,
                                         isobus::CANIdentifier(0x1CE6FF00u | source),
                                         data, 8, nullptr, nullptr, 0);
        IsobusVtInterface::OnVtToEcuMessage(message, &vt);
    }

    static void UpdateFailover(IsobusVtInterface& vt) { vt.updateFailover(); }

    static void ResetFailover(IsobusVtInterface& vt) {
        vt.failover = VtFailoverPolicy();
        vt.failover.Start(millis());
    }

    static void ResetPendingKeys(IsobusVtInterface& vt) {
        vt.pendingWiderPress     = false;
        vt.pendingNarrowerPress  = false;
        vt.pendingCalibratePress = false;
    }
};
}  // namespace triton

namespace {

// A debug Stream that records what was written to it. The watchdog prints on
// every fire and dereferences serialDebug unconditionally there, so unlike
// the other ISOBUS fixtures this one cannot pass nullptr.
class VtCaptureStream : public Stream {
public:
    size_t write(uint8_t c) override {
        if (len < sizeof(text) - 1) text[len++] = (char)c;
        text[len] = '\0';
        return 1;
    }
    void Clear() { len = 0; text[0] = '\0'; }
    bool Contains(const char* needle) const { return strstr(text, needle) != nullptr; }

    char   text[1024] = {};
    size_t len = 0;
};

// ---------------------------------------------------------------------------
// One shared fixture for the whole file, for the same reason
// test_IsobusTcInterface.cpp has one: AgIsoStack's network manager is a
// process-wide singleton, so control functions cannot be created and thrown
// away per test.
//
// Nothing here calls Update(): that would pump vtClient's state machine
// against std::chrono wall-clock time. Every test below drives one entry
// point directly, so this file costs no real time.
//
// The CAN hardware interface is deliberately left alone -- this fixture never
// puts a frame on the bus, and starting it here would collide with
// test_IsobusTcInterface.cpp's own start() depending on which file's fixture
// is built first.
// ---------------------------------------------------------------------------
GuidanceSource     vtGuidance;
VtCaptureStream    vtDebug;
ImplementPlough*   vtImplement = nullptr;
IsobusVtInterface* vtInterface = nullptr;

IsobusVtInterface& Fixture() {
    if (vtInterface == nullptr) {
        EEPROM.eepromReset();

        // A NAME distinct from test_IsobusTcInterface.cpp's, at a different
        // preferred address: both fixtures can exist at once in this binary,
        // and two internal control functions claiming one address on one
        // channel is not a situation worth reproducing here.
        isobus::NAME ourName(0);
        ourName.set_arbitrary_address_capable(true);
        ourName.set_industry_group(2);
        ourName.set_device_class(6);
        ourName.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::RateControl));
        ourName.set_identity_number(3);
        ourName.set_manufacturer_code(1407);
        auto controlFunction =
            isobus::CANNetworkManager::CANNetwork.create_internal_control_function(ourName, 0, 0x1D);

        vtImplement = new ImplementPlough(nullptr, &vtGuidance);
        vtInterface = new IsobusVtInterface(&vtDebug, vtImplement, &vtGuidance, controlFunction);
        vtInterface->Begin();

        // Begin() installs the class's own log sink at Info, which on a real
        // controller goes to the serial console. Here it goes to stdout and
        // buries AUnit's own result lines, so turn it back down.
        isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Critical);
    }
    return *vtInterface;
}

void Reset() {
    millisValue(0);
    IsobusVtInterfaceTestAccess::ResetWatchdog(Fixture());
    IsobusVtInterfaceTestAccess::ResetVtStatusCounters(Fixture());
    IsobusVtInterfaceTestAccess::ResetPendingKeys(Fixture());
    vtDebug.Clear();
}

// A VT-to-ECU frame whose function byte is `function`; the other seven bytes
// are roughly what a real VT Status Message carries and none of them are read.
std::vector<std::uint8_t> VtToEcuFrame(std::uint8_t function) {
    return { function, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF };
}

const std::uint8_t kVtStatusFunction =
    static_cast<std::uint8_t>(isobus::VirtualTerminalClient::Function::VTStatusMessage);

void Deliver(const std::vector<std::uint8_t>& data) {
    IsobusVtInterfaceTestAccess::DeliverVtToEcu(Fixture(), data.data(),
                                                static_cast<std::uint8_t>(data.size()));
}

void Release(std::uint16_t objectId) {
    IsobusVtInterfaceTestAccess::Key(
        Fixture(), objectId,
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased);
}

}  // namespace

// ---------------------------------------------------------------------------
// Raw VT Status Message counter
//
// A second, independent global listener on the VT-to-ECU PGN alongside
// AgIsoStack's own -- it exists to tell "the VT is not broadcasting" apart
// from "it is, and we are not acting on it in time" (GitHub issue #17). Its
// whole value is counting what is on the wire and nothing else, so what it
// must NOT count is as much of the contract as what it must.
// ---------------------------------------------------------------------------

test(IsobusVtInterface, vtStatus_countsVtStatusMessages) {
    Reset();
    millisValue(4321);
    Deliver(VtToEcuFrame(kVtStatusFunction));
    assertEqual(Fixture().GetVtStatusMessageCount(), (unsigned int)1);
    assertEqual(Fixture().GetVtStatusMessageAgeMs(), (unsigned long)0);

    millisValue(5321);
    assertEqual(Fixture().GetVtStatusMessageAgeMs(), (unsigned long)1000);

    Deliver(VtToEcuFrame(kVtStatusFunction));
    assertEqual(Fixture().GetVtStatusMessageCount(), (unsigned int)2);
    assertEqual(Fixture().GetVtStatusMessageAgeMs(), (unsigned long)0);
    millisValue(0);
}

test(IsobusVtInterface, vtStatus_ignoresOtherFunctionsOnTheSamePgn) {
    Reset();
    // Every VT-to-ECU message shares one PGN and is distinguished only by its
    // function byte, so a listener that did not check byte 0 would count
    // soft-key activations and object-pool responses as status broadcasts --
    // and report a healthy VT that is not broadcasting at all.
    Deliver(VtToEcuFrame(static_cast<std::uint8_t>(
        isobus::VirtualTerminalClient::Function::SoftKeyActivationMessage)));
    Deliver(VtToEcuFrame(static_cast<std::uint8_t>(
        isobus::VirtualTerminalClient::Function::ButtonActivationMessage)));
    Deliver(VtToEcuFrame(static_cast<std::uint8_t>(
        isobus::VirtualTerminalClient::Function::EndOfObjectPoolMessage)));
    assertEqual(Fixture().GetVtStatusMessageCount(), (unsigned int)0);
}

test(IsobusVtInterface, vtStatus_ignoresAnEmptyFrame) {
    Reset();
    const std::uint8_t nothing = 0;
    IsobusVtInterfaceTestAccess::DeliverVtToEcu(Fixture(), &nothing, 0);
    assertEqual(Fixture().GetVtStatusMessageCount(), (unsigned int)0);
}

test(IsobusVtInterface, vtStatus_survivesANullParentPointer) {
    Reset();
    const auto frame = VtToEcuFrame(kVtStatusFunction);
    IsobusVtInterfaceTestAccess::DeliverVtToEcuWithoutParent(
        frame.data(), static_cast<std::uint8_t>(frame.size()));
    assertEqual(Fixture().GetVtStatusMessageCount(), (unsigned int)0);
}

test(IsobusVtInterface, vtStatus_ageIsZeroBeforeTheFirstMessage) {
    Reset();
    millisValue(90000);
    // Not 90000: with no message yet there is no age, and reporting uptime
    // instead would read on the debug menu as a very stale VT.
    assertEqual(Fixture().GetVtStatusMessageAgeMs(), (unsigned long)0);
    millisValue(0);
}

// ---------------------------------------------------------------------------
// Soft-key handling
// ---------------------------------------------------------------------------

test(IsobusVtInterface, key_widerReleaseSetsAConsumeOncePress) {
    Reset();
    Release(Key_Wider);
    assertTrue(Fixture().ConsumeWiderPress());
    // Consume-once: main.cpp's loop() calls this every iteration, and a flag
    // that stayed set would adjust the plough on every pass instead of once
    // per tap.
    assertFalse(Fixture().ConsumeWiderPress());
    assertTrue(vtDebug.Contains("VT: Wider pressed"));
}

test(IsobusVtInterface, key_narrowerAndCalibrateAreIndependent) {
    Reset();
    Release(Key_Narrower);
    assertFalse(Fixture().ConsumeWiderPress());
    assertFalse(Fixture().ConsumeCalibratePress());
    assertTrue(Fixture().ConsumeNarrowerPress());

    Release(Key_Calibrate);
    assertFalse(Fixture().ConsumeWiderPress());
    assertFalse(Fixture().ConsumeNarrowerPress());
    assertTrue(Fixture().ConsumeCalibratePress());
}

test(IsobusVtInterface, key_actsOnReleaseOnly) {
    Reset();
    // A VT sends press, then ButtonStillHeld cyclically while the finger is
    // down, then release. Acting on anything but release would turn one tap
    // into a stream of adjustments.
    IsobusVtInterfaceTestAccess::Key(
        Fixture(), Key_Wider,
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonPressedOrLatched);
    IsobusVtInterfaceTestAccess::Key(
        Fixture(), Key_Wider,
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonStillHeld);
    IsobusVtInterfaceTestAccess::Key(
        Fixture(), Key_Wider,
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonStillHeld);
    assertFalse(Fixture().ConsumeWiderPress());

    Release(Key_Wider);
    assertTrue(Fixture().ConsumeWiderPress());
}

test(IsobusVtInterface, key_anAbortedPressIsNotAPress) {
    Reset();
    // The operator put a finger on the key and slid off it. The VT reports
    // ButtonPressAborted instead of a release, and nothing must move -- which
    // matters most for Calibrate, a single tap that enters a blocking wizard.
    IsobusVtInterfaceTestAccess::Key(
        Fixture(), Key_Calibrate,
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonPressedOrLatched);
    IsobusVtInterfaceTestAccess::Key(
        Fixture(), Key_Calibrate,
        isobus::VirtualTerminalClient::KeyActivationCode::ButtonPressAborted);
    assertFalse(Fixture().ConsumeCalibratePress());
}

test(IsobusVtInterface, key_autoIsLoggedButNotWired) {
    Reset();
    // InterfacePlough's AUTO mode is derived from GPS and hitch state, not
    // user-settable, so this key has no control target by design -- see the
    // class header.
    Release(Key_Auto);
    assertFalse(Fixture().ConsumeWiderPress());
    assertFalse(Fixture().ConsumeNarrowerPress());
    assertFalse(Fixture().ConsumeCalibratePress());
    assertTrue(vtDebug.Contains("VT: Auto pressed"));
}

test(IsobusVtInterface, key_unknownObjectIdIsIgnored) {
    Reset();
    // Other objects in the pool raise button events too -- the data mask, the
    // numeric fields. None of them mean anything here.
    Release(Plough_DataMask);
    Release(Out_Position);
    Release(0xFFFF);
    assertFalse(Fixture().ConsumeWiderPress());
    assertFalse(Fixture().ConsumeNarrowerPress());
    assertFalse(Fixture().ConsumeCalibratePress());
}

test(IsobusVtInterface, key_rearmsAfterBeingConsumed) {
    Reset();
    Release(Key_Wider);
    assertTrue(Fixture().ConsumeWiderPress());
    Release(Key_Wider);
    assertTrue(Fixture().ConsumeWiderPress());
    assertFalse(Fixture().ConsumeWiderPress());
}

test(IsobusVtInterface, key_allThreeCanBePendingAtOnce) {
    Reset();
    Release(Key_Wider);
    Release(Key_Narrower);
    Release(Key_Calibrate);
    assertTrue(Fixture().ConsumeWiderPress());
    assertTrue(Fixture().ConsumeNarrowerPress());
    assertTrue(Fixture().ConsumeCalibratePress());
}

// ---------------------------------------------------------------------------
// Connect-state reporting (IsobusDebugMenu's "step N of M" line)
// ---------------------------------------------------------------------------

test(IsobusVtInterface, state_reportsDisconnectedBeforeAnyHandshake) {
    Reset();
    assertEqual(Fixture().GetStateStep(), 0);
    assertEqual(Fixture().GetStateName(), "Disconnected");
    assertFalse(Fixture().IsConnected());
}

test(IsobusVtInterface, state_totalStepsExcludesFailed) {
    Reset();
    // The name table has 23 entries, but Failed is a terminal state rather
    // than a forward step, so "of N" must read 22.
    assertEqual(Fixture().GetStateTotalSteps(), 22);
}

test(IsobusVtInterface, state_vtVersionIsUnknownBeforeConnecting) {
    Reset();
    // get_connected_vt_version() only means anything past
    // WaitForPartnerVTStatusMessage; before that the stack's raw byte is 0,
    // which is neither a version nor the 0xFF "2 or older" sentinel.
    assertEqual(Fixture().GetVtVersionName(), "(unknown)");
}

// ---------------------------------------------------------------------------
// Reconnect watchdog (GitHub issue #18)
//
// The connected branch is not reachable from a host test -- get_is_connected()
// reads a private state machine only a real VT server on a real bus can
// advance -- so what is pinned here is the whole of the disconnected policy,
// including the arming rule that is the reason this watchdog is safe to run
// at all.
// ---------------------------------------------------------------------------

test(IsobusVtInterface, watchdog_neverFiresBeforeAFirstConnection) {
    Reset();
    // The initial handshake has its own timing and takes as long as it takes.
    // A watchdog firing during it would tear down a connection that was
    // progressing normally.
    for (unsigned long t = 0; t <= 120000UL; t += 5000UL) {
        millisValue(t);
        IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    }
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)0);
    assertEqual(IsobusVtInterfaceTestAccess::DisconnectedSinceMs(Fixture()), (unsigned long)0);
    millisValue(0);
}

test(IsobusVtInterface, watchdog_firstTickAfterAConnectionOnlyStartsTheClock) {
    Reset();
    IsobusVtInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    millisValue(1000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(IsobusVtInterfaceTestAccess::DisconnectedSinceMs(Fixture()), (unsigned long)1000);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)0);
    millisValue(0);
}

test(IsobusVtInterface, watchdog_waitsTenSecondsBeforeTheFirstAttempt) {
    Reset();
    IsobusVtInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    millisValue(1000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());

    // 9999 ms into the outage: comfortably past AgIsoStack's own 3 s
    // VT_STATUS_TIMEOUT_MS, but a connection re-handshaking on its own must
    // not be interrupted.
    millisValue(10999);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)0);

    millisValue(11000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)1);
    assertTrue(vtDebug.Contains("forcing reconnect attempt 1"));
    assertTrue(vtDebug.Contains("VT: disconnected 10s"));
    // terminate() then initialize(): the client has to come back usable, not
    // be left torn down. See the header for why initialize() on its own is a
    // no-op on a live client.
    assertTrue(IsobusVtInterfaceTestAccess::ClientIsInitialized(Fixture()));
    millisValue(0);
}

test(IsobusVtInterface, watchdog_spacesRepeatAttemptsTenSecondsApart) {
    Reset();
    IsobusVtInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    millisValue(1000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    millisValue(11000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)1);

    // A partner that is genuinely gone stays gone. Without the retry spacing
    // this would tear the client down and rebuild it on every loop pass.
    millisValue(20999);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)1);

    millisValue(21000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)2);

    millisValue(31000);
    IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)3);
    millisValue(0);
}

test(IsobusVtInterface, watchdog_attemptsAreOnePerTenSecondsNotOnePerLoop) {
    Reset();
    IsobusVtInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    // A minute of outage pumped at a realistic loop rate. The attempt count
    // is the number a rig operator reads off the debug menu, so it has to
    // track wall time rather than iteration count.
    for (unsigned long t = 1000; t <= 61000UL; t += 20UL) {
        millisValue(t);
        IsobusVtInterfaceTestAccess::Watchdog(Fixture());
    }
    // The outage starts at 1000; attempts land at 11000, 21000 ... 61000.
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)6);
    millisValue(0);
}

// ---------------------------------------------------------------------------
// VT failover (VtFailoverPolicy.hpp has the session 13 field case). The
// policy's own decisions are covered in test_VtFailoverPolicy.cpp; these
// check that the interface acts on them against the real network manager.
// AUnit runs tests in name order, so "requests" runs before "switches",
// while the fixture's partner is still unbound.
// ---------------------------------------------------------------------------

namespace {

// An address claim, straight into the network manager's receive path, then
// processed: the way a real VT joins the address table.
void ClaimAddress(std::uint8_t address, std::uint64_t name) {
    isobus::CANMessageFrame frame = {};
    frame.identifier      = 0x18EEFF00u | address;
    frame.isExtendedFrame = true;
    frame.dataLength      = 8;
    for (int i = 0; i < 8; i++) frame.data[i] = static_cast<std::uint8_t>(name >> (8 * i));
    isobus::CANNetworkManager::CANNetwork.process_receive_can_message_frame(frame);
    isobus::CANNetworkManager::CANNetwork.update();
}

// The two VT NAMEs from the session 13 capture, as their 64-bit values (the
// wire bytes are little-endian: 0131C20B081D00A0 is the CNH VT).
constexpr std::uint64_t kCnhVtName       = 0xA0001D080BC23101ULL;
constexpr std::uint64_t kInCommandVtName = 0x80001D000C206EB1ULL;

}  // namespace

test(IsobusVtInterface, failover_requestsClaimsWhenNoOtherVtIsHeard) {
    Reset();
    IsobusVtInterfaceTestAccess::ResetFailover(Fixture());
    // No VT on the bus at all: after the grace period the interface asks
    // every CF to claim again, so a VT the prune evicted can be seen.
    millisValue(VtFailoverPolicy::kPartnerSilentMs - 1);
    IsobusVtInterfaceTestAccess::UpdateFailover(Fixture());
    assertEqual(Fixture().GetVtClaimRequestCount(), (unsigned int)0);

    millisValue(VtFailoverPolicy::kPartnerSilentMs);
    IsobusVtInterfaceTestAccess::UpdateFailover(Fixture());
    assertEqual(Fixture().GetVtClaimRequestCount(), (unsigned int)1);
    assertTrue(vtDebug.Contains("requesting address claims"));
    assertEqual(Fixture().GetVtSwitchCount(), (unsigned int)0);
    millisValue(0);
}

test(IsobusVtInterface, failover_switchesToTheVtThatIsStillTalking) {
    Reset();
    // Session 13: bound to the CNH VT, the InCommand's VT also on the bus.
    ClaimAddress(0x26, kCnhVtName);
    ClaimAddress(0x80, kInCommandVtName);
    assertEqual(Fixture().GetPartnerAddress(), (std::uint8_t)0x26);
    const std::shared_ptr<isobus::PartneredControlFunction> before = Fixture().GetPartner();

    IsobusVtInterfaceTestAccess::ResetFailover(Fixture());
    millisValue(1000);
    IsobusVtInterfaceTestAccess::DeliverVtStatusFrom(Fixture(), 0x26);
    IsobusVtInterfaceTestAccess::DeliverVtStatusFrom(Fixture(), 0x80);

    // The CNH VT stops; the InCommand keeps broadcasting at 1 Hz.
    for (unsigned long t = 2000; t <= 1000 + VtFailoverPolicy::kPartnerSilentMs; t += 1000) {
        millisValue(t);
        IsobusVtInterfaceTestAccess::DeliverVtStatusFrom(Fixture(), 0x80);
        IsobusVtInterfaceTestAccess::UpdateFailover(Fixture());
    }

    assertEqual(Fixture().GetVtSwitchCount(), (unsigned int)1);
    assertTrue(vtDebug.Contains("switching to it"));
    assertTrue(Fixture().GetPartner() != before);
    // A brand-new client, initialised, on a partner that binds to the
    // InCommand's VT -- and only to it -- once the manager runs.
    assertTrue(IsobusVtInterfaceTestAccess::ClientIsInitialized(Fixture()));
    isobus::CANNetworkManager::CANNetwork.update();
    assertEqual(Fixture().GetPartnerAddress(), (std::uint8_t)0x80);
    assertTrue(Fixture().IsPartnerAddressValid());
    assertEqual(Fixture().GetPartner()->get_NAME().get_full_name(), kInCommandVtName);
    millisValue(0);
}

#endif  // ISOBUS
