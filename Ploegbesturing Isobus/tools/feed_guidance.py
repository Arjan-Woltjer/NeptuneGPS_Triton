#!/usr/bin/env python3
"""Feed a serial-guidance plough control through a USB-to-serial (FTDI) adapter.

Three formats, the three the firmware's SerialGuidanceChannel parses:

  jd       John Deere CAN frames as serial CAN lines, exactly as the JD CAN-to-serial
           converter (MeijWorks Programs/Can_converter_JD_UNO) forwards them, 115200 baud:
           "$<29-bit id>,<8 bytes hex>*<NMEA XOR>": 0CFEF31C position, 0CFEE81C
           course/speed/altitude, 0CFFFF2A cross-track error
  nmea     $GPGGA, $GPVTG and $GPXTE. --cnh-xte sends $GPXTE the way the CNH XTE converter
           (MeijWorks Programs/CAN_v3_2_CNH_XTE) does instead of a standard receiver
  trimble  $GPGGA and $GPVTG, plus the cross-track error as a Trimble @ROXTE frame

    py feed_guidance.py --mode jd                      # first FTDI adapter, 115200 baud, 1 Hz
    py feed_guidance.py --mode nmea --port COM6 --baud 38400
    py feed_guidance.py --mode trimble --xte-fixed -0.12
    py feed_guidance.py --list                         # serial ports, FTDI ones marked
    py feed_guidance.py --selftest                     # encoders against the firmware's fixtures

The cross-track error follows a slow sine (+-xte-amp metres over period seconds) so the
display moves; --xte-fixed <m> sends a constant instead (negative = left). Every message is
echoed to stdout. Ctrl-C stops it.

Baud rate: 115200 for jd (the converter's), 4800 for nmea and trimble, --baud overrides.
The plough autodetects 4800-115200 at boot (4800 x 1, 2, 3, 4, 6, 8, 12, 24), on the
regular Ploegbesturing and, from NeptuneGPS_Triton#214, on the Ploegbesturing Isobus serial
builds too; a rate outside that list is never found. Needs pyserial.

The NMEA sentences copy a real receiver log (2026, the source of known_good_sentences.txt):
GGA with six-decimal minutes, VTG with zero-padded speeds, and $GPXTE with a SIGNED value
and the side as well, L for zero and up, R below ("0.017374,L" / "-0.008822,R"). The
firmware's NMEA parser reads the sign from the number; the L/R is redundant.

Wire formats, as the firmware decodes them (MeijWorks Libs/VehicleGuidance):
  0CFEF31C  bytes 0-3 latitude, 4-7 longitude: little-endian uint32, 1e-7 deg + 2,100,000,000
  0CFEE81C  J1939 PGN 65256: bytes 0-1 course 1/128 deg, 2-3 speed 1/256 km/h,
            4-5 pitch 1/128 deg - 200, 6-7 altitude 1/8 m - 2500 (all little-endian).
            The firmware reads bytes 2-3 as knots (NeptuneGPS_Triton#204, fix in #205), so until
            that lands the plough shows 1.852x the speed sent here; --speed-as-knots sends what
            today's firmware expects instead of what a tractor sends.
  0CFFFF2A  byte 0 = 0x77 (the XTE message selector), byte 1 high nibble 1 = RTK,
            bytes 3-4 = 32000 + 2 * XTE in cm (little-endian)
  @ROXTE    0xBF '@' "ROXTE,<metres>" <sum hi> <sum lo> 0x10 0x03, where the sum is the 16-bit
            total of every byte from '@' through the last value character
"""
import argparse
import math
import struct
import sys
import time
from datetime import datetime, timezone

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # --selftest runs without it
    serial = None
    list_ports = None

FTDI_VID = 0x0403

# Bytes SerialGuidanceChannel treats as framing or control rather than data. A Trimble
# checksum byte equal to one of these would break the frame on the receiving end.
TRIMBLE_UNSAFE = {0, 10, 13, 20, 32, 36, 42, 44, 58, 64, 191}


# ---------------------------------------------------------------------------
# Encoders
# ---------------------------------------------------------------------------

def nmea_checksum(body: str) -> str:
    c = 0
    for ch in body:
        c ^= ord(ch)
    return f"{c:02X}"


def nmea_sentence(body: str) -> bytes:
    return f"${body}*{nmea_checksum(body)}\r\n".encode("ascii")


