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

// Specific headers for what the body uses beyond the class declaration; see
// the header for why the AgIsoStack.hpp umbrella is avoided.
#include <can_NAME_filter.hpp>
#include <can_network_manager.hpp>
#include <isobus_standard_data_description_indices.hpp>
#include <isobus_task_controller_client_objects.hpp>

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
// The AEF Tramline Control Level 1 required set, all eight DDIs, in one
// device element -- see BuildDdop(). Ordered as the spec lists them.
constexpr std::uint16_t kObjTramlineLevel         = 8;   // DDI 505
constexpr std::uint16_t kObjTramlineSetpointLevel = 9;   // DDI 506
constexpr std::uint16_t kObjTramlineSequence      = 10;  // DDI 507
constexpr std::uint16_t kObjAbLineId              = 11;  // DDI 508
constexpr std::uint16_t kObjActualTrack           = 12;  // DDI 509
constexpr std::uint16_t kObjTrackRight            = 13;  // DDI 510
constexpr std::uint16_t kObjTrackLeft             = 14;  // DDI 511
constexpr std::uint16_t kObjTramlineState         = 15;  // DDI 515

// DDI 505 value: bitfield of Tramline Control Levels we support.
//   bit 0 = Level 1, bit 1 = Level 2, bit 2 = Level 3
//
// Deliberately ZERO -- "I participate in the Tramline Control handshake but
// support no level." This is a probe (GitHub issue #21), not a capability
// claim: Level 1 means "the implement calculates the tramline tracks", which
// a plough does not do and must not advertise.
//
// A tramline-capable TC should still answer with DDI 506 = 0 ("No common
// Level", the spec's explicit no-match case), which is all the probe needs:
// ANY 506 arriving proves the terminal implements Tramline Control, and
// therefore that DDI 513/514 are reachable if we build the feature out
// properly. Silence is the other answer -- but see the caveat in the issue:
// if a terminal short-circuits the reply for a zero-capability implement,
// silence is ambiguous, and the next step is to re-run once with bit 0 set
// before concluding the terminal lacks the feature entirely.
//
// THAT RE-RUN HAS NOW BEEN DONE, and both answers were silence -- see
// Documentation/HardwareTestNotes.md session 7 (2026-09-08, Ag Leader
// InCommand 1200). Run 1 declared 0 for 16 minutes; run 2 temporarily declared
// 0x01 (Level 1) for 6 minutes, with the structure label bumped TC03 -> TC04
// so the terminal could not serve run 1's cached pool. Both runs reported
// connected, task active, TC-GEO-with-position, and `DDOP Activated without
// error`, and both saw zero DDI 506, zero Value Commands and zero Value
// Requests. So the ambiguity above is closed on this terminal: it very likely
// does not implement Tramline Control at all, and no DDOP work will produce
// DDI 513/514 from it.
//
// Deliberately reverted to 0 here. The Level-1 declaration was a diagnostic
// claim made solely to force a reply -- a plough does not calculate tramline
// tracks and must not advertise that it does -- so it stays in the session log
// and out of the firmware. Do not set this to a non-zero value again except as
// a time-boxed probe on a spike branch; if a future terminal genuinely needs
// Tramline Control, build the feature out honestly (DDIs 505, 506, 515, 507,
// 508, 509, 510, 511 per the Basic Requirements doc) rather than claiming a
// level this implement cannot honour.
//
// One caveat carried forward from session 7: zero Value *Requests* is equally
// consistent with our implement never having been mapped into the running task
// on the terminal's own setup screen, and `Task active: Y` does not settle that
// (AgIsoStack warns the flag is unreliable per brand). Confirm the mapping
// before treating the "no Tramline Control" reading as established.
// SESSION 8 (2026-09-08, John Deere terminal) ANSWERED THIS, and the answer is
// that the level value was never the blocker. That terminal has a Tramlines
// option, it was switched on, GPS was live with an RTK fix, and it reported
// `no compatible implements detected` -- both with this declared as 0 and,
// after a reflash under label TC04, with it declared as 0x01. Level 1 is not
// sufficient on its own.
//
// The reason is in NeptuneGPS Documentation/ISOBUS/research/TramlineControl_TC_Support_Research.md sec 6:
// Level 1's *required* DDI set is 505, 506, 507, 508, 509, 510, 511 and 515,
// all in one device element. We declare two of those eight, so a terminal
// declining to see a compatible implement is the correct behaviour, not a
// quirk. It also makes session 7's Ag Leader silence far less likely to have
// meant "no Tramline Control" -- more likely the same incomplete declaration,
// meeting a terminal that says nothing rather than reporting it.
//
// Left at 0, deliberately, and NOT as a placeholder to be flipped: going
// further means declaring the full eight-DDI set, which claims the implement
// calculates tramline tracks. A plough does not. Whether Triton should present
// itself as a tramline implement at all is a product decision that has not
// been made -- the probes established that a terminal *would* talk to us, not
// that we should ask it to.
// Bit 0 set = "Level 1 supported". Now declared for real rather than as a
// probe, because the full required set below backs it up.
//
// Be clear about what this claims. Declaring Level 1 tells a Task Controller
// that this implement calculates tramline tracks. A plough does not, and
// nothing behind these DDIs computes anything yet -- 507 and 515 report
// static "inactive" values. This is deliberate and the user's call: the point
// is to find out whether a terminal will complete the handshake at all once
// the DDOP is well-formed, which session 8 proved it will not do with a
// partial set. If the answer is yes on John Deere, Ag Leader or Raven, then
// whether Triton should present itself as a tramline implement in shipped
// firmware is a product decision to take separately -- do not let this
// constant reach a customer build unexamined.
constexpr std::int32_t kTramlineControlLevelsSupported = 1;

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
                                      std::shared_ptr<InternalControlFunction> controlFunction,
                                      std::shared_ptr<PartneredControlFunction> primaryVtPartner)
    : serialDebug(serialDebug), implement(implement), guidance(guidance), controlFunction(controlFunction),
      primaryVtPartner(primaryVtPartner) {
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
                      "TC05",  // Structure label -- bumped from TC01 for the
                               // tree-shape fix (root Device element +
                               // child-object references added, see below),
                               // then TC02 -> TC03 for the Tramline Control
                               // probe (DDI 505/506 added, see below).
                               //
                               // TC04 IS BURNED -- DO NOT REUSE IT. Session
                               // 7's probe run 2 uploaded a pool under TC04
                               // whose DDI 505 declared Level 1, and any
                               // terminal that took part still caches it under
                               // that label. Reverting DDI 505 to 0 makes this
                               // pool identical to TC03's again, so TC03 was
                               // the honest label to carry until the next real
                               // tree change -- which is this one: TC05 adds
                               // DDIs 507, 508, 509, 510, 511 and 515 to
                               // complete the Level 1 required set (six new
                               // objects), and sets DDI 505 to 0x01.
                               // Bump this on every DDOP tree change
                               // (added/removed/renumbered objects), see
                               // design doc sec 4.5. Terminals cache pools by
                               // this label, and AgIsoStack logs an explicit
                               // error if an updated pool reuses one.
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

    // --- Tramline Control probe (GitHub issue #21) --------------------
    // 513/514 are OPTIONAL members of the AEF Tramline Control DDI set, not
    // standalone process data. Per "Tramline Control -- Basic Requirements
    // v1.16" (ISO 11783-11 DDE supplement, attached to the DDI 505 entity on
    // isobus.net), a Level 1 system also requires DDIs 505, 506, 515, 507,
    // 508, 509, 510 and 511 in the DDOP. Declaring only the two optional ones
    // -- which is what this DDOP did until now -- leaves them as orphan
    // objects a TC has no reason to ever write to, which is the most likely
    // explanation for never having seen a single Value Command in the
    // project's history.
    //
    // Session 8 (2026-09-08, John Deere) settled that declaring the handshake
    // pair alone is not enough: with DDI 505 = 0x01 and nothing else, the
    // terminal still reported "no compatible implements detected". So this
    // now declares the **complete Level 1 required set** -- 505, 506, 507,
    // 508, 509, 510, 511, 515 -- all in one device element, which is what the
    // spec asks for and what that message says we were missing.
    //
    // Per sec 2.2.2/2.2.3 the implement declares 505 (supported levels;
    // "shall not change during runtime", so a DPT not a DPD) and the TC
    // replies with 506 naming the level to use, or 0 for "no common level".
    // Both "shall be listed in the DDOP only once", and 506 "shall be placed
    // in the same device element as DDI 505".
    //
    // Direction, verified against the AEF guideline itself
    // (TramlineControl_BasicRequirements v1.16, now archived in
    // NeptuneGPS Documentation/ISOBUS/reference/dd-attachments/) rather than
    // inferred -- an earlier pass got two of these wrong:
    //   505 implement -> TC   what we support        DPT, static
    //   506 TC -> implement   the level to use       Settable
    //   507 TC -> implement   tramline sequence no.  Settable
    //   508 TC -> implement   unique A-B line ID     Settable
    //   509 TC -> implement   actual track number    Settable
    //   510 TC -> implement   track number right     Settable
    //   511 TC -> implement   track number left      Settable
    //   515 TC <-> implement  track control state    Settable + OnChange
    //
    // 507 reads like an implement-side counter but is not: sec 2.2.22 says it
    // "has to be sent from the Tramline Controller to indicate a new Tramline
    // Sequence to the Implement", and it is what ties 508-511 together into
    // one consistent set -- 508 "shall be sent as first value after the
    // Tramline Sequence".
    //
    // 515 is defined as having "the same purpose and definition like the
    // Section Control State DDI 160", and DDI 160's own entry states that
    // "the property flag 'setable' and the trigger method 'on change' should
    // be used with this DDE": the TC sets the state, and the client replies
    // with its own. So it is Settable in both directions of use, not a
    // read-only report.
    //
    // Everything except 505 is therefore Settable. Note isobus.net has since
    // renamed this family Tramline -> Track (505 is now "Supported Track
    // Control Levels", 515 "Track Control State"); the vendored AgIsoStack
    // enum still uses the older Tramline names, and the DDI numbers are what
    // actually matter on the wire.
    //
    // Placement matches the spec's own example 1b (sec 3.5.2), which groups
    // the tramline DDIs in a dedicated function element -- here alongside
    // 513/514, which already live on Ploughbody.
    ddop->add_device_property("Tramline Control Level", kTramlineControlLevelsSupported,
                               static_cast<std::uint16_t>(DataDescriptionIndex::TramlineControlLevel),
                               NULL_OBJECT_ID, kObjTramlineLevel);
    // Settable: the TC writes this one to us, same direction as 513/514.
    ddop->add_device_process_data("Setpoint Tramline Control Level",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::SetpointTramlineControlLevel),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjTramlineSetpointLevel);
    // Written to us by the TC, like everything else here except 505. Nothing
    // computes anything behind them yet -- see kTramlineControlLevelsSupported
    // -- so a read of 515 answers with our real state, which is "manual/off".
    ddop->add_device_process_data("Tramline Sequence Number",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::TramlineSequenceNumber),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjTramlineSequence);
    ddop->add_device_process_data("Tramline Control State",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::TramlineControlState),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjTramlineState);
    // Guidance-track information the TC pushes to us. Latched for the debug
    // menu; nothing steers off them.
    // Designator abbreviated from "Unique A-B Guidance Reference Line ID" (37
    // chars): AgIsoStack warns at DDOP build time that designators over 32
    // characters are only acceptable if they are 32 or fewer UTF-8 *characters*,
    // and this one is 37 either way. Left long it is a pool a terminal may
    // legitimately reject, which would be indistinguishable from the tramline
    // handshake failing -- the exact question this DDOP exists to answer.
    ddop->add_device_process_data("Unique A-B Guidance Ref Line ID",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::UniqueABGuidanceReferenceLineID),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjAbLineId);
    ddop->add_device_process_data("Actual Track Number",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::ActualTrackNumber),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjActualTrack);
    ddop->add_device_process_data("Track Number to the Right",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::TrackNumberToTheRight),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjTrackRight);
    ddop->add_device_process_data("Track Number to the Left",
                                   static_cast<std::uint16_t>(DataDescriptionIndex::TrackNumberToTheLeft),
                                   NULL_OBJECT_ID,
                                   kSettable, kTriggers, kObjTrackLeft);

    // Same attachment requirement as the Connector's Offset X/Y above.
    auto functionElement = std::static_pointer_cast<task_controller_object::DeviceElementObject>(ddop->get_object_by_id(kObjFunction));
    functionElement->add_reference_to_child_object(kObjDeviation);
    functionElement->add_reference_to_child_object(kObjQuality);
    functionElement->add_reference_to_child_object(kObjTramlineLevel);
    functionElement->add_reference_to_child_object(kObjTramlineSetpointLevel);
    functionElement->add_reference_to_child_object(kObjTramlineSequence);
    functionElement->add_reference_to_child_object(kObjTramlineState);
    functionElement->add_reference_to_child_object(kObjAbLineId);
    functionElement->add_reference_to_child_object(kObjActualTrack);
    functionElement->add_reference_to_child_object(kObjTrackRight);
    functionElement->add_reference_to_child_object(kObjTrackLeft);
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
    // Kept as a member (not a local) so IsPartnerAddressValid()/
    // GetPartnerAddress() can surface it -- see their comment in the header.
    partner = CANNetworkManager::CANNetwork.create_partnered_control_function(0, tcNameFilters);

    buildDdop();

    // Third argument is the *primary VT's* partnered control function, not a
    // VirtualTerminalClient. AgIsoStack uses it in
    // select_language_command_partner(): for a TC server older than version 4
    // it sources language/unit data from the VT, and only falls back to a
    // global request -- warning "no VT was provided ... might not be ideal" --
    // when this is null. Every TC met so far reports version 3, so this path
    // is always taken. See GitHub issue #21.
    tcClient = std::make_shared<TaskControllerClient>(partner, controlFunction, primaryVtPartner);

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
        updateReconnectWatchdog();
    }
}

