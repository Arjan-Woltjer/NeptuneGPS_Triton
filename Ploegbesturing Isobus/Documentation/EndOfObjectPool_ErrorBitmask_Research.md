# End of Object Pool Response — Decoding "Pool Error Bitmask Value 9"

**Date compiled:** 10 August 2026
**Purpose:** Reference research resolving the Fendt Universal Terminal's rejection of Triton's VT object pool, logged in Session 3 (`HardwareTestNotes.md`, Bos). Follow-up to the same day's off-tractor reference-parser verification.

---

## 1. Background

Session 3 (Bos, Fendt tractor/UT) got the VT handshake all the way to object pool upload, then received:

> `Error in end of object pool message. Faulty Object 0 Faulty Object Parent 65535 Pool error bitmask value 9`

Two dead ends were ruled out the same day, off-tractor:

- **Not a bug in the pool bytes.** `Open-Agriculture/AgIsoStack-plus-plus`'s own IOP parser (`parse_iop_into_objects()`, fetched fresh from `main`) accepted the real 445-byte, 23-object pool outright — all objects parsed, zero errors, byte-for-byte match confirmed against the `append*()` encoding used in `VTObjectPool.cpp`.
- **Not decodable from AgIsoStack alone.** AgIsoStack's own reference VT server (`AgIsoVirtualTerminal`, via `VirtualTerminalServer::update()`) hardcodes the error-code byte to `0` on every response it sends — `/// @todo Get the parent object ID of the faulting object` sits next to it, unimplemented. So the value `9` genuinely isn't decodable from anything in the Open-Agriculture codebase.

This left the leading theory as "Fendt-proprietary code, opaque without a bench CAN test." That turned out to be wrong — the value is standard, just not implemented on the sending side by AgIsoStack's own reference server.

---

## 2. The message structure (ISO 11783-6, §C.2.5)

The End of Object Pool Response message (VT → ECU, PGN 1810, 8 bytes) carries **two separate error-code bytes**, confirmed directly from the standard text:

**Byte 2 — Error Codes** (coarse pass/fail flag):

| Bit | Meaning |
|---|---|
| 0 | Errors in the Object Pool (see remaining bytes) |
| 1 | VT ran out of memory during transfer |
| 2–3 | Reserved |
| 4 | Any other error |

**Bytes 3–4 / 5–6** — Parent Object ID / Object ID of the faulty object (transmitted as NULL Object ID, `0xFFFF`/65535, if there are no errors).

**Byte 7 — Object Pool Error Codes** (the byte behind "bitmask value 9"):

| Bit | Value | Meaning |
|---|---|---|
| 0 | 1 | Method or attribute not supported by the VT |
| 1 | 2 | Unknown object reference (missing object) |
| 2 | 4 | Any other error |
| 3 | 8 | Object pool was deleted from volatile memory |
| 4–7 | — | Reserved |

Byte 8 is reserved.

Critically, the standard also states directly: when the VT replies with an error of any type, it **should delete the object pool from volatile memory** and inform the operator. That means bit 3 is not case-specific diagnostic information — it rides along with essentially any rejection as a matter of course. **Only bits 0, 1, and 2 actually carry a diagnosis.**

---

## 3. Decoding this session's value

`9` (decimal) = `0b1001` = **bit 0 + bit 3**.

| Field | Value | Meaning |
|---|---|---|
| Faulty Object ID | `0` | The WorkingSet (root object — correct, it's object 0 in the pool) |
| Faulty Parent Object ID | `65535` (`0xFFFF`) | NULL — correct, the WorkingSet has no parent |
| Object Pool Error Codes | `9` = bit 0 + bit 3 | Bit 0: **method or attribute not supported by the VT**. Bit 3: pool deleted from volatile memory (standard boilerplate on any error — see §2) |

**Net result:** stripping the boilerplate bit, the Fendt UT is reporting a single, specific complaint — **some method or attribute value on the WorkingSet object itself is not supported by this VT** — not a structural or encoding error. That's consistent with, and now explains, the earlier finding that AgIsoStack's own reference parser accepts the exact same bytes without complaint: the pool is spec-legal, but this particular VT doesn't support something about it anyway.

---

## 4. Implications for the next bisection round

Session 3's live bisection already varied and cleared:
- Bare WorkingSet + empty DataMask, `softKeyMask = NULL_OBJECT_ID`
- DataMask referencing a real SoftKeyMask instead of NULL
- Background colour `1` + one real visible child, matching AgIsoStack's reference pool byte-for-byte

All three failed identically — which, read against §3, makes sense: none of those changes touched the WorkingSet object's *own* attribute fields, only its children or referenced masks. The next bisection round should vary the WorkingSet object's own fields specifically, for example:

- Any macro attached directly to the WorkingSet
- The `Selectable` attribute (present on newer WorkingSet encodings)
- The Active Mask object ID reference
- Anything version-gated that the reference parser accepts leniently but a production VT enforces strictly

This is a cheaper next step than the full `AgIsoVirtualTerminal` GUI / CAN-adapter bench test, and narrows Session 4 (van Mastwijk) to a sharper question: does the InCommand 1200 accept the same WorkingSet encoding that Fendt rejects?

---

## 5. Caveats

- The uploaded standard is **ISO/FDIS 11783-6:2004(E)**, and the connected terminal negotiated **VT version 6** (ISO 11783-6:2018). The End of Object Pool error-code table is foundational, low-level protocol structure that has not needed revision across VT version bumps in any implementation checked here (Wireshark's current dissector, tracking the modern standard, defines the identical bit layout — see reference 2). Treated as reliable, but not verified against a 2018 copy of the text directly.
- This decodes *what* the VT is telling you, not *which* WorkingSet attribute it objects to. That still requires either the bisection round in §4 or the deferred `AgIsoVirtualTerminal` GUI / real-CAN test.
- Not yet field-verified against the actual Fendt terminal — this is a paper analysis pending the next Bos or bench session.

---

## 6. References

1. ISO/FDIS 11783-6:2004(E), *Tractors and machinery for agriculture and forestry — Serial control and communications data network — Part 6: Virtual terminal*, §C.2.4–C.2.5 "End of Object Pool message" / "End of Object Pool Response message", pp. 87–88 — user-provided copy, primary source for the byte tables in §2.
2. Wireshark — `packet-isobus-vt.c` ISOBUS VT dissector (independent cross-check of the same bit layout, current `main`) — https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-isobus-vt.c
3. Wireshark 4.4.0 source mirror (Fossies) — used to confirm the Byte 2 `errorCodes` field labels — https://fossies.org/linux/misc/wireshark-4.4.0.tar.xz/wireshark-4.4.0/epan/dissectors/packet-isobus-vt.c
4. Open-Agriculture/AgIsoStack-plus-plus — reference IOP parser (`parse_iop_into_objects()`) used for the same-day offline pool-acceptance test — https://github.com/Open-Agriculture/AgIsoStack-plus-plus
5. Open-Agriculture/AgIsoVirtualTerminal — reference VT server; confirms the error-code byte is hardcoded to `0` on send (unimplemented `@todo`) — https://github.com/Open-Agriculture/AgIsoVirtualTerminal
6. `HardwareTestNotes.md`, Session 3 (Bos, 2026-08-10) and same-day "Follow-up (off-tractor)" section — internal project log this research follows up on.

---

*This document is a companion to `HardwareTestNotes.md` (Session 3 / Session 4 field log) and `ISOBUS_TC_Manufacturer_Comparison.md` (TC-GEO licensing research) — both referenced above.*
