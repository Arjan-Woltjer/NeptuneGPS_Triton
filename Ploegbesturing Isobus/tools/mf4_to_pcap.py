#!/usr/bin/env python3
"""Convert CANedge (CSS Electronics) MF4 CAN logs to a SocketCAN pcap for Wireshark.

Written for docs/isobus-jd-gps-objectpool-capture-2026-09-07.md: recover the
VT object pool a John Deere GPS unit uploads to the Virtual Terminal from a
CANedge3 log. Wireshark's built-in ISO 11783 dissectors ("isobus", "isobus.vt")
do the Transport Protocol reassembly and the object-pool decode; this script
only produces a pcap they can read, and optionally prints an ISOBUS overview
(address claims with decoded NAMEs, TP/ETP sessions, object-pool candidates) so
the right source/destination pair can be found quickly.

Usage:
    mf4_to_pcap.py LOG.MF4 [MORE.MF4 ...] -o out.pcap [--channel N] [--summary]

    --channel N   only frames from CANedge bus channel N (1 = CAN1, 2 = CAN2)
    --summary     print the ISOBUS overview to stdout after converting
    --backend     mdf_iter (CSS Electronics' own reader, handles unfinalized
                  files straight off the SD card) | asammdf | auto (default:
                  mdf_iter if installed, else asammdf)

Input must be plain .MF4. Compressed (.MFC) or encrypted (.MFE) CANedge logs
must first be converted with CSS Electronics' free "mdf2finalized" tool.

Dependencies: `pip install mdf_iter` (recommended) or `pip install asammdf`.
The pcap writer itself has no dependencies.

Frames show up as plain "CAN" rather than "ISOBUS" until the dissector is
selected explicitly. Wireshark 4.6.8 ships packet-isobus.c but registers no
CAN *heuristic* for it, so there is nothing to switch on under Analyze >
Enabled Protocols: right-click a frame > Decode As... > CAN next-level >
ISOBUS, or on the command line use `-d can.subdissector=isobus` -- a SINGLE
"=" after the table name, as "==" fails with a misleading "Unknown protocol"
error. Filter on `isobus.vt` for Virtual Terminal traffic.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Iterable, Iterator, Optional

# ---------------------------------------------------------------------------
# Frame model
# ---------------------------------------------------------------------------


@dataclass
class Frame:
    timestamp: float          # seconds since Unix epoch (UTC)
    can_id: int               # 11- or 29-bit identifier, no flag bits
    extended: bool
    data: bytes
    channel: int = 1          # CANedge bus channel (1-based)
    fd: bool = False
    brs: bool = False
    rx: bool = True


# ---------------------------------------------------------------------------
# MF4 readers
# ---------------------------------------------------------------------------


def _read_with_mdf_iter(path: str) -> Iterator[Frame]:
    import mdf_iter  # type: ignore

    with open(path, "rb") as handle:
        mdf = mdf_iter.MdfFile(handle)
        df = mdf.get_data_frame()

    if df.empty:
        return

    # Index is a tz-aware datetime64 (UTC). Columns per CSS Electronics' API:
    # BusChannel, ID, IDE, DLC, DataLength, Dir, EDL, BRS, DataBytes.
    columns = set(df.columns)

    def col(name: str, default):
        return df[name].to_numpy() if name in columns else None

    ts = df.index.to_numpy().astype("datetime64[ns]").astype("int64") / 1e9
    ids = df["ID"].to_numpy()
    ide = col("IDE", None)
    edl = col("EDL", None)
    brs = col("BRS", None)
    direction = col("Dir", None)
    channel = col("BusChannel", None)
    data_bytes = df["DataBytes"].to_numpy()
    data_length = col("DataLength", None)

    for i in range(len(df)):
        raw = data_bytes[i]
        length = int(data_length[i]) if data_length is not None else len(raw)
        yield Frame(
            timestamp=float(ts[i]),
            can_id=int(ids[i]) & 0x1FFFFFFF,
            extended=bool(ide[i]) if ide is not None else int(ids[i]) > 0x7FF,
            data=bytes(raw[:length]),
            channel=int(channel[i]) if channel is not None else 1,
            fd=bool(edl[i]) if edl is not None else False,
            brs=bool(brs[i]) if brs is not None else False,
            rx=(int(direction[i]) == 0) if direction is not None else True,
        )


def _read_with_asammdf(path: str) -> Iterator[Frame]:
    from asammdf import MDF  # type: ignore

    with MDF(path) as mdf:
        start = mdf.header.start_time
        if start.tzinfo is None:
            start = start.replace(tzinfo=timezone.utc)
        epoch = start.timestamp()

        for group_index, group in enumerate(mdf.groups):
            names = {ch.name for ch in group.channels}
            if "CAN_DataFrame" not in names:
                continue

            sig = mdf.get("CAN_DataFrame", group=group_index, raw=True)
            fields = sig.samples.dtype.names or ()

            def column(name: str):
                full = f"CAN_DataFrame.{name}"
                return sig.samples[full] if full in fields else None

            ids = column("ID")
            data_bytes = column("DataBytes")
            if ids is None or data_bytes is None:
                continue
            ide = column("IDE")
            dlc = column("DLC")
            data_length = column("DataLength")
            edl = column("EDL")
            brs = column("BRS")
            direction = column("Dir")
            channel = column("BusChannel")

            for i in range(len(sig.timestamps)):
                raw_id = int(ids[i])
                if data_length is not None:
                    length = int(data_length[i])
                elif dlc is not None:
                    length = _dlc_to_length(int(dlc[i]))
                else:
                    length = len(data_bytes[i])
                yield Frame(
                    timestamp=epoch + float(sig.timestamps[i]),
                    can_id=raw_id & 0x1FFFFFFF,
                    extended=bool(ide[i]) if ide is not None else (raw_id & 0x80000000 != 0 or raw_id > 0x7FF),
                    data=bytes(data_bytes[i][:length]),
                    channel=int(channel[i]) if channel is not None else 1,
                    fd=bool(edl[i]) if edl is not None else False,
                    brs=bool(brs[i]) if brs is not None else False,
                    rx=(int(direction[i]) == 0) if direction is not None else True,
                )


_FD_DLC = {9: 12, 10: 16, 11: 20, 12: 24, 13: 32, 14: 48, 15: 64}


def _dlc_to_length(dlc: int) -> int:
    return dlc if dlc <= 8 else _FD_DLC.get(dlc, 64)


def _have(module: str) -> bool:
    try:
        __import__(module)
        return True
    except ImportError:
        return False


def read_frames(path: str, backend: str) -> Iterator[Frame]:
    if backend == "mdf_iter":
        return _read_with_mdf_iter(path)
    if backend == "asammdf":
        return _read_with_asammdf(path)
    if backend != "auto":
        sys.exit(f"Unknown backend {backend!r}")

    # auto: mdf_iter understands CANedge files exactly as they come off the SD
    # card, but yields nothing for MF4 bus logs written by other tools (e.g. an
    # asammdf export) -- fall back to asammdf in that case.
    have_mdf_iter, have_asammdf = _have("mdf_iter"), _have("asammdf")
    if not (have_mdf_iter or have_asammdf):
        sys.exit("Neither mdf_iter nor asammdf is installed: pip install mdf_iter")

    def frames() -> Iterator[Frame]:
        produced = 0
        if have_mdf_iter:
            for frame in _read_with_mdf_iter(path):
                produced += 1
                yield frame
        if produced == 0 and have_asammdf:
            if have_mdf_iter:
                print(f"{path}: mdf_iter found no CAN frames, retrying with asammdf", file=sys.stderr)
            yield from _read_with_asammdf(path)

    return frames()


# ---------------------------------------------------------------------------
# pcap writer (LINKTYPE_CAN_SOCKETCAN = 227)
# ---------------------------------------------------------------------------

LINKTYPE_CAN_SOCKETCAN = 227
CAN_EFF_FLAG = 0x80000000
CANFD_BRS = 0x01
CANFD_FDF = 0x04


class PcapWriter:
    """Minimal pcap (not pcapng) writer, microsecond timestamps."""

    def __init__(self, handle):
        self.handle = handle
        self.count = 0
        handle.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, LINKTYPE_CAN_SOCKETCAN))

    def write(self, frame: Frame) -> None:
        # Per the tcpdump LINKTYPE_CAN_SOCKETCAN definition the ID/flags word is
        # big-endian; the classic frame is 16 bytes, the FD frame 72 bytes.
        can_id = frame.can_id | (CAN_EFF_FLAG if frame.extended else 0)
        if frame.fd or len(frame.data) > 8:
            payload = frame.data[:64].ljust(64, b"\x00")
            flags = CANFD_FDF | (CANFD_BRS if frame.brs else 0)
            record = struct.pack(">IBBH", can_id, len(frame.data), flags, 0) + payload
        else:
            payload = frame.data.ljust(8, b"\x00")
            record = struct.pack(">IBBH", can_id, len(frame.data), 0, 0) + payload

        seconds = int(frame.timestamp)
        micros = int(round((frame.timestamp - seconds) * 1_000_000))
        if micros >= 1_000_000:
            seconds += 1
            micros -= 1_000_000
        self.handle.write(struct.pack("<IIII", seconds, micros, len(record), len(record)))
        self.handle.write(record)
        self.count += 1


# ---------------------------------------------------------------------------
# ISOBUS overview (optional, --summary)
# ---------------------------------------------------------------------------

PGN_ADDRESS_CLAIM = 0xEE00
PGN_TP_CM = 0xEC00
PGN_TP_DT = 0xEB00
PGN_ETP_CM = 0xC800
PGN_ETP_DT = 0xC700
PGN_ECU_TO_VT = 0xE700
PGN_VT_TO_ECU = 0xE600
PGN_REQUEST = 0xEA00

VT_FUNCTION_NAMES = {
    0x11: "Object Pool Transfer",
    0x12: "End of Object Pool",
    0xC0: "Get Memory",
    0xC2: "Get Number of Soft Keys",
    0xC3: "Get Text Font Data",
    0xC7: "Get Hardware",
    0xD0: "Store Version",
    0xD1: "Load Version",
    0xD2: "Delete Version",
    0xD3: "Get Versions",
    0xFE: "VT Status",
    0xFF: "Working Set Maintenance",
}

# Best-effort labels for NAME manufacturer codes. The authoritative list is the
# ISOBUS/SAE manufacturer code registry (isobus.net); verify before relying on
# any of these -- the raw code is always printed alongside.
MANUFACTURER_NAMES = {
    89: "John Deere (verify)",
}

FUNCTION_NAMES = {
    29: "Virtual Terminal",
    130: "Task Controller",
}


def decode_pgn(can_id: int) -> tuple[int, int, int, int]:
    """Return (priority, pgn, destination, source) for a 29-bit ID.

    PDU1 (PF < 240): PS is a destination address and not part of the PGN.
    PDU2 (PF >= 240): PS is a group extension, destination is global (0xFF).
    """
    priority = (can_id >> 26) & 0x7
    pf = (can_id >> 16) & 0xFF
    ps = (can_id >> 8) & 0xFF
    sa = can_id & 0xFF
    dp = (can_id >> 24) & 0x3
    if pf < 240:
        return priority, (dp << 16) | (pf << 8), ps, sa
    return priority, (dp << 16) | (pf << 8) | ps, 0xFF, sa


@dataclass
class Name:
    raw: int

    @property
    def identity_number(self) -> int:      return self.raw & 0x1FFFFF
    @property
    def manufacturer_code(self) -> int:    return (self.raw >> 21) & 0x7FF
    @property
    def ecu_instance(self) -> int:         return (self.raw >> 32) & 0x7
    @property
    def function_instance(self) -> int:    return (self.raw >> 35) & 0x1F
    @property
    def function(self) -> int:             return (self.raw >> 40) & 0xFF
    @property
    def device_class(self) -> int:         return (self.raw >> 49) & 0x7F
    @property
    def device_class_instance(self) -> int: return (self.raw >> 56) & 0xF
    @property
    def industry_group(self) -> int:       return (self.raw >> 60) & 0x7
    @property
    def self_configurable(self) -> bool:   return bool(self.raw >> 63)

    def describe(self) -> str:
        manufacturer = MANUFACTURER_NAMES.get(self.manufacturer_code, "")
        function = FUNCTION_NAMES.get(self.function, "")
        return (
            f"NAME=0x{self.raw:016X} manufacturer={self.manufacturer_code}"
            f"{' ' + manufacturer if manufacturer else ''}"
            f" function={self.function}{' ' + function if function else ''}"
            f" fn_inst={self.function_instance} ecu_inst={self.ecu_instance}"
            f" class={self.device_class}/{self.device_class_instance}"
            f" ig={self.industry_group} identity={self.identity_number}"
        )


# ---------------------------------------------------------------------------
# PGN names
# ---------------------------------------------------------------------------

# Sourced from Wireshark's own ISOBUS dissector (packet-isobus.c) by running
# it over the card session 24 and 25 captures and collecting every name it
# produced -- not hand-written, and not guessed. Regenerate with:
#
#   tshark -r LOG.pcap -d can.subdissector=isobus -O isobus #     | grep -E "^    PGN: " | sort -u
#
# NOTE the decode-as syntax: a SINGLE "=" after the table name. Wireshark
# 4.6.8 ships the ISOBUS dissector but registers no CAN *heuristic* for it, so
# frames show as plain "CAN" until it is selected explicitly. Any advice to
# "enable the ISOBUS heuristic under Analyze > Enabled Protocols" does not
# apply to this build -- there is no such heuristic to enable.
#
# Hand-guessing this table is a trap worth recording: an earlier pass inferred
# 0xFE00-0xFE0F as "auxiliary valve N measured position" from the neighbouring
# 0xFE10-0xFE1F block, and the dissector shows 0xFE0A is actually "Tractor
# control command tractor response". The ranges are not symmetrical.

PGN_NAMES = {
    0x0ab00: "File Server to Client (ECU) message",
    0x0ac00: "Agricultural Guidance Machine Info",
    0x0b100: "Proprietarily Configurable Message #1",
    0x0cb00: "Process Data Message",
    0x0d000: "Background lighting level command",
    0x0d700: "Binary Data Transfer",
    0x0d800: "Memory Access Response",
    0x0d900: "Memory Access Request",
    0x0e600: "Virtual Terminal-to-Node",
    0x0e700: "Node-to-Virtual Terminal",
    0x0e800: "Acknowledgment Message",
    0x0ea00: "Request",
    0x0eb00: "Transport Protocol - Data Transfer",
    0x0ec00: "Transport Protocol - Connection Mgmt",
    0x0ee00: "Address Claimed",
    0x0ef00: "Proprietary A",
    0x0f004: "Electronic Engine Controller 1",
    0x0f022: "Machine Selected Speed",
    0x0fab3: "AUTOSAR Time Synchronization",
    0x0fd02: "All implements stop operations switch state",
    0x0fd07: "Direct Lamp Control Command 1",
    0x0fd32: "ECU diagnostic protocol",
    0x0fdcc: "Operators External Light Controls Message",
    0x0fe0a: "Tractor control command tractor response",
    0x0fe0d: "Working Set Master ",
    0x0fe0f: "Language command",
    0x0fe11: "Auxiliary valve 1 estimated flow",
    0x0fe12: "Auxiliary valve 2 estimated flow",
    0x0fe13: "Auxiliary valve 3 estimated flow",
    0x0fe14: "Auxiliary valve 4 estimated flow",
    0x0fe1a: "Auxiliary valve 10 estimated flow",
    0x0fe1e: "Auxiliary valve 14 estimated flow",
    0x0fe1f: "Auxiliary valve 15 estimated flow",
    0x0fe41: "Lighting command",
    0x0fe43: "Primary or Rear Power Take off Output Shaft",
    0x0fe44: "Secondary or Front Power Take off Output Shaft",
    0x0fe45: "Primary or Rear Hitch Status",
    0x0fe46: "Secondary or Front Hitch Status",
    0x0fe48: "Wheel-based Speed and Distance",
    0x0fe49: "Ground-based Speed and Distance",
    0x0fe56: "Aftertreatment 1 Diesel Exhaust Fluid Tank 1 Information 1",
    0x0fe68: "Vehicle Fluids",
    0x0feae: "Air Supply Pressure",
    0x0feca: "Active Diagnostic Trouble Codes",
    0x0fee5: "Engine Hours, Revolutions",
    0x0fee6: "Time/Date",
    0x0fee8: "Vehicle Direction/Speed",
    0x0fee9: "Fuel Consumption (Liquid) 1",
    0x0feee: "Engine Temperature 1",
    0x0feef: "Engine Fluid Level/Pressure 1",
    0x0fef2: "Fuel Economy (Liquid)",
    0x0fef3: "Vehicle Position 1",
    0x0fef7: "Vehicle Electrical Power 1",
    0x0fef8: "Transmission Fluids 1",
    0x0fefc: "Dash Display 1",
    0x1ef00: "Proprietary A2",
    0x1f010: "System Time",
}


def pgn_name(pgn: int) -> str:
    """Return a human-readable name for a PGN, or "unknown"."""
    return PGN_NAMES.get(pgn, "unknown")


@dataclass
class Session:
    protocol: str            # "TP", "TP-BAM", "ETP"
    source: int
    destination: int
    pgn: int
    size: int
    first_seen: float
    first_byte: Optional[int] = None
    data_frames: int = 0
    bytes_seen: int = 0


@dataclass
class Summary:
    frames: int = 0
    extended: int = 0
    first_ts: Optional[float] = None
    last_ts: Optional[float] = None
    claims: dict = field(default_factory=dict)       # (channel, sa) -> (ts, Name)
    pgn_counts: dict = field(default_factory=dict)   # pgn -> count
    device_pgns: dict = field(default_factory=dict)  # (channel, sa) -> {pgn: count}
    sessions: list = field(default_factory=list)
    open_sessions: dict = field(default_factory=dict)  # (channel, sa, da) -> Session

    def observe(self, frame: Frame) -> None:
        self.frames += 1
        self.first_ts = frame.timestamp if self.first_ts is None else min(self.first_ts, frame.timestamp)
        self.last_ts = frame.timestamp if self.last_ts is None else max(self.last_ts, frame.timestamp)
        if not frame.extended:
            return
        self.extended += 1

        _, pgn, da, sa = decode_pgn(frame.can_id)
        self.pgn_counts[pgn] = self.pgn_counts.get(pgn, 0) + 1
        per_device = self.device_pgns.setdefault((frame.channel, sa), {})
        per_device[pgn] = per_device.get(pgn, 0) + 1
        d = frame.data

        if pgn == PGN_ADDRESS_CLAIM and len(d) >= 8:
            self.claims[(frame.channel, sa)] = (frame.timestamp, Name(int.from_bytes(d[:8], "little")))
        elif pgn == PGN_TP_CM and len(d) >= 8:
            control = d[0]
            enclosed = d[5] | (d[6] << 8) | (d[7] << 16)
            if control == 16:    # RTS -- destination-specific session
                self._open("TP", frame, sa, da, enclosed, d[1] | (d[2] << 8))
            elif control == 32:  # BAM -- broadcast session
                self._open("TP-BAM", frame, sa, 0xFF, enclosed, d[1] | (d[2] << 8))
            elif control in (19, 255):  # EndOfMsgAck / Abort
                self._close(frame.channel, da, sa)
        elif pgn == PGN_ETP_CM and len(d) >= 8:
            control = d[0]
            enclosed = d[5] | (d[6] << 8) | (d[7] << 16)
            if control == 20:    # ETP RTS
                self._open("ETP", frame, sa, da, enclosed, int.from_bytes(d[1:5], "little"))
            elif control in (23, 255):  # ETP EndOfMsgAck / Abort
                self._close(frame.channel, da, sa)
        elif pgn in (PGN_TP_DT, PGN_ETP_DT) and len(d) >= 2:
            session = self.open_sessions.get((frame.channel, sa, da))
            if session is not None:
                session.data_frames += 1
                session.bytes_seen += len(d) - 1
                if session.first_byte is None:
                    session.first_byte = d[1]
                if session.bytes_seen >= session.size:
                    self._close(frame.channel, sa, da)

    def _open(self, protocol: str, frame: Frame, sa: int, da: int, pgn: int, size: int) -> None:
        key = (frame.channel, sa, da)
        self._close(*key)
        session = Session(protocol, sa, da, pgn, size, frame.timestamp)
        self.open_sessions[key] = session
        self.sessions.append(session)

    def _close(self, channel: int, sa: int, da: int) -> None:
        self.open_sessions.pop((channel, sa, da), None)

    def print(self, out=sys.stdout) -> None:
        def fmt_ts(ts: Optional[float]) -> str:
            if ts is None:
                return "-"
            return datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3] + "Z"

        print(f"frames: {self.frames} ({self.extended} extended-ID)   "
              f"span: {fmt_ts(self.first_ts)} .. {fmt_ts(self.last_ts)}", file=out)

        print("\naddress claims (PGN 0xEE00):", file=out)
        if not self.claims:
            print("  none -- was logging running before the device powered on?", file=out)
        for (channel, sa), (ts, name) in sorted(self.claims.items()):
            print(f"  ch{channel} SA=0x{sa:02X} ({sa:3d}) at {fmt_ts(ts)}  {name.describe()}", file=out)

        print("\ntransport-protocol sessions (TP.CM RTS / BAM, ETP.CM RTS):", file=out)
        if not self.sessions:
            print("  none", file=out)
        for s in self.sessions:
            marker = ""
            if s.pgn == PGN_ECU_TO_VT:
                fn = VT_FUNCTION_NAMES.get(s.first_byte, f"function 0x{s.first_byte:02X}" if s.first_byte is not None else "?")
                marker = f"  <-- ECU to VT: {fn}"
                if s.first_byte == 0x11:
                    marker += "  *** OBJECT POOL ***"
            elif s.pgn == PGN_VT_TO_ECU:
                marker = "  <-- VT to ECU"
            complete = "complete" if s.bytes_seen >= s.size else f"{s.bytes_seen}/{s.size} bytes seen"
            print(f"  {fmt_ts(s.first_seen)} {s.protocol:6s} SA=0x{s.source:02X} -> DA=0x{s.destination:02X}"
                  f"  PGN=0x{s.pgn:05X}  size={s.size}  ({s.data_frames} data frames, {complete}){marker}", file=out)

        print("\ntop PGNs:", file=out)
        for pgn, count in sorted(self.pgn_counts.items(), key=lambda kv: -kv[1])[:15]:
            print(f"  0x{pgn:05X} ({pgn:6d}): {count}", file=out)

    def print_inventory(self, out=sys.stdout) -> None:
        """Per-device message inventory: who is on the bus, and what each sends.

        A different question from print(): not "what happened in this log" but
        "what does this machine offer us". Rates are the useful part -- a 10 Hz
        tractor-ECU broadcast is a live signal worth consuming, a 0.1 Hz one is
        housekeeping.
        """
        duration = 0.0
        if self.first_ts is not None and self.last_ts is not None:
            duration = self.last_ts - self.first_ts
        span = duration if duration > 0 else 1.0

        print(f"\ndevice inventory  ({duration:.0f} s, {self.frames} frames)", file=out)
        print("PGN names come from Wireshark's ISOBUS dissector -- see PGN_NAMES.", file=out)

        for (channel, sa) in sorted(self.device_pgns):
            counts = self.device_pgns[(channel, sa)]
            claim = self.claims.get((channel, sa))
            identity = claim[1].describe() if claim else "no address claim seen in this log"
            print(f"\n  SA 0x{sa:02X} ({sa:3d})  ch{channel}  {sum(counts.values())} frames", file=out)
            print(f"    {identity}", file=out)
            for pgn, count in sorted(counts.items(), key=lambda kv: -kv[1]):
                print(f"      0x{pgn:05X} {pgn:6d}  {count:7d}  {count / span:6.1f}/s"
                      f"  {pgn_name(pgn)}", file=out)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def convert(inputs: Iterable[str], output: Optional[str], backend: str, channel: Optional[int],
            summary: Optional[Summary]) -> int:
    handle = open(output, "wb") if output else None
    try:
        writer = PcapWriter(handle) if handle is not None else None
        count = 0
        for path in inputs:
            before = count
            for frame in read_frames(path, backend):
                if channel is not None and frame.channel != channel:
                    continue
                if writer is not None:
                    writer.write(frame)
                count += 1
                if summary is not None:
                    summary.observe(frame)
            print(f"{path}: {count - before} frames", file=sys.stderr)
    finally:
        if handle is not None:
            handle.close()
    return count


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("inputs", nargs="+", metavar="LOG.MF4")
    parser.add_argument("-o", "--output", help="pcap file to write (optional when only analysing)")
    parser.add_argument("--channel", type=int, help="only this CANedge bus channel (1 = CAN1, 2 = CAN2)")
    parser.add_argument("--backend", choices=("auto", "mdf_iter", "asammdf"), default="auto")
    parser.add_argument("--summary", action="store_true", help="print an ISOBUS overview")
    parser.add_argument("--inventory", action="store_true",
                        help="print a per-device message inventory (who is on the bus, "
                             "what each sends, and at what rate)")
    args = parser.parse_args(argv)

    if not args.output and not (args.summary or args.inventory):
        parser.error("nothing to do: pass -o/--output, and/or --summary / --inventory")

    summary = Summary() if (args.summary or args.inventory) else None
    total = convert(args.inputs, args.output, args.backend, args.channel, summary)
    if args.output:
        print(f"wrote {total} frames to {args.output}", file=sys.stderr)
    if args.summary:
        summary.print()
    if args.inventory:
        summary.print_inventory()
    return 0


if __name__ == "__main__":
    sys.exit(main())
