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
#include <map>
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
#include <isobus/isobus/isobus_device_descriptor_object_pool.hpp>
#include <isobus/isobus/isobus_standard_data_description_indices.hpp>
#include <isobus/isobus/isobus_task_controller_client_objects.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "FakeCanPlugin.hpp"
#include "TestBus.hpp"
#include "isobus/IsobusTcInterface.hpp"

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
    static void ReportWidth(IsobusTcInterface& tc) { tc.reportWidthIfChanged(); }
    static void ResetWidth(IsobusTcInterface& tc) {
        tc.lastWidthCm      = -1;
        tc.widthChangeCount = 0;
        tc.widthReportCount = 0;
    }

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

// TC08 (#150): the plough's working width. With none declared, a John Deere
// filled in its own 3 m on every connect and moved the guidance lines with it
// (session 11). 66, the setpoint, is left out on purpose: the terminal must
// not be able to command the plough's width.
test(IsobusTcInterface, ddop_declaresTheWorkingWidth_butNotASetpoint) {
    const auto ddis = DdisInUploadedPool();
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::ActualWorkingWidth));    // 67
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::DefaultWorkingWidth));   // 68
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::MinimumWorkingWidth));   // 69
    assertTrue(PoolHas(ddis, isobus::DataDescriptionIndex::MaximumWorkingWidth));   // 70
    assertFalse(PoolHas(ddis, isobus::DataDescriptionIndex::SetpointWorkingWidth)); // 66
}

// In the bytes we upload: 67 is reported by us (not settable, in the default
// set, like 141), offered on change, and hangs off the Ploughbody function
// element; the three limits follow the share count, in mm.
test(IsobusTcInterface, ddop_workingWidthIsReportedOnChangeWithShareBasedLimits) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));

    using namespace isobus::task_controller_object;
    const std::int32_t shares = tcImplement->GetShares();
    std::uint16_t actualId = 0xFFFF;
    std::uint8_t  actualProperties = 0, actualTriggers = 0;
    std::map<std::uint16_t, std::int32_t> limits;
    std::set<std::uint16_t> functionChildren;
    for (std::uint16_t i = 0; i < parsed.size(); i++) {
        auto object = parsed.get_object_by_index(i);
        if (object == nullptr) continue;
        if (object->get_object_type() == ObjectTypes::DeviceProcessData) {
            auto dpd = std::static_pointer_cast<DeviceProcessDataObject>(object);
            if (dpd->get_ddi() == 67) {
                actualId         = dpd->get_object_id();
                actualProperties = dpd->get_properties_bitfield();
                actualTriggers   = dpd->get_trigger_methods_bitfield();
            }
        } else if (object->get_object_type() == ObjectTypes::DeviceProperty) {
            auto dpt = std::static_pointer_cast<DevicePropertyObject>(object);
            if (dpt->get_ddi() >= 68 && dpt->get_ddi() <= 70) limits[dpt->get_ddi()] = dpt->get_value();
        } else if (object->get_object_type() == ObjectTypes::DeviceElement) {
            auto det = std::static_pointer_cast<DeviceElementObject>(object);
            if (det->get_type() == DeviceElementObject::Type::Function) {
                for (std::uint16_t c = 0; c < det->get_number_child_objects(); c++) {
                    functionChildren.insert(det->get_child_object_id(c));
                }
            }
        }
    }
    assertNotEqual((int)actualId, 0xFFFF);
    assertTrue(functionChildren.count(actualId) != 0);
    assertEqual((int)actualProperties, 0x01);   // default set, not settable
    assertTrue((actualTriggers & static_cast<std::uint8_t>(DeviceProcessDataObject::AvailableTriggerMethods::OnChange)) != 0);
    assertEqual(limits[68], shares * 40 * 10);
    assertEqual(limits[69], shares * 20 * 10);
    assertEqual(limits[70], shares * 60 * 10);
}

