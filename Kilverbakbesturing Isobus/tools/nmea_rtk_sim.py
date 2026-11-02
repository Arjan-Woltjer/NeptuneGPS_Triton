"""Bench GNSS for the kilverbak: GPGGA + GPVTG on a serial port, with keys to change the fix.

For the bench test (Documentation/SESSION_01_TEST_BRIEF.md): a USB-serial adapter into the
board's receiver port (Serial1, pins 0 RX / 1 TX, 3.3 V logic) stands in for an RTK receiver on
the kilverbak, so Auto, the gates (G! / R! / S!) and the valves can be seen without a field.

    py tools/nmea_rtk_sim.py COM7                  # 4800 baud, 2 Hz, RTK fixed, 6 km/h
    py tools/nmea_rtk_sim.py COM7 --baud 38400 --altitude 12.34

Keys (Windows console):  q  quality 4 (RTK fixed) -> 5 (float) -> 1 (GPS) -> 0 (no fix) -> 4
                         s  speed on / off (6 km/h or standing still: the speed gate)
                         +  height 1 cm up       -  height 1 cm down
                         p  pause / resume sending (a quiet UART: hand-over to CAN, then G!)
                         x  quit
Moving north from the start point at the set speed, so the distance along A -> B changes too.
"""
import argparse
import math
import sys
import time

import serial


def checksum(body):
    c = 0
    for ch in body:
        c ^= ord(ch)
    return f"{c:02X}"


def sentence(body):
    return f"${body}*{checksum(body)}\r\n".encode("ascii")


def ddmm(value, is_lat):
    hemi = ("N" if value >= 0 else "S") if is_lat else ("E" if value >= 0 else "W")
    value = abs(value)
    deg = int(value)
    minutes = (value - deg) * 60
    return (f"{deg:02d}{minutes:07.4f}" if is_lat else f"{deg:03d}{minutes:07.4f}"), hemi


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=4800, help="the rate the board has stored (byte 10; erased = 4800)")
    ap.add_argument("--rate", type=float, default=2.0, help="GGA+VTG pairs per second (4800 baud carries ~4)")
    ap.add_argument("--lat", type=float, default=52.0)
    ap.add_argument("--lon", type=float, default=5.0)
    ap.add_argument("--altitude", type=float, default=10.0, help="metres")
    ap.add_argument("--speed", type=float, default=6.0, help="km/h while moving")
    args = ap.parse_args()

    try:
        import msvcrt
    except ImportError:
        msvcrt = None

    port = serial.Serial(args.port, args.baud)
    qualities = [4, 5, 1, 0]
    q_index, moving, paused = 0, True, False
    altitude, lat = args.altitude, args.lat
    period = 1.0 / args.rate
    print(__doc__[__doc__.index("Keys"):__doc__.index("Moving north")] if msvcrt else "(no key control on this platform)")
    while True:
        if msvcrt:
            while msvcrt.kbhit():
                k = msvcrt.getwch()
                if k == "q":
                    q_index = (q_index + 1) % len(qualities)
                elif k == "s":
                    moving = not moving
                elif k == "+":
                    altitude += 0.01
                elif k == "-":
                    altitude -= 0.01
                elif k == "p":
                    paused = not paused
                elif k == "x":
                    return
        speed_kmh = args.speed if moving else 0.0
        lat += speed_kmh / 3.6 * period / 111320.0
        quality = qualities[q_index]
        if not paused:
            t = time.gmtime()
            hhmmss = time.strftime("%H%M%S", t) + ".00"
            la, ns = ddmm(lat, True)
            lo, ew = ddmm(args.lon, False)
            gga = f"GPGGA,{hhmmss},{la},{ns},{lo},{ew},{quality},12,0.8,{altitude:.2f},M,46.0,M,,"
            knots = speed_kmh / 1.852
            vtg = f"GPVTG,0.0,T,,M,{knots:.2f},N,{speed_kmh:.2f},K,A"
            port.write(sentence(gga) + sentence(vtg))
        sys.stdout.write(f"\rquality {quality}  speed {speed_kmh:4.1f} km/h  height {altitude:7.2f} m  "
                         f"{'PAUSED ' if paused else 'sending'}   ")
        sys.stdout.flush()
        time.sleep(period)


if __name__ == "__main__":
    main()
