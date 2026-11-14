# Session 28 part V, van Mastwijk, 2026-11-14 09:45-10:12 -- handover

Written on the rig laptop at 14:27. The workstation's Remote Control session was not reachable from the laptop
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
| V9 | pass (owner, 14:39; the fail tick at 10:11 was a mistake) | The unplug was 10:02:42-10:03:02, standing, not on the line in AUTO. XTE 0.04 m and width 1800 mm before and after; VT back 10:03:04 after one "The VT Server is NACK-ing our VT messages"; TC back 10:03:09. The AUX-N assignment lost at this replug is V5 |
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

**Owner, 2026-11-14 (evening):** CNH = VT 1, 1200 = VT 2 and preferred VT 1 (the CNH) is the intended state. Both
terminals used to be VT 1; he first made the 1200 VT 1 and the CNH VT 2 for the farmer, AUX-N did not work that way
either, so he turned them round. Nothing has to be put back. The afternoon's last save left preferred VT 1.

What the morning's log showed:

Per the log: preferred VT 2, saved from the VT at 10:09:34; the plough went to the 1200 at 0x80 as VT 2 at 10:10:46,
so the VT numbers are still swapped. The owner confirmed this at 14:40: the board is still on preferred VT 2, and
the preferred VT **still has to be set to 1** (his sheet note "preferred vt set to 1" was the intention). Bus off
at 10:12:12.

## Next visit needs

A dump (`m1q`) after every step, V5's power cycle and refusal check, V6's hour, V10, V11's photos, V12 and part J.

## Checked on the workstation (2026-11-14, afternoon)

Read against the serial log, the firmware source and CANedge session 129 (`bucket-AD4F266A/session-00000129`).

- **Finding 2, the rejected preferred assignment: the frames.** The plough announced **24 bytes** in 4 packets
  (`10 18 00 04`, 10:03:00.896, 1 ms after the Load Version response). The content is 21 bytes: `22 01`, NAME
  A00086000BC9F19F, model 85, 2 functions, 170 -> 28100, 171 -> 28102. The last packet is `04 ff ff ff ff ff ff ff`:
  three 0xFF bytes counted into the message length. AgIsoStack does this on purpose (`prepare_auxiliary_assignment_
  transaction_locked()`, "padding to the next eight-byte boundary", upstream e399f72, still on main e2bda27). The CNH
  answered `22 10 ff ff` at 10:03:00.918. The CNH's own Auxiliary Assignment to us is a 14-byte TP message, not
  padded to 16. **Likely cause, not proven:** the CNH counts three bytes too many. Second candidate: the message
  came 1 ms after the Load Version response. The empty one (`22 00`, 8 bytes, no padding question) was accepted at
  10:00:58.573.
- **Finding 3, the 352 dropped frames: explained.** `setup()` starts the ISOBUS stack and then waits in
  `delay(2000)` for the splash screen (src/main.cpp:355) before the first `loop()`. Nobody reads the receive queue
  in those 2 s: 64 + 352 = 416 frames in 2 s is 208 frames/s, which is a 12 % bus. The count stayed 352, so nothing
  was dropped after boot. For V6: compare against the boot figure, not against 0.
- **Finding 4, the log formatting: old, not from #343.** The `000000000000000lx` line is upstream #923 (newlib-nano
  has no `%llx`); it was noted at W3 on the bench on 2026-11-13. The empty `error flags )` is a second one:
  `isobus::to_string()` writes a `std::uint8_t` as a character; the raw byte 0x10 is in the log file.
- **Finding 5, the count.** The 72 lines are 54 "Request to Send" plus 18 "Abort" with a global destination.

## The XTE gap at 09:53 (added 14:59, rig laptop)

The serial log has two XTE gaps while the 1200 was up (laptop clock):

| Gap | VT 0x80 | TC | GGA | Cause |
|---|---|---|---|---|
| 09:53:29 - 09:56:16 | connected, status never older than 0.5 s | connected | arriving | The owner **stopped the task on the 1200** (his recollection, "might have been"). TC status: task inactive 09:53:22, active again 09:55:03 (CANedge clock); the XTE came back about 70 s after that |
| 10:09:55 - 10:10:53 | lost | lost | lost | The 1200 switched off for V8 |

So the 1200 stops sending XTE while its task is stopped, with position and RTK quality still arriving. The bits of
PGN 0xFFB6 from 0xF5 that change at 09:53:22 and 09:56:14 therefore mark the task or the line, not necessarily the
autosteer engage; V13 has to separate the two (nothing else touched on the 1200 during its runs).

