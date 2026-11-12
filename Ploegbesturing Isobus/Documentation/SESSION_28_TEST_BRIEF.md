# Session 28 test brief -- full ISOBUS on an original Teensy 3.2 board

Written 2026-11-12. Session 28 runs the plough with the **VT, the TC and diagnostics on a Teensy 3.2**, on an
**unmodified original board**: the Teensy 3.2 PCB as ploegbesturing v1.20-v1.31 used it, CAN on the 3.2's only
controller CAN0, **pins 3 TX / 4 RX, no bodge wire**. It is env `teensy32_isobus`, PR #341.

`test/session28` is `test/session27` (41b8ca1) plus #341, #342 and the v0.28 bump. The Teensy 4 builds differ from session
27 only by the new `Heap:` line in the dump and ` hp=` on the 1 Hz line, so **session 26 and 27 keep their order** on
the rig 4.1; this session needs only a 3.2 board and the bench.

| Part | Where | Decides |
|---|---|---|
| **B** | The bench: an original 3.2 board + Saturn + the PC VT | Boots, claims, keeps the old calibration, uploads MW18; **the heap and stack numbers after the upload** |
| **L** | The bench, the same board | An hour on the VT: the heap does not creep, the stack stays clear of it |
| **T** | Optional, only where a TC is on the bus (the 1200 at van Mastwijk), owner's call | The DDOP uploads and the TC connects |

This session's notes go on **`test/session28`**, see section 7.

## 0. Before the session: questions for the owner

