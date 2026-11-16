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
    // Lifted and held at one distance (the headland, or the end of a pass).
    void Lifted(float distanceM) {
        for (int i = 0; i < 3; i++) Step(At(distanceM), false);
    }
    // END 1 or END 2 pressed here.
    EndResult End(uint8_t end, uint16_t field = kField) { return planner.RecordEnd(end, config, field); }
    // The simulator's trapezoid in one pass: END 1 at 158 m of land left,
    // the pass along to 138 m, END 2 there with the plough in the ground.
    void TrapezoidPlan() {
        LowerAt(158);
        End(1);
        Walk(158, 138);
        End(2);
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
    r.LowerAt(150);
    r.End(1);
    r.Step(At(150), true);
    assertFalse(r.planner.HasSetpoint());
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::ModeXte);
}

test(WidthPlanner, noEnds_noPlan) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(150);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::NoPlan);
}

test(WidthPlanner, noTrackData_noWidth) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(150);
    r.End(1);
    TrackSample none;
    r.Step(none, true);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::NoTrack);
}

// --- The ends ----------------------------------------------------------------

// An end records where the plough is, so it needs fresh track data.
test(WidthPlanner, end_needsTrackData) {
    Rig r(Config(WidthMode::FanOut));
    assertEqual((int)r.End(1), (int)EndResult::NoDistance);
    r.LowerAt(150);
    TrackSample none;
    r.Step(none, true);
    assertEqual((int)r.End(1), (int)EndResult::NoDistance);
    assertEqual((int)r.planner.GetState(), (int)PlanState::NoPlan);
}

test(WidthPlanner, end2_needsEnd1InTheSameField) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(158);
    assertEqual((int)r.End(2), (int)EndResult::NoEnd1);
    assertEqual((int)r.End(1), (int)EndResult::Recorded);
    r.Walk(158, 138);
    assertEqual((int)r.End(2, kField + 1), (int)EndResult::OtherField);
    assertEqual((int)r.planner.GetState(), (int)PlanState::OneEnd);
}

// Between the ends the plough works at the nominal width.
test(WidthPlanner, oneEnd_atTheNominalWidth) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(160);
    assertEqual((int)r.End(1), (int)EndResult::Recorded);
    r.Step(At(160), true);
    assertEqual((int)r.planner.GetState(), (int)PlanState::OneEnd);
    assertTrue(r.planner.HasSetpoint());
    assertEqual((int)r.planner.GetSetpointCm(), 200);
    assertNear(r.planner.GetEndM(1), 160.0f, 0.05f);
}

// END 2 again does nothing; END 1 over a plan starts a new one.
test(WidthPlanner, end1OverAPlan_startsAgain) {
    Rig r(Config(WidthMode::FanOut));
    r.TrapezoidPlan();
    assertEqual((int)r.End(2), (int)EndResult::AlreadyPlanned);
    assertEqual((int)r.planner.GetState(), (int)PlanState::Planned);
    assertEqual((int)r.End(1), (int)EndResult::Recorded);
    assertEqual((int)r.planner.GetState(), (int)PlanState::OneEnd);
    assertEqual((int)r.planner.GetM(), 0);
}

// --- The plan ---------------------------------------------------------------

// The simulator's trapezoid, 158 m of land at one end and 138 m at the
// other: M = 74, with END 2's pass the plan's first.
test(WidthPlanner, trapezoid_plansTheSimulatorsM) {
    Rig r(Config(WidthMode::FanOut));
    r.TrapezoidPlan();
    assertEqual((int)r.planner.GetState(), (int)PlanState::Planned);
    assertEqual((int)r.planner.GetM(), 74);
    assertEqual((int)r.planner.GetMleft(), 74);
    assertTrue(r.planner.GetStored().passStarted);
    assertTrue(r.planner.IsFeasible());
    assertEqual((int)r.planner.GetStored().endADm, 1580);
    assertEqual((int)r.planner.GetStored().endBDm, 1380);
}

// Both ends recorded with the plough up, along the headland: the next
// lowering starts the plan's first pass, and the turn after it the second.
test(WidthPlanner, endsWithThePloughUp_theNextLoweringIsThePlansFirst) {
    Rig r(Config(WidthMode::FanOut));
    r.Lifted(158);
    r.End(1);
    for (int i = 1; i <= 100; i++) r.Step(At(158 - 20.0f * i / 100), false);
    r.Lifted(138);
    assertEqual((int)r.End(2), (int)EndResult::Planned);
    assertFalse(r.planner.GetStored().passStarted);
    assertEqual((int)r.planner.GetMleft(), 74);
    r.LowerAt(138);
    assertTrue(r.planner.GetStored().passStarted);
    assertEqual((int)r.planner.GetMleft(), 74);
    r.Lifted(158);
    r.LowerAt(156);
    assertEqual((int)r.planner.GetMleft(), 73);
}

// Fan-out: what is left, spread evenly over the passes left.
test(WidthPlanner, fanOut_spreadsWhatIsLeft) {
    Rig r(Config(WidthMode::FanOut));
    r.TrapezoidPlan();
    r.Step(At(138), true);
    assertEqual((int)r.planner.GetSetpointCm(), 186);   // 138 / 74 = 1.865 m
    r.Lifted(138);
    r.LowerAt(156);                                      // the next pass: Mleft 73
    assertEqual((int)r.planner.GetSetpointCm(), 214);   // 156 / 73 = 2.137 m
}

