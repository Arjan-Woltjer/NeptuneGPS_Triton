# Session 13 test brief — New Holland + Ag Leader, the address-205 visit

One question, one rig, about 30 minutes, no plough and no driving:
**does a control function the roll-call prune evicts now come back and stay?**
That is AgIsoStack fork patch #5, never yet run on hardware, plus patch #6,
which removes the dead code next to it. The result goes on upstream PR
[#718](https://github.com/Open-Agriculture/AgIsoStack-plus-plus/pull/718).

---

## 0. Why this rig

It is the only rig where we have seen the failure. In session 6 (2026-09-05)
the tractor's own ECUs churned `is now offline` → `has claimed address` every
~2 s, for the whole session:

| Session 6 log | Duration | Address 205 offline | Address 172 offline |
|---|---|---|---|
| `raw1_prepatch-full` | 1453 s | 455 | 322 |
| `raw2_postpatch-steady` | 981 s | 321 | 0 |
| `raw3_postpatch-join-test` | -- | 252 | 201 |

- **205 = `0xCD`, CNH System Monitor**, and **172 = `0xAC`, CNH Vehicle
  Navigation**, per the session 9 CANedge capture at van Mastwijk. The session 6
  serial log cannot confirm this: it prints every NAME as
  `000000000000000lx`.
- The John Deere is no use for this test. Session 11's serial logs there have
  no `[NM]` lines at all: that bus does not roll-call.
- The churn needs roll-calls, and in session 6 they came when **both
  terminals were live**: the New Holland VT and the InCommand 1200. One
  terminal alone gave zero.

Patch #4 has kept our VT/TC partner out of the prune since session 6. So this
test is about the **other** CFs on the bus, not about our connection.

---

## 1. What to flash

```
git fetch origin
git checkout test/session12
git pull
pio run -e teensy41_isobus -t upload
```

Same firmware as session 12. It pins the AgIsoStack fork at `9aa491e`
(patches #1–#6). Check `platformio.ini` shows that hash, not `0.1.5-neptune1`,
or you are testing the old library.

---

## 2. Setup

- **Tractor:** the New Holland with the InCommand 1200. Record the owner and
  where it is, in the timeline.
- **Both terminals powered**, the New Holland VT and the InCommand 1200. That is
  the configuration that roll-called every ~2 s.
- **CANedge on the bus**, logging before Triton powers on, termination off.
  **It is required this time.** It is the only proof that roll-calls happened
  (section 4), and the only way to read `0xCD`'s and `0xAC`'s NAMEs, which the
  serial log cannot print.
  **Check the SD card is in the CANedge before starting it.** The first
  session 13 run (2026-09-30) was done without one: the serial side looked
  like a pass, but with no capture it proves nothing.
- **Optional: Saturn as a second, passive logger**, set up as in session 12
  (`--label session13-nh-vanmastwijk`). It must be the receive-only build and
  never the VT app: this test counts joins and roll-calls, and a Saturn that
  address-claimed would be one more join. Listen-only, it doesn't exist on the
  bus. Wire and power it **before** the terminals come up, and leave it
  through the power-cycles.
- **Part 2 of this visit is `SESSION_14_TEST_BRIEF.md`** (driven, autosteer
  engaged, CANedge ch2 + Saturn on Ag Leader's CAN A): where does the
  InCommand put its cross-track error. Saturn is better spent there than as a
  second ISOBUS logger; ch1 already covers the ISOBUS.
- **Serial log** on from power-on.

---

## 3. The test

1. Power Triton. Wait for `vt=Y` and `tc=Y` on the 1 Hz line.
2. **Make sure roll-calls happen.** They are broadcast, so the serial log does
   not show them directly, and if patch #5 works there may be almost no
   `[NM]` lines either. So don't wait for evidence at the rig: after
   2 minutes, **power-cycle one terminal once** and note the time. In
   session 6 a terminal joining is what set them off. The CANedge will show
   afterwards whether they came.
3. Leave it for **15 minutes**. Do nothing. Note the times of anything you do
   touch.
4. **Optional, if time allows:** power the New Holland VT off and on again.
   That is session 6's worst case, the join storm. Wait 5 minutes.

---

## 4. Reading the result

**First, from the MF4: did roll-calls happen?** Count PGN 59904 (Request)
frames to global (`0x18EAFF..`) whose data starts `00 EE 00`, i.e. requests for
PGN 60928. Session 6 saw one every ~2 s. If there are none, stop here: the
test did not run. (Serial-log `NACK-ing PGN request for PGN 60928` lines don't
answer this. Only requests addressed to us are NACKed: session 6's post-patch
log shows 0 NACKs while 205 was pruned 321 times.)

**Then, from the serial log:**

```
grep -o "address [0-9]* and NAME" <log> | sort | uniq -c  # offline events per address
```

- **Pass:** 205 and 172 go offline **a few times at most**, then stay. The
  per-address counts are in single figures, against 321–455 in session 6.
- **Pass:** VT and TC stay up throughout, as in every session since patch #4.
- **Fail:** any address still going offline every 2–3 s. Keep the log and the
  MF4. That would mean patch #5 does not do what #718 says it does.
- **Not a result:** no roll-call requests in the MF4. The bus never
  exercised the prune, whatever the serial log says.

A handful of offline lines per address is normal: ECUs do go quiet.
Only a steady beat on one address is the failure.

---

## 5. What to capture

- The serial log from power-on, named
  `<date>_session13_<rig>-<owner>_<what>.log` in `Documentation/logs/`.
- The CANedge MF4, named with the card session, in `Documentation/canlogs/`,
  with a row in its `README.md`.
- A handwritten timeline: rig, owner, what was powered when, and any terminal
  power-cycles.

---

## 6. What this session can close

| Question | Then |
|---|---|
| Does a restored CF stay restored (patches #5 + #6)? | Post the per-address counts, session 6 against session 13, on upstream #718. Tag `0.1.5-neptune2` on the fork and switch `lib_deps` to it. Mark patch #5 field-verified in `AgIsoStackVendorPatches.md`. |
| Are 205 and 172 the CNH System Monitor and Vehicle Navigation? | Read their NAMEs from the MF4 and note them in `HardwareTestNotes.md`. |
