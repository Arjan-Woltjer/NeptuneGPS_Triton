# ISOBUS Tramline Control — Which Task Controllers Actually Support It

**Date compiled:** 5 September 2026
**Purpose:** Establish whether the terminals we test Triton against implement AEF/ISO 11783 Tramline Control (DDI 505/506/507/508/509/510/511/512/513/514/515/516/518), before spending tractor time on it. Written specifically because the Ag Leader InCommand 1200 is the live test target (`HardwareTestNotes.md`, Session 4, van Mastwijk).

Companion to `ISOBUS_TC_Manufacturer_Comparison.md` (TC-GEO licensing) — and see §7, which corrects a miscitation in that document.

---

## 1. Summary

**Tramline Control only became an AEF-certifiable ISOBUS functionality five days ago.** The AEF released it as **TRACK Generation 1**, together with its conformance test, on **31 August 2026**. Before that date it was a DDI definition plus a requirements document (`TramlineControl_BasicRequirements_v1.16`) with no certification path at all. Consequence: there is essentially no certification data to look up. Any tramline interop working in the field today was built against the requirements document directly, vendor-to-vendor, not certified.

**TRACK Generation 1 covers Levels 1 and 2 only — the levels where the *implement* calculates the tramlines.** Level 3, where the Task Controller does the tramline maths, is explicitly deferred to a future TRACK Generation 2. So no Task Controller on the market can be certified for Level 3 today, and any Level 3 claim you encounter is proprietary.

**For the Ag Leader InCommand 1200 specifically: no public evidence was found that it implements ISOBUS Tramline Control, in either direction.** Ag Leader's documentation does not mention DDI 505/506 or tramline control as an ISOBUS functionality anywhere I could reach.

There is a trap here that needs stating plainly. **The InCommand 1200 does have a feature called "Tramlines" — and it is not this.** It lives in the *Guidance and Steering* chapter of the operator's manual (pp. 129–130), next to AB lines, curves and SmartPath. It marks which guidance passes are tramline passes and flashes the pass number under the on-screen lightbar when you reach one. It works only with Straight AB and Identical Curve patterns. It is an operator aid; nothing is negotiated with an implement ECU. Anyone searching "InCommand tramline" will land on this and conclude, wrongly, that the display supports ISOBUS Tramline Control.

The only terminal-side implementation I could confirm from a manufacturer's own current page is **Fendt** ("Fendt Section Control with Tramline Control", free within the Section Control package). **Amazone** (Level 1, named explicitly) and **CCI** (CCI.Command) are also credible. **John Deere leads the AEF TRACK project team** but publishes nothing about shipping support — do not read the former as the latter.

Practical answer for the tractor: assume the InCommand 1200 does not do it, and run the cheap decisive test in §6 to find out for certain, because the negotiation is designed to give you an unambiguous answer.

---

## 2. Per-brand findings

