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
#include "WidthPlanner.hpp"

#include <math.h>

namespace triton
{

namespace
{
float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

uint16_t ToDm(float m) {
    const float dm = roundf(m * 10.0f);
    if (dm <= 0.0f) return 0;
    if (dm >= 65535.0f) return 65535;
    return static_cast<uint16_t>(dm);
}
}  // namespace

float WidthPlanner::Distance(int32_t track, int32_t trackLeft, float swathM, float xteM) {
    // Heading opposite the AB line: left is the next track up (bench pass 1,
    // track -686, left -685); heading the same way: the next one down.
    const float sign = (trackLeft == track + 1) ? -1.0f : 1.0f;
    return static_cast<float>(track - kReferenceTrack) * swathM + sign * xteM;
}

void WidthPlanner::NewField(uint16_t fieldIndex) {
    plan = StoredPlan();
    plan.state      = PlanState::FirstPass;
    plan.fieldIndex = fieldIndex;
    // Pressed with the plough already in the ground (the first pass under
    // way): that pass is pass 1, on the side it is on.
    if (wasWorking) {
        plan.passStarted = true;
        plan.lastSide    = lastSide;
    }
    firstSeen       = false;
    feasible        = true;
    resetSmoothing();
    dirty = true;
}

void WidthPlanner::AdjustMleft(int delta) {
    if (plan.state != PlanState::Planned) return;
    int value = int(plan.mleft) + delta;
    if (value < 0) value = 0;
    if (value > 65534) value = 65534;
    if (value != plan.mleft) {
        plan.mleft = static_cast<uint16_t>(value);
        dirty = true;
    }
}

void WidthPlanner::Restore(const StoredPlan& stored) {
    plan = stored;
    firstSeen = false;   // a first pass cut short by a power cut records on
    resetSmoothing();
    dirty = false;
}

void WidthPlanner::resetSmoothing() {
    haveSmoothed = false;
    s1 = s2 = smoothedM = lastRawM = 0.0f;
    lastUpdateMs = 0;
}

void WidthPlanner::smooth(float rawM, float speedMs, uint32_t nowMs, float smoothingM) {
    if (!haveSmoothed) {
        s1 = s2 = smoothedM = lastRawM = rawM;
        lastUpdateMs = nowMs;
        haveSmoothed = true;
        return;
    }
    const float dtS = static_cast<float>(nowMs - lastUpdateMs) / 1000.0f;
    lastUpdateMs = nowMs;
    lastRawM = rawM;
    const float ds = speedMs * dtS;
    if (ds <= 0.01f) return;   // standing still: nothing new about the furrow
    const float length = smoothingM < 0.1f ? 0.1f : smoothingM;
    const float alpha  = Clamp(2.0f / (length / ds + 1.0f), 0.0f, 1.0f);
    s1 += alpha * (rawM - s1);
    s2 += alpha * (s1 - s2);
    smoothedM = 2.0f * s1 - s2;
}

void WidthPlanner::planFromFirstPass(const WidthPlannerConfig& config, float swathM) {
    // The simulator's M: every station gets the same number of passes, so
    // pick N with N * Wmin <= distance <= N * Wmax everywhere, nearest the
    // middle of the extremes. R passes at the reference are not part of it.
    const float wnom  = config.wnomCm / 100.0f;
    const float wmin  = config.wminCm / 100.0f;
    const float wmax  = config.wmaxCm / 100.0f;
    const float strip = config.parallelPasses * swathM;
    const float fmax  = firstMaxM - strip;
    const float fmin  = firstMinM - strip;

    long n = 0;
    feasible = true;
    if (fmax > 0.0f && wnom > 0.0f && wmin > 0.0f && wmax > 0.0f) {
        const long middle = lroundf((fmax + fmin) / 2.0f / wnom);
        const long lo     = static_cast<long>(ceilf(fmax / wmax - 1e-4f));
        const long hi     = static_cast<long>(floorf(fmin / wmin + 1e-4f));
        if (lo <= hi) {
            n = middle < lo ? lo : (middle > hi ? hi : middle);
        } else {
            n = middle;
            feasible = false;
        }
        if (n < 1) n = 1;
    }
    long total = n + config.parallelPasses;   // the pass starting now included
    if (total > 65534) total = 65534;
    plan.state      = PlanState::Planned;
    plan.mleft      = static_cast<uint16_t>(total);
    plan.m          = static_cast<uint16_t>(total + 1);
    plan.firstMaxDm = ToDm(firstMaxM);
    plan.firstMinDm = ToDm(firstMinM);
}

void WidthPlanner::onLowered(const WidthPlannerConfig& config, float swathM, bool side) {
    if (plan.state == PlanState::NoPlan) return;
    if (!plan.passStarted) {
        // The first lowering after a reset: pass 1 begins.
        plan.passStarted = true;
        plan.lastSide    = side;
        dirty = true;
        return;
    }
    if (side == plan.lastSide) return;   // lifted and lowered mid-pass

    // Turned over and lowered: the next pass.
    plan.lastSide = side;
    if (plan.state == PlanState::FirstPass) {
        // Nothing recorded (no track data all pass): this pass is pass 1 again.
        if (firstSeen) planFromFirstPass(config, swathM);
    } else if (plan.mleft > 0) {
        plan.mleft--;
    }
    resetSmoothing();
    dirty = true;
}

int16_t WidthPlanner::widthFor(const WidthPlannerConfig& config, float swathM) {
    const float wnom  = config.wnomCm / 100.0f;
    const float wmin  = config.wminCm / 100.0f;
    const float wmax  = config.wmaxCm / 100.0f;
    const int   fanLeft = int(plan.mleft) - int(config.parallelPasses);
    const float rem   = fabsf(smoothedM) - config.parallelPasses * swathM;

    float w;
    if (fanLeft == 1) {
        w = rem;   // the last planned pass takes what is left
    } else if (config.mode == WidthMode::FanOut) {
        w = rem / fanLeft;
    } else {
        const int   k  = config.k < 1 ? 1 : (int(config.k) < fanLeft ? int(config.k) : fanLeft);
        const float dw = (rem - fanLeft * wnom) / k;
        w = wnom + Clamp(dw, wmin - wnom, wmax - wnom);
    }
    feasible = rem >= fanLeft * wmin - 0.005f && rem <= fanLeft * wmax + 0.005f;
    w = Clamp(w, wmin, wmax);
    return static_cast<int16_t>(lroundf(w * 100.0f));
}

void WidthPlanner::Update(const WidthPlannerConfig& config, const TrackSample& sample,
                          bool working, bool side, float speedMs, uint32_t nowMs) {
    const float swathM = sample.swathMm / 1000.0f;

    if (working && !wasWorking) onLowered(config, swathM, side);
    wasWorking = working;
    lastSide   = side;

    const bool usable = sample.valid && sample.swathMm > 0 &&
                        (sample.trackLeft == sample.track + 1 || sample.trackLeft == sample.track - 1);
    if (usable && working) {
        float raw = Distance(sample.track, sample.trackLeft, swathM, sample.xteM);
        // The track number (from the TC) and the XTE (PGN 129283) come by
        // different paths, so at a track change one can be a step ahead of
        // the other and the distance jumps by one spacing. Take the
        // neighbouring track that keeps it continuous.
        if (haveSmoothed && fabsf(raw - lastRawM) > 0.5f * swathM) {
            const float up = raw + swathM, down = raw - swathM;
            if (fabsf(up - lastRawM) < fabsf(raw - lastRawM)) raw = up;
            if (fabsf(down - lastRawM) < fabsf(raw - lastRawM)) raw = down;
        }
        smooth(raw, speedMs, nowMs, config.smoothingM);

        if (plan.state == PlanState::FirstPass && plan.passStarted) {
            const float furrow = fabsf(smoothedM) - config.wnomCm / 100.0f;
            if (!firstSeen) {
                firstMaxM = firstMinM = furrow;
                firstSeen = true;
            } else {
                if (furrow > firstMaxM) firstMaxM = furrow;
                if (furrow < firstMinM) firstMinM = furrow;
            }
        }
    }

    // What the width is now.
    if (config.mode == WidthMode::Xte)            { reason = PlanReason::ModeXte; return; }
    if (plan.state == PlanState::NoPlan)          { reason = PlanReason::NoPlan; return; }
    if (!usable)                                  { reason = PlanReason::NoTrack; return; }
    if (sample.fieldIndex != plan.fieldIndex)     { reason = PlanReason::OtherField; return; }
    if (plan.state == PlanState::FirstPass) {
        setpointCm = config.wnomCm;               // the first pass at the nominal width
        reason = PlanReason::Planned;
        return;
    }
    if (plan.mleft == 0)                          { reason = PlanReason::Done; return; }
    if (int(plan.mleft) <= int(config.parallelPasses)) { reason = PlanReason::ParallelStrip; return; }
    if (!haveSmoothed)                            { reason = PlanReason::NoTrack; return; }
    setpointCm = widthFor(config, swathM);
    reason = PlanReason::Planned;
}

}  // namespace triton