// A Wider/Narrower press changes the plough's working width, so DDI 67 must
// follow it (#150). The fixture has no TC server, so nothing is actually sent
// -- the trigger only fires while connected, and get_is_connected() is driven
// by a state machine only a real server can advance. What this pins is the
// change detection that decides when to fire.
test(IsobusTcInterface, widthReport_countsAChange_butNotTheStartingValue) {
    Fixture();
    IsobusTcInterfaceTestAccess::ResetWidth(Fixture());
    IsobusTcInterfaceTestAccess::ReportWidth(Fixture());           // first reading
    assertEqual(Fixture().GetWidthChangeCount(), (unsigned long)0);  // is the baseline, not a change
    IsobusTcInterfaceTestAccess::ReportWidth(Fixture());
    assertEqual(Fixture().GetWidthChangeCount(), (unsigned long)0);  // nothing moved

    // Ten steps up through the plough's own path. The mocked clock stays
    // under AUnit's 10 s test timeout (it runs on the same clock).
    const short int before = tcImplement->GetOffset();
    millisValue(5000);
    tcImplement->Update(0, 10);
    assertEqual((int)tcImplement->GetOffset(), before + 10);
    IsobusTcInterfaceTestAccess::ReportWidth(Fixture());
    assertEqual(Fixture().GetWidthChangeCount(), (unsigned long)1);
    assertEqual((int)Fixture().GetLastWidthCm(), before + 10);
    assertEqual(Fixture().GetWidthReportCount(), (unsigned long)0);   // not connected

    // Put the shared fixture back as it was.
    millisValue(5500);
    tcImplement->Update(0, -10);
    IsobusTcInterfaceTestAccess::ReportWidth(Fixture());
    millisValue(0);
}

// The width in mm, from the plough's own offset in cm (#150).
test(IsobusTcInterface, valueRequest_workingWidthAnswersTheOffsetInMm) {
    Fixture();
    assertMore((int)tcImplement->GetOffset(), 0);
    assertEqual(IsobusTcInterfaceTestAccess::Request(Fixture(), 67),
                (std::int32_t)tcImplement->GetOffset() * 10);
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

// ---------------------------------------------------------------------------
// SPIKE #347 (TC09): the plough as a one-section implement, for a TC that only
// does section control (the InCommand 1200). In the bytes we upload.
// ---------------------------------------------------------------------------

// One Section element, under the Ploughbody function element, carrying its two
// offsets and the SAME width object as Ploughbody's DDI 67, so the section is
// as wide as the plough is set.
test(IsobusTcInterface, spike347_ddopHasOneSectionUnderPloughbodyWithTheSharedWidth) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));

    using namespace isobus::task_controller_object;
    int sections = 0;
    std::uint16_t sectionParent = 0xFFFF, functionId = 0xFFFE, widthId = 0xFFFD;
    std::set<std::uint16_t> sectionChildren, functionChildren;
    for (std::uint16_t i = 0; i < parsed.size(); i++) {
        auto object = parsed.get_object_by_index(i);
        if (object == nullptr) continue;
        if (object->get_object_type() == ObjectTypes::DeviceElement) {
            auto det = std::static_pointer_cast<DeviceElementObject>(object);
            if (det->get_type() == DeviceElementObject::Type::Section) {
                sections++;
                sectionParent = det->get_parent_object();
                for (std::uint16_t c = 0; c < det->get_number_child_objects(); c++) sectionChildren.insert(det->get_child_object_id(c));
            } else if (det->get_type() == DeviceElementObject::Type::Function) {
                functionId = det->get_object_id();
                for (std::uint16_t c = 0; c < det->get_number_child_objects(); c++) functionChildren.insert(det->get_child_object_id(c));
            }
        } else if (object->get_object_type() == ObjectTypes::DeviceProcessData) {
            auto dpd = std::static_pointer_cast<DeviceProcessDataObject>(object);
            if (dpd->get_ddi() == 67) widthId = dpd->get_object_id();
        }
    }
    assertEqual(sections, 1);
    assertEqual((int)sectionParent, (int)functionId);
    assertEqual((int)sectionChildren.size(), 3);              // offset X, offset Y, width
    assertTrue(sectionChildren.count(widthId) != 0);
    assertTrue(functionChildren.count(widthId) != 0);         // still on Ploughbody too
}

