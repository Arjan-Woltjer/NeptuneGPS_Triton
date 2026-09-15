/*
  test_CanFrameGuidanceChannel - Tests for the shared raw-CAN-frame guidance
  channel (MeijWorks Libs/VehicleGuidance): the four ACAN_T4 filter ids the
  Teensy projects accept, the standard NMEA2000 PGNs, and the identifier
  field extraction. Payloads match known_good_sentences.txt and
  test_IsobusPgnDecode.cpp.
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

#include "CanFrameGuidanceChannel.hpp"

using namespace aunit;
using namespace triton;

namespace
{
bool near(float a, float b, float eps = 1e-3f) {
    float d = a - b;
    return d > -eps && d < eps;
}

// The four extended ids main.cpp's ACAN_T4 filters accept (priority bits as
// the senders use them; PgnFromId() ignores them anyway).
constexpr uint32_t kIdLegacyPosition = 0x0CFEF31C;
constexpr uint32_t kIdLegacySpeed    = 0x0CFEE81C;
constexpr uint32_t kIdJohnDeereXte   = 0x0CFFFF2A;
constexpr uint32_t kIdTrimbleXte     = 0x1CEBACAA;
}  // namespace

// ---------------------------------------------------------------------------
// Identifier fields
// ---------------------------------------------------------------------------

test(CanFrameGuidanceChannel, pgn_and_source_address_from_id) {
    assertEqual(CanFrameGuidanceChannel::PgnFromId(kIdLegacyPosition), (uint32_t)0xFEF3);   // PDU2: PS is part of the PGN
    assertEqual(CanFrameGuidanceChannel::PgnFromId(0x18FEF31C), (uint32_t)0xFEF3);          // other priority, same PGN
    assertEqual(CanFrameGuidanceChannel::PgnFromId(kIdJohnDeereXte), (uint32_t)0xFFFF);
    assertEqual(CanFrameGuidanceChannel::PgnFromId(kIdTrimbleXte), (uint32_t)0xEB00);       // PDU1: PS is a destination
    assertEqual(CanFrameGuidanceChannel::PgnFromId(0x09F8011C), (uint32_t)129025);          // data page 1
    assertEqual(CanFrameGuidanceChannel::SourceAddressFromId(kIdJohnDeereXte), (uint8_t)0x2A);
    assertEqual(CanFrameGuidanceChannel::SourceAddressFromId(kIdTrimbleXte), (uint8_t)0xAA);
}

// ---------------------------------------------------------------------------
// Legacy proprietary frames, as VehicleGps::Update(id, data, len) took them
// ---------------------------------------------------------------------------

// $0CFEF31C,00072A9C80652680 -> lat 52.0 N, lon 5.0 E
test(CanFrameGuidanceChannel, legacy_position) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x00, 0x07, 0x2A, 0x9C, 0x80, 0x65, 0x26, 0x80 };
    millisValue(1234);
    assertTrue(ch.Update(kIdLegacyPosition, d, 8));
    assertTrue(near(g.GetLatitude(), 52.0f, 1e-5f));
    assertTrue(near(g.GetLongitude(), 5.0f, 1e-5f));
    assertEqual(g.GetGgaTimestamp(), (unsigned long)1234);
}

// $0CFEE81C,002D00020000804F -> course 90.0, speed 2.0 kt, altitude 44.0 m
test(CanFrameGuidanceChannel, legacy_speed_course_altitude) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x00, 0x2D, 0x00, 0x02, 0x00, 0x00, 0x80, 0x4F };
    millisValue(50);
    assertTrue(ch.Update(0x18FEE81C, d, 8));   // the other priority the filter mask admits
    assertTrue(near(g.GetCourse(), 90.0f));
    assertTrue(near(g.GetSpeed(), 2.0f));
    assertTrue(near(g.GetAltitude(), 44.0f));
    assertEqual(g.GetVtgTimestamp(), (unsigned long)50);
}

test(CanFrameGuidanceChannel, legacy_speed_not_available_is_skipped) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    g.SetSpeedKnots(3.0f);
    const uint8_t d[8] = { 0x00, 0x2D, 0xFF, 0xFF, 0x00, 0x00, 0x80, 0x4F };
    assertTrue(ch.Update(kIdLegacySpeed, d, 8));   // course and altitude still land
    assertTrue(near(g.GetSpeed(), 3.0f));
    assertTrue(near(g.GetCourse(), 90.0f));
}

// John Deere XTE: selector 0x77, quality nibble 0x1 -> RTK, xte 0
test(CanFrameGuidanceChannel, john_deere_xte) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x77, 0x10, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    millisValue(77);
    assertTrue(ch.Update(kIdJohnDeereXte, d, 8));
    assertEqual(g.GetXte(), 0);
    assertEqual((int)g.GetQuality(), 4);
    assertEqual(g.GetXteTimestamp(), (unsigned long)77);
}

test(CanFrameGuidanceChannel, john_deere_other_selector_is_ignored) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    g.SetXte(-42);
    const uint8_t d[8] = { 0x92, 0xFF, 0x80, 0x3E, 0xFC, 0xFF, 0xFF, 0xFF };   // the 1 Hz 0x92 message, #30
    assertFalse(ch.Update(kIdJohnDeereXte, d, 8));
    assertEqual(g.GetXte(), -42);
}

test(CanFrameGuidanceChannel, john_deere_pgn_from_other_source_is_ignored) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x77, 0x10, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 };
    assertFalse(ch.Update(0x0CFFFF80, d, 8));   // Ag Leader/Raven address, #20
    assertEqual(g.GetXteTimestamp(), (unsigned long)0);
}

// Trimble/CNH XTE: byte 0 == 2, byte 5 == 7, bytes 1-4 a big-endian float
test(CanFrameGuidanceChannel, trimble_xte) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x02, 0x3D, 0x4C, 0xCC, 0xCD, 0x07, 0x00, 0x00 };   // 0.05f
    assertTrue(ch.Update(kIdTrimbleXte, d, 8));
    assertEqual(g.GetXte(), 5);
    assertEqual((int)g.GetQuality(), 4);
}

test(CanFrameGuidanceChannel, trimble_xte_wrong_marker_bytes_is_ignored) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x01, 0x3D, 0x4C, 0xCC, 0xCD, 0x07, 0x00, 0x00 };
    assertFalse(ch.Update(kIdTrimbleXte, d, 8));
}

test(CanFrameGuidanceChannel, short_frame_is_ignored) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[7] = { 0x00, 0x07, 0x2A, 0x9C, 0x80, 0x65, 0x26 };
    assertFalse(ch.Update(kIdLegacyPosition, d, 7));
    assertEqual(g.GetGgaTimestamp(), (unsigned long)0);
}

test(CanFrameGuidanceChannel, unrelated_pgn_is_ignored) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    assertFalse(ch.Update(0x18EEFF1C, d, 8));   // address claim
    assertFalse(ch.Update(0x0CF00400, d, 8));   // EEC1
}

// ---------------------------------------------------------------------------
// Standard NMEA2000 PGNs, the same frames the ISOBUS build subscribes to
// ---------------------------------------------------------------------------

test(CanFrameGuidanceChannel, nmea2000_position) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[8] = { 0x00, 0x92, 0xFE, 0x1E, 0x80, 0xF0, 0xFA, 0x02 };   // 52.0 N, 5.0 E
    assertTrue(ch.Update(0x09F8011C, d, 8));
    assertTrue(near(g.GetLatitude(), 52.0f, 1e-5f));
    assertTrue(near(g.GetLongitude(), 5.0f, 1e-5f));
}

test(CanFrameGuidanceChannel, nmea2000_speed_and_course) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    const uint8_t d[6] = { 0, 0, 0x5C, 0x3D, 0xE8, 0x03 };   // COG 1.5708 rad, SOG 10.00 m/s
    assertTrue(ch.Update(0x09F8021C, d, 6));
    assertTrue(near(g.GetSpeedMs(), 10.0f, 0.01f));
    assertTrue(near(g.GetCourse(), 90.0f, 0.01f));
}

test(CanFrameGuidanceChannel, nmea2000_xte_without_quality) {
    GuidanceSource g;
    CanFrameGuidanceChannel ch(&g);
    g.SetQuality(4);
    const uint8_t d[6] = { 0, 0x00, 0x7B, 0x00, 0x00, 0x00 };   // 123 hundredths
    assertTrue(ch.Update(0x09F9031C, d, 6));
    assertEqual(g.GetXte(), 123);
    assertEqual((int)g.GetQuality(), 4);   // this PGN carries none; the stored one stands
}
