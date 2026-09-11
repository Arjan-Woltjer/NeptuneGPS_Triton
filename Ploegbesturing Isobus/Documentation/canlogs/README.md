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
uploads) followed by driving a line, 15 m forward/15 m back, **autosteer
engaged**.

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
  to request it). PGN `0xAD00` (Guidance System Command) is still absent
  despite autosteer being engaged this run -- refines the
  john-deere-bus-inventory note's earlier "absent, consistent with autosteer
  disengaged" reading: on this rig 0xAD00 appears to simply never be
  broadcast, regardless of autosteer state. `0xAC00` (guidance/curvature, from
  `0xF0`) is present throughout at 9.9 Hz as before.

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
