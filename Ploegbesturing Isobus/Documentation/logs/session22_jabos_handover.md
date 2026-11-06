# Session 22 handover -- field laptop to the bench, 2026-11-06

Session 22 ran at **J.A. Bos (Fendt + Trimble GFX-1260 / NAV-900 / NAV-900 autosteer)**, not at van Mastwijk. The
brief's "CNH VT" was the Fendt VT here, and "the 1200" the GFX-1260's UT. Results table at the top of
`logs/2026-11-06_session22_jabos_timeline.md`; serial log `logs/2026-11-06_session22_jabos_serial.log`
(13:56:49-14:31:22). CANedge AD4F266A was moved to this rig: sessions 99 and 100 (files 1-11, to 14:30:15).

## Board

Teensy 4.1 13249100 runs `fb09684` (v0.22, MW16), as flashed on the workstation. **Preferred VT left at 1** (the
Fendt VT 0x26): on this rig AUX-N only works through VT 1. No boot print was caught (the board was up before the
logger), so the EEPROM settings weren't read back today.

## Results, and where they go

| Issue | Result |
|---|---|
| #282 MW16 | **PASS on both VTs** (P1-P4 on the Fendt VT and the GFX UT; photos with Arjan). MW16 upload to the GFX 1.7 s, no error. **New: the soft key text is very small on the Fendt VT**, a bit better on the GFX |
| #254 AUX-N | **PASS on the Fendt VT**: BREDER/SMALLER assign to the Fendt joystick (input unit 0x80, AGCO, model 85), presses and releases logged, cab unit shows wider/narrower, unassign works. The GFX says "AUX-N not available (only on VT1)"; the `Preferred Assignment Error - Other` at every bind to the GFX (VT 2) fits. Arjan: the VT-1 terminal hosts AUX-N (as the 1200 at van Mastwijk). So session 21's "broken link" on the 1200 (as VT 1) is down to the 1200 or the CNH armrest, not the VT number |
| P5 / failover (#255, #274) | PASS: VOORK. VT switches in 0.7-1 s with both VTs up; a VT-number change on the Fendt was followed in 3 s / 13 s |
| #275 (N2S) | Wheel speed: **not applicable**, the cab LCD has no wheel-speed field and nothing uses VehicleSensors' pulse input (brief error). Instead GNSS speed vs Fendt display vs GPS: match at 1.2 km/h (reverse) and 3-4 km/h. GNSS speed stays positive in reverse |
| #272 (L2D), #31 (C8), #250 (E6) | **Not run**: AUTO needs an XTE source and RTK; this rig has neither on the ISOBUS |

## The Trimble's XTE is not on the ISOBUS

Arjan held the tractor still at four known deviations (GFX readings): 1.86 m R (14:00:05-14:01:30), 32-33 cm R
(14:28:36-14:29:05), 16 cm L (14:29:23-33), 32 cm L (14:29:49-14:30:14), also with the Trimble autosteer engaged.
A search over every SA/PGN/byte offset and integer/float type at m/cm/mm scales found **nothing that follows the XTE**.
What the Trimble puts on the ISOBUS: the GFX UT (0x87, NAME A0001D080C42B955, Trimble = manufacturer 98) sends VT
traffic plus 65267/65256 and 65096/65097, only while its UT is on; the Trimble TC (0xF5) only its status broadcast;
0x83/0xAD/0xD8 (Trimble, function 128) only claim addresses. The NAV-900 isn't on this bus. Same conclusion as
session 3 (2026-08-10, HardwareTestNotes.md). GNSS on the bus is the Fendt's own receiver (0x4A): 129029 **DGNSS
(quality 2) all session**, never RTK.

Options for this rig (Arjan's call): **serial NMEA from the GFX-1260 or the NAV-900** (the serial guidance parsers are
there, #277) or a TC-GEO licence. Arjan may have a photo of the GFX's serial-output menu.

Note: the 11-bit IDs 0x065-0x06F in these MF4s are the CANedge's own GNSS/IMU (BusChannel 9), not the Trimble.

## Small findings (no issue filed yet)

- [NM] log lines print the NAME as `000000000000000lx` and shifted address/channel numbers (format string on Teensy).
- The "XTE Trimble legacy 60160" counter counts the VT's TP.DT frames.
- AUX-N unassign lines print "from input 65535".
- The AGCO TC (0xF7) asks us for DDI 506 and 515 (-1 = request).
- GNSS speed has no direction; 65096 wheel-based speed from the Fendt carries one, if the speed gate should ever care.

## For the bench

1. File the findings above (soft key text size on the Fendt, NAME format, Trimble-legacy counter) or fold them into #282/#254.
2. AUX-N: compare the Fendt's working assignment with the 1200's "broken link" in the CANedge (session 100 files 8-9 vs session 21).
3. Decide the XTE route for J.A. Bos (serial NMEA vs TC-GEO).
4. Session 21's driving steps (L2D, C8, E6) are still open: they need van Mastwijk (lightbar XTE).
