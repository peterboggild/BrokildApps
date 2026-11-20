# Ræs i Bordbyen

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
(nu **v=9**).

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

## Graphic novel: nat og regn (v=4)
Et valg på startsiden ("Stil"), og Y ved bordet skifter undervejs (N på computeren). Byen er den samme, kun looket skifter:
* **Farverne** bliver mørke, afblegede og kolde (blålige), og i skyggerne lægges **krydsskravering** som pennestreger: et lag i mellemtonen og et
  til, på tværs, i den mørkeste. Stregerne ligger i byens egne meter på den flade, overfladen vender mest mod (vægge op og på tværs, jorden fladt),
  så de sidder fast på husene i begge øjne og i begge størrelser, og de toner ud, hvor de ville blive finere end en pixel. Tegnestregerne er lidt
  tykkere.
* **Lys i mørket:** gadelygterne gløder, og omkring 40 % af vinduerne er tændt med varmt lys (valgt ud fra hvor de sidder, så det er de samme hver
  gang).
* **Himlen** er en lav stormhimmel med drivende skyer og en diset måne; disen er mørk og tættere.
* **Regn:** streger, der falder gennem en kasse omkring dig, når du kører. Når byen står på bordet (bygning, valg af bil, byoverblik), regner det ikke.
* **Lyn og torden** hvert 10. til 28. sekund, når du er nede i byen: himlen og lyset blinker to gange, og tordenen ruller lidt efter.
* **Musik**, lavet i koden mens den spiller: langsom noir-jazz i D-mol, 68 slag i minuttet, med mørke akkorder på en blød flade (Dm9, B♭maj7,
  Gm9, A7♭9, to takter hver), en gående kontrabas, børstetrommer på to og fire og et swingende bækken, og spredte vibrafontoner ind i et langt
  ekko. Under det hele regnens sus.

## Tunneller, træer man kan ramme, hele bilen kolliderer, nyt navn (v=9)
* **Navnet** er nu "Ræs i Bordbyen" (mappen og adressen er stadig `bordbyen`).
* **Tunneller:** et lag under byen (gulv i −4,5 m, loft i −0,6 m; brættet er blevet en 6 m tyk blok, så tunnellerne er inde i den). "Tunnel" strygs
  som en vej, kun tunnelveje kan ligge dernede, og de må gå under alt. Tosporede, med vægge hvor de er lukkede, lamper i loftet og striber.
  "Nedkørsel" er en rampe over to felter fra overfladen ned til tunnellen: skrå asfalt, betonmure i siderne, en tunnelmunding med gule og sorte
  striber, og terrænet om den gøres fladt. På bordet (byggeri, bilvalg, byoverblik) ses tunnellerne som et brunt, halvgennemsigtigt bånd ovenpå.
* **Niveauer:** en bil er på overfladen (0), på en rampe eller i en tunnel (−1); den skifter ved rampens ender. Sammenstød med vægge regnes på
  bilens eget niveau (i tunnellen er alt, der ikke er tunnel, fast; rampens mure gælder alle; foden af en rampe er lukket for biler oppe på
  overfladen), og biler mødes kun på samme niveau. Modstandernes vejsøgning går gennem begge lag, så de også bruger tunnellerne som genveje.
  I tunnellen regner det ikke, og kameraet bag bilen holder sig under loftet.
* **Træer:** står mindst 3 m inde fra kanten af parker, haver og kirkegårde, så kronerne ikke hænger ud over vejen; alle træer kan man støde ind i.
* **Hele bilen kolliderer:** to cirkler langs bilens længde holdes ude af huse, træer og mure (før kun én cirkel om midten, så næse og hale kunne
  stikke ind i ting).

