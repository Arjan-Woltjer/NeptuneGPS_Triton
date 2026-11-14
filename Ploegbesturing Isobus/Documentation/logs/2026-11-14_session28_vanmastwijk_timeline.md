# Session 28, van Mastwijk, 2026-11-14 afternoon -- timeline

Laptop clock (the serial log's). The CANedge clock ran about 3 s behind it in the morning. Rig Teensy 4.1 13249100
on `a160ce6` (v0.28). Morning run (09:45-10:12): see `session28_vanmastwijk_handover.md`.

| Time | What | Source |
|---|---|---|
| 15:34:13 | Board boots with the tractor; no CrashReport. CNH VT 0x26 by Load Version 15:34:19. 1200 off | log |
| 15:35:45 | Dump: receive queue high water 63 of 64, **dropped 328** (boot, the splash delay); stack peak 1 176, heap 47 460; TC not connected | dump |
| 15:35:44 - 15:36:45 | Calibration page on the CNH VT opened three times, saved 15:35:49, 15:35:58, 15:36:45. Owner: V11 pass; preferred VT set to 1 | log, owner |
| 15:37:01 | 1200 up: lightbar partner 0xF5 seen, plough claims 0xDC | log |
| 15:37:09 | TC 0xF7: `DDOP Activated without error` (a full upload: the 1200 keeps no DDOP, see the handover). **V6's hour starts here** | log |
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

## After V6: the calibration page, the #347 spike, the Teensy 3.2

| Time | What | Source |
|---|---|---|
| 16:06:59 | V10's dump on a160ce6 (see above) | dump |
| 16:10:26 - 16:12:42 | Calibration page on the VT: saved 16:11:11; cancelled 16:11:49; saved 16:12:31 with preferred VT 2 (plough moves to the 1200 at 0x80, Load Version); saved 16:12:42 with preferred VT 1 (back to the CNH, Load Version) | log |
| 16:13:33 | Dump before the flash: dropped 328, stack peak 1 216, heap peak 55 236, VT switches 2, width 180 cm | dump |
| 16:13:37 | Logger stopped for the flash. First try refused by `flash_teensy.ps1`'s own guard, nothing written (Windows PowerShell 5.1: `.Count` of a single PnP device is empty) | laptop |
| 16:14:20 - 16:14:28 | **Spike flashed:** `spike347/teensy41_isobus_spike347_tc09_27e8e16.hex`. The loader's log says `Programming ... Booting`; the script then reported a failure because `ExitCode` was empty (5.1 again) | loader log |
| 16:14:29 | Spike boots (from the uptime). The boot print was lost: the logger opened the port at 16:14:47 | `..._spike347_serial.log` |
| by 16:14:47 | TC 0xF7: `DDOP Activated without error`: **the 1200 accepts TC09**, one section, the width object shared by two elements | log |
| 16:15:10 | Dump: `Section (SPIKE #347): we report 161 = on, 160 = 0`; TC wrote 160: never; 290: never; requests 0, set-value 0, measurement 0; task active; on the CNH VT, preferred VT 1 and width 180 cm kept; dropped 452 at this boot | dump |
| 16:15 - 16:20 | Owner looks on the 1200 for the plough as an implement or a section: "nothing found" | owner |
| 16:20:44 | Dump: still 0 / 0 / 0, task active, tc=Y on all 334 lines | dump |
| 16:21:47, 16:21:48 | Wider pressed twice on the VT; the owner also restarted the task and moved the hitch: "nothing from the 1200" | log, owner |
| 16:22:47 | Last dump on the spike: width 183 cm, changes 3, **reported 3**; TC wrote 160: never; 290: never; requests 0, set-value 0, measurement 0 | dump |
| 16:22:54 - 16:23:04 | Flashed back to `session28/teensy41_isobus_a160ce6.hex` (script fixed: OK) | laptop |
| 16:23:05 | a160ce6 boots: v0.28, no `SPIKE #347` line; CNH VT by Load Version 16:23:08; TC `DDOP Activated without error` 16:23:14 (a full upload again: the 1200 keeps no DDOP, see the handover). Boot print: **max correction per share 100 mm** (75 this morning; set by the owner on purpose), preferred VT 1, width 183 cm | log |
| 16:24:09 | Rig 4.1 unplugged from the ISOBUS for V12 (VT and TC time out). Dump 16:24:24: dropped 423 at this boot | log, dump |
| 16:25:07 | Teensy 3.2 (7515900, `teensy32_isobus_4c96780.hex`) boots on USB: v0.28, identity 751590, claims 0x81. Its stored settings: offset calibration 600/34, 4 shares, max correction per share 50 mm, preferred VT 0, plough side 1 | `..._teensy32_serial.log` |
| 16:25 - 16:29 | The 3.2 on the ISOBUS plug: **not one frame received** (msgs=0, receive queue high water 0 of 64, 1 Hz line load 0.0 %), `VT: partner 0xFE silent` every 10 s. Dump 16:27:11: heap peak 33 852 of 58 868 B, stack 23 872 B never used, longest loop pass 3 047 us | log, dump |
| about 16:28 | Owner: CAN high and low are swapped on this board, "and it's messing with the bus". Plug pulled. Owner afterwards: the bus is fine again | owner |
| 16:29:44 | Loggers stopped. The rig 4.1 was not put back on the tractor | laptop |

## Results of the afternoon

- **#347 spike on the InCommand 1200:** the 1200 accepts a one-section DDOP (TC09) and then ignores it exactly as it
  ignores TC08: in 8 minutes with an active task it sent no value request, no measurement command and no command on
  DDI 160 or 290, also after a task restart, two width steps and a hitch move
  (the bus shows the width steps fell in a stopped task and the hitch move did not change the work state, see the
  handover). The echo hex was not flashed: it only
  differs after the TC writes DDI 160, and the TC wrote nothing. The TC10 hex was not needed.
- **V12 not run:** this Teensy 3.2 board has CAN high and low crossed at its connector. It heard nothing and its
  own transmit attempts disturbed the tractor's bus until it was unplugged. The CANedge has that interval
  (about 16:25-16:28 laptop time).
- **Left as found?** The rig 4.1 is off the tractor. On the rig board: width 183 cm (200 cm at 09:45), max correction
  per share 100 mm (75 at 09:45), preferred VT 1. The tractor's VT numbers are still swapped (1200 = VT 2).
