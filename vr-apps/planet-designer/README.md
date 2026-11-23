# Planet Designer

Design en stjerne og et solsystem, byg en planet indefra, og se om den kan bære liv, og om et menneske kan gå
på den uden dragt. Hele specifikationen står i `PLANET-STUDIO-DESIGN.md` i repoets rod (filen beholder sit gamle
navn). Ejerens beslutninger står i dens §14.

Live: `https://peterboggild.github.io/BrokildApps/vr-apps/planet-designer/` (på Brokild VR-siden som "Test"-kort, så ejeren kan følge arbejdsversionen).

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

## Status: M3 (23. november 2026)

- **Klimamodel** (`physics/climate.js`): Den er endimensional og regner med årstider. Den finder temperaturen på 36 breddegrader gennem et helt år ud fra stjernens lys, banen og aksehældningen. Med i regningen er varmetransport (skaleret med tryk og rotation efter Williams & Kasting 1997), is, der kaster lyset tilbage (så sneboldplaneter og deres "hukommelse" findes), og drivhusgasser (Budyko-tilpasning med CO₂ og CH₄). Låste planeter regnes ud fra vinklen til punktet lige under stjernen.
  - Jorden giver 14,4 °C med iskant ved cirka 76°.
  - Med 10 % mindre lys forbliver en varm Jord ikke-frossen, mens en frossen forbliver frossen.
  - Modellen kører i en baggrundstråd (`physics/worker.js`) og følger selv med, når man ændrer noget. Den starter fra planetens forrige klima, så man kan fryse en verden og tø den op igen og se hysteresen. Et nyt design eller en forudindstilling starter varmt.
- **Tavlen** bruger klimamodellen til flydende vand (også kun om sommeren eller under is), middeltemperatur, vådtemperatur (Stull 2011, 70 % luftfugtighed i den varmeste sæson) og land med et klima, man kan bo i.
- **Stråling og luft** (`physics/space.js`):
  - Stjernens røntgen- og UV-lys gennem livet. Røde dværge er urolige meget længere, og det er en tilpasning, ikke Jacksons tabel.
  - Exosfærens temperatur og dermed Jeans-parameteren.
  - Den kosmiske kystlinje (Zahnle & Catling 2017, linjen lagt gennem Mars).
  - Kosmisk stråling ved overfladen ud fra luftsøjlens masse (Jorden 0,35 mSv/år, Mars 0,64 mSv/dag).
- **Fanen Klima**: temperatur over breddegrader og den kosmiske kystlinje med solsystemets kloder.
- **Grafik**:
  - Atmosfæren regner spredning af lys gennem luften (molekyler og dis eller støv) med stjernens farve, luftsøjlen og gassernes art. Sollyset på jorden bliver rødt ved terminatoren.
  - Skyer i bånd (ækvator, stormbælter, klare subtroper) med skygger på jorden. Drivhusverdener har et lukket skydække, og låste planeter samler skyerne under stjernen.
  - Is og planter følger klimamodellens temperaturer.
  - Kvalitetsindstillingen styrer også skyernes detaljer, skyggerne og antallet af trin i atmosfæren.

## Status: M4 (23. november 2026)

- **Forklaringskort** (`ui/cards.js`): klik på en række på tavlen. Kortet har fem dele: hvad, hvorfor det betyder noget, i vores solsystem, prøv og (fra niveauet Studerende) mere (ligning, model og usikkerhed). Kilden står der også. Omstridte rækker (termostat/tektonik, aksens stabilitet uden måne, røde dværges aktivitet, magnetfeltets betydning for luften og låste planeter) viser begge synspunkter med kilder. Alle 30 rækker har kort på dansk og engelsk, og det testes.
- **Årsagskort** (`ui/causality.js`, knappen "Årsagskort" eller tasten M): 31 knuder i ni kolonner fra stjernen til mennesker, med de aktuelle tal. Knuderne er farvet efter de rækker på tavlen, de hører til. Klik på en knude for at se alt nedstrøms. En ændring sender en puls ned gennem kæden. Kortene har en knap "Vis i årsagskortet".
- **Jordlighedsindeks (ESI)** står lille under lamperne, med forbeholdet om, at det måler lighed og ikke beboelighed (Schulze-Makuch m.fl. 2011).

## Status: M5 (23. november 2026): VR-værkstedet

- Knappen **"Gå ind i VR"** øverst i 3D-visningen (vises kun, når browseren kan VR, som Quest-browseren). Den åbner **værkstedet**: en platform ude i rummet.
  - **Stjernesmedjen** for enden: stjernen i en ring med to drejeknapper til masse og alder. Man peger på skiven og holder aftrækkeren. Stjernens farve lyser hele platformen op.
  - **Bordet med solsystemet** i midten, hvor planeterne trækkes med strålen og aftrækkeren. Den grønne plads tilføjer en planet.
  - **Planetholderen** til højre med den valgte planet (3,5 m).
  - **Tavlen** på venstre væg.
- **Styring** (samme principper som Illusion Studio, `scene/xr.js`):
  - Venstre pind flyver, og højre pind drejer 30° ad gangen og flytter op og ned.
  - Greb med én hånd flytter verden, og greb med to hænder skalerer og drejer den.
  - **Y eller B** åbner panelet.
  - Kun Meta-knappen centrerer.
