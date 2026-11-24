# Lysrytme

Lys der blinker med en valgt frekvens (2–40 Hz) i en Meta Quest 3 eller på en almindelig skærm, med mønstre,
farver, blinkformer og færdige forløb, og med Sleeper Agents støj og musik under. Status: **demo / test**. Den er
testet på flad skærm og i en emuleret Quest 3, ikke i rigtige briller.

## Hvad den gør

- **Frekvens** 2–40 Hz. Skærmen i Quest 3 viser 120 billeder i sekundet (kan sættes til 90 eller 72). En frekvens er
  kun præcis, når den går op i skærmens takt (120/f er et helt tal): 40, 30, 24, 20, 15, 12, 10, 8, 6, 5, 4, 3,
  2 Hz ved 120 Hz. Siden og infotavlen i brillerne siger, om den valgte frekvens er præcis. "Kun præcise
  frekvenser" retter frekvensen til den nærmeste præcise.
- **Blinkform:** hårdt blink, blød bølge, kort glimt, blink og fade, svulmende.
- **Mønstre:** helt felt, farvegradient, ringe, skakternet der vender (lysstyrken er konstant, mønstret skifter),
  spiral, farveskift (to farver skiftes, ingen mørke). Mønstrene sidder fast i forhold til hovedet, som lysbriller.
- **Farver:** varm, hvid, blå/violet, grøn/turkis, regnbue (glider langsomt).
- **Øjne på skift:** højre øje er en halv periode forskudt, så øjnene blinker skiftevis.
- **Rolig midte:** kun udkanten blinker; i midten et svagt fast lys og et lille punkt at se på.
- **Lysstyrke og kontrast** (kontrast under 100 % betyder, at lyset aldrig går helt i sort).
- **Forløb:** Fri; Fokus 30 Hz (15 min); Gamma 40 Hz (20 min); Ro 12 → 10 Hz (15 min); Meditation 10 → 6 Hz
  (20 min); Søvn 8 → 3 Hz (25 min, lyset dæmpes til mørke fra minut 18). Lyset fader ind over 10 sekunder og ud til
  sidst. Frekvensen glider jævnt (i forhold, så 10 → 5 Hz tager lige så lang tid som 6 → 3 Hz).
- **Lyd:** Sleeper Agents fem forvalg (Deep, Rainroom, Forest, Shore, Drift) med støj og ambient-temaer, og en
  **takt-lyd** (tone på 196 Hz eller en sus), hvis styrke følger præcis samme blinkform og rytme som lyset.

Styringen i brillerne og på tastaturet står på startsiden.

## Sikkerhed

Startsiden har en advarsel om epilepsi, som skal krydses af, før man kan starte. Frekvenser i 15–25 Hz får en
advarsel både på siden og i brillerne. Aftrækkeren (eller et knib med fingrene) giver pause: lyset bliver straks
svagt og roligt. Begge greb i 1 sekund afslutter.

## Sådan er den bygget

- `index.html`: startsiden (dansk), alle indstillinger, gemt i `localStorage` (`brokild-lysrytme`).
- `app.js`: WebGL2 og WebXR uden three.js. Hele billedet er én fuldskærms-shader pr. øje, regnet ud fra den retning
  hver pixel ser i (view space), så mønstrene følger hovedet. VR-sessionen er `immersive-vr` med referencerummet
  `viewer`, og appen beder om 120 Hz (`updateTargetFrameRate`).
- **Timing:** lysets fase læses fra lydens ur (`sound.js`) ved hvert billedes visningstid. Ved præcise frekvenser
  tages blinkformen midt i billedet, så et hårdt blink er rent tændt/slukket (ved 40 Hz og 120 Hz: 1 billede tændt,
  2 slukket). Ved upræcise frekvenser tages gennemsnittet over billedet, så ujævnheden bliver små trin i lysstyrke i
  stedet for blink af forskellig længde. `levelsFor(f, fr, form, n)` i `window.__lys` viser billederne.
- `sound.js`: lyden. Sleeper Agents lyd laves af `health-apps/sleep-noise/sound-engine.js` (den samme fil, som
  Sleeper Agent selv bruger) i en worker (`sound-worker.js`), så brillerne aldrig venter et billede på den.
  Takt-lyden er en oscillator ved blinkfrekvensen, bygget af blinkformens Fourier-række, som styrer tonens
  styrke. Frekvensplanen (hvornår frekvensen skifter) deles af oscillatoren og lyset, så de holder takten, og
  `getOutputTimestamp` lægger lyset oven på lyden, som den kommer ud af højttaleren.
- Uden lyd (før første klik) bruger uret `performance.now()`.

## Test uden briller

- `python3 -m http.server 8765` fra roden, åbn `/vr-apps/lysrytme/` (`?flat` starter skærmvisningen med det samme).
- `test/xr.test.cjs`: emuleret Quest 3 (IWER), se kommentaren øverst i filen. Tjekker at sessionen kører ved
  120 Hz, at øjne på skift giver modsat lys i de to øjne, aftrækker = pause, højre pind skifter frekvens, begge
  greb afslutter.

## Ikke set i brillerne endnu

Om billedet holder 120 Hz (infotavlen tæller tabte billeder), hvordan blinket føles ved 40 Hz og under 10 Hz, om
mønstrene er behagelige i stereo, om lyd og lys føles i takt (Quest-browserens lydforsinkelse er ukendt), og
lysstyrken: Quest 3 er en LCD-skærm, så "sort" er mørkegrå, og kontrasten er mindre end i lysbriller.
