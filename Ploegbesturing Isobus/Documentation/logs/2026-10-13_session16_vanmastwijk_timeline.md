# Session 16 -- 2026-10-13 -- van Mastwijk (CNH, InCommand 1200, SteerCommand Z2) -- timeline

Brief: `../SESSION_16_TEST_BRIEF.md`. Handover: `session16_vanmastwijk_handover.md`.
Board 1324910 must be on `test/rig-lightbar-failover` @ `2a25331` (flashed at the workstation 2026-10-10 20:30,
per the handover note; the brief's "not yet flashed" line is older). Proof at the rig: the dump's lightbar block has
the `Requests: softwareId=.. (sent ..x) ignored=..` line.
Serial log: `2026-10-13_session16_vanmastwijk_serial.log` (laptop wall clock, CEST). CANedge uploads land in OneDrive `CANedge logs/bucket-AD4F266A/session-00000056/` (today).
No real L160 at the farm: nothing to unplug.

## Checklist (ticked live)

SETUP (tractor on, InCommand OFF)
- [ ] S1 tractor on, CNH VT running, InCommand OFF
- [ ] S2 CANedge: card IN, logging LED on, ch1 on the in-cab connector, started __:__:__
- [x] S3 serial logger started 20:38:56 (pid 31832), board was already on USB (booted ~20:38:27), so no boot print; the dump replaces it
- [x] S4 plough control powered on the in-cab connector; dump 20:39:23: 0x81 CLAIMED, lightbar 0xDC CLAIMED, heartbeats=28, softwareId sent 1x
- [x] S5 BUILD CHECK passed 20:39:23: `Requests: softwareId=0 (sent 1x) ignored=5 last ignored PGN=60928` present = 2a25331. failures=0. #213: `PGN 129283` count at start = 0
- [ ] S6 one-minute card check (needs a reader; else skip and note)

IDENT (section 3; dump after each step)
- [x] I1 covered by I4 (same scenario: display boots with our bar present): identification, Prop A and 65462 all came
- [x] I2 dump 20:44:01: ignored=100 (50 at +1 min: **24/min while vt=Y, the display keeps polling our claim despite broadcasting**); 65462 frames=958 (+625 in 125 s = 5 Hz); lb=63Re steady on the 1 Hz line
- [x] I3 (done FIRST today, the display was already on): plugged in 20:40:56. Dump 20:41:56: requests=8/8 unknown=0, softwareId=1 (sent 2x: the display did ask 65242 after all), **propA=1**, **65462 frames=333** in 60 s, `lb=63Re`, display shows 63. **Q1 = YES**
- [x] I4 display cold boot with our bar present: off ~20:45:45, on 20:46:57. Dump 20:48:14: propA=2 (1 new), 65462 resumed at 5 Hz, softwareId asked again
- [ ] If no Prop A and no 65462 after I1-I4: section 7 (NAME arbitrary-address bit; answer 64965/64653; answer the display's Prop A; plan B own XTE). Nothing more to do at the rig unless a toolchain laptop is there.

DRIVE (section 4; only if 65462 is coming; AB line, RTK fixed; AUTO off until D1 passes)
- [x] D1 engaged 20:53: `lb=` follows the display (115R -> 1R, display 1) **but `xte=` frozen at -1.30 m: byte 2 = 3 while engaged, decoder commits only on 2. Must fix before AUTO (#207)**
- [x] D2 right: seen standing at 63-64 cm right (display right, `lb=63Re`, 20:46) and 87-115R approaching under autosteer (20:53)
- [x] D3 left: 17-27L during RTK convergence (20:48, display left) and 68-79L by hand after disengaging (20:57, display left). **Q2 answered: bit 7 set = right, clear = left**
- [x] D4 disengaged 20:56:42, by hand to 68L: 65462 keeps coming at 5 Hz; byte 2 stays 3; byte 6 bit 1 cleared (0x93 -> 0x91) = the engaged flag; `xte=` still frozen (decoder)
- [x] D5 headland turn 20:59:50: lb jumped at the track change and settled at 5L on the new pass; byte 2 went 3 -> 2 -> 1 -> 2 and the XTE un-froze (xte follows lb on the new pass, 21:01)
- [ ] D6 (free) speed check once more: `spd=` vs InCommand ____/____

FAILOVER (section 5; plough must be bound to 0x80 first: if on 0x26, do F3 first or pull/replug once)
- [x] F1 InCommand off ~20:45:45 -> `partner 0x80 silent, VT at 0x26 alive -- switching to it` 20:46:07, MW08 on the CNH VT 20:46:08, ~20-25 s, automatic (the session 15 failure is fixed)
- [x] F2 InCommand on 20:46:57 -> stays on the CNH VT (switches=1); TC 20:47:21; identification again 16/16, propA=2; 65462 resumed, lb=64Re
- [x] F3 CNH VT off by key ~20:49:30 -> `VT at 0x80 alive -- switching to it` 20:49:45, MW08 on the InCommand, `switches=2`, ~15 s
- [x] F4 CNH VT on 20:50:32 -> stays on 0x80, `switches=2` (dump 20:51:33). Side note: 0xE9 and 0xF5 flagged offline at 20:51:09 during the CNH restart roll-call

END
- [x] E1 final dump 21:02:46; #213: 129283 count start 0, end 71; lightbar failures=0
- [x] E2 CANedge stopped 21:03:51; session 56, uploaded to OneDrive bucket-AD4F266A
- [x] E3 committed and pushed on `test/session12` 21:05
- [x] E4 handover sent to the workstation session "ISO-Bus cabling diagrams" 21:05 (Remote Control, delivery unconfirmed)

## Timeline

| Time (CEST) | Event |
|---|---|

## Results (for the handover)

| # | Question | Answer |
|---|---|---|
| 1 | Prop A from display / 65462 per scenario I1-I4, with the counters (requests, softwareId, ignored) | **YES in every scenario.** Bar after display (20:40:56): requests 8/8, softwareId asked 1x (sent 2x), Prop A 1, 65462 at 5 Hz within 60 s, lb=63Re = display 63. Display cold boot with the bar present (20:46:57): identification again, Prop A 2, 65462 resumed. Replug 20:54: again (Prop A 3). The display still polls our claim at ~24/min throughout (ignored 544 at the end), so that polling is not a rejection sign. 65462 total 6129 frames, 0 send failures. |
| 2 | Side bit: `lb=` at 30 cm right / left | Seen standing still during RTK convergence 20:47:58-20:48:11: display left <-> `lb=..L` (byte 6 bit 7 clear, xte negative), display right <-> `lb=..R` (bit 7 set, xte positive). **Right = bit 7 set, confirmed both ways.** D2/D3 under autosteer: see below if driven. |
| 3 | F1-F4 times and `switches=` | F1 InCommand off ~20:45:45 -> CNH VT 20:46:07 (~25 s, switches=1). F2 InCommand on 20:46:57 -> stays, TC back 20:47:21. F3 CNH VT off ~20:49:30 -> InCommand 20:49:45 (~15 s, switches=2). F4 CNH VT on 20:50:32 -> stays (switches=2). **All four passed; #188 complete on this rig.** |
| 4 | #213: 129283 count start / end | 0 -> 71 (14 after the first minute on the bus, +3..6 at each join/VT event). Capture: confirm none on the wire. |
| 5 | `failures>0` in the lightbar block, with time | None (failures=0 all session, incl. the plug-in and the InCommand power cycle). |
| + | #190 churn, 44032 with autosteer, anything else | **Decoder bug (#207, blocks AUTO): the XTE commit is gated on byte 2 = 2, but the display sends 3 from the first engage on (and after engage/disengage transitions), so `xte=` froze at -1.30 m for the whole first pass while `lb=` followed the display (115 -> 1 cm). Commit on 2 and 3, hold on 1.** Byte 6 bit 1 = autosteer engaged (0x93 engaged, 0x91 not); the lb "e" flag (byte 2 = 2) is wrong. Byte 4 is heading-related (0x2D-0x3C one way, 0x79-0xB7 the other), not an offset-127 error. Track change at the headland: lb jumps for ~10 s with byte 2 = 1, then settles. No 205/172 churn except the CNH VT restart (where 0xE9/0xF5 were also flagged offline for 6 s: prune of late answerers?). 44032 still lockout=YES/READY under autosteer. Speed raw 0x218 (2.09 km/h) in the final dump with the 1 Hz showing spd=0.00: check which is right. |
| 20:35 | Operator: tractor started with the CANedge logging; InCommand switched on a few minutes later (so the display is up BEFORE our bar today: I3 order first, then I4/I1). CANedge now uploads over the internet: OneDrive `CANedge logs/bucket-AD4F266A/session-00000056/`, 3 files by 20:35. |
| 20:38:27 | Plough control booted (USB COM4), before the logger; no boot print. Logger started 20:38:56, 1 Hz line on. |
| 20:39:23 | Dump: **build check passed** (`Requests:` line present = 2a25331). 0x81 and 0xDC claimed, heartbeats 28, softwareId sent 1x, failures 0, ignored=5 (our own claim requests, no VT bound). **CAN error-PASSIVE, TX err 128, bus load 0.1 %, msgs=0: the board sees no bus at all.** |
| 20:40:56 | **Plough control plugged into the bus** (InCommand already on = the "bar after the display" order, I3). CAN back to error-active (TX peak 152 from the time alone). VT bound to the InCommand 0x80 (pool lives on the 1200, operator), TC 0xF7 connected. |
| 20:41:56 | **Dump at +60 s: QUESTION 1 ANSWERED YES.** `Display partner: 0xF5 requests=8 answered=8 unknown=0 hello=Y`; `softwareId=1 (sent 2x)`; **`proprietary A from display=1`**; **`PGN 65462 XTE: frames=333 committed=333`** (5 Hz), raw `3F0002002F019104` -> 63 cm right, engaged, line. 1 Hz line: `xte=0.63m`, `lb=63Re`. **Operator: the plough control reads 63 and so does the InCommand screen.** `ignored=50` (was 5 before the bus; our own claim requests count while vt=N; rate to be judged at +3 min). #213: 129283 count already 14. VT failover claim requests=12 (ours, while alone). |
| 20:44:01 | **I2 dump at +3 min:** 65462 frames=958 (625 in 125 s = 5 Hz), committed all, `lb=63Re` steady, display 63. **`ignored` 50 -> 100 in 125 s = 24/min while vt=Y: the display still polls our address claim every 2.5 s even though it broadcasts 65462.** So the polling is not a rejection signal. 129283 count stays 14 (came with the join; #213). 44032 lockout=YES/READY, curvature -1.25. failures=0. |
| 20:45 | Operator: standing still near the line, **autosteer NOT engaged**, yet byte 2 = 0x02 (decoded "engaged"). So byte 2 is not the autosteer state as read from log 42 (there it flipped 1 -> 2 when the error started falling). Candidates: line loaded / guidance active / steering available. D4 and the capture decide. The decoder commits only on byte 2 = 2, so the commit rule needs a second look (#207). |
| 20:46 | Operator: the InCommand shows the tractor **right** of the line, 63 cm. Frame byte 6 = 0x91 (bit 7 set) -> R. **First side-bit point: right = bit 7 set, as assumed.** D3 (left) still to confirm. |
| ~20:45:45 | **F1: InCommand switched off** (operator message 20:45:58; TC status timeout 20:45:58). |
| 20:46:07 | `VT: partner 0x80 silent, VT at 0x26 alive -- switching to it`; MW08 label matched on the CNH VT, loaded from NVM 20:46:08. **F1 PASSED, automatic, ~20-25 s after the switch** (session 15: never). 65462 stopped with the display; the 1 Hz line still shows xte=0.63m with xteAge 19.6 s: the stale XTE stays displayed, check what the plough logic does with it (#207 staleness). |
| 20:46:57 | **F2 + I4: InCommand switched on** (dark since ~20:45:45, 72 s). TC DDOP activated 20:47:21 (+24 s). |
| 20:48:14 | Dump at +77 s: partner stays 0x26, `switches=1` (**F2 passed**, no switch back). Lightbar: identification ran again, requests=16 answered=16 unknown=0, softwareId asked again (2, sent 3x), **propA=2**, 65462 frames=1809 committed=1744 (65 uncommitted during the display start-up: capture), `lb=64Re`. **I4 passed.** Operator: XTE follows the 1200 nicely while the pool stays on the CNH screen. 129283 count 23 (#213, grows on join events). |
| 20:47:58-20:48:11 | **Side bit, both sides seen while RTK converged (standing still):** display went left then right (operator); the 1 Hz line went `lb=27Le`..`17Le` (byte 6 = 0x51, bit 7 clear, xte -0.27..-0.17) then jumped at 20:48:09 to `48Re`, `61Re`, `63-64Re` (byte 6 = 0x91, bit 7 set, xte +0.64). **Bit 7 set = right, clear = left, matching the display both ways.** Also bytes 4-5 went 2F 01 -> 00 00 for 5 s around the jump (the "heading error" candidate). Q2 answered, D2/D3 would be the clean repeat under autosteer. |
| ~20:49:30 | **F3: CNH VT switched off by its key** (operator message 20:49:47). 20:49:45 `VT: partner 0x26 silent, VT at 0x80 alive -- switching to it`; MW08 loaded from the InCommand NVM 20:49:45. Dump 20:50:02: partner 0x80 (v3), `switches=2`. **F3 passed**, ~15 s. 65462 keeps coming (2349 frames), lb=64Re. |
| 20:50:32 | **F4: CNH VT switched on again.** 20:51:09 CFs 172, 205 (CNH) AND 233 (0xE9), 245 (0xF5, the InCommand display function) `now offline`; re-claims 20:51:15-20:51:20. 0xE9/0xF5 going "offline" at a CNH restart looks like a roll-call prune of late answerers (#574 pattern on the #190 build): check in the capture whether 0xF5 really re-claimed or was pruned while alive. 65462 kept coming regardless. |
| 20:51:33 | Dump at +61 s: partner stays 0x80, `switches=2`. **F4 passed.** 65462 frames=2806, failures=0, propA=2, ignored=268 (25/min, the display still polls). 129283 count 23. |
| 20:52:57-20:53:41 | **D1: driving forward, autosteer engaged.** `lb=` magnitude follows the display exactly: 87R rising to 115R while the tractor approached, then 113 -> 1 cm as autosteer pulled it onto the line (display: 1). **BUT byte 2 = 0x03 while driving engaged**, the decoder only commits on 0x02, so **the committed `xte=` is FROZEN at -1.30 m** (the last value committed while byte 2 was 2) throughout the drive. Byte-2 reading so far today: 2 = standing still, line loaded, not engaged; 3 = driving, engaged. Byte 6 went 0x91 -> 0x93 (bit 1 set) once moving, 0x13 at 0 cm (side bit clear). Byte 4 0x2D-0x3C. **Critical for #207: as built, the XTE freezes exactly when the plough would need it; the commit rule must accept 3 (or not gate on byte 2 at all). AUTO stays off today.** |
| 20:54:49 | Operator replugged the plough control (thinking the XTE was lost): VT pool reloaded 20:54:47, TC 20:54:53, nothing changed, `xte=` still -1.30 (byte 2 still 3, also standing still at 20:55:15). Then drove backwards on the line, lb 0-1 cm. |
| 20:56:42 | **D4: autosteer disengaged, drove off the line by hand** (asked to go right), 1 -> 68 cm, stop, then on to 79. **Byte 2 STAYS 0x03 after disengaging**, so 3 is not "engaged". What did change: **byte 6 bit 1: 0x93 while engaged and moving (from 20:53:14), 0x91 after disengaging (20:56:33)** -> bit 1 = autosteer engaged is the better reading. Side bit went clear (0x51, `L`) as the tractor left the line; display side and travel direction to be asked (the tractor had been reversing). `xte=` still frozen at -1.30: the decoder never commits once byte 2 = 3. |
| 20:57:30 | Operator: the display says **left** (68-79 cm). Frames: `lb=68L`..`79L`, byte 6 = 0x51, bit 7 clear. **Left = bit 7 clear, third confirmation of the side bit.** |
| 20:59:50-21:00:46 | **D5: headland turn onto the next pass.** lb ran 0 -> 92R leaving the line, then jumped at the track change (59R, 56L, 24L, 16L, 102L, 48R, 73R, 66R...) and settled on the new pass (37L -> 5L by 21:00:40). **Byte 2 during the turn: 3 -> 2 (20:59:58) -> 1 (21:00:03-09, byte 6 = 0xA1, bit 5 set) -> 2 (21:00:10).** With byte 2 = 2 the decoder commits again: `xte=` un-froze at 20:59:58 (0.59, -0.56, -0.24 ... following lb with the sign). Byte 2 = 1 = no valid line solution (values jump); the decoder correctly holds then. |
| 21:01:04-21:01:26 | On the new pass, autosteer engaged (byte 6 0x93): `xte=` follows `lb=` exactly: 0.01..0.02 R, 0.00, -0.01..-1.16 L as the tractor ran off the line end. **Operator: "we have xte again."** 21:01:27 next turn: byte 2 = 1, xte holds at -1.22. |
| 21:01 | **Byte-2 summary for #207:** 1 = no valid line solution (turning, line switch); 2 = valid XTE; 3 = valid XTE too (seen from the first engage 20:53:14 until the first headland turn, engaged or not, moving or not). The decoder must commit on 2 AND 3, hold on 1. Byte 6: bit 7 = right, bit 1 = autosteer engaged (0x93\/0x13 engaged, 0x91\/0x51\/0x11 not), bit 5 seen only with byte 2 = 1 (0xA1, 0x61). Byte 4 ran 0x2D-0x3C on pass 1 and 0x79-0xB7 on pass 2 (opposite direction): heading-related, not an offset-127 error. |
| 21:02 | Operator disengaged by turning the wheel, standing still 84 cm left. Byte 2 = 3 again, byte 6 = 0x61 (bits 5+6 set, bit 1 clear = not engaged, bit 7 clear = left). `xte=` holds at 1.05 (last commit). So 3 follows an engage/disengage transition; the display's own state machine, for the capture. Rule for the decoder stays: commit on 2 and 3, hold on 1. |
| 21:02:46 | **Final dump.** CAN error-active, TX peak 152 / RX peak 4, 2 error-passive entries (longest 134 s = the time alone before plugging in), 0 bus-off. Bus load 15.3 %. 65267/65256 x12313 from 0x80 (speed raw 0x218 = 2.09 km/h while the 1 Hz said 0.00: standing still? note), 60160 x221 from 0x26, 64770 x1311, 44032 x13104 (lockout=YES/READY still, curvature -37). VT partner 0x80 (v3), switches=2, claim requests=12. Lightbar: identification 30/30 (four runs: plug-in, display cold boot, the 20:54 replug, +1), propA=3, sent 929 frames, 0 failures, heartbeats 870, softwareId asked 3x (sent 4x), ignored=544 (~25/min throughout). **65462: frames=6129, committed=3577** (the rest had byte 2 = 1 or 3). TC 0xF7 connected, TC-GEO Y/N. **#213: 129283 count 0 at start, 71 at end** (none expected on the wire). |
| 21:03:51 | CANedge switched off (operator). Card/upload session 56 (OneDrive `CANedge logs/bucket-AD4F266A/session-00000056/`). |
| 21:03:45 | Plough control unplugged from the bus (load 0.1 %, vt=N). Last 65462 before that: `29000B00D7006104`, **byte 2 = 0x0B**, a value not seen before (after the operator left the line/turned off guidance?). More states exist than 1/2/3; the capture has them all. Logger stopped 21:04:03. End of session. |

## Results from the MF4 (workstation, 2026-10-13 evening)

Capture: card/upload session 56, eight 5-minute pieces, **20:23:05 to 20:59:59.98 CEST**, real clock
(the logger syncs over LTE now). The engage the operator called at 20:53:14 is on the wire at
20:53:14.134, so laptop and bus agree to well under a second. **The ninth piece (21:00 to 21:03:51)
is not in the bucket yet**: the logger was switched off before it closed, and it uploads at its next
power-up. Pass 2, the 21:02 wheel disengage, the final dump and the `0B` frame are in that piece.
Archived as `../canlogs/2026-10-13_session16_vanmastwijk_log56/` (the eight pieces).

**(a) PGN 65462 from 0xF5, byte by byte** (5306 frames):

| Byte | Values seen | Reading |
|---|---|---|
| 2 | 255 (24 frames), 1 (41), 2 (3156), 3 (2085) | 255 = not available (display booting, 20:47:14-19, whole frame 0xFF); 1 = display up, no line solution yet (20:47:19-28); **2 and 3 both carry a valid XTE** |
| 6 bit 1 (0x02) | | **autosteer engaged**: set 20:53:14.134 (operator: engage 20:53:14), clear 20:56:21.294 (operator noted the disengage at 20:56:42), set 20:59:18.846, clear 20:59:49.438 (just before the headland turn) |
| 6 bit 7 (0x80) | | right of the line |
| 6 bit 6 (0x40) | | left of the line |
| 6 bits 7+6 both clear | 0x11, 0x13 | **exactly on the line**: every 0 cm frame has neither bit, so "left" at 0 cm in the decoder is really "centre" (harmless, the value is 0) |
| 6 bit 4 (0x10) | | set whenever byte 2 >= 2, clear with byte 2 = 1 |
| 6 bit 5 (0x20) | 0x21, 0x22 | only with byte 2 = 1 (as the field notes say) |
| 6 bit 0 (0x01) | | set once the display has a line (byte 7 = 4); 0x22 at byte 7 = 1 |
| 7 | 0, 1, 4 | 0 = booting, 1 = no line, 4 = line present |

When byte 2 changes says what it is not: it does not follow the engage. **2 -> 3 at 20:52:53.740**,
20 s before the engage, with the magnitude jumping 3 cm L -> 137 L -> 29 R in 0.6 s; **3 -> 2 at
20:59:58.040** in the headland turn, jumping 1 R -> 131 R -> 22 L. Both transitions coincide with the
display re-referencing to another line or pass, while the engage (byte 6 bit 1) comes and goes
inside a stretch of 3. Our best reading: 2 and 3 are two line-following states of the display
(perhaps pass direction or line-lock), both valid. What 3 means exactly stays open; the decision
for #207 does not depend on it.

**(b) The "offline" of 0xE9 and 0xF5 at 20:51:09 was the stack's prune, not a lost claim.**
0xF5 sent 11 frames/s without a gap from 20:50:58 to 20:51:22. At 20:51:08.688 the CNH 0xCD sent a
*global* request for address claim; 0x2B, 0x81, 0xDC and 0xF0 answered within 7 ms, but **the
InCommand's 0xE9, 0xF5, 0xF7 and 0x80 did not answer it**. They did answer the next global request,
from 0xFE (the CNH VT restarting) at 20:51:15.502. The #190 stack prunes control functions that do
not answer a global request (upstream #574), except partnered ones (fork patch 723fad6): 0x80 (our VT
partner) and 0xF7 (our TC partner) are exempt, 0xE9 and 0xF5 are not, hence 6 s "offline" until
20:51:15. Why the InCommand ignores 0xCD's request is unknown. Consequence for the lightbar channel:
in such a window 0xF5 is not in the stack's table and an addressed reply to it would count as a
send failure (none was needed this time, failures=0). Making 0xF5 a partnered control function of
the lightbar channel (NAME filter manufacturer 97, function 128, instance 5) would exempt it.

**(c) PGN 129283 is on the wire: 65 frames, all from 0xCD, all zero-length**, each within 300 ms of
one of our 65 requests for 129283 (from 0x81). The plough control's counter (71 at the end, the
rest falls after the capture) is right. Session 15 is the same: 208 frames, 207 within 300 ms of our
212 requests. **The "no 129283 on the bus" finding of session 15 was wrong**: the analysis compared
against `0x1F803`, which is PGN 129027; 129283 is `0x1F903`. #213 is invalid and closed; 0xCD's
empty answers to our 129283 requests were already known from session 13.

**(d) The 65 uncommitted frames after the display's cold boot** (frames 1809, committed 1744 at
20:48:14) are exactly the 24 frames with byte 2 = 255 and the 41 with byte 2 = 1, 20:47:14 to
20:47:28. Correct behaviour.

**(e) Speed at the final dump: not answerable yet.** The dump (21:02:46) falls in the missing ninth
piece. In the last captured minute 65256 from 0x80 and the TECU's 65096 agree (2.60 and 2.46 km/h at
20:59:59); the ninth piece settles the standing-still reading.

**(f) The replug at 20:54:47 got a full identification, twice:** seven reads 20:54:47.275-48.157, the
display's Proprietary A at 20:54:48.743 together with a repeat read of 0x8007, then the other six
again to 20:54:49.925. Reads in the capture: 8 at 20:40, 8 at 20:47, 14 at 20:54 = 30, matching the
dump.

**For #207, the decoder rule the capture supports** (to be decided by the owner, not yet changed):
valid when byte 7 = 4 and byte 2 is 2 or 3 (1 and 255 never); side from byte 6 bits 7 (right) and 6
(left), neither = 0; engaged from byte 6 bit 1. Open policy question: should the plough take the XTE
only while the tractor's autosteer is engaged? The capture says the value is valid either way, and
the big jumps come at byte 2 transitions (line changes), not at the engage.
