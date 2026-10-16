# Session 17 -- 2026-10-16 -- van Mastwijk (CNH, InCommand 1200, SteerCommand Z2) -- timeline

Brief: `../SESSION_17_TEST_BRIEF.md` (workstation, written 2026-10-13 after session 16). Previous: `2026-10-13_session16_vanmastwijk_timeline.md`.
Board 1324910 flashed by Arjan before this session; the brief wants `test/rig-lightbar-failover` @ `2745ff4`.
Serial log: `2026-10-16_session17_vanmastwijk_serial.log` (laptop wall clock, CEST). Logger started 09:14:49 (pid 20176, auto-find PJRC).
CANedge AD4F266A uploads by itself: OneDrive `CANedge logs/bucket-AD4F266A/session-00000057/` (today; piece 1 = 09:00-09:05 arrived 09:05).
Session 56 piece 9 (21:00-21:03:51 on 10-13) arrived 09:05 today with the logger's power-up: the brief's open question is answered.

Start order today (differs from the brief's setup): CNH VT up, **InCommand 1200 OFF**, plough control first on USB only, then onto the bus.
So the plough binds the **CNH VT 0x26 first**; the lightbar channel has no display function to talk to until the 1200 comes on.
Failover therefore runs F3/F4 first (to get onto 0x80), then F1/F2.

## Build check (first dump, on USB before the bus)

Three builds are possible on the board; the dump's lightbar block tells them apart:

| Dump shows | Build | Meaning for today |
|---|---|---|
| `Display partner: 0x.. ... stack partner=` present | **2745ff4** (brief) | full plan: R, D, G, F |
| no `stack partner=`, but the lightbar line says `state N` / `autosteer off` | bad487d | decoder fix in, 0xF5 not a partner: R, D, G, F; F4 may show the 0xF5 prune again (expected) |
| only `Requests: softwareId=.. ignored=..`, no state word | 2a25331 (session 16) | **decoder bug, NO AUTO**; drive D1 only to confirm the freeze; reflash (`pio run -e teensy41_isobus -t upload`, plough Teensy alone on USB) |

Also expected on USB alone: CAN error-passive, bus load 0.1 %, msgs=0 (no bus yet), 0x81 and 0xDC claimed anyway.

## Checklist (tick live)

SETUP
- [x] S0 CANedge powered early: session 56 piece 9 uploaded **Y** (09:05), new session **57** started (piece 1 at 09:05)
- [x] S1 tractor on, CNH VT running, InCommand OFF (per Arjan 09:14)
- [x] S2 CANedge: logging and uploading since ~09:00 (session 57); bus tap not checked against the MF4 at the rig (workstation: confirm SA 0x81 / 0xDC in session 57)
- [x] S3 serial logger started 09:14:49 (pid 20176) before the board was on USB
- [x] S4 plough control on USB 09:14:53 (COM4); boot print seen Y; dump 09:18:24: 0x81 CLAIMED, 0xDC CLAIMED, failures=0, error-passive TX 128 (alone, as expected), msgs=0
- [x] S5 **BUILD CHECK passed** 09:18:24: `Display partner: 0xFF ... hello=N  stack partner=N` -> build **2745ff4**
- [x] S6 plough control onto the bus 09:19:27; dump 09:20:29: CAN error-active (TX peak 144), VT bound to **0x26** (v4, switches=0), **MW08 from NVM Y** (09:19:28, 1 s after the join), TC N (TC 0xF7 is the 1200's, off), `stack partner=N`, 65462 frames=0, failures=0. 129283 count 34 at +60 s
- [ ] S7 one-minute inventory: not run at the rig (left for the workstation)

R  REGRESSION on the new library (standing still; needs the 1200 on)
- [x] R0 InCommand ON ~09:21:00 (Arjan "now"; claims 09:21:16, TC DDOP activated 09:21:24). Identification 8/8, propA=1, 65462 at 5 Hz, `lb=103R` = the display, pool stays on 0x26 (switches=0). Was: Serial: identification (requests=answered, unknown=0, softwareId sent), propA>=1, 65462 starts at 5 Hz, `lb=` = display. Pool **stays on 0x26** (= F2 behaviour, note it, switches=0)
- [x] R1 dump 09:23:22 (+2 min): MW08 from NVM **Y** (on 0x26)  TC **Y** (0xF7, TC-GEO Y)  propA=**1**  65462 frames=611 committed=545  **stack partner=0xF5**  failures=**1** (new, see timeline)  ignored=91  softwareId asked 2x (sent 3x)
- [x] R2 **PASSED**: plug out ~09:32:57, back in 09:33:26 (~29 s); MW08 from NVM 09:33:26.8 (Arjan: immediately on the CNH screen), TC 09:33:28; dump 09:34:28: identification again (22/22 total), propA=2, **stack partner=0xF5**, 65462 committed, failures still 1. Partners 0x26/0xF7/0xF5 NOT pruned while we were off; 0x2B, 0x80, 0xAC, 0xCD, 0xE9, 0xF0 pruned (expected). **Bus-off entries 2, error-passive 3 (TX peak 248)** in the plug-out window (session 16: 0 bus-off)

D  DRIVE with the fixed decoder (only after R1; AB line, RTK fixed; AUTO stays off until D1 passes)
- [x] D1 **PASSED** 09:24:00-09:25:20: autosteer drive from 103R onto the line, `xte=` = `lb=` with the sign every second (1.03 -> 0.39 -> 0.05 -> 0.00, +-0.01 on the line); byte 2 = 2 throughout, 3 seen 09:25:36-37 in the turn (committed); `e` flag on from 09:24:09 (byte 6 0x93); `C` at 0 cm. Hand-drive 0.5 m off not done separately (the turn's run-off covered it, 09:26:38-44 by hand to 114L, `-` flag)
- [x] D2 **PASSED** headland turn(s) 09:25:22-09:26:10 and 09:26:38-09:26:53: state 1 at 09:25:28-29 (2 s), 09:25:45-50 (6 s), 09:26:45-49 (5 s): `xte=` HOLDS at the last commit, xteAge grows to ~5.7 s, then follows on state 2. Engaged again 09:25:57, on the line 0Ce 09:26:10
- [x] D3 **PASSED** (Arjan 09:34): speed on the plough and the 1200 match; standing still `spd=0.00` on the 1 Hz line
- [ ] D4 AUTO with the hitch down: **not done** (the pass-number finding makes the current rule unsafe on the AB line and 3L+; next build)

G  NEW: the InCommand's "GPS lost" during a CNH VT reboot (owner triggers it)
- [x] G1 baseline = the R2 dump 09:34:28 (standing 103R on 1R, `xte=` = `lb=`, xteAge 0.14 s, spd 0)
- [x] G2 **HOW triggered**: CNH VT switched OFF (Arjan: "this also triggers GPS lost"), ~09:35:44; and a CNH reboot ~09:37:4x
      trigger ~09:35:44 (off) and ~09:37:25 (reboot)   "GPS lost" shown / GPS back: times not called
      1 Hz line: **nothing changes on the plough**: ggaAge ~50 ms, xteAge ~150 ms, `lb=` 102-103R live, byte 2 = 2, 0x80's 65535 payload keeps changing, in both events. The 1200's broadcasts do not stop.
      plough HOLD: **no reason to** (no data went stale); VT GPS indicator: ask. What goes silent: **0x26 (CNH VT), 0xAC (172, the CNH GPS module per Arjan), 0xCD (205)**; 0xE9 pruned too
      no separate dump (the 1 Hz line covers it)
- [x] G3 the plough never lost anything: `xte=` follows throughout, stack partner=0xF5, switches went to 1 only because the CNH VT itself went away (F3)
- [x] G4 repeated (CNH reboot 09:37:25-09:38:05): same, 0xAC and 0xCD drop out, the plough's data stays fresh

F  FAILOVER on the new library (#188 + #774). Order today: F3, F4, then F1, F2 (plough starts on 0x26)
- [x] F3 **PASSED** CNH VT off ~09:35:44 (VT status timeout 09:35:47) -> `partner 0x26 silent, VT at 0x80 alive -- switching to it` **09:35:59** (~15 s, as session 16), MW08 from the 1200's NVM 09:35:59.3 (Arjan: pool moved to the 1200)
- [x] F4 **PASSED** CNH VT back (claims 09:36:18-20, boot RTS 09:36:25-56), and a second CNH reboot 09:37:4x-09:38:05: plough **stays on 0x80**; prunes at 09:36:12 (0x26, 0xAC, 0xCD, 0xE9) and 09:37:53 (0xAC, 0xCD, 0xE9): **0xF5 never offline** (session 16: 6 s). 0xE9 still pruned (expected)
- [x] F1 **PASSED** InCommand off ~09:40:03 (last GGA; Arjan "now") -> `partner 0x80 silent, VT at 0x26 alive -- switching to it` **09:40:18** (~15 s; session 16: ~20-25 s: faster with #774), MW08 from NVM 09:40:19.3 (Arjan: pool back on the CNH VT). 65462 stopped 09:40:06.6; ggaAge/xteAge grow (15.6 s / 12.8 s at 09:40:19) -> HOLD by the 2 s rule
- [x] F2 **PASSED** InCommand on ~09:40:47 -> **stays on 0x26** (switches=2); TC restarts 2-5 while it booted, DDOP activated 09:41:18; identification again (30/30), propA=3, 65462 resumes, **stack partner=0xF5**; Arjan: GPS without RTK at first, XTE on the plough

END
- [x] E1 final dump 09:41:45: failures=1, 65462 frames=5658 committed=4882, switches=2, propA=3, identification 30/30, ignored=527, error-passive entries 3, bus-off 2 (both in the R2 plug-out), TC restarts 5
- [x] E2 plough unplugged ~09:42:30 (last serial line 09:42:29); **CANedge left running** (Arjan), so session 57's pieces upload by themselves
- [x] E3 committed and pushed on `test/session12` (423f41c, then the end rows)
- [x] E4 handover sent to the workstation Remote Control session (see the end of the timeline)

## Timeline

| Time (CEST) | Event |
|---|---|
| 09:05 | CANedge upload: session 56 piece 9 (`00000009-6AD1CCA2.MF4`, 1.46 MB) and session 57 piece 1 (`00000001-6AD1CCB0.MF4`) landed in OneDrive. Logger was powered ~09:00. |
| 09:14 | Arjan: CNH VT up, InCommand 1200 down, Triton not connected, going to plug it into USB. |
| 09:14:49 | Serial logger started (pid 20176), waiting for PJRC 16C0:0483. |
| 09:14:53 | Plough control on USB (COM4), boot print captured. 0x81 claimed 09:14:57, lightbar 0xDC claimed 09:14:59. CAN error-passive after the claim (alone on USB, no bus). 1 Hz line switched on by the logger. Every 10 s: `VT: partner 0xFE silent -- requesting address claims` (no VT yet). |
| 09:18:24 | Dump on USB alone: **build check passed**, `stack partner=N` present = 2745ff4. VT failover claim requests=20 (ours), lightbar softwareId sent 1x, ignored=20 (= our own claim requests), failures=0, 65462 frames=0. |
| 09:19:27 | **Plough control onto the bus** (Arjan "now"). 7x `[AC]: Address violation for address 129` in the join second (session 16 had 12, session 15 had 8: known, harmless so far). `[NM]: Partnered control function ... has claimed` (the CNH VT). |
| 09:19:27.709 | `VT Server has a matching label for MW08` -> **pool loaded from the CNH VT's NVM** 09:19:28.108, no errors. **Rebased library: NVM on the CNH VT works.** |
| 09:20:29 | Dump at +62 s: error-active, bus load 12 %, VT 0x26 v4 connected, 63 status msgs, switches=0, claim requests=26. 60160 x99 from 0x26, 44032 x618 (lockout=YES/READY, curvature -29.5), 64770 x62, 129283 x34. Lightbar claimed 0xDC, no display partner (1200 off), ignored=27, failures=0. TC not connected (1200 off). |
| ~09:21:00 | **InCommand 1200 switched on** (Arjan "now"). 09:21:16 0xF5/0xF7 claims (`[NM] Partnered control function ... claimed`, twice: VT client and TC client). 09:21:24 TC DDOP activated (server version 3). 09:21:21-09:21:52: 24x `[TP] RTS with a global destination, ignoring` + 8 aborts, every ~1.3 s (sessions 15/16 had 105/81: the 1200's boot, known). |
| ~09:22-09:23 | Arjan: XTE on the CNH VT screen and on the plough. While RTK converged the XTE went 0 -> all over the place -> steady **103 cm** with RTK fixed. |
| 09:23:11 | 1 Hz line: `xte=1.03m ... lb=103R-:670002002F019104` -> **the decoder commits: `xte=` = `lb=`** with byte 2 = 2, byte 6 = 0x91 (right, not engaged), byte 7 = 4. |
| 09:23:22 | **R1 dump: passed.** Lightbar `Display partner: 0xF5 identification requests=8 answered=8 unknown=0 hello=Y stack partner=0xF5`, propA=1, 65462 frames=611 committed=545 (66 uncommitted = the display start-up, as session 16's 65), `-> 103 cm right, state 2, autosteer off, line (committed)`. TC 0xF7 connected, TC-GEO Y/N, task active. VT 0x26 v4, switches=0. 65267/65256/65535 from 0x80 x1240 each; position 53.442062 / 6.755857, course 302.8. 44032 lockout=YES/READY. 129283 count 40. **`failures=1`** (session 16: 0 all session): not printed on serial; counts a stack refusal or a destination missing from the table; happened between 09:20:29 and 09:23:22, i.e. during the 1200's start-up. Guess: the second softwareId BAM refused while the first ran (asked 2x). **Capture question: which frame from 0xDC was not sent.** `Quality: 0` / `IsRtkQuality=N` with RTK fixed: the legacy 65267 carries no quality (same as session 16?). |
| 09:24:00 | **D1: autosteer drive** (Arjan "driving with autosteer now"), 0.6 m/s. `xte=` = `lb=` every second: 1.00 R, 0.39, 0.36, engaged flag on 09:24:09 (byte 6 0x91 -> 0x93), 0.15, 0.05, **0.00 `0Ce` (byte 6 0x13, centre)** 09:24:21, then 3L/1L/1R/0C on the line to 09:25:12 (stopped). Byte 2 = 2 throughout. **D1 passed: the decoder fix works.** |
| 09:25:22-09:26:10 | **D2: headland turn.** Runs off to 102R (disengaged), state 1 09:25:28-29 (xte holds 1.14, age 1.9 s), state 2 swings L/R while the display re-references (lb 132L..55R..81L, committed: follows the display), state **3** 09:25:36-37 (committed), **state 1 09:25:45-50, byte 6 0xA1: `xte=` held at -1.25 m, xteAge 0.7 -> 5.7 s**, state 2 09:25:51 on the new pass: 99R/94L/120L then converging 113L -> 0Ce by 09:26:10, engaged again 09:25:57. **D2 passed.** |
| 09:26:38-09:26:53 | Second turn (Arjan "turned around"): disengaged, by hand off the line 1L -> 114L (`-`, by hand = the D1 hand part), state 1 09:26:45-49 (xte holds -1.14, age to 5.1 s), state 2 09:26:50 with byte 6 **0xA1/0x61 (bit 5 set with state 2**, session 16 saw bit 5 only with state 1). Stopped 09:26:53. Note for AUTO: in state 2 during a line change the XTE swings over a metre for a few seconds; the plough follows it (the hitch is up in the turn, so no action). |
| 09:26:55-09:27:24+ | **FINDING (#207, blocks AUTO on this pass): third turn onto a different pass** (Arjan "turned around now again, different pass"). State 1 from 09:26:57 and it **stays 1 on the new pass while autosteer is engaged and converging**: byte 6 **0xA3** (bit 7 right, bit 5, bit 1 engaged), `lb=` 34R -> 26 -> 16 -> 7 -> 1R -> **0Ce** (byte 6 0x23) smoothly, like a valid line. The decoder holds on 1: **`xte=` frozen at -1.19 m, xteAge 27 s and growing.** So state 1 is not only "no solution in the turn": it can carry a steered, valid XTE. Session 16 saw state 1 only for seconds in turns (bit 5 only with state 1). What distinguishes this pass from the earlier ones: ask what the 1200 shows (pass number, line type, any warning). |
| 09:28:13-09:29:24 | Turn from pass 1 onto **pass -1** (Arjan's numbering): byte 2 went 1 -> **2** (crossing pass 0, 09:28:19) -> **3** (09:28:24) -> **4** (09:28:31-32, overshoot; decoder held 2 s) -> **3** (09:28:37 on). Pass -1 driven engaged with byte 2 = 3: `xte=` = `lb=` 109R -> 33R -> 0Ce/1L/1R, **works**. Stopped 09:29:24. |
| 09:30 | **Arjan: pass 0 worked (both directions), pass 1 did not, pass -1 worked.** Byte 2 per pass: **pass 0 = 2, pass 1 = 1, pass -1 = 3, and 4 seen while overshooting towards -2.** **Byte 2 (probably bytes 2-3) is the display's PASS/TRACK NUMBER, not a line state** (byte 2 = 2 - Arjan's pass number). Fits session 16 too: 2->3 at 20:52:53 coincided with the display jumping to another line; the short "state 1" stretches in turns were passes crossed in the turn; 1 during RTK convergence = the nearest pass jumping. Byte 7 = 4 (line present) throughout. Byte 6 bit 4 set on passes 0/-1 (2, 3), bit 5 set on pass 1: track-related too, not validity. **Consequence for #207: the decoder must not gate on byte 2 at all. Proposed rule: commit when byte 7 == 4 and bytes 2-3 != 0xFFFF; report bytes 2-3 as the pass number.** Arjan decides; until then AUTO only on passes with byte 2 = 2 or 3. |
| 09:30-09:31 | Arjan: pass 1 is the furthest from the field boundary. Standing still at 09:30:4x, the 1200 says **"2 L"** while byte 2 = 3 and the XTE is 0-1 cm: "2 L" is the **pass label**. |
| 09:31:08 | Arjan drives onto **"1L"**: byte 2 goes 3 -> **2** at 09:31:08 as the nearest pass changes (lb 121L -> 72R in one second), stopped 30L of 1L at 09:31:16. **Mapping: screen 2L = 3, 1L = 2, so byte 2 = (passes left of AB) + 1; Arjan's "pass 1" (byte 2 = 1) should be the AB line itself (to confirm on the screen), and the right side is still unknown (0? 0xFF..?).** |
| 09:31:32 | Arjan drives onto **"0"** (the AB line; = his earlier "pass 1"): byte 2 goes 2 -> **1** at 09:31:32 (lb 106L -> 89L/98R, byte 6 0x61/0xA1, bit 5 set). Decoder holds again (`xte=` -1.23, frozen). **Mapping confirmed: byte 2 = pass number + 1 counted to the left: 0 -> 1, 1L -> 2, 2L -> 3, 3L -> 4.** Byte 6 bit 5 goes with pass 0, bit 4 with the L passes. Right side next (1R). |
| 09:31:52 | Arjan drives onto **"1 R"**: byte 2 goes 1 -> **2** at 09:31:52 (lb 106L -> 88L/96R), standing 103R at 09:31:58 with byte 6 **0xA1/0x61 (bit 5 set, bit 4 clear)**. **1R sends the same byte 2 as 1L.** |
| 09:32 | **Complete mapping (#207): byte 2 = |pass| + 1** (0 -> 1, 1L/1R -> 2, 2L -> 3, 3L -> 4); bytes 2-3 = 0x0001.. little-endian (byte 3 = 0 throughout). **Side of the AB line is in byte 6: bit 4 = left passes (1L, 2L, 3L), bit 5 = the AB line and the right passes (0, 1R).** None of this is validity; **byte 7 = 4 (line present) is the only validity flag**, 0xFF in byte 2 = not available (display booting). Session 16's "bit 4 when byte 2 >= 2, bit 5 only with byte 2 = 1" was the same thing: that field only drove the AB line and left passes. **Decoder fix: commit when byte 7 == 4 and byte 2 != 0xFF; log pass = byte 2 - 1 with L (bit 4) / R (bit 5).** |
| 09:25:15, 09:31:07, 09:32:59 | `[VT]: Server response to a command timed out` (CNH VT 0x26), one each, while connected; no switch, nothing visible. The third was the plug pull. |
| ~09:32:57 | **R2: plough ISOBUS plug pulled** (Arjan "pulling out now"). 09:33:00 VT status timeout, 09:33:01 TC status timeout, reconnect attempts 1/2 at 09:33:10/20, 09:33:12/22 our claim requests. 09:33:13 prune: 0x2B, 0x80, 0xAC, 0xCD, 0xE9, 0xF0 `now offline`; **0x26, 0xF7, 0xF5 (all three partners) not pruned**. |
| 09:33:26 | **Plugged back in** (Arjan "now"). Re-claims seen; 3x `[PR] NACK-ing PGN request for PGN 60928 because no callback could handle it` (session 16: 8, session 15: 583) and `[VT]: The VT Server is NACK-ing our VT messages. Disconnecting.` (session 15: 1, session 16: 0) at the same ms, then **MW08 label matched, loaded from NVM 09:33:26.8**; TC DDOP activated 09:33:28. Arjan: the pool was immediately on the CNH screen. |
| 09:34:28 | **R2 dump at +62 s: passed.** VT 0x26 connected, reconnect attempts=2, switches=0. Lightbar: identification 22/22 (8 + 14 after the replug), propA=**2**, softwareId asked 3x (sent 4x), **stack partner=0xF5**, 65462 frames=3790 committed=3114, failures=**1** (unchanged since 09:23). TC 0xF7 connected, TC-GEO Y, restart 1. CAN error-active again, but **error-passive entries 3, bus-off entries 2, TX peak 248, RX peak 82** from the plug-out (session 16: 0 bus-off). 129283 count 53. |
| 09:34 | **D3: Arjan: speed on the plough and the 1200 match.** |
| ~09:35:44 | **G2 + F3: CNH VT switched off** (Arjan: also triggers "GPS lost" on the 1200). 09:35:47 VT status timeout, 09:35:57 reconnect attempt 3, **09:35:59 `partner 0x26 silent, VT at 0x80 alive -- switching to it`**, MW08 from the 1200's NVM 09:35:59.3. **GPS on the plough unaffected**: ggaAge ~50 ms, xteAge ~150 ms, lb 102-103R throughout. 09:36:12 prune: 0x26, **0xAC (172 = CNH GPS module, Arjan)**, 0xCD (205, unknown), 0xE9; **not 0xF5**. Arjan: 172, 38, 205 stop communicating. |
| 09:36:18-09:36:56 | CNH back: 0xE9, 0xCD, 0x26 re-claim 09:36:18-20; 24x global RTS + aborts 09:36:25-56 (CNH boot). Plough stays on 0x80. |
| ~09:37:25-09:38:05 | **Second event: CNH VT rebooted** (Arjan "now", "rebooting", "done rebooting"). 09:37:53 prune: 0xAC, 0xCD, 0xE9 (not 0xF5); re-claims 09:38:01-05. Arjan: 172 and 205 stop while the CNH reboots. GPS/XTE on the plough fresh throughout; 65535 payload from 0x80 changed (0B050A -> 0D060B) at 09:37:50. **G answer from the serial side: the 1200 keeps broadcasting position, speed and 65462 during its "GPS lost"; the event is the CNH GPS module 0xAC (and 0xCD) dropping out with the CNH VT. The capture decides whether 0x80's 65267 values freeze or stay live.** |
| 09:39:18-09:39:40 | Tractor moving again: lb 104R -> 141L across pass 0 (byte 2 = 1, held 6.5 s) -> pass 1 (byte 2 = 2). |
| ~09:40:03 | **F1: InCommand 1200 switched off** (Arjan "turning off the 1200?" / "now"). Last GGA ~09:40:03.4, last 65462 ~09:40:06.6. 09:40:05.8 VT command timeout, 09:40:06 status timeout, 09:40:11 TC status timeout, 09:40:16 reconnect attempt 1, **09:40:18 `partner 0x80 silent, VT at 0x26 alive -- switching to it`**, MW08 from the CNH VT's NVM 09:40:19.3. **~15 s, F1 passed.** Arjan: pool back on the CNH VT. |
| ~09:40:47 | **F2: InCommand 1200 switched on** (Arjan "turning 1200 back on"). TC restarts 2-5 every 15 s while it boots, 1x NACK 60928 09:41:08, DDOP activated 09:41:18. Arjan: GPS without RTK at first, then XTE on the plough. |
| 09:41:45 | **F2 dump: passed.** VT 0x26, switches=2, reconnect attempts 0. Lightbar: identification 30/30 (third run), propA=**3**, softwareId asked 4x (sent 5x), **stack partner=0xF5**, 65462 frames=5658 committed=4882, `38 cm right, state 2` committed, failures=1 (unchanged since 09:23). TC 0xF7 connected after 5 restarts, TC-GEO Y. CAN error-active, error-passive 3, bus-off 2 (last 499 s ago = the R2 plug-out). |
| 09:41:5x | Arjan: RTK fixed again. |
| ~09:42:30 | **Plough control unplugged** (Arjan "unplugging triton now"); last serial line 09:42:29. CANedge left running so session 57 uploads in full. End of session. |

## Results (for the handover)

| # | Question (brief section) | Answer |
|---|---|---|
| 1 | Regression (3): pool from NVM on both VTs, TC, identification, 65462 on the rebased library | **Yes.** MW08 from NVM on the CNH VT (join 09:19:28, R2 09:33:26, F1 09:40:19) and on the 1200 (F3 09:35:59). TC 0xF7 each time the 1200 was up. Identification 8/8 three times (30/30 total), Prop A 3x, 65462 at 5 Hz. #190's library works on hardware. |
| 2 | 0xF5 bound as stack partner, survives the CNH restart (3, F4) | **Yes.** `stack partner=0xF5` after every identification. Not pruned at the R2 plug-out (09:33:13), the CNH VT off (09:36:12) or the CNH reboot (09:37:53); 0xE9 was pruned each time (not a partner). Session 16's 6 s prune is gone. |
| 3 | D1: `xte=` follows `lb=` through an engaged drive with the fixed decoder (4) | Yes on passes 0 and -1 (byte 2 = 2, 3). **No on pass 1 (byte 2 = 1): byte 2 is the pass number, not a line state.** Decoder must gate on byte 7 == 4 only (#207). |
| 4 | G: how the "GPS lost" is triggered, what the bus does, does the plough hold (5) | Trigger: the CNH VT switched off or rebooted. **0xAC (CNH GPS module, per Arjan), 0xCD and 0x26 go silent; 0x80's position/speed/65535 and 0xF5's 65462 keep coming with live values**, so the plough's GPS and XTE never went stale and there was nothing to hold. The capture should confirm that 0x80's 65267 stays live and show what the 1200 used 0xAC for. |
| 5 | F1-F4 times and `switches=` on the new library (6) | All passed. F3 CNH off ~09:35:44 -> 0x80 09:35:59 (~15 s). F4 CNH back/rebooted: stays on 0x80. F1 1200 off ~09:40:03 -> 0x26 09:40:18 (~15 s; session 16 ~20-25 s). F2 1200 on: stays on 0x26, switches=2. |
| 6 | D3 speed standing still / moving | Arjan: the plough and the 1200 match; `spd=0.00` standing still. |
| + | Anything else (prune, churn, failures>0 with time) | **(a) #207 decoder, blocks AUTO on some passes: byte 2 of 65462 is the pass number (|pass| + 1: AB line 1, 1L/1R 2, 2L 3, 3L 4), not a line state; side of AB in byte 6 bit 4 (L) / bit 5 (AB line, R). The 2745ff4 rule (commit on 2, 3) freezes `xte=` on the AB line and on 3L+.** Fix: commit when byte 7 == 4 and byte 2 != 0xFF; log the pass. (b) Lightbar `failures=1` during the 1200's first start-up (09:20:29-09:23:22), never again; serial does not say which frame. (c) 2 bus-off + 3 error-passive entries in the R2 plug-out window (session 16: 0 bus-off). (d) 3x `[VT] Server response to a command timed out` from the CNH VT while connected (09:25:15, 09:31:07) without consequences. (e) `[VT]: The VT Server is NACK-ing our VT messages` once at the R2 rejoin, recovered at once. (f) TC needed 5 restarts while the 1200 booted in F2. |

Afterwards (brief section 8): results to #207 (D, G), #188 (F), #190 (R: the new library on hardware, needed before #190 merges); AgIsoStack-plus-plus #717 note if F4 or G2 shows the prune again.

## Results from the MF4 (workstation, 2026-10-16)

Capture: upload session 57, nine pieces, **09:04:36.5 to 09:44:14.6 CEST**, 560 438 extended frames on
channel 1, real clock. Archived as `../canlogs/2026-10-16_session17_vanmastwijk_log57/`. Session 58
(10:16:55-10:17:25) is a 30 s tractor start with only the TECU, the CNH VT, 0xAC and 0xCD on the bus:
nothing of ours, not archived. Session 56's ninth piece (21:00:00-21:03:45 on 10-13) is analysed in the
session 16 timeline, (e).

**1. Inventory.** Both of our addresses are on the wire: **0x81** (NAME `a0101000afe00001`) from the join
at 09:19:27.4 to 09:42:30.1, **0xDC** (`a00083000c21887e`) to 09:42:29.5. Senders by identity number:

| Identity | Manufacturer code | Addresses (function) |
|---|---|---|
| 28337 (the InCommand 1200) | 97 | 0x80 (29, VT), 0xF7 (130, TC), 0xF5 (128 instance 5, display), 0xE9 (128), **0x2B (129)** |
| 143617 (the CNH display) | 94 | 0x26 (29, VT), **0xAC (23)** |
| 1212289 | 94 | 0xCD (33) |
| 651679 | 94 | 0xF0 (134, TECU) |

At the join, the CAN controller flushed what it had queued while the board was alone on USB: 0xDC's
boot software-ID BAM (made at 09:14:59) went out at 09:19:27.428-.436 with **1-3 ms between packets**
(ISO 11783-3 wants 50-200 ms for a BAM), together with repeated claims. Only after a USB-first start;
the display was off then, so nobody listened. Low priority.

**2. PGN 65462, all bytes** (sessions 56 and 57 together, 12 335 frames from 0xF5):

| Byte(s) | Meaning | Evidence |
|---|---|---|
| 0-1 | \|XTE\| in cm, little-endian, 0xFFFF = not available | as decoded since #207 |
| 2 | **\|pass\| + 1** (AB line = 1), 0xFF while the display boots | the field mapping of this session; and session 56 21:02:42-21:03:15, driving across the field: 3 -> 11, one step every ~4 s |
| 3 | 0 (0xFF while booting): probably the pass number's high byte | 0 in every frame with a line |
| 4-5 | **vehicle heading, whole degrees**, little-endian, 0-359 | against 0x80's 65256 course above 1.5 km/h (3380 frames): median -0.1 deg, 5th percentile -1.1 deg; about 1 in 20 is 180 deg off (reversing, probably) |
| 6 bit 0 | set exactly when byte 7 = 4 | 12 140 / 12 140 |
| 6 bit 1 | autosteer engaged; also set while the display boots (byte 7 = 1) | as session 16 |
| 6 bits 2, 3 | never set | |
| 6 bits 4 / 5 | **side of the pass**: bit 4 = left of the AB line, bit 5 = the AB line or right; with a line always exactly one of the two | 8 119 / 4 021, never both, never neither |
| 6 bits 6 / 7 | vehicle left / right of the line; neither exactly when \|XTE\| = 0 | 2 539 / 8 844; 757 frames at 0 cm, all without either bit |
| 7 | 0 booting, 1 no line (XTE 0), 4 line present | |

Byte 2 with a line (byte 7 = 4): 1 x690, 2 x7872, 3 x3250, 4-11 x328. **1018 frames with a line and a
valid XTE had byte 2 outside 2-3**, so 2745ff4's rule dropped them. Session 16's reading ("2 and 3
valid, 1 = no solution", "bit 4 when byte 2 >= 2, bit 5 only with 1") is superseded: that field only
drove the AB line and left-hand passes. **Validity is byte 7 = 4 (bit 0 repeats it).** A decoder that
commits on byte 7 = 4 and \|XTE\| <= 30000 commits every one of these frames; the pass number
(byte 2 - 1, side from bit 4/5) and the heading can be reported alongside. Decision for #207: Arjan.

**3. G, the 1200's "GPS lost".**

- **0xAC sends nothing but address claims on the ISOBUS** (625 claims, 61 TP data frames of zeros,
  1 request). It shares its identity number with the CNH VT 0x26, so it is a second function of the
  CNH display unit, and it goes off and reboots with it.
- **0x2B, a function of the 1200, polls every node with an addressed request for address claim
  every 2 s** (0xAC 596x, 0xCD 555x, 0xDC 470x) and asks each new node for 65242, 64965 and 64653.
  0xAC's last answer before the switch-off is 09:35:51; from then 0x2B retries about 1.5 times a
  second until 0xAC answers again. Same at the reboot (answers stop 09:37:3x, back 09:38:0x). That
  is what the ISOBUS shows of the trigger; whatever the 1200 takes from 0xAC reaches it some other way.
- **The 1200's own position stays live**: standing still, 0x80's 65267 shows 1-4 distinct values per
  5 s at the 1e-7 deg resolution both before and during (no frozen stretch), 65256 changes as before,
  65535 changes payload at 09:36:03-04 and 09:37:49 (`0d060b` -> `0d050b` -> `0b050a`, the DOP-like
  triple of #20). 65462 keeps its 5 Hz. The plough was right not to hold.

**4. Lightbar `failures=1` (09:20:29-09:23:22).** At 09:21:17.700 0x2B requested 65242 from 0xDC, and
0xDC started the software-ID BAM at 09:21:17.703 (packets 52 ms apart, correct). At 09:21:17.702 the
display's first identification read arrived; the channel sends the software ID before answering a
read, and that second BAM was refused while the first one ran: **the one failure**. Nothing is
missing for the display: it repeated the read at 09:21:18.119, both reads were answered (09:21:18.506
and .587), all eight DIDs and the Proprietary A exchange completed by 09:21:19.545, and every request
from 0x2B got its answer. Fix if wanted: skip the pre-read software ID while a BAM from 0xDC is in
progress.

**5. The R2 rejoin at 09:33:26.**

- Back on the bus, **the plough's VT client carried on before it re-claimed 0x81**: Change Numeric
  Value to 0x26 at 09:33:26.441 and .445, a working-set maintenance at .444, the claim only at .466.
  The CNH VT answered the two commands (.450, .456) and **NACKed the working-set maintenance (PGN
  59136) at .463**: after 29 s away it no longer had us as a client. That is the `VT Server is
  NACK-ing our VT messages`; the stack disconnected, reconnected and loaded MW08 from NVM at .786.
- **The three `[PR] NACK-ing PGN request for PGN 60928` are ours**: 0x81 sent NACKs (global) at
  .502-.504 for addressed requests for address claim from 0x2B (.444) and 0xF7 (.445, .449), in the
  same milliseconds as its claims (.490, .503, .504). The claims are the right answer; the NACKs are
  surplus and do no harm here.
- Follow-up candidates (no issue yet): reset the VT client when the bus was gone longer than the VT
  status timeout, and let the stack answer a request for address claim with the claim only.
