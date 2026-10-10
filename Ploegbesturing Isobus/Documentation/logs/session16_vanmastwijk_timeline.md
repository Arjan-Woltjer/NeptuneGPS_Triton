# Session 16 -- 2026-10-__ -- van Mastwijk (CNH, InCommand 1200, SteerCommand Z2) -- timeline

Brief: `../SESSION_16_TEST_BRIEF.md`. Handover: `session16_vanmastwijk_handover.md`.
Board 1324910 must be on `test/rig-lightbar-failover` @ `2a25331` (flashed at the workstation 2026-10-10 20:30,
per the handover note; the brief's "not yet flashed" line is older). Proof at the rig: the dump's lightbar block has
the `Requests: softwareId=.. (sent ..x) ignored=..` line.
Serial log: `2026-10-__\_session16_vanmastwijk_serial.log` (laptop wall clock, CEST). Rename both files with the date on the day.
No real L160 at the farm: nothing to unplug.

## Checklist (ticked live)

SETUP (tractor on, InCommand OFF)
- [ ] S1 tractor on, CNH VT running, InCommand OFF
- [ ] S2 CANedge: card IN, logging LED on, ch1 on the in-cab connector, started __:__:__
- [ ] S3 serial logger running before the board powers up
- [ ] S4 plough control on the in-cab connector, powered; boot print `claiming 0xDC` __:__:__ and `address claimed: 0xDC` __:__:__
- [ ] S5 BUILD CHECK, dump: `Requests: softwareId=0 (sent 1x) ignored=..` line present (= 2a25331; missing = old build, stop and reflash). heartbeats counting, failures=0. #213: `PGN 129283` count at start = ____
- [ ] S6 one-minute card check (needs a reader; else skip and note)

IDENT (section 3; dump after each step)
- [ ] I1 InCommand on __:__:__ (our bar already on the bus). Dump at +60 s: requests=__/__ unknown=__ hello=_; `Requests: softwareId=__ (sent __x) ignored=__`; **`proprietary A from display=__`**; `65462 frames=__`; `lb=` on the 1 Hz line Y/N
- [ ] I2 dump at +3 min: ignored=__ (growing ~24/min while vt=Y = display still polling; ~6/min while vt=N = our own claim request); 65462 frames=__ (expect ~+300/min); lb= Y/N
- [ ] I3 our bar after the display: ISOBUS plug out __:__:__, 20 s, in __:__:__ (InCommand stays on). Dump at +60 s: softwareId=__ (expect 0, BAM went out unasked) propA=__ 65462=__
- [ ] I4 display cold boot with our bar present: InCommand off __:__:__, 30 s, on __:__:__. Dump at +60 s: propA=__ 65462=__
- [ ] If no Prop A and no 65462 after I1-I4: section 7 (NAME arbitrary-address bit; answer 64965/64653; answer the display's Prop A; plan B own XTE). Nothing more to do at the rig unless a toolchain laptop is there.

DRIVE (section 4; only if 65462 is coming; AB line, RTK fixed; AUTO off until D1 passes)
- [ ] D1 autosteer engaged __:__:__, 50 m: display XTE ____ vs `lb=____`; `xte=` = lb in m with sign
- [ ] D2 stop ~30 cm RIGHT of the line, engaged, 10 s __:__:__: `lb=____` (expect R; L = side bit inverted, note and carry on)
- [ ] D3 stop ~30 cm LEFT, 10 s __:__:__: `lb=____`  -> Q2 answered
- [ ] D4 disengaged, by hand 0.5 m off __:__:__: `lb=____` (byte 2 = 1? still coming?)
- [ ] D5 headland turn, adjacent pass __:__:__: `lb=` jumps once then settles
- [ ] D6 (free) speed check once more: `spd=` vs InCommand ____/____

FAILOVER (section 5; plough must be bound to 0x80 first: if on 0x26, do F3 first or pull/replug once)
- [ ] F1 InCommand off __:__:__ -> `partner 0x80 silent -- requesting address claims` __:__:__, `VT at 0x26 alive -- switching` __:__:__ (expect within ~35 s), MW08 on the CNH VT, `switches=__`
- [ ] F2 InCommand on __:__:__ -> stays on the CNH VT Y/N; TC reconnected __:__:__; identification runs again (I4 pattern) propA=__; 65462 resumes Y/N
- [ ] F3 CNH VT off by its key __:__:__ -> switch to 0x80 __:__:__ (expect ~16 s), `switches=__`
- [ ] F4 CNH VT on __:__:__ -> stays on 0x80 Y/N

END
- [ ] E1 final dump; #213: `PGN 129283` count at end = ____ (start: see S5); failures=__
- [ ] E2 CANedge stopped __:__:__; card session numbers ____
- [ ] E3 serial log + timeline (renamed with the date) committed and pushed on `test/session12`
- [ ] E4 handover sent to the workstation session (ListAgents; last time "ISO-Bus cabling diagrams")

## Timeline

| Time (CEST) | Event |
|---|---|

## Results (for the handover)

| # | Question | Answer |
|---|---|---|
| 1 | Prop A from display / 65462 per scenario I1-I4, with the counters (requests, softwareId, ignored) | |
| 2 | Side bit: `lb=` at 30 cm right / left | |
| 3 | F1-F4 times and `switches=` | |
| 4 | #213: 129283 count start / end | |
| 5 | `failures>0` in the lightbar block, with time | |
| + | #190 churn, 44032 with autosteer, anything else | |