// ------------------------------------------------------------------
// Reconnect watchdog -- see the header's reconnect fields. GitHub issue #18.
// ------------------------------------------------------------------
void IsobusTcInterface::updateReconnectWatchdog() {
    // Longer than the VT's equivalent: TaskControllerClient's own connect
    // sequence includes a six-second WaitForStartUpDelay, so anything
    // shorter risks cutting off a re-attempt that is already in progress.
    constexpr unsigned long kReconnectAfterMs         = 15000UL;
    constexpr unsigned long kReconnectRetryIntervalMs = 15000UL;

    const unsigned long now = millis();

    if (IsConnected()) {
        hasEverConnected    = true;
        disconnectedSinceMs = 0;
        return;
    }

    // Deliberately not armed before the first successful connection. A TC
    // that has never connected is the #19 case, whose cause was the partner
    // being evicted from AgIsoStack's control-function table -- restarting
    // the client would not have helped, and firing during the normal
    // six-second startup delay would interrupt a connection in progress.
    if (!hasEverConnected) return;

    if (disconnectedSinceMs == 0) {
        disconnectedSinceMs = now;
        return;
    }
    if (now - disconnectedSinceMs < kReconnectAfterMs) return;
    if (reconnectAttempts != 0 && (now - lastReconnectTryMs) < kReconnectRetryIntervalMs) return;

    lastReconnectTryMs = now;
    reconnectAttempts++;

    serialDebug->print("TC: disconnected ");
    serialDebug->print((now - disconnectedSinceMs) / 1000);
    serialDebug->print("s after having been connected -- forcing restart ");
    serialDebug->print(reconnectAttempts);
    serialDebug->print(" (partner addr=0x");
    serialDebug->print(GetPartnerAddress(), HEX);
    serialDebug->print(" valid=");
    serialDebug->print(IsPartnerAddressValid() ? "Y" : "N");
    serialDebug->println(")");

    tcClient->restart();
}

