# Session 26 test brief -- AgIsoStack on plain upstream (02f3fe9), MW18, the stack measured

Written 2026-11-08. Session 26 tests **one change of library** and **how much stack it leaves**. It comes after
session 25: `test/session26` is `test/session25` (be1a1b0's content: #162, the #316 spike, the settings console #326)
plus two PRs and the v0.26 bump. Run session 25's own steps on be1a1b0, not on this build, so a session 25 finding
can't be blamed on the library.

| Part | Where | Decides |
|---|---|---|
| **S** | The bench: rig 4.1 + Saturn + the PC VT | The stack numbers and the CrashReport line exist; MW18 uploads; no fault at boot |
| **P** | van Mastwijk (CNH VT + InCommand 1200, both on the bus), at least an hour | #834 instead of our partner patch: the VT and TC partners stay connected; no receive stalls; the stack's peak after a real day |
| **X** | van Mastwijk with the VT numbers swapped (CNH = VT 1), or J.A. Bos (Fendt = VT 1) | AUX-N through upstream's code (#655): assign, press, unassign, the Preferred Assignment after a reconnect |

This session's notes go on **`test/session26`**, see section 7.

## 1. What changed on the board since session 25

| Change | PR | What to see |
|---|---|---|
| **AgIsoStack-plus-plus upstream main 02f3fe9**, through the fork's `neptune-next`: no fork patches left, all upstream | #327 | Nothing on its own. The partner exemption (our 978f5f5) is gone: upstream's #834 takes a partner offline only after **two** missed requests for address claim in a row |
| **AUX-N enabled in the VT client** (upstream #655 made it opt-in). Upstream takes assignments only from **VT 1** (function instance 0) | #327 | Part X. On the CNH as VT 2, assigning is refused, as the terminals themselves did in sessions 22 and 23 |
| **Pool label MW17 -> MW18**: upstream's soft key and font scaling differ from the fork's | #327 | Every VT uploads MW18 fresh, once |
| **Stack measured**: painted at boot; `Stack: peak N of M bytes since boot, F never used` in the dump; ` stk=N` at the end of the 1 Hz line | #328 | Part S |
| **CrashReport** printed at boot when the board rebooted from a fault (a stack overflow into the core's MPU guard among them) | #328 | Nothing at a normal boot. A block starting `CrashReport:` in a boot print is a fault: keep the whole block |
| **AgIsoStack's boot, upload and connect code runs from flash** (a linker script, no library patch): the 4.1 build is back to 9 ITCM blocks | #328 | On the 4.1, `free for local variables` is **71.5 KB** (session 25: 74.6 KB; the library move alone would leave 42 KB), 16 KB under the next block. The pool upload and the DDOP may take a little longer; nothing else should differ |
| Version | | Splash `Triton PB-ISO  v0.26`, banner `Triton PB-ISO version 0.26` |

## 2. Kit and builds

OneDrive `MeijWorks Projects/Firmware builds/session26/`:

| Hex | For | Contains |
|---|---|---|
| `teensy41_isobus_f434459.hex` | the rig Teensy 4.1 (Loader 13249100) | `test/session26`, v0.26 |
| `teensy40_isobus_f434459.hex` | a standard Teensy 4.0 plough board | the same |

**No customer board gets this build.** The FET client stays on what session 25 left there.

**Fallback:** `Firmware builds/session25/teensy41_isobus_be1a1b0.hex` (v0.25, MW17, the fork's d4ad930). Going back
re-uploads MW17 on every VT, once.

**Flashing:** `tools/flash_teensy.ps1 -Serial <Loader serial> -Hex <hex>`, with exactly one PJRC board on USB (unplug
the Saturn) and the serial logger stopped. Settings are kept: the EEPROM layout is session 25's.

**Every part:** serial logger open **before** the board boots, the 1 Hz line on (debug `2`), CANedge on the ISOBUS.

## 3. Part S -- the bench

| Step | Do | Expect |
|---|---|---|
| S1 | Flash, logger open, boot. | Banner `version 0.26`, the SPIKE line, `identity N (board serial M)`. **No `CrashReport:` block.** Settings as on be1a1b0. |
| S2 | Debug `1` at once. | A `Stack:` line under `Loop pass:`. Peak a few KB; size 71 456 bytes on the 4.1 (`free for local variables` minus the 32-byte guard). Note both. |
| S3 | PC VT on the Saturn. | **MW18 uploads** (note the time from the first request to the main mask). Soft keys and the main screen look as with MW17. |
| S4 | The 1 Hz line for a minute. | ` stk=N` at the end of each line; N never goes down. |
| S5 | Unplug the Saturn for 20 s, plug it back (a VT reconnect, a new VT client). Dump `1`. | The plough reconnects as before. Note the stack peak after it. |

## 4. Part P -- van Mastwijk, both VTs, at least an hour

The rig 4.1 on the session 26 hex, preferred VT 2 as left by session 25 (the 1200 = VT 1, the CNH = VT 2).

| Step | Do | Expect |
|---|---|---|
| P1 | Boot with the tractor. | MW18 uploads **once on each VT**; the 1200's TC takes the DDOP (TC08, unchanged). No errors. Note the times. |
| P2 | Work or stand for **at least an hour** with both VTs on the bus. | The plough stays on its VT and TC throughout. **Afterwards, from the serial log and the CANedge:** the number of `is now offline` lines for the VT 0x26, the 1200 0x80 and the TC. Our patch made this 0; upstream's #834 should keep it at 0 unless a partner really misses two requests for address claim. |
| P3 | Through P2: VT command timeouts (#248) and stalls. | As on be1a1b0: no VT stalls, `CAN receive queue: dropped 0`. |
| P4 | Failover: the 1200 off and on again, as session 23's R7. | Failover to the CNH VT and back as before. **Dump after each switch: the stack peak.** A switch builds a new VT client, which the library now constructs from flash. |
| P5 | End of the day: debug `1`. | **The stack line: peak and never used.** This is the number session 26 is for. Note any `CrashReport:` at any boot of the day. |

## 5. Part X -- AUX-N on VT 1

AUX-N needs the VT that is **VT 1**: upstream's client takes assignments only from function instance 0. At van
Mastwijk the 1200 never enables AUX-N (session 23), so swap the VT numbers (the CNH = VT 1) and restart the tractor, as
session 23's A-steps did. At J.A. Bos the Fendt is VT 1 already.

| Step | Do | Expect |
|---|---|---|
| X1 | On VT 1's AUX screen: assign BREDER and SMALLER to the inputs that worked before (CNH armrest 28100/28101; Fendt joystick 32064/32065). | Both assign. The serial log shows the assignment. |
| X2 | Press each, held and released. | The width changes as with the cab buttons (BREDER wider, SMALLER narrower). |
| X3 | Unplug the plough's ISOBUS for 20 s and replug (the same VT client reconnects). | Does the VT show the assignment again? Note what it shows. Upstream keeps the assignments in RAM for as long as the same VT client lives, and offers them as the Preferred Assignment when it reconnects; a failover builds a new client, which starts empty. |
| X4 | Power-cycle the plough. | The assignments are gone from the plough's side (RAM only); note what the VT does. |
| X5 | Unassign both. | Both unassigned, presses do nothing. |
| X6 | (van Mastwijk) Put the VT numbers back (the 1200 = VT 1, the CNH = VT 2). Try to assign on the CNH. | **Refused.** That's upstream's rule and matches sessions 22 and 23 (the VT 2 terminals refused themselves). |

## 6. Checklist (copy into the timeline, tick live)

PART S -- BENCH
- [ ] S1 banner 0.26, SPIKE Y/N, CrashReport none Y/N
- [ ] S2 stack peak ______ of ______
- [ ] S3 MW18 upload __ s; screen as MW17 Y/N
- [ ] S4 stk= on the 1 Hz line Y/N
- [ ] S5 reconnect OK Y/N; peak after ______

PART P -- VAN MASTWIJK
- [ ] P1 MW18 on 1200 __ s, CNH __ s; TC08 Y/N
- [ ] P2 from __:__ to __:__; `is now offline` VT 0x26 __ 1200 0x80 __ TC __
- [ ] P3 stalls __; dropped __
- [ ] P4 failover to CNH Y/N, back Y/N; peak after ______
- [ ] P5 end of day: peak ______ of ______, never used ______; CrashReport at any boot Y/N

PART X -- AUX-N ON VT 1 (where: ______)
- [ ] X1 assigned Y/N   - [ ] X2 presses Y/N
- [ ] X3 after a reconnect the VT shows ______   - [ ] X4 after a power cycle ______
- [ ] X5 unassigned Y/N   - [ ] X6 CNH as VT 2 refuses Y/N

## 6b. If something differs

- **A `CrashReport:` block in any boot print:** keep the whole block and the minutes of log before it, and flash the
  fallback (be1a1b0). It holds the address that faulted.
- **A partner going offline (P2), a VT stall (P3), or the screen differing from MW17:** note the time, keep the log and
  the CANedge, flash the fallback, and run the rest on it.
- **Not a difference:** the first MW18 upload per VT; AUX-N refused by a VT 2.

## 7. Afterwards

- **Notes:** serial log and timeline go on **`test/session26`** under `Ploegbesturing Isobus/Documentation/logs/`,
  named `<date>_session26_<place>_*`.
- **Where results go:** S1-S3, P1-P4 and X1-X6 to PR #327; S2, S5, P4 and P5 (the stack numbers) to PR #328.
- **When P and X pass:** move the fork's `neptune-main` to 02f3fe9, merge #327 and #328, then the Documentation PR
  #54. Proteus and Kilverbak's VT stack (#299-#307) follow with their own pins.
