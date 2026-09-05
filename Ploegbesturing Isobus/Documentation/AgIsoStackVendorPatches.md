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

### 3. `CANNetworkManager::update_new_partners()` -- carry over CF liveness -- added 2026-09-05

**Files:** `can_network_manager.cpp`

**What:** when a `PartneredControlFunction` is late-bound to a control
function that's already active in the table (i.e. the real device -- VT, TC,
etc. -- claimed its address on the bus *before* our own `Begin()` created the
partner filter for it), the adoption code already copies `address` and
`controlFunctionNAME` onto the partner. It did not also copy
`claimedAddressSinceLastAddressClaimRequest`, so the newly-adopted partner
starts that flag at its default-constructed `false` even though the CF it
was just adopted from already has it `true`. Added one line copying it over,
right next to the existing address/NAME copies:

```cpp
partner->claimedAddressSinceLastAddressClaimRequest = currentActiveControlFunction->claimedAddressSinceLastAddressClaimRequest;
```

**Why:** this is a known, still-open upstream bug --
[Open-Agriculture/AgIsoStack-plus-plus#584](https://github.com/Open-Agriculture/AgIsoStack-plus-plus/issues/584)
(filed 2025-06-03, independently re-confirmed against current `main` as
recently as 2026-07-09, no merged fix as of this writing). Root-caused
against **our own** hardware symptom, not just read off the upstream report:
GitHub issue #17 (`IsobusVtInterface`'s VT Status Timeout ~2-3s after connect
on an Ag Leader InCommand 1200, see `HardwareTestNotes.md` Session 5). Full
chain, traced through this vendored source:

1. `IsobusVtInterface::Begin()` calls `create_partnered_control_function()`
   for the VT. On this rig the InCommand 1200 was already on the bus and
   already address-claimed (it's the tractor's own main screen, running
   before Ploegbesturing powers up) -- so our partner gets bound via
   `update_new_partners()`'s "adopt an already-active CF" branch (this
   patch's location), never via `update_control_functions()`'s
   "brand new claim" branch (which *does* get its flag set correctly, via a
   separate call to `update_address_table()` on the same inbound frame).
2. Any node broadcasting a PGN 60928 (Address Claim) *request* --
   unremarkable on a real tractor bus, and arguably *expected* right after a
   new implement joins, since a VT/TC re-enumerating its connected
   implements is a completely normal reflex -- resets
   `claimedAddressSinceLastAddressClaimRequest` to `false` for every tracked
   CF and starts a 755 ms clock (`MAX_ADDRESS_CLAIM_RESOLUTION_TIME`).
3. 755 ms later, `prune_inactive_control_functions()` evicts any CF whose
   flag is still `false` -- including our VT partner, since (pre-patch) it
   was never `true` to begin with. Its `controlFunctionTable` slot is set to
   `nullptr` and it's marked `ControlFunctionState::Offline`.
4. `process_can_message_for_global_and_partner_callbacks()` only dispatches
   a broadcast (destination-less) message to *any* global PGN callback if
   `message.get_source_control_function()` resolves non-null. Once step 3
   nulls that table slot, **every subsequent VT Status Message frame from
   the real, still-broadcasting InCommand 1200 is silently dropped** before
   reaching either AgIsoStack's own `VirtualTerminalClient::process_rx_message`
   (registered globally, `isobus_virtual_terminal_client.cpp:55`) or
   `IsobusVtInterface`'s own independent `OnVtToEcuMessage` diagnostic
   listener (registered the same way) -- which is exactly why both counters
   froze in perfect lockstep in Session 5's captures. That agreement looked
   like corroboration that "the terminal itself stopped broadcasting," but
   both listeners share the exact same broken lookup, so it proved nothing
   about the physical bus.
5. This also explains GitHub issue #18 (no automatic reconnect): a
   compliant VT normally only broadcasts Address Claim once, at its own
   startup, not spontaneously to satisfy us -- so once evicted, nothing
   naturally repopulates the table slot, and `get_address_valid()` genuinely
   (if wrongly) reports invalid from then on.
6. **Also very likely explains GitHub issue #19** (TC never connected at
   all, same Session 5 rig): `IsobusTcInterface::Begin()` binds its own TC
   partner via the identical `NAMEFilter` + `create_partnered_control_function()`
   pattern (`IsobusTcInterface.cpp:161-170`), adopted the same way if the
   InCommand 1200's TC function was already claimed before we powered up.
   `TaskControllerClient`'s `WaitForServerStatusMessage` state
   (`isobus_task_controller_client.cpp:769-773`) has **no timeout at all** --
   it waits indefinitely for the TC server's first status broadcast, which
   only ever reaches it via the same `process_can_message_for_global_and_
   partner_callbacks()` gate. If the TC partner is evicted before that first
   broadcast arrives, the client is stuck forever with nothing to time out
   or retry -- matching #19's "zero state change, zero capability query, for
   120+ seconds" exactly, independently of the VT's own connection state
   (the "downstream of the VT" theory #19 raised was likely a red herring;
   this is a third, independently-evicted partner hitting the same bug).

Ties #17, #18, and #19 together as one root cause rather than three
unrelated findings. Also explains why Session 4 (single VT, CNH) only saw an
intermittent version of this while Session 5 (InCommand 1200 **plus** the
tractor's own built-in VT live simultaneously) hit it 100% of the time --
more real ECUs on the bus means a PGN 60928 request is far more likely to
occur in the first few seconds after we join.

**Not yet field-verified** against #17, #18, or #19 -- see `HardwareTestNotes.md`
for the next session's result. Per project convention, stays on
`isobus-tc-client` (not merged to main) until confirmed on real hardware.

**Consumed by:** nothing programmatic -- this changes control-flow inside
AgIsoStack itself, not anything Triton-side calls directly.

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

The first two active patches are small, self-contained additions with no
behavioral change to existing code (`get_state()` is a pure accessor; the
error-bit decoding only adds log lines, changes no control flow). Patch #3
is a real one-line control-flow fix for a genuine bug, but is equally
self-contained. Worth proposing upstream to `Open-Agriculture/AgIsoStack-Arduino`
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
- [x] `update_new_partners()` CF liveness carry-over -- opened as a draft PR
      2026-09-05: https://github.com/Open-Agriculture/AgIsoStack-Arduino/pull/16
      (branch `fix-partner-cf-liveness-carryover`, same fork). Also fixes the
      upstream `AgIsoStack-plus-plus` report of the same bug -- commented on
      https://github.com/Open-Agriculture/AgIsoStack-plus-plus/issues/584
      linking this PR, since that repo shares this exact source file. Not yet
      re-verified on the InCommand 1200 hardware that surfaced it -- next
      hardware session's job.
- [ ] Reconsider the `CANNetworkManager` singleton-vs-eager-global question
      upstream, if the hang symptom is ever reproduced cleanly enough to
      write up as a bug report (a Teensy-specific global-static-init timing
      issue, not obviously reproducible off-target).

**Once any PR merges upstream:** bump `platformio.ini`'s
`lib_deps` past whatever release/commit includes it, then remove the
corresponding "Active patches" entry above (and, if PR #14 merges, drop the
`.pio/libdeps/` hand-patch entirely rather than reapplying it after the next
`pio pkg update`/clean rebuild). Until then, all three PRs are drafts on
`Arjan-Woltjer/AgIsoStack-Arduino` -- mark them "ready for review" on GitHub
when satisfied with the wording (PR #16 additionally wants real-hardware
re-verification against the InCommand 1200 before that).
