/*
  test_IsobusTcInterface - Tests for the Task Controller client against the
  real AgIsoStack running on a fake CAN bus: the DDOP we upload, and the
  process-data tables the TC drives us through (NeptuneGPS_Triton#98).
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
#include <set>
#include <string>
#include <vector>

#include <AUnit.h>

// AUnit.h has pulled in the Arduino stub by now, so its min()/max() macros
// are live and would mangle the three-argument std::min/std::max these
// headers reach through the STL. Same sandwich as GuidanceGeometry.hpp's.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/hardware_integration/can_hardware_interface.hpp>
#include <isobus/isobus/can_network_manager.hpp>
#include <isobus/isobus/isobus_device_descriptor_object_pool_helpers.hpp>
#include <isobus/isobus/isobus_device_descriptor_object_pool.hpp>
#include <isobus/isobus/isobus_standard_data_description_indices.hpp>
#include <isobus/isobus/isobus_task_controller_client_objects.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "FakeCanPlugin.hpp"
#include "TestBus.hpp"
#include "isobus/IsobusTcInterface.hpp"
#include "planner/WidthPlanning.hpp"
#include "calibration/SettingsConsole.hpp"
#include "VehicleSensors.hpp"

using namespace aunit;
using namespace triton;

namespace triton
{
// The hook IsobusTcInterface declares a friend: it reaches the two static
// process-data callbacks, which AgIsoStack would otherwise be the only
// caller of.
struct IsobusTcInterfaceTestAccess {
    static bool Command(IsobusTcInterface& tc, std::uint16_t ddi, std::int32_t value) {
        return IsobusTcInterface::OnValueCommand(0, ddi, value, &tc);
    }
    static std::int32_t Request(IsobusTcInterface& tc, std::uint16_t ddi) {
        std::int32_t value = -12345;   // overwritten by the callback
        IsobusTcInterface::OnValueRequest(0, ddi, value, &tc);
        return value;
    }

    // The reconnect watchdog (GitHub issue #18) and the connection history it
    // arms off. get_is_connected() is driven by a private state machine only
    // a real TC server on a real bus can advance, so the "we have been
    // connected once" flag is set here rather than reached through it.
    static void Watchdog(IsobusTcInterface& tc) { tc.updateReconnectWatchdog(); }

    static void SetHasEverConnected(IsobusTcInterface& tc, bool value) { tc.hasEverConnected = value; }

    static void ResetWatchdog(IsobusTcInterface& tc) {
        tc.hasEverConnected    = false;
        tc.disconnectedSinceMs = 0;
        tc.lastReconnectTryMs  = 0;
        tc.reconnectAttempts   = 0;
    }

    static unsigned long DisconnectedSinceMs(const IsobusTcInterface& tc) { return tc.disconnectedSinceMs; }
};
}  // namespace triton

// ---------------------------------------------------------------------------
// One shared fixture for the whole file.
//
// AgIsoStack keeps its own time with std::chrono and its network manager is a
// process-wide singleton, so control functions cannot be created and thrown
// away per test the way an ImplementPlough can. Everything below shares one
// hardware interface, one internal control function and one IsobusTcInterface,
// built once on first use. Nothing here waits on the stack's timers, so no
// test in this file costs wall-clock time.
// ---------------------------------------------------------------------------

namespace {

// A debug Stream that records what was written to it. The reconnect watchdog
// dereferences serialDebug unconditionally when it fires, so this fixture
// cannot pass nullptr the way it used to.
class TcCaptureStream : public Stream {
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

GuidanceSource   tcGuidance;
TcCaptureStream  tcDebug;
FakeCanPlugin*   tcPlugin = nullptr;
ImplementPlough* tcImplement = nullptr;
IsobusTcInterface* tcInterface = nullptr;

IsobusTcInterface& Fixture() {
    if (tcInterface == nullptr) {
        EEPROM.eepromReset();
        // The binary's one fake bus: started here or by whichever test needed
        // it first (TestBus.hpp).
        tcPlugin = SharedTestBus();

        // The identity main.cpp claims, near enough: what matters here is that
        // a real InternalControlFunction exists, because buildDdop() reads its
        // NAME into the device object.
        isobus::NAME ourName(0);
        ourName.set_arbitrary_address_capable(true);
        ourName.set_industry_group(2);
        ourName.set_device_class(6);
        ourName.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::RateControl));
        ourName.set_identity_number(2);
        ourName.set_manufacturer_code(1407);
        auto controlFunction =
            isobus::CANNetworkManager::CANNetwork.create_internal_control_function(ourName, 0, 0x1C);

        tcImplement = new ImplementPlough(nullptr, &tcGuidance);
        tcInterface = new IsobusTcInterface(&tcDebug, tcImplement, &tcGuidance, controlFunction);
        tcInterface->Begin();
    }
    return *tcInterface;
}

// Every DDI carried by a process-data or property object in the pool we
// actually upload: generate the binary, hand it back to AgIsoStack's own
// parser, and read the objects out. Testing the bytes rather than the
// builder is the point -- the bytes are what a terminal rejects.
std::set<std::uint16_t> DdisInUploadedPool() {
    std::set<std::uint16_t> ddis;
    std::vector<std::uint8_t> binary;
    if (!Fixture().GenerateDdopBinary(binary) || binary.empty()) return ddis;

    isobus::DeviceDescriptorObjectPool parsed;
    if (!parsed.deserialize_binary_object_pool(binary, isobus::NAME(0))) return ddis;

    for (std::uint16_t index = 0; index < parsed.size(); index++) {
        auto object = parsed.get_object_by_index(index);
        if (object == nullptr) continue;
        if (object->get_object_type() == isobus::task_controller_object::ObjectTypes::DeviceProcessData) {
            ddis.insert(std::static_pointer_cast<isobus::task_controller_object::DeviceProcessDataObject>(object)->get_ddi());
        }
        else if (object->get_object_type() == isobus::task_controller_object::ObjectTypes::DeviceProperty) {
            ddis.insert(std::static_pointer_cast<isobus::task_controller_object::DevicePropertyObject>(object)->get_ddi());
        }
    }
    return ddis;
}

bool PoolHas(const std::set<std::uint16_t>& ddis, isobus::DataDescriptionIndex ddi) {
    return ddis.count(static_cast<std::uint16_t>(ddi)) != 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// The DDOP we upload
// ---------------------------------------------------------------------------

test(IsobusTcInterface, ddop_generatesAndParsesBack) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    assertMore(binary.size(), (size_t)0);

    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));
    assertMore((int)parsed.size(), 0);
}

// The reason this file exists. DDIs 513 and 514 are optional members of the
// AEF Tramline Control set; a Level 1 system must also declare 505, 506, 507,
// 508, 509, 510, 511 and 515, or a terminal has no reason to write to the two
// we care about. Declaring only the optional pair is the most likely cause of
// never having seen a Value Command in this project's history
// (NeptuneGPS_Triton#21, sessions 8 to 10). Losing any one of them again
// would be silent on the bench and cost another rig session.
test(IsobusTcInterface, ddop_declaresTheCompleteTramlineControlLevel1Set) {
    const auto ddis = DdisInUploadedPool();
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::SupportedTrackControlLevels));            // 505
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::SetpointTrackControlLevel));    // 506
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::GuidanceTrackSequenceNumber));          // 507
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::UniqueGuidanceReferenceLineID)); // 508
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::ActualGuidanceTrackNumber));               // 509
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::GuidanceTrackNumberToTheRight));           // 510
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::GuidanceTrackNumberToTheLeft));            // 511
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::TrackControlState));            // 515
}

test(IsobusTcInterface, ddop_declaresTheTwoGuidanceDdisWeActuallyConsume) {
    const auto ddis = DdisInUploadedPool();
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::GuidanceLineDeviation));  // 513
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::GNSSQuality));            // 514
}

// #305: DDI 141 Actual Work State, so a John Deere records again: with an
// ISOBUS implement in the task it takes its recording trigger from the
// implement's work state, not from the hitch.
test(IsobusTcInterface, ddop_declaresActualWorkState) {
    const auto ddis = DdisInUploadedPool();
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::ActualWorkState));  // 141
}

// Reported by us, not written by the TC: not settable, and in the default set
// so a TC logs it without being told to.
test(IsobusTcInterface, ddop_actualWorkStateIsOursAndInTheDefaultSet) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));
    bool found = false;
    for (std::uint16_t index = 0; index < parsed.size(); index++) {
        auto object = parsed.get_object_by_index(index);
        if (object == nullptr || object->get_object_type() != isobus::task_controller_object::ObjectTypes::DeviceProcessData) continue;
        auto pd = std::static_pointer_cast<isobus::task_controller_object::DeviceProcessDataObject>(object);
        if (pd->get_ddi() != static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkState)) continue;
        found = true;
        assertEqual((int)pd->get_properties_bitfield(), 0x01);           // default set, not settable
        assertEqual((int)(pd->get_trigger_methods_bitfield() & 0x08), 0x08);  // on change
    }
    assertTrue(found);
}

// 1 = working (hitch down), 0 = not; main.cpp feeds !GetHitch() every pass.
test(IsobusTcInterface, valueRequest_actualWorkStateFollowsSetWorking) {
    Fixture().SetWorking(true);
    assertTrue(Fixture().IsWorking());
    assertEqual(IsobusTcInterfaceTestAccess::Request(Fixture(), 141), (std::int32_t)1);
    Fixture().SetWorking(false);
    assertEqual(IsobusTcInterfaceTestAccess::Request(Fixture(), 141), (std::int32_t)0);
}

test(IsobusTcInterface, ddop_declaresTheConnectorOffsets) {
    const auto ddis = DdisInUploadedPool();
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::DeviceElementOffsetX));
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::DeviceElementOffsetY));
}

// ISO 11783-10 wants exactly one device element of type Device in a pool, and
// a missing one was half of a real TC-side rejection on 2026-08-10.
test(IsobusTcInterface, ddop_hasExactlyOneRootDeviceElement) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));

    int rootElements = 0;
    for (std::uint16_t index = 0; index < parsed.size(); index++) {
        auto object = parsed.get_object_by_index(index);
        if (object == nullptr) continue;
        if (object->get_object_type() != isobus::task_controller_object::ObjectTypes::DeviceElement) continue;
        auto element = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(object);
        if (element->get_type() == isobus::task_controller_object::DeviceElementObject::Type::Device) rootElements++;
    }
    assertEqual(rootElements, 1);
}

// TC11: the AgOpenGPS TC sends guidance data (507-513, 514) only to a client
// with at least one section, which it counts with AgIsoStack's geometry
// reader. So the pool must read back as one boom with one section, 1.5 m
// behind the hitch on the centre line, with the boot width as its width.
test(IsobusTcInterface, ddop_readsBackAsOneBoomWithOneSection) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));

    auto implement = isobus::DeviceDescriptorObjectPoolHelper::get_implement_geometry(parsed);
    int sections = 0;
    for (const auto& boom : implement.booms) {
        sections += (int)boom.sections.size();
        for (const auto& subBoom : boom.subBooms) sections += (int)subBoom.sections.size();
    }
    assertEqual((int)implement.booms.size(), 1);
    assertEqual(sections, 1);

    const auto& section = implement.booms[0].sections[0];
    assertTrue(section.xOffset_mm.exists());
    assertEqual((long)section.xOffset_mm.get(), -1500L);
    assertEqual((long)section.yOffset_mm.get(), 0L);
    assertEqual((long)section.width_mm.get(), (long)(tcImplement->GetOffset() * 10));
}

// No section control: the TC gets nothing to switch. DDI 161 is there, but
// ours to report (TC12): not settable.
test(IsobusTcInterface, ddop_declaresNoSectionControlDdis) {
    const auto ddis = DdisInUploadedPool();
    assertFalse(PoolHas(ddis, isobus::DataDescriptionIndex::SectionControlState));             // 160
    assertFalse(PoolHas(ddis, isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16));  // 290

    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));
    bool found = false;
    for (std::uint16_t index = 0; index < parsed.size(); index++) {
        auto object = parsed.get_object_by_index(index);
        if (object == nullptr || object->get_object_type() != isobus::task_controller_object::ObjectTypes::DeviceProcessData) continue;
        auto pd = std::static_pointer_cast<isobus::task_controller_object::DeviceProcessDataObject>(object);
        if (pd->get_ddi() != static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16)) continue;
        found = true;
        assertEqual((int)pd->get_properties_bitfield(), 0x01);  // default set, not settable
    }
    assertTrue(found);
}

// The section's state is the hitch: 01 working, 00 not; the other fifteen 11.
test(IsobusTcInterface, valueRequest_condensedWorkStateFollowsSetWorking) {
    Fixture().SetWorking(true);
    assertEqual((long)IsobusTcInterfaceTestAccess::Request(Fixture(), 161), (long)(std::int32_t)0xFFFFFFFDu);
    Fixture().SetWorking(false);
    assertEqual((long)IsobusTcInterfaceTestAccess::Request(Fixture(), 161), (long)(std::int32_t)0xFFFFFFFCu);
}

test(IsobusTcInterface, ddop_declaresTheSwathWidthAndTheWorkingWidth) {
    const auto ddis = DdisInUploadedPool();
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::GuidanceLineSwathWidth));  // 512
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::ActualWorkingWidth));      // 67
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::DefaultWorkingWidth));     // 68
}

// ---------------------------------------------------------------------------
// What the TC writes to us, and what we answer when it asks
//
// Both callbacks are static and take plain integers, so they are driven here
// exactly as AgIsoStack drives them, with the interface as the parent pointer.
// ---------------------------------------------------------------------------

namespace {

bool Command(isobus::DataDescriptionIndex ddi, std::int32_t value) {
    return IsobusTcInterfaceTestAccess::Command(Fixture(), static_cast<std::uint16_t>(ddi), value);
}

std::int32_t Request(isobus::DataDescriptionIndex ddi) {
    return IsobusTcInterfaceTestAccess::Request(Fixture(), static_cast<std::uint16_t>(ddi));
}

}  // namespace

test(IsobusTcInterface, valueCommand_deviationAndQualityAreStoredAndTimestamped) {
    millisValue(1000);
    assertTrue(Command(isobus::DataDescriptionIndex::GuidanceLineDeviation, -250));
    assertEqual(Fixture().GetDrpDeviationMm(), -250);
    assertEqual(Fixture().GetDrpTimestamp(), (unsigned long)1000);

    millisValue(1500);
    assertTrue(Command(isobus::DataDescriptionIndex::GNSSQuality, 4));
    assertEqual((int)Fixture().GetTcGnssQuality(), 4);
    assertEqual(Fixture().GetQualityTimestamp(), (unsigned long)1500);
    millisValue(0);
}

test(IsobusTcInterface, valueCommand_countsEveryDdi_notJustTheOnesWeUse) {
    const unsigned long before = Fixture().GetValueCommandCount();
    Command(isobus::DataDescriptionIndex::GuidanceLineDeviation, 1);
    Command(static_cast<isobus::DataDescriptionIndex>(0x1234), 99);   // nothing we declare
    assertEqual(Fixture().GetValueCommandCount(), before + 2);
    assertEqual((int)Fixture().GetLastValueCommandDdi(), 0x1234);
}

// The Tramline Control probe's answer. It is latched separately from
// lastValueCommandDdi, which any later DDI overwrites: the point of the probe
// is that 506 arrived at all, so it has to survive to be read off the debug
// menu afterwards. Level 0 ("no common level") still counts as an answer.
test(IsobusTcInterface, valueCommand_tramlineSetpointLatchesEvenAtLevelZero) {
    Command(isobus::DataDescriptionIndex::SetpointTrackControlLevel, 0);
    assertTrue(Fixture().HasTramlineSetpoint());
    assertEqual(Fixture().GetTramlineSetpointLevel(), 0);

    Command(isobus::DataDescriptionIndex::GNSSQuality, 2);   // a later, different DDI
    assertTrue(Fixture().HasTramlineSetpoint());             // still latched
    assertEqual(Fixture().GetTramlineSetpointLevel(), 0);
}

test(IsobusTcInterface, valueCommand_guidanceTrackFieldsAreStored) {
    Command(isobus::DataDescriptionIndex::GuidanceTrackSequenceNumber, 7);
    assertTrue(Fixture().HasGuidanceTrackInfo());
    assertEqual((int)Fixture().GetTramlineSequenceNumber(), 7);

    Command(isobus::DataDescriptionIndex::UniqueGuidanceReferenceLineID, 42);
    assertEqual((int)Fixture().GetAbLineId(), 42);

    Command(isobus::DataDescriptionIndex::TrackControlState, 1);
    assertTrue(Fixture().HasTrackControlState());
    assertEqual((int)Fixture().GetCommandedTrackControlState(), 1);
}

test(IsobusTcInterface, valueCommand_swathWidthIsStoredAndEchoed) {
    Command(isobus::DataDescriptionIndex::GuidanceLineSwathWidth, 1680);
    assertTrue(Fixture().HasSwathWidth());
    assertEqual((int)Fixture().GetSwathWidthMm(), 1680);
    assertEqual((int)Request(isobus::DataDescriptionIndex::GuidanceLineSwathWidth), 1680);
}

// DDI 67 is the measured width, in mm; ImplementPlough keeps cm.
test(IsobusTcInterface, valueRequest_actualWorkingWidthIsTheMeasuredWidthInMm) {
    assertEqual((int)Request(isobus::DataDescriptionIndex::ActualWorkingWidth),
                (int)(tcImplement->GetPosition() * 10));
}

test(IsobusTcInterface, valueRequest_echoesWhatTheTcWroteToUs) {
    Command(isobus::DataDescriptionIndex::GuidanceLineDeviation, 125);
    Command(isobus::DataDescriptionIndex::GNSSQuality, 4);
    Command(isobus::DataDescriptionIndex::GuidanceTrackSequenceNumber, 9);
    Command(isobus::DataDescriptionIndex::UniqueGuidanceReferenceLineID, 77);

    assertEqual((int)Request(isobus::DataDescriptionIndex::GuidanceLineDeviation), 125);
    assertEqual((int)Request(isobus::DataDescriptionIndex::GNSSQuality), 4);
    assertEqual((int)Request(isobus::DataDescriptionIndex::GuidanceTrackSequenceNumber), 9);
    assertEqual((int)Request(isobus::DataDescriptionIndex::UniqueGuidanceReferenceLineID), 77);
}

// DDI 515 shares DDI 160's definition: the TC sets the state and the client
// answers with its own. Ours is 0 and stays there until something actually
// performs track control. Answering with the commanded value would be a lie
// the TC has no way to detect.
test(IsobusTcInterface, valueRequest_trackControlStateAnswersOurOwnState_notTheCommandedOne) {
    Command(isobus::DataDescriptionIndex::TrackControlState, 1);
    assertEqual((int)Fixture().GetCommandedTrackControlState(), 1);
    assertEqual((int)Request(isobus::DataDescriptionIndex::TrackControlState), 0);
}

test(IsobusTcInterface, valueRequest_countsEveryRequest) {
    const unsigned long before = Fixture().GetValueRequestCount();
    Request(isobus::DataDescriptionIndex::GNSSQuality);
    Request(static_cast<isobus::DataDescriptionIndex>(0x4321));   // nothing we declare
    assertEqual(Fixture().GetValueRequestCount(), before + 2);
}


// ---------------------------------------------------------------------------
// Reconnect watchdog (GitHub issue #18)
//
// Deliberately slower than the VT's 10 s equivalent: TaskControllerClient's
// own connect sequence includes a six-second WaitForStartUpDelay, so a
// shorter window would cut off a re-attempt already in progress. The
// connected branch is not reachable from a host test -- get_is_connected()
// reads a private state machine only a real TC server can advance -- so what
// is pinned here is the whole of the disconnected policy.
// ---------------------------------------------------------------------------

namespace {

void ResetTcWatchdog() {
    millisValue(0);
    IsobusTcInterfaceTestAccess::ResetWatchdog(Fixture());
    tcDebug.Clear();
}

void TcWatchdogAt(unsigned long ms) {
    millisValue(ms);
    IsobusTcInterfaceTestAccess::Watchdog(Fixture());
}

}  // namespace

test(IsobusTcInterface, watchdog_neverFiresBeforeAFirstConnection) {
    ResetTcWatchdog();
    IsobusTcInterfaceTestAccess::SetHasEverConnected(Fixture(), false);
    // A TC that has never connected is GitHub issue #19's case: the partner
    // had been evicted from AgIsoStack's control-function table, and
    // restarting the client would not have helped. Firing during the normal
    // six-second startup delay would only interrupt a connection in progress.
    for (unsigned long t = 0; t <= 180000UL; t += 5000UL) {
        TcWatchdogAt(t);
    }
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)0);
    assertEqual(IsobusTcInterfaceTestAccess::DisconnectedSinceMs(Fixture()), (unsigned long)0);
    millisValue(0);
}

test(IsobusTcInterface, watchdog_firstTickAfterAConnectionOnlyStartsTheClock) {
    ResetTcWatchdog();
    IsobusTcInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    TcWatchdogAt(2000);
    assertEqual(IsobusTcInterfaceTestAccess::DisconnectedSinceMs(Fixture()), (unsigned long)2000);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)0);
    millisValue(0);
}

test(IsobusTcInterface, watchdog_waitsFifteenSecondsBeforeTheFirstRestart) {
    ResetTcWatchdog();
    IsobusTcInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    TcWatchdogAt(2000);

    // 14999 ms into the outage. Shorter than this and the client's own
    // six-second startup delay plus its handshake would be cut off mid-way.
    TcWatchdogAt(16999);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)0);

    TcWatchdogAt(17000);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)1);
    assertTrue(tcDebug.Contains("forcing restart 1"));
    assertTrue(tcDebug.Contains("TC: disconnected 15s"));
    millisValue(0);
}

test(IsobusTcInterface, watchdog_spacesRepeatRestartsFifteenSecondsApart) {
    ResetTcWatchdog();
    IsobusTcInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    TcWatchdogAt(2000);
    TcWatchdogAt(17000);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)1);

    // A TC server that is genuinely gone stays gone. Without the retry
    // spacing this would restart the client on every loop pass.
    TcWatchdogAt(31999);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)1);

    TcWatchdogAt(32000);
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)2);
    millisValue(0);
}

test(IsobusTcInterface, watchdog_restartsAreOnePerFifteenSecondsNotOnePerLoop) {
    ResetTcWatchdog();
    IsobusTcInterfaceTestAccess::SetHasEverConnected(Fixture(), true);
    // Ninety seconds of outage pumped at a realistic loop rate. The attempt
    // count is what a rig operator reads off the debug menu, so it has to
    // track wall time rather than iteration count.
    for (unsigned long t = 2000; t <= 92000UL; t += 20UL) {
        TcWatchdogAt(t);
    }
    // The outage starts at 2000; restarts land at 17000, 32000 ... 92000.
    assertEqual(Fixture().GetReconnectAttemptCount(), (unsigned int)6);
    millisValue(0);
}


// ---------------------------------------------------------------------------
// WidthPlanning: the TC's track data and an XTE fix in, the plough's planned
// width out, the plan in EEPROM 176-185.
// ---------------------------------------------------------------------------

namespace {

unsigned long gPlanClock = 0;

// One XTE fix (PGN 129283's path into GuidanceSource), the TC's track data
// for track 81 / left 80 at 1.6 m spacing in field 3, then a planner step.
void PlanStep(triton::WidthPlanning& planning, int xteCm = 0) {
    if (gPlanClock < millis()) gPlanClock = millis();
    gPlanClock += 1000;
    millisValue(gPlanClock);
    Command(isobus::DataDescriptionIndex::UniqueGuidanceReferenceLineID, (3 << 16) | 1);
    Command(isobus::DataDescriptionIndex::ActualGuidanceTrackNumber, 81);
    Command(isobus::DataDescriptionIndex::GuidanceTrackNumberToTheLeft, 80);
    Command(isobus::DataDescriptionIndex::GuidanceLineSwathWidth, 1600);
    tcGuidance.SetSpeedKnots(4.0f);
    tcGuidance.SetXte(xteCm);
    planning.Update();
}

}  // namespace

test(WidthPlanning, xteMode_leavesTheXteControl) {
    VehicleSensors tractor;
    triton::WidthPlanning planning(tcImplement, &tractor, &tcGuidance, &Fixture());
    planning.Begin();
    planning.SetMode(triton::WidthMode::Xte);
    PlanStep(planning);
    assertTrue(planning.HasTrackSupport());
    assertFalse(tcImplement->HasPlannedWidth());
    gPlanClock = 0;
    millisValue(0);
}

// Fan-out after NEW FIELD: the first pass at the set width, and the plan in
// EEPROM, read back by the next power-up.
test(WidthPlanning, newField_firstPassAtTheSetWidth_andTheStoredPlan) {
    VehicleSensors tractor;   // hitch down: working
    triton::WidthPlanning planning(tcImplement, &tractor, &tcGuidance, &Fixture());
    planning.Begin();
    planning.SetMode(triton::WidthMode::FanOut);
    PlanStep(planning);
    planning.NewField();
    PlanStep(planning);
    assertEqual((int)planning.GetPlanner().GetReason(), (int)triton::PlanReason::Planned);
    assertTrue(tcImplement->HasPlannedWidth());
    assertEqual((int)planning.GetPlanner().GetSetpointCm(), (int)tcImplement->GetOffset());
    assertEqual((int)planning.GetPlanner().GetState(), (int)triton::PlanState::FirstPass);
    assertEqual((int)EEPROM.read(triton::kEepromPlan), 0x5A);

    triton::WidthPlanning afterPowerUp(tcImplement, &tractor, &tcGuidance, &Fixture());
    afterPowerUp.Begin();
    assertEqual((int)afterPowerUp.GetMode(), (int)triton::WidthMode::FanOut);
    assertEqual((int)afterPowerUp.GetPlanner().GetState(), (int)triton::PlanState::FirstPass);
    assertTrue(afterPowerUp.GetPlanner().GetStored().passStarted);

    planning.SetMode(triton::WidthMode::Xte);   // leave the shared EEPROM as found
    PlanStep(planning);
    assertFalse(tcImplement->HasPlannedWidth());
    gPlanClock = 0;
    millisValue(0);
}

// Without the TC's spacing there is no track support, and no planned width.
test(WidthPlanning, rangeSettings_validatedLikeTheBootLoad) {
    VehicleSensors tractor;
    triton::WidthPlanning planning(tcImplement, &tractor, &tcGuidance, &Fixture());
    planning.Begin();
    planning.SetWidthRangePerShare(36, 48);
    planning.SetWidthRangePerShare(50, 40);   // min above max: refused
    planning.SetWidthRangePerShare(10, 48);   // below 20 cm per share: refused
    assertEqual((int)planning.GetWminPerShare(), 36);
    assertEqual((int)planning.GetWmaxPerShare(), 48);
    assertEqual((int)planning.GetConfig().wminCm, 36 * (int)tcImplement->GetShares());
}


// ":plan" on the serial console: settings stored at once, out-of-range
// values refused, and the state printed after every command.
test(WidthPlanning, consolePlan_setsAndRefuses) {
    VehicleSensors tractor;
    triton::WidthPlanning planning(tcImplement, &tractor, &tcGuidance, &Fixture());
    planning.Begin();
    triton::PloughSettings settings(tcImplement, &tractor, &tcGuidance, nullptr);
    triton::SettingsConsole console(&tcDebug, &settings);
    console.AttachPlanning(&planning);

    char mode[] = "plan mode fan";
    tcDebug.Clear();
    console.HandleLine(mode, 0);
    assertEqual((int)planning.GetMode(), (int)triton::WidthMode::FanOut);
    assertTrue(tcDebug.Contains("P:mode=fan"));
    assertTrue(tcDebug.Contains("OK"));

    char range[] = "plan range 50 40";
    tcDebug.Clear();
    console.HandleLine(range, 0);
    assertTrue(tcDebug.Contains("ERR:range"));

    char k[] = "plan k 12";
    console.HandleLine(k, 0);
    assertEqual((int)planning.GetK(), 12);

    char back[] = "plan mode xte";   // leave the shared EEPROM as found
    console.HandleLine(back, 0);
    char kback[] = "plan k 10";
    console.HandleLine(kback, 0);
}

#endif  // ISOBUS