// Straightening: the error over K = 10 passes, limited to the range.
test(WidthPlanner, straight_correctsOverKPasses) {
    Rig r(Config(WidthMode::Straight));
    r.TrapezoidPlan();
    r.Step(At(138), true);
    assertEqual((int)r.planner.GetSetpointCm(), 170);   // -10 m / 10 = -1 m, limited to -30 cm
    r.Lifted(138);
    r.LowerAt(146.5f);                                   // Mleft 73: 0.5 m over 73 x 2 m
    assertEqual((int)r.planner.GetSetpointCm(), 205);   // 0.5 m / 10 = 5 cm
}

// R passes kept parallel at the reference: the plan leaves R spacings, and
// those passes go back to plain XTE control.
test(WidthPlanner, parallelStrip_leftForTheLastPasses) {
    Rig r(Config(WidthMode::FanOut, 2));
    r.TrapezoidPlan();
    assertEqual((int)r.planner.GetMleft(), 74);          // 72 planned + 2 parallel
    r.Step(At(138), true);
    assertEqual((int)r.planner.GetSetpointCm(), 186);   // (138 - 4) / 72 = 1.861 m

    r.planner.AdjustMleft(-70);                          // skip ahead: Mleft 4
    r.Lifted(150);
    r.LowerAt(6.2f);                                     // turned over: Mleft 3, the last planned pass
    assertEqual((int)r.planner.GetMleft(), 3);
    assertEqual((int)r.planner.GetSetpointCm(), 220);   // takes what is left: 6.2 - 4 = 2.2 m
    r.Lifted(6.2f);
    r.LowerAt(4.0f);                                     // Mleft 2: the parallel strip
    assertFalse(r.planner.HasSetpoint());
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::ParallelStrip);
}

// A wide field: more passes than a byte holds.
test(WidthPlanner, wideField_moreThan255Passes) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(618);
    r.End(1);
    r.Walk(618, 598, 200);
    r.End(2);
    assertEqual((int)r.planner.GetM(), 304);            // (618 + 598) / 2 / 2 m
    assertEqual((int)r.planner.GetMleft(), 304);
    r.Step(At(598), true);
    assertEqual((int)r.planner.GetSetpointCm(), 197);   // 598 / 304 = 1.967 m
}

// A range that cannot hold the field: no N fits, and the plan says so.
test(WidthPlanner, narrowRange_isInfeasible) {
    WidthPlannerConfig c = Config(WidthMode::FanOut);
    c.wminCm = 190;
    c.wmaxCm = 210;
    Rig r(c);
    r.TrapezoidPlan();
    assertFalse(r.planner.IsFeasible());
}

// --- Pass counting ------------------------------------------------------------

// Lifted and lowered on the same side mid-pass: the same pass.
test(WidthPlanner, relowerOnTheSameSide_isTheSamePass) {
    Rig r(Config(WidthMode::FanOut));
    r.TrapezoidPlan();
    r.Step(At(138), false);
    for (int i = 0; i < 3; i++) r.Step(At(138), true);   // same side
    assertEqual((int)r.planner.GetMleft(), 74);
    r.Lifted(138);
    r.LowerAt(156);                                      // turned over
    assertEqual((int)r.planner.GetMleft(), 73);
}

test(WidthPlanner, adjustMleft_staysInRange) {
    Rig r(Config(WidthMode::FanOut));
    r.TrapezoidPlan();
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
    r.TrapezoidPlan();
    r.Step(At(138, kField + 1), true);
    assertEqual((int)r.planner.GetReason(), (int)PlanReason::OtherField);
}

test(WidthPlanner, restore_carriesOnWithThePlan) {
    Rig r(Config(WidthMode::FanOut));
    r.TrapezoidPlan();
    assertTrue(r.planner.IsDirty());
    const StoredPlan stored = r.planner.GetStored();

    Rig after(Config(WidthMode::FanOut));
    after.planner.Restore(stored);
    after.side = r.side;
    for (int i = 0; i < 3; i++) after.Step(At(138), true);   // lowered on the same side
    assertEqual((int)after.planner.GetMleft(), 74);
    assertEqual((int)after.planner.GetSetpointCm(), 186);
    assertEqual((int)after.planner.GetStored().endADm, 1580);
    assertEqual((int)after.planner.GetStored().endBDm, 1380);
}

// --- The track change race ---------------------------------------------------

// The TC's track number steps a moment before PGN 129283's XTE does: the
// distance stays continuous instead of jumping one spacing.
test(WidthPlanner, trackAheadOfXte_staysContinuous) {
    Rig r(Config(WidthMode::FanOut));
    r.LowerAt(150.9f);
    r.End(1);
    TrackSample s = At(150.9f);   // track 76, xte +0.9
    s.track += 1;                 // the TC is already on 77
    s.trackLeft += 1;
    r.Step(s, true);
    assertNear(r.planner.GetDistanceM(), 150.9f, 0.05f);
}
