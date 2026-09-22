/*
  test_SerialGuidanceChannel - Tests for the shared SerialGuidanceChannel and
  GuidanceSource (MeijWorks Libs/VehicleGuidance), fed through a fake UART.
  Sentences match test/native/tests/known_good_sentences.txt.
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
#include <string.h>

#include "SerialGuidanceChannel.hpp"

using namespace aunit;
using namespace triton;

namespace
{

// A HardwareSerial whose receive side is a byte queue the test fills.
class FakeGpsSerial : public HardwareSerial {
public:
    void Feed(const char* s) { Feed((const uint8_t*)s, strlen(s)); }
    void Feed(const uint8_t* bytes, size_t n) {
        for (size_t i = 0; i < n && len < sizeof(buf); i++) buf[len++] = bytes[i];
    }
    int available() override { return int(len - pos); }
    int read() override      { return pos < len ? buf[pos++] : -1; }

private:
    uint8_t buf[512] = {};
    size_t  len = 0;
    size_t  pos = 0;
};

// A debug Stream that records what was written to it instead of printing.
class CaptureStream : public Stream {
public:
    size_t write(uint8_t c) override {
        if (len < sizeof(text) - 1) text[len++] = (char)c;
        text[len] = '\0';
        return 1;
    }
    char   text[512] = {};
    size_t len = 0;
};

bool near(float a, float b, float eps = 1e-4f) {
    float d = a - b;
    return d > -eps && d < eps;
}

const char* kGga = "$GPGGA,151503.00,5326.480207,N,00645.193203,E,2,09,1.0,44.05,M,0.00,M,,*67\r\n";
const char* kVtg = "$GPVTG,213.4,T,,M,002.91,N,005.39,K*61\r\n";
const char* kCanPos = "$0CFEF31C,00072A9C80652680*52\r\n";

// 0xBF '@' ROXTE,0.050 <hi> <lo> 0x10 0x03 -- the outer Trimble frame carries
// a 16-bit sum of every byte from '@' through the last data character
// ('@' + "ROXTE," + "0.050" = 753 = 2 * 256 + 241) in place of an NMEA '*XX'.
const uint8_t kTrimbleGood[] = { 191, '@', 'R', 'O', 'X', 'T', 'E', ',', '0', '.', '0', '5', '0', 2, 241, 16, 3 };
const uint8_t kTrimbleBad[]  = { 191, '@', 'R', 'O', 'X', 'T', 'E', ',', '0', '.', '0', '5', '0', 2, 240, 16, 3 };

}  // namespace

// ---------------------------------------------------------------------------
// GuidanceSource -- the one calibratable value, and the default state
// ---------------------------------------------------------------------------

test(GuidanceSource, rtk_quality_accepts_4_and_2_only) {
    GuidanceSource g;
    assertEqual((int)g.GetRtkQuality(), 4);
    g.SetRtkQuality(2);
    assertEqual((int)g.GetRtkQuality(), 2);
    g.SetRtkQuality(3);   // neither RTK fixed nor DGPS -> falls back to 4
    assertEqual((int)g.GetRtkQuality(), 4);
    g.SetRtkQuality(0);
    assertEqual((int)g.GetRtkQuality(), 4);
}

test(GuidanceSource, is_rtk_quality_compares_against_calibrated_value) {
    GuidanceSource g;
    g.SetQuality(4);
    assertTrue(g.IsRtkQuality());
    g.SetRtkQuality(2);
    assertFalse(g.IsRtkQuality());
    g.SetQuality(2);
    assertTrue(g.IsRtkQuality());
}

// ---------------------------------------------------------------------------
// SerialGuidanceChannel -- whole sentences through the character loop
// ---------------------------------------------------------------------------

test(SerialGuidanceChannel, gga_and_vtg_commit_to_source) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed(kGga);
    assertTrue(ch.Update());
    assertTrue(near(g.GetLatitude(), 53.4413f));
    assertTrue(near(g.GetLongitude(), 6.7532f));
    assertEqual((int)g.GetQuality(), 2);
    assertTrue(near(g.GetAltitude(), 44.05f));

    serial.Feed(kVtg);
    assertTrue(ch.Update());
    assertTrue(near(g.GetCourse(), 213.4f));
    assertTrue(near(g.GetSpeed(), 2.91f));
}

test(SerialGuidanceChannel, sentence_split_across_two_updates) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed("$GPVTG,213.4,T,,M,00");
    assertFalse(ch.Update());
    assertTrue(near(g.GetSpeed(), 0.0f));

    serial.Feed("2.91,N,005.39,K*61\r\n");
    assertTrue(ch.Update());
    assertTrue(near(g.GetSpeed(), 2.91f));
}

test(SerialGuidanceChannel, bad_nmea_checksum_is_not_committed) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed("$GPVTG,213.4,T,,M,002.91,N,005.39,K*62\r\n");   // should be *61
    assertFalse(ch.Update());
    assertTrue(near(g.GetSpeed(), 0.0f));
    assertEqual(g.GetVtgTimestamp(), (unsigned long)0);
}

test(SerialGuidanceChannel, can_over_serial_position) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed(kCanPos);
    assertTrue(ch.Update());
    assertTrue(near(g.GetLatitude(), 52.0f, 1e-5f));
    assertTrue(near(g.GetLongitude(), 5.0f, 1e-5f));
}

test(SerialGuidanceChannel, trimble_frame_commits_once_outer_checksum_passes) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed(kTrimbleGood, sizeof(kTrimbleGood));
    assertTrue(ch.Update());
    assertEqual(g.GetXte(), 5);
}

test(SerialGuidanceChannel, trimble_frame_with_bad_outer_checksum_is_not_committed) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    g.SetXte(-99);
    serial.Feed(kTrimbleBad, sizeof(kTrimbleBad));
    assertFalse(ch.Update());
    assertEqual(g.GetXte(), -99);
}

test(SerialGuidanceChannel, unknown_sentence_is_ignored) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed("$GPRMC,151503.00,A,5326.480207,N,00645.193203,E,2.91,213.4,140926,,,D*5A\r\n");
    assertFalse(ch.Update());
    assertTrue(near(g.GetLatitude(), 0.0f));
}

// ---------------------------------------------------------------------------
// Diagnostics: sentence tap, raw echo, runtime baudrate change
// ---------------------------------------------------------------------------

test(SerialGuidanceChannel, sentence_tap_keeps_last_line_and_counts) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    assertEqual(ch.GetSentenceSeq(), (uint32_t)0);
    assertEqual(ch.GetLastSentence(), "");

    serial.Feed(kGga);
    serial.Feed(kVtg);
    ch.Update();
    assertEqual(ch.GetSentenceSeq(), (uint32_t)2);
    assertEqual(ch.GetLastSentence(), "$GPVTG,213.4,T,,M,002.91,N,005.39,K*61");
}

test(SerialGuidanceChannel, sentence_tap_shows_trimble_frame_as_printable_remains) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed(kTrimbleGood, sizeof(kTrimbleGood));
    serial.Feed("\r\n");
    ch.Update();
    assertEqual(ch.GetSentenceSeq(), (uint32_t)1);
    assertEqual(ch.GetLastSentence(), "@ROXTE,0.050");
}

test(SerialGuidanceChannel, sentence_tap_caps_at_nmea_maximum) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    char line[130];
    line[0] = '$';
    for (int i = 1; i < 120; i++) line[i] = 'A';
    line[120] = '\r'; line[121] = '\n'; line[122] = '\0';
    serial.Feed(line);
    ch.Update();
    assertEqual(strlen(ch.GetLastSentence()), (size_t)90);
}

test(SerialGuidanceChannel, raw_echo_off_by_default_and_verbatim_when_on) {
    FakeGpsSerial serial;
    CaptureStream debug;
    GuidanceSource g;
    SerialGuidanceChannel ch(&debug, &serial, &g);

    assertFalse(ch.GetRawEcho());
    serial.Feed(kVtg);
    ch.Update();
    assertEqual(debug.len, (size_t)0);

    ch.SetRawEcho(true);
    assertTrue(ch.GetRawEcho());
    serial.Feed(kVtg);
    ch.Update();
    assertEqual(debug.text, kVtg);
}

test(SerialGuidanceChannel, raw_echo_with_no_debug_stream_does_not_crash) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    ch.SetRawEcho(true);
    serial.Feed(kVtg);
    assertTrue(ch.Update());
}

test(SerialGuidanceChannel, apply_baudrate_reopens_port_and_drops_partial_sentence) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed("$GPVTG,213.4,T,,M,00");   // read at the old rate, never completed
    ch.Update();

    ch.ApplyBaudrate(9600);
    assertEqual(serial.begunBaud, (unsigned long)9600);

    // The next complete sentence decodes cleanly instead of being glued to
    // the leftover term.
    serial.Feed(kVtg);
    assertTrue(ch.Update());
    assertTrue(near(g.GetSpeed(), 2.91f));
    assertEqual(ch.GetLastSentence(), "$GPVTG,213.4,T,,M,002.91,N,005.39,K*61");
}

test(SerialGuidanceChannel, apply_baudrate_rejects_zero_and_negative) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    ch.ApplyBaudrate(0);
    ch.ApplyBaudrate(-4800);
    assertEqual(serial.begunBaud, (unsigned long)0);
}

// ---------------------------------------------------------------------------
// GPTXT through the character loop -- NeptuneGPS_Triton#61 case 7
// ---------------------------------------------------------------------------
//
// The ATGM336H's antenna supervisor is the first sentence handled here whose
// text contains a space, and the tokenizer used to bitbucket ' ' outright.
// That broke it twice over: the space never reached parity, so the sentence's
// real checksum (*25) never matched the 0x05 the channel computed and the
// whole sentence was dropped before commitTo(); and the space never reached
// term[], so NmeaParser compared "ANTENNAOPEN" against "ANTENNA OPEN".
//
// The existing NmeaParser TXT tests call parseTerm(4, "ANTENNA OPEN")
// directly, so they exercised neither half. On the #61 field log the pump ran
// on through a real antenna pull, duty climbing, while the console showed
// ANTENNA OPEN arriving on schedule.
//
// Trimble frames still bitbucket the space: TrimbleParser uses this same
// parity as its own checksum (useParityAsChecksum()), and that decode is
// already proven on the rig -- see the trimble_frame_* tests above, which
// pin the behaviour this fix deliberately leaves alone.

test(SerialGuidanceChannel, gptxt_antenna_open_survives_the_space_in_its_text) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    assertTrue(g.GetAntennaOk());   // default: assume the antenna is fine

    serial.Feed("$GPTXT,01,01,01,ANTENNA OPEN*25\r\n");
    assertTrue(ch.Update());        // checksum must verify, not just parse
    assertFalse(g.GetAntennaOk());
}

test(SerialGuidanceChannel, gptxt_antenna_short_survives_the_space_in_its_text) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    serial.Feed("$GPTXT,01,01,01,ANTENNA SHORT*63\r\n");
    assertTrue(ch.Update());
    assertFalse(g.GetAntennaOk());
}

test(SerialGuidanceChannel, gptxt_antenna_ok_clears_the_flag_again) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    g.SetAntennaOk(false);          // a prior OPEN already latched this

    serial.Feed("$GPTXT,01,01,01,ANTENNA OK*35\r\n");
    assertTrue(ch.Update());
    assertTrue(g.GetAntennaOk());
}

// A space inside the text must not be silently dropped from the term either:
// an unrelated TXT banner still has to leave the flag alone rather than be
// mangled into one of the antenna strings.
test(SerialGuidanceChannel, gptxt_unrelated_banner_leaves_the_flag_alone) {
    FakeGpsSerial serial;
    GuidanceSource g;
    SerialGuidanceChannel ch(nullptr, &serial, &g);

    g.SetAntennaOk(false);

    serial.Feed("$GPTXT,01,01,02,ANTENNA IS FINE*0C\r\n");
    assertTrue(ch.Update());
    assertFalse(g.GetAntennaOk());   // untouched, not reset to the default
}
