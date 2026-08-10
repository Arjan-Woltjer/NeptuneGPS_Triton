/*
  IsobusTcInterface - ISOBUS Task Controller client for the plough controller
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
#include "IsobusTcInterface.hpp"

// The header is itself empty unless ISOBUS is defined (see its own comment) --
// guard the body too, so this compiles to an empty translation unit instead
// of failing on undeclared isobus:: symbols when PlatformIO's LDF pulls this
// file in on teensy41_serial anyway.
#ifdef ISOBUS

using namespace isobus;

namespace triton
{

namespace {

// Object IDs, unique within the DDOP. Element numbers (used for process
// data addressing, separate namespace from object IDs) mirror the design
// doc's numbering: 0=Device, 1=Connector, 2=Function.
constexpr std::uint16_t kElementDevice    = 0;
constexpr std::uint16_t kElementConnector = 1;
constexpr std::uint16_t kElementFunction  = 2;

constexpr std::uint16_t kObjDevice       = 0;
constexpr std::uint16_t kObjPloughDevice = 1;  // root DeviceElement, Type::Device -- see buildDdop()
constexpr std::uint16_t kObjConnector    = 2;
constexpr std::uint16_t kObjOffsetX      = 3;
constexpr std::uint16_t kObjOffsetY      = 4;
constexpr std::uint16_t kObjFunction     = 5;
constexpr std::uint16_t kObjDeviation    = 6;
constexpr std::uint16_t kObjQuality      = 7;

// TODO: measure against the actual plough frame before trusting DDI 513 --
// see Triton_TC_Client_Design.md sec 4.3 ("the offsets are part of the
// control path") and the Phase 3 test plan (sec 8), which validates DDI
// 513 specifically by varying these and checking the reported deviation
// tracks the declared geometry. Zero is an explicit "not yet measured"
// placeholder, not a real offset -- do not trust DDI 513 output until
// these are replaced with a real measurement.
constexpr std::int32_t kHitchOffsetXMm = 0;
constexpr std::int32_t kHitchOffsetYMm = 0;

}  // namespace

// ------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------
IsobusTcInterface::IsobusTcInterface(Stream* serialDebug, ImplementPlough* implement, GuidanceSource* guidance,
                                      std::shared_ptr<InternalControlFunction> controlFunction)
    : serialDebug(serialDebug), implement(implement), guidance(guidance), controlFunction(controlFunction) {
}

// ------------------------------------------------------------------
// DDOP -- see this class's header comment for what's deliberately NOT
// declared (working width, work state) and why.
// ------------------------------------------------------------------
void IsobusTcInterface::buildDdop() {
    ddop = std::make_shared<DeviceDescriptorObjectPool>();

    // All 0xFF = "not yet received from the TC's language command" -- the
    // client's own state machine (RequestLanguage/WaitForLanguageResponse)
    // runs before DDOP upload; we don't populate this ourselves.
    std::array<std::uint8_t, task_controller_object::DeviceObject::MAX_STRUCTURE_AND_LOCALIZATION_LABEL_LENGTH> localizationLabel;
    localizationLabel.fill(0xFF);

    ddop->add_device("MeijWorks Ploegbesturing",
                      "0.1.0",
                      "001",
                      "TC02",  // Structure label -- bumped from TC01 for this
                               // tree-shape fix (root Device element +
                               // child-object references added, see below).
                               // Bump this on every DDOP tree change
                               // (added/removed/renumbered objects), see
                               // design doc sec 4.5. Terminals cache pools by
                               // this label.
                      localizationLabel,
                      {},  // no extended structure label
                      controlFunction->get_NAME().get_full_name());

    // Mandatory root Device-type element -- ISO 11783-10 requires exactly one
    // per DDOP (see task_controller_object::DeviceElementObject::Type::Device's
    // own doc comment in AgIsoStack: "the device descriptor object pool shall
    // have one device element of type device"). Connector/Function hang off
    // this, not off the DVC (kObjDevice) directly. Missing entirely --
    // confirmed as (part of) the cause of a real TC-side DDOP rejection on
    // hardware 2026-08-10 ("Faulting parent ID: 1 Faulting object: 0" /
    // "Unknown object reference (missing object)").
    ddop->add_device_element("Plough", kElementDevice, kObjDevice,
                              task_controller_object::DeviceElementObject::Type::Device, kObjPloughDevice);

    ddop->add_device_element("Hitch", kElementConnector, kObjPloughDevice,
                              task_controller_object::DeviceElementObject::Type::Connector, kObjConnector);
    ddop->add_device_property("Offset X", kHitchOffsetXMm,
                               static_cast<std::uint16_t>(DataDescriptionIndex::DeviceElementOffsetX),
                               NULL_OBJECT_ID, kObjOffsetX);
    ddop->add_device_property("Offset Y", kHitchOffsetYMm,
                               static_cast<std::uint16_t>(DataDescriptionIndex::DeviceElementOffsetY),
                               NULL_OBJECT_ID, kObjOffsetY);
    // add_device_property()/add_device_process_data() don't take a parent --
    // DeviceElementObject::add_reference_to_child_object() is the only thing
    // that actually attaches a DPT/DPD to its owning DET in the generated
    // binary pool. Never called before this fix, so Offset X/Y were floating,
    // unattached objects -- the other half of the "Unknown object reference"
    // rejection above.
    auto connectorElement = std::static_pointer_cast<task_controller_object::DeviceElementObject>(ddop->get_object_by_id(kObjConnector));
    connectorElement->add_reference_to_child_object(kObjOffsetX);
    connectorElement->add_reference_to_child_object(kObjOffsetY);

    ddop->add_device_element("Ploughbody", kElementFunction, kObjPloughDevice,
                              task_controller_object::DeviceElementObject::Type::Function, kObjFunction);

    // Both DDI 513 and 514 are written TO us by the TC (Settable), not
    // reported BY us -- see the class header's source-arbitration note.
    // Trigger methods declare what we're willing to accept updates as;
    // design doc sec 4.4 recommends on-change plus a time interval so a
    // stale channel is detectable independent of a Permit/connected check.
    constexpr std::uint8_t kSettable = static_cast<std::uint8_t>(task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable);
    constexpr std::uint8_t kTriggers = static_cast<std::uint8_t>(task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange) |
                                        static_cast<std::uint8_t>(task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval);

    ddop->add_device_process_data("Guidance Deviation",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::GuidanceLineDeviation),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjDeviation);
    ddop->add_device_process_data("GNSS Quality",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::GNSSQuality),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjQuality);

    // Same attachment requirement as the Connector's Offset X/Y above.
    auto functionElement = std::static_pointer_cast<task_controller_object::DeviceElementObject>(ddop->get_object_by_id(kObjFunction));
    functionElement->add_reference_to_child_object(kObjDeviation);
    functionElement->add_reference_to_child_object(kObjQuality);
}

// ------------------------------------------------------------------
// Begin -- call once from setup(), after the control function's address
// is claimed.
// ------------------------------------------------------------------
void IsobusTcInterface::Begin() {
    // Partner: any control function claiming the TaskController function
    // code. Pattern matches IsobusVtInterface::Begin()'s VT partner filter,
    // confirmed working on hardware.
    const NAMEFilter tcFilter(NAME::NAMEParameters::FunctionCode,
                              static_cast<uint8_t>(NAME::Function::TaskController));
    const std::vector<NAMEFilter> tcNameFilters = { tcFilter };
    // Must go through the factory method -- see the matching comment on
    // IsobusVtInterface::Begin()'s identical fix (2026-08-10 hardware
    // session). A direct std::make_shared construction never registers into
    // CANNetworkManager's partneredControlFunctions list, so it can never be
    // matched against an incoming Address Claim and get_address_valid()
    // stays false forever.
    auto partnerTC = CANNetworkManager::CANNetwork.create_partnered_control_function(0, tcNameFilters);

    buildDdop();

    tcClient = std::make_shared<TaskControllerClient>(partnerTC, controlFunction, nullptr);

    tcClient->configure(ddop,
                        0,      // maxNumberBoomsSupported -- plough has no booms
                        0,      // maxNumberSectionsSupported -- no section control
                        1,      // maxNumberChannelsSupportedForPositionBasedControl
                        false,  // reportToTCSupportsDocumentation
                        false,  // ...TCGEOWithoutPositionBasedControl
                        true,   // ...TCGEOWithPositionBasedControl <-- required for 513/514
                        false,  // ...PeerControlAssignment
                        false); // ...ImplementSectionControl

    tcClient->add_value_command_callback(OnValueCommand, this);
    tcClient->add_request_value_callback(OnValueRequest, this);

    // false = do NOT spawn a thread. This build uses
    // can_hardware_interface_single_thread; Update() pumps the client, same
    // as IsobusVtInterface's vtClient.
    tcClient->initialize(false);
}

// ------------------------------------------------------------------
// Update -- call every loop() iteration.
// ------------------------------------------------------------------
void IsobusTcInterface::Update() {
    if (tcClient) {
        tcClient->update();
    }
}

// ------------------------------------------------------------------
// Value command callback -- the TC writes to us here. This is where
// 513/514 arrive.
// ------------------------------------------------------------------
bool IsobusTcInterface::OnValueCommand(std::uint16_t elementNumber,
                                       std::uint16_t DDI,
                                       std::int32_t  processVariableValue,
                                       void*         parentPointer) {
    auto* self = static_cast<IsobusTcInterface*>(parentPointer);
    if (self == nullptr) {
        return true;
    }

    // Unconditional -- fires for ANY DDI the TC pushes, not just 513/514.
    // See the header comment on GetValueCommandCount() for why this exists
    // separately from the two specific counters below.
    self->valueCommandCount++;
    self->lastValueCommandDdi = DDI;
    self->lastValueCommandMs  = millis();

    switch (DDI) {
        case static_cast<std::uint16_t>(DataDescriptionIndex::GuidanceLineDeviation):  // 513, mm
            // Wire value is already signed (mm, positive = guidance line
            // right of the DRP); AgIsoStack decodes it into processVariableValue directly.
            self->drpDeviationMm = processVariableValue;
            self->lastDrpUpdate  = millis();
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::GNSSQuality):  // 514
            self->tcGnssQuality     = static_cast<uint8_t>(processVariableValue);
            self->lastQualityUpdate = millis();
            break;

        default:
            break;
    }

    // Return true unconditionally: a TC expects every "settable" DPD in the
    // DDOP to be writable. Returning false triggers TC-side error handling.
    return true;
}

// ------------------------------------------------------------------
// Value request callback -- the TC reads from us here. Both DDIs we
// declare are Settable (TC -> us), so a request here is the TC re-syncing
// its own idea of what it last commanded (e.g. after a reconnect), not us
// reporting fresh data -- echo back the last received value.
// ------------------------------------------------------------------
bool IsobusTcInterface::OnValueRequest(std::uint16_t elementNumber,
                                       std::uint16_t DDI,
                                       std::int32_t& processVariableValue,
                                       void*         parentPointer) {
    auto* self = static_cast<IsobusTcInterface*>(parentPointer);
    if (self == nullptr) {
        processVariableValue = 0;
        return true;
    }

    self->valueRequestCount++;

    switch (DDI) {
        case static_cast<std::uint16_t>(DataDescriptionIndex::GuidanceLineDeviation):
            processVariableValue = self->drpDeviationMm;
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::GNSSQuality):
            processVariableValue = self->tcGnssQuality;
            break;

        default:
            processVariableValue = 0;
            break;
    }
    return true;
}

}  // namespace triton

#endif  // ISOBUS