def serial_can(can_id: int, data: bytes) -> bytes:
    """One CAN frame as the serial CAN line a CAN-to-serial bridge forwards."""
    assert len(data) == 8
    return nmea_sentence(f"{can_id:08X},{data.hex().upper()}")


def jd_position(lat: float, lon: float) -> bytes:
    bias = 2_100_000_000
    return serial_can(0x0CFEF31C, struct.pack("<II", round(lat * 1e7) + bias, round(lon * 1e7) + bias))


def jd_speed(course_deg: float, speed_raw_per_256: float, alt_m: float) -> bytes:
    course = round(course_deg * 128) & 0xFFFF
    speed = min(round(speed_raw_per_256 * 256), 0xFAFF)
    pitch = 200 * 128                       # 0 deg
    alt = round((alt_m + 2500) * 8) & 0xFFFF
    return serial_can(0x0CFEE81C, struct.pack("<HHHH", course, speed, pitch, alt))


def jd_xte(xte_m: float, rtk: bool) -> bytes:
    raw = 32000 + 2 * round(xte_m * 100)
    raw = max(0, min(raw, 0xFFFF))
    data = bytes([0x77, 0x10 if rtk else 0x00, 0x00]) + struct.pack("<H", raw) + b"\xFF\xFF\xFF"
    return serial_can(0x0CFFFF2A, data)


def nmea_lat(lat: float) -> tuple[str, str]:
    hemi = "N" if lat >= 0 else "S"
    lat = abs(lat)
    deg = int(lat)
    return f"{deg:02d}{(lat - deg) * 60:09.6f}", hemi


def nmea_lon(lon: float) -> tuple[str, str]:
    hemi = "E" if lon >= 0 else "W"
    lon = abs(lon)
    deg = int(lon)
    return f"{deg:03d}{(lon - deg) * 60:09.6f}", hemi


def gpgga(hms: str, lat: float, lon: float, quality: int, sats: int, alt: float) -> bytes:
    lat_s, ns = nmea_lat(lat)
    lon_s, ew = nmea_lon(lon)
    return nmea_sentence(f"GPGGA,{hms},{lat_s},{ns},{lon_s},{ew},{quality},{sats:02d},1.0,"
                         f"{alt:.2f},M,0.00,M,,")


def gpvtg(course: float, knots: float, kmh: float) -> bytes:
    return nmea_sentence(f"GPVTG,{course:.1f},T,,M,{knots:06.2f},N,{kmh:06.2f},K")


def gpxte(xte_m: float) -> bytes:
    """As the real receiver sends it: signed metres, six decimals, L for >= 0 and R below,
    unit N in name only (the value is metres)."""
    side = "L" if xte_m >= 0 else "R"
    return nmea_sentence(f"GPXTE,A,A,{xte_m:.6f},{side},N")


def gpxte_cnh(xte_m: float) -> bytes:
    """As CAN_v3_2_CNH_XTE's prepMessage() builds it: whole centimetres as signed metres with
    two decimals, 'L' for positive and 'R' for negative, unit 'M'."""
    cm = int(xte_m * 100)                  # the converter truncates the float the same way
    side = "R" if cm < 0 else "L"
    sign = "-" if cm < 0 else ""
    cm = abs(cm) % 1000000
    return nmea_sentence(f"GPXTE,A,A,{sign}{cm // 100}.{cm % 100:02d},{side},M")


def trimble_roxte_text(text: str) -> bytes:
    payload = f"@ROXTE,{text}".encode("ascii")
    total = sum(payload) & 0xFFFF
    return bytes([0xBF]) + payload + bytes([total >> 8, total & 0xFF, 0x10, 0x03])


def trimble_roxte(xte_m: float) -> bytes:
    """@ROXTE with three decimals.

    The firmware drops the last value character before parsing (it reads "0.050" as "0.05"),
    so the millimetre digit is free: when the checksum would land on a byte the receiver
    treats as framing, the next millimetre value that keeps the centimetres is used instead.
    """
    base = f"{xte_m:.3f}"
    for mm in range(10):
        text = base[:-1] + str(mm) if mm else base
        frame = trimble_roxte_text(text)
        hi, lo = frame[-4], frame[-3]
        if hi not in TRIMBLE_UNSAFE and lo not in TRIMBLE_UNSAFE:
            return frame
    return trimble_roxte_text(base)        # practically unreachable


def printable(frame: bytes) -> str:
    return "".join(chr(b) if 32 <= b < 127 else f"<{b:02X}>" for b in frame.rstrip(b"\r\n"))


