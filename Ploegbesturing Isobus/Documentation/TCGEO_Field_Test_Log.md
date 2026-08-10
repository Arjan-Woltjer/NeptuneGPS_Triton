# TC-GEO Field Test Log — Ploegbesturing Isobus Project

**Date:** 10 August 2026
**Purpose:** Verify, on real farm terminals, whether TC-GEO is actually licensed/unlocked — and what that means for Triton when it declares itself as an ISOBUS TC client on someone else's terminal.

This log is a direct follow-up to the TC-GEO licensing research (see comparison table from this morning's discussion). The core question driving these visits: **Triton cannot assume the host terminal has TC-GEO active.** Licensing is per-farm, per-terminal, and — per the research — TC-GEO is gated behind a separate purchase on four of the five major brands. This log tracks real-world confirmation, starting with two farms that happen to sit on opposite ends of that licensing spectrum.

---

## Test 1 — Bos: Trimble (GFX-series) + Fendt

**Status:** Completed this morning.

| Field | Detail |
|---|---|
| Terminal | Trimble GFX-series *(exact model — 350/750/1060/1260 — TBC, fill in)* |
| Tractor | Fendt |
| TC-GEO observed | **Likely NOT unlocked** |

### What was seen
TC-GEO functionality appeared unavailable on Bos's terminal. This lines up with the licensing structure found in this morning's research: Trimble sells the base **"ISOBUS Task Controller" license** (TC-BAS + TC-SC) as one SKU, and geo-referenced/prescription functionality (**TC-GEO**) as a *separate, additional* license on top of that. Trimble's own documentation is explicit that the base TC license is a prerequisite for the prescription license — so a farm can hold TC-SC without ever having purchased the TC-GEO layer. That matches what Bos is showing.

### The plough-control connection
This directly explains something Bos flagged: **they've had to buy a separate plough control solution that needs its own separate A/B lines**, rather than being able to drive plough guidance off the tractor terminal's own geo-referenced task data. Without TC-GEO licensed on the terminal, there is no position-tagged data channel coming out of the terminal's Task Controller for an implement to hook into — TC-SC only gives section on/off logic and totals (TC-BAS), not a georeferenced stream. A plough control system in that situation has no choice but to run its own independent guidance line rather than consuming anything from the tractor's TC.

**This is the exact scenario Triton needs to handle gracefully.** If Triton ends up on a terminal without TC-GEO licensed, it cannot rely on the terminal for georeferenced task execution or prescription-driven plough behavior — it needs to be self-sufficient, sourcing its own guidance reference (consistent with the DDI 513 architecture decision: deviation is measured at the implement's own Device Reference Point, not borrowed from the terminal). Bos is a live example of exactly the degraded case the architecture already anticipated.

### Open items to confirm
- [ ] Exact GFX model number and firmware/Precision-IQ version
- [ ] Whether Bos's dealer confirmed "not purchased" vs. some other fault (e.g., licensed but misconfigured)
- [ ] Name/brand of the separately-purchased plough control system currently in use, and whether it exposes any ISOBUS interface at all or is fully standalone
- [ ] Whether TC-SC (section control) itself was confirmed working — this would isolate "no TC-GEO license" from "no TC license at all"

---

## Test 2 — van Mastwijk: Ag Leader InCommand 1200 + CNH

**Status:** Planned for this afternoon.

| Field | Detail |
|---|---|
| Terminal | Ag Leader InCommand 1200 |
| Tractor | CNH *(Case IH / New Holland / Steyr — TBC)* |
| TC-GEO expected | **Should be unlocked out of the box** |

### Why this one should be different
The InCommand 1200 is the one display in the Ag Leader lineup that ships with Universal Terminal *and* Task Controller standard — no unlock purchase required, unlike the InCommand 800 (needs a paid ISOBUS unlock for both UT and TC) or the Compass display (UT unlock only, no TC support at all, regardless of purchase). This makes van Mastwijk a useful contrast case to Bos: same underlying AEF functionality, but a brand where the commercial gate is largely absent by default.

### What to check on-site
- [ ] Confirm TC-GEO shows as active/licensed in the terminal's ISOBUS diagnostics screen
- [ ] Confirm whether Triton's DDOP is accepted and the working set connects cleanly against a terminal that *does* have TC-GEO
- [ ] Check whether the terminal actually processes position-tagged DDI values from Triton, or only logs totals — i.e. confirm TC-GEO isn't just "present" but functionally exchanging geo-referenced data
- [ ] Compare behavior directly against this morning's Bos observations — same DDOP, same Triton firmware, different terminal capability — to isolate what changes
- [ ] Note the CNH tractor variant (Case IH/New Holland/Steyr all share the same PLM/AFS backend per the research, so behavior should be consistent regardless of badge)

---

## Why this matters for Triton's design

Two consequences worth carrying into the TC client architecture:

1. **Triton must degrade gracefully when TC-GEO isn't present.** It should detect this from the terminal's declared functionalities during the ISOBUS boot-up handshake (working set master announces its supported functionalities) rather than assuming it and failing silently. If TC-GEO isn't there, Triton falls back to whatever TC-SC/TC-BAS-level exchange is possible, and continues to rely on its own DDI 513-based deviation measurement at the implement's DRP rather than expecting georeferenced task data from the terminal.

2. **Field testing has to be done per-terminal, not once.** Since TC-GEO licensing is a farm-by-farm commercial decision rather than a fixed hardware capability, "does Triton work with ISOBUS terminal X" isn't a single yes/no — it's conditional on that specific unit's licensed feature set. Bos and van Mastwijk are the first two data points; worth building a small compatibility log (terminal model + confirmed licensed functionalities + Triton behavior observed) as more farms get tested, the same way the three-farm CAN log set (Van der Molen/Raven, Van Veen/Raven+CNH, van Os/John Deere) is already being used for protocol reference.

---

## Reference: licensing gate by brand (from this morning's research)

| Brand | TC-GEO gating |
|---|---|
| John Deere | Free at firmware level (SU 18-2+); paywall is one layer up, at Operations Center data sync |
| **Trimble** | **Separate paid license, on top of base TC license** ← Bos |
| Raven | Bundled with TC-SC in one paid activation key (no free tier at all) |
| CNH built-in | Bundled with TC-SC in one dealer activation code |
| **Ag Leader** | **Included standard on InCommand 1200**; paid unlock on InCommand 800; unavailable on Compass regardless of purchase ← van Mastwijk |

---

*Next update: fill in van Mastwijk results after this afternoon's visit.*
