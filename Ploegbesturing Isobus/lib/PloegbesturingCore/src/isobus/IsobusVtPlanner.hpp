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
#pragma once

// The planner works on the TC's track data: ISOBUS only, on the board and in
// [env:native].
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

#include "IsobusVtCalibration.hpp"   // VtCalibrationOutput
#include "../planner/WidthPlanning.hpp"

namespace triton
{

// What the main screen's plan word is made of (WordFor()).
struct PlanWordInputs {
    WidthMode  mode         = WidthMode::Xte;
    PlanState  state        = PlanState::NoPlan;
    PlanReason reason       = PlanReason::NoPlan;
    bool       feasible     = true;
    bool       trackSupport = false;
    bool       widthMismatch = false;   // the main width is not the track spacing
};

// The width planner (WidthPlanning) on the VT:
//   - its word under the state on the main screen, always: XTE, GEEN PLAN,
//     GEEN LIJN, VELD?, BREEDTE?, EIND 2?, KLAAR, PARALLEL, LIMIET!, or
//     RECHT / WAAIER with the passes left; and the range bar there;
//   - the planner page PLAN opens: the mode, K, R and the smoothing as inputs
//     (stored at once, like ":plan"; without track data only XTE shows),
//     where the plough is and the two ends as passes to the reference, the
//     word, Mleft and M. GANG +1 / -1 correct the pass counter, EIND 1
//     starts a plan (held kEnd1HoldMs over an existing one), EIND 2
//     completes it, TERUG goes back;
//   - the width range page LIMIET opens (on the main screen): Wmin and Wmax
//     in cm over a bar of the plough's range, + / - step the one MIN/MAX
//     selects by 1 cm per share, OPSLAAN stores, ANNUL. leaves them.
// Nothing here holds the control: the plough keeps working on the main mask's
// rules while a page is open.
class IsobusVtPlanner {
public:
    // END 1 over a plan throws it away: a tap on a bumping tractor must not.
    static constexpr unsigned long kEnd1HoldMs = 1000;
    // The main width and the TC's track spacing differ by more than this:
    // the plan's arithmetic (one pass = one track) does not hold.
    static constexpr int32_t kWidthMismatchMm = 20;
    // The width now goes to the range page at most this often.
    static constexpr unsigned long kNowSendMs = 200;

    IsobusVtPlanner(Stream* serialDebug, WidthPlanning* planning)
        : serialDebug(serialDebug), planning(planning) {
        forgetShown();
    }

    bool IsOpen() const { return open; }
    bool IsRangeOpen() const { return rangeOpen; }

    // A planner key, on release; heldMs is how long it was down. False when
    // it is not one of the planner's keys (Key_Plan included).
    bool OnSoftKey(uint16_t keyObjectId, unsigned long heldMs);

    // A value the operator entered on the planner page.
    void OnNumericValue(uint16_t objectId, uint32_t value);

    // A new VT, or the old one gone: every value is sent again, the pages
    // close (the range draft is dropped).
    void OnConnectionLost();

    // Sends what the VT does not show yet. Every loop pass while connected.
    void Update(VtCalibrationOutput& out);

    // The string Ptr_PlanWord shows, and whether the passes left follow it.
    static uint16_t WordFor(const PlanWordInputs& in);
    static bool     WordHasCount(uint16_t word);
    PlanWordInputs  CurrentWord() const;

    // The range draft, per share (cm): what + / - change.
    uint8_t GetDraftMin() const { return draftMin; }
    uint8_t GetDraftMax() const { return draftMax; }
    bool    IsMaxSelected() const { return maxSelected; }

private:
    static constexpr uint32_t kNotSent = 0xFFFFFFFF;

    // The values the VT shows, by what they are.
    enum Shown : uint8_t {
        Word, CountPtr, Mleft,
        ModePtr, Mode, TuningPtr, K, R, Smooth, M, PassNow, PassNowPtr, End1, End1Ptr, End2, End2Ptr,
        RangeMin, RangeMax, RangeNom, RangeNow, BarMin, BarMax, BarNom, BarNow, Lo, Hi, MinMark, MaxMark,
        kShownCount
    };

    Stream*        serialDebug;
    WidthPlanning* planning;

    bool     open = false;
    bool     rangeOpen = false;
    uint16_t wantedMask = 0;     // the mask to show, 0 = none pending
    uint8_t  draftMin = 0;
    uint8_t  draftMax = 0;
    bool     maxSelected = false;
    uint32_t shown[kShownCount];
    unsigned long lastNowSendMs = 0;
    unsigned long lastPassSendMs = 0;

    void forgetShown();
    void forgetPage();
    void forgetRange();
    void send(VtCalibrationOutput& out, Shown what, uint16_t objectId, uint32_t value);
    void openRange();
    void stepRange(int delta);
};

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
