/*
  WidthPlanning - WidthPlanner wired to the plough: track data in, the
  planned width out, the plan kept in EEPROM
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
#include "WidthPlanning.hpp"

#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <EEPROM.h>

namespace triton
{

namespace
{
// The plan's bytes, from kEepromPlan.
constexpr uint8_t kPlanMagic = 0x5A;
constexpr uint16_t kPlanMagicAt  = kEepromPlan + 0;
constexpr uint16_t kPlanFlagsAt  = kEepromPlan + 1;  // bits 0-1 state, 2 pass started, 3 side
constexpr uint16_t kPlanMAt      = kEepromPlan + 2;
constexpr uint16_t kPlanMleftAt  = kEepromPlan + 3;
constexpr uint16_t kPlanFieldAt  = kEepromPlan + 4;  // uint16 LE
constexpr uint16_t kPlanMaxAt    = kEepromPlan + 6;  // uint16 LE, dm
constexpr uint16_t kPlanMinAt    = kEepromPlan + 8;  // uint16 LE, dm
static_assert(kEepromPlan + 10 == kEepromPlanEnd, "the plan is 10 bytes");

// Only a byte that changes is written: the plan changes once per pass.
void Put(uint16_t addr, uint8_t value) {
    if (EEPROM.read(addr) != value) EEPROM.write(addr, value);
}
void Put16(uint16_t addr, uint16_t value) {
    Put(addr, uint8_t(value & 0xFF));
    Put(addr + 1, uint8_t(value >> 8));
}
uint16_t Get16(uint16_t addr) {
    return uint16_t(EEPROM.read(addr)) | uint16_t(EEPROM.read(addr + 1)) << 8;
}
}  // namespace

WidthPlanning::WidthPlanning(ImplementPlough* implement, VehicleSensors* tractor,
                             GuidanceSource* guidance, IsobusTcInterface* tc)
    : implement(implement), tractor(tractor), guidance(guidance), tc(tc) {}

void WidthPlanning::Begin() {
    readSettings();
    readPlan();
}

void WidthPlanning::readSettings() {
    const uint8_t m  = EEPROM.read(kEepromPlannerMode);
    const uint8_t kk = EEPROM.read(kEepromPlannerK);
    const uint8_t r  = EEPROM.read(kEepromPlannerParallel);
    const uint8_t s  = EEPROM.read(kEepromPlannerSmoothing);
    const uint8_t lo = EEPROM.read(kEepromPlannerWminShare);
    const uint8_t hi = EEPROM.read(kEepromPlannerWmaxShare);
    mode       = IsValidMode(m) ? WidthMode(m) : WidthMode::Xte;
    k          = IsValidK(kk) ? kk : kDefaultPlannerK;
    parallel   = IsValidParallel(r) ? r : kDefaultPlannerParallel;
    smoothingM = IsValidSmoothing(s) ? s : kDefaultPlannerSmoothing;
    if (IsValidPerShare(lo) && IsValidPerShare(hi) && lo < hi) {
        wminPerShare = lo;
        wmaxPerShare = hi;
    } else {
        wminPerShare = kDefaultWminPerShareCm;
        wmaxPerShare = kDefaultWmaxPerShareCm;
    }
}

void WidthPlanning::readPlan() {
    if (EEPROM.read(kPlanMagicAt) != kPlanMagic) return;   // never written: no plan
    const uint8_t flags = EEPROM.read(kPlanFlagsAt);
    if ((flags & 0x03) > uint8_t(PlanState::Planned)) return;
    StoredPlan stored;
    stored.state       = PlanState(flags & 0x03);
    stored.passStarted = (flags & 0x04) != 0;
    stored.lastSide    = (flags & 0x08) != 0;
    stored.m           = EEPROM.read(kPlanMAt);
    stored.mleft       = EEPROM.read(kPlanMleftAt);
    stored.fieldIndex  = Get16(kPlanFieldAt);
    stored.firstMaxDm  = Get16(kPlanMaxAt);
    stored.firstMinDm  = Get16(kPlanMinAt);
    planner.Restore(stored);
}

void WidthPlanning::writePlan() {
    const StoredPlan& p = planner.GetStored();
    const uint8_t flags = uint8_t(p.state) | (p.passStarted ? 0x04 : 0) | (p.lastSide ? 0x08 : 0);
    // The magic goes last, once the rest is in place.
    Put(kPlanFlagsAt, flags);
    Put(kPlanMAt, p.m);
    Put(kPlanMleftAt, p.mleft);
    Put16(kPlanFieldAt, p.fieldIndex);
    Put16(kPlanMaxAt, p.firstMaxDm);
    Put16(kPlanMinAt, p.firstMinDm);
    Put(kPlanMagicAt, kPlanMagic);
    planner.ClearDirty();
}

WidthPlannerConfig WidthPlanning::GetConfig() const {
    WidthPlannerConfig c;
    c.mode           = mode;
    c.k              = k;
    c.parallelPasses = parallel;
    c.smoothingM     = smoothingM;
    c.wnomCm         = implement->GetOffset();
    c.wminCm         = int16_t(wminPerShare) * implement->GetShares();
    c.wmaxCm         = int16_t(wmaxPerShare) * implement->GetShares();
    return c;
}

bool WidthPlanning::HasTrackSupport() const {
    return tc->HasGuidanceTrackInfo() && tc->GetGuidanceTrackAgeMs() < kTrackMaxAgeMs &&
           tc->HasSwathWidth() && tc->GetSwathWidthMm() > 0;
}

TrackSample WidthPlanning::sample() const {
    TrackSample s;
    const unsigned long xteAt = guidance->GetXteTimestamp();
    const bool xteFresh = xteAt != 0 && (millis() - xteAt) <= kXteMaxAgeMs;
    s.valid      = HasTrackSupport() && xteFresh;
    s.track      = tc->GetActualTrackNumber();
    s.trackLeft  = tc->GetTrackNumberLeft();
    s.swathMm    = tc->GetSwathWidthMm();
    s.xteM       = guidance->GetXte() / 100.0f;   // GuidanceSource keeps cm
    s.fieldIndex = uint16_t(uint32_t(tc->GetAbLineId()) >> 16);
    return s;
}

void WidthPlanning::Update() {
    // One step per XTE fix, as ImplementPlough's setpoint.
    const unsigned long xteAt = guidance->GetXteTimestamp();
    if (xteAt != lastXteFix) {
        lastXteFix = xteAt;
        planner.Update(GetConfig(), sample(), !tractor->GetHitch(), implement->GetSide(),
                       guidance->GetSpeedMs(), millis());
        if (planner.IsDirty()) writePlan();
    }
    implement->SetPlannedWidth(planner.HasSetpoint(), planner.GetSetpointCm());
}

void WidthPlanning::NewField() {
    planner.NewField(sample().fieldIndex);
    writePlan();
}

void WidthPlanning::AdjustMleft(int delta) {
    planner.AdjustMleft(delta);
    if (planner.IsDirty()) writePlan();
}

void WidthPlanning::SetMode(WidthMode value) {
    if (!IsValidMode(int(value))) return;
    mode = value;
    Put(kEepromPlannerMode, uint8_t(value));
}

void WidthPlanning::SetK(uint8_t value) {
    if (!IsValidK(value)) return;
    k = value;
    Put(kEepromPlannerK, value);
}

void WidthPlanning::SetParallel(uint8_t value) {
    if (!IsValidParallel(value)) return;
    parallel = value;
    Put(kEepromPlannerParallel, value);
}

void WidthPlanning::SetSmoothingM(uint8_t value) {
    if (!IsValidSmoothing(value)) return;
    smoothingM = value;
    Put(kEepromPlannerSmoothing, value);
}

void WidthPlanning::SetWidthRangePerShare(uint8_t wmin, uint8_t wmax) {
    if (!IsValidPerShare(wmin) || !IsValidPerShare(wmax) || wmin >= wmax) return;
    wminPerShare = wmin;
    wmaxPerShare = wmax;
    Put(kEepromPlannerWminShare, wmin);
    Put(kEepromPlannerWmaxShare, wmax);
}

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
