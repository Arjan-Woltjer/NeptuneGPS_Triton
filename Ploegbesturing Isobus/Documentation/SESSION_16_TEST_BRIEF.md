# Session 16 test brief -- van Mastwijk: does the InCommand accept the fixed L160 emulation, and does failover now see a dead InCommand

Written 2026-10-10 after the session 15 capture (card log 43). Same rig as sessions 13-15: CNH with
the InCommand 1200, SteerCommand Z2, one ISOBUS (both VTs, TC, display function 0xF5). No real L160
exists at the farm any more, so everything rides on the emulation.

## 0. What session 15 found, and what this session decides

- The display identified our bar three times with answers **byte-identical** to the real L160's,
  and never started PGN 65462. On the wire the differences were around the answers: it asked us
  for our software identification (PGN 65242), ECU identification (64965) and PGN 64653, and the
  stack **NACKed** all three; the real bar broadcasts 65242 unasked right after claiming and ignores
  the other two silently. It then polled our address claim every 2.5 s for the rest of the session,
  which the real bar never got. We also answered within 1-4 ms where the bar waits 80 ms, and sent
  the hello before the software ID where the bar does the opposite.
- **Fixed in PR #207 (c2fe415):** no NACKs, 65242 answered with the software ID BAM, software ID
  before the hello on first contact, every frame 80 ms apart. Which of the four was the trigger the
  capture cannot say; all four now match the bar.
