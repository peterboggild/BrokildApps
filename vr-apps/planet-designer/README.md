# Planet Designer

Design en stjerne og et solsystem, byg en planet indefra, og se om den kan bære liv, og om et menneske kan gå
på den uden dragt. Hele specifikationen står i `PLANET-STUDIO-DESIGN.md` i repoets rod (filen beholder sit gamle
navn). Ejerens beslutninger står i dens §14.

Live: `https://peterboggild.github.io/BrokildApps/vr-apps/planet-designer/` (ikke på Brokild VR-siden før M5).

## Status: M0 (23. november 2026)

- **Regnemaskinen** (`physics/`): stjernen (Mamajeks tabel, lysere med alderen efter Gough 1981, farve,
  plantelys), den beboelige zone (Kopparapu 2013/2014, med planetens masse), tid i zonen, bane og flux,
  stabilitet i Hill-radier, tidevandslåsning som et bånd, måner (Roche-grænse, tidevand, formørkelser),
  planetens radius fra Zengs tabeller, tyngde, undvigelseshastighed, lag, atmosfærens tryk, masse,
  skalahøjde og Jeans-parameter, ozon, kogepunkt.
- **Et midlertidigt klimaskøn** (0-dimensionalt, Budyko-type med CO₂-forcering) i stedet for den rigtige
  klimamodel, som kommer i M3. Det siger selv, når det er uden for sit område.
- **Tavlen** (`physics/ledger.js`): 30 kriterier i fem trin med grøn/gul/rød, "fordi …" og kilde.
  Vådtemperatur og stråling står som "kommer senere".
- **Siden**: panel med faner (én beskrivelse i `ui/schema.js`, som VR-panelet skal genbruge), tre niveauer
  (Opdager / Studerende / Forsker), dansk og engelsk, forudindstillinger (Solen og Jorden, K-dværg, Mars,
  Venus, rød dværg), delingslink i adressen, gem og åbn som fil, nulstil (to tryk).

## Filer

| Fil | Indhold |
|---|---|
| `index.html` | Siden og dens stil. Indlæser `main.js?v=1`. |
| `main.js` | Tilstanden, knapperne i toppen, gem/del. Testkrog: `window.__pd`. |
| `design.js` | Designobjektet: standard, forudindstillinger, validering (hårde grænser), gem, delingslink. |
| `i18n.js` | Al tekst på dansk og engelsk. |
| `ui/schema.js`, `ui/dom.js` | Panelets beskrivelse og tegningen af den i browseren. |
| `physics/*.js` | Rene funktioner uden three.js: `star`, `hz`, `orbit`, `structure`, `atmosphere`, `derive`, `ledger`, `const`. |
| `data/*.js` | Tabeller kopieret med script fra kilderne (se hovedet i hver fil). |
| `test/physics.test.mjs` | Benchmarks fra spec §5.12: `node test/physics.test.mjs`. |
| `test/flat.test.cjs` | Browsertjek med Playwright (se filens hoved). |

**Cache:** alle imports har `?v=1`. Hæv dem alle sammen ved hver ændring.

## Kendte forenklinger (står også i koden)

- Mamajeks tabel er skaleret ~2 %, så Solen ved 4,57 mia. år er præcis 1 L☉ og 1 R☉.
- Gough-formlen er generaliseret til andre masser ved at måle alderen i stjernens egen levetid.
  Røde dværges lyse ungdom er ikke med endnu (tidsmaskinen, M6).
- Klimaskønnet giver Jorden cirka 17–18 °C (den publicerede Budyko-fit), ikke 15 °C. Det skal M3's
  klimamodel rette.
- Landandelen ud fra havmængden er en tabel-heuristik for jordlignende terræn.
- Jeans-parameteren bruger en fast exosfære på 1000 K. Opvarmning fra stjernens røntgen- og UV-lys kommer i M3.
- Pladetektonik og termostat er en grov regel med mærket "forskerne er uenige".
