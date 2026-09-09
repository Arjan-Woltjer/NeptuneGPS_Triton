# Object pool snapshots

Binary snapshots of what this firmware actually *builds and sends*, pulled off
a running board with the debug menu's `4. Dump DDOP as hex` and
`5. Dump VT object pool as hex`, then converted from hex to `.iop`.

One dated folder per pull. These sit with the project, alongside
[`../logs/`](../logs/) and [`../canlogs/`](../canlogs/), because they are
artifacts of this implement's firmware -- unlike the general ISOBUS tooling and
research, which moved to `NeptuneGPS Documentation/ISOBUS/` on 2026-09-08.

## Why keep them

A DDOP is easy to reason about wrongly from the source alone: designators get
truncated, objects get declared but never referenced by a parent, and structure
labels drift from what the code comment claims. A pulled pool is the ground
truth for all three -- it is the bytes the terminal received.

They also make terminal-side behaviour reproducible after the fact. When a
terminal rejects a pool or ignores a DDI, the question "what exactly did we
send that day" is otherwise unanswerable once the board has been reflashed.

## Reading them

Open `.iop` files in AgIsoDDOPGenerator (DDOP) or AgIsoTerminalDesigner (VT
pool). The `*_serial_dump.txt` files are the raw menu output the `.iop` was
made from, kept so the conversion can be redone or checked.

To rebuild an `.iop` from a dump by hand, strip the `BEGIN`/`END` marker lines
and:

```
python -c "import sys;open('OUT.iop','wb').write(bytes.fromhex(''.join(sys.stdin.read().split())))" < hex.txt
```

Note Python is not on `PATH` in Git Bash on this machine; see
`../canlogs/README.md` for the interpreter path that works.

## Snapshots

| Date | Firmware | DDOP | VT pool | Notes |
|---|---|---|---|---|
| [`2026-09-09/`](2026-09-09/) | `feat/21-full-tramline-ddop` @ `b6474f7` | 541 B, structure label **TC05** | 538 B, label `MW03` | Pulled on the bench before session 9. First pool carrying the full Tramline Control Level 1 DDI set (505, 506, 507, 508, 509, 510, 511, 515) for #21, and the first with the DDI 508 designator shortened to fit the 32-character limit. |