## The #347 spike seen from the bus (added in the evening, workstation)

CANedge session 130 (11 pieces, 15:33:03 - 16:24:59) covers the afternoon up to the flash back to a160ce6. The
piece with the crossed Teensy 3.2 (from about 16:25) is still on the card. Times below are the CANedge clock,
about 3 s behind the laptop's.

**The spike's result holds.** Between the TC connect at 16:14:41.9 and the flash at 16:23:04 the 1200's TC (0xF7)
sent the plough exactly six process-data frames (plus the transport flow control for the upload), all of them the
connect handshake: version response, version request,
structure label response, transfer permission, transfer response, activation response. No value request, no
set-value, no measurement command, no acknowledgement.

| Time | Direction | Frame | Meaning |
|---|---|---|---|
| 16:14:41.874 | F7 -> 81 | `10 03 28 14 00 01 90 01` | the 1200's version response, as always |
| 16:14:41.878 | 81 -> F7 | `10 04 ff 14 00 01 01 01` | ours on the spike: section control + TC-GEO with position, 1 boom, 1 section, 1 channel (a160ce6 sends `10 04 ff 04 00 00 00 01`) |
| 16:14:41.884 | F7 -> 81 | `11 ff ff ff ff ff ff ff` | structure label response: **no label stored** |
| 16:14:41.886 | 81 -> F7 | `41 8b 03 00 00` | request to transfer 907 bytes (a160ce6: 716) |
| 16:14:42.000 | F7 -> 81 | `71 00 8b 03 00 00` | transfer accepted |
| 16:14:42.005 | F7 -> 81 | `91 00 ff ff ff ff 00 ff` | activated without error |

Three things the bus adds to the serial log:

1. **The 1200 keeps no DDOP.** It answered the structure label request with all 0xFF at each of the three connects
   of the afternoon (15:37:07 TC08, 16:14:41 TC09, 16:23:13 TC08 again), and the plough uploaded the whole DDOP every
   time. So "TC08 was still stored" at 16:23 is not what happened: it was a fresh upload, as on every boot (the
   2026-11-10 field-day document already counts one DDOP transfer per boot). The label only matters on a TC that
   stores, such as the John Deere's.
2. **The two Wider presses fell in a stopped task.** The TC status shows the task inactive at 16:16:18 - 16:16:22,
   16:17:06 - 16:20:11 and 16:21:34 - 16:22:05. The plough sent DDI 67 = 1810, 1820, 1830 for elements 2 and 3 at
   16:21:46 - 16:21:47, inside the last gap. Of the 502 s on the spike the task was active for about 282 s. The
   three dumps (16:15:10, 16:20:44, 16:22:47) were all taken with the task active, as they say.
3. **The hitch move did not reach the plough.** The plough sent no DDI 141 and no DDI 161 value in the whole of
   session 130, and every dump of the afternoon says `1 working (hitch down)`. This morning the same board sent
   DDI 141 = 0 / 1 / 0 with the hitch (10:05:40, 10:06:21, 10:07:11). So the section state never changed on the
   bus during the spike.

What that means: "one section alone does not make the 1200 subscribe" stands, because a TC that wanted the section
would have set up its measurements or written DDI 160 / 290 after the activation, and the task was active for
almost five minutes. What the spike did not show is how the 1200 reacts to a width or a work-state change with a
section present and the task running. For the try on the John Deere: press the width keys and move the hitch
while the task is active, and check on the dump that the work state really went 1 / 0 / 1.

The XTE stopped at 16:17:07 with the task (`xteAge` counts up from there in the spike's serial log), which agrees
with the morning's finding that the 1200 sends no XTE while its task is stopped.

## Answer to an open question of the field days (owner, 2026-11-14)

The capture document of the 2026-11-09 / 11-10 field days (`2026-11-10_fielddays_vanmastwijk_capture.md`, on
`test/session25`) asks why the plough stopped coming down in AUTO on 2026-11-09, 11:16:58 - 11:35:36 (session 112),
with the XTE staying high. The farmer has confirmed to the owner that the pressure hose of the load-sensing
hydraulics came out of its socket a few times. That is cause 1 of that document's list (hydraulics), not the valve
output, the sensor or the firmware. The farmer gave no times, so which stretches it explains is not confirmed one
by one; session 112 fits, and the frozen position on 2026-10-19 (session 72, 18:26 - 18:36) may be the same.
