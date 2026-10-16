# Session 17 handover (field laptop -> workstation), 2026-10-16

No workstation session was reachable over Remote Control at the end of the session, so this file is the handover.

**Files** on `test/session12` (pushed): `logs/2026-10-16_session17_vanmastwijk_timeline.md` (checklist, timeline,
results table) and `logs/2026-10-16_session17_vanmastwijk_serial.log`.
**Board** 1324910 on `test/rig-lightbar-failover` @ `2745ff4` (build check passed: `stack partner=` field present).
**Capture**: CANedge session **57** (OneDrive `CANedge logs/bucket-AD4F266A/session-00000057/`, 7 pieces at 09:44,
the logger was left running so the rest uploads by itself). Session 56 piece 9 (10-13, 21:00-21:03:51) arrived at
09:05 today.
Start order: CNH VT up, InCommand 1200 off, plough on the bus 09:19:27 (bound to the CNH VT 0x26), 1200 on ~09:21.

## Verified

- **#190 (rebased AgIsoStack-plus-plus, pin 4625be4) on hardware: R1/R2 pass.** MW08 from NVM on the CNH VT
  (join, R2 replug, F1) and on the 1200 (F3); TC 0xF7 each time; identification 8/8 three times (30/30), Prop A 3x.
- **#207 0xF5 partner (2745ff4): works.** `stack partner=0xF5` after every identification; 0xF5 never pruned at
  the R2 plug-out, the CNH VT off, or the CNH reboot (0xE9 was, each time).
- **#188 + #774 failover: F1-F4 pass.** CNH off -> 0x80 in ~15 s; 1200 off -> 0x26 in ~15 s (session 16: 20-25 s);
  no switch back in either direction; switches=2 at the end.
- D1 autosteer drive on the AB-line-adjacent passes: `xte=` = `lb=` with the sign. D2: the XTE holds while byte 2
  is not 2/3. D3 speed: plough = 1200 (Arjan).
- **G, the 1200's "GPS lost"**: triggered by switching the CNH VT off, and by rebooting it. On the bus 0x26, **0xAC
  (172, the CNH GPS module per Arjan)** and 0xCD go silent. **0x80's position/speed/65535 and 0xF5's 65462 keep
  coming, live** (GGA age ~50 ms, XTE age ~150 ms throughout); the plough never went stale, so there was nothing to hold.

## Finding: byte 2 of PGN 65462 is the PASS NUMBER (#207, blocks AUTO on some passes)

Arjan drove passes and read the labels off the 1200:

| 1200 label | byte 2 | byte 6 bit 4 | byte 6 bit 5 |
|---|---|---|---|
| 0 (AB line) | 1 | | set |
| 1L | 2 | set | |
| 1R | 2 | | set |
| 2L | 3 | set | |
| 3L (overshoot) | 4 | set | |

So byte 2 = |pass| + 1 (byte 3 stayed 0), side of the AB line in byte 6 bit 4 (L) / bit 5 (AB line, R). None of it
is a validity flag; session 16's "2/3 valid, 1 = no solution" readings were passes crossed in turns. With 2745ff4's
rule (commit on 2 or 3), `xte=` freezes for a whole pass on the AB line (09:26:57-09:28:13, 75 s, autosteer engaged
and converging 34R -> 0) and would on 3L+. **Proposed rule: commit when byte 7 == 4 and byte 2 != 0xFF; log the
pass as (byte 2 - 1) with L/R from bits 4/5.** Arjan decides; D4 (AUTO with the hitch down) was not done today.

## Other observations

- Lightbar `failures=1` once, during the 1200's first start-up (between 09:20:29 and 09:23:22), never again.
  Serial does not say which frame. Candidate: the second softwareId BAM refused while the first ran (asked twice).
- 2 bus-off + 3 error-passive entries (TX peak 248) in the R2 plug-out window. Session 16 had 0 bus-off.
- `[VT] Server response to a command timed out` from the CNH VT at 09:25:15 and 09:31:07 while connected, no effect.
- `[VT] The VT Server is NACK-ing our VT messages` once at the R2 rejoin (09:33:26), with 3 NACKs of PGN 60928 in the
  same ms; recovered at once (pool from NVM 0.3 s later).
- TC needed 5 restarts (15 s apart) while the 1200 booted in F2; activated 09:41:18.
- 7x `[AC] Address violation for address 129` at the join (session 16: 12): known.

## Questions for the capture (session 57)

1. Inventory: SA 0x81 and 0xDC on the wire (bus tap not checked at the rig).
2. 65462 across all passes: confirm byte 2 = |pass| + 1 and bits 4/5 = side; anything in bytes 3-5 that changes
   with the pass; what byte 4 is.
3. G (09:35:44 CNH off, 09:37:25 reboot): does 0x80's 65267 keep moving or freeze; what 0xAC sent before it went
   silent (and what the 1200 uses it for); which 0x80/0xF5 PGNs change during "GPS lost".
4. The lightbar send failure between 09:20:29 and 09:23:22: which 0xDC frame is missing (softwareId BAM overlap?).
5. The R2 rejoin at 09:33:26: who NACKed what (the "VT Server is NACK-ing" line).

## Next

- Decoder fix on #207 (byte 7 gate only, pass number reported), native tests, flash, then a pass on the AB line and
  on 1R with AUTO and the hitch down.
- #190 can merge on R1/R2's evidence.
