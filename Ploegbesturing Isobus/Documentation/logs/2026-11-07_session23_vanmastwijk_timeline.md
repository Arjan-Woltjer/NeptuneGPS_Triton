# Session 23 -- van Mastwijk (CNH + InCommand 1200), 2026-11-07 (timeline)

Brief: `SESSION_23_TEST_BRIEF.md`; live sheet https://claude.ai/artifact/VTPss3Uxh4GQCL5LEZKsA8 (results per step in
its db). Plough control: **the Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge, COM4), env `teensy41_isobus`, build
**`d6b3675`** (code a25313e, v0.23, MW16), flashed on the workstation 2026-11-06, not reflashed. Field laptop Ganymedes,
times are laptop time (CET). CANedge AD4F266A on the plough's ISOBUS: sessions **107** (from ~17:00), **108**, **109**.
Serial log 17:11:00-17:59:13 (`2026-11-07_session23_vanmastwijk_serial.log`; opened after the board was up, so no
boot print and no first MW16 uploads in it).

## Results

**Everything passes except AUX-N on the 1200; AUX-N passes on the CNH VT.** #296 (shared ISOBUS layer): no regression.

| Area | Result |
|---|---|
| S0-S2 setup | PASS. Splash v0.23 (Arjan). CANedge 107 inventory: our NAME at 0x81 (1407 / function 132 / class 2 / identity 1), CNH VT 0x26, tractor ECU 0xF0. The plough first bound to the CNH VT (1200 off) and uploaded MW16 there, then moved to the 1200 (preferred VT 1) at ~17:10 and uploaded MW16 there: both before the logger, times in CANedge 107 |
| R1-R4 #296 debug menu | PASS. 17:11:07 dump: CLAIMED 0x81, bound to the 1200 (0x80, VT version 3). Dump vs session 22's (13:59:40) with numbers masked: same sections, order and field names. 1 Hz line `lb= ... vt= ... tc=` as before. DDOP 540 bytes, **VT pool 13 263 bytes** (MW16) |
| P1a-P4a #282 on the 1200 | PASS. Popups readable now (session 21: faint). **Finding: ANNUL. lands in an odd place on the 1200**, on the calibration page and on the BREEDTE page, but in the right place on the CNH VT. Cause (likely): the pool lays the soft keys out for a 2 x 6 key VT (`VTObjectPool.hpp` SoftKeySlot), ANNUL. at column 2 row 6; Arjan suspects the 1200 has 2 x 5, so row 6 overflows. Fix: ANNUL. on row 5 (fits both). Confirm the 1200's key count in the capture (Get Number of Soft Keys response) |
| R8, P1b-P4b #282 on the CNH VT | PASS (Arjan) |
| A0-A4 #254 on the 1200 | A0, A1 PASS: armrest AUX inputs already on, BREDER and SMALLER listed. **A2 FAIL: assigning fails on the 1200**; the plough got nothing. **CANedge: the 1200 never sent the armrest an input enable (0x25)**, 17:13-17:25, as in every earlier capture |
| A5 #254 on the CNH VT as VT 1 | **PASS** at 17:44:29: `Assigned function 170 to input 28100`, `171 to input 28101`; presses logged; unassign 17:47:22 (`input 65535`, correct). See the AUX-N section below for the failed attempts before it |
| L2D #272 | PASS: AUTO from ~17:55 on the 1200's line, autosteer engaged, XTE 7-8 cm L from the 1200's lightbar frame |
| C8 #31 | PASS: holds with the cab wizard open, picks up after closing |
| E6 #250 | PASS: valve cuts at the end stop |
| R7 failover | PASS (Arjan); no separate plug pull, the day's switches cover it (17:25, 17:36, 17:48, below) |
| Loop / CAN | Longest loop 6.3 ms after the first dump (123 ms once at boot). RX drops 323, all at boot (#268). No CAN errors, load 15-16 % |

### AUX-N (#254): what the capture shows

The armrest (tractor ECU 0xF0, NAME A00086000BC9F19F, AUX-N model 85) sends its maintenance message at 10 Hz, "ready"
except a few seconds of "initializing" at each VT change (17:25:06, 17:27:30, 17:34:55).

| Time | Event |
|---|---|
| 17:13-17:25 | Plough on the 1200 (VT 1). BREDER/SMALLER listed on the 1200's AUX screen; assigning fails. **No 0x25 from the 1200 to the armrest, no 0x24 to us** |
| 17:25 | Arjan swaps the VT numbers (CNH = 1, 1200 = 2). Plough moves to 0x26 at 17:25:40 (MW16 from the VT's memory, 0.4 s) |
| 17:25:39, 17:28:07 | CNH VT sends us "unassign 170/171" (plough answers OK). Learn on the CNH fails; still no 0x25 to the armrest |
| 17:34:45, ~17:36 | 1200 off; tractor off/on (tractor ECU pool moves to the CNH VT). Plough rebinds to 0x26 at 17:36:48 |
| 17:38:48, 17:39:53 | **CNH VT -> armrest 0x25 enable inputs 28109 and 28110; armrest answers error 0x01 (invalid input object ID).** CNH shows "de voornoemde ingang kan niet worden geactiveerd. De inrichting aan de ingang heeft een fout of ongeldige status aangegeven, controleer extra toekenningen" |
| 17:44:29 | **CNH VT -> armrest 0x25 enable 28100 and 28101: accepted (error 0)**; assignment 0x24 reaches the plough. Works |
| 17:47:19 | Unassign: 0x25 disable 28100/28101, accepted |

So on this rig: (1) the 1200 never starts the AUX-N dialog with the armrest; (2) the CNH VT only did after the tractor
ECU restart (its pool then on the CNH VT); (3) the armrest refuses inputs 28109/28110 (the ones CNH likely uses itself)
and accepts 28100/28101. Arjan: fine as is, AUX-N works on the CNH VT.

Side note: the dump's `AUX-N: Breder events=176 Smaller events=182` counts every input status message (the armrest
sends them ~2 Hz globally once assigned), not presses.

## Other findings

- **The plough's mode is not on the VT screen** (Arjan): with the calibration page open the plough is not in AUTO, and
  nothing on the VT says so. Wanted: the mode (AUTO / HOLD / MANUAL, and the HOLD reason S! / G!) on the main screen,
  as on the cab LCD.
- **AUTO speed margin**: driving on autosteer at 0.57-0.58 m/s, just above MINSPEED 0.5 m/s; dips below it put the
  plough in HOLD (S!). Arjan's first "not auto" (~17:50).
- **The 1200 lost MW16**: at 17:48:05 (VOORK. VT 2, the 1200 as VT 2) "No version label from the VT matched", uploaded
  again (17:48:05.2-06.8, 1.6 s, stored, no error), though MW16 was uploaded to it at ~17:10. Check in CANedge 107
  whether the first upload was stored (Store Version response), or the 1200 drops pools at a reboot / renumbering.
- XTE standing still (17:11-17:12): the 1200's own lightbar frame jumped 5 R .. 62 L for ~15 s, then held 64 L; the
  plough shows exactly that (upstream).
- 0xCD (CNH, function 33) sends PGN 129283 with length 0 (rejected=112, harmless).
- Known: `[NM]` NAMEs print `000000000000000lx` (#315).

## End state

- Board: `d6b3675`, **preferred VT 2**, bound to the 1200 (0x80) at 17:59, because **the VT numbers are still
  swapped** (CNH = 1, 1200 = 2) unless Arjan restored them after 17:59. Restore 1200 = VT 1, CNH = VT 2; the
  plough then binds to the CNH VT (VT 2).
- AUX-N: unassigned (17:47:22).
- Final dump 17:59:10: failover switches=3, claim requests=6, TC reconnect attempts 48 (the 1200's reboots).

## Timeline

| Time | Event |
|---|---|
| ~17:05 | Plough on the CNH VT (1200 off), MW16 uploaded there (CANedge 107) |
| ~17:10 | 1200 on; plough moves to it (preferred VT 1), MW16 upload |
| 17:11:00 | Serial logger open |
| 17:11:07 | R1 dump |
| 17:11:47-17:12:25 | R3 1 Hz line |
| 17:12:28 / 17:12:32 | R4 DDOP / VT pool dumps |
| 17:13:15 | Plough sees the armrest as AUX-N input unit (model 85) |
| 17:15:14-17:17:36 | P2a-P4a on the 1200 |
| ~17:19-17:25 | A0-A2 on the 1200 |
| 17:25:08-17:25:41 | VT numbers swapped; plough to the CNH VT |
| 17:26-17:37 | Phase E on the CNH VT; A5 attempts |
| 17:34:45 | 1200 off |
| ~17:36 | Tractor off/on; plough rebinds to the CNH VT 17:36:48 |
| 17:38:48, 17:39:53 | Armrest refuses inputs 28109/28110 |
| 17:44:29-17:44:41 | AUX-N assigned on the CNH VT, presses logged |
| 17:46:29 | 1200 back; TC DDOP activated |
| 17:47:22 | AUX-N unassigned |
| 17:48:03-17:48:06 | VOORK. VT 2 -> 1200 (VT 2), MW16 uploaded again |
| ~17:50-17:58 | Phase F driving: L2D, C8, E6 |
| 17:59:10 | Z1 final dump; logger closed 17:59:13 |
