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
