# Session 17 test brief -- van Mastwijk: the rebased AgIsoStack, the 0xF5 partner, and the InCommand's "GPS lost"

Written 2026-10-13 after session 16 (capture log 56). Same rig as sessions 13-16: CNH with the
InCommand 1200, SteerCommand Z2, one ISOBUS (both VTs, the TC 0xF7, the display function 0xF5).

## 0. What changed since session 16, and what this session decides

Session 16 settled the lightbar emulation (Prop A and 65462 in every scenario), the side bit
(byte 6 bit 7 = right), and VT failover F1-F4. Three changes are on the board for the first time:

| Change | Commit | Why |
|---|---|---|
| XTE decoder commits on line state 2 **and 3**, side from byte 6 bits 7/6, engaged from bit 1 | `bad487d` (#207 `b07aa94`) | session 16: `xte=` froze at -1.30 m for the whole first pass because the display sent state 3 |
| AgIsoStack-plus-plus rebased onto upstream of 2026-10-13 | pin `4625be4` (#190 `8f34870`) | 21 upstream commits. **#774**: the VT client now ignores VT Status from any VT but its partner (the library side of the session 15 failover failure). **#719**: waiting for the first VT Status gives up and retries after 5 s. Load/End-of-Pool waits on a VT v3+ end after three "not parsing" status messages instead of a fixed 3 s. |
| The display 0xF5 is the lightbar channel's partnered control function | `2745ff4` (#207 `ae4bf5a`) | session 16 20:51:08: the InCommand skipped one global request for address claim (from 0xCD, mid CNH restart) and the stack took 0xF5 out of its table for 6 s while it kept transmitting. Partners are exempt from that prune. |

This session decides:

1. Does the rebased library still do everything session 16 did: VT pool from NVM on **both** VTs, TC, identification, 65462? (Regression, section 3.)
2. Is 0xF5 bound as the partner, and does it survive a CNH restart? (Section 3 and F4.)
3. With the fixed decoder, does `xte=` follow `lb=` through an engaged drive? (Section 4, D1.)
4. **The InCommand's "GPS lost" during a CNH VT reboot** (the owner can trigger it): what the InCommand puts on the bus while it says so, and whether the plough holds. (Section 5, the new test.)
5. Failover F1-F4 on the new library. (Section 6.)

## 1. Kit and build

- Plough control 1324910 must run **`test/rig-lightbar-failover` @ `2745ff4`**. Flash at the
  workstation or with the field laptop: `git fetch && git switch test/rig-lightbar-failover && git pull`,
  `pio run -e teensy41_isobus -t upload`, **only the plough's Teensy on USB**.
- **Build check:** the dump's lightbar block line `Display partner: 0x.. identification ...` now ends
  with **`stack partner=`** (`0xF5` once the display has claimed, `N` before). Session 16's build has no
  such field. If it is missing, the old build is on the board.
- CANedge AD4F266A, channel 1 on the in-cab connector; uploads by itself over LTE. **Power it up early**:
  the ninth piece of session 16 (log 56, 21:00-21:03:51: pass 2, the final dump, the `0B` frame, the
  speed question) only uploads at the logger's next power-up. Check from the field laptop with
  `canedge_bucket_pull.py --list` that session 56 gets a ninth file and a new session starts.
- Field laptop, serial logger (1 Hz line on by itself). Dump (menu 1) at every step below.

## 2. Setup

Tractor on, both VTs on, plough control on the bus, logger on, CANedge logging. Dump at +60 s: 0x81
and 0xDC claimed, VT bound, TC 0xF7 connected, `failures=0`.

## 3. Regression on the new library (standing still)

| Step | Do | Expect |
|---|---|---|
| R1 | Dump at +60 s after the plough joins. | VT bound; **`MW08` loaded from NVM** (serial: label matched, no full upload). TC connected. Lightbar: requests=answered, unknown=0, propA>=1, `PGN 65462 XTE: frames=` counting, **`stack partner=0xF5`**. |
| R2 | Pull the plough's ISOBUS plug, 20 s, plug back. Dump at +60 s. | Same as R1, `stack partner=0xF5` again. A full pool upload instead of NVM means the label matching changed: note it, carry on. |

## 4. Drive with the fixed decoder (only after R1)

AB line, RTK fixed. AUTO stays off until D1 shows `xte=` following `lb=`.

| Step | Do | Expect |
|---|---|---|
| D1 | Engage autosteer on the line, 50 m, then disengage and drive by hand 0.5 m off. | `xte=` follows `lb=` in metres with the sign **throughout** (session 16: frozen at -1.30). The raw byte 2 shows 2 or 3; both commit now. |
| D2 | Headland turn onto the next pass. | During the turn byte 2 = 1 for a few seconds: `xte=` holds, then follows again. |
| D3 | (free) speed: `spd=` vs the InCommand's speed, moving and standing still. | Same within 0.1 km/h. Session 16 left a standing-still `spd=0.00` vs raw 2.09 km/h open. |

## 5. NEW: the InCommand's "GPS lost" during a CNH VT reboot

The owner has seen the InCommand report lost GPS when the CNH VT reboots, and can trigger it. In
session 16 the CNH VT was switched off and on by its key (F3/F4) and the capture shows **no** gap: the
InCommand's 65267/65256/65097 kept coming at 10 Hz from 0x80 with live position values, and 65462 at
5 Hz with line state 2 throughout. So the trigger is a different kind of reboot, or the InCommand
shows the message without stopping its broadcasts. This test finds out which.

The plough's side of it: `GpsReadyToSteer()` (`PloughGates.hpp`) holds when the XTE, position or
speed is older than 2 s (`kGuidanceFixMaxAgeMs`). The decoder does not commit line state 1, so a lost
solution should turn into HOLD within ~2 s; a VT GPS indicator that goes red at the same moment
confirms it.

| Step | Do | Read, with times to the second |
|---|---|---|
| G1 | On the line, autosteer engaged (or standing on it), plough in AUTO only if D1 passed, else HOLD/manual. Dump. | Baseline: `xte=` = `lb=`, `xteAge` < 1 s, `spd=`. |
| G2 | **Trigger the CNH VT reboot the way that makes the InCommand say "GPS lost".** Write down **how** (key, menu, power, ignition?), the time you trigger it, the time the message appears, and the time GPS returns. Dump while the message shows. | 1 Hz line: does `lb=` keep changing, and what is the raw byte 2 (1 = no solution, 2/3 valid)? Does `xteAge` grow? Does the plough go **HOLD** and the VT's GPS indicator go red within ~2 s of `lb=` freezing or going to state 1? Does `spd=` keep updating? |
| G3 | Dump ~30 s after GPS is back. | `xte=` follows `lb=` again, `xteAge` < 1 s, plough out of HOLD (if it held). `stack partner=0xF5` still; `VT failover: switches=` unchanged unless the VT itself went away. |
| G4 | If time allows, repeat G2 once. | Same answer = reproducible. |

What the capture answers afterwards: whether 65267 from 0x80 **stops**, **freezes** (same payload) or
**keeps moving** during the message; what 65462's byte 2 and byte 7 do; whether the InCommand skips
global requests for address claim again (the 0xF5 prune pattern); and whether anything else on the
bus (0xCD, 0xAC, 0xF0, 0x26) disappears.

## 6. VT failover on the new library (#188 + #774)

As session 16. Plough bound to the InCommand 0x80 first.

| Step | Do | Expect (session 16 times for comparison) |
|---|---|---|
| F1 | InCommand off. | Switch to the CNH VT 0x26, MW08 from NVM, `switches=1`. Session 16: ~20-25 s. With #774 the library itself also drops on the partner's silence; equal or faster is fine, slower is a finding. |
| F2 | InCommand on. | Stays on 0x26; TC back; identification again, 65462 resumes, `stack partner=0xF5`. |
| F3 | CNH VT off by its key. | Switch to 0x80, `switches=2`. Session 16: ~15 s. |
| F4 | CNH VT on. | Stays on 0x80. Session 16 saw 0xE9/0xF5 "offline" for 6 s here; **now 0xF5 must not go offline** (serial: no `now offline` for 245/0xF5; 0xE9 may still). |

## 7. Checklist

```
BUILD [ ] dump lightbar block has "stack partner=" (= 2745ff4)   [ ] CANedge on early: log 56 piece 9 uploaded Y/N, new session ____
R     [ ] R1 MW08 from NVM Y/N  TC Y/N  propA=__  65462=__  stack partner=____   [ ] R2 replug: same Y/N
D     [ ] D1 xte= follows lb= Y/N (state 3 seen Y/N)   [ ] D2 state 1 in the turn, held Y/N   [ ] D3 spd ____ vs InCommand ____
G     [ ] G1 baseline __:__:__
      [ ] G2 HOW: ______________  trigger __:__:__  "GPS lost" shown __:__:__  GPS back __:__:__
            lb= frozen/state 1 at __:__:__   HOLD at __:__:__   VT GPS dot red Y/N
      [ ] G3 +30 s: xte follows Y/N  partner 0xF5 Y/N  switches=__   [ ] G4 repeat same Y/N
F     [ ] F1 InCommand off __:__:__ -> 0x26 at __:__:__ (__ s)   [ ] F2 stays Y/N
      [ ] F3 CNH VT off __:__:__ -> 0x80 at __:__:__ (__ s)       [ ] F4 stays Y/N  0xF5 offline Y/N
END   [ ] final dump   [ ] CANedge stopped __:__:__ (wait for the current piece to upload)   [ ] session ____
```

## 8. Afterwards

MF4s arrive in OneDrive `CANedge logs/bucket-AD4F266A/session-<n>/` by themselves; serial log +
timeline with this checklist filled in, on `test/session12`. Results to #207 (D1-D3, G), #188 (F1-F4),
#190 (R1-R2: the new library on hardware, needed before #190 merges), and a short note on
AgIsoStack-plus-plus #717 if F4 or G2 shows the prune again.