# ---------------------------------------------------------------------------
# Self-test against the firmware's own fixtures
# ---------------------------------------------------------------------------

def selftest() -> int:
    """Re-encode the firmware test fixtures (known_good_sentences.txt and
    test_SerialGuidanceChannel.cpp's Trimble frame) and compare byte for byte."""
    checks = [
        # Re-encoded from the values in a real receiver log, compared byte for byte.
        ("GGA, receiver log", gpgga("151503.00", 53 + 26.480207 / 60, 6 + 45.193203 / 60, 2, 9, 44.05),
         b"$GPGGA,151503.00,5326.480207,N,00645.193203,E,2,09,1.0,44.05,M,0.00,M,,*67\r\n"),
        ("GGA, receiver log", gpgga("151524.00", 53 + 26.466402 / 60, 6 + 45.178635 / 60, 2, 9, 43.99),
         b"$GPGGA,151524.00,5326.466402,N,00645.178635,E,2,09,1.0,43.99,M,0.00,M,,*6F\r\n"),
        ("VTG, receiver log", gpvtg(213.4, 2.91, 5.39), b"$GPVTG,213.4,T,,M,002.91,N,005.39,K*61\r\n"),
        ("XTE +, receiver log", gpxte(0.017374), b"$GPXTE,A,A,0.017374,L,N*68\r\n"),
        ("XTE -, receiver log", gpxte(-0.008822), b"$GPXTE,A,A,-0.008822,R,N*5D\r\n"),
        ("XTE -, receiver log", gpxte(-0.036539), b"$GPXTE,A,A,-0.036539,R,N*57\r\n"),
        ("JD position 52.0 N 5.0 E", jd_position(52.0, 5.0), b"$0CFEF31C,00072A9C80652680*52\r\n"),
        ("Trimble ROXTE 0.050", trimble_roxte_text("0.050"),
         bytes([191]) + b"@ROXTE,0.050" + bytes([2, 241, 16, 3])),
    ]
    ok = True
    for name, got, want in checks:
        good = got == want
        ok &= good
        print(f"{'ok  ' if good else 'FAIL'} {name}: {printable(got)}")
        if not good:
            print(f"     want {printable(want)}")

    # Decode our own JD speed and XTE frames the way CanSerialParser does.
    spd = bytes.fromhex(jd_speed(90.0, 2.0, 44.0)[10:26].decode())
    course, speed, _, alt = struct.unpack("<HHHH", spd)
    good = (course / 128, speed / 256, alt / 8 - 2500) == (90.0, 2.0, 44.0)
    ok &= good
    print(f"{'ok  ' if good else 'FAIL'} JD speed decodes to 90.0 deg / 2.0 / 44.0 m")
    for xte in (0.0, 0.15, -0.37):
        xd = bytes.fromhex(jd_xte(xte, True)[10:26].decode())
        val = xd[3] | (xd[4] << 8)
        got_cm = (val - 32000) >> 1
        good = got_cm == round(xte * 100) and xd[0] == 0x77 and (xd[1] & 0xF0) == 0x10
        ok &= good
        print(f"{'ok  ' if good else 'FAIL'} JD XTE {xte:+.2f} m decodes to {got_cm:+d} cm, RTK")
    for xte, want in ((0.15, "$GPXTE,A,A,0.15,L,M*"), (-0.15, "$GPXTE,A,A,-0.15,R,M*"),
                      (12.34, "$GPXTE,A,A,12.34,L,M*"), (-0.07, "$GPXTE,A,A,-0.07,R,M*")):
        got = gpxte_cnh(xte).decode()
        good = got.startswith(want)
        ok &= good
        print(f"{'ok  ' if good else 'FAIL'} CNH-converter XTE {xte:+.2f} m: {got.rstrip()}")
    for xte in (0.05, -0.12, 0.333, -1.5):
        frame = trimble_roxte(xte)
        hi, lo = frame[-4], frame[-3]
        good = hi not in TRIMBLE_UNSAFE and lo not in TRIMBLE_UNSAFE
        ok &= good
        print(f"{'ok  ' if good else 'FAIL'} Trimble {xte:+.3f} m: {printable(frame)}")
    print("selftest", "PASSED" if ok else "FAILED")
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# Serial port
# ---------------------------------------------------------------------------

def list_serial_ports() -> int:
    ports = list(list_ports.comports())
    if not ports:
        print("no serial ports found")
    for p in ports:
        mark = "FTDI" if p.vid == FTDI_VID else "    "
        print(f"{mark}  {p.device:8}  {p.description}  {p.hwid}")
    return 0


