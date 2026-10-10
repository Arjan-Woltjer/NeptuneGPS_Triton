# Session 16 (van Mastwijk, fixed L160 emulation + failover): handover to the field laptop

Written at the workstation 2026-10-10 evening. Follow `../SESSION_16_TEST_BRIEF.md` at the rig.

## Board

Plough control 1324910 runs **`test/rig-lightbar-failover` @ `2a25331`**, flashed from a clean build
and bench-checked 2026-10-10 20:30: 0x81 and 0xDC claim, heartbeat, software ID sent, 0 failures,
and the dump's lightbar block has the new `Requests: softwareId=.. (sent ..x) ignored=.. last
ignored PGN=..` line (that line is how you tell this build from session 15's b73cea8).

It is session 15's build plus two fixes from the capture:

| Fix | PR | What changed |
|---|---|---|
| Lightbar identification | #207 c2fe415 | the display's requests for 65242/64965/64653 and for our address claim are no longer NACKed; 65242 is answered with the software ID; software ID goes before the hello on first contact; every frame 80 ms apart |
| VT failover with the InCommand off | #188 85714f7 | the partner's own silence counts even while AgIsoStack says connected (the client counts any VT's status) |

Reflash only from that branch: `git fetch && git switch test/rig-lightbar-failover && git pull`,
`pio run -e teensy41_isobus -t upload`, only the plough's Teensy on USB.

## What session 15's capture settled (so you do not chase it again)

- Our seven identification answers were byte-identical to the real L160's. The display declined
  because of what happened around them (NACKs, pacing, order). Section 0 of the brief.
- The InCommand does **not** keep its VT server up after power-off: it went silent 16 s after the
  switch. The 1 Hz VT status the serial log saw for five minutes was the CNH VT's. Expect F1 in the
  brief to switch within ~35 s now.
- No PGN 129283 was on the bus; the 194 the dump counted are phantom (#213). Note the count at
  start and end again.

## At the rig

- CANedge channel 1 on the in-cab connector; one-minute inventory check (manufacturer-97 claims).
- InCommand **off** while the plough control comes up, then the four identification scenarios I1-I4,
  a dump after each. The first new thing to watch for is `proprietary A from display=1`, then
  `PGN 65462 XTE: frames=` counting and `lb=` on the 1 Hz line.
- `ignored=` rising by ~24 per minute means the display is still polling our claim every 2.5 s,
  i.e. still not satisfied. Note the number at +1 and +3 minutes.
  Caveat: while **no VT is bound**, our own failover sends a global claim request every 10 s and
  the lightbar counts that too (bench: `ignored=6` after a minute alone, `last ignored PGN=60928`).
  Judge the rate only while `vt=Y`: 6/min is us, 24/min is the display.
- No real L160 at the farm: the bar is never on the bus; nothing to unplug.

## What to bring back

MF4 (card session numbers), serial log + timeline with the brief's checklist filled in, on
`test/session12`. Per question: (1) Prop A from display / 65462 yes-no per scenario I1-I4 with the
counters; (2) `lb=` at 30 cm right and left; (3) F1-F4 times and `switches=`; (4) #213 counts; (5)
any `failures>0` with time. If 65462 never comes, the capture of the identification exchange is the
deliverable, and section 7 of the brief lists what to change next.
