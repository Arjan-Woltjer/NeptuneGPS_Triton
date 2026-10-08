# CANedge full-bus captures

Raw MF4 CAN logs from real-bus test sessions, referenced by
[`../HardwareTestNotes.md`](../HardwareTestNotes.md). These sit here rather
than alongside the serial logs in [`../logs/`](../logs/) because they are
binary whole-bus recordings, not our own board's serial output -- they
contain every control function's traffic, including the vendors' own.

## Provenance

**The RTC timestamps inside these files are wrong** (they read 2000-2001).
The CANedge3's GNSS never gets a fix, so it never sets its clock, and the
card's own session numbering is the only reliable ordering. Everything below
therefore has to be recorded by hand -- if it is not in this table, it is not
recoverable from the file.

| File | Card session | Date | Rig | Owner | Duration | Frames |
|---|---|---|---|---|---|---|
| `2026-09-08_session8_jd-vanos_log24_gps-connect-no-reception.MF4` | `AD4F266A` / 24 | 2026-09-08 | John Deere | van Os | 678 s | 232 578 |
| `2026-09-08_session8_jd-vanos_log25_xte-outside.MF4` | `AD4F266A` / 25 | 2026-09-08 | John Deere | van Os | 115 s | 45 419 |
| `2026-09-09_session9_jd-vanos_log26_full-ddi-set.MF4` | `AD4F266A` / 26 | 2026-09-09 | John Deere | van Os | 454 s | 192 034 |
| `2026-09-09_session9_agleader-vanmastwijk_log27_faulty-short.MF4` | `AD4F266A` / 27 | 2026-09-09 | Ag Leader kit on a CNH tractor | van Mastwijk | 22 s | 1 246 |
| `2026-09-09_session9_agleader-vanmastwijk_log28_main.MF4` | `AD4F266A` / 28 | 2026-09-09 | Ag Leader kit on a CNH tractor | van Mastwijk | 842 s | 230 490 |
| `2026-09-11_session10_jd-vanos_log29_iop-harvest.MF4` | `AD4F266A` / 29 | 2026-09-11 | John Deere | van Os | 529 s | 268 123 |
| `2026-09-25_session11_jd-vanos_log30_autosteer-reverse.MF4` | `AD4F266A` / 30 | 2026-09-25 | John Deere | van Os | 2729 s | 1 143 997 |
| `2026-10-01_session13_nh-vanmastwijk_log31_join-addressed-requests-vt-off.MF4` | `AD4F266A` / 31 | 2026-10-01 | Ag Leader kit on a CNH tractor | van Mastwijk | 3070 s | 843 066 |
| `2026-10-01_session13_nh-vanmastwijk_log32_after-canedge-restart.MF4` | `AD4F266A` / 32 | 2026-10-01 | Ag Leader kit on a CNH tractor | van Mastwijk | 136 s | 37 340 |
| `2026-10-08_lemken_agleader-vanmastwijk_log42_ibbc-lemken-l160-xte.MF4` | `AD4F266A` / 42 | 2026-10-08 | Ag Leader kit on a CNH tractor, new harness; Lemken plough control + L160 lightbar on the IBBC, ch2 on the IBBC | van Mastwijk | 347 s | 155 482 |

Both were recorded during **Session 8** (see `HardwareTestNotes.md`). Card
`AD4F266A` sessions 6-10 are real ISOBUS; sessions 11-23 are a different
machine's powertrain bus and are not ours -- always verify the bus before
trusting a capture off this card.

- **log 24** -- indoors on the rig. The GPS module is **connected partway
  through the recording** (its address claim lands at t = 462 s), and it has
  **no GPS reception** for the whole log. This is the "what does the bus look
  like without a fix" reference.
- **log 25** -- outdoors, GPS live with an RTK fix, recorded while the
  operator called out the terminal's own cross-track error. This is the log
  that carries a **real XTE**.

### Session 9, 2026-09-09

**Triton is on the bus in all three**, at SA `0x81` -- the first captures that
contain our own control function, thanks to a test connector that lets the
plough control and the logger share the segment.

- **log 26** -- John Deere, van Os. First rig run of the full Tramline Control
  Level 1 DDI set (firmware `feat/21-full-tramline-ddop`, structure label
  TC05). Contains the **first Process Data exchange a Task Controller has ever
  had with us**: `0xF7` sends a MeasurementChangeThreshold and a RequestValue
  on **DDI 515**, and `0x81` answers with a Value.
