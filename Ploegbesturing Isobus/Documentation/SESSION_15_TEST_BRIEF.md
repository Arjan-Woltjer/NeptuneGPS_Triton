# Session 15 test brief -- van Mastwijk: the lightbar channel, VT failover, and the IBBC capture

Written 2026-10-09 for the next van Mastwijk visit (CNH with the new harness, InCommand 1200,
SteerCommand Z2, L160 lightbar available). Supersedes the XTE-hunt part of
`SESSION_14_TEST_BRIEF.md`: the InCommand's XTE is found (PGN 65462, card log 42), and this
session tests the firmware that reads it. The failover part repeats the 2026-10-02 handover's
item 5 on this rig.

## 0. What we know, and what this session decides

- The InCommand broadcasts XTE as **PGN 65462 from its display CF 0xF5**, 5 Hz, global, but in
  every capture only **after an L160 lightbar identified itself** over ISO 15765-2. Decode: bytes
  0-1 cm, byte 2 engaged (2), byte 6 bit 7 side (set = right, from the operator's position at the
  start of the 10-08 drive), byte 7 line present (4). Write-up: Documentation
  `ISOBUS/research/agleader-incommand-65462-xte-2026-10-08.md`.
- The plough control now **presents as an L160** on a second control function (0xDC) and commits
  65462 as XTE (draft PR #207). Bench: both control functions claim, heartbeat and software ID go
  out. Never tested against a display.
- **The bus split seen on 10-08 was the Lemken technician's doing, not the harness** (owner,
  2026-10-09): he had disconnected the IBBC and the InCommand branch from the tractor bus for his
  demo. On a normal day everything is one bus, with the InCommand VT at 0x80 and the CNH VT at 0x26
  (log 31, 10-03). So the in-cab connector is a valid tap again, and VT failover can be tested the
  original way (section 5). The one-minute inventory check decides which it is on the day.

This session decides:

