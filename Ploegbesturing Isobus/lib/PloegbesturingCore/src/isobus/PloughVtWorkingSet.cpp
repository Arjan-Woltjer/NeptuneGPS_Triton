/*
  PloughVtWorkingSet - the plough's working screen, keys and calibration page on the VT
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
#include "PloughVtWorkingSet.hpp"

// The header is itself empty unless ISOBUS is defined -- guard the body too,
// so this compiles to an empty translation unit when PlatformIO's LDF pulls
// it in on a serial build anyway.
#ifdef ISOBUS

#include "VTObjectPool.hpp"
#include "../PloughGates.hpp"

using namespace isobus;

namespace triton
{

static_assert(IsobusVtCalibration::kHeldTimeoutMs == PloughVtWorkingSet::kSoftKeyHeldTimeoutMs,
              "the width procedure's held keys time out like the main soft keys");

// ----------------------------------------------------------------
// The object pool (VTObjectPool.cpp)
// ----------------------------------------------------------------
void PloughVtWorkingSet::BuildPool() {
    BuildObjectPool();
}

const uint8_t* PloughVtWorkingSet::GetPoolData() const {
    return VT3PoolData;
}

uint32_t PloughVtWorkingSet::GetPoolSize() const {
    return VT3PoolSize;
}

const char* PloughVtWorkingSet::GetPoolLabel() const {
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
    // (IsobusVtInterface's set_object_pool_scaling()), then AgIsoStack's
    // soft-key scaling fix (neptune-main a9453ff).
    // MW06 -> MW07, same day: the plough pictures and centred key labels.
    // MW07 -> MW08, same day: GPS and speed indicators, speed in km/h.
    // MW08 -> MW09, 2026-10-20: the calibration page (#240).
    // MW09 -> MW10, 2026-10-24: the Ag Leader lightbar setting, 9 rows (#272).
    // MW10 -> MW11, 2026-10-27: AUX-N functions Breder and Smaller (#254).
    // MW11 -> MW12, 2026-10-27: readable list popups, 12x16 units, every main
    // screen row in cm (#282).
    // MW12 -> MW13, same day: soft keys by position, large + and -, no AUTO
    // key (#282; MW12 was only ever on the bench VT).
    // MW13 -> MW14, same day: the key positions follow the bench VT's fill
    // order, column 2 top to bottom, then column 1 (#282; MW13 bench only).
    // MW14 -> MW15, same day: CALIBR, BREEDTE, OPSLAAN and ANNUL. in column 2;
    // the width screen's VASTLEG and TERUG read OPSLAAN and ANNUL. (#282).
    // MW15 -> MW16, same day: + above - in rows 1 and 2, as the cab buttons,
    // so every key fits a 1 x 6 VT (#282).
    // MW16 -> MW17, 2026-11-07: the control's state on the main screen
    // (#318), and both ANNUL. keys on row 5 for the 1200's 2 x 5 keys
    // (session 23).
    return "MW17";
}

uint16_t PloughVtWorkingSet::StateString(uint8_t mode, bool calibrating, HoldReason hold) {
    if (calibrating) return Str_StateCal;
    switch (mode) {
        case 0:  return Str_StateAuto;
        case 1:  return hold == HoldReason::Speed ? Str_StateHoldSpeed : Str_StateHoldGps;
        default: return Str_StateHand;
    }
}

// ----------------------------------------------------------------
// The connection's events
// ----------------------------------------------------------------
void PloughVtWorkingSet::OnClientCreated(std::shared_ptr<VirtualTerminalClient> newClient) {
    client = newClient;
    // A new VT means a fresh handshake: send every value again once it
    // connects.
    sentInitialVtVariables = false;
}

void PloughVtWorkingSet::OnConnectionLost() {
    // An open calibration page belongs to the VT being left (#240).
    if (calibration != nullptr) calibration->OnConnectionLost();
    DropKeys();
}

// A VT that goes away takes its keys with it: nothing held on it survives the
// connection, so only a new press after the reconnect drives a valve.
void PloughVtWorkingSet::DropKeys() {
    widerKey.Drop();
    narrowerKey.Drop();
    pressedTapKey = kNoTapKey;
    aux.Drop();
}

void PloughVtWorkingSet::Update(bool connected) {
    if (!connected) DropKeys();
    if (calibration != nullptr) {
        // The page lives on this VT only: a lost connection drops its draft.
        if (connected) calibration->Update(*this);
        else if (calibration->IsOpen()) calibration->OnConnectionLost();
    }
}

bool PloughVtWorkingSet::ChangeActiveMask(uint16_t dataMask) {
    return client && client->send_change_active_mask(Plough_WorkingSet, dataMask);
}

bool PloughVtWorkingSet::ChangeNumericValue(uint16_t objectId, uint32_t value) {
    return client && client->send_change_numeric_value(objectId, value);
}

// ----------------------------------------------------------------
// The working screen's values
// ----------------------------------------------------------------
void PloughVtWorkingSet::UpdateValues() {
    if (!client || !client->get_is_connected()) return;

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
        client->send_change_numeric_value(Var_Position, static_cast<uint32_t>(position));
        lastSentPosition = position;
    }
    if (forceSend || setpoint != lastSentSetpoint) {
        client->send_change_numeric_value(Var_Setpoint, static_cast<uint32_t>(setpoint));
        lastSentSetpoint = setpoint;
    }
    if (forceSend || xte != lastSentXte) {
        client->send_change_numeric_value(Var_XTE, static_cast<uint32_t>(xte));
        lastSentXte = xte;
    }
    if (forceSend || offset != lastSentOffset) {
        client->send_change_numeric_value(Var_Offset, static_cast<uint32_t>(offset));
        lastSentOffset = offset;
    }
    // The picture follows the ploughing side: GetSide() is true for left
    // (the reversible plough's turn sensor, with the configured swap applied).
    const uint16_t ploughImage = implement->GetSide() ? Img_PloughLeft : Img_PloughRight;
    if (forceSend || ploughImage != lastSentPloughImage) {
        client->send_change_numeric_value(Ptr_PloughImage, ploughImage);
        lastSentPloughImage = ploughImage;
    }
    // GPS and speed indicators: the same gates the control holds on
    // (PloughGates.hpp), so a green dot means that gate is met.
    const uint16_t gpsIcon = GpsReadyToSteer(*guidance, millis()) ? Img_StatusOk : Img_StatusWarn;
    if (forceSend || gpsIcon != lastSentGpsIcon) {
        client->send_change_numeric_value(Ptr_GpsStatus, gpsIcon);
        lastSentGpsIcon = gpsIcon;
    }
    const uint16_t speedIcon = SpeedReadyToSteer(*guidance) ? Img_StatusOk : Img_StatusWarn;
    if (forceSend || speedIcon != lastSentSpeedIcon) {
        client->send_change_numeric_value(Ptr_SpeedStatus, speedIcon);
        lastSentSpeedIcon = speedIcon;
    }
    // The control's state (#318), with the hold reason the LCD shows.
    const uint16_t state = StateString(controlMode, controlCalibrating, PloughHoldReason(*guidance, millis()));
    if (forceSend || state != lastSentState) {
        client->send_change_numeric_value(Ptr_State, state);
        lastSentState = state;
    }
    // Speed in 0.1 km/h (the OutputNumber scales by 0.1).
    float speedDeciKmh = guidance->GetSpeedMs() * 36.0f;
    if (speedDeciKmh < 0.0f) speedDeciKmh = 0.0f;
    const int32_t speed = static_cast<int32_t>(speedDeciKmh + 0.5f);
    if (forceSend || speed != lastSentSpeed) {
        client->send_change_numeric_value(Var_Speed, static_cast<uint32_t>(speed));
        lastSentSpeed = speed;
    }

    sentInitialVtVariables = true;
    if (heartbeatDue) lastVtVariableHeartbeat = millis();
}

// ----------------------------------------------------------------
// Keys, values and AUX functions
// ----------------------------------------------------------------
bool PloughVtWorkingSet::IsWiderHeld() const {
    return widerKey.IsHeld(millis()) || aux.IsWiderHeld(millis());
}

bool PloughVtWorkingSet::IsNarrowerHeld() const {
    return narrowerKey.IsHeld(millis()) || aux.IsNarrowerHeld(millis());
}

void PloughVtWorkingSet::OnNumericValue(const VirtualTerminalClient::VTChangeNumericValueEvent& event) {
    if (calibration != nullptr) calibration->OnNumericValue(event.objectID, event.value);
}

void PloughVtWorkingSet::OnAuxInputStatus(uint8_t operatingState) {
    aux.OnInputStatus(operatingState, millis());
}

// A status of an input assigned to one of our AUX functions (#254). Logged on
// press and release, as the soft keys are.
void PloughVtWorkingSet::OnAuxFunction(const VirtualTerminalClient::AuxiliaryFunctionEvent& event) {
    const std::uint16_t function = event.function.functionObjectID;
    if (function != Aux_Wider && function != Aux_Narrower) return;
    const bool wider = function == Aux_Wider;
    const unsigned long now = millis();
    const bool wasHeld = wider ? aux.IsWiderHeld(now) : aux.IsNarrowerHeld(now);
    const HeldKey::Change change = aux.OnFunctionValue(wider, event.value1, now);
    (wider ? auxWiderEvents : auxNarrowerEvents)++;
    if (change == HeldKey::Change::Ignored) {
        serialDebug->println(wider ? "VT: AUX Breder held without a press -- ignored"
                                   : "VT: AUX Smaller held without a press -- ignored");
    }
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

// A VT key event as a HeldKey event: a press, a still-held repeat, or
// anything else (release, aborted press) that ends the hold.
static HeldKey::Event ToHeldKeyEvent(VirtualTerminalClient::KeyActivationCode code) {
    using Code = VirtualTerminalClient::KeyActivationCode;
    if (code == Code::ButtonPressedOrLatched) return HeldKey::Event::Press;
    if (code == Code::ButtonStillHeld) return HeldKey::Event::StillHeld;
    return HeldKey::Event::Release;
}

bool PloughVtWorkingSet::OnKeyEvent(const VirtualTerminalClient::VTKeyEvent& event) {
    using Code = VirtualTerminalClient::KeyActivationCode;

    // Wider/Narrower: held from a press until release, an aborted press, the
    // timeout or a lost connection, see IsWiderHeld() and HeldKey (#245,
    // #320). A still-held repeat only keeps a live hold alive; it never starts
    // one. Session 24: after a cable pull AgISOVirtualTerminal kept repeating
    // still-held for a release it never got through, and the old rule (a
    // repeat counts as a press) drove the valve for 30 s after the reconnect.
    if (event.objectID == Key_Wider || event.objectID == Key_Narrower) {
        const bool wider = event.objectID == Key_Wider;
        switch ((wider ? widerKey : narrowerKey).On(ToHeldKeyEvent(event.keyEvent), millis())) {
            case HeldKey::Change::Pressed:
                serialDebug->println(wider ? "VT: Wider pressed" : "VT: Narrower pressed");
                break;
            case HeldKey::Change::Released:
                serialDebug->println(wider ? "VT: Wider released" : "VT: Narrower released");
                break;
            case HeldKey::Change::Ignored:
                serialDebug->println(wider ? "VT: Wider still held without a press -- ignored"
                                           : "VT: Narrower still held without a press -- ignored");
                break;
            case HeldKey::Change::None:
                break;
        }
        return false;
    }

    // The width procedure's Smaller / Breder (#240): held levels by the same
    // rule, kept by the calibration page.
    if (event.objectID == Key_CalWider || event.objectID == Key_CalNarrower) {
        if (calibration != nullptr) calibration->OnWidthKey(event.objectID, ToHeldKeyEvent(event.keyEvent));
        return false;
    }

    // Every other key is a tap that acts on its release -- but only on the
    // release of the press this connection saw (#320): a release left over
    // from before a lost connection, or whose press went elsewhere, does
    // nothing.
    if (event.keyEvent == Code::ButtonPressedOrLatched) {
        pressedTapKey = event.objectID;
        return false;
    }
    if (event.keyEvent == Code::ButtonPressAborted) {
        if (pressedTapKey == event.objectID) pressedTapKey = kNoTapKey;
        return false;
    }
    if (event.keyEvent != Code::ButtonUnlatchedOrReleased) return false;
    if (pressedTapKey != event.objectID) {
        serialDebug->print("VT: key ");
        serialDebug->print(event.objectID);
        serialDebug->println(" released without a press -- ignored");
        return false;
    }
    pressedTapKey = kNoTapKey;

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
            return calibration != nullptr && calibration->OnSoftKey(event.objectID);
        default:
            break;
    }
    return false;
}

}  // namespace triton

#endif  // ISOBUS
