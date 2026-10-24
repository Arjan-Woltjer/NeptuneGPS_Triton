# Session 21 -- Mookhoek 57 (John Deere, StarFire 7000, JD 4640 display), 2026-10-23: timeline

Serial log: `2026-10-23_session21_mookhoek57_serial.log` (all times local, from the logger; every flash is marked
with a `### flashed` line). CANedge: running from the start of the session (Arjan). Today's upload had not reached
OneDrive by 20:00; the workstation's newest was session 90 (10-22). Read it from the SD card later.

Builds were made on the laptop from a scratch clone outside OneDrive. OneDrive was still syncing the shared
`.git`, and a fetch there failed with missing objects, so the laptop stayed out of git. The workstation
(`neptune-gps-80`, via `neptune-gps-fa` on the laptop) analysed the results and pushed the diagnostic branch
`diag/jd-pool-bisect`.

## Result

**ROOT CAUSE (variant 11, 20:13:27): AgIsoStack's ETP sender sends 16 packets per DPO
(`numberOfPacketsPerDPOMessage = 16`) even when the VT's CTS allows more.** The JD 4640 then reassembles the pool
wrongly and answers End of Object Pool "other error" with no object. The same MW08 pool, sent with 255 packets per
DPO, stores fine. **Production fix: DPO packet count 255** (desktop, on main and test/session21, with a test; bench
check on the CNH VT and InCommand 1200). Before this was found:

**The JD 4640 rejects every VT pool sent by ETP** with End of Object Pool "other error", naming no object. It
accepts the same content when it arrives as TP-sized transfers (variant 9). Production fix (desktop, on main with
tests): send the pool in pieces under ~1.7 kB, and cut the 8-bit plough pictures into strips.

| Build | Pool | Transport | 4640 |
|---|---|---|---|
| test/session21 6bd25d0 (MW09) | ~13 kB | ETP | FAIL |
| main 9842f4a (MW08) | ~13 kB | ETP | FAIL |
| bisect 3: no scaling, 1-bit icon (MWJ3) | 872 B | TP | OK 19:05:16 |
| bisect 1: no scaling, 8-bit RLE pictures (MWJ1) | ~11.8 kB | ETP | FAIL |
| bisect 2: scaling, 1-bit icon (MWJ2) | 872 B | TP | OK 19:13:12 |
| bisect 4: RLE pictures marked transparent (0x05), NO_LIGHTBAR | 11 760 B | ETP | FAIL |
| bisect 5: pictures uncompressed, NO_LIGHTBAR | 19 376 B | ETP | FAIL |
| bisect 6: 14x14 8-bit RLE icons, NO_LIGHTBAR | 1 028 B | TP | OK 19:44:16 |
| bisect 7: no 8-bit, 1-bit 120x76 pictures, NO_LIGHTBAR | 3 080 B | ETP | FAIL |
| **bisect 9: variant 7 split into 1 597 + 1 483 B, NO_LIGHTBAR** | 3 080 B | 2x TP | **OK 19:59:48** |
| **bisect 11: MW08 content, 255 packets per DPO, NO_LIGHTBAR** | 11 760 B | ETP | **OK 20:13:27** |

Ruled out: autoscaling (2 OK), 8-bit pictures as such (6 OK), RLE (5 fails), the RLE options byte (4 fails), the
Ag Leader lightbar CF (4, 5 and 7 fail without it; 3 OK with it), the CAN receive drops (boot-only, below).

VT (dump): address **0x84**, VT version 4, 480 x 480 px, 256 colour, soft keys 72 x 72 px, 64 virtual / 12
physical, fonts 6x8 to 128x192 (no 32x32). The van Os JD (session 11) was at 0x26.

## Findings

- **ETP pool upload to the JD 4640 fails** (the production fix above).
- **CAN receive ring overflows at boot:** `CAN receive queue: high water 63 of 64, dropped 515`, unchanged at
  19:46:21, 19:57:46 and 19:58:46 while uploads kept failing. Cause (desktop): CAN starts in setup() before the
  2 s splash `delay(2000)`, and nothing drains the 64-frame FlexCANT4Plugin ring meanwhile. Separate bug; the
  desktop will file it.
- **Ag Leader L160 lightbar emulation** (CF 0xDC) is Ag Leader-specific and never tested on JD (Arjan); it
  should not run on this rig. `NO_LIGHTBAR` exists on the diagnostic branch only. Desktop proposal, Arjan's call:
  start it only once an Ag Leader NAME is seen, plus a force-off setting.
- Arjan cleared the 4640's stored pools around 19:07-19:08. It made no difference to the ETP failures.

## Timeline

- 18:42:26 Serial logger started (laptop), COM4. Board already on the bus: TC connected, VT pool rejected
  (`No version label ... upload`, then EOP `Other error`), retried every 8 s at 53-55 % bus load.
- 18:42:31-~18:45 Plough unplugged by Arjan: no ACKs, error-passive, one bus-off (expected). Plugged back in.
- 18:54:03 Flashed main 9842f4a (MW08): same rejection.
- 19:04:52 bisect 3 (MWJ3): **stored**; dump with the VT capability lines.
- 19:07:08 bisect 1 (MWJ1): fails 19:07:34 onwards.
- 19:09:01 test/session21 6bd25d0 (MW09): fails.
- 19:12:48 bisect 2 (MWJ2): **stored** 19:13:12.
- 19:29:29 bisect 4 + NO_LIGHTBAR: fails 19:29:54.
- 19:31:38 bisect 5 + NO_LIGHTBAR: fails 19:32:05.
- 19:43:52 bisect 6 + NO_LIGHTBAR: **stored** 19:44:16.
- 19:45:51 bisect 7 + NO_LIGHTBAR: fails 19:46:15.
- 19:59:22 bisect 9 + NO_LIGHTBAR: **stored** 19:59:48. The plough stays on variant 9.
- 20:09 Arjan: **the VT loads** on the 4640 with variant 9 (screens shown).
- 20:11 Arjan: calibrating (cab wizard) makes the VT time out: **expected** on this build (the wizard blocks the loop, #31; the non-blocking wizard is #264 on test/session21, not on the diagnostic branch).
- 20:13:01 bisect 11 + NO_LIGHTBAR (835446c, MW08 content, ETP with 255 packets per DPO): **stored 20:13:27**. Arjan: "loads". Root cause found.
- 20:23:31 Flashed **issue-269-dpo-255 f3694e7** (test/session21 6bd25d0 + DPO 255, MW09, lightbar ON). 20:23:57.194 **Stored object pool with no error**; VT and TC connected, task active, lightbar 0xDC claimed. Issue **#269**, PR into test/session21.
- 20:25 Arjan: **MW09 successfully loads** on the 4640 with f3694e7. PR #270 ready to squash-merge into test/session21 (the merge was not done from the laptop: needs Arjan's go).
