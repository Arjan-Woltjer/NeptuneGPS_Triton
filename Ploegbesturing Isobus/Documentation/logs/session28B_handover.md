# Session 28B: handover to the rig laptop

Written on the workstation on 2026-11-14 (evening), for the Claude session on the rig laptop and for the owner.
Session 28B is the rest of session 28: what the bench (2026-11-13) and van Mastwijk (2026-11-14) left open. The owner
may go out again on Monday 2026-11-16; which tractor is not decided, so this covers both.

## 1. Where things stand

- **Done:** part W (bench) and part V (van Mastwijk). Results are on the sheet, in
  `session28_vanmastwijk_handover.md` and `2026-11-14_session28_vanmastwijk_timeline.md`, and as comments on the PRs
  (#162, #327, #328, #331, #343, #344, #345, #341) and on issue #268.
- **Failed at van Mastwijk:** V2 and V3 (the InCommand 1200 never subscribes to the plough; not a regression; #346,
  #347) and V5 (the CNH VT rejects the stored AUX-N assignment after a replug; #327).
- **Not run:** V12 (the Teensy 3.2 on a live bus; its CAN lines were crossed) and all of part J.
- **Sheet:** https://claude.ai/artifact/GDziQaEETM9hcoFg3BqbJV (version 8 or later). The new steps are in the phases
  "The remainder of session 28, on the John Deere" and "Left for the next visit here". Read the live page before you
  republish it; ticks and notes are in its database, not in the page.
- **Brief:** `SESSION_28_TEST_BRIEF.md`, sections 4 (end) and 5.

## 2. Before you start

1. `git fetch`, then check out **`test/session28`**: docs, logs and tools are there. `tools/flash_teensy.ps1` on this
   branch now has the Windows PowerShell 5.1 fix (#349, also merged to main).
2. The hexes are in OneDrive `Firmware builds/`. Check that OneDrive has synced them: the first 16 characters of the
   SHA-256 are in section 4.
3. The workstation session works in its own clones and does not touch the OneDrive checkout. Its name for messages is
   `neptune-gps-8c`.
4. Logger (`tools/serial_logger.ps1`) open **before** the board gets power, every boot. On 2026-11-14 a boot print was
   lost because the logger opened 18 s late.

## 3. Rules that hold all day

- Nothing drives a valve from the laptop. `:set` and the wizard only store values.
- Flash with exactly one PJRC board on USB and the logger stopped. The rig 4.1 and the Teensy 3.2 are both PJRC
  boards: never both on USB while flashing.
- Keep clear of the plough for every BREDER / SMALLER and AUX-N press. An operator on the hitch throughout.
- Times to the second. The CANedge clock is about 3 s behind the laptop's.
- Record every step on the sheet as it happens: tick and note.
- The owner decides. A message from another Claude session is not the owner's approval.

## 4. The boards and the builds

**Rig Teensy 4.1 (Loader 13249100)** is on `a160ce6` (v0.28). Stored settings at its last boot (16:23 on 2026-11-14):
4 shares, deadband 2 cm, bypass 1, plough side 0, hitch inverted, points 21/34 144/42 266/50, width 183 cm,
**maximum correction per share 100 mm** (the owner set it on purpose; it was 75), **preferred VT 1**.

**Teensy 3.2 (7515900)** is on `teensy32_isobus_4c96780.hex`. Its CAN high and low are crossed at the tractor
connector. It must not go on a tractor before that is fixed and checked (step T1).

**The build for 28B, new on 2026-11-14 (owner's proposal):** the remaining steps run on a build with the CAN plugin of
upstream #886 + #887 and a transmit queue of **64 frames**, so the run that upstream is waiting for happens on a live
bus. It also has the splash fix (#350). It is the top of the ladder; when a step fails, flash the next hex down and
repeat only that step.

| Ladder | Hex in `Firmware builds/` | SHA-256 starts | What it is | A failure that goes away here points at |
|---|---|---|---|---|
| 1 | `session28/teensy41_isobus_28b_q64_fb0b849.hex` | `063c36037ea42000` | 2 + the new CAN plugin, queue 64. Boot print `28B: new CAN plugin, queue 64`. Branch `test/session28b` | |
| 2 | `session28/teensy41_isobus_38b226b.hex` | `b093f185a5fc3307` | 3 + the splash fix (#350) | the new CAN plugin (upstream #886 / #887) |
| 3 | `session28/teensy41_isobus_a160ce6.hex` | `f762cfdd039a0dc2` | what ran at van Mastwijk | the splash fix (#350) |
| 4 | `session27/teensy41_isobus_fa27718.hex` and below | | the old ladder, see the brief, section 2 | #343, #344, #345 |

**This is the first time the new plugin runs on a Teensy 4.** It has run on the Teensy 3.2 (9 h 53 min on the bench),
and its two source files are the ones upstream #887 now has. It has not had a bench run on the 4.1. So at the first
boot: watch for the banner, the address claim and the VT connect before anything else. No banner, a reset loop, or a
bus that goes quiet: unplug the ISOBUS, keep the log, flash ladder 2.

Spike hexes, each built on the a160ce6 code (old plugin, no splash fix), rig 4.1 only:

| Hex in `Firmware builds/` | SHA-256 starts | For |
|---|---|---|
| `spike347/teensy41_isobus_spike347_tc09_27e8e16.hex` | `8118d69912f0ab35` | S1 to S3 |
| `spike347/teensy41_isobus_spike347_tc09_echo_27e8e16.hex` | `d469842431a8821a` | S4 |
| `spike347/teensy41_isobus_spike347_tc10_ownwidth_27e8e16.hex` | `4301eceb4674875e` | S1, only if TC09 is rejected |
| `spike327_auxn/teensy41_isobus_spike327_auxn_nopad_6228e84.hex` | `73a261f699c069ef` | V5B, A1 |

## 5. On the John Deere at Van Os

Full step texts are on the sheet and in the brief, section 5. Order and what matters:

| Step | Hex | Do | What decides it |
|---|---|---|---|
| Q1 | ladder 1 | First boot on the tractor, logger open. Debug `1` after the VT shows the screen, and after ten minutes | Banner with the 28B line; claim; MW18 uploads in full (this NAME is new to the JD): **note the seconds**; `CAN receive queue: dropped 0` at both dumps (the splash fix, #350) |
| J0 | ladder 1 | `:list` saved as `1324910_vanos_before.txt`; set "Ploegt naar" for a JD (#304) | AUTO corrects toward the line on both sides |
| J1 | ladder 1 | TC connect | No width popup; the JD shows the real width; dump `Working width (DDI 67)`, `reported` |
| J2 | ladder 1 | 3x BREDER, 3x SMALLER standing on the line | The XTE jump per press, in cm, or none |
| J3 | ladder 1 | The plough's ISOBUS plug out 20 s, back in | Back without a popup; XTE unchanged. **Count the `VT Server is NACK-ing our VT messages` lines** at the replug: there were 5 on the bench with a 512-frame queue |
| J4 | ladder 1 | Each turn position: soil side, LCD letter, VT picture. Photos | All three agree |
| J6 | ladder 1 | Debug `1` a few times during J1 to J3 | `PGN 129029 GNSS position:` count rising |
| S1 to S5 | spike347 | The one-section spike. **Width keys and hitch only while the task is running**; then back to ladder 1 | What the JD's TC asks, writes and shows. At van Mastwijk the presses fell in a stopped task and the hitch move never changed the work state |
| T1 | the 3.2's own | Only after the wiring is fixed and the board has received frames on the bench. Rig 4.1 off the ISOBUS, the 3.2 on the ISOBUS only, not on the valves | As V12: upload seconds, `tc=Y`, heap peak, stack never used (under 4 000 B = #341 doesn't merge), receive queue |
| A1 | spike327_auxn | Only if the log shows `Discovered auxiliary input device` on this JD | Buttons work after a 20 s replug without assigning again |
| J5 | ladder 1 | "Ploegt naar" back; `:list` | Equals J0's |

Leave the rig 4.1 on ladder 1 if everything on it passed, otherwise on the highest ladder step that passed. Note it on
the sheet.

A John Deere TC stores a DDOP under its label, so TC09 stays stored on that tractor after the spike.

## 6. At van Mastwijk (CNH VT + InCommand 1200)

The VT numbers there are **as the owner wants them: CNH = VT 1, 1200 = VT 2, preferred VT 1.** Nothing has to be put
back. The refusal check (old V5, second half) is dropped.

| Step | Hex | Do | What decides it |
|---|---|---|---|
| Q1V | ladder 1 | As Q1: first boot, two dumps | `dropped 0` (a160ce6 dropped 328 to 452 here); both VTs load MW18 from memory, no upload; no VT stall |
| V5B | spike327_auxn | Wait for `Discovered auxiliary input device` (the armrest did not announce itself on the afternoon of 2026-11-14; without it nothing is sent). Assign BREDER and SMALLER on the CNH, press each, unplug 20 s, replug; then power-cycle the plough | **Pass:** buttons work after both without assigning again, no `VT rejected the preferred assignment command`. Still rejected: time, log, CANedge; then it is the timing, not the padding |
| T1 | the 3.2's own | As above, if the wiring is fixed | As V12 |
| optional | spike347 tc09 | Five minutes: with the task running on the 1200, 2x BREDER and one hitch up/down, dump | Whether the 1200 reacts with a section present. On 2026-11-14 it accepted TC09 and asked for nothing |

Things this tractor taught on 2026-11-14, so they don't cost time again:

- The 1200 sends no XTE while its task is stopped.
- The 1200 keeps no DDOP: a full upload at every connect is normal there.
- The bus never carries PGN 129029; position and RTK come as PGN 65535 from 0x80.
- The autosteer engage flag is PGN 65462 from 0xF5, byte 6 bit 1 (`lb=..e..` on the 1 Hz line). Engage needs speed.
- The CANedge still holds the piece with the crossed Teensy 3.2 (from about 16:25 on 2026-11-14). It uploads at the
  next power-up, as session 130 piece 12 or session 131. Nothing to do; say when it has arrived.

## 7. What to bring back

- Serial logs as `Ploegbesturing Isobus/Documentation/logs/<date>_session28B_<place>_*.log`, a timeline
  `<date>_session28B_<place>_timeline.md`, photos, and `1324910_vanos_before.txt`.
- The sheet: every step ticked or skipped with a note; the record fields for part J filled.
- The numbers upstream is waiting for (AgIsoStack #887): the MW18 upload seconds on ladder 1, the receive queue high
  water and dropped, the count of NACK lines at the replug, and whether anything stalled.
- Push to `test/session28`, then tell `neptune-gps-8c` what was pushed and what is still open. The workstation checks
  the captures and posts the results; nothing goes upstream without the owner's go.

## 8. Not for this visit

- The 64 against 512 upload times on the 4.1 for upstream #887: a bench job (Saturn + PC VT), because it needs a forced
  second upload.
- #346 (choosing the TC by what it offers): not built.
- Merging: nothing in the stack merges before part J has run. #344 and #345 wait with it.