def find_ftdi_port() -> str:
    ftdi = [p.device for p in list_ports.comports() if p.vid == FTDI_VID]
    if len(ftdi) == 1:
        return ftdi[0]
    if not ftdi:
        sys.exit("no FTDI adapter found; plug it in, or pass --port (see --list)")
    sys.exit(f"several FTDI adapters ({', '.join(ftdi)}); pick one with --port")


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--mode", choices=["jd", "nmea", "trimble"], help="message format, see above")
    p.add_argument("--port", help="serial port, e.g. COM6 (default: the only FTDI adapter)")
    p.add_argument("--baud", type=int, default=None, help="default 115200 for jd, 4800 otherwise")
    p.add_argument("--rate", type=float, default=1.0, help="message sets per second")
    p.add_argument("--lat", type=float, default=53.442230)
    p.add_argument("--lon", type=float, default=6.755487)
    p.add_argument("--alt", type=float, default=4.4, help="metres")
    p.add_argument("--quality", type=int, default=4,
                   help="GGA fix quality; for jd, 4 sets the RTK flag in the XTE frame")
    p.add_argument("--sats", type=int, default=14)
    p.add_argument("--course", type=float, default=301.5, help="degrees")
    p.add_argument("--speed-kmh", type=float, default=2.0)
    p.add_argument("--speed-as-knots", action="store_true",
                   help="jd: put knots, not km/h, in 0CFEE81C bytes 2-3 (what the firmware reads "
                        "until #205 lands; a real tractor sends km/h)")
    p.add_argument("--cnh-xte", action="store_true",
                   help="nmea: send $GPXTE as the CNH XTE converter does (signed, L = positive, M)")
    p.add_argument("--xte-amp", type=float, default=0.5, help="sine amplitude, metres")
    p.add_argument("--period", type=float, default=60.0, help="sine period, seconds")
    p.add_argument("--xte-fixed", type=float, default=None,
                   help="constant XTE in metres (negative = left)")
    p.add_argument("--list", action="store_true", help="list serial ports and exit")
    p.add_argument("--selftest", action="store_true", help="check the encoders and exit")
    a = p.parse_args()

    if a.selftest:
        return selftest()
    if serial is None:
        sys.exit("pyserial is missing: py -m pip install pyserial")
    if a.list:
        return list_serial_ports()
    if a.mode is None:
        p.error("--mode is required (jd, nmea or trimble)")

    port = a.port or find_ftdi_port()
    if a.baud is None:
        a.baud = 115200 if a.mode == "jd" else 4800
    speed_field = a.speed_kmh / 1.852 if a.speed_as_knots else a.speed_kmh

    with serial.Serial(port, a.baud, timeout=0, write_timeout=2) as sp:
        print(f"feeding {port} @ {a.baud} baud: {a.mode} at {a.rate:g} Hz, quality {a.quality}, "
              f"{a.speed_kmh} km/h, course {a.course}", flush=True)
        period = 1.0 / a.rate
        t0 = time.monotonic()
        n = 0
        warned = False
        while True:
            now = datetime.now(timezone.utc)
            if a.xte_fixed is not None:
                xte = a.xte_fixed
            else:
                xte = a.xte_amp * math.sin(2 * math.pi * (time.monotonic() - t0) / a.period)

            if a.mode == "jd":
                frames = [jd_position(a.lat, a.lon),
                          jd_speed(a.course, speed_field, a.alt),
                          jd_xte(xte, a.quality == 4)]
            else:
                frames = [gpgga(now.strftime("%H%M%S") + ".00", a.lat, a.lon, a.quality, a.sats, a.alt),
                          gpvtg(a.course, a.speed_kmh / 1.852, a.speed_kmh)]
                if a.mode == "trimble":
                    frames.append(trimble_roxte(xte))
                else:
                    frames.append(gpxte_cnh(xte) if a.cnh_xte else gpxte(xte))

            line_time = sum(len(f) for f in frames) * 10 / a.baud
            if line_time > period and not warned:
                print(f"warning: one set takes {line_time:.2f} s at {a.baud} baud, more than the "
                      f"{period:.2f} s period; lower --rate or raise --baud", flush=True)
                warned = True

            stamp = now.strftime("%H:%M:%S")
            for frame in frames:
                sp.write(frame)
                print(stamp, printable(frame), flush=True)

            n += 1
            delay = t0 + n * period - time.monotonic()
            if delay > 0:
                time.sleep(delay)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
