/*
  WidthPlanner - the plough's own furrow straightening and fan-out
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

#include <stdint.h>

namespace triton
{

// How the plough picks its width in AUTO.
//   Xte      - today's control: the set width plus the XTE correction, which
//              pulls every furrow onto the nearest guidance track.
//   Straight - the simulator's hybrid: the width corrects the distance still
//              to go over about K passes, then works at the nominal width.
//   FanOut   - the distance still to go spread evenly over every pass left.
enum class WidthMode : uint8_t { Xte = 0, Straight = 1, FanOut = 2 };

// Where the plan is.
//   NoPlan    - never started: no end recorded for this field
//   OneEnd    - END 1 recorded, END 2 not yet; the plough works at the
//               nominal width meanwhile (EEPROM value 1, the first pass's
//               state before the ends, 2026-11-16)
//   Planned   - the number of passes is known, Mleft counts down per pass
enum class PlanState : uint8_t { NoPlan = 0, OneEnd = 1, Planned = 2 };

// What RecordEnd() did.
enum class EndResult : uint8_t {
    Recorded,        // END 1: the plan starts, END 2 to go
    Planned,         // END 2: the passes are planned
    NoDistance,      // no fresh track data: nothing recorded
    NoEnd1,          // END 2 before END 1
    OtherField,      // END 2 in another field than END 1
    AlreadyPlanned,  // END 2 again: END 1 starts a new plan
};

// Why there is no planned width (GetReason()).
enum class PlanReason : uint8_t {
    Planned = 0,    // a width is planned
    ModeXte,        // the mode is Xte
    NoPlan,         // no plan: record the field's ends first
    NoTrack,        // no fresh track number, XTE or track spacing
    OtherField,     // the plan belongs to another field (DDI 508)
    ParallelStrip,  // the last passes at the reference: plain XTE control
    Done,           // every planned pass ploughed
};

struct WidthPlannerConfig {
    WidthMode mode           = WidthMode::Xte;
    uint8_t   k              = 10;   // Straight: passes to straighten over
    uint8_t   parallelPasses = 1;    // R: passes left parallel at the reference
    float     smoothingM     = 3.0f; // how far the position is smoothed over
    int16_t   wnomCm         = 0;    // the plough's set width (offset)
    int16_t   wminCm         = 0;    // the narrowest width the plan may use
    int16_t   wmaxCm         = 0;    // the widest
};

// What the guidance gives, already checked for age by the caller.
struct TrackSample {
    bool     valid        = false;   // track (509/511), XTE and spacing fresh
    int32_t  track        = 0;       // DDI 509, as the TC sends it
    int32_t  trackLeft    = 0;       // DDI 511
    int32_t  swathMm      = 0;       // DDI 512
    float    xteM         = 0.0f;    // PGN 129283, + = right of the track
    uint16_t fieldIndex   = 0;       // DDI 508's upper 16 bits (the TC's field)
};

// What survives a power cut: written by the caller when IsDirty().
struct StoredPlan {
    PlanState state       = PlanState::NoPlan;
    bool      passStarted = false;   // the current pass has been lowered once
    bool      lastSide    = false;   // the side the current pass started on
    uint16_t  mleft       = 0;       // passes left, the current one included
    uint16_t  m           = 0;       // passes planned in all
    uint16_t  fieldIndex  = 0;
    uint16_t  endADm      = 0;       // END 1: land left to the reference (dm)
    uint16_t  endBDm      = 0;       // END 2
};

// The plough-local planner of plow_width_control_sim (8): from the track
// number and XTE it works out its own distance from the reference line, and
// from that the width for every pass. No map, no field length, no odometry:
// the tractor drives in the previous furrow, so where the plough is now is
// where the previous furrow is.
//
// Reference: track kReferenceTrack as the AgOpenGPS TC sends it (it adds 1
// to AgOpenGPS's own numbers, so this is AgOpenGPS's AB line). Distances are
// counted from there, towards the unploughed land.
//
// Pure arithmetic: no Arduino, no EEPROM, no time source of its own, so the
// native suite drives it directly.
class WidthPlanner {
public:
    static constexpr int32_t kReferenceTrack = 1;

    // The plough's signed distance from the reference line, in metres.
    // AgOpenGPS's XTE follows the direction of travel and its track numbers
    // the field, so the XTE counts with its sign when heading the same way as
    // the AB line (left = track - 1) and against it when heading opposite
    // (left = track + 1). Bench 2026-11-16, both directions.
    static float Distance(int32_t track, int32_t trackLeft, float swathM, float xteM);

    // One step: call on every new XTE fix. working = plough lowered (hitch),
    // side = turnover side, speedMs = ground speed.
    void Update(const WidthPlannerConfig& config, const TrackSample& sample,
                bool working, bool side, float speedMs, uint32_t nowMs);

    // The planned width, when there is one (GetReason() == Planned).
    bool    HasSetpoint() const   { return reason == PlanReason::Planned; }
    int16_t GetSetpointCm() const { return setpointCm; }
    PlanReason GetReason() const  { return reason; }

    PlanState GetState() const { return plan.state; }
    uint16_t  GetM() const     { return plan.m; }
    uint16_t  GetMleft() const { return plan.mleft; }
    // The plan cannot be ploughed inside Wmin..Wmax: the passes left need a
    // width outside them somewhere (the simulator's "no solution").
    bool      IsFeasible() const { return feasible; }
    // Live values for the screens and the debug dump.
    float     GetDistanceM() const         { return smoothedM; }
    bool      HasDistance() const          { return haveSmoothed; }

    // The operator's controls: the field's two ends, and a pass the counter
    // missed or counted twice.
    //
    // At each end of the field the operator records where the plough is,
    // as the land left to the reference (the current pass included): END 1
    // starts a new plan (whatever plan there was goes), END 2 then plans
    // the passes from the two. Both on the same pass, or with the plough up
    // (along the headland, before ploughing). With the plough in the ground
    // at END 2 that pass is the plan's first; with it up, the next lowering is.
    EndResult RecordEnd(uint8_t end, const WidthPlannerConfig& config, uint16_t fieldIndex);
    void AdjustMleft(int delta);

    // The ends recorded, metres of land left (0 = not recorded).
    float GetEndM(uint8_t end) const { return (end == 1 ? plan.endADm : plan.endBDm) / 10.0f; }
    // Whether the distance is known now (fresh track data since the last
    // pass change): what an end records.
    bool  CanRecordEnd() const { return haveSmoothed && lastUsable; }

    const StoredPlan& GetStored() const { return plan; }
    void Restore(const StoredPlan& stored);
    bool IsDirty() const { return dirty; }
    void ClearDirty() { dirty = false; }

private:
    StoredPlan plan;
    bool       dirty = false;

    // Pass detection: a pass starts when the plough is lowered on the other
    // side (or the first lowering after END 2 with the plough up). Lifting and lowering on the
    // same side mid-pass is the same pass.
    bool     wasWorking       = false;
    bool     lastSide         = false;   // the side at the last Update()

    // Smoothing (Brown's double exponential over distance travelled).
    bool     haveSmoothed     = false;
    float    s1 = 0.0f, s2 = 0.0f;
    float    smoothedM        = 0.0f;
    float    lastRawM         = 0.0f;
    uint32_t lastUpdateMs     = 0;

    // The last Update()'s track data, for RecordEnd().
    bool     lastUsable       = false;
    float    lastSwathM       = 0.0f;

    int16_t    setpointCm = 0;
    PlanReason reason     = PlanReason::NoPlan;
    bool       feasible   = true;

    void onLowered(bool side);
    void planFromEnds(const WidthPlannerConfig& config);
    void smooth(float rawM, float speedMs, uint32_t nowMs, float smoothingM);
    void resetSmoothing();
    int16_t widthFor(const WidthPlannerConfig& config, float swathM);
};

}  // namespace triton
