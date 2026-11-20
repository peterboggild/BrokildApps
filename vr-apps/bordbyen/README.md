# Bordbyen

Byg en lille by på dit rigtige bord i blandet virkelighed, og bliv så gjort lille, så du kan køre race i dens gader. WebXR til Meta Quest 3
(`immersive-ar`, passthrough), three.js r170 i `vendor/`, almindelige ES-moduler uden byggetrin. Kører også fladt på en computer (`?flat`).

## Filer

| Fil | Indhold |
|---|---|
| `index.html` | Startsiden (dansk): knapper, valg af styring, vejledning. Indlæser `game.js?v=N`. |
| `game.js` | Renderer, tilstande (find bord, byg, vælg bil, ned, kør, op), paletten, tegning, bilvalg, overgangen, kørestyring, instrumentbrættet, den flade forhåndsvisning og testkrogene (`window.__bb`). |
| `city.js` | Byen: gitteret, veje, grunde, porte; alle bygninger lavet i kode; kollisioner, underlag, modstandernes rute; gem/indlæs; startbyen; landskabet uden om. |
| `cars.js` | De fire biler, bygget i kode, og hvordan de kører (`HANDLING`). |
| `race.js` | Kørsel (arkadefysik), modstanderne, porte og omgange. |
| `audio.js` | Al lyd, lavet i koden: motorer, bump, nedtælling, mål, klik. |
| `toon.js` | Tegneserie-looket: flade farver i tre toner og sorte tegnestreger. |

**Cache:** `index.html` indlæser `game.js?v=N`, og `game.js`/`race.js` indlæser de andre med samme `?v=N`. Hæv dem alle sammen ved hver ændring
(nu **v=3**).

## Sådan hænger det sammen

* **Ét sæt modeller, to skalaer.** Byen er bygget i meter (en celle er 8 m, en vejs bredde). På bordet vises den i 1:160 (en celle er 5 cm, en
  bil 2,5 cm); når man kører, i 1:1. Kun byens rodmatrix skifter (`holder.matrix`).
* **Bordet** findes i rumscanningen (`plane-detection` og `mesh-detection`, mærket `table`): bordpladens højde, midte, retning og størrelse
  (hovedakser af punkterne). Det nærmeste bord vælges, og brættet drejes, så den lange side vender på tværs af dig. Findes intet bord på 3 s,
  kan man markere to modsatte hjørner med controllerens spids. Venstre stick ned finder bordet igen.
* **Gitteret:** brættets størrelse er bordets, op til 40 × 40 felter. Hvert felt er vej, en del af en grund (et rektangel med én slags ting) eller
  græs. Bygninger erstatter det, de lægges over; veje erstatter grunde.
* **Bygninger** laves af kasser, prismer, kegler og kugler, slået sammen til ét net pr. grund, med farver pr. hjørne. Hver grund har sit eget
  tilfældige frø, så den ser ens ud hver gang: rækkehuse med haver og skorstene, byhuse med butiksmarkiser, højhuse (nogle med en aftrapning),
  rådhus med urtårn og kobbertag, kirke med spir, parker med træer, stier og sø, torve med springvand og lygter.
* **Veje** får fortov og kantsten, hvor naboen ikke er vej, midterstriber på lige stykker, en stiplet bue i sving og fodgængerfelter ved kryds.
* **Ruten:** start/mål og portene i den rækkefølge, de blev sat. Modstanderne kører den korteste vej (bredde først) gennem veje og torve fra port
  til port, rundet af (Chaikin). Kan en port ikke nås, står der hvorfor på paletten, og man kan ikke starte.
* **Ned i byen:** byens matrix blandes fra bordet til køreskalaen, så kameraets sted i byen (førerens øjne, eller et punkt bag og over bilen)
  rejser fra bordet hen til dit hoved, mens skalaen vokser fra 1:160 til 1:1 (2,6 s). Himlen og tågen toner frem i sidste del, så rummet forsvinder.
  Op igen er det samme baglæns.
* **Kørslen:** byen flyttes rundt om dig, ikke dig gennem rummet. Dit hoved bestemmer udsigten (du kan kigge rundt i bilen), men bilens plads
  følger bilen. Højre stick ned retter ind efter, hvor du sidder nu.
* **Fysik:** fart langs næsen, styring som en cykelmodel med mindre udslag ved høj fart, lidt sideglid. Vej og torv er hurtige, græs og park holder
  hver bil til sin egen brøkdel af topfarten (Bulderen klarer græs bedst). Bygningerne (grunden minus fortov), træer, springvand og portenes stolper
  skubber bilen ud og tager fart af den. Biler skubber hinanden.
* **Modstanderne** følger ruten i hver sin vognbane, kigger længere frem jo hurtigere de kører, og sætter farten ned efter hvor meget ruten drejer
  forude. En lille gummibåndseffekt holder løbet samlet. Sidder én fast, bakker den og prøver igen.
