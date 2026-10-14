#!/usr/bin/env python3
"""Feed an old (serial-GPS) plough control with valid GPGGA, GPVTG and GPXTE at 1 Hz.

The VehicleGps parser reads: GGA terms 1 (time), 2-5 (lat/lon), 6 (quality), 9 (altitude);
VTG terms 1 (course) and 5 (speed, knots); XTE term 3 (magnitude, metres), term 4 is the side.

    python feed_nmea.py --port COM6 --baud 38400 [--xte-amp 0.5] [--period 60] [--speed-kmh 2.0]

The cross-track error follows a slow sine (+-xte-amp metres, period seconds), so the display
shows movement; --xte-fixed <m> sends a constant value instead (negative = left).
Ctrl-C stops it. Every sentence is echoed to stdout.
"""
import argparse
import math
import sys
import time
from datetime import datetime, timezone

import serial


def checksum(body: str) -> str:
    c = 0
    for ch in body:
        c ^= ord(ch)
    return f"{c:02X}"


def sentence(body: str) -> bytes:
    return f"${body}*{checksum(body)}\r\n".encode("ascii")


def nmea_lat(lat: float) -> tuple[str, str]:
    hemi = "N" if lat >= 0 else "S"
    lat = abs(lat)
    deg = int(lat)
    minutes = (lat - deg) * 60
    return f"{deg:02d}{minutes:07.4f}", hemi


def nmea_lon(lon: float) -> tuple[str, str]:
    hemi = "E" if lon >= 0 else "W"
    lon = abs(lon)
    deg = int(lon)
    minutes = (lon - deg) * 60
    return f"{deg:03d}{minutes:07.4f}", hemi


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="COM6")
    p.add_argument("--baud", type=int, default=38400)
    p.add_argument("--lat", type=float, default=53.442230)
    p.add_argument("--lon", type=float, default=6.755487)
    p.add_argument("--alt", type=float, default=4.4)
    p.add_argument("--quality", type=int, default=4, help="GGA fix quality (4 = RTK fixed)")
    p.add_argument("--sats", type=int, default=14)
    p.add_argument("--course", type=float, default=301.5)
    p.add_argument("--speed-kmh", type=float, default=2.0)
    p.add_argument("--xte-amp", type=float, default=0.5, help="sine amplitude, metres")
    p.add_argument("--period", type=float, default=60.0, help="sine period, seconds")
    p.add_argument("--xte-fixed", type=float, default=None, help="constant XTE in metres (negative = left)")
    a = p.parse_args()

    lat_s, ns = nmea_lat(a.lat)
    lon_s, ew = nmea_lon(a.lon)
    knots = a.speed_kmh / 1.852

    with serial.Serial(a.port, a.baud, timeout=0) as sp:
        print(f"feeding {a.port} @ {a.baud}: GGA/VTG/XTE at 1 Hz, quality {a.quality}, "
              f"{a.speed_kmh} km/h, course {a.course}", flush=True)
        t0 = time.monotonic()
        n = 0
        while True:
            now = datetime.now(timezone.utc)
            hms = now.strftime("%H%M%S") + ".00"
            if a.xte_fixed is not None:
                xte = a.xte_fixed
            else:
                xte = a.xte_amp * math.sin(2 * math.pi * (time.monotonic() - t0) / a.period)
            side = "R" if xte >= 0 else "L"

            gga = (f"GPGGA,{hms},{lat_s},{ns},{lon_s},{ew},{a.quality},{a.sats:02d},0.8,"
                   f"{a.alt:.1f},M,45.0,M,1.0,0000")
            vtg = f"GPVTG,{a.course:.1f},T,,M,{knots:.2f},N,{a.speed_kmh:.2f},K"
            xtes = f"GPXTE,A,A,{abs(xte):.3f},{side},N"

            for body in (gga, vtg, xtes):
                frame = sentence(body)
                sp.write(frame)
                print(now.strftime("%H:%M:%S"), frame.decode("ascii").rstrip(), flush=True)
            n += 1
            # keep a steady 1 Hz
            target = t0 + n
            delay = target - time.monotonic()
            if delay > 0:
                time.sleep(delay)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
