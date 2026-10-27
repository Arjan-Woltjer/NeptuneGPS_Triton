# Session 21 handover -- van Mastwijk, 2026-10-27 (field laptop Ganymedes, neptune-gps-fa -> desktop)

Remote Control was not on for this session, so this file is the handover. Full record:
`logs/2026-10-27_session21_vanmastwijk_{serial.log,timeline.md}` on `test/session21` @ `6630864` (results table at
the top of the timeline).

## Board and settings

- **Board on the rig: the Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge), not the 4.0. Its own EEPROM: it
  started with preferred VT 0, bypass 0 and its own width points; set during the day to the rig's values.
- **Firmware now: `test/session21` @ `6dd5f2c`** = `7b714f1` (10-26 merges #276-#278) + **#279** (end shutoff) +
  **#254** (AUX-N, pool **MW11**). Hex: OneDrive `Firmware builds/session21/teensy41_isobus_6dd5f2c.hex`
  (also `teensy41_isobus_7b714f1.hex`, `teensy40_isobus_7b714f1.hex`).
- **Left at:** shares 4, deadband 2, bypass 1, hitch inversion 1, RTK 4, preferred VT 2 (on the CNH VT), lightbar
  AUTO, width points 21/34, 144/42, 266/50, width 160.
- CANedge AD4F266A sessions **94, 95, 96, 97, 98** (11:21:48 on; 98 from 12:55:07). Nothing before 11:21:48.

## Verified (brief SESSION_21_TEST_BRIEF.md)

- **#264 cab wizard:** C1-C7 pass (C2: 90 s open, VT/TC/1 Hz line held, loop 2.7 ms; C5 tested with NEE). C8 not run.
- **#265 VT page:** K1-K10 pass on both VTs. K7 done as a variant (page open on the 1200, CNH VT on:
  `calibration dropped`, nothing saved).
- **#266 width on the VT:** B1, B2, B4, B5, B7 (1200, no stutter) pass. **B3 failed on 7b714f1, passes on 6dd5f2c.**
- **#269:** MW10 and MW11 uploaded with the long bursts to both the CNH VT (0x26) and the 1200 (0x80), no pool error.
- **#273 lightbar:** L1, L3, L4 (with AAN), L5 pass; AAN -> AUTO without a break. L2: 65462 committed and xte == lb
  (the 1200 had RTK fixed indoors); following the line in AUTO not run (standing still).
- **Failover / preferred VT:** T3, K8, K10, T6/V8 pass; preference saved and read through every reboot. #274's
  carry-over from byte 70 was not testable (this board's byte 70 was 0).
- **#276/#278:** LCD starts clean, hitch M/H follows.

## Findings and PRs (none merged: Arjan's call)

1. **#279 / PR #280 (base main):** the end shutoff never latched against the stop. The filtered width (whole cm)
   flips 1 cm on 1-2 counts of sensor noise; `position != lastPosition` restarted the 3 s timer every pass, in manual
   and AUTO. Fix: a move is >= 2 cm from where the timer last restarted. Native 437/437 on main; field-verified on
   6dd5f2c. Arjan: after the cut the pump stops once the LS signal has cleared, which is slow (bypass fitted).
2. **#254 / draft PR #281 (base `feat/240-vt-width-procedure`):** AUX-N Breder/Smaller (Boolean momentary), MW11.
   The 1200 lists both functions, but **nothing can be assigned** (broken link), also after a 1200 reboot. The armrest
   is an AUX-N input unit: tractor ECU 0xF0, NAME A00086000BC9F19F, model 85, only connected to the 1200 (0xE700
   function 0x23 maintenance all session). No assignment command reached the plough. Most likely no armrest input
   is compatible with Boolean momentary. Native 515/515 on the stack branch.
3. **Pool cosmetics for the next label (no issue yet):** list popups unreadable (white text, transparent background on
   the VTs' light popup: invisible on the CNH VT, faint on the 1200); `cm`/`mm` on the page and the BREEDTE screen in
   the small 8x8 font. **Arjan's requests:** `cm` after the values on the main screen, and XTE in cm too (label `XTE`
   without `(m)`). Layout proposal: values right-aligned to x 174, `cm` at 176-200 in 12x16; XTE gets a 5-character
   field.
4. **Loop time:** the 6dd5f2c boot at 12:53 had one 123.7 ms loop pass (first MW11 upload?); every pass after it
   2.6 ms. To recheck at a boot that loads from NVM.
5. Small: the width-procedure soft keys log nothing (only the main ones do); the dump has no hitch/wheel-speed line;
   one lightbar sender failure per fresh claim; boot drops per boot 346-562 (#268), constant within a boot.

## For the desktop

- **Capture questions (sessions 96-98):** (a) 0xE600/0xE700 AUX traffic between the 1200 (0x80) and the tractor ECU
  0xF0 during the AUX test, 12:57-13:09 (session 98): any assignment-related messages, any 0x26 status at all? (b) the
  1200's first MW10 upload (11:37:47, session 96) and MW11 upload (12:57:32, session 98): DPO/CTS sequence with the
  255-packet bursts. (c) the 12:53 boot (session 98): what the plough sent in the 124 ms pass.
- **Branches:** `fix/279-end-shutoff-jitter` and `feat/254-aux-n` (both off test/session21, merged into it) are
  superseded by the PR branches `fix/279-end-shutoff-main` and `feat/254-aux-n-stack`.
- Leftover git worktree admin folders `.git/worktrees/wt254`, `wt279` (OneDrive lock): `git worktree prune` later.

## Next rig visit

- AUX: first learn the armrest's input types (Auxiliary Capabilities request 0x27 to the 1200, not in AgIsoStack, or
  a bench input unit), then the matching function type, with the direction mapping confirmed in a log-only build.
- Driving steps still open: L2's AUTO half, C8, E6, wheel speed (N2).
- The pool cosmetics (finding 3) once built: a look on both VTs.
