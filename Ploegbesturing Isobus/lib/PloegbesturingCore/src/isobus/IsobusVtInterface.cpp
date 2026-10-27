/*
  IsobusVtInterface - ISOBUS Virtual Terminal client and diagnostics for the plough controller
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
#include "IsobusVtInterface.hpp"

// The header is itself empty unless ISOBUS is defined (see its own comment) --
// guard the body too, so this compiles to an empty translation unit instead
// of failing on undeclared isobus:: symbols when PlatformIO's LDF pulls this
// file in on teensy41_serial anyway.
#ifdef ISOBUS

// Specific headers for what the body uses beyond the class declaration; see
// the header for why the umbrella is avoided and the macros parked.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/isobus/can_NAME_filter.hpp>
#include <isobus/isobus/can_network_manager.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "VTObjectPool.hpp"
#include "../PloughGates.hpp"

using namespace isobus;

namespace triton
{

// ----------------------------------------------------------------
// Logger
// ----------------------------------------------------------------
void IsobusVtInterface::Logger::sink_CAN_stack_log(CANStackLogger::LoggingLevel level, const std::string& text) {
    switch (level) {
        case LoggingLevel::Debug:    Serial.print('D'); break;
        case LoggingLevel::Info:     Serial.print('I'); break;
        case LoggingLevel::Warning:  Serial.print('W'); break;
        case LoggingLevel::Error:    Serial.print('E'); break;
        case LoggingLevel::Critical: Serial.print('!'); break;
    }
    Serial.print("] ");
    Serial.println(text.c_str());
}

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------
IsobusVtInterface::IsobusVtInterface(Stream* serialDebug, ImplementPlough* implement, GuidanceSource* guidance,
                                      std::shared_ptr<InternalControlFunction> controlFunction)
    : serialDebug(serialDebug), implement(implement), guidance(guidance), controlFunction(controlFunction) {
}

// ----------------------------------------------------------------
// Begin -- call once from setup(), after address claim
// ----------------------------------------------------------------
void IsobusVtInterface::Begin() {
    BuildObjectPool();
    LoadPreferredVt();

    CANStackLogger::set_can_stack_logger_sink(&logger);
    // Bumped Warning -> Info 2026-09-05: at Warning, AgIsoStack's own [NM]
    // control-function lifecycle lines (address claims, "is now offline")
    // are invisible on the serial console -- confirmed by re-reading Session
    // 5's captured logs, which contain zero [NM] lines despite a control-
    // function eviction being the leading theory for GitHub issue #17's VT
    // Status Timeout (see NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md patch #3
    // and HardwareTestNotes.md Session 5). Info is the cheapest way to see
    // these events directly instead of inferring them.
    CANStackLogger::set_log_level(CANStackLogger::LoggingLevel::Info);

    diagnostics = std::make_shared<DiagnosticProtocol>(controlFunction);
    diagnostics->initialize();
    diagnostics->set_product_identification_brand("MeijWorks");
    diagnostics->set_product_identification_model("Ploegbesturing Isobus");
    diagnostics->set_product_identification_code("001");
    diagnostics->set_software_id_field(0, "0.1.0");

    const NAMEFilter vtFilter(NAME::NAMEParameters::FunctionCode, static_cast<uint8_t>(NAME::Function::VirtualTerminal));
    const std::vector<NAMEFilter> vtNameFilters = { vtFilter };
    // Must go through the factory method, not a direct std::make_shared
    // construction -- same class of bug as IsobusGuidanceChannel::Begin()'s
    // create_internal_control_function() comment. Only
    // create_partnered_control_function() registers the object into
    // CANNetworkManager's partneredControlFunctions list, and only members
    // of that list are ever checked against an incoming Address Claim frame
    // (can_network_manager.cpp's update_control_functions()) -- so a directly
    // constructed partner can NEVER become address-valid, no matter how long
    // a real VT sits on the bus. Confirmed on hardware 2026-08-10: VT stuck
    // at state 0/22 "Disconnected" indefinitely against a live Fendt
    // Universal Terminal. AgIsoStack's own header (can_control_function.hpp)
    // and its VirtualTerminal.ino example both use the factory form.
    // Kept as a member (not a local) so IsPartnerAddressValid()/
    // GetPartnerAddress() can surface it -- see their comment in the header.
    createClient(vtNameFilters);
    failover.Start(millis());

    // See the header's GetVtStatusMessageCount()/GetVtStatusMessageAgeMs()
    // comment -- independent raw counter for the VT's own periodic status
    // broadcast, alongside (not replacing) vtClient's internal tracking.
    // The same listener feeds the failover policy, per sender.
    CANNetworkManager::CANNetwork.add_global_parameter_group_number_callback(
        static_cast<std::uint32_t>(CANLibParameterGroupNumber::VirtualTerminalToECU), OnVtToEcuMessage, this);
}

// ----------------------------------------------------------------
// The partner and the client on it. From Begin() with a function-code-only
// filter (the first VT on the bus), and from switchTo() with a filter pinned
// to one VT's exact NAME.
// ----------------------------------------------------------------
void IsobusVtInterface::createClient(const std::vector<NAMEFilter>& filters) {
    partner = CANNetworkManager::CANNetwork.create_partnered_control_function(0, filters);

    vtClient = std::make_shared<VirtualTerminalClient>(partner, controlFunction);
    // Bumped MW01 -> MW02, 2026-08-10 (van Mastwijk): the pool's structure
    // genuinely changed (WorkingSet gained a real child object, see
    // VTObjectPool.cpp's appendWorkingSet() comment for the confirmed bug
    // this fixes) -- same discipline as the TC DDOP's TC01->TC02 bump in
    // Session 3, so no terminal that cached a pool under "MW01" confuses it
    // with this one.
    // Bumped MW02 -> MW03, 2026-08-10 (desktop, same day): WorkingSet's
    // child changed from a placeholder OutputString to a real PictureGraphic
    // icon (Icon_Plough) -- see VTObjectPool.cpp's appendPictureGraphic()
    // call site and GitHub issue #14.
    // Bumped MW03 -> MW04, 2026-09-09 (session 9, Ag Leader): the pool had
    // changed twice under the MW03 label without a bump -- 1bb3295 added the
    // Calibrate soft key and 359f618 changed the pool again -- so terminals
    // holding an MW03 copy from before those commits kept serving it. Caught
    // on the rig: the Ag Leader was displaying a pool with no Calibrate
    // button while the firmware had had one for weeks.
    //
    // The label is the *only* thing a terminal compares. It caches by label
    // and skips the upload on a match ("VT Server has a matching label ...
    // upload will be skipped"), so an unbumped label after a pool change is
    // silent -- nothing errors, the terminal just shows an old screen. Bump
    // this on every change to VT3PoolData, exactly as the DDOP's TC0x label
    // is bumped on every DDOP change.
    // Bumped MW04 -> MW05 -> MW06, 2026-10-01: 12x16 font and autoscaling
    // (below), then AgIsoStack's soft-key scaling fix (neptune-main a9453ff).
    // MW06 -> MW07, same day: the plough pictures and centred key labels.
    // MW07 -> MW08, same day: GPS and speed indicators, speed in km/h.
    // MW08 -> MW09, 2026-10-20: the calibration page (#240).
    // MW09 -> MW10, 2026-10-24: the Ag Leader lightbar setting, 9 rows (#272).
    // MW10 -> MW11, 2026-10-27: AUX-N functions Breder and Smaller (#254).
    // MW11 -> MW12, 2026-10-27: readable list popups, 12x16 units, every main
    // screen row in cm (#282).
    // MW12 -> MW13, same day: soft keys by position, large + and -, no AUTO
    // key (#282; MW12 was only ever on the bench VT).
    // ETP bursts as long as the receiver's CTS allows (#269). AgIsoStack's
    // default sends 16 packets per DPO whatever the CTS says; the John Deere
    // 4640 (Mookhoek 57, session 21) then rebuilt our pool wrongly and
    // answered End of Object Pool "other error" with no object, on every pool
    // too big for TP. Still bounded by each receiver's own CTS window.
    CANNetworkManager::CANNetwork.get_configuration().set_number_of_packets_per_dpo_message(255);
    vtClient->set_object_pool(0, VT3PoolData, VT3PoolSize, "MW13");
    // Scale the pool to each terminal's real screen. It is drawn for the VT3
    // minimum, a 200 px data mask, and 60 px soft keys (VTObjectPool.cpp's
    // layout); unscaled, the InCommand 1200 showed it small and squeezed
    // (session 13 repeat, 2026-10-01). AgIsoStack reads the terminal's data
    // mask and soft key sizes during the handshake and resizes positions,
    // sizes and fonts before the upload. Each terminal caches its own scaled
    // copy under the label, so one label still serves every screen size --
    // and so a change in how the pool SCALES (these numbers, or AgIsoStack's
    // scaling code) needs a label bump just like a change to the pool itself:
    // the bench VT kept showing MW05 scaled by the pre-fix library.
    vtClient->set_object_pool_scaling(0, 200, 60);
    softKeyListener = vtClient->get_vt_soft_key_event_dispatcher().add_listener(
        [this](const VirtualTerminalClient::VTKeyEvent& e) { onVtKeyEvent(e); });
    buttonListener = vtClient->get_vt_button_event_dispatcher().add_listener(
        [this](const VirtualTerminalClient::VTKeyEvent& e) { onVtKeyEvent(e); });
    numericValueListener = vtClient->get_vt_change_numeric_value_event_dispatcher().add_listener(
        [this](const VirtualTerminalClient::VTChangeNumericValueEvent& e) {
            if (calibration != nullptr) calibration->OnNumericValue(e.objectID, e.value);
        });
    auxFunctionListener = vtClient->get_auxiliary_function_event_dispatcher().add_listener(
        [this](const VirtualTerminalClient::AuxiliaryFunctionEvent& e) { onAuxFunctionEvent(e); });
    vtClient->initialize(false);
}

// ----------------------------------------------------------------
// Update -- call every loop() iteration
// ----------------------------------------------------------------
void IsobusVtInterface::Update() {
    if (millis() - lastVtUpdate >= 100) {
        lastVtUpdate = millis();
        updateVtVariables();
    }

    diagnostics->update();
    vtClient->update();
    if (calibration != nullptr) {
        // The page lives on this VT only: a lost connection drops its draft.
        if (IsConnected()) calibration->Update(*this);
        else if (calibration->IsOpen()) calibration->OnConnectionLost();
    }
    updateReconnectWatchdog();
    updateFailover();
}

static_assert(IsobusVtCalibration::kHeldTimeoutMs == IsobusVtInterface::kSoftKeyHeldTimeoutMs,
              "the width procedure's held keys time out like the main soft keys");

bool IsobusVtInterface::ChangeActiveMask(uint16_t dataMask) {
    return vtClient && vtClient->send_change_active_mask(Plough_WorkingSet, dataMask);
}

bool IsobusVtInterface::ChangeNumericValue(uint16_t objectId, uint32_t value) {
    return vtClient && vtClient->send_change_numeric_value(objectId, value);
}

// ----------------------------------------------------------------
// VT failover -- see VtFailoverPolicy.hpp for the field case and why a
// request for address claim comes first.
// ----------------------------------------------------------------
// Bus load above this counts as traffic from other CFs (see updateFailover()).
static constexpr float kOthersTrafficBusloadPercent = 1.0f;

void IsobusVtInterface::updateFailover() {
    const unsigned long now = millis();
    // Traffic from the others, for the policy's rejoin grace. Alone on the bus
    // (session 18: unplugged, or on USB only) the estimate sits at 0.0-0.2 %
    // from our own unacknowledged frames; a live ISOBUS is 12-19 %. A quiet
    // bus below 1 % is still covered: every VT status counts as well
    // (OnVtToEcuMessage). After a loop stall this is the first call again, so
    // the stall shows up as a gap too.
    if (CANNetworkManager::CANNetwork.get_estimated_busload(0) > kOthersTrafficBusloadPercent) {
        failover.OnBusTraffic(now);
    }
    const VtFailoverPolicy::Decision decision =
        failover.Evaluate(now, IsConnected(), GetPartnerAddress(), IsPartnerAddressValid());

    switch (decision.action) {
        case VtFailoverPolicy::Action::None:
            break;

        case VtFailoverPolicy::Action::RequestAddressClaims:
            // Global, so every CF on the bus answers, and each answer puts
            // its sender back in AgIsoStack's address table -- the only way
            // a pruned VT's status ever reaches OnVtToEcuMessage again.
            // Sent from the NULL address (0xFE), which ISO 11783-5 allows for
            // this request (#199): AgIsoStack feeds a transmitted request for
            // the claim PGN back into its receive path, and from our own SA
            // that tripped its address-violation check -- a logged violation
            // and an extra defending claim per request. The stack still
            // answers the request with our own claim, as it should.
            serialDebug->print("VT: partner 0x");
            serialDebug->print(GetPartnerAddress(), HEX);
            serialDebug->println(" silent -- requesting address claims to find another VT");
            CANNetworkManager::CANNetwork.send_request_for_address_claim(controlFunction->get_can_port());
            failover.OnRequestSent(now);
            break;

        case VtFailoverPolicy::Action::SwitchTo:
            switchTo(decision.address);
            break;
    }
}

void IsobusVtInterface::switchTo(std::uint8_t address) {
    const std::shared_ptr<ControlFunction> target = CANNetworkManager::CANNetwork.get_control_function(0, address);
    if (!target) {
        // Its status reached us, so it was in the table a moment ago. Wait
        // for the next status rather than bind to nothing.
        return;
    }
    const NAME name = target->get_NAME();

    if (address == failover.GetPreferredAddress()) {
        serialDebug->print("VT: preferred VT ");
        serialDebug->print(preferredVt);
        serialDebug->print(" up at 0x");
        serialDebug->print(address, HEX);
        serialDebug->println(" -- moving to it");
    }
    else {
        serialDebug->print("VT: partner 0x");
        serialDebug->print(GetPartnerAddress(), HEX);
        serialDebug->print(" silent, VT at 0x");
        serialDebug->print(address, HEX);
        serialDebug->println(" alive -- switching to it");
    }

    // Pinned to that one VT, not just "a VT": with a function-code-only
    // filter the new partner would match the first VT in the table, which is
    // the silent one we are leaving whenever it has the lower address.
    const std::vector<NAMEFilter> filters = {
        NAMEFilter(NAME::NAMEParameters::FunctionCode, static_cast<uint8_t>(NAME::Function::VirtualTerminal)),
        NAMEFilter(NAME::NAMEParameters::IdentityNumber, name.get_identity_number()),
        NAMEFilter(NAME::NAMEParameters::ManufacturerCode, name.get_manufacturer_code()),
        NAMEFilter(NAME::NAMEParameters::FunctionInstance, name.get_function_instance()),
    };

    // terminate() drops the old client's PGN callbacks; deactivating the old
    // partner takes it out of the partner list, and the network manager puts
    // an ordinary external CF in its table slot, which the roll-call prune
    // may evict again like any other.
    // An open calibration page belongs to the VT being left (#240).
    if (calibration != nullptr) calibration->OnConnectionLost();
    vtClient->terminate();
    CANNetworkManager::CANNetwork.deactivate_control_function(partner);
    createClient(filters);
    failover.OnSwitched(millis());

    // A new VT means a fresh handshake, possibly with a full pool upload.
    // Disarm the reconnect watchdog until it connects, exactly as at boot,
    // so the watchdog cannot cut an upload short.
    hasEverConnected    = false;
    disconnectedSinceMs = 0;
    reconnectAttempts   = 0;
    sentInitialVtVariables = false;
}

// ----------------------------------------------------------------
// Reconnect watchdog -- see the header's reconnect fields for the rationale
// and for why initialize() alone is not enough. GitHub issue #18.
// ----------------------------------------------------------------
void IsobusVtInterface::updateReconnectWatchdog() {
    // How long a post-connection outage is tolerated before forcing a clean
    // re-attempt. Comfortably longer than AgIsoStack's own 3 s
    // VT_STATUS_TIMEOUT_MS plus a normal automatic re-handshake, so a
    // connection that is recovering on its own is never interrupted.
    constexpr unsigned long kReconnectAfterMs = 10000UL;
    // Minimum spacing between attempts, so a partner that is genuinely gone
    // produces one line every 10 s rather than a churning callback list.
    constexpr unsigned long kReconnectRetryIntervalMs = 10000UL;

    const unsigned long now = millis();

    if (IsConnected()) {
        hasEverConnected    = true;
        disconnectedSinceMs = 0;
        return;
    }

    // Never armed before the first successful connection: the initial
    // handshake has its own timing, and a watchdog firing during it would
    // interrupt a connection that was progressing normally.
    if (!hasEverConnected) return;

    if (disconnectedSinceMs == 0) {
        disconnectedSinceMs = now;
        return;
    }
    if (now - disconnectedSinceMs < kReconnectAfterMs) return;
    if (reconnectAttempts != 0 && (now - lastReconnectTryMs) < kReconnectRetryIntervalMs) return;

    lastReconnectTryMs = now;
    reconnectAttempts++;

    serialDebug->print("VT: disconnected ");
    serialDebug->print((now - disconnectedSinceMs) / 1000);
    serialDebug->print("s after having been connected -- forcing reconnect attempt ");
    serialDebug->print(reconnectAttempts);
    serialDebug->print(" (partner addr=0x");
    serialDebug->print(GetPartnerAddress(), HEX);
    serialDebug->print(" valid=");
    serialDebug->print(IsPartnerAddressValid() ? "Y" : "N");
    serialDebug->println(")");

    // terminate() then initialize(): see the header. terminate()'s
    // delete-object-pool branch only runs when Connected, which we are not,
    // so this is just a callback teardown and a state-machine reset.
    vtClient->terminate();
    vtClient->initialize(false);
}

// ----------------------------------------------------------------
// Private helpers
// ----------------------------------------------------------------
void IsobusVtInterface::updateVtVariables() {
    if (!vtClient->get_is_connected()) return;

    const int32_t position = static_cast<int32_t>(implement->GetPosition());
    const int32_t setpoint = static_cast<int32_t>(implement->GetSetpoint());
    // XTE is signed (cm); bias by +1000 so the uint32 variable stays
    // non-negative. That bias silently assumes +/-10 m, so enforce it rather
    // than implying it: an out-of-range value used to wrap through the
    // uint32_t cast below and render as a plausible-looking huge number.
    // Session 6 (2026-09-05) saw exactly that -- the VT displayed
    // 42949532.47, which decodes as (uint32)(-14049), i.e. a GetXte() of
    // -15049 from the then-broken Ag Leader decode (GitHub issue #20).
    // Clamping makes a bad reading peg visibly at the limit instead.
    constexpr int32_t kXteBias      = 1000;   // cm, = 10 m
    constexpr int32_t kXteBiasedMin = 0;      // -10 m or worse
    constexpr int32_t kXteBiasedMax = 2000;   // +10 m or worse
    int32_t xteBiased = static_cast<int32_t>(guidance->GetXte()) + kXteBias;
    if (xteBiased < kXteBiasedMin) xteBiased = kXteBiasedMin;
    if (xteBiased > kXteBiasedMax) xteBiased = kXteBiasedMax;
    const int32_t xte = xteBiased;
    const int32_t offset   = static_cast<int32_t>(implement->GetOffset());

    // See the header's comment on these fields for why this is on-change +
    // heartbeat rather than unconditional every-100ms.
    const bool heartbeatDue = (millis() - lastVtVariableHeartbeat >= 1000);
    const bool forceSend = !sentInitialVtVariables || heartbeatDue;

    if (forceSend || position != lastSentPosition) {
        vtClient->send_change_numeric_value(Var_Position, static_cast<uint32_t>(position));
        lastSentPosition = position;
    }
    if (forceSend || setpoint != lastSentSetpoint) {
        vtClient->send_change_numeric_value(Var_Setpoint, static_cast<uint32_t>(setpoint));
        lastSentSetpoint = setpoint;
    }
    if (forceSend || xte != lastSentXte) {
        vtClient->send_change_numeric_value(Var_XTE, static_cast<uint32_t>(xte));
        lastSentXte = xte;
    }
    if (forceSend || offset != lastSentOffset) {
        vtClient->send_change_numeric_value(Var_Offset, static_cast<uint32_t>(offset));
        lastSentOffset = offset;
    }
    // The picture follows the ploughing side: GetSide() is true for left
    // (the reversible plough's turn sensor, with the configured swap applied).
    const uint16_t ploughImage = implement->GetSide() ? Img_PloughLeft : Img_PloughRight;
    if (forceSend || ploughImage != lastSentPloughImage) {
        vtClient->send_change_numeric_value(Ptr_PloughImage, ploughImage);
        lastSentPloughImage = ploughImage;
    }
    // GPS and speed indicators: the same gates the control holds on
    // (PloughGates.hpp), so a green dot means that gate is met.
    const uint16_t gpsIcon = GpsReadyToSteer(*guidance, millis()) ? Img_StatusOk : Img_StatusWarn;
    if (forceSend || gpsIcon != lastSentGpsIcon) {
        vtClient->send_change_numeric_value(Ptr_GpsStatus, gpsIcon);
        lastSentGpsIcon = gpsIcon;
    }
    const uint16_t speedIcon = SpeedReadyToSteer(*guidance) ? Img_StatusOk : Img_StatusWarn;
    if (forceSend || speedIcon != lastSentSpeedIcon) {
        vtClient->send_change_numeric_value(Ptr_SpeedStatus, speedIcon);
        lastSentSpeedIcon = speedIcon;
    }
    // Speed in 0.1 km/h (the OutputNumber scales by 0.1).
    float speedDeciKmh = guidance->GetSpeedMs() * 36.0f;
    if (speedDeciKmh < 0.0f) speedDeciKmh = 0.0f;
    const int32_t speed = static_cast<int32_t>(speedDeciKmh + 0.5f);
    if (forceSend || speed != lastSentSpeed) {
        vtClient->send_change_numeric_value(Var_Speed, static_cast<uint32_t>(speed));
        lastSentSpeed = speed;
    }

    sentInitialVtVariables = true;
    if (heartbeatDue) lastVtVariableHeartbeat = millis();
}

// ----------------------------------------------------------------
// State-machine step name/index -- see the header comment and
// NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md. Order matches
// isobus::VirtualTerminalClient::StateMachineState exactly (0-based);
// keep in sync if that enum changes.
// ----------------------------------------------------------------
namespace {
constexpr const char* kVtStateNames[] = {
    "Disconnected",
    "WaitForPartnerVTStatusMessage",
    "SendWorkingSetMasterMessage",
    "ReadyForObjectPool",
    "SendGetMemory",
    "WaitForGetMemoryResponse",
    "SendGetNumberSoftkeys",
    "WaitForGetNumberSoftKeysResponse",
    "SendGetTextFontData",
    "WaitForGetTextFontDataResponse",
    "SendGetHardware",
    "WaitForGetHardwareResponse",
    "SendGetVersions",
    "WaitForGetVersionsResponse",
    "SendStoreVersion",
    "WaitForStoreVersionResponse",
    "SendLoadVersion",
    "WaitForLoadVersionResponse",
    "UploadObjectPool",
    "SendEndOfObjectPool",
    "WaitForEndOfObjectPoolResponse",
    "Connected",
    "Failed",
};
constexpr int kVtStateCount = sizeof(kVtStateNames) / sizeof(kVtStateNames[0]);
}  // namespace

int IsobusVtInterface::GetStateStep() const {
    if (!vtClient) return 0;
    return static_cast<int>(vtClient->get_state());
}

int IsobusVtInterface::GetStateTotalSteps() const {
    return kVtStateCount - 1;  // Failed isn't a forward step, exclude it from "of N"
}

const char* IsobusVtInterface::GetStateName() const {
    if (!vtClient) return "(no client)";
    int index = static_cast<int>(vtClient->get_state());
    if (index < 0 || index >= kVtStateCount) return "(unknown)";
    return kVtStateNames[index];
}

const char* IsobusVtInterface::GetVtVersionName() const {
    if (!vtClient) return "(no client)";
    switch (vtClient->get_connected_vt_version()) {
        case VirtualTerminalClient::VTVersion::Version2OrOlder:   return "<=2";
        case VirtualTerminalClient::VTVersion::Version3:          return "3";
        case VirtualTerminalClient::VTVersion::Version4:          return "4";
        case VirtualTerminalClient::VTVersion::Version5:          return "5";
        case VirtualTerminalClient::VTVersion::Version6:          return "6";
        default:                                                  return "(unknown)";
    }
}

void IsobusVtInterface::OnVtToEcuMessage(const CANMessage& message, void* parentPointer) {
    if (parentPointer == nullptr || message.get_data_length() < 1) return;
    IsobusVtInterface* self = static_cast<IsobusVtInterface*>(parentPointer);

    // An AUX input unit's status (#254), assigned to us or not: only its
    // learn-mode bit is read here; AgIsoStack dispatches the values.
    if (message.get_uint8_at(0) == static_cast<std::uint8_t>(VirtualTerminalClient::Function::AuxiliaryInputTypeTwoStatusMessage)) {
        if (message.get_data_length() >= 8) self->aux.OnInputStatus(message.get_uint8_at(7), millis());
        return;
    }
    if (message.get_uint8_at(0) != static_cast<std::uint8_t>(VirtualTerminalClient::Function::VTStatusMessage)) return;

    self->vtStatusMessageCount++;
    self->lastVtStatusMessageMs = millis();
    // Every VT's status, not only our partner's: the policy needs to know
    // which other VT is alive. Traffic first, so the first status after a
    // rejoin starts the grace before it can make its sender a candidate.
    self->failover.OnBusTraffic(self->lastVtStatusMessageMs);
    self->failover.OnVtStatus(message.get_identifier().get_source_address(), self->lastVtStatusMessageMs);
    self->notePreferredCandidate(message.get_identifier().get_source_address());
}

void IsobusVtInterface::LoadPreferredVt() {
    preferredVt = ReadPreferredVt();
    // Found again by function instance from the next VT status on.
    failover.SetPreferredAddress(VtFailoverPolicy::kNoAddress);
}

// Called for every VT status: is its sender the preferred VT (#255)? A table
// lookup per status, at 1 Hz per VT, instead of scanning every loop.
void IsobusVtInterface::notePreferredCandidate(std::uint8_t source) {
    if (preferredVt == 0) return;
    const std::shared_ptr<ControlFunction> cf = CANNetworkManager::CANNetwork.get_control_function(0, source);
    if (!cf) return;
    const NAME name = cf->get_NAME();
    const bool isVt = name.get_function_code() == static_cast<std::uint8_t>(NAME::Function::VirtualTerminal);
    if (IsPreferredVt(isVt, name.get_function_instance(), preferredVt)) {
        failover.SetPreferredAddress(source);
    }
    else if (failover.GetPreferredAddress() == source) {
        failover.SetPreferredAddress(VtFailoverPolicy::kNoAddress);  // that address is another VT's now
    }
}

bool IsobusVtInterface::IsWiderHeld() const {
    return (widerHeld && millis() - widerEventMs <= kSoftKeyHeldTimeoutMs) || aux.IsWiderHeld(millis());
}

bool IsobusVtInterface::IsNarrowerHeld() const {
    return (narrowerHeld && millis() - narrowerEventMs <= kSoftKeyHeldTimeoutMs) || aux.IsNarrowerHeld(millis());
}

// A status of an input assigned to one of our AUX functions (#254). Logged on
// press and release, as the soft keys are.
void IsobusVtInterface::onAuxFunctionEvent(const VirtualTerminalClient::AuxiliaryFunctionEvent& event) {
    const std::uint16_t function = event.function.functionObjectID;
    if (function != Aux_Wider && function != Aux_Narrower) return;
    const bool wider = function == Aux_Wider;
    const unsigned long now = millis();
    const bool wasHeld = wider ? aux.IsWiderHeld(now) : aux.IsNarrowerHeld(now);
    aux.OnFunctionValue(wider, event.value1, now);
    (wider ? auxWiderEvents : auxNarrowerEvents)++;
    const bool isHeld = wider ? aux.IsWiderHeld(now) : aux.IsNarrowerHeld(now);
    if (isHeld != wasHeld) {
        serialDebug->print(wider ? "VT: AUX Breder " : "VT: AUX Smaller ");
        serialDebug->print(isHeld ? "pressed" : "released");
        serialDebug->print(" (input ");
        serialDebug->print(event.function.inputObjectID);
        serialDebug->print(", value ");
        serialDebug->print(event.value1);
        serialDebug->println(aux.InLearnMode(now) ? ", learn mode)" : ")");
    }
}

void IsobusVtInterface::onVtKeyEvent(const VirtualTerminalClient::VTKeyEvent& event) {
    using Code = VirtualTerminalClient::KeyActivationCode;

    // Wider/Narrower: held from press (or a still-held repeat) until release
    // or an aborted press, see IsWiderHeld() (#245).
    if (event.objectID == Key_Wider || event.objectID == Key_Narrower) {
        const bool wider = event.objectID == Key_Wider;
        bool& held = wider ? widerHeld : narrowerHeld;
        const bool down = event.keyEvent == Code::ButtonPressedOrLatched || event.keyEvent == Code::ButtonStillHeld;
        if (event.keyEvent == Code::ButtonPressedOrLatched) {
            serialDebug->println(wider ? "VT: Wider pressed" : "VT: Narrower pressed");
        }
        else if (!down && held) {
            serialDebug->println(wider ? "VT: Wider released" : "VT: Narrower released");
        }
        held = down;
        (wider ? widerEventMs : narrowerEventMs) = millis();
        return;
    }

    // The width procedure's Smaller / Breder (#240): held levels too, kept
    // by the calibration page.
    if (event.objectID == Key_CalWider || event.objectID == Key_CalNarrower) {
        if (calibration != nullptr) {
            calibration->OnWidthKey(event.objectID, event.keyEvent == Code::ButtonPressedOrLatched ||
                                                        event.keyEvent == Code::ButtonStillHeld);
        }
        return;
    }

    if (event.keyEvent != Code::ButtonUnlatchedOrReleased) return;

    // Calibrate opens the calibration page (#240), Opslaan and Annuleren
    // close it; see IsobusVtCalibration. It is a single tap, where the
    // cab's equivalent is both buttons held for about a second, but nothing
    // changes before Opslaan, so a stray tap does no harm (#31 point 3).
    switch (event.objectID) {
        case Key_Calibrate:
            serialDebug->println("VT: Calibrate pressed");
            if (calibration != nullptr) calibration->OnCalibrateKey();
            break;
        case Key_CalSave:
        case Key_CalCancel:
        case Key_CalWidth:
        case Key_CalCapture:
        case Key_CalWidthBack:
            // A saved page may have changed the preferred VT (#255).
            if (calibration != nullptr && calibration->OnSoftKey(event.objectID)) LoadPreferredVt();
            break;
        default:
            break;
    }
}

}  // namespace triton

#endif  // ISOBUS
