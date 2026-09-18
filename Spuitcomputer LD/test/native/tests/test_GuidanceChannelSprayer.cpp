/*
  test_GuidanceChannelSprayer - The shared guidance channel as the sprayer
  builds it: GN-talker and XTE sentences, hemisphere signs, a Trimble frame,
  the raw echo, a baudrate change mid-sentence, CAN-serial and NMEA2000 bridge
  lines, and the interface's button edge logging (NeptuneGPS_Triton#87).
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
#include <cstdio>
#include <string>

#include <AUnit.h>
#include "InterfaceSprayer.hpp"
#include "SerialGuidanceChannel.hpp"

using namespace aunit;
using namespace triton;

// ---------------------------------------------------------------------------
// Fixtures: a receiver port the test fills with bytes, and a debug port that
// captures what the channel echoes.
// ---------------------------------------------------------------------------

struct GpsPort : public HardwareSerial {
    std::string bytes;
    size_t      pos = 0;
    void Feed(const std::string& s) { bytes += s; }
    void FeedBytes(const uint8_t* b, size_t n) { bytes.append((const char*)b, n); }
    int available() override { return int(bytes.size() - pos); }
    int read() override      { return pos < bytes.size() ? (uint8_t)bytes[pos++] : -1; }
};

struct CaptureStream : public Stream {
    std::string out;
    size_t write(uint8_t c) override { out.push_back((char)c); return 1; }
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
};

// "$" + body + "*XX\r\n" with the NMEA checksum computed, so a test never
// carries a hand-typed checksum that silently makes it a no-op.
static std::string nmea(const char* body) {
    uint8_t cs = 0;
    for (const char* p = body; *p; ++p) cs ^= (uint8_t)*p;
    char tail[8];
    snprintf(tail, sizeof(tail), "*%02X\r\n", cs);
    return std::string("$") + body + tail;
}

static bool near(float a, float b, float eps = 1e-3f) { return (a - b) < eps && (b - a) < eps; }

// From Ploegbesturing Isobus' test_SerialGuidanceChannel.cpp: 0xBF '@' ROXTE,0.050
// followed by the outer checksum (hi, lo), 0x10, 0x03.
static const uint8_t kTrimbleGood[] = { 191, '@', 'R', 'O', 'X', 'T', 'E', ',', '0', '.', '0', '5', '0', 2, 241, 16, 3 };
static const uint8_t kTrimbleBad[]  = { 191, '@', 'R', 'O', 'X', 'T', 'E', ',', '0', '.', '0', '5', '0', 2, 240, 16, 3 };

// ---------------------------------------------------------------------------
// NMEA talkers, hemispheres, XTE
// ---------------------------------------------------------------------------

test(GuidanceChannelSprayer, gn_talker_gga_and_vtg_commit) {
    millisValue(1234);
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.Feed(nmea("GNGGA,123519,5230.000,N,00500.000,E,4,08,0.9,45.0,M,,M,,"));
    port.Feed(nmea("GNVTG,90.0,T,,M,2.0,N,3.7,K"));
    assertTrue(ch.Update());
    assertTrue(near(g.GetLatitude(), 52.5f));
    assertTrue(near(g.GetLongitude(), 5.0f));
    assertEqual((int)g.GetQuality(), 4);
    assertTrue(near(g.GetAltitude(), 45.0f));
    assertTrue(near(g.GetCourse(), 90.0f));
    assertTrue(near(g.GetSpeed(), 2.0f));
    assertEqual(g.GetGgaTimestamp(), (unsigned long)1234);
    assertEqual(g.GetVtgTimestamp(), (unsigned long)1234);
}

test(GuidanceChannelSprayer, south_and_west_are_negative) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.Feed(nmea("GPGGA,123519,5230.000,S,00500.000,W,1,08,0.9,45.0,M,,M,,"));
    ch.Update();
    assertTrue(near(g.GetLatitude(), -52.5f));
    assertTrue(near(g.GetLongitude(), -5.0f));
}

test(GuidanceChannelSprayer, gga_without_fix_commits_quality_zero_and_stamps) {
    millisValue(777);
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    g.SetQuality(4);
    port.Feed(nmea("GPGGA,123519,,,,,0,00,,,M,,M,,"));
    ch.Update();
    assertEqual((int)g.GetQuality(), 0);
    assertEqual(g.GetGgaTimestamp(), (unsigned long)777);
}

test(GuidanceChannelSprayer, xte_sentences_commit_hundredths_of_a_metre) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.Feed(nmea("GPXTE,A,A,0.67,L,N"));
    ch.Update();
    assertEqual(g.GetXte(), 67);
    port.Feed(nmea("GNXTE,A,A,1.25,R,N"));
    ch.Update();
    assertEqual(g.GetXte(), 125);
}

test(GuidanceChannelSprayer, bad_checksum_commits_nothing) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.Feed("$GPVTG,90.0,T,,M,2.0,N,3.7,K*00\r\n");
    assertFalse(ch.Update());
    assertTrue(near(g.GetSpeed(), 0.0f));
    assertEqual(g.GetVtgTimestamp(), (unsigned long)0);
}

// ---------------------------------------------------------------------------
// The other parsers the sprayer build links: Trimble framing, CAN-serial and
// NMEA2000 bridge lines. One each, so this build proves its own parser set.
// ---------------------------------------------------------------------------

test(GuidanceChannelSprayer, trimble_frame_commits_only_with_good_outer_checksum) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.FeedBytes(kTrimbleBad, sizeof(kTrimbleBad));
    assertFalse(ch.Update());
    assertEqual(g.GetXte(), 0);
    port.FeedBytes(kTrimbleGood, sizeof(kTrimbleGood));
    assertTrue(ch.Update());
    assertEqual(g.GetXte(), 5);
}

test(GuidanceChannelSprayer, can_serial_legacy_speed_line) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.Feed(nmea("0CFEE81C,002D00020000804F"));
    assertTrue(ch.Update());
    assertTrue(near(g.GetCourse(), 90.0f, 0.01f));
    assertTrue(near(g.GetSpeed(), 2.0f, 0.01f));
    assertTrue(near(g.GetAltitude(), 44.0f, 0.01f));
}

test(GuidanceChannelSprayer, nmea2000_cog_sog_bridge_line) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.Feed(nmea("1DF8021C,00005C3D6700FFFF"));
    assertTrue(ch.Update());
    assertTrue(near(g.GetCourse(), 90.0f, 0.01f));
    assertTrue(near(g.GetSpeed(), 2.0f, 0.01f));
}

// ---------------------------------------------------------------------------
// Raw echo and a baudrate change mid-sentence
// ---------------------------------------------------------------------------

test(GuidanceChannelSprayer, raw_echo_mirrors_every_byte_to_debug_while_on) {
    GuidanceSource g; GpsPort port; CaptureStream dbg; SerialGuidanceChannel ch(&dbg, &port, &g);
    const std::string s = nmea("GPVTG,90.0,T,,M,2.0,N,3.7,K");
    assertFalse(ch.GetRawEcho());
    port.Feed(s);
    ch.Update();
    assertEqual(dbg.out.size(), (size_t)0);

    ch.SetRawEcho(true);
    port.Feed(s);
    ch.Update();
    assertEqual(dbg.out.c_str(), s.c_str());

    ch.SetRawEcho(false);
    port.Feed(s);
    ch.Update();
    assertEqual(dbg.out.c_str(), s.c_str());   // nothing added
}

test(GuidanceChannelSprayer, baudrate_change_discards_the_partial_sentence) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    const std::string full = nmea("GPVTG,90.0,T,,M,2.0,N,3.7,K");
    const size_t cut = 20;                       // "$GPVTG,90.0,T,,M,2.0"
    port.Feed(full.substr(0, cut));
    ch.Update();

    ch.ApplyBaudrate(19200);
    assertEqual(port.begunBaud, (unsigned long)19200);

    // The rest of the old sentence arrives at the new rate: with the parser
    // state cleared it cannot complete into a commit.
    port.Feed(full.substr(cut));
    assertFalse(ch.Update());
    assertTrue(near(g.GetSpeed(), 0.0f));
    assertEqual(g.GetVtgTimestamp(), (unsigned long)0);

    millisValue(50);
    port.Feed(full);
    assertTrue(ch.Update());
    assertTrue(near(g.GetSpeed(), 2.0f));
    assertEqual(g.GetVtgTimestamp(), (unsigned long)50);
}

test(GuidanceChannelSprayer, apply_baudrate_ignores_nonsense) {
    GuidanceSource g; GpsPort port; SerialGuidanceChannel ch(nullptr, &port, &g);
    port.begunBaud = 4800;
    ch.ApplyBaudrate(0);
    ch.ApplyBaudrate(-9600);
    assertEqual(port.begunBaud, (unsigned long)4800);
}

// ---------------------------------------------------------------------------
// InterfaceSprayer button edges are logged when a debug port is attached
// ---------------------------------------------------------------------------

test(GuidanceChannelSprayer, interface_logs_button_edges_on_debug_port) {
    CaptureStream dbg;
    InterfaceSprayer iface(&dbg);
    const uint8_t pin = iface.buttons[0].pin;
    char expectLow[32], expectHigh[32];
    snprintf(expectLow,  sizeof(expectLow),  "Button pin %d: LOW",  pin);
    snprintf(expectHigh, sizeof(expectHigh), "Button pin %d: HIGH", pin);

    // Released (pull-up HIGH) first so the flag is set, then pressed.
    for (int i = 0; i < NUM_DIGITAL_IN; ++i) digitalReadValue(iface.buttons[i].pin, true);
    millisValue(0);
    iface.Update();
    assertFalse(dbg.has(expectLow));

    digitalReadValue(pin, false);
    millisValue(10);
    iface.Update();
    assertTrue(dbg.has(expectLow));
    assertFalse(iface.buttons[0].state);         // not yet held long enough
    millisValue(40);
    iface.Update();
    assertTrue(iface.buttons[0].state);

    digitalReadValue(pin, true);
    millisValue(50);
    iface.Update();
    assertTrue(dbg.has(expectHigh));
    assertFalse(iface.buttons[0].state);
}
