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

Triton itself is **not** on the bus in either file: the plough control had
already been disconnected from the ISOBUS by this point in the session (see
Session 8's `#18` notes). No control function claims with our manufacturer
code 1407, and neither log can be paired against a Triton serial reading. What
they do carry is the vendors' own guidance traffic, which is what issues #20
and #21 needed.

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
Needs `mdf_iter` (or `asammdf`); on this machine that is the Python 3.13 at
`C:\Users\arjan\AppData\Local\Programs\Python\Python313\python.exe`, which is
not on `PATH` in Git Bash.

**Gotcha when scripting against it:** `decode_pgn()` returns
`(priority, pgn, destination, source)` -- destination before source. Unpacking
it the other way round silently reports every sender as `0xFF`.