// ------------------------------------------------------------------
// Value command callback -- the TC writes to us here. This is where
// 513/514 arrive.
// ------------------------------------------------------------------
bool IsobusTcInterface::GenerateDdopBinary(std::vector<std::uint8_t>& out) {
    out.clear();
    if (ddop == nullptr) {
        return false;
    }
    return ddop->generate_binary_object_pool(out);
}

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

        case static_cast<std::uint16_t>(DataDescriptionIndex::SetpointTramlineControlLevel):  // 506
            // The Tramline Control probe's answer (GitHub issue #21). Latched
            // separately from lastValueCommandDdi, which any later DDI would
            // overwrite -- the whole point of the probe is that this arrived
            // AT ALL, so it must survive to be read off the debug menu later.
            // Value 0 ("no common level") counts as a positive result: it
            // still proves the terminal implements Tramline Control.
            self->tramlineSetpointLevel   = static_cast<int>(processVariableValue);
            self->tramlineSetpointSeen    = true;
            self->lastTramlineSetpointMs  = millis();
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::TramlineSequenceNumber):  // 507
            // Starts at 1 and increments per sequence, so 0 stays a usable
            // "never seen" marker.
            self->tramlineSequenceNumber = processVariableValue;
            self->guidanceTrackSeen      = true;
            self->lastGuidanceTrackMs    = millis();
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::TramlineControlState):  // 515
            // What the TC asked us to be. What we actually are is answered in
            // OnValueRequest, and it is not this.
            self->commandedTrackControlState = processVariableValue;
            self->trackControlStateSeen      = true;
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::UniqueABGuidanceReferenceLineID):  // 508
            self->abLineId            = processVariableValue;
            self->guidanceTrackSeen   = true;
            self->lastGuidanceTrackMs = millis();
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::ActualTrackNumber):  // 509
            self->actualTrackNumber   = processVariableValue;
            self->guidanceTrackSeen   = true;
            self->lastGuidanceTrackMs = millis();
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::TrackNumberToTheRight):  // 510
            self->trackNumberRight    = processVariableValue;
            self->guidanceTrackSeen   = true;
            self->lastGuidanceTrackMs = millis();
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::TrackNumberToTheLeft):  // 511
            self->trackNumberLeft     = processVariableValue;
            self->guidanceTrackSeen   = true;
            self->lastGuidanceTrackMs = millis();
            break;

        default:
            break;
    }

    // Return true unconditionally: a TC expects every "settable" DPD in the
    // DDOP to be writable. Returning false triggers TC-side error handling.
    return true;
}