- **Panelet** (`ui/vrpanel.js`) tegner de samme faner og rækker som PC'en (`ui/schema.js`) på en tavle med − og + (gentager, når man holder), skydere og chips. Fanen **VR** har gennemskæring, rotation, højdeoverdrivelse, grafikkvalitet, niveau og "Forlad VR". Diagrammerne vises kun på PC.
- I VR er grafikken **lav** som standard (Quest 3), og der er ingen glød (bloom virker ikke i XR). Alle egne shadere tone-mapper selv, så farverne passer uden efterbehandling.
- **Efter første test i Quest 3 (styrt):** den levende landskabs-shader var for tung for brillerne. Ved "lav" (altid i VR og på Quest-browserens flade side) bliver planetens overflade nu **bagt** til billeder på 2048 × 1024 (farve og ruhed, normal og højde). Det sker 16 rækker pr. billede, så intet enkelt billede bliver tungt, og billederne tegnes på en kugle. Skyerne bliver bagt på samme måde, og Mælkevejen regnes pr. hjørne i stedet for pr. pixel. I VR-panelet kan man kun vælge lav eller middel. Går grafikken ned, står der nu en tydelig besked på siden.
- Test: `test/xr.test.cjs` i en emuleret Quest 3 (IWER).

## Efter første test i brillerne (ejerens ønsker)

- **VR-fanen** begynder med **"Start fra"**: Start med Jorden, K-dværg, rød dværg, lille tør verden og drivhusverden. Derefter kommer knappen **"Flyt til et tempereret sted i zonen"**, som også findes på PC under Bane. Den sætter den valgte planet der, hvor den får cirka lige så meget lys som Jorden (S ≈ 0,95), inden for den forsigtige zone og med banens excentricitet regnet med.
- **Måner og planeter kan tilføjes og fjernes i VR**: fanen Måner har skydere for masse og afstand. Fanen Solsystem kan vælge en planet, gøre den beboelig eller til en anden planet, fjerne den (to tryk) og tilføje en ny.
- **Drejeknapperne er relative og fine.** Man griber skiven og drejer fra der, hvor den står, og en hel drejning flytter værdien et halvt spænd. **Træk på bordet** er også relativt (en fjerdedel af bevægelsen), fordi hele den beboelige zone kun er cirka 3 cm på bordet.
- Fanerne i VR-panelet skrumper, så alle ni kan være der (VR-fanen lå uden for kanten).

## Filer

| Fil | Indhold |
|---|---|
| `index.html` | Siden og dens stil. Indlæser `main.js?v=1`. |
| `main.js` | Tilstanden, knapperne i toppen, gem/del. Testkrog: `window.__pd`. |
| `design.js` | Designobjektet: standard, forudindstillinger, validering (hårde grænser), gem, delingslink. |
| `i18n.js` | Al tekst på dansk og engelsk. |
| `ui/schema.js`, `ui/dom.js` | Panelets beskrivelse og tegningen af den i browseren. |
| `physics/*.js` | Rene funktioner uden three.js: `star`, `hz`, `orbit`, `structure`, `atmosphere`, `climate` (klimamodellen), `space` (røntgenlys, kystlinje, stråling), `terrain`, `noise`, `derive` (med `deriveSync`), `ledger`, `const`, `worker`. |
| `data/*.js` | Tabeller kopieret med script fra kilderne (se hovedet i hver fil). |
| `test/physics.test.mjs` | Benchmarks fra spec §5.12: `node test/physics.test.mjs`. |
| `test/flat.test.cjs` | Browsertjek med Playwright og software-grafik: panel, solsystem (klik på pladsen, træk en planet), planet og gennemskæring. |
| `scene/*.js` | `view` (renderer, bloom, kamera, mus, navne), `sky`, `star`, `orrery`, `planet`, `glsl` (støj og landskab), `workshop` (VR-værkstedet), `xr` (session, rig, styring). |
| `ui/vrpanel.js`, `ui/cards.js`, `ui/causality.js`, `ui/charts.js` | VR-panelet, forklaringskortene, årsagskortet, diagrammerne. |
| `test/xr.test.cjs` | VR-test i emuleret Quest 3 (IWER, se filens hoved). |
| `vendor/` | three.js r170 (samme som de andre apps) plus OrbitControls og efterbehandling (bloom), MIT. Indlæses via et import map i `index.html`. |

**Cache:** alle imports har `?v=7`. Hæv dem alle sammen ved hver ændring.

## Kendte forenklinger (står også i koden)

- Mamajeks tabel er skaleret ~2 %, så Solen ved 4,57 mia. år er præcis 1 L☉ og 1 R☉.
- Gough-formlen er generaliseret til andre masser ved at måle alderen i stjernens egen levetid.
  Røde dværges lyse ungdom er ikke med endnu (tidsmaskinen, M6).
- Klimamodellen er kalibreret til Jorden (varmetransport og albedo). Uden for 0,5–2 bar og 100–10.000 ppm CO₂ står der, at den er uden for sit afprøvede område. Tykke CO₂-atmosfærer kræver Williams & Kasting's polynomier, som ikke er med endnu.
- Atmosfærens farver i grafikken bruger tilnærmede spredningstal pr. gas. Meget tyk luft er loftet ved en optisk dybde på 12 (et grafisk valg).
- Landandelen kommer nu fra terrænets højdefordeling (M2). Den gamle tabel ligger stadig i `atmosphere.js`, men bruges ikke.
- Temperaturen over breddegrader er et skøn (ækvator varmere end polerne, aksehældningen udjævner). Den rigtige klimamodel kommer i M3.
- Indre kerne: 0,35 af kernens radius over 0,6 jordmasser og ingen under 0,2 (en pladsholder indtil tidsmaskinen i M6). Mars' kerne bliver 1.695 km mod InSights 1.830 km, fordi modellen ikke kender svovl i kernen.
- Jeans-parameteren bruger en fast exosfære på 1000 K. Opvarmning fra stjernens røntgen- og UV-lys kommer i M3.
- Pladetektonik og termostat er en grov regel med mærket "forskerne er uenige".
