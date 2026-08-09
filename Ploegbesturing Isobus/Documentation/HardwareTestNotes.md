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
5. **`NanoLibcCompat.cpp`** (a `swprintf` linker shim) was suspected of being unnecessary/invented, deleted, and re-tested: link genuinely fails without it under `TEENSY_OPT_SMALLEST_CODE` (`--specs=nano.specs`). Confirmed load-bearing, restored. Open follow-up (tracing exactly what pulls in the wide-locale facet) filed as [GitHub issue #13](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/13).
6. **`OnLegacyPosition` never decodes lat/lon at all** -- it only stamps the GGA fix-age timer. Debug dump shows `Lat/Lon: 0.000000/0.000000` even with a fresh (<100 ms) fix age. **Not fixed** -- a comment in the code claims this matches old `VehicleGps` behavior, but the sibling legacy decoder (`CanSerialParser.cpp`'s `CAN_POS` case) *does* decode+set position, so that claim looks inaccurate. Needs a decision: does anything actually need live coordinates out of this path before adding the decode?

**Still open at end of session 1:**
- XTE now updates (fix age goes fresh) after the source-address fix, but reads an implausible **167.67 m**, and `Quality` stays **0** (the quality byte `d[1]` isn't matching the expected `0x15`). Added raw diagnostics (`word=`, `byte1=` on the `PGN 65535` debug line) to read the real values next session.
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
