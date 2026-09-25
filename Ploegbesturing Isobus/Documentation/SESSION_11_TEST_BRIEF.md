# Session 11 test brief — John Deere (van Os)

Everything on branch `fix/21-tc-counters` (PR #44) is desk work that has never
run on a tractor. This is what to flash, what to read, and what each result
would mean.

**This supersedes the session 10 brief.** Session 10 ran on 2026-09-11 at the
same John Deere rig, but **Triton was never on the bus** — it was a JD-only
capture to harvest the terminal's VT object pools (card session 29). None of
this branch was exercised. So the experiments below are unchanged in substance,
but two of session 10's findings change the *reasoning* around them, and the
flashing instructions have gone out of date. Both are in section 0.

`main` already carries the full Track Control Level 1 DDOP (#43, merged after
session 9 confirmed it on hardware). This branch adds four things on top, none
of them hardware-verified.

---

## 0. What changed since the last brief — read this first

**The vendor-patch step is gone. Do not go looking for it.** The old brief told
you to check the AgIsoStack patches were present in gitignored `.pio/libdeps`
before building, "or the session is wasted". That is no longer true and no
longer possible: the project now depends on
`https://github.com/Arjan-Woltjer/AgIsoStack-Arduino.git#0.1.5-neptune1`, a
fork carrying every patch as a real commit on a tag. A clean clone gets them.
`AgIsoStackVendorPatches.md` is now history, not a checklist.

**The test count moved.** The branch was rebased onto `main` on 2026-09-15 and
picked up the native-test work (#86, #98, #113). Expect **244** native tests in
this project, not the 124 the old brief quoted. A lower number means the wrong
build.

### Session 10 finding 1 — you cannot confirm autosteer from the bus

The old brief reasoned: PGN 45056 (`0xAD00`, Guidance System Command) had zero
frames in session 9, *therefore* autosteer was engaged in neither session.

**Session 10 refutes that.** Autosteer was engaged for the entire driven
segment and `0xAD00` still never appeared. On this rig it looks like it is
simply never broadcast, independent of steering state. `0xAC00` (curvature,
from `0xF0`) is present throughout at 9.9 Hz as always.

The experiment below is unaffected — engaging autosteer is still the right
test. What is affected is the *evidence*: **write down by hand when autosteer
was engaged and disengaged, with times**, because nothing in the capture will
tell you afterwards.

### Session 10 finding 2 — the speed interlock is no longer an unknown

Three interlocks can hold the plough: guidance quality, `MINSPEED` (0.5 m/s =
1.8 km/h), and the tractor's own `MechanicalSystemLockout`.

Session 9's runs peaked at **0.38 m/s** and never cleared MINSPEED, which is
why its Wider/Narrower null result carried no information. Session 10 measured
a sustained **1.15 m/s (4.1 km/h)** on an ordinary driven line, confirmed
independently against wheel-based speed (PGN 65096), not just GPS.

So a normal working pass clears MINSPEED comfortably. On the John Deere,
guidance quality was already valid in session 9 and this rig is not locked out.
**That leaves quality as the one interlock still worth checking before you
touch anything** — see 3.2.

---

## 1. What to flash

```
git fetch origin
git checkout fix/21-tc-counters
git pull
pio run -e teensy41_isobus -t upload
```

Expect **244/244** native tests and a clean build of both `teensy41_isobus` and
`teensy41_serial`. No manual patch step (section 0).

**Labels to expect on the terminal:** DDOP structure label **TC05**, VT object
pool label **MW04**. If a terminal shows an old screen, the label did not
change — that is the failure mode session 9 lost time to.

---

## 2. Bench checks, before going near a tractor

Both are cheap and both catch things that would otherwise look like a failed
experiment.

**Dump the pools.** Debug menu **option 4** (DDOP) and **option 5** (VT object
pool), then convert and open the DDOP in
[AgIsoDDOPGenerator](https://github.com/Open-Agriculture/AgIsoDDOPGenerator):

```
python -c "import sys;open('DDOP.iop','wb').write(bytes.fromhex(''.join(sys.stdin.read().split())))" < hex.txt
```

Remember what session 9 established: **the dump is the pool as authored, not
as uploaded.** A version-3 Task Controller makes AgIsoStack rebuild the DDOP
and drop the version-4-only extended structure label, so the bytes on the wire
are one shorter. A malformed pool still shows up here, which is the point.

Session 10 added `extract_iop.py` and `validate_iop.py` under
`NeptuneGPS Documentation/ISOBUS/tools/`. `validate_iop.py` walks a pool
object-by-object and lands exactly on the final byte if and only if every
object is well-formed — a faster integrity check on our own VT pool than
opening it in a designer.

**Confirm the new debug lines appear at all.** Menu option 1, full dump. If any
of the fields in section 4 below is missing, the wrong build is on the board.

---

## 3. The experiments, in priority order

### 3.1 Autosteer engaged — the big one

**This single test addresses #21 and #42 together**, which is why it is first.
The reasoning: DDIs 508–511 are *track numbering*, and a terminal has little
reason to publish those while nothing is following a line.

Engage autosteer, drive a line, and watch:

| Read | Where | Means |
|---|---|---|
| `Tramline setpoint (DDI 506)` | full dump | **any** value, 0 included, = the handshake completed |
| `Guidance track (DDI 507-511)` | full dump | populated track numbering, even if 506 stays silent |
| `PGN 129283 XTE NMEA2000` counter | full dump | non-zero = an NMEA2000 XTE carrier is present (#42) |
| `[on bus] Measurement commands` | full dump | the TC configuring reporting on our DPDs |

**Record engagement and disengagement times by hand** (section 0). The capture
will not show them.

Do it on **John Deere**. It is the rig that engaged in session 9 (a measurement
command on DDI 515) and the only one that has ever given an explicit error
message, so a negative there is interpretable. A negative on Ag Leader is not,
for the reason in the appendix.

### 3.2 Wider/Narrower — a real test for the first time

Session 9's null result on this is **fully explained and carries no
information**: guidance quality was 0 and peak speed was 0.38 m/s against a
`MINSPEED` of 0.5.

Session 10 settled the speed half (section 0). So before pressing anything,
read **one** field in the dump:

- `IsRtkQuality=Y`?

If yes, drive above 1.8 km/h and press Wider/Narrower. A press that does
nothing is then a genuine finding rather than a known-blocked control path.

If `IsRtkQuality=N` on this rig, check `PGN 129029 GNSS position` first. If
that counter is non-zero, the new receive path is working and the rig really is
reporting sub-RTK quality. If it is zero, quality is coming only from the
legacy John Deere decoder (SA `0x2A`) and the NMEA2000 path never ran. Those
are different results, and the counter is what separates them.

### 3.3 Reconnect recovery (#18) — cheap, and open since session 8

Pull the ISOBUS connector for ~15 s with everything live, then plug it back in
**without touching power**.

The watchdogs fired on hardware in session 8, but the board was powered down
before reconnection, so recovery has never actually been observed. #113 merged
native tests covering both the VT and TC watchdogs, which means the logic is
pinned — it does not mean it recovers on a real terminal. That is what this
tests.

---

## 4. New in the debug dump, and what each field means

### Task Controller counters — now two groups

```
[via stack] Value cmds to handler:  N   last DDI=...
[via stack] Value req callbacks:    N   (mostly AgIsoStack's own re-polling -- NOT bus traffic)
[on bus]    Requests for our values: N
[on bus]    Set-value commands:      N
[on bus]    Measurement commands:    N   last DDI=... type=...
[on bus]    Other, addressed to us:  N
```

**Read the `[on bus]` group.** The `[via stack]` numbers are what reached our
handlers, and session 9 proved they answer "did the TC ask us anything?"
wrongly in both directions at once: `Value commands` read **0** while the John
Deere TC was actively commanding us, and `Value requests` read ~20 000/s, which
is impossible on a 250 kbit bus and was AgIsoStack polling itself.

For calibration, replayed over the session 9 captures the `[on bus]` group
would have read:

| | John Deere | Ag Leader |
|---|---|---|
| Requests for our values | 1 | 0 |
| Measurement commands | 1 — DDI 515, type 8 | 0 |
| Other, addressed to us | 5 | 18 |

So on John Deere expect small non-zero numbers. On Ag Leader, everything but
"other" was zero — the TC accepted our pool and never addressed us again.

### Guidance machine info (PGN 44032)

The standard ISO 11783-7 guidance channel, broadcast at 10 Hz by the tractor
ECU on both rigs with no Task Controller session needed. **Diagnostics only** —
it carries *curvature*, not cross-track error, and nothing from it reaches the
control path.

Its value is the status fields, which say *why* guidance is or is not
happening. Session 9 baseline for this rig: John Deere `lockout=no`,
`steeringReady` toggling between not-ready and READY (218 times).

### GNSS position data (PGN 129029) — the quality field

```
PGN 129029 GNSS position: N msgs, method=4 RTK FIXED  SVs=18  HDOP=0.85  last X ms ago
PGN 129027 position deltas: N
```

**This is the only message on any bus captured so far that carries a GNSS
quality indicator.** Field 8 ("Method, GNSS") maps onto our `quality` with no
translation: 4 = RTK fixed, the value `IsRtkQuality()` tests for.

Without it, an ISOBUS build can only get quality from the John Deere legacy XTE
decoder (SA `0x2A`) or the Trimble one (`0xAA`). On a rig with neither, quality
is stuck at 0 and `IsRtkQuality()` can never be true.

**If this counter stays 0, that is itself the result** and worth capturing —
see the caveat below.

---

## 5. Traps, honestly stated

**The 129027/129029 receive path has never run.** No NMEA2000 guidance PGN has
ever reached this firmware on any rig — not because it is broken, but because
none was ever broadcast while we were listening. Its tests are built from the
NMEA 2000 v1.301 Appendix B spec, **not from captured frames**, unlike every
other fixture in that file. Green tests mean "matches the spec as we read it".

So if a receiver publishes 129029 and our counter stays 0, **do not conclude
the receiver is not sending it.** Take a CANedge capture and check the bus
directly — the bug could equally be our fast-packet registration, which is also
first-run code.

**129029 is fast packet**, 43 bytes, registered through the fast-packet
protocol with `allow_any_control_function(true)` rather than the ordinary PGN
callback. That is a different code path from every other PGN we receive.

**Nothing in this branch changes the control path except one thing**: PGN
129029 now writes `quality` into `GuidanceSource`. If a rig starts publishing
it, `IsRtkQuality()` can become true where it never could before — which is the
intent, but it means the plough can leave HOLD on a rig where it previously
could not. Worth knowing before the first press.

**The DDOP and VT labels have not moved** (TC05 / MW04). If the pool is changed
during the session, bump them — `TC0x` for the DDOP, `MW0x` for the VT pool.
Session 9 lost time to a stale `MW03` serving a weeks-old screen with nothing
logged and nothing errored.

**Distances called out at the rig are not measurements.** Session 10's driven
segment was called "15 m forward, 15 m back" at the time and was 37.4 m out and
34.1 m back — wrong by more than 2x. If a distance matters to a conclusion, get
it from the log, not from the cab.

---

## 6. What to capture

- **CANedge on the ISOBUS segment**, logging *before* the board powers on.
  Passive tap, and the logger's own termination **off** — the segment is
  already terminated at both ends.
- **Triton on the bus at the same time**, via the test connector. Session 9 was
  the first time our own control function appeared in a capture and it is what
  made the DDI 515 exchange provable.
- **Serial logs** alongside, so the two can be correlated.
- **Write down date, rig and owner by hand**, plus the autosteer engage and
  disengage times. The CANedge clock is wrong — its GNSS never gets a fix, so
  its timestamps read 2001 and carry none of this. Name files as
  `<date>_session<N>_<rig>-<owner>_log<card session>_<what>`.

Copy captures to `Documentation/canlogs/` and add a row to that folder's
`README.md` provenance table.

---

## 7. Open questions this session could close

| Issue | Question | Test |
|---|---|---|
| #21 | Does DDI 506 ever arrive? | 3.1 |
| #42 | Is there an NMEA2000 XTE carrier on this rig? | 3.1 — watch 129283 and 129029 |
| #18 | Does a VT/TC connection recover on its own? | 3.3 |
| #45 | — | No test needed; it is a decision about the manufacturer code and identity number |

---

## Appendix — if the Ag Leader / CNH rig comes up instead

Do not repeat session 9's mistake of reading that rig's silence as the terminal
declining to engage. The capture showed the **CNH tractor reported
`MechanicalSystemLockout = LOCKED OUT` for all 8420 frames**. Nothing
guidance-related could have happened regardless of the terminal, so that
session is not evidence about the Ag Leader TC at all.

On that rig, **read the new guidance line before anything else**:

```
Guidance machine info (PGN 44032): N msgs, last X ms ago
  lockout=...  steeringReady=...  remoteEngage=...  curvature=... 1/km
```

If it still says locked out, the rig cannot answer any guidance question and
there is no point running 3.1 there. Find out why it is locked out first.

Also remember that rig is **two vendors**: manufacturer 94 = CNH Industrial
(the tractor: `0x26`, `0xAC`, `0xCD`, `0xF0`), 97 = Ag Leader (the kit: `0x2B`,
`0x80`, `0xE9`, `0xF5`, `0xF7`). Always say which one a finding is about.
