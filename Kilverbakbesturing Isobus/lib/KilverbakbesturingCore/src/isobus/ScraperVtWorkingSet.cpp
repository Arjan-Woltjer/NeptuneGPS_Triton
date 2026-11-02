/*
  ScraperVtWorkingSet - the kilverbak's working screen and keys on the VT
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
#include "ScraperVtWorkingSet.hpp"

// The header is itself empty unless ISOBUS is defined -- guard the body too,
// so this compiles to an empty translation unit when PlatformIO's LDF pulls
// it in on a serial build anyway.
#ifdef ISOBUS

#include "VTObjectPool.hpp"
#include "../ScraperGates.hpp"

using namespace isobus;

namespace triton
{

// ----------------------------------------------------------------
// The object pool (VTObjectPool.cpp)
// ----------------------------------------------------------------
void ScraperVtWorkingSet::BuildPool() {
    BuildObjectPool();
}

const uint8_t* ScraperVtWorkingSet::GetPoolData() const {
    return VT3PoolData;
}

uint32_t ScraperVtWorkingSet::GetPoolSize() const {
    return VT3PoolSize;
}

const char* ScraperVtWorkingSet::GetPoolLabel() const {
    // A terminal caches the pool under this label and skips the upload when
    // it matches, so an unbumped label after a pool change shows the old
    // screen without any error (the plough's MW history in
    // PloughVtWorkingSet.cpp). Bump it on every change to VTObjectPool.cpp
    // or VTImages.generated.cpp. KB, not the plough's MW: a terminal that
    // keys its cache on the label alone must never take one for the other.
    // KB01, 2026-11-02: the working screen and the reference page (#291).
    return "KB01";
}

// ----------------------------------------------------------------
// The connection's events
// ----------------------------------------------------------------
void ScraperVtWorkingSet::OnClientCreated(std::shared_ptr<VirtualTerminalClient> newClient) {
    client = newClient;
    // A new VT means a fresh handshake: send every value again once it
    // connects.
    sentInitialValues = false;
}

void ScraperVtWorkingSet::OnConnectionLost() {
    // An open reference page belongs to the VT being left.
    refCapture.OnConnectionLost();
}

void ScraperVtWorkingSet::Update(bool connected) {
    // The page lives on this VT only: a lost connection closes it.
    if (connected) refCapture.Update(*this);
    else if (refCapture.IsOpen()) refCapture.OnConnectionLost();
}

bool ScraperVtWorkingSet::ChangeActiveMask(uint16_t dataMask) {
    return client && client->send_change_active_mask(Scraper_WorkingSet, dataMask);
}

bool ScraperVtWorkingSet::ChangeNumericValue(uint16_t objectId, uint32_t value) {
    return client && client->send_change_numeric_value(objectId, value);
}

// ----------------------------------------------------------------
// The working screen's values
// ----------------------------------------------------------------
uint16_t ScraperVtWorkingSet::ScreenValueObject(ScreenValue value) {
    switch (value) {
        case Deviation:    return Var_Deviation;
        case DeviationBar: return Var_DeviationBar;
        case Offset:       return Var_Offset;
        case Reference:    return Var_Reference;
        case Height:       return Var_Height;
        case Distance:     return Var_Distance;
        case Slope:        return Var_Slope;
        case Speed:        return Var_Speed;
        case Picture:      return Ptr_ScraperImage;
        case GpsIcon:      return Ptr_GpsStatus;
        case SpeedIcon:    return Ptr_SpeedStatus;
        case Mode:         return Ptr_Mode;
        default:           return 0xFFFF;
    }
}

static int32_t clampTo(int32_t value, int32_t lowest, int32_t highest) {
    return value < lowest ? lowest : value > highest ? highest : value;
}

void ScraperVtWorkingSet::ComputeScreenValues(ImplementScraper& implement, GuidanceSource& guidance,
                                              InterfaceScraper& scraperInterface, unsigned long nowMs,
                                              uint32_t (&values)[kScreenValueCount]) {
    // The biased variables (VTObjectPool.hpp) stay non-negative; a value past
    // a field's range pegs at its limit instead of wrapping through the
    // uint32 (the plough's XTE lesson, #20).
    const int32_t deviation = implement.GetDeviation();
    values[Deviation]    = uint32_t(clampTo(deviation, -999, 999) + kSmallBias);
    values[DeviationBar] = uint32_t(clampTo(deviation, -kBarRangeCm, kBarRangeCm) + kBarRangeCm);
    values[Offset]       = uint32_t(clampTo(implement.GetOffset(), -999, 999) + kSmallBias);
    values[Reference]    = uint32_t(int32_t(implement.GetRefHeight()) + kLargeBias);
    values[Height]       = uint32_t(int32_t(implement.GetHeight()) + kLargeBias);
    values[Distance]     = uint32_t(clampTo(implement.GetDistance(), -99999, 99999) + kLargeBias);
    values[Slope]        = uint32_t(clampTo(implement.GetSlope(), -999, 999) + kSmallBias);

    // Speed in 0.1 km/h (the OutputNumber scales by 0.1).
    float speedDeciKmh = guidance.GetSpeedMs() * 36.0f;
    if (speedDeciKmh < 0.0f) speedDeciKmh = 0.0f;
    values[Speed] = uint32_t(speedDeciKmh + 0.5f);

    // The side view follows the sign of the deviation, which is what Auto
    // drives on: with no position sensor (NOSENS, every board) any deviation
    // but 0 moves the valves, and positive lowers the bucket.
    values[Picture] = deviation > 0 ? Img_AbovePlane : deviation < 0 ? Img_BelowPlane : Img_OnPlane;

    // GPS: a fresh, RTK fixed fix (the gates without the speed gate). Speed:
    // OK when the speed gate is met, or switched off and so never holds.
    const bool gpsOk = ScraperHoldReason(guidance, false, nowMs) == ScraperHold::None;
    const bool speedOk = !implement.GetSpeedGate() || ScraperSpeedGateMet(guidance, nowMs);
    values[GpsIcon]   = gpsOk ? Img_StatusOk : Img_StatusWarn;
    values[SpeedIcon] = speedOk ? Img_StatusOk : Img_StatusWarn;

    // The mode as the LCD shows it: A, M, or H with the reason.
    switch (scraperInterface.GetMode()) {
        case 0:
            values[Mode] = Str_ModeAuto;
            break;
        case 1:
            values[Mode] = scraperInterface.GetHoldReason() == ScraperHold::NoRtk ? Str_HoldNoRtk
                         : scraperInterface.GetHoldReason() == ScraperHold::Speed ? Str_HoldSpeed
                                                                                 : Str_HoldNoFix;
            break;
        default:
            values[Mode] = Str_ModeHand;
            break;
    }
}

void ScraperVtWorkingSet::UpdateValues() {
    if (!client || !client->get_is_connected()) return;

    uint32_t values[kScreenValueCount];
    ComputeScreenValues(*implement, *guidance, *scraperInterface, millis(), values);

    // On change, plus everything once a second, so a dropped frame cannot
    // leave the VT stale.
    const bool heartbeatDue = millis() - lastHeartbeatMs >= 1000;
    const bool forceSend = !sentInitialValues || heartbeatDue;
    for (uint8_t i = 0; i < kScreenValueCount; i++) {
        if (forceSend || values[i] != lastSent[i]) {
            client->send_change_numeric_value(ScreenValueObject(ScreenValue(i)), values[i]);
            lastSent[i] = values[i];
        }
    }
    sentInitialValues = true;
    if (heartbeatDue) lastHeartbeatMs = millis();
}

// ----------------------------------------------------------------
// Keys
// ----------------------------------------------------------------
bool ScraperVtWorkingSet::IsPlusHeld() const {
    return plusHeld && millis() - plusEventMs <= kSoftKeyHeldTimeoutMs;
}

bool ScraperVtWorkingSet::IsMinusHeld() const {
    return minusHeld && millis() - minusEventMs <= kSoftKeyHeldTimeoutMs;
}

bool ScraperVtWorkingSet::OnKeyEvent(const VirtualTerminalClient::VTKeyEvent& event) {
    using Code = VirtualTerminalClient::KeyActivationCode;

    // + and -: held from press (or a still-held repeat) until release or an
    // aborted press, see IsPlusHeld().
    if (event.objectID == Key_Plus || event.objectID == Key_Minus) {
        const bool plus = event.objectID == Key_Plus;
        bool& held = plus ? plusHeld : minusHeld;
        const bool down = event.keyEvent == Code::ButtonPressedOrLatched || event.keyEvent == Code::ButtonStillHeld;
        if (event.keyEvent == Code::ButtonPressedOrLatched) {
            serialDebug->println(plus ? "VT: + pressed" : "VT: - pressed");
        }
        else if (!down && held) {
            serialDebug->println(plus ? "VT: + released" : "VT: - released");
        }
        held = down;
        (plus ? plusEventMs : minusEventMs) = millis();
        return false;
    }

    if (event.keyEvent != Code::ButtonUnlatchedOrReleased) return false;

    switch (event.objectID) {
        case Key_RefA:
        case Key_RefB:
            if (cabWizard != nullptr && cabWizard->IsActive()) {
                serialDebug->println("VT: REF key ignored, the cab wizard is open");
                break;
            }
            refCapture.Open(event.objectID == Key_RefA ? ScraperRefCapture::Point::A : ScraperRefCapture::Point::B);
            break;
        case Key_RefSave:
        case Key_RefCancel:
            refCapture.OnSoftKey(event.objectID);
            break;
        default:
            break;
    }
    // Nothing here saves settings, so the preferred VT stays as it was.
    return false;
}

}  // namespace triton

#endif  // ISOBUS
