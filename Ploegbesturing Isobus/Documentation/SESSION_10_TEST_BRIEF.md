# Session 10 test brief

Everything on branch `fix/21-tc-counters` (PR #44) is desk work that has never
run on a tractor. This is what to flash, what to read, and what each result
would mean. Written 2026-09-09 evening, after the session 9 log analysis.

`main` already carries the full Track Control Level 1 DDOP (#43, merged after
session 9 confirmed it on hardware). This branch adds four things on top, none
of them hardware-verified.

---

## 1. What to flash

```
git fetch origin
git checkout fix/21-tc-counters
git pull
```

**Before building, check the AgIsoStack vendor patches are present** in
`.pio/libdeps/teensy41_isobus`. They live in gitignored territory and a clean
clone does not have them — see
`NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md`. Without
them the control-function eviction behaviour silently returns and the session
is wasted.

Then `pio run -e teensy41_isobus -t upload`. Expect 124/124 native tests and a
clean build of both `teensy41_isobus` and `teensy41_serial`.

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

**Confirm the new debug lines appear at all.** Menu option 1, full dump. If any
of the fields in section 4 below is missing, the wrong build is on the board.

---

## 3. The experiments, in priority order

### 3.1 Autosteer engaged — the big one

**This single test addresses #21 and #42 together**, which is why it is first.
The reasoning: DDIs 508–511 are *track numbering*, and a terminal has little
reason to publish those while nothing is following a line. PGN 45056
(`0xAD00`, Guidance System Command) had **zero frames on both rigs** in session
9, so autosteer was engaged in neither.

Engage autosteer, drive a line, and watch:

| Read | Where | Means |
|---|---|---|
| `Tramline setpoint (DDI 506)` | full dump | **any** value, 0 included, = the handshake completed |
| `Guidance track (DDI 507-511)` | full dump | populated track numbering, even if 506 stays silent |
| `PGN 129283 XTE NMEA2000` counter | full dump | non-zero = the Ag Leader's XTE carrier is found (#42) |
| `[on bus] Measurement commands` | full dump | the TC configuring reporting on our DPDs |

Do it on **John Deere first**. It is the rig that engaged in session 9 (a
measurement command on DDI 515) and the only one that has ever given an
explicit error message, so a negative there is interpretable. A negative on Ag
Leader is not, for the reason in 3.3.

### 3.2 Wider/Narrower on the John Deere, above 1.8 km/h

Session 9's null result on this is **fully explained and carries no
information**: two interlocks were holding the control path independently —
guidance quality 0, and peak speed 0.38 m/s against a `MINSPEED` of 0.5 m/s.

To actually test it you need **both** at once:

- ground speed above **0.5 m/s = 1.8 km/h**, and
- `IsRtkQuality=Y`.

On the John Deere guidance was already valid in session 9, so speed is the only
missing piece. Check `MinSpeed` and `IsRtkQuality` in the dump *before*
concluding anything from a press that does nothing.

### 3.3 Ag Leader / CNH — check the lockout first

Do not repeat session 9's mistake of reading that rig's silence as the terminal
declining to engage. The capture showed the **CNH tractor reported
`MechanicalSystemLockout = LOCKED OUT` for all 8420 frames**. Nothing
guidance-related could have happened regardless of the terminal.

So on that rig, **read the new guidance line first**:

```
Guidance machine info (PGN 44032): N msgs, last X ms ago
  lockout=...  steeringReady=...  remoteEngage=...  curvature=... 1/km
```

If it still says locked out, the rig cannot answer any guidance question and
there is no point running 3.1 there. Find out why it is locked out first.

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
happening. Session 9 baselines: John Deere `lockout=no, steeringReady` toggling
between not-ready and READY; CNH `lockout=LOCKED OUT, steeringReady=n/a`.

### GNSS position data (PGN 129029) — the quality field

```
PGN 129029 GNSS position: N msgs, method=4 RTK FIXED  SVs=18  HDOP=0.85  last X ms ago
PGN 129027 position deltas: N
```

**This is the only message on any bus captured so far that carries a GNSS
quality indicator**, and it is why Raven coverage matters beyond position.
Field 8 ("Method, GNSS") maps onto our `quality` with no translation: 4 = RTK
fixed, the value `IsRtkQuality()` tests for.

Without it, an ISOBUS build can only get quality from the John Deere legacy XTE
decoder (SA `0x2A`) or the Trimble one (`0xAA`). On a rig with neither, quality
is stuck at 0 and `IsRtkQuality()` can never be true — a third interlock, on top
of quality and speed, that nothing else could satisfy.

**If this counter stays 0 on a Raven rig, that is itself the result** and worth
capturing: see the caveat below.

---

## 5. Traps, honestly stated

**The 129027/129029 receive path has never run.** No NMEA2000 guidance PGN has
ever reached this firmware on any rig — not because it is broken, but because
none was ever broadcast while we were listening. Its tests are built from the
NMEA 2000 v1.301 Appendix B spec, **not from captured frames**, unlike every
other fixture in that file. Green tests mean "matches the spec as we read it".

So if a Raven publishes 129029 and our counter stays 0, **do not conclude the
Raven is not sending it.** Take a CANedge capture and check the bus directly —
the bug could equally be our fast-packet registration, which is also
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

---

## 6. What to capture

- **CANedge on the ISOBUS segment**, logging *before* the board powers on.
  Passive tap, and the logger's own termination **off** — the segment is
  already terminated at both ends.
- **Triton on the bus at the same time**, via the test connector. Session 9 was
  the first time our own control function appeared in a capture and it is what
  made the DDI 515 exchange provable.
- **Serial logs** alongside, so the two can be correlated.
- **Write down date, rig and owner by hand.** The CANedge clock is wrong — its
  GNSS never gets a fix, so its timestamps read 2001 and carry none of this.
  Name files as `<date>_session<N>_<rig>-<owner>_log<card session>_<what>`.

Copy captures to `Documentation/canlogs/` and add a row to that folder's
`README.md` provenance table.

---

## 7. Open questions this session could close

| Issue | Question | Test |
|---|---|---|
| #21 | Does DDI 506 ever arrive? | 3.1, John Deere first |
| #42 | Where is the Ag Leader's cross-track error? | 3.1 — watch 129283 and 129029 |
| #18 | Does a VT/TC connection recover on its own? | Pull the ISOBUS connector ~15 s with everything live, plug back in **without touching power** |
| #45 | — | No test needed; it is a decision about the manufacturer code and identity number |

#18 is cheap and has been open since session 8, where the watchdogs fired but
the board was powered down before reconnection so recovery was never observed.
