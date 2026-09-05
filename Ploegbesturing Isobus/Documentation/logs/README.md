# Hardware test logs

Serial captures from real-bus test sessions, referenced by
[`../HardwareTestNotes.md`](../HardwareTestNotes.md). Two tiers:

**Annotated excerpts** (`*_runN_*.log`) -- trimmed to the moments that
matter, with inline commentary explaining what each block shows and why it
was significant. Start here; they are meant to be readable on their own.

**Raw captures** (`*_rawN_*.log`) -- the complete, untouched serial output.
Kept only for sessions where the raw data materially drove a conclusion, and
where re-deriving it later would be impossible without the rig.

Session 6 is the reason that distinction exists. Its XTE analysis
initially reached a confident but *wrong* conclusion (a 100x scale error)
from a single ground-truth data point. What overturned it was counting how
many distinct values the decoded field took across ~400 consecutive samples
-- something only the raw capture can answer. Curated excerpts would have
preserved the wrong conclusion and thrown away the evidence against it.

## Format

Each line is `[<seconds since capture start>] <serial output>`. The
timestamp is relative to when the logging script attached to the port, *not*
to board reset -- the board is often already running, and reboots mid-capture
appear as a `--- disconnected ---` / `--- connected ---` pair followed by the
MeijWorks banner. The capture script reconnects automatically across resets,
so one file can span several board sessions; watch for the banner.

The recurring `[ISOBUS]` line is `IsobusDebugMenu`'s periodic summary. Fields
worth knowing:

| field | meaning |
|---|---|
| `vt=Y/N(n/22)` | VT connected, and step through the 22-state handshake |
| `vtstat=<count>/<age>ms` | VT status broadcasts received, and age of the last one |
| `tc=Y/N` | Task Controller connected |
| `drp=` / `tcq=` | DDI 513 (DRP deviation) / DDI 514 (GNSS quality) |
| `msgs=` | total guidance PGNs received -- a liveness check on the CAN path |
| `aiso=` | All-Implement-Stop (PGN 64770) count |
| `xte=` | decoded cross-track error (**known wrong** -- see Session 6 Phase 5) |

`[NM]` lines are AgIsoStack's control-function lifecycle events. Their
*argument values print as garbage* (`name 000000000000000lx`, absurd
addresses) due to a format-string mismatch in AgIsoStack's logging on this
toolchain -- treat them as event markers, not as data.
