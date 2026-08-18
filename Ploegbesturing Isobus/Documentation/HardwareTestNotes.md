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
morning (`Documentation/ISOBUS_TC_Manufacturer_Comparison.md`) into how each
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
§C.2.5 directly (see `Documentation/EndOfObjectPool_ErrorBitmask_Research.md`
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
ISOBUS_TC_Manufacturer_Comparison.md` for the licensing research this was
prompted by.

## Session 4 -- 2026-08-10 (afternoon, van Mastwijk) -- PLANNED, not yet run

**Rig:** second visit to the same site as Session 1 (Ag Leader InCommand
1200, CNH tractor -- Case IH/New Holland/Steyr TBC; all three share the same
PLM/AFS backend per `ISOBUS_TC_Manufacturer_Comparison.md`, so behavior
should be consistent regardless of badge).

**Why this session matters:** the InCommand 1200 is the one display in the
Ag Leader lineup that ships with UT *and* TC standard, TC-GEO included, no
unlock purchase required (unlike the InCommand 800, which needs a paid
unlock for both, or Compass, which never gets TC at any price) -- see
`ISOBUS_TC_Manufacturer_Comparison.md` sec 3. This makes van Mastwijk the
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
      per `Documentation/EndOfObjectPool_ErrorBitmask_Research.md` that's a
      "method/attribute not supported" complaint about the WorkingSet object
      specifically -- try `VTObjectPool.cpp`'s new `VT_WORKINGSET_BISECT_VARIANT`
      switch (0=production, 1=selectable=false, 2=no language codes,
      3=language "en" instead of "nl"; ships as 0, no behavior change until
      set), one variant per flash, watching `IsobusDebugMenu`'s VT state line
      to see if any variant gets past `WaitForEndOfObjectPoolResponse`. The
      AgIsoStack vendor patch documented in `Documentation/
      AgIsoStackVendorPatches.md` (#2) will print the decoded error bit
      directly in the serial log if it still fails, instead of a raw number.

**Results:** Two terminals present on-site: an Ag Leader display and CNH's own
built-in VT (AFS Pro/IntelliView) -- tested against CNH specifically (Ag
Leader was powered off throughout, to keep results attributable to one VT;
our partner filter matches on function code alone and can't otherwise
distinguish which VT it binds to when more than one is live). CNH's own
stack unlocks TC-BAS+TC-SC+TC-GEO together via one dealer activation code
(different licensing shape than Ag Leader's, see
`ISOBUS_TC_Manufacturer_Comparison.md` sec 3) -- so this ended up testing a
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

---

*Historical note: this file absorbed the standalone `TCGEO_Field_Test_Log.md`
on 2026-08-10 (Bos content folded into Session 3 above, van Mastwijk content
became this Session 4 placeholder) -- that file no longer exists separately.
`ISOBUS_TC_Manufacturer_Comparison.md` (TC-GEO-per-brand licensing research)
and `EndOfObjectPool_ErrorBitmask_Research.md` (decodes the Session 3 VT
rejection's "bitmask value 9" against the actual ISO 11783-6 text) remain
separate, standalone reference docs.*
