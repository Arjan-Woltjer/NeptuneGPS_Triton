# AgIsoStack vendored-library patches

Tracks every local modification made directly to the AgIsoStack source under
`.pio/libdeps/teensy41_isobus/AgIsoStack/` (the copy `[env:teensy41_isobus]`'s
`lib_deps` actually pulls and compiles against -- see `platformio.ini`).

**Why this file exists:** `.pio/` is gitignored (PlatformIO-fetched, not
committed). Any edit made directly in `.pio/libdeps/` is invisible to git and
will be **silently lost** the next time PlatformIO re-fetches the dependency
-- `pio pkg update`, deleting `.pio/` for a clean rebuild, or a fresh clone.
There is no automatic reapplication today. If AgIsoStack-dependent behavior
that used to work suddenly regresses after a clean build, check this list
before chasing a code-logic bug.

---

## Active patches

### 1. `VirtualTerminalClient::get_state()` -- added 2026-08-09

**Files:** `isobus_virtual_terminal_client.hpp`, `isobus_virtual_terminal_client.cpp`

**What:** added a public `StateMachineState get_state() const;` accessor that
returns the private `state` member. Mirrors `TaskControllerClient::get_state()`,
which already exists unpatched in the same library version -- `VirtualTerminalClient`
was simply missing the equivalent.

**Why:** `IsobusDebugMenu` needed visibility into VT connect/upload progress
(coarse step N of ~22 through the handshake/upload/activate state machine)
for hardware testing. Before this patch, `get_is_connected()` was the *only*
public signal -- a single boolean, true only once the entire ~22-step sequence
(WaitForPartnerVTStatusMessage -> ... -> UploadObjectPool -> ... -> Connected)
completes. No way to tell "stuck waiting for VT status" from "uploading the
pool" from "waiting for the end-of-pool response."

**Exact change:**

```cpp
// isobus_virtual_terminal_client.hpp, next to the existing get_is_connected() declaration:
bool get_is_connected() const;
StateMachineState get_state() const;   // <-- added

// isobus_virtual_terminal_client.cpp, next to get_is_connected()'s definition:
bool VirtualTerminalClient::get_is_connected() const
{
    return (StateMachineState::Connected == state);
}

VirtualTerminalClient::StateMachineState VirtualTerminalClient::get_state() const   // <-- added
{
    return state;
}
```

**Consumed by:** `IsobusVtInterface::GetStateStep()` / `GetStateTotalSteps()` /
`GetStateName()` (`lib/PloegbesturingCore/src/isobus/IsobusVtInterface.cpp`),
displayed by `IsobusDebugMenu`.

**Considered but not done:** byte-accurate upload percentage via the CAN
transport-protocol layer's `TransportProtocolSession::get_percentage_bytes_transferred()`
(that method IS public) -- rejected because reaching an active session requires
`CANNetworkManager`'s private per-channel `TransportProtocolManager`, which has
no public accessor either. Patching that too was judged disproportionate to the
value over the coarse step indicator for a first pass; revisit if the coarse
indicator proves insufficient during real testing.

---

### 2. `VirtualTerminalClient` End of Object Pool error-bit decoding -- added 2026-08-10

**Files:** `isobus_virtual_terminal_client.cpp`

**What:** in the `EndOfObjectPoolMessage` handler, after the existing raw
`LOG_ERROR("... Pool error bitmask value N")` line, added per-bit decoding
of `objectPoolErrorBitmask` (byte 6 of the response) into readable
`LOG_ERROR` lines: bit 0 = "method or attribute not supported by the VT",
bit 1 = "unknown object reference (missing object)", bit 2 = "any other
error". Bit 3 ("pool deleted from volatile memory") is suppressed unless
it's the *only* bit set, because ISO 11783-6 states a VT should delete the
object pool from volatile memory on any error at all -- it rides along with
essentially every rejection and isn't itself diagnostic.

**Why:** this library only ever logged the raw integer -- upstream doesn't
decode ISO 11783-6's own defined bit layout for this byte anywhere (neither
the client's log line nor the reference `VirtualTerminalServer`, which
hardcodes the byte to `0` on send with an unimplemented `/// @todo`). Session
3 (2026-08-10, Bos, Fendt UT) hit exactly this: `"Pool error bitmask value
9"` required a manual trip to the actual ISO/FDIS 11783-6:2004(E) standard
text to decode as bit 0 + bit 3 (see `Documentation/
EndOfObjectPool_ErrorBitmask_Research.md` for the full byte tables and
sourcing) -- a future rejection will now say "method or attribute not
supported by the VT" directly in the serial log, no manual decode needed.

