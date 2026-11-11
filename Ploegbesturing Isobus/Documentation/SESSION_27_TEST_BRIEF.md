# Session 27 test brief -- AgIsoStack upstream main 5ff90cb: the rejected-pool fail-safe, fast packet locks

Written 2026-11-11. Session 27 tests **one library step**: upstream main 02f3fe9 (session 26) to **5ff90cb**, PR #331.
`test/session27` is `test/session26` (f961b5a: #327 + #328) plus #331 and the v0.27 bump. **Run session 26 first and
only start this one after session 26's P and X pass**, so a finding here can only come from the library step.

| Part | Where | Decides |
|---|---|---|
| **S** | The bench: rig 4.1 + Saturn + the PC VT | Boots clean; MW18 comes back by Load Version, not re-uploaded; stack as session 26 |
| **P** | van Mastwijk (CNH VT + InCommand 1200, both on the bus), at least an hour | The VT client after #868: no new `Failed`, no surprise uploads, failover as before; PGN 129029 still decoded after #858 |

This session's notes go on **`test/session27`**, see section 7.

## 0. Before the session: one question for the owner

**Q1 -- the reconnect watchdog vs #868.** Upstream #868: after a VT **rejects** the pool (an error in its End of
Object Pool response), the client stays in `Failed` until that VT stops sending VT Status. Before, it re-uploaded every
5 s, and the VT showed its alarm again each time. ISO 11783-6 C.2.5 wants a fail-safe instead. Our reconnect watchdog
(`IsobusVtInterface::updateReconnectWatchdog`, #18) runs `terminate()` + `initialize()` every 10 s after a lost
connection, and so re-uploads anyway. It is disarmed at boot and after a failover switch, so this only happens when a
VT that once accepted MW18 rejects it later. Leave it (the current build), or make the watchdog skip `Failed`?
Session 27 can run either way: no step below needs a rejected pool.

## 1. What changed on the board since session 26

| Change | Upstream | What to see |
|---|---|---|
| **A rejected pool is not re-sent** until that VT goes away; a NACK no longer moves the client out of `Failed` | #868 | Nothing on a VT that accepts MW18. If a VT rejects it: **one** `E] [VT]: Error in end of object pool message` and the State stays `Failed`, with no upload every 5 s |
| **A Store Version timeout goes to `Connected`** instead of `Failed` (which used to re-upload a pool already in use) | #868 | A `W] [VT]: Store Version Response Timeout` line, after which the plough stays on that VT. Before, it was an `E]` line and a second upload |
| **Fast packet callbacks run outside the protocol's locks** | #858 | Nothing. This is our PGN 129029 receive path (GNSS position, the only source of a GNSS quality, so of `RTK quality`) |
| VT command scaling (opt-in, not used), Input Boolean scaling (no Input Boolean in the pool) | #861 #866 #872 | Nothing: the uploaded pool is byte-for-byte the same, **so the label stays MW18** and no VT uploads again |
| VT server NACKs; `thread_local` on ESP-IDF | #851 #855 | Nothing: not built for Teensy |
| Version | | Splash `Triton PB-ISO  v0.27`, banner `Triton PB-ISO version 0.27` |

Build: flash code +3.0 KB versus session 26; RAM1 unchanged, **free for local variables 71 488 B on the 4.1** (the same
9 ITCM blocks, 12.7 KB padding left).

## 2. Kit and builds

OneDrive `MeijWorks Projects/Firmware builds/session27/`:

| Hex | For | Contains |
|---|---|---|
| `teensy41_isobus_fa27718.hex` | the rig Teensy 4.1 (Loader 13249100) | `test/session27`, v0.27 |
| `teensy40_isobus_fa27718.hex` | a standard Teensy 4.0 plough board | the same |

**No customer board gets this build.**

**Fallback:** `Firmware builds/session26/teensy41_isobus_f434459.hex` (v0.26, MW18, upstream 02f3fe9). Same label, so
going back uploads nothing.

**Flashing:** `tools/flash_teensy.ps1 -Serial <Loader serial> -Hex <hex>`, with exactly one PJRC board on USB (unplug
the Saturn) and the serial logger stopped. Settings are kept: the EEPROM layout is session 25's.

**Every part:** serial logger open **before** the board boots, the 1 Hz line on (debug `2`), CANedge on the ISOBUS.

## 3. Part S -- the bench

| Step | Do | Expect |
|---|---|---|
| S1 | Flash, logger open, boot. | Banner `version 0.27`. **No `CrashReport:` block.** Settings as on session 26. |
| S2 | Debug `1` at once. | `Stack:` size 71 456 bytes on the 4.1, as session 26. Note the peak. |
| S3 | PC VT on the Saturn. | If the PC VT stored MW18 in session 26: **Load Version, no upload** (a few seconds to the main mask). If not: one MW18 upload, then a Store Version. Either way **no `Store Version Response Timeout`** and no second upload. Dump `1`: `State: ... Connected`. |
| S4 | Unplug the Saturn for 20 s, plug it back. | The plough reconnects by Load Version. Note the stack peak after it. |

## 4. Part P -- van Mastwijk, both VTs, at least an hour

The rig 4.1 on the session 27 hex, preferred VT and VT numbers as session 26 left them.

| Step | Do | Expect |
|---|---|---|
| P1 | Boot with the tractor. | Both VTs got MW18 in session 26, so **both load it by version: no upload on either**. The 1200's TC takes the DDOP (TC08, unchanged). Note the times. |
| P2 | Work or stand for **at least an hour** with both VTs on the bus. | The plough stays on its VT and TC throughout. **Afterwards, from the serial log:** the number of `E] [VT]` and `W] [VT]` lines (especially `Store Version Response Timeout` and `end of object pool`), and of `is now offline` for the VT 0x26, the 1200 0x80 and the TC. All expected 0, as session 26. |
| P3 | Through P2: VT command timeouts (#248) and stalls. | As session 26: no VT stalls, `CAN receive queue: dropped 0`. |
| P4 | Debug `1` a few times through P2. | `PGN 129029 GNSS position:` count rising, method and satellites as session 26, `RTK quality` as session 26. **This is #858's check.** |
| P5 | Failover: the 1200 off and on again, as session 23's R7. | Failover to the CNH VT and back as before, **each by Load Version**. A switch builds a new VT client; it must reach `Connected`, not stay in `Failed`. Stack peak after each switch. |
| P6 | End of the day: debug `1`. | Stack peak and never used, against session 26's P5. Any `CrashReport:` at any boot of the day. |

## 5. Checklist (copy into the timeline, tick live)

PART S -- BENCH
- [ ] S1 banner 0.27, CrashReport none Y/N
- [ ] S2 stack peak ______ of ______
- [ ] S3 MW18 by Load Version Y/N (upload __ s if not); Store Version timeout none Y/N; State Connected Y/N
- [ ] S4 reconnect by Load Version Y/N; peak after ______

PART P -- VAN MASTWIJK
- [ ] P1 Load Version on 1200 Y/N __ s, CNH Y/N __ s; TC08 Y/N
- [ ] P2 from __:__ to __:__; `E] [VT]` __ `W] [VT]` __; `is now offline` VT 0x26 __ 1200 0x80 __ TC __
- [ ] P3 stalls __; dropped __
- [ ] P4 129029 count rising Y/N; method ______; RTK quality ______
- [ ] P5 failover to CNH Y/N, back Y/N, both by Load Version Y/N; peak after ______
- [ ] P6 end of day: peak ______ of ______, never used ______; CrashReport at any boot Y/N

## 6. If something differs

- **The State stuck in `Failed` on a VT** (dump `1`): note the time and which VT, keep the log and the CANedge from
  boot (the End of Object Pool response is in it), power-cycle that VT once and see whether the plough connects. Then
  flash the fallback (f434459) and run the rest on it. This is #868 doing what it says; the question is why the VT
  rejected MW18, which it accepted in session 26.
- **An upload where a Load Version was expected:** note it; not a failure on its own, but put it in the notes for #331.
- **`PGN 129029 GNSS position: none` where session 26 showed it, or RTK quality lost:** flash the fallback; #858 is the
  suspect.
- **A `CrashReport:` block:** keep the whole block and the log before it, flash the fallback.
- **Not a difference:** a VT that never stored MW18 uploading it once.

## 7. Afterwards

- **Notes:** serial log and timeline go on **`test/session27`** under `Ploegbesturing Isobus/Documentation/logs/`,
  named `<date>_session27_<place>_*`.
- **Where results go:** all of it to PR #331; Q1's answer too.
- **When S and P pass:** merge #331 after #327 (it is stacked on it; retarget before `--delete-branch`), then the
  Documentation PR. If Q1 says "skip `Failed`", that is its own PR with a native test, not part of #331.
