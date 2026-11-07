# Session 24 bench, 2026-11-07 (evening)

Rig Teensy 4.1 13249100 on the bench with AgISOVirtualTerminal 1.6.0 (VT at 0x26, VT version 5) through the Saturn gs_usb interface. Serial log: `2026-11-07_session24_bench_serial.log` (COM7, timestamps from the workstation clock).

Firmware:

| From | Build |
|---|---|
| 21:59 | `test/session24` 503c773 (v0.24, MW17: #296 + #317 + #319) |
| 23:34 | 264216f: + cbfc82d, a still-held repeat no longer starts the main Breder / Smaller, and a lost VT drops them (#320) |
| 23:47 | d59c191: + b4d0c61, the same rule for every key and AUX-N (`HeldKey`), tap keys act only on the release of a press this connection saw (#320) |

## Steps

| Step | What | Result |
|---|---|---|
| B1 | MW17 upload | PASS. The first try (23:01:05 Get Memory timeout) was CAN H and L swapped on the bench; after the swap: the stored KB02 label deleted, MW17 stored with no error (23:01:14). |
| B2 | `HAND`, + / - from the VT and from the cab | PASS |
| B3 | `H G!` (auto, no GPS) | PASS, the cab LCD shows `HL G!` at the same time |
| B4 | `H S!` / `AUTO` | Skipped: no CAN GPS on the bench. These strings mirror the control's mode, as the cab does. |
| B5 | CALIBR, ANNUL. on slot 5, back to `H G!` | PASS. The cab LCD shows that the VT is calibrating. |
| B6 | BREEDTE + / -, ANNUL. on slot 5 | PASS (23:06:20-23:06:23) |
| B7 | Cab wizard: `CAL` on the VT | PASS |
| B8 | Held VT key, CAN cable pulled: the valve stops | PASS (23:19:15 press, 23:19:17 VT lost, valve off on the 600 ms timeout) |
| B8 finding | After the reconnect (23:19:52) the narrower valve drove for 30 s with no key touched, until the owner pressed and released it at 23:20:24 | Filed as #320 |
| B9 | Debug dump (23:20:29) | PASS: CLAIMED 0x81, error-active, VT connected to 0x26, `preferred=VT2 (not seen)` |
| B8 rerun on 264216f | Same cable pull | PASS: 23:35:42 `VT: Narrower still held without a press -- ignored`, the valve stays off |
| Recheck on d59c191 | CALIBR / ANNUL., BREEDTE + held / ANNUL., main - held | PASS (23:48:37-23:49:07) |

## Notes

- **#320 cause:** AgISOVirtualTerminal never got the release through while the cable was out, and kept repeating still-held after the reconnect. The 23:35:42 line shows it doing that again.
- **Bus-off history in the dump** (5 698 entries, longest about 2 h): from the board running with no bus and with CAN H/L swapped before 23:01. The last one is the B8 cable pull.
- **PGN 60160 counter** reads 6 from 0x26 (the VT): 60160 is TP.DT, so the "XTE Trimble legacy" counter probably counts the VT's own transport frames. A question for #296, not a #319 change.
- **AISO (PGN 64770) not tested:** nothing on the bench sends it (counter 0).