## Vælg selv bordet, ingen dale under bordet, langsommere uden for vejen (v=8)
* **Valg af bord:** alle flader, der kan være et bord, findes (mærket bord eller skrivebord i rumscanningen, og vandrette flader uden mærke i 30 til
  130 cm højde); den samme flade fundet to gange tælles én gang. De sorteres med dem foran dig først. Et grønt felt viser, hvor byen kommer til at
  ligge: aftrækkeren vælger det, B eller højre stick viser det næste, X mærker bordet med hånden. Før valgte spillet selv det nærmeste bord, og det
  var ofte forkert.
* **Terrænet går ikke under bordpladen:** "Dal" er blevet til "Sænk", der sænker bakker igen, men aldrig under nul. Det sænkede terræn forsvandt
  ind i brættets plade (og ville stikke ned i det rigtige bord). Ældre byer med terræn under nul løftes op ved indlæsning.
* **Uden for vejen** nu højst en tredjedel af farten (Bulderen 40 %).

## Bakker, ingen gadelygter, ujævnt uden for vejen, mindre biler, stillingen (v=7)
* **Bakker og dale:** to nye værktøjer. Et træk hæver (eller sænker) et område blødt med op til 3,5 m i midten, og det klinger ud en celle uden for
  rektanglet; flere træk lægges oven i (mellem −10 og +16 m). Højden findes i hvert cellehjørne, men ikke på brættets kant, så kanten forbliver
  flad. Veje, fortove, græs, parker, torve og gitterlinjerne lægges ned på terrænet (hvert hjørne løftes); husene står lodret i højde med det
  højeste punkt under grunden, på en stensokkel ned til det laveste.
* **Bilerne kører i terrænet:** de følger højden og vipper med hældningen (din egen ikke, når du sidder i den, for så ville motorhjelmen dække
  udsynet op ad bakke); op ad bakke bremser tyngden, ned ad bakke speeder den op.
* **Gadelygterne er væk fra vejene**; trafiklysene står yderst på hjørnet, og bilerne kan ikke længere støde ind i dem (heller ikke lygterne i
  parker og på torve).
* **Uden for vejen:** alle biler højst halv fart (Bulderen 60 %), og det ryster: bilen hopper og vipper tilfældigt, udsynet får små stød, controllerne
  brummer i korte stød, og en gang imellem bumper det i lyden.
* **Bilerne er 20 % mindre** (krop, øjne og sammenstødscirkler).
* **Stillingen:** instrumentbrættet viser alle bilerne i rækkefølge med en prik pr. port (grøn når taget) og "MÅL"; i byoverblikket svæver et
  navneskilt med "Porte x/y" over hver bil.

## Kroket-regler, nye knapper, rat i hænderne, sædet følger hovedet (v=5)
* **Løbet er som kroket:** alle porte skal tages, i vilkårlig rækkefølge, og så gælder det om at komme først over start/mål. Modstanderne planlægger
  selv: den nærmeste port, de mangler, ad vejene, så den næste, og til sidst mål. Pladsen regnes efter antal porte og afstanden til den næste.
* **Portene ses tydeligt:** over hver port står en svag lyssøjle og en svævende diamant, 30 m oppe (19 cm på bordet); dine porte bliver grønne.
* **Knapper i bilen:** højre aftrækker gas, højre greb bremse og bak (venstre aftrækker også). A skifter førersæde og kamera bag bilen; hold A: forfra.
  B: byoverblik ved bordet, mens løbet kører, og du kan stadig styre din bil; B igen ned. X: pause med byoverblik; X igen fortsætter, nede i bilen.
  Y: stop løbet.
* **Rattet følger hænderne** (valget "Rat"): rattet sidder mellem dine hænder, i deres størrelse, og drejer med linjen mellem dem; man behøver
  ikke holde grebene inde (så højre greb er fri til bremsen).
* **Sædet følger hovedet:** førerens øjne holdes ved dine: højden følger dit hoved hele tiden (sid eller stå), og læner du dig mere end 12 cm væk,
  glider sædet efter. Så kan hovedet ikke komme op gennem taget.
* **Instrumentbrættet** sidder lavt på forruden, halvgennemsigtigt, under synslinjen; bag bilen svæver det højt og mindre.

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