// On the parent, as the DDI 161 and 290 definitions ask: 161 is ours (default
// set, not settable), 290 and 160 are the TC's to write (settable).
test(IsobusTcInterface, spike347_ddopDeclaresTheSectionControlDdis) {
    std::vector<std::uint8_t> binary;
    assertTrue(Fixture().GenerateDdopBinary(binary));
    isobus::DeviceDescriptorObjectPool parsed;
    assertTrue(parsed.deserialize_binary_object_pool(binary, isobus::NAME(0)));

    using namespace isobus::task_controller_object;
    std::map<std::uint16_t, int> properties;
    for (std::uint16_t i = 0; i < parsed.size(); i++) {
        auto object = parsed.get_object_by_index(i);
        if (object == nullptr || object->get_object_type() != ObjectTypes::DeviceProcessData) continue;
        auto dpd = std::static_pointer_cast<DeviceProcessDataObject>(object);
        properties[dpd->get_ddi()] = dpd->get_properties_bitfield();
    }
    assertEqual((int)properties.count(161), 1);
    assertEqual((int)properties.count(290), 1);
    assertEqual((int)properties.count(160), 1);
    assertEqual(properties[161], 0x01);   // default set, reported by us
    assertEqual(properties[290], 0x02);   // settable
    assertEqual(properties[160], 0x02);   // settable
    assertEqual((int)properties.count(289), 0);   // no single setpoint work state next to the condensed one
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

// SPIKE #316. DDI 515 shares DDI 160's definition: the TC sets the state and
// the client answers with its own, which is 0 on main. The AEF Tramline
// Control start (Figure 1) has the implement answer the TC's 1, so on this
// branch a read echoes what the TC commanded, back to 0 included.
test(IsobusTcInterface, valueRequest_trackControlStateEchoesTheCommandedOne_spike316) {
    Command(isobus::DataDescriptionIndex::TrackControlState, 1);
    assertEqual((int)Fixture().GetCommandedTrackControlState(), 1);
    assertEqual((int)Request(isobus::DataDescriptionIndex::TrackControlState), 1);

    Command(isobus::DataDescriptionIndex::TrackControlState, 0);
    assertEqual((int)Request(isobus::DataDescriptionIndex::TrackControlState), 0);
}

// A changed 515 is reported to the TC at once (the implement's answer in the
// AEF start sequence). The callback only flags it: AgIsoStack calls it under
// the client mutex, so Update() sends the trigger.
test(IsobusTcInterface, valueCommand_trackControlStateChangeFlagsAReport_spike316) {
    Command(isobus::DataDescriptionIndex::TrackControlState, 1);
    const std::int32_t current = Fixture().GetCommandedTrackControlState();
    Command(isobus::DataDescriptionIndex::TrackControlState, current == 1 ? 0 : 1);
    assertTrue(Fixture().IsTrackControlReportPending());
}

// SPIKE #316: the TC choice. "Any" is the function code alone, as on main;
// TC n adds NAME function instance n - 1 (TC numbering, as with VTs).
test(IsobusTcInterface, tcNameFilters_anyIsFunctionCodeOnly_tcNumberAddsTheInstance_spike316) {
    const auto any = IsobusTcInterface::TcNameFilters(0);
    assertEqual((int)any.size(), 1);
    assertTrue(any[0].get_parameter() == isobus::NAME::NAMEParameters::FunctionCode);
    assertEqual((int)any[0].get_value(), (int)isobus::NAME::Function::TaskController);

    const auto tc2 = IsobusTcInterface::TcNameFilters(2);
    assertEqual((int)tc2.size(), 2);
    assertTrue(tc2[1].get_parameter() == isobus::NAME::NAMEParameters::FunctionInstance);
    assertEqual((int)tc2[1].get_value(), 1);
}

// Choosing another TC rebinds and clears what the last TC told us, so the
// dump describes the TC now bound. The process-data callbacks still work on
// the new client afterwards.
test(IsobusTcInterface, selectTc_rebindsAndClearsTheLastTcsObservations_spike316) {
    Command(isobus::DataDescriptionIndex::TrackControlState, 1);
    Command(isobus::DataDescriptionIndex::SetpointTrackControlLevel, 1);
    assertTrue(Fixture().HasTrackControlState());

    Fixture().SelectTc(2);
    assertEqual((int)Fixture().GetSelectedTcNumber(), 2);
    assertFalse(Fixture().HasTrackControlState());
    assertFalse(Fixture().HasTramlineSetpoint());
    assertEqual(Fixture().GetValueCommandCount(), 0UL);
    assertFalse(Fixture().IsConnected());

    Command(isobus::DataDescriptionIndex::TrackControlState, 1);
    assertEqual((int)Request(isobus::DataDescriptionIndex::TrackControlState), 1);

    Fixture().SelectTc(0);   // the shared fixture goes back to "any"
    assertEqual((int)Fixture().GetSelectedTcNumber(), 0);
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
// SPIKE #347: what the TC writes to the one section never reaches the plough.
// ---------------------------------------------------------------------------

// The section's state is the hitch: section 1 in bits 0-1, the other fifteen
// "not installed" (11). A TC that commands the section off and section control
// on is stored and counted, and 141 and 161 keep saying what the hitch says.
test(IsobusTcInterface, spike347_sectionStateFollowsTheHitch_notTheTc) {
    Fixture().SetWorking(true);
    assertEqual(Request(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16), (std::int32_t)0xFFFFFFFDu);

    const unsigned long before = Fixture().GetSectionSetpointCommandCount();
    assertTrue(Command(isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16, (std::int32_t)0xFFFFFFFCu));  // section 1 off
    assertTrue(Command(isobus::DataDescriptionIndex::SectionControlState, 1));                                     // auto/on
    assertEqual(Fixture().GetSectionSetpointCommandCount(), before + 1);
    assertTrue(Fixture().HasSectionSetpoint());
    assertEqual((int)(Fixture().GetCommandedSectionSetpoint() & 0x3), 0);
    assertTrue(Fixture().HasSectionControlState());
    assertEqual(Fixture().GetCommandedSectionControlState(), (std::int32_t)1);

    assertTrue(Fixture().IsWorking());
    assertEqual(Request(isobus::DataDescriptionIndex::ActualWorkState), (std::int32_t)1);
    assertEqual(Request(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16), (std::int32_t)0xFFFFFFFDu);
    // The TC's own setpoint is echoed back, like the other settable DDIs.
    assertEqual(Request(isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16), (std::int32_t)0xFFFFFFFCu);
    // 160: our own state (manual/off), or the TC's in the echo hex.
    assertEqual(Request(isobus::DataDescriptionIndex::SectionControlState),
                (std::int32_t)(Fixture().IsSectionControlEchoOn() ? 1 : 0));

    Fixture().SetWorking(false);
    assertEqual(Request(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16), (std::int32_t)0xFFFFFFFCu);
    assertTrue(Command(isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16, (std::int32_t)0xFFFFFFFDu));  // section 1 on
    assertFalse(Fixture().IsWorking());
    assertEqual(Request(isobus::DataDescriptionIndex::ActualWorkState), (std::int32_t)0);
    assertEqual(Request(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16), (std::int32_t)0xFFFFFFFCu);
}

#endif  // ISOBUS