| Brand / terminal | Tramline Control (DDI 505/506)? | Level(s) | Evidence strength | Source |
|---|---|---|---|---|
| **Ag Leader InCommand 1200** | No evidence found. Has an unrelated guidance-side "Tramlines" feature | — | **Unknown** (leaning no) | [Manual, Guidance & Steering ch.](https://www.manualslib.com/manual/3850603/Ag-Leader-Incommand-1200.html) |
| **Fendt** (FendtONE / Varioterminal) | **Yes** — "Fendt Section Control with Tramline Control" | Not stated; behaviour is L1/L2 | **Confirmed** (manufacturer page) | [fendt.com](https://www.fendt.com/int/smart-farming/fendt-section-control-with-tramline-control) |
| **Amazone** (AmaTron 4, GPS-Track) | **Yes** — "ISOBUS Level 1 tramline control for seed drills" | **Level 1**, named | **Confirmed** (manufacturer page) | [amazone.net](https://amazone.net/en/products-digital-solutions/digital-solutions/terminals-hardware/isobus-terminals/gps-track-972724) |
| **CCI** (CCI 1200, CCI.Command) | Likely — "Tramline Control generates tram lines automatically via GPS" | Not stated | **Likely** (vendor pages, no level, no DDI) | [cc-isobus.com](https://www.cc-isobus.com/en/cci-1200/), [KUHN](https://www.kuhn.com/en/electronics-connected-solutions/terminals-and-isobus-solutions/cci-apps) |
| **John Deere** (Gen4 / G5) | No evidence found — but leads the AEF TRACK team | — | **Unknown** | [AEF](https://www.aef-online.org/aef-news/new-aef-project-team-tramline-control-started.html) |
| **Trimble** (GFX/XCN) | No evidence found | — | **Unknown** | — |
| **CNH** (AFS Pro 700/1200, IntelliView) | No evidence found | — | **Unknown** | — |
| **Claas** (Cemis 1200) | Guidance-side tramline flagging only — same trap as Ag Leader | — | **Likely not** (weak source) | [Future Farming](https://www.futurefarming.com/tech-in-focus/cemis-1200-simplifies-the-management-of-reference-tracks-and-tramlines/) |
| **Topcon** | Implement-side only (ISOBUS Artemis drill controller); nothing for X-series terminals | — | **Likely not** (TC side) | [Topcon brochure](https://www.topconpositioning.com/content/dam/topcon_digital_asset_hub/collateral/brochures/topcon_ISOBUS-Artemis-Seeding_enEU23broc_RevA.pdf) |
| **Horsch** (implement) | **Yes**, implement side — "tramline control level 1 and 2 (ISOBUS)" | **L1 + L2**; L3 proprietary to Horsch terminals | **Confirmed** (manufacturer page) | [horsch.com](https://www.horsch.com/en/products/intelligence/isobus-applications) |
| **Väderstad** (implement) | **Yes**, implement side — "ISOBUS Dynamic Tramlining" | Not stated | **Confirmed** (manufacturer news) | [vaderstad.com](https://www.vaderstad.com/en/about-us/news/news-archive/2025/international/introducing-isobus-dynamic-tramlining-for-tempo-and-proceed) |
| **Lemken** (implement) | Implement-side "iQblue tramline control"; no ISOBUS level or DDI cited | — | **Likely** proprietary | [lemken.com](https://lemken.com/en-en/agricultural-machines/iqblue/licenses/iqblue-tramline-control) |

"Unknown" above means *no public evidence found either way* — not "probably yes". For Trimble and CNH I found current ISOBUS/TC licensing documentation that discusses TC-BAS, TC-SC and TC-GEO in detail and never mentions tramlines. That is suggestive of absence but is not proof; these are marketing and licensing pages, not protocol conformance statements.

---

## 3. Detail per brand

### Ag Leader — InCommand 1200 (the live question)

Three separate lines of enquiry, all negative for ISOBUS Tramline Control:

1. **The operator's manual.** "Tramlines", "Tramline Starting Location" and "Tramline Adjust" appear on pp. 129–130, inside the **Guidance and Steering** chapter (p. 115) — the chapter covering AB lines, curves, SmartPath and steering. Not in the ISOBUS chapter. The manual text reads: *"Tramline selection can be enabled from the New Pattern or Load Pattern menu. When enabled the Tramline setup page will appear during the load pattern process."* Supporting documentation describes the visual cue as *"the tram number below the on screen lightbar that will flash the pass number when the tramline is reached"*, with a configurable "paths between tramlines" value, and support limited to *"Straight AB and Identical Curve patterns only"*. This is guidance-pattern bookkeeping for the driver. There is no implement in the loop.

2. **Ag Leader's ISOBUS documentation.** Their support article on ISOBUS/UT/Task Controller (the one already cited in `ISOBUS_TC_Manufacturer_Comparison.md` for the InCommand TC matrix) enumerates UT and Task Controller availability per display model and does not mention tramline control. The portal is a JavaScript app and would not render for automated fetching today, so this rests on search-engine extracts of the same article plus the earlier reading captured in the sibling document — **weaker than I would like**.

3. **Firmware release notes.** InCommand v9.5 release notes mention ISOBUS only in the context of planter/air seeder configurations, variable swath width, AutoSwath section shutoff and Load & Go. No tramline entries. This is one version of one release note, not a full history.

Two weak indirect signals, both pointing the same way. Väderstad publishes per-brand ISOBUS Task Control setup guides for Case IH AFS Pro 700, Fendt VarioTerminal 10.4, John Deere 4600, New Holland IntelliView IV, Topcon X25/X30 and Trimble TMX — **Ag Leader is not among them**. And Ag Leader's market centre of gravity is North American row-crop planting, where tramlining is a far less common practice than in European cereal drilling; the feature has less reason to exist. Neither of these is evidence about the protocol. I mention them only because they make "no evidence found" easier to act on.

**Verdict: unknown, leaning strongly to unsupported.** I am not willing to write "does not support it" without either an Ag Leader statement or a bus trace, and neither exists publicly. See §6 for the test that settles it in one connection.

### Fendt — confirmed

The strongest terminal-side evidence found, from Fendt's own product page (fetched 5 September 2026):

> "Tramline Control is available as a free update in the "Fendt Section Control" function package for ISOBUS-capable machines and can be used with Tramline Control-capable implements."

> "The position data of the tractor is transferred to the enabled seeder so that it automatically activates the corresponding seeding units for the tramline, independently of the processing pattern. There is no need for tedious counting and convenient wayline types and turning modes can be selected."

Note what that second sentence describes: the tractor sends position/track data, **the seeder decides and switches**. That is Level 1 or Level 2 behaviour. Fendt does not state a level anywhere I could find. It also appears in Fendt's 30-day software trial list as "Section Control - also with Tramline Control", remotely activatable by a dealer since July 2025.

Fendt has publicly promoted the Fendt + Väderstad pairing ("New ISOBUS function free of charge for Tramline Control capable implements by Väderstad", Fendt Global social channels, circa 2024 — **social media, weak as a source**, but it corroborates the product page and identifies a known-working counterparty).

Relevant to this project: Session 3 at Bos was on a Fendt UT. If tramline work is ever wanted on real hardware, that is the tractor with a documented TC-side implementation.

### Amazone — confirmed Level 1

Amazone's GPS-Track page (last modified 23 September 2024) states:

> "GPS-Track also enables ISOBUS Level 1 tramline control for seed drills, as an alternative to tramline control via track markers or a working position sensor and as an option for working in lands."

This is a **terminal-side** claim — GPS-Track is a software licence for Amazone's own AmaTron 4 terminal — and it names the level explicitly, which almost nobody else does. It is also a nice statement of what the feature replaces: track markers and working-position sensors, i.e. the pre-GNSS mechanical way of doing this.

Separately, Amazone's GPS-Switch pro is described as offering "intelligent tramlining switching, which, after having created the first AB line, can already calculate all further tramlines". Calculating tramlines in the terminal is Level 3 behaviour, but this is Amazone terminal plus Amazone implement, and no level or DDI is cited. **Treat as proprietary unless proven otherwise — inferred, unverified.**

### CCI (CCI 1200 / CCI.Command) — likely

CCI's own page says "Tramline Control generates tram lines automatically via GPS". KUHN's page for the CCI app suite places it inside CCI.Command Parallel Tracking, marks it "Included" (not a separate licence), and adds the caveat "Check seed drill compatibility" — which implies a negotiated, implement-dependent feature rather than a display-only one. Neither page names a level or a DDI.

CCI matters disproportionately because the CCI terminals are the shared multi-vendor platform behind Amazone, KUHN, Lemken, Krone, Grimme and Rauch branded terminals. If CCI.Command implements DDI 505/506, that is one implementation reaching many nameplates. I could not confirm the protocol detail.

### John Deere — unknown, and be careful here

John Deere's position is the most easily over-read in this whole report. **Manuela Bilz of John Deere is the Team Lead of the AEF TRACK (formerly TRAM) project team**, and she is credited in the AEF's release announcement for the specification work. John Deere is therefore about as close to this standard as a company can be.

That tells you nothing about what ships in a Gen4 or G5 display today. Deere's public Gen4/G5 compatibility charts and release notes cover TC-BAS, TC-SC and TC-GEO, ISOBUS control unit limits and VT4 support. Nothing about tramlines. **No public evidence of shipping support was found.** Leading the standards work is a reason to expect support eventually, not evidence of it now.

### Trimble — no evidence found

Trimble's ISOBUS material for the GFX/XCN line is entirely about the tiered UT / Task Controller / prescriptions licence structure (documented at length in `ISOBUS_TC_Manufacturer_Comparison.md`). No tramline mention in any Trimble document, product page or dealer listing found. Väderstad does publish an ISOBUS Task Control guide for the Trimble TMX, but that is generic TC setup, not tramlining.

### CNH (Case IH AFS Pro 700/1200, New Holland IntelliView) — no evidence found

Same picture. CNH's ISO Task Controller material describes section control, variable rate, mapping and multi-implement compatibility. Tramlines are not mentioned. Väderstad publishes Task Control setup guides for both AFS Pro 700 and IntelliView IV, again generic TC.

### Claas — the same trap as Ag Leader

Coverage of the Cemis 1200 describes improved management of reference tracks and field segments, with tramlines "flagged with colour and acoustic signals". That is the terminal marking passes for the operator — the Ag Leader pattern, not DDI exchange. Claas's own site was not reachable for this (HTTP 403), so this rests on a trade-press article accessed via search extract. **Weak source; treat the "likely not" as provisional.**

### Topcon — implement side only

Topcon's ISOBUS Artemis is a **seed drill controller** — an implement ECU, not a Task Controller. It runs tramline sequences itself, with its own Tramline Control Module (HBM) and hardware tramline advance switches, and lets the operator pick the bout number when re-entering work. This is the classic pre-ISOBUS-negotiation architecture, and it is a useful concrete example for §5. Nothing found suggesting the Topcon X-series consoles implement TC-side tramline control.

### Implement-side manufacturers (useful as evidence about the TC side)

- **Horsch** is the most informative source found on levels. Their ISOBUS applications page states support for **"tramline control level 1 and 2 (ISOBUS)"** for AutoLine, "available in combination with a terminal eosT10 Pro or other tramline-capable ISOBUS terminals". Crucially, it also notes that track recording (**level 3**) is *"proprietarily available in combination with HORSCH terminal eosT10 or Touch 800/1200"* — i.e. Level-3-like behaviour exists but only against Horsch's own terminals, by their own admission proprietary. That is the clearest available confirmation that Level 3 over standard ISOBUS is not a thing you can buy today.
- **Väderstad** introduced "ISOBUS Dynamic Tramlining" for Tempo planters and Proceed V (model year 2025 onward, retrofittable via a new-generation Gateway). Their statement is the most useful sentence in this whole report for our purposes: *"To function, the Väderstad machine relies on the tractor ISOBUS screen to be compatible and unlocked with the ISOBUS Tramline feature. This is currently seen on several newer terminals on the market."* An implement manufacturer stating outright that TC-side support is required, is not universal, and is limited to "several newer terminals".
- **Lemken** iQblue tramline control: implement-side, the drill calculates. No ISOBUS level or DDI cited on the product page. Lemken's Gregor Genneper is credited in the AEF TRACK release, so Lemken is engaged in the standard even if the product page predates it.
- **Amazone** appears on both sides — its drills do tramlining and its terminals provide Level 1.

---

## 4. Is Tramline Control an AEF-certifiable functionality?

Yes — as of **31 August 2026**, and not before. The timeline is worth having in full, because a claim's date determines what it can possibly mean:

| Date | Event |
|---|---|
| Autumn 2024 | Idea raised within AEF |
| 9 April 2025 | Project team kick-off, Frankfurt. Team Lead **Manuela Bilz (John Deere)**, deputy **Marco Brück (AEF)**. Stated goal: certifiable by end of 2025 |
| 30 July 2025 | Internal plugfest and workshop at OSB connagtive, Munich |
| 1 October 2025 | Initial draft implementation guideline circulated for AEF member review; work begins to represent the function in the AEF ISOBUS Database |
| 17 December 2025 | **TRAM renamed TRACK.** Documentation and certificates become "TRACK Generation 1" |
| **31 August 2026** | **TRACK functionality and conformance test released to AEF member companies.** Conformance test developed by OSB connagtive. Contributors credited: Manuela Bilz (John Deere), Hans van Zadelhoff (Grimme), Gregor Genneper (Lemken) |

Three things follow.

**Certification data barely exists yet.** Certification opened five days before this document was written. Products must now be submitted and tested. The AEF ISOBUS Database's TRACK column will be empty or near-empty for some time. Absence from that database currently means nothing at all.

**The generation split is the important detail.** TRACK Generation 1 covers the levels where the implement calculates. **TRACK Generation 2, planned but unreleased, is what adds Tramline Control Level 3** — moving the calculation to the Task Controller. There is therefore no certification path in existence for a Level 3 Task Controller.

**The rename is a search hazard.** Post-December-2025 AEF material says TRACK; the underlying spec, the DDI names and all vendor marketing still say Tramline Control. AEF was explicit that customer-facing material keeps "Tramline Control". Searching only for "tramline" will miss the certification story entirely, and searching only for "TRACK" will drown in guidance-track results.

**A caution on the launch date.** The AEF news article carries a `31.08.2026` datestamp, which I read twice and which is consistent with neighbouring items in the news list. One automated read of the news *index* page instead reported this headline under 9 January 2026. I could not reconcile the two and I have gone with the date on the article itself. If the precise date matters for something, verify it directly.

**On the TC-GEO conflation the brief warned about.** I found nothing that conflates them, and the structural evidence supports the warning: the AEF's own functionality list (UT, AUX, TECU, ISB, TIM, FS, TC-BAS, TC-GEO, TC-SC) contains no tramline entry, and if an existing ISO 11783-10 `ServerOptions` capability bit had covered tramlining, the AEF would not have needed to spend eighteen months creating a new functionality, a new guideline and a new conformance test for it. **A terminal advertising TC-GEO tells you nothing about tramline support.** Treat any source that implies otherwise as wrong.

---

## 5. Is this negotiated over ISOBUS in practice?

Mostly **no**, historically — and the brief's suspicion on this point is correct. But it is changing, and the honest answer is "both, with the non-negotiated path still dominant".

**The dominant pattern is that the drill does it alone.** The implement's own ECU holds the tramline rhythm, counts bouts, and switches the seeding units, configured entirely on its own UT screens. The trigger is a track marker, a working-position sensor, a bout counter or a manual advance switch. Topcon's ISOBUS Artemis is a textbook example: a dedicated Tramline Control Module and physical tramline advance switches, with the operator selecting the bout number on re-entry. Lemken's iQblue tramline control is implement-side. Amazone describes its Level 1 support as an *alternative* to "tramline control via track markers or a working position sensor" — naming the incumbent mechanism it replaces. No Task Controller is involved in any of this, and none needs to be.

**A second pattern looks like tramline support but is only an operator aid.** Ag Leader's InCommand "Tramlines" and, apparently, Claas's Cemis 1200 tramline flagging both fall here: the terminal knows which guidance passes are tramline passes and tells the driver, visually or audibly. Nothing is exchanged with the implement. This is the single biggest source of false positives when researching this question, and it is exactly what the InCommand 1200 has.

**The negotiated path is real but thin.** Amazone (Level 1), Horsch (Levels 1–2), Fendt, Väderstad and CCI all point at genuine DDI 505/506 exchange. Väderstad's "relies on the tractor ISOBUS screen to be compatible and unlocked with the ISOBUS Tramline feature… currently seen on several newer terminals" is the most candid public description of the state of play: it works, it needs the terminal's cooperation, and only some terminals have it.

**Even when it is negotiated, the Task Controller usually isn't doing the tramline maths.** This is the point most likely to be misunderstood. At Levels 1 and 2 — the only levels that are certifiable, and everything anyone ships today — the implement calculates. What the TC contributes is guidance-track information: the unique A-B line ID (508), the actual track number (509), the track numbers to right and left (510/511), swath width (512), deviation (513), GNSS quality (514). So "this Task Controller supports Tramline Control" mostly means "this Task Controller publishes its guidance-line numbering over the TC protocol and honours the 505/506 handshake" — not "this Task Controller computes tramlines". Level 3, the version where the TC really does own the calculation and the implement receives only setpoints, is available today only proprietarily (Horsch, on Horsch terminals) and is deferred to an unreleased TRACK Generation 2 in the standard.

The upshot for anyone about to go and test: the ISOBUS handshake is the *thin* part of tramline control, and the implement is where the intelligence lives.

---

## 6. Open questions, and what only real hardware can settle

**The decisive InCommand 1200 test.** The handshake is designed to answer this unambiguously, which makes the test cheap. Build a DDOP that declares:

- **DDI 505** (Tramline Control Level) as a **DPT**, value with bit 0 set = "Level 1 supported"
- **DDI 506** (Setpoint Tramline Control Level) **in the same device element** — the spec requires this explicitly
- the rest of the Level 1 required set: 515, 507, 508, 509, 510, 511

Connect, and watch what the TC does with DDI 506. Three outcomes, all informative:

- **Non-zero value command for DDI 506** → the InCommand implements Tramline Control, and the value names the level it wants.
- **Value command of 0** → "No common Level". A definitive *supported-but-no-match*, or at minimum a TC that knows what DDI 506 is.
- **Nothing at all** → the TC does not recognise the DDIs. This is what I expect.

Worth noting the TC-version dependency while doing this: if both TC and implement are TC version 4 or later, values go via `SetValueAndAcknowledgeCommand (0x0A)` rather than the ordinary `ValueCommand (0x03)`. Watching for the wrong PGN could produce a false negative.

A useful side-question for the same connection: does the InCommand populate the guidance-track DDIs (508/509/510/511) at all, independent of tramline support? Some TCs may publish track numbering for other reasons, and it would be worth knowing.

**Other open items.**

- Ag Leader has published nothing on this either way. A direct question to Ag Leader support is likely faster and more definitive than any further searching — and given how new TRACK is, "are you pursuing TRACK certification?" is a fair question to pair with it.
- Firmware currency. My newest Ag Leader release notes are v9.5. If anything changed later, I did not find it.
- The AEF ISOBUS Database requires a login and could not be queried. It should now grow a TRACK column. Given certification opened 31 August 2026, expect it to be empty for months; **do not read absence as a negative** for at least a year.
- Fendt's level is unstated. The described behaviour is Level 1 or 2; which one is unknown.
- CCI's implementation is unconfirmed at protocol level, and matters more than its single brand suggests because of how many terminals it sits behind.
- Level 3 cannot be meaningfully tested against any certified Task Controller, because none exists. If Triton ever wants Level 3 semantics, that is a proprietary conversation with one vendor, not a standards-based one.
- Claas's status rests on a single trade-press article; their own site blocked automated access.

---

## 7. Correction to `ISOBUS_TC_Manufacturer_Comparison.md`

Reference 2 of that document cites the AEF article *"AEF Announces Update: TRAM becomes TRACK"* as the source for **TC-GEO's technical vs. customer-facing naming** (TC-GEO / "Variable Rate Control").

That is a miscitation. That article is about **Tramline Control**: the AEF renaming its TRAM functionality to TRACK on 17 December 2025, ahead of releasing the guideline and certificates as "TRACK Generation 1". It has nothing to do with TC-GEO. The TC-GEO naming point in §1 of that document may well still be correct, but it is not supported by the source attached to it and needs a different citation.

Flagging rather than editing, since that document is outside the scope of this task.

---

## 8. Caveats

- Certification for this functionality is **five days old**. Every "no evidence found" in this report should be re-checked in six to twelve months, by which point the AEF database becomes a meaningful source rather than an empty one. This document has an unusually short shelf life.
- Several manufacturer sites (Fendt's US/regional pages, Claas, Future Farming, Ag Leader's portal and knowledgebase) refused automated fetching. Where a finding rests on a search-engine extract rather than a page I read directly, I have said so inline. The Fendt, Amazone and Horsch findings are from pages fetched and read directly today; the Claas and Ag Leader-portal findings are not.
- The vocabulary is genuinely treacherous. "Tramline" in a terminal's documentation means at least three different things: ISOBUS Tramline Control (DDI-negotiated), a guidance-pattern operator aid (Ag Leader, Claas), and the implement's own internal tramline logic (Topcon Artemis, Lemken iQblue). Post-December 2025 AEF material adds a fourth term, TRACK, for the first of these. Any future search on this topic should assume a hit is the wrong sense until the source proves otherwise.
- I did not find a single Task Controller vendor stating a Tramline Control **level** except Amazone (Level 1). Horsch states levels 1 and 2 from the implement side. Everyone else describes behaviour without naming a level, which makes cross-brand comparison partly inferential.

---

## 9. Sources

All accessed **5 September 2026** unless noted.

### Primary specification
1. AEF / ISO 11783-11 — *Tramline Control — Basic Requirements v1.16* (attachment to DDI 505 on isobus.net), 42 pp. — https://www.isobus.net/isobus/attachments/623/TramlineControl_BasicRequirements_v1.16_AEF_Development-v2.pdf — text extracted and read directly; source for the level definitions, the full DDI tables, the 505/506 handshake and the TC-version/PGN note.

### AEF — certification status and timeline
2. AEF — "New AEF project team Tramline Control started", 23 April 2025 — https://www.aef-online.org/aef-news/new-aef-project-team-tramline-control-started.html
3. AEF — "AEF team Tramline Control met for testing in Munich", 30 July 2025 — https://www.aef-online.org/aef-news/tramline.html
4. AEF — "News out of project team Tramline Control (TRAM)", 1 October 2025 — https://www.aef-online.org/aef-news/news-out-of-project-team-tramline-control-tram.html
5. AEF — "AEF Announces Update: TRAM becomes TRACK", 17 December 2025 — https://www.aef-online.org/aef-news/aef-announces-update-tram-becomes-track.html
6. AEF — "TRACK: NEW ISOBUS functionality and Conformance Test launched", 31 August 2026 — https://www.aef-online.org/aef-news/track-new-isobus-functionality-and-conformance-test-launched.html
7. Precision Farming Dealer — "AEF Introduces TRACK, Extending Automation Beyond Steering" (source for the Generation 1 / Generation 2 and Level 3 split; page returned HTTP 403 on direct fetch, content via search extract — **weak**) — https://www.precisionfarmingdealer.com/articles/7182-aef-introduces-track-extending-automation-beyond-steering
8. AEF — "AEF Tour" (current functionality list: UT, AUX, TECU, ISB, TIM, FS, TC-BAS, TC-GEO, TC-SC — no tramline entry) — https://www.aef-online.org/aef-tour/index.html
9. AEF ISOBUS Database (JavaScript app, login required; could not be queried) — https://www.aef-isobus-database.org/

### Ag Leader
10. ManualsLib — Ag Leader InCommand 1200 User Manual (Guidance and Steering ch. p.115; Tramlines pp.129–130) — https://www.manualslib.com/manual/3850603/Ag-Leader-Incommand-1200.html
11. ManualsLib — Ag Leader InCommand 800 User Manual — https://www.manualslib.com/manual/1456391/Ag-Leader-Incommand-800.html
12. Ag Leader Community Portal — "ISOBUS, Universal Terminal (Virtual Terminal), & Task Controller" (JS app; would not render — content via search extract, **weak**) — https://portal.agleader.com/community/s/article/1271?language=en_US
13. AG Precision — InCommand v9.5 Firmware Release Notes (PDF, read directly; no tramline entries) — http://ag-precision.com/pdf/incommand/Release_Notes_InCommand_v9.5.pdf
14. Unearth Ag — "Ag Leader InCommand 1200 ISOBUS Functionality", 16 February 2024 (dealer blog, **weak**; notable only for what it omits) — https://unearthag.com/2024/02/16/ag-leader-incommand-1200-isobus-functionality/

### Fendt
15. Fendt — "Fendt Section Control with Tramline Control" product page (fetched and read directly; source for both verbatim quotes) — https://www.fendt.com/int/smart-farming/fendt-section-control-with-tramline-control
16. Fendt — "Machine control with FendtONE" (functionality list and 30-day trial list) — https://www.fendt.com/int/smart-farming/machine-control
17. Fendt Global — "New ISOBUS function free of charge for Tramline Control capable implements by Väderstad" (social media, **weak**) — https://www.facebook.com/FendtGlobal/posts/843914141102646/

### Implement manufacturers
18. Amazone — GPS-Track (page modified 23 September 2024; source for the "ISOBUS Level 1 tramline control" quote) — https://amazone.net/en/products-digital-solutions/digital-solutions/terminals-hardware/isobus-terminals/gps-track-972724
19. Amazone — "Software licences for precise working" (GPS-Switch pro tramline calculation claim) — https://amazone.net/en/products-digital-solutions/digital-solutions/terminals-hardware/isobus-terminals/software-licences-for-precise-working-972574
20. Horsch — ISOBUS applications (AutoLine; "level 1 and 2"; level 3 proprietary) — https://www.horsch.com/en/products/intelligence/isobus-applications
21. Horsch — ISOBUS Terminals (eosT10 / eosT10 Pro feature split) — https://www.horsch.com/en/products/intelligence/intelligence/isobus-terminals
22. Väderstad — "Introducing ISOBUS Dynamic Tramlining for Tempo and Proceed", 2025 (source for the "relies on the tractor ISOBUS screen" quote) — https://www.vaderstad.com/en/about-us/news/news-archive/2025/international/introducing-isobus-dynamic-tramlining-for-tempo-and-proceed
23. Väderstad — "Guides for ISOBUS Task Control for Tempo" (per-brand terminal list; Ag Leader absent) — https://www.vaderstad.com/us-en/support/guides-for-your-machine/isobus-task-control
24. Lemken — "iQblue tramline control" — https://lemken.com/en-en/agricultural-machines/iqblue/licenses/iqblue-tramline-control
25. Topcon — ISOBUS Artemis seed drill controller brochure (implement-side tramline module and advance switches) — https://www.topconpositioning.com/content/dam/topcon_digital_asset_hub/collateral/brochures/topcon_ISOBUS-Artemis-Seeding_enEU23broc_RevA.pdf

### Other terminals
26. CCI — CCI 1200 product page ("Tramline Control generates tram lines automatically via GPS") — https://www.cc-isobus.com/en/cci-1200/
27. KUHN — "Applications for CCI terminals" (Tramline Control included in CCI.Command Parallel Tracking; "check seed drill compatibility") — https://www.kuhn.com/en/electronics-connected-solutions/terminals-and-isobus-solutions/cci-apps
28. Future Farming — "Cemis 1200 simplifies the management of reference tracks and tramlines" (HTTP 403 on direct fetch; content via search extract — **weak**) — https://www.futurefarming.com/tech-in-focus/cemis-1200-simplifies-the-management-of-reference-tracks-and-tramlines/
29. John Deere — G5 and Generation 4 compatibility chart (TC-BAS/TC-SC/TC-GEO only; no tramline) — https://www.deere.com/assets/pdfs/common/stellarsupport/g5-and-generation-4-compatibility-chart-english.pdf
30. Case IH — AFS ISO Task Controller / Section & Rate Control (no tramline mention) — https://www.caseih.com/en/asiapacific/products/precision-technology/section-and-rate-control

---

*Companion to `ISOBUS_TC_Manufacturer_Comparison.md` (TC-GEO licensing research — see §7 for a correction to it) and `HardwareTestNotes.md` (field log). The InCommand 1200 test proposed in §6 is intended for a van Mastwijk session.*
