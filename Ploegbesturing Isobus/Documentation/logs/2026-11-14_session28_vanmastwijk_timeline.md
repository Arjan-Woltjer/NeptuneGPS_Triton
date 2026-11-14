# Session 28, van Mastwijk, 2026-11-14 afternoon -- timeline

Laptop clock (the serial log's). The CANedge clock ran about 3 s behind it in the morning. Rig Teensy 4.1 13249100
on `a160ce6` (v0.28). Morning run (09:45-10:12): see `session28_vanmastwijk_handover.md`.

| Time | What | Source |
|---|---|---|
| 15:34:13 | Board boots with the tractor; no CrashReport. CNH VT 0x26 by Load Version 15:34:19. 1200 off | log |
| 15:35:45 | Dump: receive queue high water 63 of 64, **dropped 328** (boot, the splash delay); stack peak 1 176, heap 47 460; TC not connected | dump |
| 15:35:44 - 15:36:45 | Calibration page on the CNH VT opened three times, saved 15:35:49, 15:35:58, 15:36:45. Owner: V11 pass; preferred VT set to 1 | log, owner |
| 15:37:01 | 1200 up: lightbar partner 0xF5 seen, plough claims 0xDC | log |
| 15:37:09 | TC 0xF7: `DDOP Activated without error` (TC08 stored). **V6's hour starts here** | log |
| 15:37:20 | Dump: `preferred=VT1 at 0x26 (bound)`; TC connected, task active, bound TC 1 manufacturer 97 version 3 booms 1 sections 144; requests 0, measurement commands 0; dropped 328; PGN 129029 none received; lightbar `autosteer off` | dump |
| 15:37:45 | RTK quality 1 -> 4 | 1 Hz line |
| about 15:37:56 | V13: owner presses the autosteer enable switch. It was already on "field"; to enable after a restart it has to go to road and back to field | owner |
| before 15:39:38 | V13: owner presses engage, tractor standing. The 1200 refuses: "heading not ok". Lightbar flag (PGN 65462 byte 6 bit 1) stays off, byte 6 = 0x51 | owner, 1 Hz line |
| 15:39:42 | Tractor starts to move (0.58 m/s) | 1 Hz line |
| 15:39:46 | V13: **engaged**. Lightbar flag on (`lb=..e..`, byte 6 0x91 -> 0x93) | 1 Hz line, owner "pressing again now" |
| 15:40:16 | V13: disengaged, flag off (byte 6 0x51) | 1 Hz line, owner "pressed again to disengage" |
| 15:40:33 | V13: engaged again, flag on (0x53) | 1 Hz line, owner "reversing, pressed auto again" |
| about 15:41:28 | Owner presses the other side of the button, meaning unknown | owner |
| 15:41:43 | Flag off (0x51), still moving | 1 Hz line |
| 15:41:52 | Tractor stands still | 1 Hz line |
| 15:41:55 | Engaged while standing, flag on (0x53) | 1 Hz line, owner "engaged, standing still it should disengage" |
| 15:42:25 | Flag off (0x51) after 30 s standing still, no speed | 1 Hz line, owner "now" |

V13 so far: PGN 65462 (0xFFB6) from 0xF5, byte 6 bit 1, followed the engage switch on all four engagements and
all four releases, within the 1 s resolution of the 1 Hz line. It is the bit `IsobusPgnDecode` already decodes as
`engaged` (owner's decision 2026-10-13: reported only, not a commit condition) and the bit the workstation picked
out of the morning capture. The enable (road/field) switch shows no change in this message.

## V6, V7, V10 (15:37:09 - 16:07:18)

The owner agreed at 15:46 to stop V6 after 30 minutes and use the light for the #347 spike. Dumps at 15:37:20,
15:44:02, 15:53:59 and 16:06:59.

| Check | Result |
|---|---|
| VT and TC | 1 810 one-second lines, every one `vt=Y(21/22) tc=Y`; plough on the CNH VT (preferred VT 1), TC 0xF7 with an active task |
| Offline, `E] [VT]`, `W] [VT]` | 0, 0, 0 |
| Stalls | no gap over 3 s between 1 Hz lines |
| Receive queue | dropped 328 at all four dumps: the boot figure, nothing lost while running |
| Stack / heap | stack peak 1 176 B throughout; heap peak 47 460 -> 47 980 B |
| GNSS (V7) | PGN 65535 status from 0x80: 185 -> 4 199 -> 10 177 -> 17 977; RTK fixed, quality 4 on 1 774 lines; PGN 129029 none, as always on this bus |
| TC requests | requests 0, measurement commands 0, width `reported 0` (no width change this afternoon) |
| `[TP]` warnings | 29, from 0xCD after the 1200 came up |
