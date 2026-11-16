/*
  test_WidthPlanner - the plough's own furrow straightening and fan-out
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
#include <math.h>

#include <AUnit.h>

#include "planner/WidthPlanner.hpp"

using namespace aunit;
using namespace triton;

namespace
{

constexpr float    kSwathM  = 2.0f;
constexpr uint16_t kField   = 3;
constexpr float    kSpeedMs = 2.0f;

// The simulator's defaults: 2.00 m nominal, range +-15 %.
WidthPlannerConfig Config(WidthMode mode, uint8_t parallel = 0) {
    WidthPlannerConfig c;
    c.mode           = mode;
    c.k              = 10;
    c.parallelPasses = parallel;
    c.smoothingM     = 3.0f;
    c.wnomCm         = 200;
    c.wminCm         = 170;
    c.wmaxCm         = 230;
    return c;
}

// What the TC and PGN 129283 would send for a plough `distanceM` from the
// reference, heading the same way as the AB line.
TrackSample At(float distanceM, uint16_t field = kField) {
    TrackSample s;
    s.valid      = true;
    s.swathMm    = int32_t(kSwathM * 1000);
    s.track      = int32_t(floorf(distanceM / kSwathM + 0.5f)) + WidthPlanner::kReferenceTrack;
    s.trackLeft  = s.track - 1;
    s.xteM       = distanceM - (s.track - WidthPlanner::kReferenceTrack) * kSwathM;
    s.fieldIndex = field;
    return s;
}

struct Rig {
    WidthPlanner       planner;
    WidthPlannerConfig config;
    uint32_t           now  = 1000;
    bool               side = false;

    explicit Rig(const WidthPlannerConfig& c) : config(c) {}

    void Step(const TrackSample& s, bool working) {
        now += 1000;
        planner.Update(config, s, working, side, kSpeedMs, now);
    }
    // A pass along the field: lowered on the next side, the distance from
    // the reference going from `top` to `bottom`, then lifted.
    void Pass(float top, float bottom, int stations = 100) {
        side = !side;
        for (int i = 0; i <= stations; i++) Step(At(top + (bottom - top) * i / stations), true);
        Step(At(bottom), false);
    }
    // Along the pass, still lowered: the previous furrow's distance changes
    // a little per step, as it can along a real pass, then holds.
    void Walk(float from, float to, int stations = 100) {
        for (int i = 1; i <= stations; i++) Step(At(from + (to - from) * i / stations), true);
        for (int i = 0; i < 10; i++) Step(At(to), true);
    }
    // Lowered on the next side and held at one distance.
    void LowerAt(float distanceM) {
        side = !side;
        for (int i = 0; i < 3; i++) Step(At(distanceM), true);
    }
};

}  // namespace

// --- Distance: the bench's own numbers (2026-11-16, 1.2 m spacing) ---------

// Heading opposite the AB line (left = track + 1): the XTE counts against.
test(WidthPlanner, distance_headingOpposite) {
    assertNear(WidthPlanner::Distance(-686, -685, 1.2f, -0.21f), -824.19f, 0.001f);
}

// Heading the same way (left = track - 1): the XTE counts with its sign.
test(WidthPlanner, distance_headingSameWay) {
    assertNear(WidthPlanner::Distance(-682, -683, 1.2f, -0.15f), -819.75f, 0.001f);
}

// The bench's track change on the return pass: the distance moves 18 cm, not
// one spacing.
test(WidthPlanner, distance_continuousThroughATrackChange) {
    const float before = WidthPlanner::Distance(-682, -683, 1.2f, -0.44f);
    const float after  = WidthPlanner::Distance(-683, -684, 1.2f, 0.58f);
    assertNear(after - before, -0.18f, 0.001f);
}

// --- When there is no planned width ----------------------------------------

test(WidthPlanner, xteMode_plansNothing) {
    Rig r(Config(WidthMode::Xte));
    r.planner.NewField(kField);
    r.LowerAt(150);
    assertFalse(r.planner.HasSetpoint());
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::ModeXte);
}

test(WidthPlanner, noReset_noPlan) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(150);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::NoPlan);
}

test(WidthPlanner, noTrackData_noWidth) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    TrackSample none;
    r.Step(none, true);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::NoTrack);
}

// --- The plan ---------------------------------------------------------------

// Pass 1 follows the field edge at the nominal width.
test(WidthPlanner, firstPass_atTheNominalWidth) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.LowerAt(160);
    assertEqual((int)r.planner.GetState(), (int)PlanState::FirstPass);
    assertTrue(r.planner.HasSetpoint());
    assertEqual((int)r.planner.GetSetpointCm(), 200);
}

// NEW FIELD pressed with the plough already in the ground: that pass is
// pass 1, and turning over starts pass 2.
test(WidthPlanner, newFieldWhileWorking_thisPassIsPassOne) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(160);
    r.planner.NewField(kField);
    assertTrue(r.planner.GetStored().passStarted);
    r.Walk(160, 140);
    r.Step(At(140), false);
    r.LowerAt(158);
    assertEqual((int)r.planner.GetState(), (int)PlanState::Planned);
    assertEqual((int)r.planner.GetM(), 75);
}

// The simulator's trapezoid, 80 passes at one end and 70 at the other: the
// first furrow is 158 / 138 m from the reference, and M is 75, as there.
test(WidthPlanner, trapezoid_plansTheSimulatorsM) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    assertEqual((int)r.planner.GetState(), (int)PlanState::Planned);
    assertEqual((int)r.planner.GetM(), 75);
    assertEqual((int)r.planner.GetMleft(), 74);
    assertTrue(r.planner.IsFeasible());
}

// Fan-out: what is left, spread evenly over the passes left.
test(WidthPlanner, fanOut_spreadsWhatIsLeft) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    assertEqual((int)r.planner.GetSetpointCm(), 214);   // 158 / 74 = 2.135 m
    r.Walk(158, 138);
    assertEqual((int)r.planner.GetSetpointCm(), 186);   // 138 / 74 = 1.865 m
}

// Straightening: the error over K = 10 passes, limited to the range.
test(WidthPlanner, straight_correctsOverKPasses) {
    Rig r(Config(WidthMode::Straight));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    assertEqual((int)r.planner.GetSetpointCm(), 230);   // 10 m / 10 = 1 m, limited to +30 cm
    r.Walk(158, 148.5f);
    assertEqual((int)r.planner.GetSetpointCm(), 205);   // 0.5 m / 10 = 5 cm
}

// R passes kept parallel at the reference: the plan leaves R spacings, and
// those passes go back to plain XTE control.
test(WidthPlanner, parallelStrip_leftForTheLastPasses) {
    Rig r(Config(WidthMode::FanOut, 2));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    assertEqual((int)r.planner.GetMleft(), 74);          // 72 planned + 2 parallel
    assertEqual((int)r.planner.GetSetpointCm(), 214);   // (158 - 4) / 72 = 2.139 m

    r.planner.AdjustMleft(-70);                          // skip ahead: Mleft 4
    r.Step(At(150), false);
    r.LowerAt(6.2f);                                     // turned over: Mleft 3, the last planned pass
    assertEqual((int)r.planner.GetMleft(), 3);
    assertEqual((int)r.planner.GetSetpointCm(), 220);   // takes what's left: 6.2 - 4 = 2.2 m
    r.Step(At(6.2f), false);
    r.LowerAt(4.0f);                                     // Mleft 2: the parallel strip
    assertFalse(r.planner.HasSetpoint());
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::ParallelStrip);
}

// A wide field: more passes than a byte holds.
test(WidthPlanner, wideField_moreThan255Passes) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(620, 600, 200);
    r.LowerAt(618);
    assertEqual((int)r.planner.GetMleft(), 304);   // (618 + 598) / 2 / 2 m
    assertEqual((int)r.planner.GetM(), 305);
    assertEqual((int)r.planner.GetSetpointCm(), 203);   // 618 / 304 = 2.033 m
}

// A range that can't hold the field: no N fits, and the plan says so.
test(WidthPlanner, narrowRange_isInfeasible) {
    WidthPlannerConfig c = Config(WidthMode::FanOut);
    c.wminCm = 190;
    c.wmaxCm = 210;
    Rig r(c);
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    assertFalse(r.planner.IsFeasible());
}

// --- Pass counting ------------------------------------------------------------

// Lifted and lowered on the same side mid-pass: the same pass.
test(WidthPlanner, relowerOnTheSameSide_isTheSamePass) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    r.Step(At(150), false);
    for (int i = 0; i < 3; i++) r.Step(At(150), true);   // same side
    assertEqual((int)r.planner.GetMleft(), 74);
    r.Step(At(140), false);
    r.LowerAt(156);                                      // turned over
    assertEqual((int)r.planner.GetMleft(), 73);
}

test(WidthPlanner, adjustMleft_staysInRange) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    r.planner.AdjustMleft(+2);
    assertEqual((int)r.planner.GetMleft(), 76);
    r.planner.AdjustMleft(-200);
    assertEqual((int)r.planner.GetMleft(), 0);
    r.Step(At(150), true);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::Done);
}

// --- Field and power cut -------------------------------------------------------

test(WidthPlanner, otherField_usesNoPlan) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    r.Step(At(158, kField + 1), true);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::OtherField);
}

test(WidthPlanner, restore_carriesOnWithThePlan) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(158);
    assertTrue(r.planner.IsDirty());
    const StoredPlan stored = r.planner.GetStored();

    Rig after(Config(WidthMode::FanOut));
    after.planner.Restore(stored);
    after.side = r.side;
    for (int i = 0; i < 3; i++) after.Step(At(158), true);   // lowered on the same side
    assertEqual((int)after.planner.GetMleft(), 74);
    assertEqual((int)after.planner.GetSetpointCm(), 214);
    assertEqual((int)after.planner.GetStored().firstMaxDm, 1580);
    assertEqual((int)after.planner.GetStored().firstMinDm, 1380);
}

// --- The track change race ---------------------------------------------------

// The TC's track number steps a moment before PGN 129283's XTE does: the
// distance stays continuous instead of jumping one spacing.
test(WidthPlanner, trackAheadOfXte_staysContinuous) {
    Rig r(Config(WidthMode::FanOut));
    r.planner.NewField(kField);
    r.Pass(160, 140);
    r.LowerAt(150.9f);
    TrackSample s = At(150.9f);   // track 76, xte +0.9
    s.track += 1;                 // the TC is already on 77
    s.trackLeft += 1;
    r.Step(s, true);
    assertNear(r.planner.GetDistanceM(), 150.9f, 0.05f);
}