- **log 27** -- Ag Leader/CNH, van Mastwijk. **Faulty and short**: the tractor
  had to be restarted. Kept only so the session numbering is complete; nothing
  to read here.
- **log 28** -- Ag Leader/CNH, van Mastwijk. The real one. Also records the
  MW04 VT object pool upload in full, which is useful independently of
  guidance work.

Note log 28's rig is **two vendors**: a CNH tractor (manufacturer 94) carrying
an Ag Leader kit (97), with a Virtual Terminal from each. Calling it "the Ag
Leader rig" hides that. Analysed in
`NeptuneGPS Documentation/ISOBUS/research/agleader-cnh-bus-inventory-2026-09-09.md`.

Triton is **not** on the bus in the two session-8 files: the plough control had
already been disconnected from the ISOBUS by that point in the session (see
Session 8's `#18` notes). It **is** present in all three session-9 files. No control function claims with our manufacturer
code 1407, and neither log can be paired against a Triton serial reading. What
they do carry is the vendors' own guidance traffic, which is what issues #20
and #21 needed.

### Session 10, 2026-09-11

**Triton is NOT on the bus** -- the plough control's latest branch was not
flashed for this outing, this was purely a JD-only capture. John Deere, van
Os, same rig as sessions 8/9. Startup (address claims + both VT object pool
uploads) followed by driving a line out and back, called "autosteer engaged"
at the rig -- but see the 2026-09-22 re-analysis note at the end of this
entry: the bus only reports steering-ready for 43 s of it.

**The driven segment, measured from the log** (log time 298-441 s): one pass
of **37.4 m**, a turnaround, then **34.1 m** back, plus a final 14.5 m
forward. Total path 143 m, net displacement 0.7 m -- it ended where it
started. Sustained **~1.15 m/s (4.1 km/h)**. Called at the rig as "15 m
forward, 15 m back", which was an eyeball estimate; the GPS distance is
confirmed against the tractor's own wheel-based speed (PGN 65096), which
integrates to ~40 m and ~36 m over the same two windows -- independent of the
GPS, so the ~35 m figure is real.

It **turned around, it did not reverse**: course (PGN 65256) holds 171 deg
outbound and 351 deg inbound, exactly 180 deg apart, sweeping through
intermediate values in between.

Worth noting for #21: at ~1.15 m/s this is the **first capture that clears the
plough's 0.5 m/s MINSPEED interlock** -- session 9 peaked at 0.38 m/s, so
speed can no longer be the thing holding the plough in HOLD on a run like
this.

- **log 29** -- 529 s, 268 123 frames. Full startup captured: both John Deere
  control functions that own a screen push their VT object pool to the
  terminal (`0x26`) and the terminal accepts both (`End of Object Pool
  Response` error code `0x00`).
  - `0xF0` (**Tractor ECU**, fn=134) pushes **two** pools back to back,
    21 643 B then 1 260 B.
  - `0x1C` (**GPS receiver / StarFire**, fn=23, "Vehicle Navigation") pushes
    a **327 501 B** pool, then uploads the **exact same pool a second time**
    (byte-identical, confirmed with `cmp`) about 17 s later.

  All four uploads reassembled and reconciled byte-for-byte against the RTS
  session sizes with `tools/extract_iop.py` (new, in the Documentation repo);
  the duplicate StarFire pool was kept only once. Harvested `.iop` files:
  [`2026-09-11_session10_jd-vanos_log29_iops/`](2026-09-11_session10_jd-vanos_log29_iops/).
  This is the actual capture the 2026-09-07 research note
  (`isobus-jd-gps-objectpool-capture-2026-09-07.md`) planned for -- open the
  `.iop` files in
  [AgIsoTerminalDesigner](https://open-agriculture.github.io/AgIsoTerminalDesigner/)
  to inspect the StarFire and tractor screens directly.

  As predicted going in, no tramline/XTE traffic (Triton was not on the bus
  to request it). PGN `0xAD00` (Guidance System Command) is still absent.
  `0xAC00` (guidance/curvature, from `0xF0`) is present throughout at 9.9 Hz
  as before.

  **Re-analysed 2026-09-22** (full write-up: "What the GPS module actually
  says on the bus" in the Documentation repo's
  `ISOBUS/research/john-deere-bus-inventory-2026-09-08.md`, tool
  `ISOBUS/tools/gps_traffic_map.py`). Three corrections to the above:
  - **"Autosteer engaged for the whole segment" is not what the bus says.**
    `0xAC00` Steering System Readiness is 1 only from log t = 296.9 to
    339.9 s (the edge at 296.9 coincides with putting the tractor in gear,
    the one at 339.9 with NAV `0x2A`'s `0xFFFF` sub `0x77` byte 1 flipping
    `0x14 -> 0x15`). Only 325-340 s drives dead straight (0.1 1/km); the
    rest of the run wobbles at +-100..350 1/km. Whether that 15 s was the
    engaged stretch or the engage happened at 339.6 s cannot be settled from
    this log. Next JD run: note engage/disengage by wall clock.
  - **The steering command is not on this bus at all**, so `0xAD00`'s absence
    says nothing about JD's use of it: nothing reaches `0xF0` at a
    steering-loop rate, NAV's only drive-time output is a 5 Hz status word
    with zero correlation to the tractor's curvature, and only CANedge
    channel 1 was connected. The loop runs on the vehicle bus; put channel 2
    there next time.
  - **The GPS module has no TC relationship whatsoever** -- zero `0xCB00`
    to or from `0x1C`; it is a broadcast nav source (5 Hz ISO trio + ~36 Hz
    of proprietary 65535) and a VT client, nothing else. None of its 65535
    sub-IDs change rate or state at either readiness edge; every state change
    in them is keyed to motion starting at t = 298-302 s.
  - Bonus: the StarFire's 126992 System Time carries real UTC (2026-09-11
    07:06 UTC at t = 220 s), so any log with it on the bus can be dated from
    its contents despite the CANedge's dead RTC.

#### Are the harvested pools correct, and where is the Working Set?

Opening the `.iop` files in a pool editor suggests they have **no Working Set
object**. They do. It is **last**, not first.

**The bytes are correct**, established four independent ways:

1. `Get Memory` (VT function 0xC0) is the ECU telling the VT how many bytes
   the pool is. The StarFire asked for **327 501** and we reassembled
   **327 501**; the tractor ECU's second pool asked for **1 260** and we
   reassembled **1 260**. Exact, on both.
2. The StarFire uploaded the same pool **twice** and the two independent
   reassemblies are **byte-identical** (`cmp`).
3. The VT answered every `End of Object Pool` with **error bitmask 0x00** --
   no error bits, so the terminal was satisfied with what it received.
4. `tools/validate_iop.py` walks **all three** pools object by object and
   lands **exactly** on the final byte -- including the StarFire's
   **9 619 objects across all 327 501 bytes**. A pool stream has no
   delimiters, so a single missing or duplicated byte desynchronises the walk
   within a couple of objects; 9 619 consecutive objects landing on the last
   byte is not something corrupt data does.

**Where the Working Set is:**

| Pool | WorkingSet at | of | position |
|---|---|---|---|
| Tractor ECU, 21 643 B | 0x5418 (21 528) | 21 643 B | **99.5% through** |
| StarFire, 327 501 B | 0x4FA5A (326 234) | 327 501 B | **99.6% through** |
| ours (MW03), 538 B | 0x0 | 538 B | first object |

The StarFire's is unmistakable: id 256, 9 children, and a table of **39
language codes** (`en fr de nl es da it ar bg cs el ...`). Exactly one, as
ISO 11783-6 requires.

**This is not obfuscation**, and the complete walk settles it: all **9 619**
objects are standard ISO 11783-6 types, and **not one** is a
ManufacturerDefined type (240-254) or anything outside the standard's range.
What the StarFire ships is an ordinary, if large, terminal UI:

| | | | |
|---|---|---|---|
| OutputLine 2 639 | OutputString 1 946 | StringVariable 1 444 | NumberVariable 787 |
| Container 677 | OutputNumber 572 | Button 345 | OutputLinearBarGraph 293 |
| OutputRectangle 183 | PictureGraphic 108 | ObjectPointer 102 | AlarmMask 99 |
| Macro 82 | DataMask 56 | OutputEllipse 56 | FontAttributes 45 |
| InputBoolean 38 | InputList 30 | FillAttributes 27 | LineAttributes 25 |
| InputString 23 | InputNumber 20 | Key 11 | InputAttributes 4 |
| OutputArchedBarGraph 3 | SoftKeyMask 2 | OutputPolygon 1 | **WorkingSet 1** |

ISO 11783-6 4.6.5 requires "one, and
only one, working set object" and says **nothing about where in the stream it
goes**; every ordering rule in the standard is about child-reference order
*within* a parent, for rendering. It cannot require definition-before-use
either, since masks reference children defined later -- the VT resolves the
whole pool only after `End of Object Pool`. So "Working Set first" is a
convention our own encoder follows and John Deere does not.

The practical consequence: **any tool that gives up parsing partway will
report there is none**, because it never reaches the last object.
`validate_iop.py --find-workingset` finds it by signature instead of by
walking, which is what to use on these.

**Why AgIsoTerminalDesigner shows no Working Set** (investigated 2026-09-11
by running its actual parser, AgIsoStack-rs, over the pool): not the
ordering, and not the object-structure gaps our own walker had. Its reader
rejects any object whose Line Attributes, Font Attributes or Input Boolean
Foreground Colour reference is NULL (`0xFFFF`) -- which ISO 11783-6 allows,
AgIsoStack++ accepts, and John Deere uses freely (213 shapes with NULL line
attributes, all 38 Input Booleans with NULL foreground colour). It stops at
the first one, object 1 640 of 9 619, and `from_iop` keeps what it had
without saying so: 1 638 objects, no Working Set. Filed and fixed upstream:
[AgIsoStack-rs#47](https://github.com/Open-Agriculture/AgIsoStack-rs/issues/47)
/ [PR #48](https://github.com/Open-Agriculture/AgIsoStack-rs/pull/48)
(reader accepts NULL; adds a strict `try_from_iop`), with the companion
[AgIsoTerminalDesigner#35](https://github.com/Open-Agriculture/AgIsoTerminalDesigner/issues/35)
/ [PR #36](https://github.com/Open-Agriculture/AgIsoTerminalDesigner/pull/36).
With both, the StarFire pool loads completely in the Designer. Until they
land, the Designer cannot open these files usefully -- so a **patched build**
is kept on the analysis machine, outside the repos:
`C:\Users\arjan\OneDrive\MeijWorks Projects\Tools\AgIsoTerminalDesigner-patched\`
(README alongside it). Verified 2026-09-11: File > Import IOP on the StarFire
pool renders the receiver's main screen ("StarFire 7000 - Hoofd", position
mode, heading/altitude/speed fields, roll/pitch) with `256: Working Set` at
the top of the object tree.

Getting that complete walk needed exactly **two** fixes to our object table,
both VT-version-4 additions that our own pool never exercised because it uses
neither object type:

- **`InputList` is 13 bytes fixed, not 12** -- an `Options` byte sits
  *between* the list-item count and the macro count, so the counts are at
  +10 and +12, not +10 and +11.
- **`InputNumber` is 38 bytes fixed, not 37** -- a second `Options` byte.

Both were confirmed the same way, and it is the method worth reusing: when
the walk stops, try small length deltas on the *preceding* object and accept
the one where the **next object's child references resolve to object IDs the
walk has already seen**. A legal type byte alone proves nothing (1 byte in 5
is a valid type); references resolving to real, already-parsed objects is
near-impossible by chance. Fitting deltas on "type byte looks legal" alone
overfits and oscillates -- it was tried and does not converge.

### Session 11, 2026-09-25

**Triton IS on the bus** (`0x81`), running the session 11 brief. John Deere,
van Os, same rig as sessions 8-10. **No plough attached**, and a big trailer
behind, so the tractor went **forward and in reverse** on one short track with
autosteer engaged, never turning round -- unlike session 10, which turned round.
The minute-by-minute record and serial logs are in
[`../logs/`](../logs/) (`2026-09-25_session11_*`).

- **log 30** -- 2729 s, 1 143 997 frames, the longest capture so far. Two
  firmware runs: the board's arrival firmware until 12:22:58, then PR #44
  (`fix/21-tc-counters` @ `2da131e`) from 12:23:12 after flashing at the rig.

**Dated from its own contents, not the card.** The StarFire (`0x1C`) sends PGN
126992 System Time with real UTC, so this log does not depend on the hand
record: **CEST = log time + 11:54:20.2**, consistent to under a second across
the whole log. Skip the *first* 126992 frame (t = 9.9 s) -- it reads
1980-01-06, before the receiver has time. The log ends 12:39:50 CEST; the
called stop (12:40:07 on the field laptop) came about 15 s later, and the
laptop clock runs about 2 s ahead of StarFire UTC. Three sharp markers agree
with the mapping:

| Marker | Log time | CEST |
|---|---|---|
| our traffic silent (flashing PR #44) | 1717.5-1731.4 s | 12:22:58-12:23:12 |
| our second address claim | 1731.4 s | 12:23:12 |
| our traffic silent (#18 unplug/replug) | 1996.3-2019.4 s | 12:27:36-12:28:00 |

**What it is good for:**

- **#21** -- the TC's Process Data to us at all four connects: DDI 506 = 1
  every time, **DDIs 507-511, 513 and 514 never**, including the whole
  autosteer-engaged stretch (12:33:32-12:36:12). Also confirms #44's
  `[on bus]` counters against the wire, frame for frame.
- **Travel direction** -- PGN 65096 from `0xF0`, byte 8 bits 1-2 (0 = reverse,
  1 = forward), with both directions well represented. Course ~138 deg was
  forward, ~318 deg reverse. This is what settles the XTE sign (#151).
- **XTE ground truth** -- five operator call-outs with both signs and both
  travel directions, all reproduced by the legacy `0x2A` decoder within 1.5 cm.
  **The sign is in the direction-of-travel frame: positive = right of the
  line**, so in the tractor's frame it flips in reverse. While stationary it
  follows the last travel direction.
- **The 12:25:38 VT/TC drop (#149)** -- the bus stays healthy throughout; we
  alone go silent for 3.19 s (12:25:35.41-12:25:38.60), then flush a 3 ms
  backlog that the VT NACKs.
- **#18 recovery** -- the unplug/replug with power kept.
- **Negatives worth keeping:** `0xAD00` Guidance System Command -- zero frames
  in 45 min with autosteer demonstrably steering, so on this rig it is simply
  never broadcast. PGN 44032 `0xAC00` steering readiness reads 0 for the whole
  engaged stretch (1 only in the first 7.6 s of the log), so **there is no
  bus-visible "autosteer engaged" signal on this implement bus** -- record it
  by hand. No NMEA2000 guidance PGNs at all (129025/26/27/29/283).

**One trap:** the board's `PGN 65535 XTE JD legacy` dump line covered *every*
65535 sender (~104 frames/s), not the XTE carrier, and its all-zero payload
was a placeholder, not data -- the decoder only captures bytes from `0x2A` and
`0x80`, and no sender in this log ever sends an all-zero 65535 payload. Fixed
in #153. Read the XTE from `0x2A`, sub-ID `0x77`.

### Session 13 repeat, 2026-10-01

**Triton IS on the bus** (`0x81`), running `test/session12` (code = `48a7ae4`, fork `9aa491e`),
NH + Ag Leader, van Mastwijk. Same rig as session 9. Brief: `SESSION_13_TEST_BRIEF.md`;
timeline and serial log in [`../logs/`](../logs/) (`2026-10-01_session13_*`).

- **log 31**: 15:09:18.7 to 16:00:28 CEST, 3070 s. **log 32**: the CANedge restarted (17 s
  gap, a new card session), 16:00:46 to 16:03:01.8.
- **Clock:** the anchor is Triton's own address claim after its reboot (log +2820.169 s =
  serial 15:56:18.919). The cross-check is the CNH VT's last VT Status (0x26, PGN 0xE600)
  before each serial `VT Status Timeout`: 2.99 s and 3.01 s earlier, against AgIsoStack's
  3 s timeout. So the mapping is good to ~10 ms. The operator's "CANedge on" (~15:09:23)
  agrees within 5 s.
- **ch2 recorded nothing.** It was on the steering controller's CAN 2 from ~15:11:34,
  Monitoring / auto-detect. Channels 1 and 9 only. Either CAN 2 carries no traffic, or it
  carries frames nobody ACKs, or the tap was on the wrong pins. The logger's syslog says
  nothing about it.
- No StarFire on this rig: no PGN 126992, so log time maps to wall time only through the
  anchor above.

### Raven terminal, 2026-10-02 -- card log 33 is the tractor's vehicle bus, NOT archived here

Unplanned visit to a Raven terminal on a CNH tractor (rig/owner: see the timeline). The plough control
ran `test/rig-2026-10-02` (8c0cc7b); serial log and timeline in [`../logs/`](../logs/)
(`2026-10-02_raven_*`). The CANedge logged **card session 33** (1740 s, 2 197 512 frames) for the
whole visit, but on the **J1939 vehicle bus**: engine 0x00, retarder 0x0F, transmission 0x03, hitches
0x23/0x2E, PTOs, CNH ECUs. No VT, no GNSS, no 0x81, no 129283. The ISOBUS side of the session exists
only in the serial log. The file stays in the OneDrive CANedge archive
(`card-AD4F266A/session-00000033`), not in this folder, so nobody mistakes it for the Raven session
later (session 23 was lost the same way in September).

- **Datable despite the bogus RTC:** CNH SA 0x28 sends J1939 Time/Date (PGN 65254) at 1 Hz with the
  local offset: log start 14:45:15.7 CEST, end 15:14:15; agrees with the laptop clock to ~1 s.
- Useful for cross-checks only: the tractor's power cycle (engine off ~14:52:10, bus silent
  14:54:45-14:56:15), ground speed (moving 15:03:00-15:06:10 and 15:11:15-15:13:00, standing still
  at both Raven XTE call-outs), rear hitch at 81 % throughout.
- **Rule, again:** run `mf4_to_pcap.py --inventory` on a card session before trusting it; a bus
  without SA 0x26/0xF7/0x81 and PGN 0xE600 is not the implement bus.

### Lemken plough control + L160 lightbar, 2026-10-08 -- card logs 34-42, the IBBC is a separate bus

Rig: the van Mastwijk CNH with the InCommand 1200 (new harness). A Lemken ISOBUS plough control
was on the IBBC for a demo, with an Ag Leader L160 lightbar. No Triton on the bus, no serial log.
Timeline: `../logs/2026-10-08_lemken_l160_timeline.md`.

- **Logs 34-38: empty** (channel 9 only). The CANedge lead was on the tractor's in-cab 9-pin
  connector, first with a loose power pin, then with the bus simply absent.
- **Log 39, 40, 41: channel 1 on the in-cab connector pair 2/4** shows only the tractor's native
  ISOBUS: TECU 0xF0 (class broadcasts at 10 Hz), CNH VT 0x26, and in log 40 the CNH's 0xAC/0xCD.
  No InCommand, no implement, although both were live. Pins 2/3 and 4/5 are bridged on that
  connector, so there is no second pair to try: **on the new harness the in-cab connector does not
  reach the IBBC.** Not archived here (OneDrive `card-AD4F266A/session-000000{39,40,41}`).
- **Log 42: channel 2 back-probed on the IBBC (pins 8/9), channel 1 still in the cab.** 347 s,
  155 482 frames. Channel 2 carries the InCommand's five Ag Leader CFs (VT 0x26, TC 0xF7, display
  0xF5, 0x2B, 0xE9; all manufacturer 97, identity 28337, the same unit as session 13), the Lemken
  box at **0xEE** (manufacturer 2047 = unassigned, class 6, "ISOBUS Plough" sw 3,14, Dutch UI,
  "Lemken UT") and the lightbar at **0xDC** (Ag Leader, function 131, software ID
  "ALTECH,AL L160;01.00.00.00;L160_UP_FW;01.05.00.00"). Channel 1 at the same time: TECU + CNH VT
  only. So the InCommand's ISOBUS branch and the IBBC form one segment, and the tractor's own
  ISOBUS is another; the InCommand's VT sits at 0x26 on its segment just like the CNH VT on the
  other. **Every future capture of implement traffic on this rig goes on the IBBC side.**
- **Pools harvested** (`2026-10-08_log42_lemken_iops/`): the Lemken VT pool, 21 253 bytes, from the
  second of two uploads (the first was cut by a power cycle), walks clean to the last byte, 1006
  objects, VT error 0; and its DDOP, 841 bytes, one device element with sections, DDIs 1/2 rate,
  67 width, 72/73 tank, 134/135 offsets, 141 status, 160/161 section control, 226 length. Nothing
  guidance-related in the DDOP.
- **The L160 receives nothing periodic.** Only an ISO 15765 identification exchange with 0xF5 at
  power-up and two Proprietary A frames. It reads a broadcast, and `xte_hunt.py` (positions from
  0x26's 65267, scan channel 2, window 185-300 s) ranks **PGN 65462 from 0xF5** first by a wide
  margin: bytes 0-1 = XTE magnitude in cm (149 at the start, 0-5 on the line, operator: "large,
  then 0-2 cm after engaging"), byte 2 = 1/2 autosteer off/on, byte 4 = signed around 127 (heading
  error?), byte 6 bit 7 toggles at zero crossing (side candidate). Sign not settled: no deliberate
  offsets in this drive. Details in the Documentation repo,
  `ISOBUS/research/agleader-incommand-65462-xte-2026-10-08.md`. **But it is conditional:** in logs 28 and 31 (sessions 9 and 13, old harness) the same 0xF5
  was on our bus and never sent 65461/65462; in log 42 both start within a second of the end of the
  L160's power-up identification exchange. So the InCommand only broadcasts them once a lightbar
  has identified itself (or once a display setting for one is on -- untested). #42: the XTE is on
  the ISOBUS as a global proprietary-B broadcast at 5 Hz, but something has to switch it on.

## Reading them

`mf4_to_pcap.py` -- now in the Documentation repo at
`NeptuneGPS Documentation/ISOBUS/tools/` -- converts to a SocketCAN pcap for
Wireshark's ISO 11783 dissectors. `--summary` prints address claims (with
decoded NAMEs), transport-protocol sessions and a PGN histogram; `--inventory`
prints who is on the bus, what each control function sends, and at what rate:

```
python mf4_to_pcap.py <LOG.MF4> -o out.pcap --summary
python mf4_to_pcap.py <LOG.MF4> --inventory            # no pcap needed
```

To decode in Wireshark the ISOBUS dissector must be selected **explicitly** --
there is no CAN heuristic for it on Wireshark 4.6.8, so frames stay plain
"CAN". Note the **single** `=`:

```
tshark -r out.pcap -d can.subdissector=isobus -O isobus
```

These two captures are analysed in
`NeptuneGPS Documentation/ISOBUS/research/john-deere-bus-inventory-2026-09-08.md`.

It reads unfinalized MF4s straight off the card -- no `mdf2finalized` step.
Needs `mdf_iter` (or `asammdf`), **plus `numpy` and `pandas`** -- without the
latter two `mdf_iter` returns `None` from its dataframe export and the
converter dies with `AttributeError: 'NoneType' object has no attribute
'empty'`, which does not name the missing dependency.

**The interpreter is machine-specific -- do not trust a hard-coded path here.**
On the analysis machine it is the Python 3.13 at
`C:\Users\arjan\AppData\Local\Programs\Python\Python313\python.exe`, which is
not on `PATH` in Git Bash. That path does **not** exist on the field laptop,
which has no system Python at all; there, the interpreter that works is
PlatformIO's own (`~/.platformio/penv/Scripts/python.exe`, 3.11.7) with a
throwaway venv:

```
~/.platformio/penv/Scripts/python.exe -m venv /tmp/mf4venv
/tmp/mf4venv/Scripts/python.exe -m pip install mdf_iter numpy pandas
```

Verified working on the field laptop against log 25 on 2026-09-09.

**Gotcha when scripting against it:** `decode_pgn()` returns
`(priority, pgn, destination, source)` -- destination before source. Unpacking
it the other way round silently reports every sender as `0xFF`.

---

# Session 9 (2026-09-09) -- analysis brief

**The files are not here yet.** They were still on the card when this was
written; the card was being mounted on the analysis machine. Add them with the
same naming convention, and fill in the provenance table above -- date, rig,
owner, duration, frames -- because none of it is recoverable from the file.

Provenance known so far: 2026-09-09, two rigs in one session, **John Deere
first, then the Ag Leader InCommand 1200** on a different tractor. At least one
capture spans the Ag Leader VT object pool upload (see Q4).

## What these logs need to answer

Four questions, in priority order. Full session context is in
[`../HardwareTestNotes.md`](../HardwareTestNotes.md) session 9.

### Q1 (highest value) -- did the John Deere TC really send us measurement commands?

Session 9 inferred, from a `Value requests` counter reading 6 750 986 (~20 000/s,
impossible as bus traffic), that the John Deere TC configured measurement
reporting on our DPDs. If true it is the first time in this whole effort that a
Task Controller has actively engaged with our DDOP rather than merely accepting
it.

**The inference is not independent.** It rests on AgIsoStack's source plus our
own counter, both downstream of the same stack -- structurally identical to
session 5's `vtstat` mistake, which looked like corroboration and was not. This
capture is the independent check, so treat Q1 as open until the bus says so.

Look for **Process Data, PGN `0x00CB00`**, sent **from the TC to us**:

- our address is `0x81` (destination), TC source is `0xF7`, and note session 8
  found a **second** TC at `0xFB` -- check both
- the command is the **low nibble of byte 0**; the DDI is bytes 1-2

The four commands that would prove it, from AgIsoStack's `ProcessDataCommands`:

| Nibble | Command |
|---|---|
| `0x04` | Measurement Time Interval |
| `0x06` | Measurement Minimum Within Threshold |
| `0x07` | Measurement Maximum Within Threshold |
| `0x08` | Measurement Change Threshold |

(`0x05`, distance interval, exists but AgIsoStack does not keep a polled list
for it, so it cannot be what drove the counter.)

Also worth counting while there: `0x02` RequestValue, `0x03` Value, and `0x0A`
SetValueAndAcknowledge. **`0x0A` matters** -- with TC version 4+ on both ends,
values move to the acknowledged PGN, and our `Value commands` counter watches
the ordinary path, so a TC using `0x0A` would read as zero on our side.

### Q2 -- which DDIs does the TC actually reference, and is 506 ever among them?

Session 9 declared the full Level 1 set (505, 506, 507, 508, 509, 510, 511,
515) under structure label TC05 and both brands accepted the pool -- no repeat
of session 8's `no compatible implements detected`. Yet `DDI 506` never arrived
and 507-511 stayed empty on both.

Extract the DDI field (bytes 1-2) of every Process Data message addressed to
`0x81` and list which DDIs appear, from which TC. Three outcomes, all useful:

- **506 present** -> the handshake does complete and our client is mishandling
  the reply; a firmware bug, not a terminal limitation.
- **only 513/514 and our own DPDs** -> the TC reads us but has no tramline
  intent, and #21's remaining question moves to the terminal's configuration.
- **nothing addressed to `0x81` at all** -> Q1 is disproved and the counter has
  another explanation, which would need finding before anything else is
  believed.

### Q3 -- where is the Ag Leader's cross-track error? (issue #42)

Confirmed again in session 9: PGN 65535 from `0x80` streams continuously with
`data[0] = 0x51` (payloads `510302FF10060DFF`, `510301FF0B0009FF` and
neighbours) and is **not** the XTE message -- #30's selector check correctly
refuses it. Position, speed, course and altitude all decode; only cross-track
is missing on this brand.

Operator ground truth, called out live during the Ag Leader run and **unpaired
on our side** because we never decode a value:

| Order | Called |
|---|---|
| 1 | 26 cm |
| 2 | 5 cm, **other side** |
| 3 | 53-54 cm |

The sign change between 1 and 2 is the lever: find a field anywhere in the
capture that is large, crosses zero, and returns large, in that order. Do not
restrict the search to PGN 65535 -- the point of #42 is that the Ag Leader
carries it somewhere else. `--inventory` first, to see who talks and on what.

Beware the trap this project has already fallen into twice: a single value
matching to within a per cent is not evidence (session 6), and a second
measurement sharing a dispatch path with the first is not independent
(session 5). Require all three points *and* the crossing.

### Q4 -- the VT object pool upload is in here, and is worth keeping

Session 9 found the VT pool label had been stale since 2026-08-10 (`MW03` reused
across two pool changes, so terminals served a weeks-old screen). It was bumped
to `MW04` and reflashed **on the rig**, so one of these captures contains a
complete VT object pool transfer: the terminal deleting its `MW03` copy and our
full upload of `MW04`.

That is a clean reference recording of an object pool upload on a real bus,
independent of #21 and #42, and pairs with the existing
`isobus-jd-gps-objectpool-capture-2026-09-07.md` in the Documentation repo.
Worth writing up as its own reference rather than being consumed and forgotten.
