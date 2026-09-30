# Session 12 test brief — John Deere (van Os), plough attached

Session 11 ran without a plough. Session 12 has one, so this is the **first time
the ISOBUS build drives a real plough.** It also carries two pieces of desk work
that can only be finished on a tractor: TC06 (#162) and max correction per
share (#164). This brief covers what to flash, what to do before the tractor,
the tests in priority order, and what each result means.

---

## 0. What changed since session 11 — read this first

**One test branch carries both drafts.** `test/session12` is `main` plus #162
and #164, merged as they stand. It exists only for this session. After the
session each PR merges on its own, and the branch is deleted.

**Every terminal will see a new implement.** TC06 gives each board its own ISOBUS
NAME: its identity number comes from the Teensy's serial number instead of the
fixed 1 every Triton used before. The John Deere keys stored pools and settings
by NAME. Expect a **full VT pool and DDOP upload on the first connect**, and
possibly implement settings to enter again. That is expected once, not a fault.

**The working width now comes from the plough.** The DDOP declares DDIs 67–70:
the actual width (the plough's `offset`) plus default, minimum and maximum. In
session 11 the JD replaced the operator's 2.25 m with its own 3 m on every
connect, which moved the guidance lines. Now it should take ours. **So set the
plough's width correctly before connecting** (section 2).

**Max correction is now per share.** It is stored as mm per share, 25–100,
default 50, and the limit is per share × shares: 20 cm on 4 shares, 25 cm on 5.
It used to reset to 50 cm at every boot whatever was set (#163). The calibration
menu now asks for it per share.

**The AgIsoStack fork moved one commit.** `lib_deps` now pins fork commit
`9aa491e` instead of the `0.1.5-neptune1` tag. It adds patch #6, which removes
dead roll-call code, raised on upstream PR #718. It should change nothing you
can see. It gets a passive check in 3.6: no extra actions, just the serial log.

**Settled since session 11 — don't re-test:**

- the 12:25 VT drop (#149) was the bus cable being pulled, not a fault;
- the XTE sign flips in reverse, but the plough is always lifted then (#151).

---

## 1. What to flash

```
git fetch origin
git checkout test/session12
git pull
pio run -e teensy41_isobus -t upload
```

Expect **290/290** native tests, and a clean build of `teensy41_isobus` and
`teensy41_serial`.

**The boot log must show all three of these lines:**

```
Maximum correction per share (mm)
50                                    <- or the value set in section 2
IsobusGuidanceChannel: identity <N> (serial <N>), claiming... address claimed: 0x81
CAN controller after the claim: error-active, TX err 0, RX err 0
```

- `identity` is the board's serial number, modulo 2,097,152. **It must not be 1.**
- **On the tractor bus, the CAN controller must say `error-active`.** On the
  bench, alone on its bus, it says `error-PASSIVE, TX err 128`. That is normal
  there, because nothing acknowledges our frames. If it says passive on the
  tractor, the board is not really on the bus: check the connector and the
  test lead before anything else.

**Labels:**

- DDOP structure label: **TC06**.
- VT object pool: **MW04**, unchanged.

---

## 2. Before the tractor: at the plough and the control box

### 2.1 Calibrate, using the physical buttons (this is also #164's check)

**Hold both buttons on the control box to enter the calibration wizard.**
**Do not use the VT's Calibrate soft key** (#31). The wizard blocks the main loop,
the CAN stack stalls behind it, and the terminal drops us. Do this before the
board is on the tractor bus, or accept that the VT will reconnect afterwards.

The wizard goes through its steps in order. Each step asks "accept or decline"
on the LCD. Decline anything that shouldn't change.

1. **Position.** Look at the boot print first. If it shows the factory curve
   (`Offset calibration data` 600/461/308), this board has never been calibrated
   on this plough, and the actual-position reading is meaningless until you do
   this step. If it has been calibrated on this plough, decline.
2. **Number of shares:** 4 or 5, as actually mounted (the 4+1).
3. **Max correction per share, the #164 check.** Accept, then:
   - the value should step by **5** per press (`Corr./share: 050 mm`);
   - it should stop at **025** and at **100**;
   - the text should say per share.
   Set it to what you'd normally use: **050** reproduces 20 cm on 4 shares.
4. **Ploughside:** as usual.
5. **Accept the last "store" screen**, or nothing is saved.

**Then check the boot print:**

- `Maximum correction per share (mm)` shows what you set;
- `Maximum correction (mm)` equals that times the share count.

**Read the total, not the per-share line, if a board carried an old setting.**
Under the old format, byte 60 held the total in cm. An old value from 25 to 100
now reads as mm *per share*.

### 2.2 Set the working width before connecting

In AUTO and HOLD, Wider/Narrower change the stored working width (`offset`, LCD
row 0, in cm) by 1 per step. A held physical button repeats; a VT key is one
step per press. **Set it to the plough's real width before the first TC
connect**, because TC06 hands this value to the John Deere.

**First, settle which way it goes — TC06 rests on it.** By the code, the Wider
key (VT, or the left button) **lowers** `offset` by 1 and Narrower raises it. In
AUTO, a setpoint above the actual position drives the `Narrower()` output. So in
the code's own naming, a *larger* offset means a *narrower* plough. That
contradicts "offset is the working width", which TC06 reports as DDI 67 without
inverting it. One of the two is wrong, and only the plough can say which:

1. Press **Wider** a few times, in HOLD (hitch down, stationary). Note whether
   LCD row 0 goes **up or down**.
2. In AUTO, watch which way the plough **physically** moves when row 0 changes.
3. After the TC connect, check whether the JD's implement width moves **the same
   way** as the plough.

If Wider lowers row 0 while the plough gets wider, then `offset` is not the width
in TC06's direction, and #162 must not merge as it is. **Record it either way.**

Also check:

- **The width stops at its limits.** On 4 shares it stops at 80 and 240, on 5 at
  100 and 300 (shares × 20 to shares × 60). Before #152 it jumped back to the
  middle.
- **The dump reads it back.** Menu 1: `Working width (DDI 67): <cm> cm, changes N, reported 0`
  (`reported` stays 0 until a TC is connected).

---

## 3. The tests, in priority order

### 3.1 First connect under the new NAME and TC06 — #162 items 1–3

Connect, and let the pools upload. Watch for three things:

| Check | Pass |
|---|---|
| The terminal accepts both pools | VT screen appears, TC shows **TC06**, no error popup |
| **No "working width updated" popup** | session 11 got one on every connect |
| The terminal's implement width | equals LCD row 0 (in m on the JD) |

**If the JD asks for implement settings** (it sees a new NAME), write down what
it asked. That is part of the result.

**Capture side (item 1):** the CANedge will hold our address claim. The NAME's
low 21 bits must equal the `identity` in the boot log. That can be read at the
workstation, so nothing to do at the rig beyond capturing.

### 3.2 First AUTO with the plough in the ground

This is the first time the loop from XTE to plough runs for real. **Be ready to
switch to MANUAL** with the mode switch.

**What the LCD shows (row 3):**

- mode: **A** auto, **H** hold, **M** manual;
- the reason for a hold: **S!** speed, **G!** guidance;
- plough side: **L** or **R**.

**What the firmware does in AUTO:** it moves the plough towards a setpoint of

```
setpoint = width  ±  XTE      (+ if side is L, − if side is R)
```

with the correction clipped to **max correction** (20 cm at 50 mm × 4 shares).
The plough ignores errors inside the error margin (2 cm by default). The
setpoint itself is not on the LCD: work it out from rows 0 and 2.

Record by hand, with times:

1. **Hitch up → M.** #151 was closed on the premise that the plough is always
   in MANUAL when lifted, so a reverse never reaches the actuator. **Raise the
   hitch and confirm the LCD goes to M.** If it doesn't, the hitch input isn't
   wired or is inverted on this rig. That puts #151 back on the table: stop, and
   write it down.
2. **Hitch down, above 1.8 km/h, RTK → A.** If it says H, the reason is on the
   LCD (S! or G!).
3. **Direction.** When the tractor is off the line, note:
   - the XTE sign on LCD row 2;
   - the plough side (L/R);
   - which way the plough moved: wider or narrower.

   **You judge whether that is the right way** for the furrow. If it is
   consistently backwards, that is what the "ploughside" setting is for. Record
   it either way. It is the first real evidence of the sign chain from the JD's
   bus to our actuator.
4. **The clip.** With |XTE| beyond max correction, actual position (row 1)
   should stop at width ± max correction and go no further.

### 3.3 Wider/Narrower while ploughing — #162 items 5 and 6

In AUTO, on a line with autosteer engaged:

- **Does the JD subscribe to DDI 67?** After a few presses, check menu 1:
  `Working width (DDI 67): ... changes N, reported M`. **`reported` above 0**
  means we sent the change to a connected TC. Also look at
  `[on bus] Measurement commands` for a DDI 67 entry.
- **The open risk: does the JD re-space its lines when the width changes?**
  Stationary on a line, guidance on, press Wider a few times and watch the XTE on
  LCD row 2. **If the XTE jumps with each press**, the JD is re-spacing its lines
  on every width report. Stop pressing and record the jump size. Live reporting
  would then need damping, for example reporting only after the presses stop.
  Nothing moves the plough while you're stationary (below 1.8 km/h it holds).

### 3.4 Unplug for ~15 s — #162 item 4 and #159's recovery half

With everything live, **pull the ISOBUS connector for ~15 s, keeping the power
on**, then plug it back in. Session 11 did the same test (#18); this time it also
checks two new things:

- **The width survives a TC reconnect (#162 item 4).** After the reconnect the
  JD's width is still ours, with no popup, and the XTE is unchanged. In session
  11 the XTE moved by a whole track width.
- **The CAN error readout recovers (#159).** On the bench only half of it could
  be proven, because there was nothing to acknowledge our frames. Menu 1 after
  replugging:

  ```
  CAN controller: error-active  TX err <small> (peak 128) ...
    error-passive entries: 1  bus-off entries: 0  longest episode: ~15000 ms
  ```

  Expected: one more error-passive entry, a longest episode about the length of
  the unplug, **back to error-active**, and **no bus-off**. An unacknowledged
  controller stops counting at the passive threshold, so it doesn't go bus-off.
  **Write the unplug and replug times down.** Session 11's unlogged cable pull
  cost an investigation.

### 3.5 Ask the operator — the #21 lead

In session 11 the TC sent DDI 506 at every connect. In session 9, on the same
rig, it didn't, and our side of the handshake was byte-identical both times. So
something set up on the terminal made the difference. **Ask what was set up on
the display in session 11:** a job, a guidance line, an implement profile,
tramline settings? Also note today's setup, and whether `Tramline setpoint
(DDI 506)` shows up in the dump.

---

### 3.6 Address-claim churn — AgIsoStack patches #5 and #6 (passive)

Nothing to do at the rig. This is read from the serial log afterwards.

This is the first field run of patch #5 (a CF that comes back after a
roll-call is credited for it) and of patch #6 (the dead code next to it
removed). Patch #4 already keeps our VT/TC partner from being pruned, so the
check is on the **other** CFs on the bus.

- **Pass:** a CF that logs `is now offline` comes back with `has claimed
  address` and **stays**. There is no repeating `offline` → `claimed` pair for
  the same address every 1–2 s. Session 6 showed address 205 doing that for
  800 s.
- **Pass:** VT and TC stay up as in every session since patch #4.
- **Fail:** any address cycling offline/online at the roll-call rate. Keep the
  log; it goes on upstream #718.

Count `is now offline` lines per address over the whole run. A handful spread
across the session is normal: ECUs do go quiet. A steady beat on one address is
the failure.

Expect little here: session 11's logs from this John Deere have no `[NM]` lines
at all, so this bus may never roll-call. The real test of #5 and #6 is session
13 on the New Holland + Ag Leader rig (`SESSION_13_TEST_BRIEF.md`).

---

## 4. New in the debug dump (menu 1)

As captured on the bench (alone on its bus, so passive and nothing received):

```
Address claim: CLAIMED  address=0x81  identity=1324910
CAN controller: error-PASSIVE  TX err 128 (peak 128)  RX err 0 (peak 0)
  error-passive entries: 1  bus-off entries: 0  longest episode: 0 ms  (current: 21408 ms)
  PGN 65535  all senders:       0   last SA=0xFF
  PGN 65535  XTE carrier 0x2A:  0   (none decoded)
             raw: nothing from 0x2A or 0x80 yet
  Working width (DDI 67): 160 cm, changes 0, reported 0
```

- **CAN controller.** `(current: N ms)` appears while an episode is in progress.
  "longest episode" counts finished episodes only. On the tractor, expect
  error-active with small numbers.
- **65535 lines (#153).** "all senders" counts every manufacturer's proprietary
  traffic. Only the **carrier** line is the XTE feed, and "raw" shows its last
  payload.
- **Guidance re-requests (#153 decision, merged).** Triton re-requests the
  position, speed and XTE PGNs whenever one of them hasn't arrived for 10 s,
  including after a receiver reboots. There is nothing to read for this; it runs
  on its own.

---

## 5. Traps

- **The VT Calibrate soft key stalls the CAN stack** (#31). Use the physical
  buttons.
- **A new NAME means a new implement to the JD.** Settings made against the old
  one may not carry over. Re-entering them is expected, not a regression.
- **The setpoint isn't displayed.** Work it out from width and XTE (3.2) before
  concluding the plough went the wrong way.
- **The XTE is the distance to the nearest track, and wraps at ±width/2.** Near
  a line that doesn't matter. Driving across lines, it jumps by a full track
  width.
- **Distances called out at the rig are not measurements.** Session 10's "15 m"
  was 37 m. Take them from the log.
- **Log physical actions with times:** hitch up/down, autosteer on/off, button
  presses, connector pulls. The capture shows none of them.

---

## 6. What to capture

- **CANedge on the ISOBUS segment**, logging *before* the board powers on:
  passive tap, and the logger's termination **off**.
  **Check the SD card is in the CANedge before starting it.** The first
  session 12 run (2026-09-30) was done without one, and nothing was captured.
- **Triton on the bus at the same time.**
- **Optional: Saturn as a second, passive logger** (NeptuneGPS Saturn,
  `docs/field-capture.md`). Only the `teensy41_jupiter_receive_only` build,
  bench-tested the night before (`py tools/gsusb_capture.py bench`). **Never the
  VT app on this bus.** Termination jumper out, the Jupiter on its own supply,
  wired from the CANedge's tap. Plug its USB in after Triton is flashed. Start
  `py tools/gsusb_capture.py capture --label session12-jd-vanos` before the
  tractor and Triton, and stop it after the CANedge. If it costs attention the
  brief needs, leave it out: the CANedge is the capture that counts.
- **Serial log** with the 1 Hz summary line (menu 2), plus a full dump (menu 1)
  after each test in section 3.
- **A handwritten timeline:** date, rig, owner, plough (shares, 4 or 4+1, side),
  and the actions above.
- **Afterwards:** the CANedge clock is wrong. The StarFire's PGN 126992 carries
  real UTC (skip its first frame). Name files as
  `<date>_session12_<rig>-<owner>_log<card session>_<what>`, copy them to
  `Documentation/canlogs/`, and add a row to its `README.md`.

---

## 7. What this session can close

| Issue / PR | Question | Test | Then |
|---|---|---|---|
| #162 (#150, #45) | Does the width run the right way (2.2)? Does TC06 fix the overwrite, and does each board get its own NAME? | 2.2, 3.1, 3.3, 3.4 | merge #162 (fix the direction first if 2.2 says so); close #150 and #45 |
| #164 (#163) | Does the per-share menu step, stop and save? | 2.1 | merge #164; close #163 |
| — | Does the plough correct the right way, and stop at max correction? | 3.2 | first closed-loop result; record it |
| #151 premise | Does hitch-up really force MANUAL on this rig? | 3.2 step 1 | if not, reopen #151 |
| #159 | Does the CAN readout show recovery on a live bus? | 3.4 | note in the hardware test notes |
| #21 | What on the terminal unlocks DDI 506? | 3.5 | progress comment on #21 |
| upstream #718 | Does a restored CF stay restored (patches #5 + #6)? | 3.6 | post the result on #718; tag `0.1.5-neptune2` on the fork |

If the Ag Leader / CNH rig comes up instead, the appendix of
`SESSION_11_TEST_BRIEF.md` still applies: read the 44032 lockout line first, and
remember that rig is two vendors.
