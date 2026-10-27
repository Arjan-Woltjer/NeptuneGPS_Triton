# Session 21 (van Mastwijk, 2026-10-27): CANedge AD4F266A capture analysis

Inputs: CANedge AD4F266A sessions 96, 97 and 98 (OneDrive `CANedge logs/bucket-AD4F266A/`; channel 1 =
ISOBUS), `2026-10-27_session21_vanmastwijk_serial.log` and `..._timeline.md`. Read with `mdf_iter` and
`NeptuneGPS Documentation/ISOBUS/tools/mf4_to_pcap.py`; the ad-hoc scripts are not kept. Answers the three
capture questions in `session21_vanmastwijk_handover.md` ("For the desktop").

All times are **bus clock, CET** (MF4 timestamp converted to Europe/Amsterdam). Bus and serial clock agree
to about 0.05 s today (the 1200's End of Object Pool response 11:37:48.962 bus, `Stored object pool`
11:37:48.9 serial; the plough's address claim at the 11:35:34 reboot is at 11:35:34.8 bus).

## 0. Coverage

| CANedge session | Bus time | Frames |
|---|---|---|
| 96 | 11:24:42.5 - 12:30:06.2 | 1 454 918 |
| 97 | 12:30:20.6 - 12:51:18.6 | 383 207 |
| 98 | 12:51:22.4 - 13:09:59.99 (pieces 1-4) | 408 104 |

Session 98's last piece (13:10 - ~13:12, ISOBUS unplugged) had not been uploaded when this was written;
the AUX test (12:57 - 13:09) is covered. Bus check passes: VT 0x26, VT 0x80, TC 0xF7 and the plough 0x81
all present.

Roster (address claims): plough **0x81** (NAME A0048400AFE00001), its lightbar CF **0xDC**
(A00083000C21887E), CNH VT 0x26 (A0001D080BC23101), the InCommand 1200's CFs 0x80 (VT), 0xF7 (TC), 0xF5,
0xE9, 0x2B, CNH 0xAC and 0xCD (these two and 0xDC re-claim about every 2 s: the roll-call of session 13), and
the **armrest = tractor ECU 0xF0** (A00086000BC9F19F, manufacturer 94, function 134).

## 1. AUX-N between the 1200 and the armrest (question a)

### What is on the bus

- **The armrest sends only Auxiliary Input Type 2 Maintenance (0xE700 function 0x23)**, to global, 10 per
  second, all day: `23 55 00 01 ff ff ff ff` = model identification 85, status **1 (ready)**. Status 0
  (initialising) only for ~2 s whenever the 1200 boots (12:12:14, 12:36:51, 12:51:39, 13:06:42) and
  while the 1200 was off (13:03:49 - 13:06:38).
- **No Auxiliary Input Type 2 Status (0x26)** from the armrest in sessions 96-98, also not during the
  button presses (A2 ~12:10-12:13, the long presses ~13:09).
- **No Auxiliary Assignment Type 2 command (0x24) and no Input Status Enable (0x25)** to anyone, and no
  Auxiliary Capabilities request (0x27). Nothing on this rig was AUX-N assigned during the session.
- The plough's **Preferred Assignment (0x22) is empty** (`22 00 ...`, zero input units). The 1200 answers
  `22 00 ff ff ...` (no error) at 12:57:31.6 and 13:06:35.6; the CNH VT answers `22 10 ...`
  (bit 4, "any other error") at 12:53:02.0 and 13:04:01.6.

So the 1200 refused the assignment in its own UI ("broken link") without sending anything on the bus.

### The armrest's input types are on the bus after all

At every 1200 boot the armrest asks for its stored pool: the 1200 answers Get Versions with one version,
**`NHA42nl`**, the armrest loads it from the 1200's NVM, and then sends a **535-byte Object Pool Transfer**
(0xE700 function 0x11, TP to 0x80; identical at 12:12:15, 12:36:52, 12:51:40 and 13:06:43). That
transfer replaces 36 Object Pointers (type 27), 27 Auxiliary Control Designator Type 2 objects (type 33,
pointer type 1) and **16 Auxiliary Input Type 2 objects (type 32)**: the input definitions, with their
function types.

| Inputs (object ids) | Type | Single assignment |
|---|---|---|
| 28100-28106, 28109-28111 (10) | **2, Boolean momentary** | yes |
| 28112, 28113 (2) | 3, Analogue return to 50 % | no |
| 28115-28118 (4) | 0, Boolean latching | no |

No input has the critical-control bit. **The armrest does have ten Boolean momentary inputs**, the type
of MW11's BREDER and SMALLER (`VTObjectPool.cpp`, `appendAuxFunction2`: type 2, not critical, no
restriction, not single assignment). The handover's working explanation, "no armrest input is compatible
with Boolean momentary", is ruled out, and the Auxiliary Capabilities request is no longer needed to learn
the input types.

Open, not answerable from the bus: why the 1200 shows a broken link. The momentary inputs are all
single-assignment, so one possibility is that the 1200 holds them as assigned to functions of a working
set that is not on the bus (stored assignments do not show on the bus until their working set connects).
The two analogue and four latching inputs are not single-assignment.

### The armrest in the whole archive