// ------------------------------------------------------------------
// Value request callback -- the TC reads from us here.
//
// Two kinds of DDI arrive here now. For the Settable ones (506, 508-511,
// 513, 514) a request is the TC re-syncing its own idea of what it last
// commanded, e.g. after a reconnect, so echo back the last received value.
// For 507 and 515 we are the source: those are the two the Level 1 set
// expects the *implement* to answer, and a TC may well read them to decide
// whether we are a working tramline implement.
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

        case static_cast<std::uint16_t>(DataDescriptionIndex::TramlineSequenceNumber):  // 507
            processVariableValue = self->tramlineSequenceNumber;
            break;

        // Per DDI 160, whose definition 515 shares: the TC sets the state and
        // the client replies with its own. Ours is 0 = manual/off and will
        // stay there until something actually performs track control -- see
        // kTramlineControlLevelsSupported. Answering with the commanded value
        // instead would be a lie the TC has no way to detect.
        case static_cast<std::uint16_t>(DataDescriptionIndex::TramlineControlState):  // 515
            processVariableValue = kReportedTrackControlState;
            break;

        // Echoes of what the TC last wrote to us.
        case static_cast<std::uint16_t>(DataDescriptionIndex::SetpointTramlineControlLevel):  // 506
            processVariableValue = self->tramlineSetpointSeen ? self->tramlineSetpointLevel : 0;
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::UniqueABGuidanceReferenceLineID):
            processVariableValue = self->abLineId;
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::ActualTrackNumber):
            processVariableValue = self->actualTrackNumber;
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::TrackNumberToTheRight):
            processVariableValue = self->trackNumberRight;
            break;

        case static_cast<std::uint16_t>(DataDescriptionIndex::TrackNumberToTheLeft):
            processVariableValue = self->trackNumberLeft;
            break;

        default:
            processVariableValue = 0;
            break;
    }
    return true;
}

}  // namespace triton

#endif  // ISOBUS
