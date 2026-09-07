# Capturing a John Deere GPS unit's VT object pool from the ISOBUS — 2026-09-07

Notes from a design discussion about whether `Ploegbesturing Isobus` (Triton
plough control) can observe a John Deere GPS unit joining the bus and recover
the object pool that unit uploads to the Virtual Terminal (VT). Written so the
capture can be attempted the next day from a CANedge3 log without re-deriving
any of this.

`file:line` anchors are against commit `062d073`.

## Summary

- **Detecting the JD unit joining** is cheap and already almost supported by the
  firmware: Address Claim (PGN 60928) is a global broadcast, and the same
  `NAMEFilter` pattern used for the VT partner today applies directly.
- **Capturing the object pool in the firmware itself** is real new work: the
  upload is a destination-specific Transport Protocol session between the JD
  ECU and the VT, the AgIsoStack build in use most likely only reassembles
  sessions addressed to its own control function, and no VT-object decoder
  exists in the repo (only an encoder for Triton's own pool).
- **Capturing it from the CANedge3 log on a PC is the recommended route.** The
  logger is a passive, promiscuous recorder, so every frame of the ECU→VT
  session is already in the `.MF4` file. Wireshark ships an ISO 11783 dissector
  and a dedicated ISO 11783-6 VT dissector that do the multi-packet reassembly
  and the object-pool decode. The only glue to write is MF4 → pcap.
- **A GPS receiver has no dedicated contract with the Task Controller.** It
  broadcasts standard GNSS PGNs (plus JD-proprietary ones) that the TC, which
  in the JD case lives inside the display, consumes for georeferencing. See
  the last section.

## 1. What the bus looks like

CAN is a broadcast medium. Every frame the JD receiver puts on the wire — its
address claim and its object pool upload — physically reaches every node on
the same segment, including Triton's Teensy 4.1 CAN controller and a CANedge3
tapped onto the bus.

The relevant traffic, in order of appearance when the JD unit powers up:

| Step | PGN | Addressing | Notes |
|---|---|---|---|
| Address Claim | 60928 (0xEE00) | global broadcast | Carries the 64-bit NAME: manufacturer code (John Deere has its own registered SAE code), function code, device class, ECU instance. This is the "JD GPS joined the bus" event. |
| VT discovery | VT Status (from VT), Working Set Maintenance (from ECU) | broadcast / to VT | ECU waits for a VT Status message, then starts talking to that VT's address. |
| Object Pool Transfer | 0xE700 "ECU to VT" (59136), VT function byte 0x11, wrapped in TP or ETP | **destination-specific**, ECU → VT | Pools over 8 bytes go through Transport Protocol (TP.CM 0xEC00 / TP.DT 0xEB00), larger ones through Extended TP (ETP.CM 0xC800 / ETP.DT 0xC700). The frames are addressed to the VT but are still visible to everyone. The VT answers on 0xE600 "VT to ECU". |

The object-pool payload itself is defined in ISO 11783-6 as a serialized list
of VT objects (working set, data masks, soft key masks, containers, output
fields, …). It is a published binary format, which is what makes offline
reconstruction feasible; a Task Controller DDOP by contrast is much harder to
interpret from a sniff alone.

## 2. Option A — capture in the Triton firmware

What exists today in `Ploegbesturing Isobus/lib/PloegbesturingCore/src/isobus/`:

- `IsobusVtInterface.cpp:73-75` builds a `NAMEFilter` on
  `NAME::Function::VirtualTerminal` and a `PartneredControlFunction` from it.
  The identical pattern with a filter on manufacturer code and/or a GPS /
  steering function code would recognise the JD unit's address claim and let
  the firmware log "JD ECU claimed address X with NAME Y".
- `IsobusGuidanceChannel.cpp:123-130` registers PGN callbacks through
  `CANNetworkManager::add_any_control_function_parameter_group_number_callback`,
  which already delivers traffic not addressed to Triton's own control
  function. Every PGN registered there today is a single-frame *global*
  broadcast (129025, 129026, 129283, the legacy JD/Trimble PGNs, AISO).
- `VTObjectPool.cpp` only *encodes* Triton's own pool for
  `VirtualTerminalClient::set_object_pool`. There is no decoder.

What is missing:

1. **Foreign-session reassembly.** Whether AgIsoStack's transport protocol
   manager will track a TP/ETP session whose destination is another node (the
   VT) rather than one of its own internal control functions is unverified for
   the pinned `AgIsoStack-Arduino#0.1.5` fork, and most ISOBUS stacks do not,
   since buffering other nodes' point-to-point transfers is wasted RAM on a
   normal ECU. If it does not, a hand-rolled TP.CM/TP.DT/ETP.CM/ETP.DT state
   machine keyed on `(source = JD ECU, destination = VT)` would be needed,
   bypassing the stack's session layer for this one purpose.
2. **A VT object decoder**, the reverse of `VTObjectPool.cpp`.
3. **Somewhere to put the bytes.** Pools can be tens of kilobytes; the Teensy
   has no filesystem in this project and EEPROM is far too small. The only
   realistic sink is streaming hex/base64 out over the existing `Serial` debug
   channel for offline reconstruction.

This is closer to building a lightweight bus analyser into the firmware than
extending an existing feature. It is feasible, but Option B gets the same
result with none of that work.

## 3. Option B — reconstruct from the CANedge3 log (recommended)

The CANedge3 is a passive logger, not a stack participant. It timestamps and
stores every frame it sees regardless of source or destination address, so
the objection in Option A item 1 does not apply: the full ECU→VT session is in
the `.MF4` file verbatim, provided the logger was on the ISOBUS segment (250
kbit/s, 29-bit extended IDs, which the CANedge3 handles as part of its normal
J1939 support) at the moment the JD unit joined and uploaded its pool.

Pipeline:

1. **Extract raw frames from the MF4.** Use CSS Electronics' free tooling:
   the `asammdf` GUI/Python API, or their `mdf_iter` / `canedge_browser`
   Python packages. The CAN frame table has arbitration ID, DLC, data bytes,
   timestamp and channel. No ISOBUS-specific step yet.
2. **Convert the frame stream into something Wireshark reads.**
   `Ploegbesturing Isobus/tools/mf4_to_pcap.py` does this: it reads the MF4
   (via `mdf_iter`, falling back to `asammdf`) and writes a SocketCAN pcap
   (link type 227). With `--summary` it also prints every address claim with
   its decoded NAME, every TP/ETP session with source, destination and
   enclosed PGN, and flags the "ECU to VT / Object Pool Transfer" session
   explicitly, so the addresses for step 4 come straight out of the log:

   ```
   pip install mdf_iter
   python mf4_to_pcap.py 00000001.MF4 00000002.MF4 -o jd.pcap --channel 1 --summary
   ```

   Verified against a synthetic MF4 (address claims, a TP object-pool upload,
   NMEA broadcasts): the pcap round-trips and the pool bytes reassemble
   exactly. The `mdf_iter` path could not be exercised on a real CANedge file
   here; if it prints "found no CAN frames" on tomorrow's log, the asammdf
   fallback kicks in automatically.
3. **Open it in Wireshark.** Wireshark's built-in `packet-isobus.c` (ISO 11783
   transport layer, PGN identification, address claim / NAME decode) and
   `packet-isobus-vt.c` (ISO 11783-6 Virtual Terminal) reassemble the TP/ETP
   session and decode the Object Pool Transfer payload into its VT objects in
   the packet detail pane. Concurrent sessions from other ECUs are
   demultiplexed by (source, destination) automatically, which is the main
   reason to use the maintained dissector instead of a home-grown one.