A scan of every session in `bucket-AD4F266A` (1-98) for the armrest's NAME:

- The armrest is on the bus in sessions 10, 28, 31, 40, 43 and 55-98. **No Auxiliary Input Type 2 Status
  (0x26), no Auxiliary Assignment (0x24) and no Auxiliary Capabilities (0x27) in any of them.** AUX-N has
  never been used on this rig while the logger ran.
- **Full pool uploads to the 1200**: session 56 (2026-10-13 20:24:42 - 20:27:14, four transfers of
  4 470 + 543 + 98 459 + 146 bytes, End of Object Pool error 0, stored as `NHA42nl`) and session 88
  (2026-10-22 09:28, the log ends during the 98 kB transfer). Every other connection is a Load Version.
- Session 56's pool reassembles to **103 614 bytes, 504 objects**, and `validate_iop.py` walks it cleanly
  to the last byte. It contains **no AUX objects** (no type 29-33): the armrest's 16 AUX-N inputs only
  exist through the 535-byte add-on above. The add-on is accepted each time: the armrest closes it with
  End of Object Pool and the 1200 answers error 0 (12:12:16.17, 12:36:53.05, 12:51:41.48, 13:06:45.04).
- Today the add-on came 6 s after Load Version at the 12:51 and 13:06 connections. At the others it came
  minutes later: 11:54:11 -> 12:12:15 (during A1/A2, the 1200's AUX screen) and 12:30:34 -> 12:36:52.
  Each time the armrest's maintenance status drops to 0 (initialising) for ~2 s, then back to 1.
- Side note: after the accidental 1200 power-off (11:52:46) the armrest asked Get Versions every second
  from 11:53:44, and the 1200's 9-byte TP answer timed out 21 times (abort reason 3, sent by 0x80)
  before it got through at 11:54:10.8.

## 2. The pool uploads with DPO 255 (question b)

The plough sends one ETP transfer per upload (no aborts, no missing or repeated packets), with each DPO
equal to the CTS the VT granted: AgIsoStack's DPO 255 caps at the receiver's CTS, as intended in #269.

| Upload | Bus time | Pool | Bytes | CTS / DPO | Windows | Duration | VT response |
|---|---|---|---|---|---|---|---|
| 1200 (0x80), failover | 11:37:47.337 - 11:37:48.940 | MW10 | 13 199 | 50 / 50 (last 36) | 38 | 1.60 s | EOP error 0 at 11:37:48.962, Store Version OK 11:37:48.968 |
| CNH VT (0x26), 6dd5f2c boot | 12:52:59.946 - 12:53:01.886 | MW11 | 13 223 | 30 / 30 (last 29) | 63 | 1.94 s | EOP error 0 at 12:53:01.994, Store Version OK 12:53:02.117 |
| 1200 (0x80), VOORK. VT 1 | 12:57:30.003 - 12:57:31.598 | MW11 | 13 223 | 50 / 50 (last 39) | 38 | 1.60 s | EOP error 0 at 12:57:31.614, Store Version OK 12:57:31.620 |

MW11 is 24 bytes larger than MW10: the two Auxiliary Function Type 2 objects (12 bytes each). The two MW11
uploads differ in content because AgIsoStack scales the pool to each VT's data mask. Every other
connection today was a Load Version from NVM (`d1 ... 00`, no error).

## 3. The 123.7 ms loop pass at the 12:53 boot (question c)

**Most likely the Teensy USB serial transmit timeout, not the ISOBUS work.**

- On the bus the boot is ordinary: claim 12:52:57.195, Working Set Master 12:52:59.892, the MW11 upload
  above. The plough's frames never pause more than 14 ms during the upload. The two longer pauses are
  protocol waits: 550 ms after the claim (AgIsoStack start-up) and 105 ms waiting for the VT's End of
  Object Pool response.
- Teensy 4 core `usb_serial.c`: the transmit side has 4 buffers (`TX_NUM 4`). When the USB is enumerated
  but no program reads the port, the queued transfers never complete. The next write then waits
  `TX_TIMEOUT_MSEC` = **120 ms** once and sets `transmit_previous_timeout`, after which writes are
  dropped at once until a host reads again. 120 ms plus one normal ~3.7 ms pass = 123.7 ms.
- That fits the log. After the flash at 12:52:34 the port stayed closed until 12:53:30.5. On opening, only
  the boot print came through (100 lines, 2 318 bytes: the queued transfers). The VT lines the plough
  logged at 12:52:59 - 12:53:02 (version check, upload) are missing. Every later dump today shows 2.6-2.7
  ms, through two VT moves and a 1200 reboot that loaded from NVM.
- At the 11:01 flash the port was also opened after the boot, and the dump at 11:02:32 showed 3046 us.
  There the one 120 ms wait most likely fell in `setup()` (the boot print alone fills the queue), which
  the loop-pass figure does not count. Not proven.

Field impact: none without a laptop. With no USB host the port is not configured and writes return at
once. The stall needs a host that has enumerated the Teensy but has not opened the port, and it happens
once per such period. A bench check would confirm it: boot with USB connected and the port closed for
~60 s, then dump (expect ~124 ms); boot with the logger already open (expect ~3 ms).