1. Does the InCommand identify **our** lightbar and start 65462, with no real L160 ever on the bus?
2. Is byte 6 bit 7 = right? (The one thing the 10-08 drive could not settle.)
3. Does VT failover (#188) recover on this rig when the bound VT disappears and returns?
4. Plus, free with the capture: #574 churn on the wire, #203 DM1, #205 speed (65256 raw vs the
   InCommand's km/h).

## 1. Kit and build

- Plough control 1324910 runs **`test/rig-lightbar-failover` @ `b73cea8`** = the 10-02 rig build
  (#190 AgIsoStack-plus-plus, #191 MW08 screen, #188 failover) + #207 lightbar channel + #205 speed
  fix + #192 FLASHMEM. Reflash only from that branch (`git fetch && git switch
  test/rig-lightbar-failover`, `pio run -e teensy41_isobus -t upload`, only the plough's Teensy on USB).
- CANedge AD4F266A: channel 1 on the in-cab connector as usual. **After one minute, inventory the
  card**: the InCommand's claims (manufacturer 97, VT 0x80, TC 0xF7, 0xF5) must be there. If they are
  not, somebody has split the bus again: move to the log-42 arrangement (channel 2 back-probed on the
  IBBC, CAN_H pin 8, CAN_L pin 9, ground pin 2, channel 1 stays in the cab for power).
- Field laptop with the serial logger (`C:\Users\arjan\field-tools\triton_logger.ps1`; it switches the
  1 Hz line on by itself).
- The real L160 lightbar, **kept unplugged until section 4 says otherwise**.
- Saturn in listen-only mode is optional; the CANedge is the record.

**Rule: our control and the real L160 are never on the bus at the same time.** Our lightbar control
function carries the L160's NAME, identity included. Two identical NAMEs on one bus is undefined
behaviour for every node, the InCommand included.

## 2. Setup (before the InCommand is switched on)

1. Tractor on, InCommand **off**, L160 **unplugged**. CANedge powered and logging (card in).
2. Plough control on the IBBC, powered, USB to the laptop, logger running. Boot print must show
   `Lightbar: presenting as an Ag Leader L160, claiming 0xDC` and within a second
   `Lightbar: address claimed: 0xDC`. If it shows `error-PASSIVE` with TX err 128 the control is
   alone: nothing else is on the segment yet, fine at this point.
3. **One-minute check**: after a minute of logging pull the card and look at the newest session's
   size, or run the inventory. A live IBBC segment with the InCommand on writes well over a megabyte a
   minute; channel 9 only means the tap is dead. Fix before anything else.

## 3. The identification test (InCommand on, L160 unplugged)

| Step | Do | Expect / read |
|---|---|---|
| I1 | InCommand on. Note the time. | Within ~60 s of its boot the dump (menu 1) shows `Display partner: 0xF5  identification requests=7 answered=7 unknown=0  hello=Y`, `Sent: ... failures=0`. (0xF5 was the display function's address on both 10-01 and 10-08; if it differs, the dump says which.) |
| I2 | Wait 10 s, dump again. | `PGN 65462 XTE: frames=` counting (5/s), `lb=` on the 1 Hz line. **This is question 1 answered yes.** |
| I3 | If I1 shows requests=0 after 2 min: InCommand off, 30 s, on again, repeat. | Still 0: the display does not look for a bar it has never seen, or needs something we do not send. Go to section 4. |
| I4 | If requests arrive but `unknown>0` or `answered<requests`: dump, note the `last DID`. | A DID the L160 was never asked on 10-08. The capture has the bytes; nothing to do at the rig. |
| I5 | If identification completes but 65462 never starts: wait for a guidance line (the display needs one; byte 7 goes 1 -> 4 with a line). Load the AB line, dump. | 65462 may only start with a line. |

## 4. Only if section 3 fails: the real L160

Never both at once. Sequence: unplug our control from the IBBC, plug the L160 in, InCommand on
(or already on), wait a minute so it identifies the bar (CANedge records it), **unplug the L160**,
plug our control back in, dump after 30 s. Three outcomes, all useful:

- 65462 already running and our dump counts it: the display remembers a bar; our identification
  may or may not be asked. Note `requests=`.
- The display re-identifies us and 65462 continues: fine for the day, and the cold-start question
  stays open for the next visit.
- Nothing: the display wants the exact bar. The capture of its exchange with the real L160 is then
  compared byte for byte with ours (card log 42 is the reference).

## 5. VT failover (#188) on this rig

On a normal day both VTs are on one bus: the InCommand's at 0x80, the CNH's at 0x26. The plough
binds to one of them at boot (whichever the stack picks first; the dump's `Partner: addr=` says).

| Step | Do | Expect (serial) |
|---|---|---|
| F1 | Plough up, MW08 on the bound VT. **Switch that VT off** (the CNH VT by its function key, or the InCommand by its power). | Within ~16 s of its status going silent: `VT: partner 0x.. silent -- requesting address claims to find another VT`, then `VT at 0x.. alive -- switching to it`; the pool appears on the other terminal; dump `VT failover: switches=1`. (The InCommand keeps sending VT status for ~90 s after shutdown; the switch comes after that silence.) |
| F2 | **Switch the first VT back on.** | The plough stays on the second terminal (no switch back); `switches=1` still. |
| F3 | Switch the second VT off. | Switches back to the first, `switches=2`. |

If the inventory check showed the bus split again, fall back to the 10-03 move: pull the ISOBUS plug
from one segment to the other without a power cycle and expect `switching to it`.

## 6. The drive (lightbar channel live, L160 unplugged)

Short field, AB line loaded, RTK fixed. Read the display's own XTE aloud at each step; the serial
log carries `lb=<cm><L|R><e|->:<raw>` once a second.

| Step | Do | Decides |
|---|---|---|
| D1 | Engage autosteer on the line, drive 50 m. | `lb=` magnitude follows the display; `xte=` on the 1 Hz line equals `lb` in metres with the sign. |
| D2 | **Stop with the tractor about 30 cm RIGHT of the line, autosteer still engaged** (nudge, or disengage, offset, re-engage). Hold 10 s. | `lb=30R` or thereabouts. If it reads `L`: the side bit is inverted, note it, carry on. |
| D3 | Same, 30 cm LEFT. Hold 10 s. | `lb=30L`. **Question 2 answered.** |
| D4 | Disengage, drive by hand 0.5 m off the line. | Does 65462 keep coming with `byte 2 = 1`? (`lb=...-`) The decoder does not commit it; the raw bytes show whether the display still sends a value. |
| D5 | Headland turn, engage on the adjacent pass. | Pass change: `lb=` jumps once, then settles. |

With a plough on: D1-D3 are also the first live check of the plough steering on this XTE. Keep
AUTO off until `lb=` has been seen to follow the display in D1.

## 7. What to read live (menu 1 dump)

- `--- Lightbar emulation (Ag Leader L160, PGN 65462) ---`: claim, partner, requests/answered/
  unknown, hello, sent/failures/heartbeats/softwareId, 65462 frames/committed, last raw payload
  with its decode. **`failures>0` means the stack refused a send**: note the number and when.
- The 1 Hz line: `lb=` (above), `xte=`, `spd=` (now km/h-correct, #205: compare with the
  InCommand's speed at two speeds), `vt=`, `tc=`.
- `VT failover: switches= claim requests=`.
- `PGN 65256 Speed legacy: raw=0x...` against the InCommand's km/h: raw/256 must equal it.

## 8. Checklist

```
SETUP [ ] tractor on, InCommand OFF, L160 UNPLUGGED   [ ] CANedge ch2 on IBBC pins 8/9/2, ch1 in cab, logging
      [ ] plough on IBBC, boot print: lightbar claimed 0xDC __:__:__   [ ] one-minute card check: ____ kB
IDENT [ ] I1 InCommand on __:__:__   requests=__ answered=__ unknown=__ hello=_   failures=__
      [ ] I2 65462 frames counting __:__:__   lb= on the 1 Hz line
      [ ] I3/I4/I5 notes: ________________________________
L160  [ ] (only if IDENT failed) L160 in __:__:__  out __:__:__  ours back __:__:__  result: ______
FAIL  [ ] F1 InCommand off __:__:__  "partner silent" at __:__:__
      [ ] F2 InCommand on __:__:__  VT reconnected __:__:__  TC __:__:__  switches=__
      [ ] F3 second VT off __:__:__ switches=__
DRIVE [ ] D1 engaged __:__:__ display XTE ____ lb=____   [ ] D2 30 cm RIGHT __:__:__ lb=____
      [ ] D3 30 cm LEFT __:__:__ lb=____   [ ] D4 by hand off line __:__:__ lb=____   [ ] D5 adjacent __:__:__
END   [ ] final dump   [ ] CANedge stopped __:__:__   [ ] card session numbers: ____
```

## 9. Afterwards

- Archive: MF4 to `canlogs/` with a README entry (say which connector each channel was on), serial log + timeline to
  `logs/`, as on 10-02 and 10-08.
- Results per PR: #207 (questions 1 and 2, the dump block), #188 (F1-F3), #205 (speed), #190
  (#574 on the wire), #191 (screen on the InCommand), #192 (nothing to report unless it misbehaves).
  Then delete `test/rig-lightbar-failover`.
- If the side bit is inverted: one line in `DecodeXteAgLeaderLightbar`, plus the three decoder
  tests, on #207.
- If the display needs a real bar first (section 4): the next change is to compare the two
  identification exchanges; the answer values live in `AgLeaderLightbarEmulation::Identity`.