* **Instrumentbrættet** viser plads, omgang, tid og fart, og nedtællingen. I førersædet sidder det på instrumentbrættet; bag bilen svæver det over den.
* **Lyd:** motoren følger farten gennem fire gear; modstandernes motorer høres, hvor de er.

## Tegneserie-looket (v=2)
* **Flade farver** i tre toner (lys, mellem, skygge) i stedet for bløde overgange, og **sorte tegnestreger** om alt, der står op: huse, tage,
  vinduesrammer, træer, lygter, biler, skyer. Stregen er lige tyk på skærmen, nær og fjern, og toner ud i disen, når man kører.
* Stregen laves ved at tegne hver figur en gang til, sort, lidt udvidet og kun bagsiden (den viser sig kun langs kanterne). Flade ting, der ligger på
  andre (asfalt, striber, stier, fliser, græs), har ingen streg, ellers ville byen blive overtegnet.
* **Flere detaljer:** gadelygter på hvert andet fortov, trafiklys på hjørnerne ved kryds, brandhaner, kloakdæksler; huse med sokkel, tagryg,
  skorsten med hætte, indrammede vinduer, blomsterkasser, farvede døre med trin, havestier, hæk og postkasser; byhuse med altaner, stribede
  markiser, butiksskilte, vandtanke og ventilationskasser på taget; højhuse med farvede hjørnepiller, helipad eller mast, plantekummer ved
  indgangen; rådhus med kvistvinduer, søjler, trappe, flag og urskiver med visere; kirke med rosevindue, buet dør og gravsten; parker med hæk,
  blomsterbede, legeplads (rutsjebane og gynger), lygter, bænke og en and i søen; torve med markedsboder, bænke og blomsterkrukker; blomstrende
  træer; skyer over markerne.
* Lygter, trafiklys, bodenes og legepladsens stolper er noget, bilerne kan køre ind i.

## Sammenstød, udsyn og overblik (v=3)
* **Sammenstød mellem biler:** hver bil er to cirkler langs sin længde. Overlap skubbes fra hinanden med det samme (biler kan ikke køre igennem
  hinanden), og stødet er et rigtigt stød i berøringspunktet, med bilernes vægt (Brummer er tungest, Boblen lettest) og lidt fjedren. En bil, der
  bliver ramt bagfra, skubbes frem ad vejen; en, der rammes på siden, skubbes til siden og drejes rundt.
* **Når du rammes:** et ryk i udsynet (en lille fjeder, højst 12 cm, der svinger tilbage), et bump i lyden, og begge controllere vibrerer efter, hvor
  hårdt stødet var.
* **Udsyn fra førersædet:** din egen bil tegnes uden tegnestreger og uden tag, når du sidder i den. Stregerne laves af bilens sorte inderside, og den
  fyldte hele udsynet i de lukkede biler. Udefra ser bilerne ud som før; indefra er de åbne, med instrumentbræt og rat foran dig.
* **Kameraet bag bilen** rykker nærmere, hvis en bygning står i vejen, så det aldrig havner inde i en mur.
* **Overblik (X):** du løftes op til bordet, mens løbet kører videre, og ser bilerne køre gennem byen; din egen bil holder stille, og en gul pil
  over den viser hvor. X igen tager dig ned i bilen, rettet ind efter hvor du står nu. Y stopper løbet.
* **En roligere start:** modstanderne kører fra startgitteret række for række, bliver i deres egen vognbane, holder afstand til bilen foran og
  skifter bane, hvis den er langsommere.

## Styring

Bygning (ved bordet): paletten på venstre hånd, peg og tryk, eller højre stick til siden. Højre aftrækker tegner. B fortryder, A går til valg af
bil, hold X i 2 s for et tomt bord, venstre stick ned finder bordet igen.

Kørsel: højre aftrækker er gas, venstre er bremse/bak. Styring med stick, eller med rattet: hold begge greb inde og drej hænderne. B skifter
kamera, A starter forfra, Y gør dig stor igen, højre stick ned retter udsigten ind.

Fladt: mus og taster (se startsiden). Testkroge på `window.__bb` (`S`, `city`, `race`, `orbit`, `toPick()`, `startDrive()`, `growUp()`,
`choose(k)`, `setView(v)`).

## Ikke prøvet i headsettet endnu

Alt herover er prøvet i den flade forhåndsvisning. Ikke prøvet på en Quest 3: at bordet findes i rumscanningen (og at markere det med hånden),
paletten på hånden, tegning med strålen, overgangen ned i byen (komfort, og om det føles som at blive lille), køresyge i førersædet og bag bilen,
rattet, billedhastigheden med en fuld by i 1:1, og lyden.

## Idéer, ikke bygget

* Flere slags grunde (fabrik, havn, stadion, tankstation), broer, rundkørsler.
* Bilmodeller efter dine egne tegninger (som rumskibet i Portalkniven).
* Gemme flere byer, og dele en by som et link.
* Trafik og fodgængere i byen, nat med gadelys.
