# Ploegbesturing Isobus -- hardware test notes

Running log of real-bus test sessions against `teensy41_isobus`. Newest session at the bottom of each addition.

## Session 1 -- 2026-08-08

**Rig:** New Holland T6 tractor, standard ISOBUS fitted. GPS: Ag Leader Dual Trac, with the InCommand 1200 screen acting as VT.

**Observed:**
- Ploegbesturing claims address `0x81` (129) -- confirmed visible as a connected ECU in the InCommand 1200's VT.
- Real CAN traffic visible on the bus, ~17% busload.
- Guidance data arrives as **legacy** PGNs, not standard NMEA2000: PGN 65267 (Position legacy), 65256 (Speed legacy) and 65535 (XTE JD legacy) all climb steadily at the same ~10 Hz rate from one real sender. Standard NMEA2000 PGNs 129025/129026 stay at 0; 129283 (XTE NMEA2000) saw a single stray hit.
- PGN 60160 (XTE Trimble legacy) only sees sporadic hits (a handful, not steady) from a different source address than the real GPS sender -- most likely incidental J1939 Transport Protocol Data Transfer traffic sharing the same PGN number (confirmed in AgIsoStack's own `can_transport_protocol.cpp`), not real Trimble XTE content. Unconfirmed, not chased further.

**Bugs found and fixed this session:**
1. **Speed read as an impossible 131.70 m/s / 256.00 kn.** `OnLegacySpeed` had no `0xFFFF` "not available" sentinel guard, unlike its NMEA2000 sibling handler. Confirmed via an added raw-value printout (`raw=0xFFFF`). Fixed: guard added, matching `OnSpeedNmea2000`'s pattern.
2. **`CANNetworkManager::CANNetwork()` called as a function** in 11 places across `IsobusGuidanceChannel.cpp`/`IsobusDebugMenu.cpp`. On AgIsoStack-Arduino 0.1.5 it's a static singleton *object* (`static CANNetworkManager CANNetwork;`), not a factory method. Fixed (dropped the parens).
3. **XTE/quality never updated** despite PGN 65535 climbing steadily. Root cause: the source-address filter used `msg.get_source_control_function()`, which is permanently `nullptr` for this sender -- it never broadcasts a real ISO Address Claim (PGN 60928), so AgIsoStack's control-function table never resolves an entry for its address. Fixed: read the address straight off the CAN identifier instead (`msg.get_identifier().get_source_address()`), bypassing control-function resolution entirely.
4. **Hardcoded John Deere legacy source address was wrong.** `kSourceAddressJohnDeere` was `0x2A` (inherited from the old VehicleGps-era fixed-CAN-ID assumption); the real unit on this rig uses source address **`0x80`**. Updated the constant to match.
   **Correction (2026-08-18):** this rig's `0x80` unit was later identified as an Ag Leader/Raven system, not John Deere -- PGN 0xFFFF is shared across brands. `0x2A` was independently verified years ago against a real John Deere system, so it was correct all along, not a stale assumption to replace. Fixed: both source addresses are now accepted (`kSourceAddressJohnDeere = 0x2A`, new `kSourceAddressAgLeaderRaven = 0x80`), not one swapped for the other. Same session's quality-byte read (`d[1] == 0x15`, see "Still open" below) also turned out to be an incomplete check, not just an unconfirmed value -- see the Session 1 XTE quality follow-up below.
5. **`NanoLibcCompat.cpp`** (a `swprintf` linker shim) was suspected of being unnecessary/invented, deleted, and re-tested: link genuinely fails without it under `TEENSY_OPT_SMALLEST_CODE` (`--specs=nano.specs`). Confirmed load-bearing, restored. Open follow-up (tracing exactly what pulls in the wide-locale facet) filed as [GitHub issue #13](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/13).
6. **`OnLegacyPosition` never decodes lat/lon at all** -- it only stamps the GGA fix-age timer. Debug dump shows `Lat/Lon: 0.000000/0.000000` even with a fresh (<100 ms) fix age. **Not fixed** -- a comment in the code claims this matches old `VehicleGps` behavior, but the sibling legacy decoder (`CanSerialParser.cpp`'s `CAN_POS` case) *does* decode+set position, so that claim looks inaccurate. Needs a decision: does anything actually need live coordinates out of this path before adding the decode?

**Still open at end of session 1:**
- XTE now updates (fix age goes fresh) after the source-address fix, but reads an implausible **167.67 m**, and `Quality` stays **0** (the quality byte `d[1]` isn't matching the expected `0x15`). Added raw diagnostics (`word=`, `byte1=` on the `PGN 65535` debug line) to read the real values next session.
  **Correction (2026-08-18):** the exact-match check (`d[1] == 0x15`) was itself wrong -- the correct check is the high nibble only (`(d[1] & 0xF0) == 0x10`), matching `CanSerialParser`'s already-verified `CAN_XTE` case (`t[2] == '1'`, the ASCII-hex form of the same nibble test), reverse-engineered and confirmed years ago. Fixed in `OnLegacyXteJohnDeere`.
- `VTG fix age` stays stale (reads as raw uptime-since-boot) -- consistent with the real device simply never having sent a valid (non-`0xFFFF`) speed reading yet. Not confirmed as a bug vs. genuine absence of data.
- Lat/Lon decode for the legacy Position PGN -- see bug #6.

## Session 2 -- 2026-08-09 (setup only, testing paused)

**Rig:** same as session 1.

Live bus testing paused before it restarted; picking back up next session. Before pausing, three questions were raised about the current code -- answered here by reading the source (not yet re-verified live):

1. **Do we renew our address claim? (VT shows an error immediately when the Teensy is disconnected.)**
   AgIsoStack's `InternalControlFunction` (`can_internal_control_function.cpp`) automatically answers Address Claim requests and reclaims on request via its own state machine (already confirmed in session 1's PGN-60928-NACK investigation -- that NACK is a separate, benign quirk, unrelated to this).
   More directly relevant to the observed VT behavior: AgIsoStack's `VirtualTerminalClient` sends the ISO 11783-6 mandatory **Working Set Maintenance Message** on its own internal timer, as long as `vtClient->update()` keeps being called -- which `IsobusVtInterface::Update()` does every `loop()` iteration ([IsobusVtInterface.cpp:89-97](../lib/PloegbesturingCore/src/isobus/IsobusVtInterface.cpp#L89-L97), wired from `main.cpp`). A compliant VT uses loss of that periodic message to flag "Working Set disconnected" almost immediately -- this is automatic AgIsoStack behavior, not project-specific code, but it *is* actively running as a direct consequence of our own `Update()` loop staying alive.

2. **Do we actively request the PGNs we need?**
   Yes. `IsobusGuidanceChannel::Begin()` ([IsobusGuidanceChannel.cpp:135-141](../lib/PloegbesturingCore/src/isobus/IsobusGuidanceChannel.cpp#L135-L141)) sends one explicit `ParameterGroupNumberRequestProtocol::request_parameter_group_number()` per PGN we consume (129025, 129026, 129283, 65267, 65256, 65535, 60160), right after address claim completes, once at startup.

3. **Do we push our program (object pool) to the VT?**
   Yes. `IsobusVtInterface::Begin()` ([IsobusVtInterface.cpp:60-84](../lib/PloegbesturingCore/src/isobus/IsobusVtInterface.cpp#L60-L84)) finds the VT via a `NAMEFilter`-matched `PartneredControlFunction`, builds a real, non-trivial object pool (`BuildObjectPool()` / `VT3PoolData`+`VT3PoolSize` in `VTObjectPool.cpp` -- not a stub), then calls `vtClient->set_object_pool(...)` and `vtClient->initialize(false)` to upload it.

   **Correction (2026-08-09):** the line above originally claimed this "matches session 1's observation of the controller actually appearing/working in the InCommand 1200's VT" -- that overstated what session 1 actually observed. Session 1's note (above) only confirms Ploegbesturing was **visible as a connected ECU** (address claim / device presence), not that its working set's data mask (the plough control screen) was ever seen displayed. As of this session, the Ploughcontrol screen itself has still not been seen on the VT. Code path being wired up correctly is not evidence it rendered -- see "Still open" below.

All three: real, wired-up code paths (traced end-to-end from `main.cpp`'s `setup()`/`loop()`), not aspirational or dead code. That the code runs is confirmed; that the resulting screen has been *seen* on a real VT is not.

**Still open going into session 3:**
- Ploughcontrol's working set / data mask has not been visually confirmed on the InCommand 1200 (or any VT) yet, despite the ECU itself being visible and `set_object_pool()`/`initialize()` running without error. Many ISOBUS VTs require the operator to manually select which connected implement's working set to display (an "implements" list or app switcher) rather than auto-switching to a newly-connected client -- worth checking on the InCommand 1200 specifically before assuming a pool/upload bug. Not yet confirmed either way.

## Session 3 -- 2026-08-10

**Rig:** Fendt tractor, Universal Terminal (UT) acting as VT. GPS/guidance: Trimble, including an embedded Trimble Task Controller. First live test of `IsobusTcInterface` (the branch this session is on, `isobus-tc-client`).

**Bugs found and fixed this session:**
1. **`IsobusVtInterface::Begin()` and `IsobusTcInterface::Begin()` constructed their partner control function directly** (`std::make_shared<PartneredControlFunction>(...)`) instead of via `CANNetworkManager::CANNetwork.create_partnered_control_function(...)`. Only the factory registers the object into `CANNetworkManager`'s `partneredControlFunctions` list, and only members of that list are ever checked against an incoming Address Claim frame (`can_network_manager.cpp`'s `update_control_functions()`) -- so a directly-constructed partner could **never** become address-valid, no matter how long a real VT/TC sat on the bus. Same class of mistake as session 1's `CANNetworkManager::CANNetwork()` bug: a direct-construction shortcut that bypasses required registration. AgIsoStack's own `can_control_function.hpp` and its `VirtualTerminal.ino` example both use the factory form. Confirmed as the fix: before it, both VT and TC sat at their initial "not connected" state indefinitely (VT: `0/22 Disconnected`); after it, VT progressed all the way to `UploadObjectPool`/`EndOfObjectPool` and TC connected outright.
2. **TC DDOP was missing its mandatory root `DeviceElement` of `Type::Device`.** AgIsoStack's own header states outright: *"the device descriptor object pool shall have one device element of type device"* -- `IsobusTcInterface::buildDdop()` had zero (Connector/Function hung directly off the DVC object instead of off a root Device element). Compounding this, `add_device_property()`/`add_device_process_data()` don't take a parent at all -- attaching a DPT/DPD to its owning `DeviceElementObject` requires a separate `add_reference_to_child_object()` call, which was never made, leaving Offset X/Y and both process-data variables as floating, unattached objects. Both fixed (root `"Plough"` Device element added; `add_reference_to_child_object()` called for both the Connector's and Function's children); structure label bumped `TC01` -> `TC02`. Confirmed as the fix: before it, TC connection failed with `"There are errors in the DDOP. Faulting parent ID: 1 Faulting object: 0"` / `"Unknown object reference (missing object)"` / `"Client terminated"`; after it, TC connects (`Connected: Y`), later reached `Task active: Y` once the operator started a task, and issued exactly 2 Value Requests -- matching our exactly 2 declared Settable DDIs (513, 514) -- confirming it recognizes our DDOP correctly by object/DDI, not just that the connection stopped erroring.

**Confirmed working this session:**
- TC connects, task goes active, and correctly identifies our two declared process data objects (2 Value Requests, one per DDI).
- `teensy41_isobus` builds clean throughout (verified after every change).

**Still open / not resolved this session:**
- **VT object pool is rejected outright**, even after fix #1 got the handshake all the way to the upload step. Real terminal (confirmed VT version 6, via a new `get_connected_vt_version()`-backed debug line -- ruling out a VT3-vs-VT4+ language-code-list version-gating theory) responds: `"Error in end of object pool message. Faulty Object 0 Faulty Object Parent 65535 Pool error bitmask value 9"`. Three live bisection rounds against the real terminal (temporarily swapping `VTObjectPool.cpp`'s real 23-object pool for a minimal one, each under a fresh version label to rule out terminal-side caching):
  1. Bare WorkingSet + empty DataMask, `softKeyMask = NULL_OBJECT_ID` -- **failed identically**.
  2. Same, but DataMask references a real (0-key) SoftKeyMask instead of NULL, matching AgIsoStack's own reference pool's convention -- **failed identically**.
  3. Same, but background colour `1` (not `0`/black) and one real visible child, matching AgIsoStack's own reference pool (`examples/VirtualTerminal/ObjectPool.cpp`) byte-for-byte on those fields -- **failed identically**.
  All three rule out pool *content* as the variable. Separately, our WorkingSet/DataMask/SoftKeyMask/Key/OutputString/OutputNumber/NumberVariable/FontAttributes encoding was checked byte-for-byte against both AgIsoStack's own `get_number_bytes_in_object()`/`get_minimum_object_length()` size formulas and the actual decoded bytes of its real reference pool -- structurally identical in every case. Root cause not found via live bisection or static analysis alone. Real 23-object pool restored before ending the session (bisection flag off, version label back to `MW01`) -- ships in its prior, still-rejected state.
  **Next step (deferred to off-tractor):** reproduce against [Open-Agriculture/AgIsoVirtualTerminal](https://github.com/Open-Agriculture/AgIsoVirtualTerminal) (a free VT *server* simulator built on AgIsoStack++ itself, Windows installer, latest v1.5.0 as of this session) to determine whether the same reference-library VT also rejects our pool (a genuine bug in our bytes/upload path) or accepts it (a Fendt-UT-specific quirk). [AgIsoTerminalDesigner](https://github.com/Open-Agriculture/AgIsoTerminalDesigner) (GUI pool designer/editor) is a second option for authoring a known-good comparison pool without hand-rolling bytes.
- **DDI 513/514 never arrived, despite TC connected + task active + an active guidance line + steering enabled** (operator-confirmed all four). TC issued 2 Value Requests (see fix #2 above) but zero Value Commands the entire session -- added raw `Value commands (any DDI)` / `Value requests (any DDI)` counters to `IsobusDebugMenu` specifically to distinguish "TC sends nothing" from "TC sends something but not 513/514"; confirmed it's the former. Leading theory: this rig's embedded Trimble Task Controller doesn't implement TC-GEO at all -- DDI 513/514 are optional per ISO 11783-10, a TC is allowed to connect and go active without ever sending them, and an AgIsoStack log line this session (`"The TC is < version 4 but no VT was provided"`) is consistent with a lightweight/basic TC implementation. Not confirmed against Trimble's own documentation; not chased further this session.
- **The project's stated real goal -- implement-independent, this rig's actual XTE -- is blocked by a separate, more fundamental gap**, independent of the DDI 513/TC-GEO question above: **all three XTE PGNs `IsobusGuidanceChannel` listens for (129283 NMEA2000, 65535 JD legacy, 60160 Trimble legacy) received zero messages the entire session**, while Position (129025) and Speed (129026) from the same Trimble unit flowed normally throughout. Confirmed the data genuinely exists on this rig and isn't simply absent: the operator observed a live 29 cm deviation directly on the Trimble terminal's own screen (with the AB line active and steering enabled) -- that number was never received by us, it's a locally-displayed reference value only, useful for cross-checking once/if we do receive real XTE. Means whatever PGN actually carries Trimble's XTE on this specific rig is not one of the three we're coded for.
  **Next step:** a proper CAN bus sniff with a CAN logger, output in MF4 (ASAM MDF) format, while jogging side-to-side a known amount to see which PGN's payload tracks the Trimble terminal's own displayed deviation -- rather than continuing to guess PGN numbers live.

**TC-GEO licensing angle (this rig is Bos):** separate research done the same
morning (`NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md`) into how each
ISOBUS terminal brand gates TC-GEO gives the DDI 513/514 theory above a
concrete, non-code explanation. Trimble sells the base "ISOBUS Task
Controller" license (TC-BAS + TC-SC) as one SKU and TC-GEO as a *separate,
additional* license on top -- Trimble's own dealer docs state the base
license is a prerequisite for the prescription license, so a farm can hold
TC-SC without ever having bought TC-GEO. **Likely explanation for this
session's "TC connects and goes active but never sends DDI 513/514":** Bos's
terminal simply isn't TC-GEO licensed, not a Triton-side or Trimble-firmware
bug. This also explains something Bos separately flagged: they've had to buy
a standalone plough-control system with its own A/B lines rather than driving
plough guidance off the tractor terminal's own geo-referenced task data --
consistent with no georeferenced stream being available out of this
terminal's Task Controller at all (TC-SC only gives section on/off + totals).
This is the exact degraded case the DDI 513 architecture already anticipated
(deviation measured at the implement's own DRP, not borrowed from the
terminal) -- see `NeptuneGPS Documentation/Design documents/
Triton_TC_Client_Design.md` sec 6. **Not yet
confirmed directly** (no ISOBUS diagnostics screen check was done this
session to see TC-GEO's licensed state explicitly) -- open items: exact GFX
model/firmware version; whether Bos's dealer confirmed "not purchased" vs.
some other fault; name of the separately-purchased plough-control system and
whether it exposes any ISOBUS interface; whether TC-SC (section control)
itself was confirmed working, to isolate "no TC-GEO license" from "no TC
license at all." A 2026-08-10 firmware change (see the reference-parser
verification follow-up below) added `IsobusTcInterface::
SupportsTcGeoWithPosition()/SupportsTcGeoWithoutPosition()`, reading the
connected TC's own reported capability bits directly -- Session 4 below is
the first chance to use it instead of inferring from DDI silence.

**Code changed this session** (all on `isobus-tc-client`, uncommitted as of end of session): `IsobusVtInterface.cpp/.hpp`, `IsobusTcInterface.cpp/.hpp`, `IsobusDebugMenu.cpp`, `VTObjectPool.cpp` (bisection scaffolding added then reverted -- `VT_POOL_MINIMAL_BISECT_TEST` present but `#define`d to `0`).

## Follow-up (off-tractor) -- 2026-08-10: reference-parser verification

Before building/wiring the full `AgIsoVirtualTerminal` GUI app (which needs a
real or PEAK-virtual CAN adapter shared between it and a live client -- not
available off-tractor), took a cheaper offline shortcut that answers the same
question session 3 deferred to it ("does the reference library's parser
accept our exact pool bytes"): `Open-Agriculture/AgIsoStack-plus-plus`'s own
IOP object-pool parser (`isobus_virtual_terminal_working_set_base.cpp`'s
`parse_iop_into_objects()`/`parse_next_object()`, fetched fresh from `main` --
this is the *same* parser code `AgIsoVirtualTerminal` links against) needs
nothing but plain C++ and a handful of self-contained files (`isobus_virtual_
terminal_objects.hpp/.cpp`, `can_stack_logger.hpp/.cpp`, `to_string.hpp`,
`thread_synchronization.hpp` -- no CAN, no network manager, no threading
beyond a `std::mutex`). Built a standalone MSVC console harness (scratch-only,
not committed) that: (1) links in a byte-for-byte copy of `VTObjectPool.cpp`'s
real (non-bisection) 23-object pool build, (2) feeds the resulting 445 bytes
straight into `parse_iop_into_objects()`, no CAN transport involved at all.

**Result: the reference parser accepts the pool outright.** `parse_iop_into_
objects()` returned `true`; all 23 objects landed in the object tree (no ID
collisions silently dropped one); zero `LOG_ERROR`/`LOG_WARNING` lines other
than the expected debug trace of the `"nl"` language code. This corroborates
session 3's manual byte-for-byte check independently (via the actual upstream
parser source rather than just its exposed size-formula getters) across every
object type actually used (`WorkingSet`, `DataMask`, `SoftKeyMask`, `Key`,
`OutputString`, `OutputNumber`, `NumberVariable`, `FontAttributes`) --
`parse_next_object()`'s field offsets for each matched our `append*()`
functions exactly, byte for byte.

**Also checked *why* AgIsoStack itself can't decode "Pool error bitmask value
9":** AgIsoStack++'s own reference `VirtualTerminalServer::update()`
(`isobus_virtual_terminal_server.cpp` ~line 2822-2834) hardcodes that error-
code byte to `0` on every `EndOfObjectPoolMessage` response it sends, whether
the pool parsed or not (`/// @todo Get the parent object ID of the faulting
object` sits right next to it, unimplemented) -- so the reference library
never populates or interprets ISO 11783-6's per-bit meaning for that byte on
the sending side.

**Correction, same day, later:** that byte is *not* actually opaque -- it's
just undecoded by AgIsoStack's own sender. Reading ISO/FDIS 11783-6:2004(E)
§C.2.5 directly (see `NeptuneGPS Documentation/ISOBUS/research/EndOfObjectPool_ErrorBitmask_Research.md`
for the full byte tables and sourcing) shows the End of Object Pool Response
carries two separate error-code bytes: a coarse pass/fail byte, and a second
"Object Pool Error Codes" byte with a standardized bit layout (bit 0 = method/
attribute not supported by the VT; bit 1 = unknown object reference; bit 2 =
any other error; bit 3 = pool deleted from volatile memory -- boilerplate
that rides along with essentially any rejection, per the standard's own text
that a VT should clear the pool on any error). **`9` decodes as bit 0 + bit
3** -- stripping the boilerplate bit 3, the Fendt UT's actual complaint is
**"method or attribute not supported by the VT,"** specifically on object 0
(the WorkingSet itself, matching "Faulty Object 0"). This is a specific,
narrower complaint than "structural/encoding error" -- consistent with the
reference parser's clean accept of the exact same bytes (§ above): the pool
is spec-legal, this VT just doesn't support something about the WorkingSet
object's own attributes.

**Conclusion: the leading theory is now sharper than "Fendt-specific
rejection reason"** -- it's specifically something in the **WorkingSet
object's own attribute fields** (not its children/masks). Session 3's three
bisection rounds varied DataMask/SoftKeyMask content and colour, never the
WorkingSet's own fields -- next bisection round should vary the WorkingSet
specifically: a macro attached directly to it, the `Selectable` attribute,
the Active Mask object ID reference, or anything version-gated that the
reference parser accepts leniently but a production VT enforces strictly.
Cheaper than the full `AgIsoVirtualTerminal` GUI/CAN-adapter bench test,
and it's a real bisection question for Session 4 (or a future Bos revisit)
rather than blind static analysis.

Also added this same day: `IsobusTcInterface::SupportsTcGeoWithPosition()`/
`SupportsTcGeoWithoutPosition()`, reading the connected TC's own reported
`ServerOptions` capability bits (from its `ParameterVersion` handshake
message) via AgIsoStack's existing `get_connected_tc_option_supported()`,
surfaced in `IsobusDebugMenu`'s TC screen as `TC-GEO (with/without pos):
Y/N`. Built clean on `teensy41_isobus`/`teensy41_serial`; committed
(086d5d6) and pushed to `origin/isobus-tc-client` ahead of Session 4 below.
This settles "does the connected TC implement TC-GEO at all" directly from
its own handshake instead of inferring it from DDI silence -- see the
Session 3 TC-GEO subsection above and `Documentation/
NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md` for the licensing research this was
prompted by.

## Session 4 -- 2026-08-10 (afternoon, van Mastwijk) -- PLANNED, not yet run

**Rig:** second visit to the same site as Session 1 (Ag Leader InCommand
1200, CNH tractor -- Case IH/New Holland/Steyr TBC; all three share the same
PLM/AFS backend per `NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md`, so behavior
should be consistent regardless of badge).

**Why this session matters:** the InCommand 1200 is the one display in the
Ag Leader lineup that ships with UT *and* TC standard, TC-GEO included, no
unlock purchase required (unlike the InCommand 800, which needs a paid
unlock for both, or Compass, which never gets TC at any price) -- see
`NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md` sec 3. This makes van Mastwijk the
contrast case to Session 3/Bos: same DDOP, same Triton firmware (`isobus-tc-
client` at commit 086d5d6 or later), a brand where the TC-GEO commercial
gate is largely absent by default. If DDI 513/514 delivery works anywhere,
this is the rig where it should.

**What to check on-site (fill in results below when run):**
- [ ] `IsobusDebugMenu`'s new `TC-GEO (with/without pos):` line -- expect `Y`
      (or at least `Y` for "with position based control", the one the DDOP
      declares support for) once TC connects.
- [ ] Whether Triton's DDOP is accepted and the working set connects cleanly
      against a terminal that *does* have TC-GEO (contrast with Bos, where
      TC connected fine but the TC-GEO data channel never activated).
- [ ] Whether DDI 513/514 Value Commands actually arrive once a task is
      active (contrast with Session 3, which got zero the entire session).
- [ ] Whether the terminal only logs totals or genuinely exchanges
      geo-referenced data -- i.e. confirm TC-GEO isn't just "present" but
      *functionally* exchanging data.
- [ ] VT object pool: does it get accepted here? (Session 1 never got as far
      as confirming the plough-control screen rendered; Session 3's Fendt UT
      rejected it outright -- a clean accept here would support the
      "Fendt-UT-specific" theory from the reference-parser follow-up above.)
      If it's *also* rejected with the same "Faulty Object 0 ... bitmask 9",
      per `NeptuneGPS Documentation/ISOBUS/research/EndOfObjectPool_ErrorBitmask_Research.md` that's a
      "method/attribute not supported" complaint about the WorkingSet object
      specifically -- try `VTObjectPool.cpp`'s new `VT_WORKINGSET_BISECT_VARIANT`
      switch (0=production, 1=selectable=false, 2=no language codes,
      3=language "en" instead of "nl"; ships as 0, no behavior change until
      set), one variant per flash, watching `IsobusDebugMenu`'s VT state line
      to see if any variant gets past `WaitForEndOfObjectPoolResponse`. The
      AgIsoStack vendor patch documented in `Documentation/
      NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md` (#2) will print the decoded error bit
      directly in the serial log if it still fails, instead of a raw number.

**Results:** Two terminals present on-site: an Ag Leader display and CNH's own
built-in VT (AFS Pro/IntelliView) -- tested against CNH specifically (Ag
Leader was powered off throughout, to keep results attributable to one VT;
our partner filter matches on function code alone and can't otherwise
distinguish which VT it binds to when more than one is live). CNH's own
stack unlocks TC-BAS+TC-SC+TC-GEO together via one dealer activation code
(different licensing shape than Ag Leader's, see
`NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md` sec 3) -- so this ended up testing a
third distinct terminal/vendor combination rather than the originally-planned
Ag-Leader-specific contrast case. That comparison (Ag Leader vs. Bos) is
still open for a future session.

**The VT object pool rejection is solved.** Root cause, found by rigorous
elimination across two independent real terminals (Fendt UT from Session 3,
CNH VT3/VT4 here) and confirmed by exact byte arithmetic, not inference:

- Every pool-*content* axis was re-tested here and failed identically
  regardless: WorkingSet `selectable`, language count, language code
  (variants 1-3, carried over from Session 3's plan), a fresh version label
  each round (ruling out terminal-side caching), background colour matching
  AgIsoStack's own reference pool exactly (variant 4), and even forcing the
  terminal itself into VT4 negotiation instead of VT3 (same physical
  terminal, same result). A corrected NAME `manufacturer_code` (was `64` --
  belongs to a real, different manufacturer, a known unfixed TODO from
  `Triton_TC_Client_Design.md` sec 7; changed to `1407`, AgIsoStack's own
  permitted-for-non-commercial-use code, matching their reference examples)
  also made no difference.
- Decisive test: uploaded **AgIsoStack's own real reference pool**
  (`examples/VirtualTerminal/ObjectPool.cpp`'s `VT3TestPool`, ~150 KB, pulled
  in via a raw `#include` of the actual file -- not a copy -- so these were
  genuinely their exact bytes) against the same CNH terminal. **It connected
  and rendered.** First time this entire project has confirmed a working,
  on-screen VT pool on real hardware. This proved the upload mechanism,
  partner registration, and NAME/identity were all fine, and that something
  specific (still unidentified at that point) remained wrong in *our* pool's
  content specifically.
- Comparing the reference pool's WorkingSet object field-by-field against
  ours surfaced the one field never isolated: the reference WorkingSet has
  **1 child object reference**; every one of our variants (0-4) kept this at
  0. Adding a child (referencing our own `Label_Position` OutputString)
  triggered a **second, previously-latent bug**: `appendWorkingSet()` wrote
  the language-code bytes immediately after the header, but the correct ISO
  11783-6 wire order (children list, then macros, then language list) puts
  children *first*. With `numChildren` always 0 until this test, the wrong
  order and the correct order produced byte-identical output -- completely
  invisible until a real child existed. The CNH terminal read our language
  code `"nl"` (bytes `0x6E, 0x6C`) as the low/high bytes of the child's
  object ID: `0x6C6E` = **27758**, exactly matching the "unknown object
  reference" (`bitmask 0x0A` = bit1 + bit3) it reported -- confirmed by hand
  arithmetic, not a guess, and reproduced identically after a full power
  cycle of the terminal (ruling out stale terminal-side state).
- Fixed both the field order (`appendWorkingSet()` now only writes the
  header; callers append any children via `appendObjRef()`, then the
  language code via the new `appendLanguageCode()`, in that order) and added
  the real WorkingSet child. **Folded into production** (`VTObjectPool.cpp`,
  `VT_WORKINGSET_BISECT_VARIANT` defaults to 0 = the fix) rather than left
  behind a toggle -- confirmed working after the fold-in too. The bisection
  variants (1-4) remain available for later re-testing against Fendt, which
  was never retested with a WorkingSet child at all -- given the exact
  same object was implicated there too (Session 3's "Faulty Object 0"), this
  may turn out to be the same root cause.

**Still open:**
- **Connection stability after Connected.** Confirmed staying connected for
  real stretches (multiple consecutive debug dumps showing
  `Connected: Y, 21/22`), but at least one drop was observed, logged as
  `E] [VT]: Status Timeout` -- traced to AgIsoStack's hardcoded
  `VT_STATUS_TIMEOUT_MS = 3000`: once connected, if the VT's own periodic
  status broadcast isn't received within any 3-second window, the client
  disconnects itself. A same-session test leaving the debug menu alone (to
  rule out `IsobusDebugMenu::printFullDump()`'s many blocking `Serial.print()`
  calls stalling `loop()` past that window) still dropped after ~12 seconds,
  which doesn't cleanly support the print-blocking theory (roughly netting
  out to a handful of real connected seconds after subtracting `setup()`'s
  two blocking delays, `delay(3000)`+`delay(2000)`, during which nothing
  processes CAN traffic at all -- though those delays run once, before the
  first connection, and can't explain a timeout that fires from the
  already-Connected state later).

  **Leading hypothesis, found post-session (2026-08-10, desktop):** the key
  clue is that the reference-pool test (see above) stayed stable throughout,
  while our own pool drops intermittently -- and the one thing that differs
  behaviorally, not just structurally, between those two tests is
  `IsobusVtInterface::updateVtVariables()`, which sent 4
  `send_change_numeric_value()` commands **unconditionally every 100 ms**
  (40 msg/s) the entire time connected, regardless of whether any value
  actually changed. AgIsoStack's own reference example only calls that
  function on a button press, not continuously. Against our own pool those 4
  object IDs are real, bound `OutputNumber` widgets -- the VT does actual
  redraw work on every one of those 40 messages/sec; against the swapped-in
  reference pool, those IDs almost certainly don't resolve to a
  `NumberVariable` at all, so the VT can reject them cheaply without
  rendering anything. Sustained real redraw load intermittently starving the
  VT's own periodic status broadcast past the 3 s window is a plausible,
  common class of embedded-UI issue -- but this is a hypothesis, not
  confirmed on hardware.

  **Fix applied (not yet field-verified):** `updateVtVariables()` now sends
  only on real value change, plus a 1 s heartbeat resend (so a dropped CAN
  frame can't leave the VT stale forever) -- cuts steady-state traffic
  roughly 10x for slow-changing plough telemetry with no functional loss.
  Built clean on `teensy41_isobus`/`teensy41_serial`. **Next session: re-test
  connection stability specifically** (long-duration `Connected: Y` dumps,
  watching for `[VT]: Status Timeout`) to confirm or rule this out -- if it
  still drops, the redraw-load theory is wrong and this needs to go back to
  irregular-vs-fixed-interval investigation.
- DDI 513/514 -- not reached this session (task never went active against
  CNH); the `TC-GEO (with/without pos):` capability line from 086d5d6 was
  not exercised live here either.
- Ag Leader itself was never tested (kept powered off to isolate CNH) --
  still a clean, distinct next test.
- Fendt re-test with the WorkingSet-child fix -- see above, a real
  candidate for also resolving Session 3's original rejection.

## Session 5 -- 2026-09-05

**Rig:** New Holland tractor. Two VTs live simultaneously: an Ag Leader
InCommand 1200 (with Ag Leader's own GPS/guidance) and the tractor's own
built-in New Holland VT. Triton's partner filter matches on function code
alone (see Session 4's note on this) and bound to the InCommand 1200 both
times it connected today; the New Holland VT was never isolated/tested on
its own this session.

**Goal:** confirm our HMI (VT object pool) uploads and renders correctly on
a terminal we hadn't tried live before, following on from Session 4's
WorkingSet fix (only verified against CNH's own VT and a Fendt UT so far).

**Firmware:** `isobus-tc-client` at `a5db807` (includes everything through
the 2026-08-18 reorg -- soft keys, app-switcher icon, WorkingSet fix, VT
flooding throttle -- none of it field-verified until today).

**Confirmed working:**
- VT object pool upload/render: **confirmed working.** The InCommand 1200
  reaches `Connected` (`21/22`) reliably on every power-cycle tested today,
  and the app-switcher icon added 2026-08-10 was visually confirmed on
  screen. This answers today's main question -- our HMI content and upload
  path are correct on this terminal.

**Bug found, not yet fixed -- reproducible VT Status Timeout:**

Every single power-cycle (2 full cycles captured with a timestamped serial
log) reproduced the exact same failure, ~100% reliably:

1. VT reaches `Connected` (`21/22`) within ~1s of `loop()` starting.
2. Stays connected and stable (busload steady ~17%, our own guidance-PGN
   traffic counter climbing normally) for **~2-3 seconds**.
3. `E] [VT]: Status Timeout` fires (AgIsoStack's own hardcoded
   `VT_STATUS_TIMEOUT_MS = 3000`), state drops to `Disconnected` (`0/22`),
   and **never recovers** -- stayed at `0/22` for the rest of both captures
   (120+ seconds in one case) with zero automatic retry. A serial replug
   does not reproduce it or clear it; only an actual Teensy reset does.

Added a new, independent diagnostic to `IsobusVtInterface`
(`GetVtStatusMessageCount()`/`GetVtStatusMessageAgeMs()`, wired into
`IsobusDebugMenu` as `vtstat=<count>/<age>ms`): a second global PGN 0xE600
(`VirtualTerminalToECU`) listener alongside AgIsoStack's own `vtClient`,
counting `Function::VTStatusMessage` frames directly off the bus,
independent of AgIsoStack's internal `lastVTStatusTimestamp_ms` (which isn't
publicly exposed). Both captures showed the same pattern: **exactly 4 VT
status messages received, then complete silence**, well inside the 3s
window before our own watchdog fires:

```
vt=Y(21/22) vtstat=2/726ms   -- just connected
vt=Y(21/22) vtstat=4/726ms   -- 2 more arrived (~1/s, as expected)
vt=Y(21/22) vtstat=4/1726ms  -- stuck at 4, ~1s with nothing
vt=Y(21/22) vtstat=4/2726ms  -- stuck at 4, ~2.7s with nothing
E] [VT]: Status Timeout
vt=N(0/22) vtstat=4/3726ms   -- never increments again
```

Since our own guidance-PGN message counter (`msgs=`) keeps climbing at a
steady rate throughout -- before, during, and after the freeze -- this rules
out a general CAN-receive/main-loop stall on our end. **The InCommand 1200
itself stops broadcasting its own mandatory VT status message ~1.5-2s after
we connect**, not us failing to hear something it keeps sending.

**Isolation test, ruling out our own outbound traffic as the cause:**
Session 4's leading theory for a similar (but intermittent) drop on CNH was
VT redraw load from `updateVtVariables()`'s unconditional traffic -- already
throttled by 2026-08-10's fix, present in this build. To test whether *any*
of our post-connect outbound traffic (the on-change/1s-heartbeat variable
updates) provokes this, fully disabled the `updateVtVariables()` call
(`#if 0`'d in `IsobusVtInterface::Update()`), rebuilt, reflashed, and
repeated the power-cycle. **Identical failure signature** -- exactly 4
status messages, then the same timeout at the same ~2-3s mark. Reverted the
test change (confirmed innocent); this rules out our own outbound VT
traffic entirely. Root cause of *why* the InCommand 1200 stops broadcasting
is still open -- current leading theory is a terminal-side quirk/limitation,
not a Triton bug, but that's not yet confirmed against Ag Leader's own
documentation or a dealer (same shape of open question as Session 3's
TC-GEO licensing angle for Bos/Trimble). Filed as
[GitHub issue #17](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/17).

**Second, separate finding -- no automatic reconnect:** AgIsoStack's own
state machine (`isobus_virtual_terminal_client.cpp`'s `Disconnected` case)
should auto-retry as soon as `partnerControlFunction->get_address_valid()`
is true, which the VT's claimed address should still be. On real hardware,
across both captures, it never did -- stuck at `0/22` with zero retries
until a physical reset. Not yet chased down why the automatic path doesn't
fire in practice. A watchdog in `IsobusVtInterface::Update()` that forces
`vtClient->initialize(...)` again after a prolonged disconnect would at
least recover automatically without a manual reset, regardless of root
cause -- proposed, not yet implemented or tested. Filed as
[GitHub issue #18](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/18).

**Not reached this session:**
- New Holland's own built-in VT was never isolated/tested (InCommand 1200
  was live the whole time and Triton bound to it both cycles) -- open
  question whether the same ~4-message cutoff is InCommand-1200-specific or
  more general.
- **Task Controller never connected at all, either cycle** -- the periodic
  line's `tc=`/`drp=`/`tcq=` fields stayed flat at `tc=N drp=0mm tcq=0` for
  the entire duration of both captures (through the VT fault and for 120+s
  afterward). Notable because the InCommand 1200 is the terminal
  `NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md` identifies as shipping with TC-GEO
  standard, no unlock required -- Session 4's original reason for wanting to
  test against it. Not clear yet whether this is downstream of the VT
  connection never staying up long enough, or a separate, unexamined gap in
  `IsobusTcInterface`'s own partner discovery. Filed as
  [GitHub issue #19](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/19).

**Raw logs:** timestamped serial captures for all three runs referenced
above are saved under `Documentation/logs/`:
`2026-09-05_session5_run1_vt-connect-fault.log` (first connect/fault
correlation, predates the vtstat counter),
`2026-09-05_session5_run2_vtstat-baseline.log` (natural failure with the new
counter), `2026-09-05_session5_run3_isolation-test-no-burst.log`
(`updateVtVariables()` disabled -- the isolation test that ruled out our own
outbound traffic).

**Code changed this session** (uncommitted as of end of session, on
`isobus-tc-client`): `IsobusVtInterface.hpp/.cpp` (new
`GetVtStatusMessageCount()`/`GetVtStatusMessageAgeMs()` + the PGN 0xE600
listener backing them), `IsobusDebugMenu.cpp` (surfaces the new counter in
both the full dump and the periodic line).

## Follow-up (off-tractor) -- 2026-09-05: #17 root-caused, not a terminal quirk

Chasing "leading theory is a terminal-side quirk" further by reading the
vendored AgIsoStack source directly (rather than waiting for another
hardware session) found the real cause, and it isn't the InCommand 1200 at
all -- **it almost certainly never stopped broadcasting its VT status
message.** The frames are silently dropped in our own stack after our VT
partner control function is wrongly evicted from AgIsoStack's own
control-function table, a known, still-open upstream bug:
[Open-Agriculture/AgIsoStack-plus-plus#584](https://github.com/Open-Agriculture/AgIsoStack-plus-plus/issues/584)
(filed 2025-06-03, independently reproduced against current `main` as
recently as 2026-07-09).

Full chain, and why it explains #18 (no auto-reconnect) too, is written up
in `NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md` (patch #3) rather than
duplicated here. Short version: `IsobusVtInterface::Begin()` binds our VT
partner via AgIsoStack's "adopt an already-active control function" path,
because the InCommand 1200 -- the tractor's own screen -- is already on the
bus and already address-claimed before Ploegbesturing powers up. That path
never marks the adopted partner as "recently claimed," so the first PGN
60928 (Address Claim) request seen anywhere on the bus afterward -- a normal
reflex for another node noticing a newly-joined implement -- makes AgIsoStack
evict our partner 755ms later even though the real device was never invalid.
Once evicted, every subsequent VT status frame from the real, still-
broadcasting InCommand 1200 gets silently dropped before reaching *any*
listener, including the independent `vtstat` counter added earlier this
session -- which is exactly why it froze in lockstep with AgIsoStack's own
tracking. That agreement looked like corroboration that the terminal itself
had gone quiet; both listeners actually shared the same broken lookup.

This also reframes why Session 4 (single VT, CNH) only saw an intermittent
version of a similar drop while Session 5 (InCommand 1200 **plus** the
tractor's own built-in VT live simultaneously) hit it 100% of the time --
more real ECUs on the bus makes a PGN 60928 request in the first few seconds
after joining far more likely.

**Also very likely explains #19 (TC never connected at all)**, checked the
same day: `IsobusTcInterface::Begin()` binds its own TC partner via the
identical `NAMEFilter` + `create_partnered_control_function()` pattern, and
`TaskControllerClient`'s `WaitForServerStatusMessage` state has **no
timeout at all** -- it waits indefinitely for the TC server's first status
broadcast, gated through the exact same broken dispatch. If the TC partner
gets evicted before that first broadcast arrives, the client is stuck
forever with nothing to time out or retry -- matching #19's "zero state
change, zero capability query, for 120+ seconds" independently of whatever
happened to the VT. The "downstream of the VT" theory #19 raised was likely
a red herring; more probably a third, separately-evicted partner hitting
the same bug. See `NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md` patch #3 for the full chain.

**Fixed** (not yet field-verified): vendor patch propagating the missing
liveness flag, plus bumping `IsobusVtInterface::Begin()`'s log level
Warning -> Info so a future capture would show AgIsoStack's own `[NM]`
control-function lifecycle lines directly instead of needing this level of
source-diving again. Filed upstream:
[AgIsoStack-Arduino#16](https://github.com/Open-Agriculture/AgIsoStack-Arduino/pull/16).
Posted the full analysis to GitHub issues #17, #18, and #19, tying all
three together as one root cause. **Next hardware session's job:** confirm
the InCommand 1200's VT stays connected past the old ~2-3s cutoff, the TC
now connects too, and cross-check any `[NM]` lines the Info log level now
surfaces against this theory.

## Session 6 -- 2026-09-05 (afternoon) -- #17/#18/#19 fix confirmed on hardware

**Rig:** same as Session 5 -- New Holland tractor, its own built-in VT, plus
an Ag Leader InCommand 1200 (with Ag Leader GPS). Both terminals available,
powered independently through the session so the same firmware could be
tested against each one alone and against both together.

**Firmware:** `isobus-tc-client` at `935de81` -- Session 5's `vtstat` counter
plus the off-tractor root-cause work (vendor patch #3 propagating
`claimedAddressSinceLastAddressClaimRequest` through
`update_new_partners()`, and the VT log level bumped Warning -> Info).

**Headline:** #17 (VT Status Timeout) and #19 (TC never connects) are both
resolved and confirmed on hardware. A *second*, deeper instance of the same
eviction pathology was then found, root-caused to PGN 60928 address-claim
roll-calls, fixed on the tractor as vendor patch #4, and verified -- VT and
TC now hold together for 5+ minutes on a full bus, the first time in the
project's history. #18 (no automatic reconnect) remains unresolved and is
now understood as the thing that turns each eviction into a permanent
outage. Separately, XTE was found to be misscaled by 100x, unnoticed since
Session 1.

### Phase 1 -- New Holland VT alone: stable

First real confirmation. VT reached `Connected` (`21/22`) about a second
after `loop()` started and **held for the entire 160-second capture** with
no drop of any kind. `vtstat` climbed steadily 1 -> 164 at almost exactly
1 Hz throughout -- i.e. the terminal's status broadcast was arriving
continuously and, critically, was still being *dispatched to our listener*,
which is precisely what the eviction bug used to break. Compare Session 5,
where the same counter froze at 4 within ~2 seconds every time.

Also confirmed live by the operator: the plough working set rendered on
screen with live data, and adjusting the plough's working width updated the
displayed values in real time. `VT: Wider pressed` / `VT: Narrower pressed`
appear in the log at each press, so the soft-key path (990e23b) is
field-verified too, first time.

An `[NM]` line appears at startup now that the log level is Info -- but its
arguments print as garbage (`name 000000000000000lx`, absurd address/channel
numbers). That's a format-string/`vsnprintf` mismatch inside AgIsoStack's own
logging on this toolchain, not something our patch introduced; the lines are
still useful as *event* markers (which is how they're used below), just not
for their values. Worth a small upstream fix eventually.

### Phase 2 -- InCommand 1200 alone: VT **and** TC both connect

With New Holland powered down and the InCommand 1200 as the only terminal,
a fresh Triton boot produced the cleanest result of the whole project so far:

- VT connected (`21/22`) within ~1s of the terminal appearing on the bus,
  `vtstat` climbing steadily at ~1 Hz.
- **`[TC]: DDOP Activated without error.` -- the Task Controller connected**,
  `tc=Y`, and stayed connected for as long as the configuration was left
  alone. This is the **first successful TC connection in the project's
  history** (Sessions 3-5 never got past `tc=N`).
- The InCommand 1200 mirrored Ploegbesturing's own screen content, operator-
  confirmed visually -- not just the app-switcher icon, the actual working
  set.

This settles #19: the TC failure was never about TC-GEO licensing or a gap in
`IsobusTcInterface`'s discovery. It was the same control-function eviction
knocking out the TC partner exactly as it did the VT partner, precisely as
the 2026-09-05 off-tractor analysis predicted (commit `935de81`). Note
`W] [TC]: The TC is < version 4 but no VT was provided` and `DDOP will be
generated using the server's version instead of the specified version. New
version: 3` -- worth revisiting whether we should hand the TC client our VT
instance, but it activated cleanly regardless.

`drp=0mm` / `tcq=0` throughout -- DDI 513/514 still never arrived, so
TC-GEO data exchange itself remains unproven. The connection and DDOP
activation are confirmed; the geo-referenced data flow is not. That part of
Session 3's original question is still open.

### Phase 3 -- the real trigger: PGN 60928 address-claim roll-calls

Every configuration that failed today had one thing in common, and it is not
the number of VTs. Counting `NACK-ing PGN request for PGN 60928` lines across
the whole session:

| Window | Duration | PGN 60928 requests | Result |
|---|---|---|---|
| New Holland VT alone | 160 s | **0** | VT stable throughout |
| InCommand 1200 alone | 22 s | **0** | VT **and** TC stable |
| Every failing window | -- | ~1 every 2 s | dies in ~3-4 s, never recovers |

The transition is visible to the second, in the capture where a healthy
VT+TC session was killed by powering the second terminal back on:

```
[ 622.850 .. 644.293]  vt=Y(21/22) tc=Y, vtstat 2 -> 24   <- zero 60928 requests
[ 645.299]  tc=N                                          <- TC drops
[ 645.814]  NACK-ing PGN request for PGN 60928            <- first request
[ 646.808]  E] [VT]: Status Timeout                       <- VT drops
[ 647.545]  ...requests every ~2 s from here on, forever
```

PGN 60928 is the ISO 11783 / J1939 **Address Claimed** message; a *request*
for it is a bus roll-call asking every node to re-announce its address --
exactly what a terminal does when it notices a new implement. In AgIsoStack,
each such request clears `claimedAddressSinceLastAddressClaimRequest` on
every tracked control function and starts a 755 ms timer, after which
`prune_inactive_control_functions()` evicts anything that hasn't
re-announced. That is the same prune that caused #17; patch #3 fixed only the
*initial* partner adoption, not this recurring cycle.

**Why it never recovers** -- found by reading `update_address_table()`:

```cpp
if (targetControlFunction != nullptr)
    targetControlFunction->claimedAddressSinceLastAddressClaimRequest = true;
else
    // a previously-pruned CF re-announces: restore it to the table...
    controlFunctionTable[channelIndex][claimedAddress] = currentControlFunction;
    // ...but the liveness flag is NEVER set on the restored CF
```

A CF that has been pruned once goes back into the table with the flag still
`false`, so the next roll-call prunes it again immediately. That is exactly
the prune/re-adopt churn observed for 300+ seconds straight: `is now
offline` -> `has claimed address` -> `is now offline`, every 1-2 seconds,
indefinitely.

### Phase 3b -- both VTs live simultaneously

Every configuration with **two VTs on the bus at once** broke, consistently
and unrecoverably. Three separate ways of reaching that state, same outcome:

1. **Second terminal joins an established connection.** Triton was connected
   and stable to New Holland for 160s; powering on the InCommand 1200
   triggered a burst of `[NM]` control-function offline/claim activity, and
   ~4s later `E] [VT]: Status Timeout`. Never recovered.
2. **Triton reboots with both already settled.** Fresh boot, both terminals
   long since address-claimed and quiet. VT connected normally, held ~4s,
   then the same `Status Timeout`, then stuck. This rules out "it's just the
   join-moment storm" -- the steady state with two VTs is itself unstable.
3. **Second terminal joins a working VT+TC session.** From Phase 2's healthy
   state, New Holland coming back online dropped **both**: first
   `E] [TC]: Server Status Message Timeout. The TC may be offline.`
   (`tc=N`), then ~1.5s later `E] [VT]: Status Timeout` (`vt=N`). Neither
   came back.

After each of these, the log settles into a **continuous churn** -- two
addresses alternately claiming and going offline every 1-2 seconds,
indefinitely (observed for 300+ seconds straight in one capture), with
`vt=N(0/22)` and `tc=N` frozen throughout. Guidance PGN traffic keeps
flowing normally the whole time (`msgs=` climbing ~31/s, busload steady
~19%), so this is specifically the partner/control-function layer thrashing,
not a bus or loop problem.

**A dead end worth recording:** the first theory here was that
`IsobusVtInterface::Begin()`'s `NAMEFilter` matches on function code alone,
so two VT-function devices confuse the single partner slot (a risk flagged
back in Session 4). **That theory is wrong.** Disabling the New Holland VT
from the tractor's own settings -- leaving exactly one VT on the bus --
reproduced the failure unchanged (connect, ~3 s, `Status Timeout`, stuck).
What actually correlates is the roll-call traffic in Phase 3 above, not the
VT count. The two-VT cases fail because a joining terminal enumerates the
bus, not because there are two of them.

### Phase 4 -- the fix, written and verified on the tractor

With the mechanism understood, applied as **vendor patch #4** (see
`NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md`): exempt `Partnered` control functions from the
roll-call prune, alongside the `Internal` exemption already there.

```cpp
(ControlFunction::Type::Internal  != controlFunction->get_type()) &&
(ControlFunction::Type::Partnered != controlFunction->get_type())
```

Rationale: a partner is a CF we explicitly bound to and are actively
conversing with. A bus roll-call must not silently evict it out from under a
live session -- and genuine partner loss is already detected at the right
layer, by the client's own status timeout (`VT_STATUS_TIMEOUT_MS`, and the
TC's server-status timeout). Deliberately applied *alone*, without also
fixing the `update_address_table()` restore bug found above, so the result
would be attributable to a single change.

**Result, in the exact configuration that had been dying in ~3 seconds all
afternoon** (InCommand 1200 + full tractor ECU population, New Holland VT
disabled):

```
t = 303.6 s   ->  5+ minutes uptime, still going
tc=Y     : 304 consecutive samples      tc=N : 0
vt=Y     : every sample, vtstat 318 and climbing ~1 Hz
Status Timeouts : 0        CF evictions : 0        TC errors : 0
```

Operator-confirmed live: working set on screen, plough width adjustments
tracking in real time (`VT: Wider/Narrower pressed` throughout). **VT and TC
both stable, together, on a full bus, for the first time in the project.**

**And it was tested against the real event.** A first look at this run
suggested the roll-calls had stopped, because it logged zero `NACK-ing PGN
request for PGN 60928` lines -- but that proxy is wrong. A *broadcast*
roll-call is never NACKed; only a request addressed specifically to us is.
The honest measure is prune activity, and there was plenty of it:

| | evictions | partner lost? |
|---|---|---|
| pre-patch, New Holland alone (healthy) | 0 | -- no prunes ran |
| pre-patch, InCommand alone (healthy) | 23 | no (survived on timing) |
| pre-patch, dual VT (failing) | 62 | yes |
| pre-patch, NH disabled (failing, died ~3 s) | 6 | yes |
| **post-patch, NH disabled, 800 s** | **235** | **no -- zero losses** |

So pre-patch, every roll-call was a dice roll: does the partner's Address
Claim response credit the liveness flag before the 755 ms prune fires?
Sometimes yes, usually not. Post-patch the partner is exempt outright, and
**235 consecutive prune cycles were survived in the exact configuration that
had been dying in three seconds** -- with external CF address 205 visibly
churning offline/online every 2 s the whole time, a live demonstration of
the `update_address_table()` restore bug that no longer touches us.

### Phase 4b -- the worst case, tested and survived

Finally, the heaviest version of the trigger: powering the New Holland VT
back on *while* Triton was connected to the InCommand 1200. Earlier the same
afternoon this exact action killed a healthy VT+TC session in about four
seconds (Phase 3b, scenario 3). Post-patch:

```
[132.305] External CF has claimed address 203452081 on channel 536903720
[138.812] External CF has claimed address 203452081 on channel 2147516416
[144.105] External CF has claimed address 203452081 on channel 2147516672
      ^^ the same enumeration burst that was previously fatal

185 consecutive samples: vt=Y AND tc=Y, zero drops
0 Status Timeouts, 0 TC errors, 104 evictions of other CFs survived
vtstat 1255 -> ~21 minutes unbroken VT + TC connection
```

Operator confirmed the plough working set stayed live and responsive on the
InCommand 1200 the whole time. `Partnered control function ... has claimed
address` count: **0** -- we never rebound to the newly-arrived VT, so the
function-code-only `NAMEFilter` ambiguity did not cause trouble here either.

**Patch #4 is therefore verified against both the steady-state prune cycle
and the worst-case terminal-join storm.** Before today the record for a held
VT connection on a full bus was roughly three seconds.

### Phase 4c -- the TC diagnostic that had never been readable

With the TC finally connected *and stable*, `IsobusDebugMenu`'s full dump
could be read for the first time -- including the `TC-GEO (with/without
pos):` line added in `086d5d6` specifically to settle Session 3's licensing
question, and never once reachable since:

```
--- Task Controller ---
  Connected:    Y
  TC-GEO (with/without pos): Y/N     <- supports TC-GEO WITH position
  Task active:  Y
  Value commands (any DDI): 0  last DDI=(none)
  Value requests (any DDI): 0
```

Operator-confirmed alongside this: a field task was running the whole time,
the AB line was selected, and the terminal was actively computing XTE.

**This kills the TC-GEO licensing theory from Session 3.** The InCommand
1200 *advertises TC-GEO with position support*, a task is active, guidance
is live -- and it sends us nothing whatsoever. Note this is strictly worse
than Session 3's Trimble, which at least issued 2 Value Requests (one per
declared settable DDI), proving it had parsed our DDOP. This TC activates
our DDOP without error and then never engages with our process data at all.

So the open question is no longer "is TC-GEO licensed on this terminal" but
**"why does a TC-GEO-capable server ignore our declared process data"** --
candidates being the DDOP's device-element structure, the server-version-3
downgrade (`W] [TC]: The TC is < version 4 but no VT was provided`, and
`DDOP will be generated using the server's version instead of the specified
version. New version: 3`), or how the implement's DRP/offsets are declared.
That is desk work against `IsobusTcInterface::buildDdop()` and ISO 11783-10,
not another field session.

### Phase 4d -- XTE quality byte is wrong for this rig too

The same dump captured the raw legacy-XTE diagnostics added in Session 1:

```
PGN 65535 XTE JD legacy: 10128  last SA=0x80 word=0xBFF byte1=0x3
Quality: 0   RTK quality=4  IsRtkQuality=N
```

The quality byte from this unit is `0x03`. Our check is
`(d[1] & 0xF0) == 0x10` -- high nibble must be 1 -- so it fails, and
`Quality` sits at 0 with `IsRtkQuality=N`. **We are therefore rejecting the
guidance quality outright on this rig**, which gates the plough control
logic.

This is not a separate defect: Phase 5 below shows it shares one root cause
with the frozen XTE value. Both come from applying the John Deere payload
layout -- byte offsets *and* quality convention, reverse-engineered years
ago against a real John Deere unit -- to an Ag Leader/Raven message on the
same proprietary PGN. `d[1]` simply is not a John Deere quality byte here.

Also still true from Session 1 bug #6: `Lat/Lon: 0.000000 / 0.000000` --
the legacy position PGN still never decodes coordinates.

### Phase 5 -- the legacy XTE decode reads the wrong payload entirely

Found by accident: the VT rendered the cross-track error as
**42949532.47**. That decodes exactly -- raw `4294953247` =
`(uint32)(-14049)`, i.e. `updateVtVariables()`'s `GetXte() + 1000` went
negative and wrapped -- so `GetXte()` was `-15049`, an absurd XTE.

Chasing it produced a wrong answer first, then the right one. Recorded in
that order deliberately, because the wrong answer was seductive.

**The wrong answer (a 100x scale error).** Operator read 144 cm off the Ag
Leader screen while our periodic line showed `xte=-144.65m`. Working back
through the decode:

```cpp
unsigned long val = (data[4] << 8) | data[3];
result.xteHundredthsMeter = int(val - 32000) >> 1;   // (val - 32000) / 2
```

`-14465` implies `val = 3070`, matching the captured `word=0xBFF` (3071)
exactly. Change the divisor from 2 to 200 and it yields **-144.65 cm**
against a measured **144 cm**. That is a near-perfect match, it made the
offset (32000) and field (`data[3..4]`) look correct, and it neatly
retro-explained Session 1's *"implausible 167.67 m"* as 1.68 m. It was
wrong.

**The right answer.** Further ground-truth readings taken moments later --
the operator called out 8 cm, then 118 cm, then 14 cm -- while our decoded
value moved not at all:

```
terminal:  144 -> 8 -> 118 -> 14 cm
ours:      only ever -144.65 or -142.09   (val = 3070 or 3582)
```

Two distinct values across ~400 samples, with `xteAge` at ~57 ms
throughout, so messages were arriving and being accepted continuously. The
field we read is essentially **frozen** while real XTE swings by more than a
metre. The 144.65 / 144 agreement was a coincidence, and a single matching
data point was never enough to conclude from.

**Root cause:** PGN 0xFFFF (65535) is manufacturer-proprietary and, as the
decode's own comment says, "heavily overloaded". Session 1's correction
added Ag Leader/Raven (SA `0x80`) as an accepted sender for the **John
Deere** decoder, on the assumption that both vendors use the same payload
layout. This rig says otherwise: same PGN number, different vendor payload.
We are reading John Deere byte offsets out of an Ag Leader message.

That single cause explains both symptoms at once -- the near-constant value
(`data[3..4]` isn't Ag Leader's XTE field) and the quality byte reading
`0x03` instead of matching the John Deere high-nibble convention
(`(d[1] & 0xF0) == 0x10`), which is why `Quality` sits at 0 and
`IsRtkQuality` at N. See Phase 4d.

**Consequences:**
- The legacy XTE path is not merely misscaled, it is decoding a foreign
  vendor's payload. It cannot be fixed by adjusting a constant.
- Fixing it needs a raw capture: all 8 bytes of PGN 65535 from SA `0x80`
  logged against ground-truth readings like the ones taken today, then the
  Ag Leader layout derived. That is the CAN sniff Session 3 recommended and
  which still has not been done.
- Worth checking the alternatives first: this session counted exactly **1**
  PGN 129283 (standard NMEA2000 XTE) message and 3 of PGN 60160, so neither
  is currently a usable source on this rig either.
- Independently, `updateVtVariables()` should clamp before casting to
  `uint32_t`, so an out-of-range value degrades visibly instead of
  rendering as 42 million. The `+1000` bias assumes ±10 m; that assumption
  should be stated or enforced rather than implied.

**Sign convention remains unverified.** The operator's left/right calls were
corrected mid-sequence, so the recorded magnitudes (144, 8, 118, 14 cm) are
reliable but their signs are not. Establish this properly during the raw
capture.

### On #18 (no automatic reconnect)

Still unresolved, and today explained *why* AgIsoStack's own retry path never
fired. `StateMachineState::Disconnected` re-enters the handshake only
`if (partnerControlFunction->get_address_valid())` -- and that was genuinely
false, because the partner had been evicted from `controlFunctionTable` with
its address set to `NULL_CAN_ADDRESS`. The retry logic was working exactly as
written; its precondition was being destroyed underneath it.

So #18 was never an independent bug -- it is what turned every eviction into
a *permanent* outage rather than a stutter, which is why a serial replug
never helped and only a power cycle did.

It is still worth fixing. Patch #4 stops roll-calls evicting partners, but
does not make us resilient to a partner genuinely disappearing and returning
-- a terminal power-cycled, a connector knocked loose, a real bus fault. In
those cases `get_address_valid()` legitimately goes false and, on current
evidence, we would again sit in `Disconnected` forever. An implement needing
a manual reset after any hiccup is not field-acceptable. Same applies to
`IsobusTcInterface`, whose `WaitForServerStatusMessage` has no timeout at all
and so cannot self-recover under any circumstances.

**Issues filed from this session:** #20 (legacy XTE decodes the wrong
vendor's payload), #21 (TC reports TC-GEO support but sends no DDI 513/514),
#22 (upstream the AgIsoStack eviction fixes). #17 and #19 closed as fixed
and verified.

**Raw logs:** `Documentation/logs/2026-09-05_session6_*.log`.

**Code changed this session:** AgIsoStack **vendor patch #4** (Partnered CFs
exempt from the roll-call prune) -- documented in
`NeptuneGPS Documentation/ISOBUS/research/AgIsoStackVendorPatches.md`, applied to the gitignored
`.pio/libdeps/teensy41_isobus` tree per the existing convention, so it does
not appear in the repo diff. No Triton-side source changed.

**Open at end of session 6** (all desk work -- nothing here needs a tractor):
- **Upstream patch #4 properly.** It is verified on hardware against both
  the steady-state prune cycle and the terminal-join storm; it should go to
  `Open-Agriculture/AgIsoStack-Arduino` together with the
  `update_address_table()` restore-without-liveness-flag fix, which was left
  deliberately unapplied today so patch #4's field result stayed
  attributable to one change. Both belong in the same PR as one coherent
  fix to the same pathology.
- **Why does a TC-GEO-capable TC ignore our process data** (Phase 4c)?
  Prime suspects: DDOP device-element structure, the server-version-3
  downgrade, or how the DRP/offsets are declared. Check
  `IsobusTcInterface::buildDdop()` against ISO 11783-10.
- **Legacy XTE decodes the wrong vendor's payload** (Phase 5, with Phase 4d
  as the same root cause). Needs a raw 8-byte capture of PGN 65535 from
  SA `0x80` against ground truth, then the Ag Leader layout derived -- the
  CAN sniff Session 3 asked for. Not fixable by adjusting a constant. Also
  clamp in `updateVtVariables()` before the `uint32_t` cast, and establish
  the sign convention while capturing.
- **#18 auto-reconnect watchdog** -- still the difference between a
  transient bus event and a dead session needing a power cycle. Less urgent
  now that the main eviction cause is fixed, but still correct to have.
- **Session 1 bug #6** -- legacy position PGN still never decodes lat/lon,
  confirmed still true today.
- Our stack NACKs PGN 60928 requests ("no callback could handle it")
  instead of answering with an Address Claim. Written off as benign in
  Session 1; it is at minimum the visible marker of the event that was
  killing us. Note it also misled today's analysis briefly -- a *broadcast*
  roll-call produces no NACK, so NACK count is not a proxy for roll-call
  activity. Prune activity is.


## Session 7 -- 2026-09-08 (Ag Leader InCommand 1200) -- DDI 505/506 handshake probe

**Question:** does the InCommand 1200 implement AEF Tramline Control at all? If
it does, DDI 513/514 are reachable and #21 is a DDOP build-out problem. If it
does not, no DDOP work will ever produce them from this terminal.

Run on branch `spike/21-ddi-505-506-handshake`, off `isobus-tc-client` at
6403897. Both vendor patches #3/#4 (plus #5) confirmed present in
`.pio/libdeps/teensy41_isobus` before flashing.

### Result: no DDI 506 reply, under either declaration

Two runs, differing only in the DDI 505 value and the structure label:

| Run | DDI 505 declared | Structure label | Uptime observed | DDI 506 | Value cmds | Value reqs |
|-----|------------------|-----------------|-----------------|---------|------------|------------|
| 1   | `0` (no level)   | TC03            | 16 min 11 s     | none    | 0          | 0          |
| 2   | `0x01` (Level 1) | TC04            | 6 min 17 s      | none    | 0          | 0          |

Run 2 exists specifically to close the ambiguity the probe's own comment
called out: a TC that short-circuits the handshake for a zero-capability
implement produces the same silence as one with no Tramline Control at all.
Declaring Level 1 removes that escape. The label bump to TC04 was required --
without it the terminal serves run 1's cached pool and the re-run proves
nothing. Both runs logged `[TC]: DDOP Activated without error`, so the pool
including DDI 505/506 was accepted each time.

Throughout both runs the TC reported `Connected: Y`, `Task active: Y`, and
`TC-GEO (with position): Y`, with `reconnect attempts=0` on both the VT
(`0x26`) and TC (`0xF7`) partners.

**Reading:** on this evidence the InCommand 1200 very likely does not implement
Tramline Control, and DDI 513/514 will not arrive from it regardless of how the
DDOP is built out. That makes #21's remaining DDOP-structure theories
(device-element shape, server-version-3 downgrade, DRP/offset declaration)
unlikely to be the operative cause *on this terminal*.

### The confound that must be closed before this is final

**Zero Value *Requests*, not just zero Value Commands.** A TC with a genuinely
active task that has our device elements mapped into it would normally at least
*request* our declared DPD values. Seeing zero in both directions is equally
consistent with our implement never having been added to the running task on
the terminal's own setup screen -- in which case both runs measured nothing at
all, and the negative above is not attributable to Tramline Control support.

`Task active: Y` does not settle this: AgIsoStack's own docs warn the flag is
unreliable per-brand (John Deere reports "always in task"), and it is advisory
only. Before treating the Tramline conclusion as established, confirm on the
InCommand itself that the Ploegbesturing implement appears in the implement
list and is attached to the running task with an AB line set. This is the same
trap as Session 5's `vtstat` counter and Session 6's single XTE reading --
do not promote this to a settled finding on one unverified precondition.

### VT soft keys -- verified on real hardware for the first time

Independent of the TC work, and previously untested since being wired up
2026-08-18: all three soft keys were pressed on the terminal and all three
arrived. BREDER x5 (`VT: Wider pressed`), SMALLER x3 (`VT: Narrower pressed`),
AUTO x2 (`VT: Auto pressed (not wired to control)`). AUTO's message is the
intended behaviour, not a fault -- `InterfacePlough`'s AUTO mode is derived,
not user-settable. Note there are no left/right soft keys in the pool at all;
only these three exist.

### Also observed

- Connection stability continues to hold: run 1 ran 16+ minutes across a host
  PC sleep/resume with zero reconnect attempts on either partner. The `[NM]`
  offline/online churn in the logs is confined to a non-partnered CF at address
  205, which is exactly what vendor patch #4 is supposed to permit.
- PGN 65535 from SA `0x80` still streams (`510301FF0B0009FF` and neighbours),
  still with `xte=0.00m` and `q=0` -- #20 unchanged, as expected, nothing was
  done about it today.

### CANedge full-bus capture -- the raw data #20 has been waiting for

A full CAN log was recorded with the CANedge logger across this session, with
**ground truth called out live, in this order: 71 cm, then 1 cm, then 0 cm,
then 99 cm on the other side of the line, then back to 0 cm, then 52 cm on that
same far side again.** Six points, three zero crossings, both sides of the line
represented, and two distinct magnitudes (99, 52) on the far side rather than
one.

That descending sequence is the important part, and it is exactly what #20
needs. Session 6's mistake was deriving a confident "100x scale error" from a
single static reading that matched to within 0.5% -- three further readings
then showed the field was frozen and the match was coincidence. A capture
containing *changes* of known size and direction cannot be satisfied by a
frozen field, so it can distinguish a scale error, a byte-offset error and a
dead decode from one another, which no single static point can.

The spread is particularly useful because it spans two orders of magnitude: a
candidate byte pair must track all four values, which kills most wrong-offset
hypotheses outright rather than leaving them merely unlikely.

**And the sequence crosses the line three times.** 71 -> 1 -> 0 approaches zero
from one side, 99 cm is on the other, the return to 0 crosses back, and 52 cm
goes out to the far side a second time. A decode can fake a single sign flip
through an unrelated bit that happened to toggle once; reproducing three
crossings in the right order, with two different far-side magnitudes, is not
something a wrong hypothesis does by accident. The two far-side values also
guard against a decode that merely saturates or latches when the sign flips --
it has to render 99 and 52 distinctly, not just "far".

The capture ends with the plough control being disconnected, which is a clean
end marker on the bus: our control function drops off, and everything after
that point is other traffic.

The crossings are what the sign convention has
been missing since session 6, when the operator's left/right calls were
corrected mid-sequence and the recorded magnitudes (144, 8, 118, 14 cm) were
left reliable but unsigned. Here the ordering itself carries the sign: whatever
field encodes XTE must be large, shrink through 1 to 0, then grow again to
roughly 99 with the opposite sign (or with a separate side/direction flag
flipping). A decode that reproduces the magnitudes but not that flip is wrong,
and one that reproduces both is almost certainly right. Session 6's open
"sign convention remains unverified" item should be closeable from this log
alone.

Pair this against PGN 65535 frames from SA `0x80`. The payloads logged on the
serial side during session 7 sat around `510301FF0B0009FF` / `510302FF10000DFF`
while our decode reported `xte=0.00m` and `q=0` throughout, so whatever carries
71 cm is in bytes we are currently misreading.

Note the two clocks are independent: the CANedge log and the serial logs above
have no shared timebase, so the called-out markers have to be located within
the CANedge capture on its own terms rather than by correlating timestamps.

**Better still, the sign convention is probably distillable from the log
itself.** The 71 / 1 / 0 figures are what the terminal displayed, which means
the terminal was transmitting that quantity on the bus the whole time -- so the
capture should contain a *continuous* ground-truth series, not just three
hand-called points. Two things follow. First, find the frames whose decoded
value passes through 71, 1 and 0 in that order: those three points are enough to
identify the carrier, and once identified it supplies ground truth at full rate
for the entire session. Second, that series almost certainly crosses zero and
goes negative somewhere in a full working log, and a zero crossing is exactly
what the sign convention needs -- which would settle an item open since session
6 without another trip to a tractor. Worth checking before planning any
sign-establishing test drive.

To do: convert the MF4 with the CANedge-to-SocketCAN-pcap converter that landed
on `claude/triton-isobus-gps-objectpool-fsrrdf` (2026-09-07), then derive the Ag
Leader byte layout against the 71 cm / 0 pair. **File location not yet recorded
here -- add it before this note ages.**

The same capture is also the independent check on session 7's own confound: if
the TC never addressed us at all, that is visible on the bus directly, without
relying on our own counters. This is the lesson from session 5's `vtstat` --
a second measurement that shares the dispatch path of the thing it corroborates
is not independent. A separate logger genuinely is.

**Raw logs:** `Documentation/logs/2026-09-08_session7_*.log`.

**Code state:** the Level-1 declaration in run 2 was a diagnostic claim only --
Level 1 means "the implement calculates the tramline tracks", which a plough
does not do. It has been **reverted to `0`**, and the structure label with it
(TC04 -> TC03, which the reverted pool matches byte for byte again). Only the
findings, the logs and the updated commentary merge; the firmware behaviour is
unchanged from where session 7 started. Note TC04 is now burned on any terminal
that took part in run 2 -- the next real DDOP tree change must go to TC05.

**Open after session 7:**
- **Confirm the implement is mapped into the task on the InCommand** -- until
  this is done the session's main result is provisional.
- If it was mapped, close #21 against this terminal as "no Tramline Control
  support" and move the DDI 513/514 question to a different brand's TC.
- **#20 now has its capture** -- the CANedge log plus six ground-truth points
  (71 / 1 / 0 / 99-far-side / 0 / 52-far-side) with three zero crossings. Next
  step is off-tractor: convert the MF4, find the PGN 65535 SA `0x80` frames, and
  derive the Ag Leader layout that tracks all six values *and* the sign flips.
  Record the MF4's location here first. On this data the sign convention should
  be derivable too, closing session 6's "sign convention remains unverified"
  without another tractor session.
- #18 auto-reconnect watchdog still unexercised -- nothing disconnected.


## Session 8 -- 2026-09-08 (afternoon, John Deere tractor) -- the terminal tells us why

Same day as session 7, different brand. Run on branch
`spike/21-ddi-505-506-jd-terminal` off `main` (session 7 having reached main via
PR #24 / #28). Both AgIsoStack vendor patches verified present before flashing.

**Headline: this terminal has Tramline Control, it was switched on, and it says
`no compatible implements detected`.** That single message is worth more than
both of session 7's silent runs -- it converts #21 from "does any terminal
implement this?" into "our DDOP is incomplete", and it gives an iterable signal.

### Why our DDOP is rejected -- answered by our own research doc

`NeptuneGPS Documentation/ISOBUS/research/TramlineControl_TC_Support_Research.md` sec 6 already specifies the Level 1
**required** set: DDI **505** (as a DPT, bit 0 set), **506 in the same device
element**, "and the rest of the Level 1 required set: **515, 507, 508, 509,
510, 511**".

We declare only 505 and 506. So `no compatible implements detected` is exactly
what the spec predicts for our pool -- the terminal is not failing to answer,
it is declining to recognise an implement that declares 2 of the 8 required
DDIs. Session 7's Ag Leader silence is now much less likely to have been "that
terminal lacks the feature", and much more likely to have been the same
incomplete declaration meeting a terminal that simply says nothing instead of
reporting it.

### Runs

| Run | DDI 505 | Label | DDOP result | Tramline screen | DDI 506 | Value cmds | Value reqs |
|-----|---------|-------|-------------|-----------------|---------|------------|------------|
| 1 | `0` | TC03 | Activated without error | (not read) | none | 0 | 0 |
| 2 | `0x01` | TC04 | Activated without error | still "no compatible implements detected" | none | 0 | 0 |

Run 2's TC04 reuses session 7's label deliberately: TC04 already denotes the
Level-1 probe pool, this terminal had never seen it, and reusing a label for
*identical* content is correct. The don't-reuse rule is about different content
under the same label.

So declaring Level 1 is **not sufficient** on its own -- the message persisted
across the reflash. This is a much sharper result than a silent run.

### Terminal differences worth recording

- **TC-GEO reported `Y/Y`** (with *and* without position). The Ag Leader
  reported `Y/N`.
- `W] [TC]: Timeout waiting for version request from TC. This is not required,
  so proceeding anways.` -- new on this brand, benign, stack proceeds.
- VT object pool loaded from the terminal's NVM by matching label `MW03` -- the
  VT side works on John Deere too. Session 3's Fendt/JD-era VT rejection has
  never been retested until now; this is the first confirmation on this brand.
- A recurring `[TP]: Received a Clear to Send (CTS) message with a global
  destination, ignoring` / `End of Message Acknowledge ... global destination`
  pair appears on this bus. **It is not ours**: a clean boot captured zero of
  them while our DDOP uploaded successfully. Someone else on the JD bus is
  sending destination-specific TP flow control to the global address, and
  AgIsoStack correctly ignores it. Noise for us -- do not chase it.

### XTE: the legacy JD decode is correct on John Deere hardware

The important correction of the session. In the `teensy41_isobus` build there
is **no serial GPS at all** -- `main.cpp` compiles the UART out under
`#ifndef ISOBUS` and guidance arrives over CAN via `IsobusGuidanceChannel`.
Only CAN is connected to this tractor.

With GPS live (RTK, quality 4), our reading matched the terminal twice, on
different magnitudes:

| Terminal | Triton |
|----------|--------|
| -6 cm | `xte=-0.06m` |
| -33 cm | `xte=-0.33m` |

Both sign and scale agree. This is the first time the legacy XTE path has been
confirmed correct against ground truth, and it supports #20's framing directly:
the decode uses John Deere byte offsets, so it is right on a John Deere and
wrong on an Ag Leader. #20 is a per-brand layout problem, not a broken decoder.

**Unresolved, and the reason for the second CANedge log:** `xteraw` showed
`F0:0000000000000000` -- all zeros -- at the same time as `xte=-0.33m`. Since
`xteraw` records only the last PGN 65535 frame seen, and both `SA=0xF0` and
`SA=0x1C` have been observed on that PGN here, the likely explanation is **two
senders on PGN 65535, one emitting zeros and one emitting real XTE**. That also
fits the alternation seen on screen. Do not treat this as established -- it is
a hypothesis with a clean test: the CANedge capture will show both source
addresses and their payloads directly.

Note the earlier no-GPS period is also in the log, where PGN 65535 carried all
zeros from every sender. Position/speed come from PGNs 65267/65256; the
NMEA2000 PGNs (129025/129026/129283) stayed at 0 throughout.

### #18's reconnect watchdog fired for the first time (end of session)

The plough control had to be disconnected from the ISOBUS partway through the
CANedge logging. That severed our end of the bus while the terminal stayed up,
and the VT and TC watchdogs added for #18 engaged -- the first time they have
ever been exercised on hardware:

```
E] [VT]: Status Timeout
E] [TC]: Server Status Message Timeout. The TC may be offline.
VT: disconnected 10s after having been connected -- forcing reconnect attempt 1
TC: disconnected 105s after having been connected -- forcing restart 7
VT: ... reconnect attempt 13        TC: ... forcing restart 8
```

**What this does and does not show.** It shows the watchdogs detect the loss and
keep retrying on their own, with the partner records still valid
(`partner addr=0x26 valid=Y`, `0xF7 valid=Y`) -- no wedge, no silent give-up,
and the retry counters climbing steadily (VT to 13, TC to 8) rather than
stalling. That is genuinely more than we knew before.

It does **not** show recovery. Our CAN connection was physically removed, so
there was nothing on the bus to reconnect *to*, and the board was powered down
before it was plugged back in -- the serial port disappeared entirely at the end
of the session. #18's actual question, whether a connection re-establishes by
itself after a transient loss, is therefore still open. The next session can
settle it cheaply and deliberately: with everything connected and working, pull
the ISOBUS connector for ~15 s, plug it back in without touching power, and
watch whether VT and TC return on their own. Do not close #18 until that is
seen.

### Two further ground-truth points, ours unpaired

The operator called **a steady 21 cm** (with a short dedicated CANedge log taken
at that value) and then **11 cm on the other side** at the end of the run. Our
side was not captured for either: by then the link was dropping, so there is no
Triton reading to pair them against.

They are still worth having, because both are inside the CANedge capture. The
second one crosses the line -- the earlier confirmed pairs were -6 and -33, both
negative, so 11 cm on the other side is the first opposite-sign point on this
brand and the one that pins the sign convention for the JD layout.

### Board state left behind

**The Teensy is still running run 2's firmware: DDI 505 = `0x01`, structure
label TC04.** It was left that way deliberately so the CANedge capture records
one consistent configuration. The *source* is reverted to `0`/TC03 before merge,
so source and board disagree until the next flash -- reflash before drawing any
conclusion from the board.

### Open after session 8

- **Next experiment, and it is now well-posed:** declare the full Level 1
  required set -- 505, 506, 507, 508, 509, 510, 511, 515 -- in a single device
  element, and see whether `no compatible implements detected` clears. That is
  the direct test of the explanation above.

  **Built 2026-09-08 evening, structure label TC05, awaiting a rig.** DDI 505
  now declares `0x01`, and all eight DDIs sit in the Ploughbody function
  element. 506 and 508-511 are Settable (TC writes them to us); 507 and 515 we
  report, statically, because the plough runs no tramline logic. What to read
  on the tractor:
  - `Tramline setpoint (DDI 506):` -- any value, 0 included, means the terminal
    completed the handshake. That is the headline result.
  - `Guidance track (DDI 508-511):` -- new line. Populated track numbering
    means the terminal speaks this part of the protocol even if 506 stays
    silent, which is a separate and useful answer.
  - The Tramline screen itself: does `no compatible implements detected` clear?
  - Watch `SetValueAndAcknowledgeCommand (0x0A)` as well as `ValueCommand
    (0x03)` -- with TC v4+ on both ends the values move to the acknowledged
    PGN, and counting only 0x03 would read as a false negative.

  Order of rigs: John Deere first (it is the one that gave us the explicit
  error message, so it is the only one that can clearly confirm the fix), then
  Ag Leader and Raven.
- Note the honesty question this raises, which is now a real decision rather
  than a probe: declaring the full Level 1 set means claiming the implement
  calculates tramline tracks. A plough does not. Whether Triton should present
  itself as a tramline implement at all is a product question to settle before
  building this out -- the probe answered "the terminal would talk to us if we
  did", not "we should".
- Watch for `SetValueAndAcknowledgeCommand (0x0A)` as well as `ValueCommand
  (0x03)`: the research doc warns that TC version 4+ on both ends moves values
  to the acknowledged PGN, and watching only 0x03 could produce a false
  negative. Worth confirming our counters cover both before the next run.
- **CANedge logs: analysed the same evening -- see the follow-up section
  below.** Both are now in `canlogs/` with their provenance. They confirmed the
  XTE decode against both unpaired ground-truth calls, disproved the two-sender
  hypothesis on PGN 65535, and turned up two firmware bugs. Note the plough
  control was already off the bus in both, so there is no Triton traffic in
  them and "does the TC ever address us" cannot be answered from these files.
- #20's Ag Leader layout still needs session 7's capture; unchanged today.

## Follow-up (off-tractor) -- 2026-09-08 evening: the CANedge logs, analysed

The two full-bus captures taken during Session 8 are now in
[`canlogs/`](canlogs/) with their provenance (2026-09-08, John Deere rig,
owner van Os -- the files' own RTC timestamps are wrong and carry none of
this). They settle the session's open question, and they turn up two real
firmware bugs that no serial log could have shown.

**Card `AD4F266A` session 24** -- 678 s, 232 578 frames, indoors, GPS module
connected partway through with no reception.
**Card `AD4F266A` session 25** -- 115 s, 45 419 frames, outdoors, RTK live,
recorded across the operator's ground-truth calls.

Triton is **not** on the bus in either file -- the plough control had already
been unplugged (see "#18's reconnect watchdog", above). Every conclusion below
is read off the vendors' own traffic.

### Who is on this bus

Session 24 caught a complete address-claim roll-call (eleven control
functions, all manufacturer code 33). The ones that matter:

| SA | Function | Identity | Role |
|---|---|---|---|
| `0x1C` | 23 (Navigation), industry group 0 | 254686 | **the GPS module** |
| `0x26` | 29, Virtual Terminal | 938640 | display |
| `0x2A` | 32, industry group 2 | 938640 | guidance, *inside the display* |
| `0xF7` | 130, Task Controller | 938640 | display |
| `0xFB` | 130, Task Controller | 625309 | a **second** TC, different device |
| `0xF0` | 134 | 224876 | tractor ECU, by far the loudest talker |

`0x2A` shares identity 938640 with the VT and TC, so the John Deere guidance
source is a function *of the display*, not of the GPS receiver. `0x1C` -- the
module physically connected at t = 462 s in session 24 -- is a separate device
and is the one broadcasting position and speed.

### The XTE sender goes silent when there is no fix

This is what session 24 is for, and it is a clean negative result:

| PGN | Session 24 (no reception) | Session 25 (RTK) |
|---|---|---|
| 65535 from `0x2A` (XTE, decoded) | **0 frames in 678 s** | 688 frames |
| 65267 position + 65256 speed from `0x1C` | 1080 each | 574 each |
| 129025 / 129026 / 129283 (NMEA2000) | **absent** | **absent** |

So without a fix, `0x2A` does not send the XTE message at all -- it does not
send zeros, it sends nothing. And the NMEA2000 guidance set is simply not
broadcast on this brand, in either log, confirming from the bus side what
Session 8 saw as permanently-zero counters. On a John Deere the legacy PGNs
are all there is.

### The XTE decode is right, and the sign convention is now pinned

Session 8 left two operator ground-truth calls unpaired, because our board was
already dropping off the bus. Session 25 contains both, and the legacy John
Deere decoder reproduces them exactly:

| Log time | Payload from `0x2A` | raw word | `(raw-32000)>>1` | Operator called |
|---|---|---|---|---|
| 0 -- 85 s | `77 15 10 D6 7C 59 89 FF` | 31958 | **-21 cm** | a steady 21 cm |
| 90 -- 115 s | `77 15 10 16 7D 3F 89 FF` | 32022 | **+11 cm** | 11 cm, other side |

(raw word being `d[4] << 8 | d[3]`.) `d[1] = 0x15`, so the high-nibble quality
check yields quality 4 on both. Between them the value walks through 0, +2,
+5, +8 cm -- the line crossing, sample by sample.

That is the third and fourth ground-truth confirmation of this decoder, and
the first pair of **opposite signs**: the sign convention holds, negative and
positive being the two sides of the line.

**The receiver's own position confirms it independently.** PGN 65267 from
`0x1C` shows the rig stationary within 0.20 m N-S / 0.36 m E-W for 88 s at
**53.4260360 N, 6.7205691 E**, then stepping **0.38 m** at t = 88.7 s -- the
same instant the XTE walks 0 -> +2 -> +5 -> +8 -> +11 (t = 87.89 to 88.70 s).
A 32 cm cross-track change out of a 0.38 m displacement is a shift about 30
degrees off perpendicular to the AB line. So this was a static test with the
rig nudged once, not a driving pass, and the decoded XTE tracks measured
motion rather than only the operator's spoken numbers.

Note this does **not** mean the layout is per-brand. Issue #20's closing
analysis established the sharper version: it is the same message *definition*
everywhere, and what differs is the message selector in `data[0]` -- see
below. On the Ag Leader rig the XTE message is simply not on the bus at all.
On John Deere we do have an XTE feed, over the old proprietary PGN rather than
NMEA2000, but a real one.

### The two-sender hypothesis is wrong -- it was our own diagnostic

Session 8 hypothesised two senders on PGN 65535, one emitting zeros, to
explain `xteraw=F0:0000000000000000` printing while `xte=-0.33m` was live.
The capture disproves it. There are **four** senders on PGN 65535 --
`0x1C` (36 Hz), `0x2A` (6 Hz), `0xD2` (1 Hz) and `0xF0` (64 Hz) -- and
**not one of them ever emits an all-zero payload**, in either log.

The zeros are ours. In `DecodeLegacyXteJohnDeere()`, `result.lengthOk` is set
*before* the source-address filter, and the early return for a non-John-Deere
sender leaves `result.rawPayload` at its default zeros.
`OnLegacyXteJohnDeere()` then copies that buffer under `if (result.lengthOk)`,
while setting `lastXteJohnDeereLegacySourceAddress` **unconditionally**, for
every sender. `0xF0` talks on this PGN ten times as often as `0x2A`, so the
payload diagnostic is being wiped to zeros almost continuously and stamped
with whichever address spoke last. It is showing an artifact, not the bus.

The fix is to write the raw payload only when the sender passed the address
filter. Note the original motivation for this diagnostic -- capturing Ag
Leader's `0x80` bytes to derive its layout -- was dropped when #20 closed, so
this is now a readout-correctness issue rather than a blocking one.

### `0x2A` is itself multiplexed on byte 0, and we ignored it -- issue #30, now fixed

Independently found and filed the same day from card sessions 7/8/9
(**issue #30**); sessions 24/25 corroborate it and add the ground-truth pair
above. **Fixed** -- see the selector check in `DecodeLegacyXteJohnDeere()`.
`0x2A` does not send one message on PGN 65535 -- it alternates two, keyed by
byte 0:

    77 15 10 D6 7C 59 89 FF     ~5 Hz    the XTE message
    92 FF 80 3E FC FF FF FF     ~1 Hz    something else entirely

`DecodeLegacyXteJohnDeere()` has no byte-0 check. It accepts the `0x92` frame,
reads its raw word as 64574, and returns `valid = true` with
`xteHundredthsMeter = +16287` and, because `d[1] = 0xFF` fails the quality
nibble, `quality = 0`. `OnLegacyXteJohnDeere()` then calls
`SetXte(16287, 0)` on it unconditionally.

So on a live John Deere bus, roughly once a second:

- `GuidanceSource::xte` is overwritten with **+162.87 m**,
- `quality` is knocked to 0, so `IsRtkQuality()` goes false and
  `InterfacePlough`'s Hold branch trips,
- and `lastXteFix` is refreshed anyway, so the 2000 ms staleness guard beside
  it can never fire on this.

It went unnoticed in the serial logs because the good message outnumbers the
bad one five to one, so the 1 Hz periodic line usually samples a correct
value. The fix is a byte-0 selector check (`d[0] == 0x77`) before decoding,
keeping the existing quality-nibble check, and placed *after* the raw
diagnostics so the selector gates what we believe rather than what we can see.

The regression test was written first and confirmed failing against the
unfixed decoder (`legacyXteJohnDeere_nonXteMessageSelector_notDecoded`,
1 failed / 95). Four pre-existing John Deere fixtures had to gain the selector
byte: they were **synthetic minimal frames**, built by zeroing every byte the
decoder does not read (`data[0]`, `data[2]`, `data[5..7]` all `0x00`, and
`data[1]` `0x10` where the bus sends `0x15`), so their `data[0] = 0x00` never
represented real hardware. `CanSerialParser`'s CAN_XTE path has the same
missing check but is **deliberately left alone**: it sits behind a
CAN-to-serial bridge whose handling of `data[0]` we have no capture of.

### `kPgnXteTrimbleLegacy` (60160) is the transport-protocol data PGN

PGN 60160 = `0xEB00` is **TP.DT**, J1939's Transport Protocol Data Transfer.
Every multi-frame message on any ISOBUS bus is carried on it -- session 24
logged 5384 of them, most being `0xF0`'s object-pool upload to the VT.

The decode itself is safe: `DecodeLegacyXteTrimble()` rechecks for source
address `0xAA` and drops everything else, and no device on this bus uses that
address. Two consequences that are not safe, though:

- `counters.xteTrimbleLegacy` counts every TP data frame on the bus, so the
  debug readout's Trimble count and its contribution to `Total()` are
  meaningless noise on a busy bus.
- `IsobusGuidanceChannel.cpp:160` issues
  `request_parameter_group_number(kPgnXteTrimbleLegacy, ...)`, i.e. it asks a
  control function to transmit the transport-protocol data PGN on request.
  That request has no meaning.

Worth recording that the legacy Trimble CAN ID `0x1CEBACAA` was therefore a
raw TP.DT frame (SA `0xAA`, DA `0xAC`) -- which is why its decoder keys on
`data[0] == 2 && data[5] == 7`: byte 0 is a TP sequence number.


## Session 9 -- 2026-09-09 (John Deere, then Ag Leader InCommand 1200) -- the full DDI set on two brands

First rig test of the full Tramline Control Level 1 DDI set (#21). Firmware:
`feat/21-full-tramline-ddop`, structure label **TC05**, all eight DDIs (505,
506, 507, 508, 509, 510, 511, 515) in the Ploughbody function element. Both
AgIsoStack vendor patches verified present before the build; 104/104 native
tests pass. The pool as built is snapshotted in
[`pools/2026-09-09/`](pools/2026-09-09/).

### Caught on the bench, before the rig: an over-long designator

The DDI 508 designator was `Unique A-B Guidance Reference Line ID` -- 37
characters against a 32-character limit -- and AgIsoStack warned about it on
every boot. Shortened to `Unique A-B Guidance Ref Line ID` (31) before leaving.

Worth doing first rather than noting afterwards: an over-long designator is a
pool a terminal may legitimately reject, and a rejected pool looks exactly like
the tramline handshake failing. Left in place it would have made a negative
result uninterpretable, which is the failure mode session 7 already spent two
runs on.

### John Deere: the TC engages with the DDOP for the first time

The pool was accepted, and the terminal's Tramline screen no longer reported
the session-8 error (operator report; the wording was ambiguous over the radio,
so the CANedge log is the record).

`DDI 506` never arrived and `Guidance track (DDI 507-511)` stayed empty. But the
status dump showed something new and initially alarming:

```
Value commands (any DDI): 0
Value requests (any DDI): 6750986        <- ~20 000/s
```

20 000/s is impossible on a 250 kbit bus, so the number is not bus traffic. It
is not garbage either. AgIsoStack invokes the request-value callback from five
places in `isobus_task_controller_client.cpp`: once for a genuine bus request in
`process_queued_commands()`, and four times for its *own* re-polling of
measurement commands -- time interval, maximum threshold, minimum threshold and
on-change threshold -- which it evaluates on every `update()`.

The important part is where those measurement lists come from. They are
populated **only** inside the received-message handler, under
`ProcessDataCommands::MeasurementTimeInterval` /
`MeasurementMaximumWithinThreshold` / `MeasurementMinimumWithinThreshold` /
`MeasurementChangeThreshold` (lines 1611-1734). They cannot fill on their own.
So a non-zero poll rate means **the John Deere TC sent us measurement
commands** -- the first evidence in this whole effort that a Task Controller has
actively configured reporting on our DPDs, rather than merely accepting the
pool.

**CONFIRMED from the CANedge log (session 26), 2026-09-09 evening.** The
inference was correct, and the capture is sharper than it: it names the DDI.
Decoding PGN `0xCB00` by command, element and DDI gives, once each:

```
0xF7 -> 0x81   element 2   DDI 515   MeasurementChangeThreshold
0xF7 -> 0x81   element 2   DDI 515   RequestValue
0x81 -> 0xF7   element 2   DDI 515   Value            <- we answered
```

So the John Deere Task Controller set a change-threshold measurement command on
**DDI 515 (Track Control State)**, asked for its value, and got one. That is
the first genuine Process Data exchange with a Task Controller in the project's
history, and it is independent of AgIsoStack's counters entirely -- it is on
the wire.

Two things follow. **The DDOP is being read, not merely accepted**: a TC does
not configure reporting on a DPD it ignored. And **DDI 515 is the one it
picked**, which is exactly the DDI whose direction was corrected from
implement-reported to `Settable` on 2026-09-08 after reading the AEF guideline
(sec 2.2.1 defines it against DDI 160, whose entry requires the "setable"
property and the on-change trigger). Had it shipped as first written, the TC
could not have written it and this exchange could not have happened.

Still absent: any DDI 506, and any of 507-511. The terminal engages with the
pool but does not complete the tramline handshake.

### The pool is accepted and activated -- confirmed on the bus, both brands

Worth establishing separately, because it removes a whole class of explanation
for the missing 506. The full ISO 11783-10 device-descriptor handshake is in
both captures, and **every response carries error code 0x00**:

```
0x81 -> 0xF7   sub=0  Request structure label
0xF7 -> 0x81   sub=1  Structure label
0x81 -> 0xF7   sub=4  Request object pool transfer
0xF7 -> 0x81   sub=5  ...RESPONSE                errorCode=0x00
               sub=6  [the pool transfer itself]
0xF7 -> 0x81   sub=7  Transfer RESPONSE          errorCode=0x00, size=540
0x81 -> 0xF7   sub=8  Object pool activate
0xF7 -> 0x81   sub=9  Activate RESPONSE          errorCode=0x00, faultyObject=65535 (none)
```

John Deere at t = 78.3 s (session 26), Ag Leader at t = 161.4 s and again after
each reflash at 359.2 s and 561.0 s (session 28).

So the pool is **not** being rejected on either brand. It transfers, is accepted
and is activated without error, and the DDI 515 measurement command shows it is
being read rather than merely stored. Whatever stops DDI 506 arriving, it is not
a malformed or refused DDOP.

Note the transfer response reports **size = 540**, not the 541 the board's own
dump contains -- the TC's own accounting independently confirming the
version-3 regeneration described in `pools/2026-09-09/README.md`.

**Decoding note:** in Device Descriptor messages (command 1) the high nibble of
byte 0 is the **sub-command**, not element-number bits. A generic Process Data
decode that treats it as an element will report nonsense element numbers like
4080 or 452 for these frames.

### Both TC diagnostics are misleading, in opposite directions

- **`Value commands: 0`** reads zero even while the TC is commanding us,
  because measurement commands never reach `add_value_command_callback`.
- **`Value requests: 6750986`** counts AgIsoStack's internal polling, so it
  cannot answer "did the TC ask us anything?" -- the question it was added for.

Together they made a genuine first contact look like another null result. Both
want fixing before the next session, or the next reading will mislead again.

### Also confirmed on John Deere

- **#30's selector fix works on live hardware.** XTE read correctly throughout
  (`0.34 m`, quality 4, `IsRtkQuality=Y`) with no sign of the `+162.87 m`
  spikes the unguarded decoder produced roughly once a second.
- **#37's course and altitude decode works**: `4.1 m` altitude, `27.1 deg`
  course, alongside a real position.
- The `[TP] ... global destination, ignoring` warnings appeared again. Still not
  ours, as established in session 8.

### The VT object pool label was stale -- found and fixed on the rig

The operator noticed the Ag Leader was showing a screen **with no Calibrate
button**, while the firmware has had one since `1bb3295`.

Cause: `set_object_pool(..., "MW03")` was last bumped on 2026-08-10, and the
pool changed twice afterwards (`1bb3295` adding the Calibrate soft key, and
`359f618`) without a bump. A terminal compares only the label; on a match it
skips the upload entirely (`VT Server has a matching label ... upload will be
skipped`). So the change was **silent** -- nothing errored, the terminal simply
kept serving a weeks-old screen. Every terminal that had ever cached MW03 was
affected, John Deere included.

Bumped to **MW04** and reflashed on the spot. Confirmed working:

```
I] [VT]: VT Server has a label for MW03   . This version will be deleted.
I] [VT]: No version label from the VT matched. Client will upload the pool and store it instead.
I] [VT]: Delete Version Response OK!
I] [VT]: Stored object pool with no error.
```

The lesson matches the DDOP's TC0x discipline exactly, and is now written
beside the label: **bump MW0x on every change to `VT3PoolData`.** This also
retro-qualifies earlier sessions -- any VT behaviour observed between
2026-08-10 and today was observed against whatever pool that terminal had
cached, not necessarily the one in the source.

### Issue #31 confirmed on hardware, and it recovers

The Calibrate soft key was pressed. It works -- the wizard starts -- and, as #31
predicts, **the ISOBUS stack stops being fed while it runs**. First hardware
confirmation of that issue, which until now had been reasoned from the source
only. The stack came back on its own once the wizard was left, so the stall is
bounded by the wizard's lifetime rather than being a permanent wedge.

### Ag Leader: no XTE on PGN 65535, and two VTs on the bus

`XTE fix age` sat at 107-146 s (i.e. never) while PGN 65535 from `0x80` streamed
steadily -- payloads `510302FF10060DFF`, `510301FF0B0009FF` and neighbours, all
with `data[0] = 0x51`, which #30's selector check correctly refuses. Position,
speed, course and altitude all decode fine (`53.442055 / 6.755811`, `4.5 m`,
`56.8 deg`); only cross-track is missing. That is **issue #42** exactly: on this
brand the XTE is somewhere else, and this session narrows it by confirming the
`0x51` frames are not it.

Operator ground truth called out for that hunt, unpaired on our side because we
never decode a value: **26 cm, then 5 cm on the other side, then 53-54 cm.** The
sign change is the useful part. All of it is inside this session's CANedge
capture, which also recorded **the MW04 object pool upload itself** -- a
complete VT pool transfer on the bus, useful independently of #42.

Note the VT partner **changed between dumps**: `0x80` at VT version 3 before the
reflash, `0x26` at VT version 4 after. There are two VTs on this bus (as in
session 5), and which one we partner with is not fixed. Worth pinning down
before trusting any per-terminal VT observation here.

### Open after session 9

- ~~Confirm the measurement-command inference from the CANedge log~~ --
  **done 2026-09-09**, confirmed on DDI 515; see above.
- ~~Fix both TC counters~~ -- **done 2026-09-09 evening.** The fix counts PGN
  0xCB00 messages *addressed to us* straight off the wire, through a raw
  network-manager callback registered alongside the TC client rather than
  through it. That independence is the point: the old numbers were computed
  downstream of the very code whose behaviour they were meant to report, which
  is the same shape as session 5's `vtstat` mistake. The classification itself
  is now a pure function in `IsobusPgnDecode` so it can be tested; 7 tests pin
  it, including the real DDI 515 frame.

  Replayed over the two captures, the new readout would have shown at a glance
  what took an evening of log analysis to establish:

  | | John Deere (s26) | Ag Leader (s28) |
  |---|---|---|
  | Requests for our values | **1** | 0 |
  | Measurement commands | **1** -- DDI 515, type 8 | **0** |
  | Set-value commands | 0 | 0 |
  | Other, addressed to us | 5 (the DDOP handshake) | 18 (handshake x3) |

  **And it exposes a brand difference that was previously invisible: the Ag
  Leader TC accepts and activates our pool, then never addresses us again.**
  Zero requests, zero measurement commands, zero set-values in 842 s. The John
  Deere engages; the Ag Leader does not. Any theory about the missing DDI 506
  has to account for both behaviours, and they are not the same behaviour.
- **Wider/Narrower**: the presses arrive (9 logged this session) and are
  consumed through `ConsumeWiderPress()` into `InterfacePlough::Update()`, so
  the wiring is intact, but no effect was visible on the Ag Leader.

  **The capture explains that completely, and with two causes rather than one.**
  Guidance was invalid (quality 0, `IsRtkQuality=N`) *and* the machine never
  reached the speed threshold: peak ground speed for the whole session was
  **0.38 m/s** against `MINSPEED` of **0.5 m/s**, so `MinSpeed()` was false
  throughout. Both interlocks held the control path independently, so the
  observation says nothing about the actuator either way. To test it properly,
  drive above **1.8 km/h** with an RTK fix -- on the John Deere guidance was
  already valid, so speed is the only missing piece there.

  **The hitch cycle is in the capture and it is a clean one**: rear hitch
  (PGN 65093) held at 99.6% for 594 s, then a smooth descent to **1.2%** at
  t = 611.8 s, **9.6 s on the ground**, and back to 100% by t ~ 650 s. 179
  distinct positions at 10 Hz. Front hitch never moved. The four short movement
  runs in the log all fall outside that window, which matches the operator's own
  account. That is the first hard confirmation that 65093 is the honest
  "implement out of work" signal the John Deere inventory argued it would be.
- #21 still has no DDI 506 and no track numbers on either brand, even with the
  full Level 1 set declared and the TC demonstrably talking to us.
- **#42: answered, negatively.** The session 28 capture rules out every
  candidate -- PGN 65535 from `0x80` carries only selector `0x51` across all
  7962 frames; PGN 129283 appears as **4 zero-length frames** (`DLC=0` verified
  in the raw MF4, so not a decode artifact); PGNs 65512/65513 are static
  constants; 44032 carries curvature, not cross-track. Cross-track error is not
  broadcast on that bus at all, while the display shows it throughout. Full
  write-up in
  `NeptuneGPS Documentation/ISOBUS/research/agleader-cnh-bus-inventory-2026-09-09.md`.

  **One lead left before calling it final, and it is the operator's own:**
  PGN 45056 `0xAD00` (Agricultural Guidance System Command) has **zero frames
  on either rig**, so autosteer was engaged in neither session, and nobody
  requested 129283 either -- those four empty frames are unsolicited. That fits
  an ECU announcing a PGN it currently has nothing to put in. **Engage autosteer
  and re-capture.** If 129283 gains a payload the fix is free, because
  `DecodeXteNmea2000()` already handles it with no source-address filter.

- **The "Ag Leader rig" is two vendors.** Manufacturer 94 = CNH Industrial (the
  tractor: `0x26`, `0xAC`, `0xCD`, `0xF0`), 97 = Ag Leader (the kit: `0x2B`,
  `0x80`, `0xE9`, `0xF5`, `0xF7`). That is the source of the two VTs and of the
  partner switching between dumps -- one VT per vendor.

- **We announce ourselves as manufacturer 1407, Open-Agriculture** -- AgIsoStack's
  own code, not a MeijWorks one. Every terminal we join sees that. Needs a
  decision: register a code with the AEF, or keep 1407 knowingly.

- **The John Deere bus is unchanged between sessions 24/25 and 26.** Every
  device reappears and the only control function new today is `0x81`, us. The
  10 Hz tractor-ECU set is identical, so that inventory is a property of the
  machine rather than of one afternoon.
---

*Historical note: this file absorbed the standalone `TCGEO_Field_Test_Log.md`
on 2026-08-10 (Bos content folded into Session 3 above, van Mastwijk content
became this Session 4 placeholder) -- that file no longer exists separately.
`NeptuneGPS Documentation/ISOBUS/research/ISOBUS_TC_Manufacturer_Comparison.md` (TC-GEO-per-brand licensing research)
and `NeptuneGPS Documentation/ISOBUS/research/EndOfObjectPool_ErrorBitmask_Research.md` (decodes the Session 3 VT
rejection's "bitmask value 9" against the actual ISO 11783-6 text) remain
separate, standalone reference docs.*
