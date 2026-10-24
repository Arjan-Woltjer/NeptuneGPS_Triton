# Session 21 handover -- Mookhoek 57, 2026-10-23 (field laptop neptune-gps-05 -> desktop neptune-gps-80)

**Rig:** John Deere tractor, StarFire 7000, **JD 4640 Universal Display**: VT at **0x84**, VT version 4, 480 x 480 px,
256 colour, soft keys 72 x 72 (64 virtual / 12 physical), fonts 6x8 to 128x192, no 32x32. Board: Teensy 4.1.

**Board firmware now: `f3694e7`** = test/session21 `6bd25d0` + #269 (255 packets per DPO). Pool MW09, lightbar on.
Arjan: "successfully loads". VT and TC are connected, the task is active, and the lightbar CF claimed 0xDC.

## Files (laptop -> OneDrive, NOT committed)

The laptop stayed out of git all session: OneDrive was still syncing the shared `.git`, and a fetch failed with
missing objects for `test/session20`. All builds came from a scratch clone outside OneDrive. **Please commit these
on test/session21 once OneDrive has carried them over** (check for `*-Ganymedes-*` conflict copies first):
- `Ploegbesturing Isobus/Documentation/logs/2026-10-23_session21_mookhoek57_serial.log`: every flash has a `### flashed` line.
- `Ploegbesturing Isobus/Documentation/logs/2026-10-23_session21_mookhoek57_timeline.md`: results table at the top.
- `Ploegbesturing Isobus/Documentation/logs/session21_mookhoek57_handover.md` (this file).

## Root cause and fix

AgIsoStack's ETP sender sends **16 packets per DPO** (`numberOfPacketsPerDPOMessage`) whatever the VT's CTS
allows. The 4640 then reassembles the pool wrongly and answers End of Object Pool "other error", Faulty Object NULL,
on every pool too big for TP. **Fix:** `CANNetworkManager::CANNetwork.get_configuration().set_number_of_packets_per_dpo_message(255)`
in `IsobusVtInterface.cpp`, just before `set_object_pool(...)`. This is network-wide, so the TC DDOP upload
follows the same rule. It's still bounded by each receiver's CTS.

- **Issue #269:** cause and full bisect table.
- **PR #270:** `issue-269-dpo-255` -> test/session21, commit `f3694e7`. Field-tested OK. **NOT merged:** the
  laptop's squash-merge was blocked by Claude Code auto mode (merge without review). **Arjan merges, or gives the go.**

## Bisect on the 4640 (diag/jd-pool-bisect, last at 835446c; diagnostic only, not for merging)

| Build | Pool | Transport | Result |
|---|---|---|---|
| test/session21 6bd25d0 MW09, main 9842f4a MW08 | ~13 kB | ETP | FAIL |
| 1 no scaling (MWJ1); 4 RLE transparent 0x05; 5 uncompressed | 11.8-19.4 kB | ETP | FAIL |
| 7 no 8-bit, 1-bit pictures | 3 080 B | ETP | FAIL |
| 2, 3 1-bit icon; 6 14x14 8-bit RLE icons | 0.9-1.0 kB | TP | OK |
| 9 variant 7 in two TP pieces | 3 080 B | 2x TP | OK |
| **11 MW08 content, 255 packets per DPO** | 11 760 B | ETP | **OK** |
| **f3694e7 (PR #270): MW09, 255 per DPO, lightbar on** | MW09 | ETP | **OK 20:23:57** |

Variants 4-9 and 11 were built with NO_LIGHTBAR; 1-3 and f3694e7 with the lightbar on.

Ruled out: autoscaling, 8-bit pictures, RLE, the RLE options byte, the lightbar CF, and the CAN receive drops.
Arjan cleared the 4640's stored pools around 19:07-19:08; that made no difference.

## To do (desktop)

1. **Merge PR #270** into test/session21 once Arjan agrees.
2. **The same line on main**, with a test (separate PR referencing #269).
3. **Native tests on #270**: they didn't run on the laptop (a library fetch failed on the field connection). The teensy41_isobus build is fine.
4. **Bench check** of the DPO 255 change on the **CNH VT (0x26)** and the **InCommand 1200 (0x80)** before the next van Mastwijk run. Both took 16-packet DPOs fine.
5. **#268** (filed by the desktop): boot RX ring overflow. `dropped 515` stayed constant at 19:46:21, 19:57:46 and 19:58:46, so the drops are boot-only.
6. **Lightbar emulation** (Ag Leader L160, CF 0xDC): Ag Leader-specific, ran on JD for the first time today. It did no harm visible on the serial log. Desktop proposal, still Arjan's call: start it only once an Ag Leader NAME is seen, plus a force-off setting in #263. Then file the issue.
7. **CANedge for this session:** not on OneDrive yet (that logger's LTE upload is about a day behind). Arjan to pull the SD card and copy it to `CANedge logs/card-<id>/`. Useful for: the ETP DPO/CTS sequence of a failed versus an OK upload, and other implements' pool uploads to the 4640 after Arjan cleared it.

## Notes

- The plough first bound to its stored preferred partner 0x26 (absent here). It switched to 0x84 about 10 s after boot every time ("partner 0x26 silent, VT at 0x84 alive").
- The cab wizard makes the VT time out on the diagnostic builds (#31, blocking). This is expected; Arjan agrees.
- The laptop's `neptune-gps-fa` relayed everything. Remote Control to the desktop dropped off at ~20:1x on 10-23, so the desktop may have missed the variant 11 result and everything after it. This file and #269 / #270 have it all.
