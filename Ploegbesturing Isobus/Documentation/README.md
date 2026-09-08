# Ploegbesturing Isobus documentation

## What is here

- **`HardwareTestNotes.md`** -- the per-session record of this firmware's
  behaviour on real rigs, session by session. Stays with the project on
  purpose: it is tied to this repo's issue numbers and source files.
- **`logs/`** -- serial captures from those sessions.
- **`canlogs/`** -- raw CANedge whole-bus MF4 captures, with the provenance
  (date, rig, owner) that the logger's own clock cannot supply.

## What moved, and where it went

On 2026-09-08 the general ISOBUS material moved out of this repo to the
**`NeptuneGPS Documentation` repo, `ISOBUS/` folder**, so that knowledge about
the protocol is not owned by one implement's firmware project. Start at
`NeptuneGPS Documentation/ISOBUS/README.md`.

| Was here | Now |
|---|---|
| `Documentation/TramlineControl_TC_Support_Research.md` | `ISOBUS/research/` |
| `Documentation/ISOBUS_TC_Manufacturer_Comparison.md` | `ISOBUS/research/` |
| `Documentation/EndOfObjectPool_ErrorBitmask_Research.md` | `ISOBUS/research/` |
| `Documentation/AgIsoStackVendorPatches.md` | `ISOBUS/research/` |
| `Documentation/ISO 11783-6-2004.pdf` | `ISOBUS/` (was a byte-identical duplicate of the copy already there) |
| `../../docs/isobus-jd-gps-objectpool-capture-2026-09-07.md` | `ISOBUS/research/` |
| `../tools/mf4_to_pcap.py` | `ISOBUS/tools/` |
| `../tools/vt_pool/` | `ISOBUS/tools/vt_pool/` |

`AgIsoStackVendorPatches.md` is the one to know about: the patches it describes
are **load-bearing**, and a clean rebuild without reapplying them silently
reverts to the broken control-function eviction behaviour. It is now at
`NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md`.
