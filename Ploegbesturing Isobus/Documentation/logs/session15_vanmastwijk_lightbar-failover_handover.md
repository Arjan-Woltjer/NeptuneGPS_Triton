# Session 15 (van Mastwijk, lightbar channel + VT failover): handover to the field laptop

Written at the workstation 2026-10-09. Follow `../SESSION_15_TEST_BRIEF.md` at the rig; this file is
what the field laptop needs to know before and after.

## What is on the plough control

Board 1324910 runs **`test/rig-lightbar-failover` @ `b73cea8`**, flashed and bench-checked
2026-10-09 (both control functions claim: 0x81 guidance, 0xDC lightbar; heartbeat every 1.7 s;
software ID sent; 0 send failures). It is a test-only merge, not for main:

| Part | PR | What to look for |
|---|---|---|
| AgIsoStack-plus-plus build | #190 | no 205/172 `is now offline` churn; CANedge shows no 0x81 claims answering requests aimed at other CFs |
| MW08 screen | #191 | already seen on the InCommand on 10-03; nothing new |
| VT failover | #188 | brief section 5 |
| **Lightbar channel** | **#207** | brief sections 3, 4, 6, 7: the identification, then PGN 65462 and `lb=` |
| Legacy speed = km/h | #205 | `spd=` on the 1 Hz line now matches the InCommand's km/h; before it was 1.852x too high |
| FLASHMEM | #192 | nothing visible; it only restores stack headroom (78.7 KB) |

Reflash, if needed: `git fetch && git switch test/rig-lightbar-failover`, then
`pio run -e teensy41_isobus -t upload` **with only the plough's Teensy on USB**. Do not flash
`test/session12` (John Deere) or `test/rig-2026-10-02` (no lightbar channel).

## Two things that are different from every earlier visit

1. **The tap goes on the IBBC, not the in-cab connector.** On this harness the in-cab connector
   (both pairs, they are bridged) carries only the tractor's own ISOBUS: TECU 0xF0 and CNH VT 0x26.
   The InCommand's control functions and the implement are on the IBBC segment. Logs 34-41 were
   lost or useless for that reason. CANedge channel 2 back-probed on the IBBC (CAN_H 8, CAN_L 9,
   ground 2), channel 1 in the cab for power. After one minute, check the card: a live segment
   writes >1 MB/min; a file under a few hundred kB is channel 9 only.
2. **The real L160 stays unplugged** while our control is on the bus. Our lightbar control function
   uses the L160's NAME. Section 4 of the brief says when and how to use the real bar.

## The serial log

- The logger (`C:\Users\arjan\field-tools\triton_logger.ps1`) switches the 1 Hz line on by itself.
  The line now carries `lb=<cm><L|R><e|->:<16 hex>`: the InCommand's XTE in cm, the side bit as
  read (R = byte 6 bit 7 set), e = engaged, and the raw 65462 payload.
- Menu 1 has a new block, `--- Lightbar emulation (Ag Leader L160, PGN 65462) ---`. Take a dump at
  every step in the checklist; the identification counters there are the first thing to report.
- Stamps during a USB stall are arrival times (#198, seen on the Raven rig with this stack); pair
  readings via the MF4 if the log goes bursty.

## What to bring back

- The MF4(s): card session numbers in the timeline; archive goes to `canlogs/` with the README
  entry naming channel 2 = IBBC.
- Serial log + timeline in `logs/` (`2026-10-xx_session15_vanmastwijk_*`), on `test/session12` as
  before; the field laptop's Claude memory does not sync, this file and the timeline are the handover.
- The filled checklist from the brief (section 8), in the timeline.
- Answers, in the timeline's results table: (1) identification with no real L160 ever seen: yes/no,
  requests/answered/unknown; (2) side bit at 30 cm right / left; (3) F1-F3 outcomes; (4) `spd=` vs
  the InCommand at two speeds; (5) any `failures>0` in the lightbar block, with time.

## If something goes wrong

- Lightbar block says `NOT CLAIMED` for more than 10 s on a live bus: dump the CAN controller state;
  an address conflict on 0xDC would mean a real L160 is on the bus.
- `failures` climbing: the stack cannot address the display; note the partner address shown. It
  means the InCommand's 0xF5 is not in the stack's table; a CANedge capture of the InCommand's address
  claims settles it.
- Serial dead after the VT connects: #198, a USB replug restarts the board; the firmware keeps running.
- Nothing from the lightbar at all and no identification requests after a power cycle of the
  InCommand: brief section 4.
