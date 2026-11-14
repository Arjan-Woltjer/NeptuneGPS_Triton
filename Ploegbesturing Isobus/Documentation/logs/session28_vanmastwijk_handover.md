# Session 28 part V, van Mastwijk, 2026-11-14 09:45-10:12 -- handover

Written on the rig laptop at 14:40. The workstation's Remote Control session was not reachable from the laptop
session at that time, so this file is the handover.

## Where the data is

- Serial log: `2026-11-14_session28_vanmastwijk_serial.log` (c7217bb). One boot, 1 Hz line on from 09:46:09, laptop
  clock. Two dumps only, at 09:45:49 and 09:46:14 (the laptop session dropped around 09:47; the logger kept running
  until 10:12, but nobody sent a dump during the tests).
- CANedge session 129, pieces 1-7, 09:39:54-10:09:59 on the CANedge clock, which runs about 3 s behind the laptop.
  The last piece (10:10-10:12) is still on the card and uploads at the next power-up.
- Sheet https://claude.ai/artifact/GDziQaEETM9hcoFg3BqbJV: rows V1, V2, V3, V5, V8, V9 carry the evidence; the
  owner's statuses are kept as he ticked them.

## The board

Rig 4.1 13249100 on `teensy41_isobus_a160ce6.hex`, v0.28, identity 1324910, address 0x81. No CrashReport, no reboot
(uptime 1 624 s at the end). Stack peak 1 776 -> 1 936 B of 228 512, heap peak 58 728 -> 60 996 B.

## Steps

| Step | Owner | Evidence |
|---|---|---|
| V1 | pass | MW18 uploaded once on each VT: CNH 0x26 2.2 s (09:45:26.4-28.5), 1200 0x80 1.6 s (09:48:18.8-20.5), both "Stored object pool with no error". TC 0xF7: "DDOP Activated without error" 09:48:17, server version 3. Numbers still swapped: 1200 = VT 2, CNH = VT 1 |
| V2 | fail | The 1200 shows no width change. On the bus the plough sent DDI 67 on every step (54 values, 2000 -> 2070 -> 1790 mm, 09:53-10:04). The 1200's TC sent **no value request, no measurement command and no PDACK for any DDI** in the whole session, with its task active. `reported` not read (no dump) |
| V3 | fail | The 1200 does not act on DDI 141. The plough sent DDI 141 = 0 / 1 / 0 at 10:05:40 / 10:06:21 / 10:07:11 (CANedge clock), with the hitch. Never asked for, never acknowledged |
| V4 | pass | Owner: the VT state line follows the cab LCD exactly. Object 172 on the bus: 173 AUTO, 174 hold speed, 175 hold GPS (at boot and after the replug), 176, 177 CAL. No deliberate RTK drop; the #344 case (stops following in AUTO) was not reported |
| V5 | fail | Assigned on the CNH (VT 1) at 10:02; Breder 28100 and Smaller 28102 pressed and released. After the 20 s unplug the assignments were gone and had to be made again (10:03:38; presses 10:04:34, Smaller now 28101). **Not done:** the plough's power cycle; numbers back + the refusal on the CNH |
| V8 | pass | 1200 silent 10:09:51, plough on the CNH 10:10:04 (Load Version); 1200 back 10:10:46, plough back on it (Load Version), TC DDOP active 10:10:49 |
| V9 | pass (owner, 14:45; the fail tick at 10:11 was a mistake) | The unplug was 10:02:42-10:03:02, standing, not on the line in AUTO. XTE 0.04 m and width 1800 mm before and after; VT back 10:03:04 after one "The VT Server is NACK-ing our VT messages"; TC back 10:03:09. The AUX-N assignment lost at this replug is V5 |
| V6, V7, V10, V11, V12, part J | open | Both VTs were on the bus for 24 minutes, not an hour; no dumps, so no PGN 129029 count and no end-of-day receive-queue figure |

## Findings

1. **V2/V3 are not a regression of #162.** Session 112 of 2026-11-09 (before TC08, NAME identity 1) shows the same:
   the only process data between the plough and TC 0xF7 is our unsolicited DDI 141. The 1200's TC never subscribes
   to anything from this DDOP.
2. **AUX-N after a reconnect (#327).** At 10:02:59.916 (CANedge clock) the plough sent an empty preferred
   assignment (`22 00`), the CNH answered without error; at 10:03:00.896 it sent the stored one by TP (24 bytes) and
   the CNH answered `22 10` = "any other error", faulty object FFFF. Log 10:03:03.921: `VT rejected the preferred
   assignment command (error flags )`.
3. **Receive queue at boot.** `CAN receive queue: high water 63 of 64, dropped 352` 31 s after boot on a 12 % bus,
   still 352 at 56 s. Bench: 2 of 64, dropped 0. Probably the seconds before the stack starts draining; V6's
   "dropped 0" cannot hold as written.
4. **Log formatting.** `[NM]: A partner with name 000000000000000lx has claimed address 536925300 on channel
   197275905`: the 64-bit NAME is not formatted and the other arguments are shifted; the AUX-N `error flags` print
   is empty. New with this build (#343 removed the libc shim?).
5. `[TP]: Received a Request to Send (RTS) message with a global destination, ignoring`: 72 times, 09:48:10 to
   10:11:13, only while the 1200 is up.

## State the tractor was left in

Per the log: preferred VT 2, saved from the VT at 10:09:34; the plough went to the 1200 at 0x80 as VT 2 at 10:10:46,
so the VT numbers are still swapped. The owner's sheet note says "1200 = vt2! preferred vt set to 1": to be
confirmed. Bus off at 10:12:12.

## Next visit needs

A dump (`m1q`) after every step, V5's power cycle and refusal check, V6's hour, V10, V11's photos, V12 and part J.
