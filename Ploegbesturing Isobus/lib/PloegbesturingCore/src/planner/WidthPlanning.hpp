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
#pragma once

// The track data comes from the Task Controller, so ISOBUS only; built for
// the board and for [env:native].
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

#include "WidthPlanner.hpp"
#include "GuidanceSource.hpp"
#include "VehicleSensors.hpp"
#include "../implement/ImplementPlough.hpp"
#include "../isobus/IsobusTcInterface.hpp"

namespace triton
{

// EEPROM 170-187: the planner's settings (170-175) and the plan (176-187).
// Free between the scraper's 130-169 and the AUX-N assignments from 190;
// still to be checked against the EEPROM memory map.
constexpr uint16_t kEepromPlannerMode       = 170;  // WidthMode, 0-2
constexpr uint16_t kEepromPlannerK          = 171;  // passes, 1-30
constexpr uint16_t kEepromPlannerParallel   = 172;  // R, 0-20
constexpr uint16_t kEepromPlannerSmoothing  = 173;  // m, 1-50
constexpr uint16_t kEepromPlannerWminShare  = 174;  // cm per share, 20-60
constexpr uint16_t kEepromPlannerWmaxShare  = 175;  // cm per share, 20-60
constexpr uint16_t kEepromPlan              = 176;  // the plan, 12 bytes
constexpr uint16_t kEepromPlanEnd           = 188;

// Defaults: a straight plough until the operator picks a mode; the width
// range the width sensor is calibrated over (34-50 cm per share).
constexpr uint8_t kDefaultPlannerK         = 10;
constexpr uint8_t kDefaultPlannerParallel  = 1;
constexpr uint8_t kDefaultPlannerSmoothing = 3;
constexpr uint8_t kDefaultWminPerShareCm   = 34;
constexpr uint8_t kDefaultWmaxPerShareCm   = 50;

class WidthPlanning {
public:
    // Fresh enough: the TC sends the track data every 250 ms, and the
    // plough's own gate wants an XTE no older than 2 s.
    static constexpr unsigned long kTrackMaxAgeMs = 3000;
    static constexpr unsigned long kXteMaxAgeMs   = 2000;

    WidthPlanning(ImplementPlough* implement, VehicleSensors* tractor,
                  GuidanceSource* guidance, IsobusTcInterface* tc);

    // Reads the settings and the plan from EEPROM. Once, from setup().
    void Begin();

    // Every loop() pass, before InterfacePlough::Update(): on a new XTE fix
    // the planner steps, and ImplementPlough gets the planned width (or none).
    void Update();

    // The operator's controls: the field's ends (WidthPlanner::RecordEnd()),
    // and a pass the counter missed or counted twice.
    EndResult RecordEnd(uint8_t end);
    void AdjustMleft(int delta);

    // Where the plough is, and the ends, as passes to the reference at the
    // TC's track spacing, in tenths (374 = 37.4 passes); -1 when unknown.
    int32_t GetPassNowTenths() const;
    int32_t GetPassAtEndTenths(uint8_t end) const;

    // The settings (step 3 puts them in PloughSettings).
    WidthMode GetMode() const       { return mode; }
    uint8_t   GetK() const          { return k; }
    uint8_t   GetParallel() const   { return parallel; }
    uint8_t   GetSmoothingM() const { return smoothingM; }
    uint8_t   GetWminPerShare() const { return wminPerShare; }
    uint8_t   GetWmaxPerShare() const { return wmaxPerShare; }
    void SetMode(WidthMode value);
    void SetK(uint8_t value);
    void SetParallel(uint8_t value);
    void SetSmoothingM(uint8_t value);
    void SetWidthRangePerShare(uint8_t wmin, uint8_t wmax);

    // Track support: a fresh track number and spacing from the TC. Without
    // it only Xte makes sense, and the settings show only that.
    bool HasTrackSupport() const;
    // The track spacing the TC sent (DDI 512), mm; 0 without track support.
    int32_t GetTrackSpacingMm() const { return HasTrackSupport() ? tc->GetSwathWidthMm() : 0; }
    // The plough's main width (ImplementPlough's offset) and share count.
    int16_t GetMainWidthCm() const { return implement->GetOffset(); }
    uint8_t GetShares() const { return implement->GetShares(); }
    int16_t GetWidthNowCm() const { return implement->GetPosition(); }

    const WidthPlanner& GetPlanner() const { return planner; }
    WidthPlannerConfig  GetConfig() const;

    // The validation the boot load applies, for PloughSettings.
    static bool IsValidMode(int v)       { return v >= 0 && v <= 2; }
    static bool IsValidK(int v)          { return v >= 1 && v <= 30; }
    static bool IsValidParallel(int v)   { return v >= 0 && v <= 20; }
    static bool IsValidSmoothing(int v)  { return v >= 1 && v <= 50; }
    static bool IsValidPerShare(int v)   { return v >= 20 && v <= 60; }

private:
    ImplementPlough*   implement;
    VehicleSensors*    tractor;
    GuidanceSource*    guidance;
    IsobusTcInterface* tc;

    WidthPlanner  planner;
    WidthMode     mode         = WidthMode::Xte;
    uint8_t       k            = kDefaultPlannerK;
    uint8_t       parallel     = kDefaultPlannerParallel;
    uint8_t       smoothingM   = kDefaultPlannerSmoothing;
    uint8_t       wminPerShare = kDefaultWminPerShareCm;
    uint8_t       wmaxPerShare = kDefaultWmaxPerShareCm;
    unsigned long lastXteFix   = 0;

    TrackSample sample() const;
    int32_t PassesTenths(float landM) const;
    void readSettings();
    void readPlan();
    void writePlan();
};

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