**Exact change:** in the `EndOfObjectPoolMessage` case's `else` branch
(after the existing `vtRanOutOfMemory`/`otherErrors` `LOG_ERROR` calls),
added an `if (0 != objectPoolErrorBitmask)` block testing bits 0/1/2 with
`LOG_ERROR` each, and bit 3 only if no other bit is set. See the source
directly for the exact code (small enough not to duplicate here).

**Consumed by:** nothing programmatic -- this is a logging-only change, read
directly off the serial debug console during live testing (same channel as
the existing raw bitmask line).

---

## Historical / no longer applied

### CANNetworkManager Meyer's-singleton patch -- applied 2026-08-04, gone by 2026-08-08

During initial CAN bring-up debugging (2026-08-02 to 2026-08-04), `CANNetworkManager::CANNetwork`
was converted from an eager global object (whose constructor did real heap
allocation as C++ global static init, hanging the board before `setup()` ever
ran) into a lazily-constructed function-local static (`CANNetworkManager::CANNetwork()`,
called with parens), with ~112 call sites updated to match.

**This patch is no longer present in the currently vendored copy.** Confirmed
2026-08-09: `can_network_manager.hpp` currently declares a plain
`static CANNetworkManager CANNetwork;` object (no-parens form) -- matching what
Hardware Test Notes session 1 (2026-08-08) independently found and fixed call
sites *back* to (removing erroneous parens added under the old patched
assumption). The singleton patch was evidently wiped by a lib reinstall/update
sometime between 2026-08-04 and 2026-08-08, and was never reapplied -- the
original pre-`main()` hang symptom hasn't recurred since (the real fix for
that turned out to be `-D TEENSY_OPT_SMALLEST_CODE` in `platformio.ini`, not
this patch -- see project memory / commit history around 2026-08-04).

**If that hang symptom reappears:** the singleton conversion is the known fix;
redo it and add it as an active patch entry above, with the same "gets wiped"
caveat.

---

## TODO: upstream

All three of the above are small, self-contained additions with no
behavioral change to existing code (`get_state()` is a pure accessor; the
error-bit decoding only adds log lines, changes no control flow; the
singleton conversion only changed *when* the object is constructed, not
what it does). Worth proposing upstream to `Open-Agriculture/AgIsoStack-Arduino`
(and/or the base `AgIsoStack-plus-plus` repo, since this handler is shared
code) so this project stops needing to carry them at all:

- [x] `VirtualTerminalClient::get_state()` -- opened as a draft PR
      2026-08-11: https://github.com/Open-Agriculture/AgIsoStack-Arduino/pull/14
      (branch `add-vt-client-get-state` on the `Arjan-Woltjer/AgIsoStack-Arduino`
      fork). Build-verified against this project's real teensy41_isobus env
      before submitting (a real consumer, `IsobusVtInterface.cpp`, calls the
      new accessor).
- [x] End of Object Pool error-bit decoding -- opened as a draft PR
      2026-08-11: https://github.com/Open-Agriculture/AgIsoStack-Arduino/pull/15
      (branch `decode-eop-error-bits`, same fork). PR body asks maintainers
      whether they'd prefer named constants/an enum over inline bit literals
      before merging -- open question, not yet answered.
- [ ] Reconsider the `CANNetworkManager` singleton-vs-eager-global question
      upstream, if the hang symptom is ever reproduced cleanly enough to
      write up as a bug report (a Teensy-specific global-static-init timing
      issue, not obviously reproducible off-target).

**Once either PR merges upstream:** bump `platformio.ini`'s
`lib_deps` past whatever release/commit includes it, then remove the
corresponding "Active patches" entry above (and, if PR #14 merges, drop the
`.pio/libdeps/` hand-patch entirely rather than reapplying it after the next
`pio pkg update`/clean rebuild). Until then, both PRs are drafts on
`Arjan-Woltjer/AgIsoStack-Arduino` -- mark them "ready for review" on GitHub
when satisfied with the wording.
