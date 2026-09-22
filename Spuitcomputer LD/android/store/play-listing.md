# Play store listing copy

Prepared for NeptuneGPS_Triton#130. Dutch is the default locale: the operators
are Dutch. English is the second. German and French are optional — the app
itself is translated into both, so adding them later costs only the copy.

**The app name is "MeijWorks SprayComputer LD" and must stay that.** "Loofdoes"
is a trademark and must not appear anywhere in the listing.

Character counts below are measured, not estimated; re-check with
`store/check_listing_lengths.py` after any edit.

---

## Dutch (nl-NL) — default

### Title (limit 30)

```
MeijWorks SprayComputer LD
```

### Short description (limit 80)

```
Bedien en kalibreer je MeijWorks loofdoodspuit vanaf telefoon of tablet.
```

### Full description (limit 4000)

```
MeijWorks SprayComputer LD is de bedieningsapp voor de loofdoodspuitcomputer
van MeijWorks. De app verbindt via Bluetooth met de spuitkast op de machine en
laat zien wat die op dit moment doet.

LET OP: deze app heeft MeijWorks-spuitapparatuur nodig. Zonder een gekoppelde
spuitkast kan de app niets doen.

WAT JE ZIET TIJDENS HET WERK

- Snelheid, gevraagde dosering en werkelijke dosering in l/ha, groot in beeld
- De stand van de in- en uitgangen: mixer, vernevelaar, pomp en aux
- Gps-kwaliteit, leeftijd van de fix en positie
- Een alarm zodra de werkelijke dosering meer dan 5 % van de gevraagde afwijkt,
  met keuze uit zes tonen, instelbaar volume en trillen

De verbinding loopt door als het scherm uitgaat, zodat het alarm je ook
bereikt terwijl je rijdt.

KALIBREREN VANAF DE TREKKER

- Volledige kalibratiewizard: eerst de drie knopstanden, daarna de pompcurve
- Alleen de potmeter, of alleen de pomp, als er maar een helft opnieuw moet
- De pomp loopt precies een minuut per punt; de kast klokt dat zelf
- Losse punten van beide tabellen achteraf corrigeren
- Werkbreedte, stuurtimeout, gps-baudrate en minimale fixkwaliteit instellen

De wizard volgt dezelfde stappen als het seriële menu op de kast, dus de
bestaande kalibratieprocedure blijft gelden. Kalibratie wordt pas aan het eind
op de kast opgeslagen; annuleren laat de bestaande waarden staan.

DE KAST BLIJFT DE BAAS

De spuitkast werkt zelfstandig. De app is een scherm en een bedieningspaneel,
geen besturing: valt de verbinding weg, dan gaat de machine gewoon door en
verlies je alleen het kalibreren en het alarm op de telefoon. De pomp wordt
altijd door de kast getimed, nooit door de telefoon.

PRIVACY

De app verzamelt niets en verstuurt niets. Hij heeft geen internettoegang: de
rechten voor internet ontbreken volledig in de app. Je instellingen staan
alleen op je eigen toestel.

Bluetooth wordt alleen gebruikt om de spuit te vinden en verbonden te blijven,
nooit om te bepalen waar je bent.
```

---

## English (en-GB) — second locale

### Title (limit 30)

```
MeijWorks SprayComputer LD
```

### Short description (limit 80)

```
Run and calibrate your MeijWorks haulm sprayer from a phone or tablet.
```

### Full description (limit 4000)

```
MeijWorks SprayComputer LD is the operator's console for the MeijWorks haulm
sprayer computer. It connects over Bluetooth to the controller on the machine
and shows what that controller is doing right now.

PLEASE NOTE: this app requires MeijWorks spraying equipment. Without a paired
sprayer controller there is nothing for it to do.

WHILE YOU WORK

- Speed, requested rate and actual rate in l/ha, large enough to read at a
  glance
- The state of the inputs and outputs: mixer, atomiser, pump and aux
- GPS quality, age of the fix and position
- An alarm as soon as the actual rate drifts more than 5% from the requested
  rate, with six tones to choose from, adjustable volume and vibration

The connection keeps running when the screen goes off, so the alarm still
reaches you while you are driving.

CALIBRATION FROM THE CAB

- Full calibration wizard: the three knob positions first, then the pump curve
- Or just the potentiometer, or just the pump, when only one half needs redoing
- The pump runs for exactly one minute per point, timed by the controller
- Correct individual points of either table afterwards
- Set working width, guidance timeout, GPS baud rate and minimum fix quality

The wizard follows the same steps as the serial menu on the controller, so the
existing calibration procedure still applies. Calibration is written to the
controller only at the end; cancelling leaves the existing values alone.

THE CONTROLLER STAYS IN CHARGE

The sprayer controller runs on its own. This app is a display and a control
panel, not the control system: if the link drops, the machine carries on and
you lose only calibration and the alarm on the phone. The pump is always timed
by the controller, never by the phone.

PRIVACY

The app collects nothing and sends nothing. It has no internet access at all —
the permission is simply absent from the app. Your settings stay on your own
device.

Bluetooth is used only to find the sprayer and stay connected to it, never to
work out where you are.
```

---

## Graphics

| Asset | Requirement | Status |
|---|---|---|
| App icon | 512 × 512 PNG, 32-bit, opaque | **Done** — `store/icon-512.png`, rendered from the app's own vector paths (#122) |
| Feature graphic | 1024 × 500 PNG or JPEG, no transparency | Not started |
| Phone screenshots | at least 2, 16:9 or 9:16, 320–3840 px | Blocked: needs a connected board |
| Tablet screenshots | at least 2 for 7" and 10" | Blocked: needs a connected board |

**Screenshots are deliberately blocked.** They have to be taken from a device
connected to a live controller, so the status screen shows real speeds and
rates rather than dashes. Screenshots full of "–" would be worse than none, and
Play reviewers do look at whether the screenshots show the app working.

Take them in Dutch, since that is the default listing locale.

---

## Categorisation and contact

| Field | Value |
|---|---|
| Application type | App |
| Category | Tools |
| Tags | pick at most 5, e.g. agriculture, utilities |
| Contact email | (MeijWorks address — to fill in) |
| Contact website | (optional) |
| Contact phone | (optional) |
| Privacy policy URL | needs hosting; draft in `store/privacy-policy.md` |
