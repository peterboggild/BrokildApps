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

## Status: M1 og M2 (23. november 2026)

- **3D-visningen** (`scene/`) i midten af siden. To "båse": **Solsystem** (tast 1) og **Planet** (tast 2).
- **Solsystemet (M1):** stjernen med solkornene (granulation), solpletter for urolige stjerner, korona og glød (bloom). Farven kommer fra stjernens temperatur. Under den ligger et bord med de beboelige zoner (mørkegrøn forsigtig, lysegrøn optimistisk), snegrænsen og den stiplede grænse for tidevandslåsning. Banerne er farvet efter excentricitet. Planeterne kan trækkes (afstand), og den lille spids ved den fjerneste del af banen kan trækkes (aflanghed og retning). En grøn, pulserende plads viser, hvor en planet mere kan ligge stabilt i zonen (mindst 10 gensidige Hill-radier). Et klik på pladsen tilføjer planeten. Planeterne står stille, når musen er over dem.
- **Planeten (M2):** en terning-kugle med detaljeniveauer, der deler sig, når man kommer tættere på (`scene/planet.js`). Landskabet er én funktion i `scene/glsl.js`, som `physics/terrain.js` gentager linje for linje:
  - kontinenter med forvredet støj
  - bjergkæder med skarpe kamme
  - midtoceaniske rygge
  - "eroderede" bakker, hvor detaljer samler sig i dalene
  Havets overflade udregnes af regnemaskinen ud fra vandmængden og landskabets højdefordeling, ligesom landandelen. Med Jordens hav giver terræn nr. 3 29 % land og 3,8 km middeldybde. Bjergene skaleres med (1/g)^0,45, så Mars får cirka 1,5 gange Jordens højdeforskel. Farverne følger et groft temperaturskøn ud fra breddegrad og højde: is og sne kun hvor der er vand, planter kun hvor der er flydende vand, og rustrødt støv på tørre verdener. Havet er farvet efter dybden og har solglimt. Atmosfæren er en forsmag (M3 bringer den rigtige model). Andre planeter vises forenklet: klippe, is, isgigant og gasgigant med bælter.
- **Gennemskæring (C):** en kvart kile skåret ud. Lagene står i rigtig skala: indre kerne (ingen for små planeter, og ikke for Mars), ydre kerne, kappe og skorpe. Navnene står ved siden af, med streger hen til lagene.
- **Højdeoverdrivelse** kan sættes fra 1 til 40 gange og står altid på skærmen.
- **Grafikkvalitet:** høj, middel eller lav. Den skifter selv ned efter mange langsomme billeder, og et valg man selv har truffet huskes. `?q=low` i adressen tvinger lav.

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
| `test/flat.test.cjs` | Browsertjek med Playwright og software-grafik: panel, solsystem (klik på pladsen, træk en planet), planet og gennemskæring. |
| `scene/*.js` | `view` (renderer, bloom, kamera, mus, navne), `sky`, `star`, `orrery`, `planet`, `glsl` (støj og landskab). |
| `vendor/` | three.js r170 (samme som de andre apps) plus OrbitControls og efterbehandling (bloom), MIT. Indlæses via et import map i `index.html`. |

**Cache:** alle imports har `?v=2`. Hæv dem alle sammen ved hver ændring.

## Kendte forenklinger (står også i koden)

- Mamajeks tabel er skaleret ~2 %, så Solen ved 4,57 mia. år er præcis 1 L☉ og 1 R☉.
- Gough-formlen er generaliseret til andre masser ved at måle alderen i stjernens egen levetid.
  Røde dværges lyse ungdom er ikke med endnu (tidsmaskinen, M6).
- Klimaskønnet giver Jorden cirka 17–18 °C (den publicerede Budyko-fit), ikke 15 °C. Det skal M3's
  klimamodel rette.
- Landandelen kommer nu fra terrænets højdefordeling (M2). Den gamle tabel ligger stadig i `atmosphere.js`, men bruges ikke.
- Temperaturen over breddegrader er et skøn (ækvator varmere end polerne, aksehældningen udjævner). Den rigtige klimamodel kommer i M3.
- Indre kerne: 0,35 af kernens radius over 0,6 jordmasser og ingen under 0,2 (en pladsholder indtil tidsmaskinen i M6). Mars' kerne bliver 1.695 km mod InSights 1.830 km, fordi modellen ikke kender svovl i kernen.
- Jeans-parameteren bruger en fast exosfære på 1000 K. Opvarmning fra stjernens røntgen- og UV-lys kommer i M3.
- Pladetektonik og termostat er en grov regel med mærket "forskerne er uenige".
