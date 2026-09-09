# 2026-09-09 -- pre-session-9 bench pull

Pulled on the bench, off-bus, immediately before the session 9 rig test.

**Firmware:** `feat/21-full-tramline-ddop` @ `b6474f7`, built and flashed from
this working copy. Both AgIsoStack vendor patches (#3 and #4) verified present
in `.pio/libdeps/teensy41_isobus` before the build; 104/104 native tests pass.

## `DDOP.iop` -- 541 bytes, structure label `TC05`

The first pool declaring the **full Tramline Control Level 1 DDI set** for #21,
after session 8's John Deere terminal answered `no compatible implements
detected` to a pool carrying only 505 and 506.

Verified present in these bytes, all in the Ploughbody function element:

| DDI | Object | Designator | Kind |
|---|---|---|---|
| 505 | 8 | Tramline Control Level | DPT (property), value `1` |
| 506 | 9 | Setpoint Tramline Control Level | DPD, settable |
| 507 | 10 | Tramline Sequence Number | DPD, settable |
| 515 | 15 | Tramline Control State | DPD, settable |
| 508 | 11 | Unique A-B Guidance Ref Line ID | DPD, settable |
| 509 | 12 | Actual Track Number | DPD, settable |
| 510 | 13 | Track Number to the Right | DPD, settable |
| 511 | 14 | Track Number to the Left | DPD, settable |

Also carries `Guidance Deviation` (513) and `GNSS Quality` (514) from the
original pool, plus the `Offset X`/`Offset Y` hitch properties.

**The DDI 508 designator is the shortened one.** It read `Unique A-B Guidance
Reference Line ID` (37 characters) until this build; AgIsoStack warned on every
boot that it exceeded the 32-character limit, and it is now `Unique A-B
Guidance Ref Line ID` (31). That matters for session 9 specifically: an
over-long designator is a pool a terminal may legitimately reject, and a
rejected pool would have been indistinguishable from the tramline handshake
failing -- the one question the session exists to answer. The warning no longer
appears on boot.

## `VTPOOL.iop` -- 538 bytes, label `MW03`

Unchanged this session; captured as a baseline alongside the DDOP. Contains the
Dutch screen labels and the three soft keys field-verified in session 7:
`POSITIE`, `SETPUNT`, `XTE (m)`, `AFWIJKING`, `BREDER`, `SMALLER`, `AUTO`,
`CALIBR`.

Note `CALIBR` is present in the pool but should not be pressed on a rig --
see issue #31, where that key starts a wizard the VT cannot drive and stalls
the CAN stack while it runs.

## Provenance caveat

Pulled **off-bus**: the board had claimed address `0x81` but no terminal was
connected, so this is the pool as *built*, not as any terminal accepted it. It
is the input to the session-9 test, not evidence about its outcome.


---

# `from-bus/` -- the same pools recovered from the CANedge capture

Reconstructed from `../../canlogs/2026-09-09_session9_agleader-vanmastwijk_log28_main.MF4`
by reassembling the ISO 11783 Transport Protocol sessions sent by `0x81`. This
was a test of whether a pool can be recovered from a passive bus capture at
all -- the answer is yes, and it now has a ground truth to prove it, because
the board's own dumps of the same pools sit beside it.

## The VT object pool: byte-for-byte identical

`VTPOOL_from_bus.iop` -- 538 bytes, md5 `cdf7555b56b010a6aa4118edfd5cf710`,
**identical to `../VTPOOL.iop`**.

Recovered from the ECU-to-VT transfer at t = 352.7 s, `0x81 -> 0x26`, PGN
`0xE700`, first byte `0x11` (Object Pool Transfer). Note the destination: this
is the **MW04 upload landing on the CNH terminal**, the reflash that fixed the
stale-label problem described in HardwareTestNotes session 9. The VT that
received our pool that time was CNH's `0x26`, not Ag Leader's `0x80`.

## The DDOP: identical apart from one byte, and the byte is explained

`DDOP_from_bus.iop` -- **540** bytes, against 541 in `../DDOP.iop`. Sent three
times (t = 161.4, 359.2, 561.0 s) to the Task Controller at `0xF7`, all three
transfers byte-identical, on PGN `0xCB00` behind a leading `0x61` Process Data
command byte which is stripped here.

The difference is a single `0x00` at offset 62 of the board's dump, immediately
after the 7-byte localization label, with everything downstream shifted by one.
That byte is the **extended structure label length**, which ISO 11783-10 defines
as **version 4 and later only**. Removing it from the board's dump reproduces
the on-wire bytes exactly:

```
dump[:62] + dump[63:] == wire     ->  True
```

And the reason is in the session's own serial log:

```
I] [TC]: DDOP will be generated using the server's version instead of the
         specified version. New version: 3
```

The Ag Leader Task Controller is **version 3**, so AgIsoStack regenerated the
DDOP at version 3 before uploading and dropped the version-4-only field.

## The caveat this exposes, which matters more than the reconstruction

**The `Dump DDOP` debug command shows the pool as authored, not necessarily the
pool that goes on the wire.** When a Task Controller negotiates a lower version,
AgIsoStack rebuilds the DDOP to match it, and the uploaded bytes differ from
what the dump gives you.

So inspecting a dump in AgIsoDDOPGenerator before a rig session is still worth
doing -- it catches malformed pools cheaply -- but it validates the *authored*
pool. If a terminal rejects a pool that looked fine in the generator, the
version-downgraded form is the thing to reconstruct from a capture and check,
and this directory is the worked example of how.

## How to redo it

`tools/` in the Documentation repo has the converter; the reassembly is a short
script over `read_frames()` + `decode_pgn()`: watch PGN `0xEC00` for an RTS
(byte 0 = 16) from the source of interest, collect PGN `0xEB00` data frames by
their sequence byte, concatenate payload bytes 1-7 in sequence order and
truncate to the RTS size. An ECU-to-VT object pool is the transfer whose
enclosed PGN is `0xE700` and whose first payload byte is `0x11`; the DDOP is
the one on `0xCB00`.
