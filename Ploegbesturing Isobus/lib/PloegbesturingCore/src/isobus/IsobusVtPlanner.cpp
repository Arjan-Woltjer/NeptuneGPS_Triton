/*
  IsobusVtPlanner - the width planner's pages on the VT, and its word on the
  main screen
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
#include "IsobusVtPlanner.hpp"

#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include "VTObjectPool.hpp"
#include "../FlashMem.hpp"

namespace triton
{

namespace
{
constexpr uint8_t kPerShareLowest  = kRangeBarMinMm / 10;   // 20 cm
constexpr uint8_t kPerShareHighest = kRangeBarMaxMm / 10;   // 60 cm

uint32_t ClampBar(int32_t mm) {
    if (mm < kRangeBarMinMm) return kRangeBarMinMm;
    if (mm > kRangeBarMaxMm) return kRangeBarMaxMm;
    return uint32_t(mm);
}
}  // namespace

FLASHMEM uint16_t IsobusVtPlanner::WordFor(const PlanWordInputs& in) {
    if (in.mode == WidthMode::Xte) return Str_PlanWordXte;
    if (in.state == PlanState::NoPlan) return Str_PlanWordNoPlan;
    if (!in.trackSupport || in.reason == PlanReason::NoTrack) return Str_PlanWordNoTrack;
    if (in.reason == PlanReason::OtherField) return Str_PlanWordField;
    if (in.widthMismatch) return Str_PlanWordWidth;
    if (in.state == PlanState::OneEnd) return Str_PlanWordEnd2;
    if (in.reason == PlanReason::Done) return Str_PlanWordDone;
    if (in.reason == PlanReason::ParallelStrip) return Str_PlanWordParallel;
    if (!in.feasible) return Str_PlanWordRange;
    return in.mode == WidthMode::Straight ? Str_PlanWordStraight : Str_PlanWordFan;
}

bool IsobusVtPlanner::WordHasCount(uint16_t word) {
    return word == Str_PlanWordStraight || word == Str_PlanWordFan;
}

FLASHMEM PlanWordInputs IsobusVtPlanner::CurrentWord() const {
    const WidthPlanner& planner = planning->GetPlanner();
    PlanWordInputs in;
    in.mode         = planning->GetMode();
    in.state        = planner.GetState();
    in.reason       = planner.GetReason();
    in.feasible     = planner.IsFeasible();
    in.trackSupport = planning->HasTrackSupport();
    if (in.trackSupport) {
        const int32_t diff = planning->GetTrackSpacingMm() - int32_t(planning->GetMainWidthCm()) * 10;
        in.widthMismatch = diff > kWidthMismatchMm || diff < -kWidthMismatchMm;
    }
    return in;
}

FLASHMEM bool IsobusVtPlanner::OnSoftKey(uint16_t keyObjectId, unsigned long heldMs) {
    switch (keyObjectId) {
        case Key_Plan:
            open = true;
            rangeOpen = false;
            forgetPage();
            wantedMask = Plough_PlanMask;
            serialDebug->println("VT: planner page open");
            return true;
        case Key_PlanMleftUp:
        case Key_PlanMleftDown:
            if (open) {
                const int delta = keyObjectId == Key_PlanMleftUp ? 1 : -1;
                planning->AdjustMleft(delta);
                serialDebug->print("VT: passes left ");
                serialDebug->println(planning->GetPlanner().GetMleft());
            }
            return true;
        case Key_PlanEnd1:
        case Key_PlanEnd2: {
            if (!open) return true;
            const uint8_t end = keyObjectId == Key_PlanEnd1 ? 1 : 2;
            // END 1 over a plan throws it away: not on a tap.
            if (end == 1 && planning->GetPlanner().GetState() == PlanState::Planned && heldMs < kEnd1HoldMs) {
                serialDebug->println("VT: END 1 over a plan needs the key held 1 s -- ignored");
                return true;
            }
            switch (planning->RecordEnd(end)) {
                case EndResult::Recorded:       serialDebug->println("VT: END 1 recorded, END 2 to go"); break;
                case EndResult::Planned:        serialDebug->println("VT: END 2 recorded, the passes are planned"); break;
                case EndResult::NoDistance:     serialDebug->println("VT: no track data -- end not recorded"); break;
                case EndResult::NoEnd1:         serialDebug->println("VT: END 1 first"); break;
                case EndResult::OtherField:     serialDebug->println("VT: END 2 in another field -- END 1 again"); break;
                case EndResult::AlreadyPlanned: serialDebug->println("VT: planned already -- END 1 starts again"); break;
            }
            return true;
        }
        case Key_PlanRange:
            // LIMIT, from the planner page or the main screen; back to the
            // one it came from.
            openRange();
            return true;
        case Key_PlanBack:
            // Also from a page a reconnect left on the VT.
            open = false;
            rangeOpen = false;
            wantedMask = Plough_DataMask;
            return true;
        case Key_RangeUp:
        case Key_RangeDown:
            if (rangeOpen) stepRange(keyObjectId == Key_RangeUp ? 1 : -1);
            return true;
        case Key_RangeSelect:
            if (rangeOpen) maxSelected = !maxSelected;
            return true;
        case Key_RangeSave:
        case Key_RangeCancel:
            if (rangeOpen && keyObjectId == Key_RangeSave) {
                planning->SetWidthRangePerShare(draftMin, draftMax);
                serialDebug->print("VT: width range ");
                serialDebug->print(draftMin);
                serialDebug->print('-');
                serialDebug->print(draftMax);
                serialDebug->println(" cm per share stored");
            }
            rangeOpen = false;
            forgetPage();
            wantedMask = open ? Plough_PlanMask : rangeToCalibration ? Plough_CalDataMask : Plough_DataMask;
            rangeToCalibration = false;
            return true;
        default:
            return false;
    }
}

FLASHMEM void IsobusVtPlanner::OnNumericValue(uint16_t objectId, uint32_t value) {
    if (!open) return;
    // Each setter refuses what the boot load would; the VT then shows the
    // value held, sent again by Update() because it differs from `shown`.
    const uint8_t v = value > 0xFF ? 0xFF : uint8_t(value);
    switch (objectId) {
        case Input_PlanMode:
            if (WidthPlanning::IsValidMode(int(value))) planning->SetMode(WidthMode(v));
            shown[Mode] = value;
            break;
        case Input_PlanK:
            planning->SetK(v);
            shown[K] = value;
            break;
        case Input_PlanR:
            planning->SetParallel(v);
            shown[R] = value;
            break;
        case Input_PlanSmooth:
            planning->SetSmoothingM(v);
            shown[Smooth] = value;
            break;
        default:
            break;
    }
}

FLASHMEM void IsobusVtPlanner::OnConnectionLost() {
    open = false;
    rangeOpen = false;
    wantedMask = 0;
    forgetShown();
}

FLASHMEM void IsobusVtPlanner::Update(VtCalibrationOutput& out) {
    const WidthPlanner& planner = planning->GetPlanner();

    // The main screen's word, always.
    const uint16_t word = WordFor(CurrentWord());
    send(out, Word, Ptr_PlanWord, word);
    send(out, Mleft, Var_PlanMleft, planner.GetMleft());
    send(out, CountPtr, Ptr_PlanCount, WordHasCount(word) ? uint32_t(Out_PlanCount) : 0xFFFFu);

    if (open && !rangeOpen) {
        const bool track = planning->HasTrackSupport();
        send(out, ModePtr, Ptr_PlanMode, track ? uint32_t(Input_PlanMode) : uint32_t(Str_PlanModeXte));
        send(out, Mode, Input_PlanMode, uint32_t(planning->GetMode()));
        send(out, TuningPtr, Ptr_PlanTuning, track ? uint32_t(Cont_PlanTuning) : 0xFFFFu);
        send(out, K, Input_PlanK, planning->GetK());
        send(out, R, Input_PlanR, planning->GetParallel());
        send(out, Smooth, Input_PlanSmooth, planning->GetSmoothingM());
        send(out, M, Var_PlanM, planner.GetM());
        // Where the plough is, and the ends, in passes; a dash when unknown.
        // NU follows the plough: at most every kNowSendMs, as the bar.
        const unsigned long nowMs = millis();
        const int32_t passNow = planning->GetPassNowTenths();
        if (shown[PassNow] == kNotSent || nowMs - lastPassSendMs >= kNowSendMs) {
            const uint32_t before = shown[PassNow];
            if (passNow >= 0) send(out, PassNow, Var_PlanNow, uint32_t(passNow));
            if (shown[PassNow] != before) lastPassSendMs = nowMs;
        }
        send(out, PassNowPtr, Ptr_PlanNow, passNow >= 0 ? uint32_t(Out_PlanNow) : uint32_t(Str_PlanDash));
        const int32_t end1 = planning->GetPassAtEndTenths(1), end2 = planning->GetPassAtEndTenths(2);
        if (end1 >= 0) send(out, End1, Var_PlanEnd1, uint32_t(end1));
        send(out, End1Ptr, Ptr_PlanEnd1, end1 >= 0 ? uint32_t(Out_PlanEnd1) : uint32_t(Str_PlanDash));
        if (end2 >= 0) send(out, End2, Var_PlanEnd2, uint32_t(end2));
        send(out, End2Ptr, Ptr_PlanEnd2, end2 >= 0 ? uint32_t(Out_PlanEnd2) : uint32_t(Str_PlanDash));
    }

    // The range bar, on the main screen and the range page: the draft while
    // the page is open, the stored range otherwise.
    const int32_t shares = planning->GetShares() > 0 ? planning->GetShares() : 1;
    const int32_t nominal = planning->GetMainWidthCm();
    const uint8_t lo = rangeOpen ? draftMin : planning->GetWminPerShare();
    const uint8_t hi = rangeOpen ? draftMax : planning->GetWmaxPerShare();
    send(out, BarMin, Var_BarMin, uint32_t(lo) * 10);
    send(out, BarMax, Var_BarMax, uint32_t(hi) * 10);
    send(out, BarNom, Var_BarNom, ClampBar(nominal * 10 / shares));
    // The correction limits: the main width -/+ KORR/FEJ, per share in mm.
    const int32_t correction = planning->GetMaxCorrectionPerShareMm();
    // Inside LIMIT, as the setpoint is.
    int32_t corrLo = nominal * 10 / shares - correction, corrHi = nominal * 10 / shares + correction;
    if (corrLo < int32_t(lo) * 10) corrLo = int32_t(lo) * 10;
    if (corrHi > int32_t(hi) * 10) corrHi = int32_t(hi) * 10;
    send(out, BarCorrLo, Var_BarCorrLo, ClampBar(corrLo));
    send(out, BarCorrHi, Var_BarCorrHi, ClampBar(corrHi));
    // The width now moves with the sensor's noise: at most every 200 ms.
    const unsigned long now = millis();
    const int32_t widthNow = planning->GetWidthNowCm() > 0 ? planning->GetWidthNowCm() : 0;
    const bool nowDue = shown[BarNow] == kNotSent || now - lastNowSendMs >= kNowSendMs;
    if (nowDue) {
        const uint32_t before = shown[BarNow];
        send(out, BarNow, Var_BarNow, ClampBar(widthNow * 10 / shares));
        if (shown[BarNow] != before) lastNowSendMs = now;
    }

    if (rangeOpen) {
        send(out, RangeMin, Var_RangeMin, uint32_t(draftMin * shares));
        send(out, RangeMax, Var_RangeMax, uint32_t(draftMax * shares));
        send(out, RangeNom, Var_RangeNom, uint32_t(nominal));
        send(out, Lo, Var_RangeLo, uint32_t(kPerShareLowest * shares));
        send(out, Hi, Var_RangeHi, uint32_t(kPerShareHighest * shares));
        send(out, MinMark, Ptr_RangeMinMark, maxSelected ? uint32_t(Str_RangeNoMark) : uint32_t(Str_RangeMark));
        send(out, MaxMark, Ptr_RangeMaxMark, maxSelected ? uint32_t(Str_RangeMark) : uint32_t(Str_RangeNoMark));
        if (nowDue) send(out, RangeNow, Var_RangeNow, uint32_t(widthNow));
    }

    if (wantedMask != 0 && out.ChangeActiveMask(wantedMask)) wantedMask = 0;
}

FLASHMEM void IsobusVtPlanner::send(VtCalibrationOutput& out, Shown what, uint16_t objectId, uint32_t value) {
    if (shown[what] != value && out.ChangeNumericValue(objectId, value)) shown[what] = value;
}

FLASHMEM void IsobusVtPlanner::openRange() {
    draftMin = planning->GetWminPerShare();
    draftMax = planning->GetWmaxPerShare();
    maxSelected = false;
    rangeOpen = true;
    forgetRange();
    wantedMask = Plough_RangeMask;
    serialDebug->println("VT: width range page open");
}

// One cm per share on the selected end, inside 20-60 and never past the
// other end: Wmin stays below Wmax, as WidthPlanning requires.
FLASHMEM void IsobusVtPlanner::stepRange(int delta) {
    if (maxSelected) {
        const int v = int(draftMax) + delta;
        if (v > int(draftMin) && v <= kPerShareHighest) draftMax = uint8_t(v);
    } else {
        const int v = int(draftMin) + delta;
        if (v >= kPerShareLowest && v < int(draftMax)) draftMin = uint8_t(v);
    }
}

FLASHMEM void IsobusVtPlanner::forgetShown() {
    for (uint32_t& value : shown) value = kNotSent;
}

// The page's values, sent again when it opens: another page may have shown
// other values in the shared objects, or the operator left an entry.
FLASHMEM void IsobusVtPlanner::forgetPage() {
    for (uint8_t i = ModePtr; i <= End2Ptr; i++) shown[i] = kNotSent;
}

FLASHMEM void IsobusVtPlanner::forgetRange() {
    for (uint8_t i = RangeMin; i < kShownCount; i++) shown[i] = kNotSent;
}

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
