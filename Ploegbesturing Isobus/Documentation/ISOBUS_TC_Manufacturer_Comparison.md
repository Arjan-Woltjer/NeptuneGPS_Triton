# ISOBUS Task Controller Capability Comparison — John Deere, Trimble, Raven, CNH, Ag Leader

**Date compiled:** 10 August 2026
**Purpose:** Reference research for the Ploegbesturing Isobus (Triton) project — establishes what TC-GEO actually is, and how each major terminal brand gates access to it, before field-testing Triton against real farm terminals (see `TCGEO_Field_Test_Log.md`).

---

## 1. What TC-GEO actually is (AEF definition)

TC-GEO is a single AEF-certified functionality, not two separate ones. It covers both:

- **Geo-referenced documentation** — logging an as-applied map, tagging totals to position
- **Geo-referenced control** — using a prescription/variable-rate map divided into management zones to automatically vary application rate by position

There is no separate "logging-only" vs. "control" AEF certification split — a terminal is either TC-GEO certified (and can do both) or it isn't. The commercial gating seen below (paid unlock, tiered SKU, subscription) is a vendor decision layered *on top of* one standardized AEF functionality, not a reflection of two different standards.

AEF's technical name for the functionality is "TC-GEO"; their customer-facing marketing term for the same thing is "Variable Rate Control."

The AEF (Agricultural Industry Electronics Foundation) was founded in 2008 by John Deere, CNH, Claas, AGCO, Kverneland Group, Grimme and Pöttinger to standardize ISOBUS interoperability and certify implements/terminals/FMIS software against it. The AEF ISOBUS database (aef-isobus-database.org) is the authoritative compatibility lookup, listing certified functionalities per product: UT, AUX-N/AUX-O, TC-BAS, TC-SC, TC-GEO, TECU.

A separate FMIS-side TC-GEO conformance test (covering field boundaries and prescription maps) was only released by AEF in September 2021 — before that, only the tractor-display/implement side of TC-GEO was independently certifiable.

---

## 2. Full comparison table

| | **John Deere** (Gen4/G5 CommandCenter) | **Trimble** (GFX-350/750/1060/1260, TMX-2050) | **Raven** (Viper 4/4+, CRx) | **CNH built-in** (Case IH AFS Pro / New Holland IntelliView, PLM) | **Ag Leader** (InCommand 800/1200/Go, Compass) |
|---|---|---|---|---|---|
| **UT** | Standard | Requires ISOBUS unlock | Standard on ROS-based VTs | Factory-fit, standard | Requires unlock (except InCommand 1200) |
| **TC-BAS** | Standard w/ AEF-certified implement | Bundled with base TC license | Free with VT | Bundled with TC unlock | Bundled with TC unlock |
| **TC-SC** | Standard (part of base ISOBUS cert) | Bundled with base "Task Controller" license | Separate paid activation key | Bundled with TC unlock | Bundled with TC unlock |
| **TC-GEO** | Free software feature since SU 18-2; using it for prescriptions needs Operations Center / Premium subscription | **Separate paid license** on top of base TC license | Same paid activation key as TC-SC (no separate GEO tier) | Same unlock as TC-SC — one activation covers all three sub-protocols | Bundled with same TC unlock as TC-SC; **standard on InCommand 1200**; unavailable on Compass regardless of purchase |
| **Is TC-GEO itself "an unlock you buy"?** | No — firmware-level free; paywall sits one layer up (cloud data sync) | Yes — explicit extra SKU | Yes — bundled into the SC+GEO activation key | Yes — one dealer activation code, but bundles SC+GEO | Yes on InCommand 800/Go; **no purchase needed on InCommand 1200**; not purchasable at all on Compass |
| **Licensing mechanism** | Activation (permanent, tied to display) or Subscription (renewable, universal displays) | Purchased license SKU (e.g. part no. 96553-10), dealer-installed | Dealer-ordered Activation Key tied to unit serial/barcode via Slingshot portal | Dealer-provided activation code entered on display | Per-display unlock code; tier depends on InCommand model |
| **Max implements via TC simultaneously** | 1 AEF-certified ISOBUS control unit at a time (explicit documented limit on Gen4) | Not restricted to 1; multi-product needs an *additional* "Multi-Product Control" license | Multiple via stackable RCM units, VT-dependent | Multiple — "mixed fleet" positioned as a selling point | Multiple via ISO Load & Go |
| **Notable quirk** | TC-GEO gated by OS version, not purchase — purchase is downstream (data sync only) | Console SKUs literally differ by nameplate: "UT+TC/SC" vs "UT+TC-SC+TC-GEO+multi-product" | Raven's own docs state outright the TC unlock is "not included with the free VT" | Marketed as "built in, not added on" — hardware never separate, only software locked | Compass is a hard ceiling: VT-only, no TC at any price |