4. **Find the right session.** Identify the JD unit's source address from its
   Address Claim (NAME manufacturer code), identify the VT's address the same
   way (function code VirtualTerminal), then filter on that pair and on the
   `isobus` / `isobus.vt` protocol tree.

Checklist for tomorrow:

- [ ] CANedge3 on the ISOBUS segment (not only the tractor's internal bus),
      correct bit rate, passive tap, no extra termination.
- [ ] Logging running *before* the JD unit powers on — the pool is uploaded
      once per VT connection, at join time.
- [ ] Pull the `.MF4` files (plain MF4; `.MFC`/`.MFE` need CSS's
      `mdf2finalized` first), run `tools/mf4_to_pcap.py --summary`, open the
      pcap in Wireshark. If frames show as plain "CAN", enable the ISOBUS
      heuristic under Analyze > Enabled Protocols or use Decode As.
- [ ] Note the JD unit's claimed address and NAME for later use in the
      firmware's own detection (Option A, step 1).

## 4. What a GPS unit gives the Task Controller

The GPS receiver's object pool is a *VT* object pool (ISO 11783-6): it exists
so the operator can configure the receiver and see satellite status on the
terminal. It has nothing to do with the Task Controller.

The Task Controller (ISO 11783-10) has its own, separate notion of an object
pool: the Device Descriptor Object Pool (DDOP), which is uploaded by
*implements* (TC clients) to describe their geometry, sections and process
data. A GPS receiver is not an implement, does not upload a DDOP, and is not a
TC client.

There is therefore no request/response contract between a GPS receiver and
the TC comparable to the VT object pool or the DDOP. What the receiver does is
**broadcast** GNSS data on the bus, and the TC consumes those broadcasts:

| PGN | Content | Format |
|---|---|---|
| 129025 | Position, Rapid Update (lat/lon) | single frame, ~10 Hz |
| 129026 | COG & SOG, Rapid Update | single frame |
| 129029 | GNSS Position Data (full fix: lat/lon/alt, fix type, HDOP, satellites, corrections) | fast-packet, multi-frame |
| 126992 / 129033 | System Time / Time & Date | single frame |
| 129539 / 129540 | GNSS DOPs / Satellites in View | fast-packet |

These are the ISO 11783-7 application-layer messages borrowed from NMEA 2000.
The first three are exactly what `IsobusGuidanceChannel.cpp:36-38` already
subscribes to. Speed can also come from the tractor ECU as wheel-based or
ground-based speed (PGN 65096 / 65097) or as Machine Selected Speed
(PGN 61474), and the TC will typically prefer those for distance-based
logging.

Two consequences worth keeping in mind:

- **Cross-track error is not a GPS output.** PGN 129283 (XTE), which
  `IsobusGuidanceChannel.cpp:38` also consumes, is produced by the *guidance*
  computer — the display or steering controller that owns the guidance line —
  not by the receiver. In the JD case the StarFire receiver is the GNSS source
  and the GreenStar / Gen4 display is the guidance computer *and* the Task
  Controller. The receiver and the display talk partly over standard PGNs and
  partly over JD-proprietary ones (`kPgnXteJohnDeereLegacy`, PGN 0xFFFF from
  source address 0x2A, `IsobusGuidanceChannel.cpp:48-51`, is one of those).
  So "what the GPS sends to the TC" is, on a JD tractor, largely internal to
  the display and vendor-specific.
- **Geometry lives on both sides.** The TC applies the GPS antenna offset,
  which is configured on the display / tractor side, to the implement geometry
  that arrives in the implement's DDOP (device element offsets, DDI 134/135/136
  X/Y/Z, connector type). From Triton's perspective the only contract that
  matters is implement ↔ TC; the TC ↔ GPS side is not something an implement
  can see or influence beyond reading the same broadcasts.

## References

- CSS Electronics, CANedge3 J1939 logging and MF4 tooling:
  <https://canlogger.csselectronics.com/canedge-getting-started/ce3/record-data/j1939-data/>,
  <https://www.csselectronics.com/pages/mdf4-decoders-dbc-mf4-parquet-csv>
- Wireshark ISOBUS dissectors:
  <https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-isobus.c>,
  <https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-isobus-vt.c>,
  display filter reference <https://www.wireshark.org/docs/dfref/i/isobus.vt.html>
- AgIsoStack++ (upstream of the pinned Arduino fork):
  <https://github.com/Open-Agriculture/AgIsoStack-plus-plus>
