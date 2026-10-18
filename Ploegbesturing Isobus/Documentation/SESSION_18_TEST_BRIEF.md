# Session 18 test brief -- van Mastwijk: AUTO with the hitch down on the byte-7 decoder, and the session 18 fixes

Written 2026-10-16 after session 17 (capture log 57). Same rig as sessions 13-17: CNH with the
InCommand 1200, SteerCommand Z2, one ISOBUS (CNH VT 0x26, InCommand VT 0x80, TC 0xF7, display
function 0xF5, the InCommand's inventory function 0x2B).

Previous results: `logs/2026-10-16_session17_vanmastwijk_timeline.md` on **`test/session12`** (that
branch keeps the session 13-17 notes; this session's notes go on `test/session18`, see section 9).

## 0. What changed since session 17, and what this session decides

Session 17 passed everything except D4 (AUTO with the hitch down), which was skipped because the
decoder froze the XTE on the AB line: byte 2 of PGN 65462 turned out to be the pass number, not a
line state. Since then main has merged #188 (failover), #190 (the library), #191 (MW08 screen),
#205 (speed in km/h) and #207 (the lightbar channel). The board gets these for the first time:

| Change | PR | What to see |
|---|---|---|
| XTE commits whenever byte 7 = 4 (line present); the pass number and side are reported | #207 (merged) | `xte=` follows `lb=` on **every** pass, AB line included. Dump: `-> 103 cm right, pass 1R, ...`; 1 Hz: `lb=103R-p1R:...` |
| **New ISOBUS NAME:** device class 2 (Tillage), function 132 (Tillage Machine Control), was 8 (Root Harvesters) / 16 | #219 (#202) | **First join on each VT: a full pool upload, not NVM** (the stored pool belongs to the old NAME). The TC may treat the plough as a new implement. Later joins load from NVM again. |
| Failover's request for address claim from the NULL address 0xFE | #217 (#199) | **No** `[AC] Address violation for address 129` lines anywhere (session 17: 28) |
| A request for address claim addressed to us is answered with the claim only | #220 (#23) | **No** `[PR]: NACK-ing PGN request for PGN 60928` lines (session 17: 3 at the replug) |
| The last PGN 129283 frame and reject reasons in the dump and the 1 Hz line | #218 (#201) | 1 Hz: `x129283=CD:0:-` (0xCD's empty answers); dump: `last SA=0xCD len=0 raw=-`, `rejected: length=N` |
| Max correction kept across boots, stored per share | #164 (#163) | A max correction above the old 10 cm boundary survives a power cycle |
| Rarely used code from flash (FLASHMEM) | #192 | Nothing visible; more stack headroom (77.7 KB) |

This session decides:

1. **D4: does the plough act correctly on the XTE in AUTO with the hitch down,** on the AB line and on 1R? This is the goal of the whole lightbar work (#42).
2. Do both terminals and the TC accept the **new NAME** (#202), and show a tillage implement?
3. Are the address violations (#199) and the claim-request NACKs (#23) gone?
4. Does the 129283 readout show what is on the bus (#201)?
5. Does the max correction survive a power cycle (#163/#164)?
6. **#203:** the InCommand's SPN 2129 / FMI 31 fault. The owner can reproduce it **without** the plough control; confirm with and without.
7. Regression: failover F1-F4, now with the 0xFE claim request in it.

## 1. Kit and build

- Plough control 1324910 must run **`test/session18`** (`bca348b` or later). Flash with the field
  laptop: `git fetch && git switch test/session18 && git pull`, then
  `pio run -d "Ploegbesturing Isobus" -e teensy41_isobus -t upload`, **with only the plough's Teensy
  on USB**. Write down `git rev-parse --short HEAD`. Close the Teensy Loader only once the board is
  back on its serial port, not on a timer.
- **Build check, on the bus with the InCommand on:** the dump's last-65462 line says **`pass ...`**
  (session 17's build said `state ...`), and the first join shows a full pool upload on the VT it binds
  to. Either one missing means the wrong build.
- CANedge AD4F266A, channel 1 on the in-cab connector; uploads by itself over LTE. Power it up early.
- Field laptop, serial logger (1 Hz line on by itself). Dump (menu 1) at every step below.

## 2. Setup and the new NAME

Start order as session 17: CNH VT up, InCommand 1200 **off**, plough control on USB first, then onto
the bus. This gives a stretch where we request 129283 and 0xCD answers.

| Step | Do | Expect |
|---|---|---|
| S1 | Plough on USB alone, ~1 min. | Every 10 s `VT: partner 0xFE silent -- requesting address claims`, and **no** `[AC] Address violation` line after it (#199). |
| S2 | Plough onto the bus (CNH VT only). Dump at +60 s. | 0x81 and 0xDC claimed; VT bound to 0x26; **full pool upload** (new NAME, first time); no address-violation lines at the join (session 17: 7). 1 Hz line has `x129283=CD:0:-`; dump `rejected: length=` counting (#201). |
| S3 | InCommand on. Dump at +2 min. | TC 0xF7 connected under the new NAME (note any "new implement" question on the 1200); identification 8/8, `stack partner=0xF5`, 65462 at 5 Hz; dump line says `pass ...`. |
| S4 | On both terminals, look at how the plough is listed (implement type/icon, device list). | Tillage / plough, not a harvester (#202). A photo helps. |

## 3. Regression (standing still)

| Step | Do | Expect |
|---|---|---|
| R1 | Dump. | As session 17: VT, TC, identification, 65462 committed, `failures=0` (session 17's single failure was the software-ID BAM overlap; a 1 here is not new). |
| R2 | Pull the plough's ISOBUS plug, 30 s, plug back. Dump at +60 s. | Back on the same VT, pool from NVM this time (second join under the new NAME). **No** `[PR]: NACK-ing PGN request for PGN 60928` (#23) and **no** `[AC] Address violation` (#199). Note bus-off/error-passive entries (session 17: 2 bus-off on the replug; #200 is not fixed yet). |

## 4. Drive: the decoder, then AUTO with the hitch down

AB line, RTK fixed. Byte 2 of 65462 is the pass number (`p0` = the AB line, `p1L`, `p1R`, ...).

| Step | Do | Expect |
|---|---|---|
| D1 | Drive the **AB line** (`p0`) engaged, 50 m, then by hand 0.5 m off. | `xte=` follows `lb=` with the sign throughout. Session 17 froze here for 75 s. |
| D2 | Over to **1R**, then **2L/3L** if the field allows. | `xte=` follows on every pass; `lb=` shows the pass (`p1R`, `p2L`, ...). |
| D3 | Headland turn. | `xte=` follows the display's re-referencing (it swings for a few seconds while the pass changes); nothing freezes. |
| **D4** | **AUTO with the hitch down,** on the AB line, then on 1R. Steer off the line on purpose by ~0.3 m each side, by hand. Operator ready to stop. | The plough corrects **toward** the line, in the right direction for each side, and never beyond the max correction. Note direction and size against `xte=`. With the hitch up or AUTO off it must not act. |

## 5. Max correction (#163/#164)

| Step | Do | Expect |
|---|---|---|
| M1 | In the calibration wizard, set the max correction **above** the old 10 cm boundary (the wizard shows it per share, in mm, in 5 mm steps). Save. Note the value. | Saved. |
| M2 | Power-cycle the plough control. Open the wizard. | The same value is back. Before #164 it came back as 50. |

## 6. #203: the InCommand's SPN 2129 / FMI 31 fault

| Step | Do | Expect |
|---|---|---|
| X1 | Plough control **unplugged** from the ISOBUS. Power-cycle the InCommand (or do whatever reproduces the fault for the owner). Open the 1200's fault list. | Owner's observation: the fault appears without us. Note against which **source address / device** it is listed. |
| X2 | Plough control plugged in. Same check. | Same fault, same source = not ours. Listed against 0x81 only when we are on the bus = ours after all. |

## 7. VT failover with the new claim request (#188 + #217)

| Step | Do | Expect (session 17 times) |
|---|---|---|
| F1 | Bound to 0x80: InCommand off. | Switch to 0x26 (session 17: ~15 s), MW08 from NVM (NAME already stored there from S2). **No address-violation lines** while it requests claims. |
| F2 | InCommand on. | Stays on 0x26; TC back; identification, `stack partner=0xF5`. |
| F3 | CNH VT off by its key. | Switch to 0x80 (~15 s). First time on 0x80 under the new NAME: a **full upload** there is expected unless S3/F2 already stored it. |
| F4 | CNH VT on. | Stays on 0x80; 0xF5 not offline. |

## 8. Checklist (copy into the timeline, tick live)

SETUP
- [ ] Build: `test/session18` @ ________ flashed; dump line says `pass ...` Y/N
- [ ] CANedge on early: new session ____ uploading
- [ ] S1 USB alone: claim requests every 10 s, address violations: ____ (expect 0)
- [ ] S2 join: full pool upload Y/N; address violations at join: ____ (expect 0); `x129283=` shows ________; `rejected: length=` ____
- [ ] S3 InCommand on: TC Y/N (new-implement question Y/N: ______), identification __/__, `stack partner=` ____, `pass` in dump Y/N
- [ ] S4 shown as tillage/plough on CNH VT Y/N, on the 1200 Y/N (photo Y/N)

REGRESSION
- [ ] R1 VT/TC/identification/65462 OK Y/N, `failures=` __
- [ ] R2 replug: back on ____, from NVM Y/N, NACK lines ____ (expect 0), violations ____ (expect 0), bus-off __ error-passive __

DRIVE
- [ ] D1 AB line: `xte=` follows `lb=` Y/N
- [ ] D2 1R Y/N, 2L Y/N, 3L Y/N (passes shown as ______)
- [ ] D3 turn: no freeze Y/N
- [ ] D4 AUTO + hitch down on the AB line: corrects toward the line Y/N, direction right Y/N, within max correction Y/N
- [ ] D4 AUTO + hitch down on 1R: same Y/N; hitch up / AUTO off does not act Y/N

MAX CORRECTION
- [ ] M1 set to ____ (per share)   - [ ] M2 after power cycle: ____ (same Y/N)

#203
- [ ] X1 without the plough: fault shown Y/N, against ______
- [ ] X2 with the plough: fault shown Y/N, against ______

FAILOVER
- [ ] F1 InCommand off __:__:__ -> 0x26 at __:__:__ (__ s), violations ____
- [ ] F2 stays on 0x26 Y/N
- [ ] F3 CNH VT off __:__:__ -> 0x80 at __:__:__ (__ s), full upload Y/N
- [ ] F4 stays on 0x80 Y/N, 0xF5 offline Y/N

END
- [ ] final dump   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [ ] session ____

## 9. Afterwards

Serial log and timeline (this checklist filled in) go on **`test/session18`** under
`Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session18_vanmastwijk_*`. MF4s arrive in
OneDrive `CANedge logs/bucket-AD4F266A/session-<n>/` by themselves. If the workstation session is not
reachable, leave a `logs/session18_vanmastwijk_handover.md` as in session 17.

Results go to the PRs carried on the branch: #219 (S2-S4, F3), #217 (S1, S2, R2, F1), #220 (R2), #218
(S2), #164 (M1-M2), #207/#42 (D1-D4), and #203 (X1-X2). Each PR merges to main only on its own
evidence.