---

## 3. Per-manufacturer notes

### John Deere
TC-GEO support was added to the Gen4 CommandCenter as a firmware feature in Software Update 18-2 — not gated by an activation code the way older functionality tiers were. The commercial layer instead sits at data sync: sending completed task data automatically to John Deere Operations Center requires a CommandCenter Premium Activation (or 4640 Universal Display Premium Subscription). So a Gen4/G5 display can technically execute a TC-GEO prescription task locally without Premium — you just can't auto-sync the result to the cloud FMIS without paying.

Two licensing models coexist on JD hardware: **Activation** (one-time, permanent, tied to the specific display — used on integrated displays like the 4600 CommandCenter) and **Subscription** (renewable, lower upfront cost but more expensive over the display's life — used on universal displays like the 4640).

A hard architectural limit worth remembering for interop testing: Gen4 CommandCenter supports only **one** AEF-certified ISOBUS control unit connected at a time.

### Trimble
Trimble's licensing is the most explicitly tiered of the five. The base "ISOBUS Task Controller" license (enabling TC-SC/TC-BAS-level rate and section control) is a discrete SKU. TC-GEO / prescription functionality requires a **further, separate license** on top of that — Trimble's own dealer documentation states the base TaskController license is a prerequisite for the prescription license, meaning a farm can hold TC-SC without ever having bought the TC-GEO layer. This is consistent with what field testing found at Bos (Trimble + Fendt).

Console product listings reflect the tiering directly at the SKU level — one configuration lists "Isobus (UT and TC/SC)" while a higher configuration lists "Isobus (UT, TC SC, TC-GEO and multi-product)" as a distinct, separately-priced option.

### Raven
Raven bundles TC-SC and TC-GEO into a single "Task Controller" activation key — there is no separate GEO-only tier. Raven's operating manual is unusually direct that this unlock is not free: the task controller unlock is explicitly stated as "not included with the free VT capabilities." Activation is tied to the specific unit's serial number/barcode and processed through Raven's Slingshot portal, with the dealer submitting the order.

Since CNH acquired Raven in 2021, some eventual convergence toward CNH's own activation-code model is plausible, but current-generation Viper 4/4+ and CRx units still run the legacy Raven licensing mechanism.

### CNH built-in (Case IH AFS Pro / New Holland IntelliView / PLM)
CNH's approach is architecturally the cleanest of the five: all ISOBUS hardware ships factory-fit ("built in, not added on"), and the entire TC stack (TC-BAS + TC-SC + TC-GEO) unlocks as **one** dealer-provided activation code rather than staged purchases. Case IH's own marketing material states plainly that once the ISOBUS Task Controller is unlocked, the full functionality — including the TC-GEO coverage map and prescription-driven variable rate — is displayed. New Holland's equivalent (PLM ISOBUS Task Controller on IntelliView) markets an identical capability set under its own branding, since both brands share the same CNH backend.

### Ag Leader
Ag Leader ties TC availability to the specific *display model*, not purely to a license purchase. Per Ag Leader's own support documentation: the InCommand 800 requires an ISOBUS unlock enabling both UT and TC together; the Compass display can be unlocked for UT only and **does not support Task Controller at all**, regardless of purchase; the InCommand 1200 ships with UT and TC standard, no unlock needed. A third-party ISOBUS compatibility chart independently lists the Ag Leader Integra/InCommand line as TC-GEO and TC-SC capable once unlocked, confirming the functionality itself is present in the software stack — the InCommand 1200 is simply the one model where the paywall doesn't apply.

---

## 4. Caveats

- Pricing, SKU numbers, and exact activation mechanisms are drawn from dealer/reseller pages and manufacturer support docs current as of August 2026 — these change without much notice (new display generations, subscription-model shifts, CNH/Raven integration). Treat specific part numbers as a starting point for a dealer conversation, not a quote.
- None of this reflects the AEF ISOBUS database's live certification records directly (that requires an AEF account/login) — it's reconstructed from manufacturer documentation, dealer product pages, and third-party compatibility charts. Worth cross-checking a specific terminal/implement pairing against aef-isobus-database.org directly before relying on it for Triton interop testing.
- "TC-GEO licensed" and "TC-GEO functionally exchanging data" are not guaranteed to be the same thing in practice — a license can be active while behavior still needs field verification, which is exactly what the Bos/van Mastwijk test log is for.

---

## 5. References

### AEF / general ISOBUS standard
1. AEF Online — "AEF Tour" (TC-GEO definition, documentation + control) — https://www.aef-online.org/aef-tour/index.html
2. AEF Online — "AEF Announces Update: TRAM becomes TRACK" (TC-GEO technical vs. customer-facing naming) — https://www.aef-online.org/aef-news/aef-announces-update-tram-becomes-track.html
3. AEF Online — "Data Management Update" (FMIS-side TC-GEO conformance test, Sept 2021) — https://www.aef-online.org/aef-news/data-management-update.html
4. Precision Farming Dealer — "AEF Releases Enhanced ISOBUS Conformance Test" — https://www.precisionfarmingdealer.com/articles/4871-aef-releases-enhanced-isobus-conformance-test
5. Farm Progress — "AEF's challenge: Encouraging manufacturers to get ISOBUS-certified" (database functionality list) — https://www.farmprogress.com/farming-equipment/aef-s-latest-challenge-encouraging-manufacturers-to-get-isobus-certified
6. CSS Electronics — "ISOBUS (ISO 11783) Explained" — https://www.csselectronics.com/pages/isobus-introduction-tutorial-iso-11783
7. Wikipedia — "Agricultural Industry Electronics Foundation" — https://en.wikipedia.org/wiki/Agricultural_Industry_Electronics_Foundation
8. Precision Farming Dealer — "8 of the Most Compatible ISOBUS Functionalities for Precision Farming" — https://www.precisionfarmingdealer.com/articles/4442-of-the-most-compatible-isobus-functionalities-for-precision-farming

### John Deere
9. John Deere — "G5 and Generation 4 compatibility chart" (PDF) — https://www.deere.com/assets/pdfs/common/stellarsupport/g5-and-generation-4-compatibility-chart-english.pdf
10. John Deere — 4640 Universal Display product page (AFME) — https://www.deere.africa/en/technology-products/precision-ag/guidance/4640-universal-display/
11. John Deere — Gen4 CommandCenter Release Notes 17-2 (PDF; "supports 1 AEF certified ISOBUS Control Unit") — https://www.deere.com/assets/pdfs/common/stellarsupport/Gen4CommandCenter_ReleaseNotes_17-2_English.pdf
12. John Deere — SU 18-2 New Features (PDF; TC-GEO support added) — https://www.deere.com/assets/pdfs/common/stellarsupport/18-2_Gen4_CommandCenter_NewFeatures_English.pdf
13. John Deere — Generation 4 CommandCenter product page (AU) — https://www.deere.com.au/en/technology-products/precision-ag-technology/guidance/generation-4-commandcenter/
14. John Deere — Gen 4 CommandCenter Premium Activation (US) — https://www.deere.com/en/technology-products/precision-ag-technology/guidance/gen-4-premium-activation/
15. Truland Equipment — "Leveraging Your John Deere Gen 4 Activations & Subscriptions" (Activation vs. Subscription model explainer) — https://www.trulandequip.com/news/precision-ag/leveraging-your-gen-4-activationssubscriptions/
16. Cross Implement — "John Deere Gen4 4600 Premium vs AutoMation Activations" — https://crossimplement.com/news/article/2022/08/john-deere-gen4-4600-premium-vs-automation-activations

### Trimble
17. AS Communications (UK) — "ISOBUS Task Controller License" product page — https://ascommunications.co.uk/product/license-isobus-task-controller/
18. Vantage Northeast — "License, Display ISOBUS Task Controller (GFX-350/750/1060/1260, TMX-2050)" — https://vantagenortheast.com/license-display-isobus-task-controller-gfx-350-750-1060-1260-tmx-2050/
19. Vantage Northeast — "GFX Prescriptions License" (base TC license as prerequisite) — https://vantagenortheast.com/license-display-prescriptions-gfx-350-750-1060-1260-tmx-2050/
20. Trimble Agriculture — "Working with ISOBUS: Task Controller" (blog) — https://ww2.agriculture.trimble.com/blog/working-with-isobus-task-controller/
21. Latitude GPS — "Le guidage GPS par PTx Trimble" (GFX-1260 SKU tiering, FR) — https://www.latitudegps.com/guidage-gps
22. PR Newswire — "Trimble Introduces ISOBUS-Compatible GFX-750 Display System" — https://www.prnewswire.com/news-releases/trimble-introduces-isobus-compatible-gfx-750-display-system-with-advanced-guidance-controller-for-agriculture-applications-300545115.html

### Raven
23. Raven / AgSpray — "Rate Control Module (RCM) Operation" manual (PDF) — https://www.agspray.com/userdocs/documents/016-0171-637-e_-_rcm_operation_manual.pdf
24. Raven — "ROS (Raven Operating Software) Basic Operation Manual" (PDF; "task controller unlock not included with free VT") — https://www.4qte.com/pdf/Raven/Raven%20Operation%20Software%20-%20Basic%20Operation%20Manual%20-%20Viper%204.pdf
25. Raven — 2019 Price Book (PDF; Service Unlock / Slingshot activation process) — https://www.farmco.com/price%20lists/raven/2019%2008-01%20raven%20catalog.pdf
26. Raven Support Center — "Cruizer II" (activation key process) — https://ravenorg.my.site.com/Support/s/topic/0TOUJ0000001BPd4AM/cruizer-ii
27. Raven Industries — "Rate Control Module (RCM)" product page — https://www.ravenind.com/products/applications-booms/rate-control-module

### CNH (Case IH / New Holland)
28. CNH Media — "Case IH Isobus task controller offers expanded compatibility for Case IH customers" (press release) — https://media.cnh.com/emea/case-ih/case-ih-isobus-task-controller-offers-expanded-compatibility-for-case-ih-customers/s/044e0508-73dd-4d0a-ac16-3330edfa6e49
29. Case IH — "ISOBUS | AFS" product page — https://caseih.cc.cnh.com/emea/en-africa/products/afs%C2%AE-advanced-farming-systems/isobus
30. Case IH — "Pro 1200" display product page — https://www.caseih.com/en/cis/products/precision-technology/displays/pro-1200
31. New Holland (Middle East) — "PLM ISOBUS Task Controller" overview — https://nhag.cc.cnh.com/middleeast/en/precision-land-management/products/application-control/plm-isobus-task-controller
32. Case IH — AFS Pro 300/700 Software Operating Guide (PDF; dealer activation codes) — https://icdn.tradew.com/file/201606/1569362/pdf/7703103.pdf

### Ag Leader
33. Ag Leader Community Portal — "ISOBUS, Universal Terminal (Virtual Terminal), & Task Controller" (InCommand 800/1200/Compass TC support matrix) — https://portal.agleader.com/community/s/article/1271?language=en_US
34. BarnDoor Ag — "Ag Leader InCommand 800 | Guidance System" — https://barndoorag.com/ag-leader-incommand-800-guidance-system-4200614/
35. BarnDoor Ag — "Ag Leader InCommand Go 10" — https://barndoorag.com/ag-leader-incommand-go-10-4200702/
36. Ag Leader — "InCommand Go" product page — https://www.agleader.com/incommand-go/
37. Kinze / Parker — ISOBUS PMM V4.00 Release Notes (PDF; third-party VT/TC-GEO/TC-SC compatibility chart including Ag Leader Integra/InCommand, JD 2630/4640, CNH Pro 700) — https://kinzecom.s3.amazonaws.com/Downloads/Release+Notes+-+Parker+ISOBUS+Version+4.00.pdf

---

*This document is a companion reference to `TCGEO_Field_Test_Log.md`, which tracks real-world confirmation of the above against actual farm terminals.*