**Q1 -- the send queue (PR #341).** `FLEXCAN_T4_PLUGIN_TX_QUEUE_SIZE=64`: FlexCAN_T4 queues 64 frames for sending
instead of 512, 4 960 B of RAM instead of 17 056 B. AgIsoStack keeps its own queue of 40 frames, which it only hands
to the driver in `update()`, once per loop pass; the driver's queue only has to keep the bus busy until the next pass.
At 250 kbit/s a frame takes about 0.55 ms. The plugin sends in order through one transmit mailbox, which the queue
refills from the interrupt, so 64 frames is about 35 ms of bus time. One `update()` hands over at most AgIsoStack's 40
frames, so a queue above that adds nothing but delay (proposed upstream as the default for every Teensy, on #887). On the 4.1 the longest pass is about 3 ms, apart from one 120 ms stall in the USB serial right after a flash. A
full queue loses nothing: the frame waits in AgIsoStack's queue, and only the pool upload slows down. B5 reads the
3.2's longest loop pass; the queue can then be set to what that pass needs.

**Q2 -- the NAME identity on a 3.2: DECIDED, fix in (PR #342).** `BoardSerialNumber()` returned 1 on a Teensy 3.x,
so every 3.2 claimed identity 1. It now reads the 3.2's own serial from the flash, as the core does for USB.

## 1. What is new on the board

| Change | PR | What to see |
|---|---|---|
| Full ISOBUS (VT MW18, TC, diagnostics, failover, lightbar) on the Teensy 3.2 | #341 | The same dump and VT screens as the 4.1 |
| ISOBUS on CAN0, pins 3 TX / 4 RX (it was FLEXCAN3, which a 3.2 does not have) | #341 | Address claim on the bus |
| The NAME's identity is the 3.2's own serial (it was 1 on every Teensy 3) | #342 | Dump: `identity N` = the board's serial |
| AgIsoStack: upstream main 9220c1f (#879 #882 #884, no C++ streams) + #886 + #887 (FlexCANT4Plugin on the Teensy 3, one controller), our fork's `neptune-teensy3-9220c1f` branch, **this env only** | #341 | Nothing: the other envs keep 5ff90cb |
| On a 3.2 the heap and the stack share one region. The dump's `Stack:` size is the room between the heap's top and the top of RAM; "never used" is what is left between the two. The core's `malloc()` fails 4 KB before they meet | #341 | `Stack:` line |
| New dump line `Heap: peak N of M bytes, shared with the stack` (on a 4.1 without ", shared ..."); ` hp=N` after `stk=` on the 1 Hz line. The heap's top never comes down, so the peak is the most it ever held | #341 | `Heap:` line, `hp=` |
| Version | | Splash `Triton PB-ISO  v0.28`, banner `Triton PB-ISO version 0.28` |

Build: `teensy32_isobus` **246 468 of 262 144 B flash (94.0 %)**, **22 644 of 65 536 B static RAM**, so the heap and
stack share 42.9 KB. (#341 alone, on main, is 241 164 B; the session branch adds #241's console, #162's identity
line and #316's spike line.) The Teensy 4.1 build keeps session 27's **71 488 B free for local variables**. Expected heap peak after the upload: about 23 KB (the CAN controller 5 KB, AgIsoStack's tables,
the scaled copy of the pool during the upload). **That leaves the stack about 16 KB; under 4 KB "never used" would mean
`malloc()` is close to failing.**

**Not on a 3.2:** the CAN error-state line in the dump (#149) reads the Teensy 4's FlexCAN registers and stays empty.
A bench without a partner that ACKs shows nothing there; look at the claim and the VT instead.

## 2. Kit and builds

OneDrive `MeijWorks Projects/Firmware builds/session28/`:

| Hex | For | Contains |
|---|---|---|
| `teensy32_isobus_174c267.hex` | an original Teensy 3.2 board (CAN on 3/4) | `test/session28`, v0.28, with #342 |
| `teensy41_isobus_bdda49f.hex` | the rig 4.1 (Loader 13249100), **only after session 27** | session 27 + the `Heap:` line |
| `teensy40_isobus_bdda49f.hex` | a standard Teensy 4.0 plough board | the same |
| `teensy32_can_bdda49f.hex` | the 3.2 board, **fallback** | plain CAN (ACAN, no VT/TC), v0.28 |

**No customer board gets this build.** The 3.2 board is a spare from the shelf.

**Before flashing the 3.2:** open its serial port and **save the old firmware's boot print** (version and calibration)
to the logs. Legacy firmware prints it once, late after boot, and Windows buffers it until the port opens, so save the
first read to a file. There is no hex of the old firmware in `Firmware builds/`; that boot print is the only record.

**Flashing the 3.2:** `tools/flash_teensy.ps1 -Serial <USB serial> -Hex <hex> -Mcu TEENSY31` (teensy_loader_cli
calls the 3.2 `TEENSY31`), with exactly one PJRC board on USB and the Saturn unplugged. The script was written for the
Teensy 4: if it aborts on the 3.2 (old firmware not a USB serial device, or a HalfKay serial it does not expect), use
the Teensy Loader GUI with the board's button, **still with the Saturn and every other Teensy unplugged**.

**Every part:** serial logger open **before** the board boots, the 1 Hz line on (debug `2`), CANedge on the bench bus
if it is free.

**Read-only:** nothing on the bench drives a valve from the laptop. The console's `:set` and the wizard store values;
the valves are not connected on the bench.

## 3. Part B -- the bench

| Step | Do | Expect |
|---|---|---|
| B1 | Old boot print saved, flash, logger open, boot. | Banner `version 0.28`, splash `v0.28` on the LCD. No hang. |
| B2 | Debug `1` at once, before the VT. | `Stack:` and `Heap:` lines; note both. **`identity` = the board's serial**: the USB serial Windows shows, divided by 10 if it is under 100 000 000 and ends in 0 (#342). Not 1. |
| B3 | `:list` on the console. | **The calibration the old firmware printed**: points, shares, side, bypass, deadband. If they differ, note both and recalibrate with the wizard; that is a finding for the EEPROM layout, not for #341. |
| B4 | Saturn + PC VT on the bus. | Address claim; **a full MW18 upload** (identity 1 has never stored MW18, so no Load Version). Note the upload time (Q1). The main screen comes up; the plough picture matches the side switch. |
| B5 | **Debug `1` straight after the upload.** | `Heap: peak N of M`: about 23 KB. `Stack:` "never used": **well above 4 KB**. These are the numbers that decide. Also note `Loop pass: longest ... us` (Q1). |
| B6 | Soft keys and cab buttons as session 24 (width +/-, CALIBR, BREEDTE page), valves not connected. | As on the 4.1: the screens follow, values stored. |
| B7 | Unplug the Saturn 20 s, plug it back. | Reconnect by Load Version now (it stored MW18 in B4). Debug `1`: heap peak unchanged or a little higher, never used still well above 4 KB. |
| B8 | Power-cycle the board. | Boots again, Load Version on the PC VT, calibration kept. |

## 4. Part L -- an hour on the bench

| Step | Do | Expect |
|---|---|---|
| L1 | Leave it on the PC VT for **at least an hour**, the 1 Hz line on. | `hp=` flat after the upload; `stk=` flat. No reboot, no banner in the middle of the log. |
| L2 | Debug `1` at the end. | Heap peak and stack never used against B5. `CAN receive queue: dropped 0`. |

## 5. Part T -- the TC (optional, owner's call)

Only where a TC is on the bus: today that is the InCommand 1200 at van Mastwijk. A spare 3.2 on that bus is a second
plough NAME next to the rig unit, so only with the rig unit unplugged, and only if the owner wants it.

| Step | Do | Expect |
|---|---|---|
| T1 | Boot on that bus. | DDOP upload, the TC connects (TC08), the plough on its VT. |
| T2 | Debug `1` after both uploads. | Heap peak: the DDOP adds to B5's. Stack never used still well above 4 KB. |

## 6. Checklist (copy into the timeline, tick live)

BEFORE
- [ ] Q1 send queue: 64 for now, set from B5's loop pass ______
- [ ] Old firmware boot print saved Y/N; version ______

PART B -- BENCH
- [ ] B1 banner 0.28 Y/N; splash 0.28 Y/N
- [ ] B2 stack ______ of ______; heap ______ of ______; identity ______ (USB serial ______)
- [ ] B3 `:list` == old boot print Y/N (differences ______)
- [ ] B4 claim Y/N; MW18 upload __ s; main screen Y/N; picture side right Y/N
- [ ] B5 heap peak ______ of ______; stack peak ______, never used ______; longest loop pass ______ us
- [ ] B6 soft keys + cab buttons as 4.1 Y/N
- [ ] B7 reconnect by Load Version Y/N; heap ______; never used ______
- [ ] B8 reboot, Load Version Y/N, calibration kept Y/N

PART L -- AN HOUR
- [ ] L1 from __:__ to __:__; hp= flat Y/N; stk= flat Y/N; reboots __
- [ ] L2 heap ______; never used ______; dropped __

PART T -- TC (optional)
- [ ] T1 DDOP Y/N; TC08 Y/N
- [ ] T2 heap ______; never used ______

## 7. If something differs

- **No banner, or the board resets in a loop:** keep the log and note it on #341. The old firmware cannot go back (no
  hex), so flash the fallback `teensy32_can_bdda49f.hex`: plain CAN through ACAN, as the board ran before ISOBUS.
- **`Not enough memory for the CAN controller`** in the log: `malloc()` failed at boot. Note the `Heap:` line; that is
  #341's finding, and Q1 (a smaller queue) or a smaller AgIsoStack table is the next step.
- **The upload stops, or the VT shows an error after it:** debug `1` at once (heap, stack), keep the CANedge; a heap
  that ran into the stack fails silently.
- **"Never used" under 4 KB** anywhere: not a failure yet, but #341 is not ready to merge on that number.
- **The calibration lost in B3:** recalibrate, note the old and new values; a separate issue.

## 8. Afterwards

- **Notes:** serial log, old boot print and timeline go on **`test/session28`** under
  `Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session28_<place>_*`.
- **Where results go:** B, L and T to PR #341, with Q1's answer; B2's identity to PR #342.
- **When B and L pass:** #341 is ready once #886 and #887 are merged upstream and the env is pinned to upstream main.
