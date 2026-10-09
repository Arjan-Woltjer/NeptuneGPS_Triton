# 2026-10-08 -- Lemken plough control + L160 lightbar on the van Mastwijk CNH -- timeline

No Triton on the bus, no serial log; this day was a CANedge-only capture of another vendor's
plough control (Lemken ISOBUS, on the IBBC) and an Ag Leader L160 lightbar, on the InCommand 1200
rig with the new cable harness. Times below are the CANedge's own session order; the card clock is
bogus and no device on the IBBC segment sends Time/Date. Operator notes came in by chat during
the day. Analysis in `../canlogs/README.md` (2026-10-08 entry) and in the Documentation repo,
`ISOBUS/research/agleader-incommand-65462-xte-2026-10-08.md`.

| Card session | Where the lead was | Result |
|---|---|---|
| 34, 35, 36 | in-cab 9-pin round connector, pair 2/4 | empty (channel 9 only); the power pin of the lead had come loose |
| 37, 38 | same, pin fixed | still empty |
| 39 | same | 7.5 min of bus: TECU 0xF0 + CNH VT 0x26 only. Lemken box was on the IBBC and did not load on the CNH VT; the InCommand was on and steering, yet absent |
| 40, 41 | same | 3.7 min each, same two devices (+ CNH 0xAC/0xCD claims in 40). Meter: pins 2-3 and 4-5 bridged on the in-cab connector, so no second pair exists |
| **42** | **channel 2 back-probed on the IBBC (pins 8/9, ground 2), channel 1 still in the cab** | 347 s. ch2: InCommand's five CFs, Lemken 0xEE, L160 0xDC. ch1: TECU + CNH VT only. Lemken power-cycled twice (pool upload at ~128 s cut, complete at ~165 s); L160 claimed at 104 s and 143 s; tractor drove 192-211 s and 216-297 s at ~2 km/h, autosteer engaged at ~200 s. Operator: XTE large at first, 0-2 cm once engaged |

Conclusions: the in-cab connector and the IBBC were separate buses THAT DAY, which is why the
Lemken pool never reached the CNH VT. **Correction 2026-10-09 (owner):** the split was not the harness. The Lemken technician had disconnected the IBBC and the InCommand's ISOBUS branch from the rest of the bus for his demo, because the tractor "sometimes eats messages". Normally everything is one bus, as logs 28/31 and the 10-03 session showed. The in-cab connector remains a valid tap on a normal day; the one-minute inventory check is what tells. Further: the
Lemken VT pool and DDOP are harvested; and the InCommand's XTE is PGN 65462 from 0xF5, global,
5 Hz, with the sign still to be fixed on a deliberate-offset drive.
