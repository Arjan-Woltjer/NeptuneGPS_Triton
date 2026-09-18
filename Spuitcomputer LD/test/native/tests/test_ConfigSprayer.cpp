/*
  test_ConfigSprayer - Tests for ConfigSprayer: defaults, range validation, the GPS baud
  table and the minimum-fix-quality rule.
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
#include <AUnit.h>
#include "config/ConfigSprayer.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Defaults must reproduce the old compile-time behaviour exactly, so a board
// without a stored set keeps working as it did (NeptuneGPS_Triton#49).
// ---------------------------------------------------------------------------

test(ConfigSprayer, defaults_matchOldCompileTimeConstants) {
    ConfigSprayer cfg;
    assertEqual(cfg.Get().widthCm, 300);
    assertEqual(cfg.Get().guidanceTimeoutMs, (unsigned long)2000);
    assertEqual(cfg.Get().gpsBaudIndex, (uint8_t)7);
    assertEqual(cfg.Get().gpsMinQuality, (uint8_t)0);
    assertTrue(cfg.Get().buzzerEnabled);
}

test(ConfigSprayer, buzzer_canBeSwitchedOff) {
    ConfigSprayer cfg;
    cfg.SetBuzzerEnabled(false);
    assertFalse(cfg.Get().buzzerEnabled);
    cfg.SetBuzzerEnabled(true);
    assertTrue(cfg.Get().buzzerEnabled);
}

test(ConfigSprayer, load_offBoard_reportsNothingStored_keepsDefaults) {
    ConfigSprayer cfg;
    assertFalse(cfg.Load());
    assertEqual(cfg.Get().widthCm, 300);
}

// ---------------------------------------------------------------------------
// Setters validate and leave the value alone on bad input
// ---------------------------------------------------------------------------

test(ConfigSprayer, width_inRange_isStored) {
    ConfigSprayer cfg;
    assertTrue(cfg.SetWidthCm(600));
    assertEqual(cfg.Get().widthCm, 600);
    assertTrue(cfg.SetWidthCm(ConfigSprayer::kMinWidthCm));
    assertTrue(cfg.SetWidthCm(ConfigSprayer::kMaxWidthCm));
}

test(ConfigSprayer, width_outOfRange_rejectedAndUnchanged) {
    ConfigSprayer cfg;
    cfg.SetWidthCm(600);
    assertFalse(cfg.SetWidthCm(0));
    assertFalse(cfg.SetWidthCm(-300));
    assertFalse(cfg.SetWidthCm(ConfigSprayer::kMinWidthCm - 1));
    assertFalse(cfg.SetWidthCm(ConfigSprayer::kMaxWidthCm + 1));
    assertEqual(cfg.Get().widthCm, 600);
}

test(ConfigSprayer, guidanceTimeout_rangeChecked) {
    ConfigSprayer cfg;
    assertTrue(cfg.SetGuidanceTimeoutMs(1000));
    assertEqual(cfg.Get().guidanceTimeoutMs, (unsigned long)1000);
    assertFalse(cfg.SetGuidanceTimeoutMs(ConfigSprayer::kMinGuidanceMs - 1));
    assertFalse(cfg.SetGuidanceTimeoutMs(ConfigSprayer::kMaxGuidanceMs + 1));
    assertEqual(cfg.Get().guidanceTimeoutMs, (unsigned long)1000);
}

test(ConfigSprayer, gpsBaudIndex_rangeChecked) {
    ConfigSprayer cfg;
    assertTrue(cfg.SetGpsBaudIndex(3));
    assertEqual(cfg.Get().gpsBaudIndex, (uint8_t)3);
    assertFalse(cfg.SetGpsBaudIndex(ConfigSprayer::kMaxGpsBaudIndex + 1));
    assertFalse(cfg.SetGpsBaudIndex(255));
    assertEqual(cfg.Get().gpsBaudIndex, (uint8_t)3);
}

test(ConfigSprayer, gpsMinQuality_onlyKnownLevels) {
    ConfigSprayer cfg;
    assertTrue(cfg.SetGpsMinQuality(0));
    assertTrue(cfg.SetGpsMinQuality(1));
    assertTrue(cfg.SetGpsMinQuality(2));
    assertTrue(cfg.SetGpsMinQuality(4));
    assertEqual(cfg.Get().gpsMinQuality, (uint8_t)4);
    assertFalse(cfg.SetGpsMinQuality(3));   // not a GGA quality the sprayer knows
    assertFalse(cfg.SetGpsMinQuality(5));   // RTK float is a fix level, not a threshold
    assertFalse(cfg.SetGpsMinQuality(9));
    assertEqual(cfg.Get().gpsMinQuality, (uint8_t)4);
}

// ---------------------------------------------------------------------------
// Baud table: the 4800 x n table the Triton receivers are configured with
// ---------------------------------------------------------------------------

test(ConfigSprayer, baudFromIndex_matchesReceiverTable) {
    assertEqual(ConfigSprayer::BaudFromIndex(0), 4800L);
    assertEqual(ConfigSprayer::BaudFromIndex(1), 9600L);
    assertEqual(ConfigSprayer::BaudFromIndex(2), 14400L);
    assertEqual(ConfigSprayer::BaudFromIndex(3), 19200L);
    assertEqual(ConfigSprayer::BaudFromIndex(4), 28800L);
    assertEqual(ConfigSprayer::BaudFromIndex(5), 38400L);
    assertEqual(ConfigSprayer::BaudFromIndex(6), 57600L);
    assertEqual(ConfigSprayer::BaudFromIndex(7), 115200L);
}

test(ConfigSprayer, baudFromIndex_outOfRange_fallsBackTo115200) {
    // An index that slipped past validation (bad NVS) must not open the port
    // at 0 baud; 115200 is what every deployed board runs today.
    assertEqual(ConfigSprayer::BaudFromIndex(8), 115200L);
    assertEqual(ConfigSprayer::BaudFromIndex(255), 115200L);
}

// ---------------------------------------------------------------------------
// Minimum fix quality
// ---------------------------------------------------------------------------

test(ConfigSprayer, quality_minAny_acceptsEverythingIncludingNoFix) {
    // Default: today's behaviour, the sprayer doses on whatever arrives.
    ConfigSprayer cfg;
    assertTrue(cfg.GuidanceQualityOk(0));
    assertTrue(cfg.GuidanceQualityOk(1));
    assertTrue(cfg.GuidanceQualityOk(4));
}

test(ConfigSprayer, quality_minGps_rejectsNoFix) {
    ConfigSprayer cfg;
    cfg.SetGpsMinQuality(1);
    assertFalse(cfg.GuidanceQualityOk(0));
    assertTrue(cfg.GuidanceQualityOk(1));
    assertTrue(cfg.GuidanceQualityOk(2));
    assertTrue(cfg.GuidanceQualityOk(4));
    assertTrue(cfg.GuidanceQualityOk(5));
}

test(ConfigSprayer, quality_minDgps_acceptsDgpsAndBothRtk) {
    ConfigSprayer cfg;
    cfg.SetGpsMinQuality(2);
    assertFalse(cfg.GuidanceQualityOk(0));
    assertFalse(cfg.GuidanceQualityOk(1));
    assertTrue(cfg.GuidanceQualityOk(2));
    assertTrue(cfg.GuidanceQualityOk(4));
    assertTrue(cfg.GuidanceQualityOk(5));   // RTK float is better than DGPS
}

test(ConfigSprayer, quality_minRtk_acceptsFixedOnly) {
    ConfigSprayer cfg;
    cfg.SetGpsMinQuality(4);
    assertFalse(cfg.GuidanceQualityOk(0));
    assertFalse(cfg.GuidanceQualityOk(1));
    assertFalse(cfg.GuidanceQualityOk(2));
    assertTrue(cfg.GuidanceQualityOk(4));
    assertFalse(cfg.GuidanceQualityOk(5));  // RTK float: numerically higher, worse fix
}