- **VT failover (#188)** switched three times correctly. The one failure, InCommand switched off,
  was ours: the InCommand went silent 16 s after the switch, but AgIsoStack's VT client stamps its
  status timer on *any* VT's status, so the CNH VT's 1 Hz kept it "connected" to a dead 0x80.
  **Fixed (85714f7):** the partner's own silence counts, connected or not.
- The side bit (right = byte 6 bit 7) is still unconfirmed, because 65462 never came.

This session decides:

1. Does the display send its Proprietary A frame to us and start 65461/65462 now?
2. If yes: is byte 6 bit 7 = right of the line? (D2/D3 below.)
3. Does the plough move to the CNH VT when the InCommand is switched off (F1)?
4. Regression: #213 (the phantom 129283 counter) and the 2.5 s address-claim polling, both visible
   in the dump without any driving.

## 1. Kit and build

- Plough control 1324910 must run **`test/rig-lightbar-failover` @ `2a25331`** (= session 15's
  b73cea8 + #207 c2fe415 + #188 85714f7). **Not yet flashed on 2026-10-10 evening: the board was
  not on USB.** Flash before leaving: `git fetch && git switch test/rig-lightbar-failover &&
  git pull`, `pio run -e teensy41_isobus -t upload`, only the plough's Teensy on USB. The boot
  print says `Lightbar: presenting as an Ag Leader L160, claiming 0xDC`; the dump's lightbar block
  has a `Requests: softwareId=.. (sent ..x) ignored=..` line that b73cea8 does not have. **If that
  line is missing, the old build is on the board.**
- CANedge AD4F266A, channel 1 on the in-cab connector, as in session 15. After one minute,
  inventory the card: the InCommand's claims (manufacturer 97) must be there.
- Field laptop, serial logger (1 Hz line on by itself). Take a dump (menu 1) at every step below.

## 2. Setup

Tractor on, InCommand **off**, plough control on the bus, logger on, CANedge logging. Dump: both
control functions claimed (0x81, 0xDC), `Requests: softwareId=0 (sent 1x) ignored=0`, heartbeats
counting, failures=0. One-minute card check.

## 3. The identification (InCommand on)

The order of the two boots matters for what the display asks. Session 15 had our bar on the bus
first; log 42 had the real bar power up after the display. Do both.

| Step | Do | Expect, and what to read |
|---|---|---|
| I1 | **InCommand on** with our bar already on the bus. Dump at +60 s. | Lightbar block: `Display partner: 0xF5 identification requests=7 answered=7 unknown=0 hello=Y`; **`Requests: softwareId=1 (sent 2x) ignored=2`** (65242 answered, 64965 + 64653 swallowed); `proprietary A from display=1` is the new thing to look for. Then `PGN 65462 XTE: frames=` counting and `lb=` on the 1 Hz line. |
| I2 | Dump again at +3 min. | `ignored=` must **stop growing** (session 15: the display polled our claim every 2.5 s; with the claim request handled, `ignored` counts those polls, so a number that keeps rising by ~24/min means the display is still in its retry loop). `65462 frames` rising by ~300/min. |
| I3 | **Our bar after the display**: unplug the plough control's ISOBUS plug, wait 20 s, plug it back (InCommand stays on). Dump at +60 s. | Same block; this time the display should not need to ask 65242 (`softwareId=0`, the BAM went out unasked after the claim). Prop A from display and 65462 again. |
| I4 | **Display cold boot with our bar present**: InCommand off, 30 s, on. Dump at +60 s. | As I1. |

If after I1-I4 the display still sends no Proprietary A and no 65462, section 7.

## 4. The drive (only if 65462 is coming)

Short field, AB line, RTK fixed. Read the display's XTE aloud at each step; the serial log has
`lb=<cm><L|R><e|->:<raw>` once a second.

| Step | Do | Decides |
|---|---|---|
| D1 | Engage autosteer on the line, 50 m. | `lb=` follows the display's XTE in magnitude; `xte=` on the 1 Hz line equals it in metres with the sign; the plough's XTE source is now the InCommand. |
| D2 | Stop ~30 cm **right** of the line, engaged (nudge or re-engage offset). Hold 10 s. | `lb=..R` expected. `L` means the side bit is inverted: note it, carry on. |
| D3 | Same, 30 cm **left**. | `lb=..L`. **Question 2 answered.** |
| D4 | Disengage, drive by hand 0.5 m off the line. | Whether 65462 keeps coming with byte 2 = 1 (`lb=..-`); the decoder does not commit it, the raw shows what the display sends. |
| D5 | Headland turn, engage on the adjacent pass. | `lb=` jumps once, settles. |

With the plough on, D1-D3 are the first live check of the plough steering on this XTE. AUTO stays
off until D1 has shown `lb=` following the display.

## 5. VT failover (#188)

| Step | Do | Expect (serial) |
|---|---|---|
| F1 | Plough bound to the InCommand VT (0x80; if it is on the CNH VT, pull/replug the plug once with the CNH VT's function off, or just do F3 first). **InCommand off.** | The InCommand goes silent within ~20 s on the wire. **Within ~35 s of the switch**: `VT: partner 0x80 silent -- requesting address claims`, `VT at 0x26 alive -- switching to it`, MW08 on the CNH VT, dump `VT failover: switches=1`. Session 15 never got this far. |
| F2 | **InCommand on again.** | Plough stays on the CNH VT; TC reconnects to 0xF7; the lightbar identification runs again (I4 pattern), 65462 resumes. |
| F3 | **CNH VT off by its key.** | Switch to 0x80 within ~16 s, `switches=2`. |
| F4 | CNH VT on again. | Stays on 0x80. |

## 6. What to read live, and what the capture must contain

- Dump, lightbar block: the `Requests:` line (new), `proprietary A from display=`, `65462 frames=`,
  `failures=`. The 1 Hz line: `lb=`, `xte=`, `vt=`, `tc=`.
- #213: `PGN 129283 XTE NMEA2000:` in the dump. Session 15 counted 194 with none on the bus. Note
  the count at the start and end; the capture settles what it is.
- The capture: the whole identification exchange (0xF5 <-> 0xDC on PGN 0xDA00, our 65242 BAM, the
  Prop A frames both ways), 65461/65462 from 0xF5, and the InCommand-off window for F1.

## 7. If the display still does not start 65462

Then the remaining differences between us and the real bar, in the order to try at the rig if
there is a laptop with the toolchain, otherwise at the workstation:

1. **The NAME's arbitrary-address-capable bit.** Ours is set (NAME `A0 00 83 00 0C 21 88 7E`), the
   L160's is not (`20 00 83 00 ...`). One line in `IsobusLightbarChannel::Begin()`.
2. **Answer 64965 and 64653 instead of ignoring them** (the display may accept either; the bar's
   silence is what we copied).
3. **The display's Proprietary A is a message to the bar**, not only a signal: `00 00 00 FF ...`.
   If it arrives but 65462 still does not, the bar may have to answer it. Nothing in log 42 says so.
4. Plan B from the session 14 brief: the plough computes its own XTE from the InCommand's positions
   (65267 at 10 Hz) and an AB line set on our VT screen. No display cooperation needed.

## 8. Checklist

```
BUILD [ ] dump has the "Requests: softwareId=" line (= 2a25331)   [ ] boot: 0x81 and 0xDC claimed
SETUP [ ] InCommand OFF, plough on bus __:__:__   [ ] CANedge ch1 in cab, one-minute check ____ kB
IDENT [ ] I1 InCommand on __:__:__  requests=__/__ unknown=__ softwareId=__ (sent __x) ignored=__ propA=__ 65462=__
      [ ] I2 +3 min: ignored=__ (growing? Y/N)  65462=__   lb= on the 1 Hz line Y/N
      [ ] I3 replug __:__:__  softwareId=__ propA=__ 65462=__
      [ ] I4 display cold boot __:__:__  propA=__ 65462=__
DRIVE [ ] D1 engaged __:__:__ display ____ lb=____   [ ] D2 30 cm RIGHT lb=____   [ ] D3 30 cm LEFT lb=____
      [ ] D4 by hand lb=____   [ ] D5 adjacent __:__:__
FAIL  [ ] F1 InCommand off __:__:__  switched to 0x26 at __:__:__ (__ s)  switches=__
      [ ] F2 on again __:__:__ stays on 0x26 Y/N  TC __:__:__  65462 resumes Y/N
      [ ] F3 CNH VT off __:__:__ -> 0x80 at __:__:__   [ ] F4 CNH VT on, stays Y/N
#213  [ ] 129283 count start ____ end ____
END   [ ] final dump   [ ] CANedge stopped __:__:__   [ ] card session ____
```

## 9. Afterwards

MF4 to `canlogs/` with a README entry, serial log + timeline to `logs/`, on `test/session12`.
Results to #207 (questions 1 and 2), #188 (F1-F4), #213 (the count), #42. Then delete
`test/rig-lightbar-failover`.
