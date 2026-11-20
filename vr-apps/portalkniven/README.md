# Portalkniven

Et spil til Meta Quest 3 (WebXR, `immersive-ar`, ingen installation). Med en kniv skærer man en ring i luften i sin egen stue; den tænder som en grøn
knitrende oval, der er en åben dør til en anden version af rummet. Se ind, gå igennem, gå tilbage.

Det begyndte som en teknisk test af selve døren og er siden vokset til:

* **en historie** (brev fra en selv i en anden dimension, kniven, ni dele til en tidsmaskine i tre verdener, maskinen bygget i hånden, en rejse til fortiden og hjem igen),
* **tre verdener med hver sit nummer** (den grønne, metalverdenen, klippeverdenen), bygget oven på headsettets scanning af stuen, med ting der giver efter for hænderne og bandets mærker,
* **verdensrummet** (flyvning, warp og et sort hul), **seks fraktale dimensioner** (flyv, omform, skift farver) og **et firedimensionalt rum**,
* **hænder**, der følger controllerne, en spiral, når døren skifter verden, og sikkerhed hele vejen (advarselslyd, åbning mod den rigtige stue tæt på).

Resten af denne fil er skrevet i den rækkefølge, tingene blev bygget; de nyeste afsnit står nederst.

Siden: `https://peterboggild.github.io/BrokildApps/vr-apps/portalkniven/`

## Kniven
* Hold udløseren og skær en løkke i luften med knivens spids; et grønt snit følger spidsen (gnister, sus og haptik). Luk løkken, og ringen tænder:
  glimt, chokring, et slag og knitrende lyd, og ringen vokser fra snittets størrelse til fuld dørstørrelse. Slip før den lukker, og snittet falmer.
* Ringen er altid en opretstående oval, 1,2 m × 2,2 m. Snittet bestemmer kun *hvor* døren står og hvilken vej den vender (normalen findes med Newells metode).
* Grib-knappen på den anden controller flytter kniven til den hånd.

## Hvordan det virker
* Ringens indre er en maske, der skriver 1 i **stencil-bufferen** (kun hvor den er synlig, dvs. ikke skjult af noget foran).
* En fuldskærmsflade ved far-planet, kun hvor stencil = 1, nulstiller dybden og maler den anden verdens himmel. Derefter tegnes
  den anden verdens objekter, også kun hvor stencil = 1, og klippet til bagsiden af ringens plan.
* Hvert øje renderes fra sin egen position, så vinduet er et rigtigt vindue (parallakse), ikke en tekstur.
* Verden A = din stue (passthrough). Verden B = den lilla verden (uigennemsigtig). At gå igennem skiven bytter, hvilken der er "hjemme".
  Set fra B er hullet alpha 0, så man ser den rigtige stue.
* Har headsettet scannet rummet (`mesh-detection`), bliver de rigtige vægge og møbler til B's vægge (omklædt).
* Tavlen på venstre controller viser stencil-bits (skal være 8), antal gennemgange og scanningsstatus.

## Kendte begrænsninger i testen
* Rummets scanning skjuler ikke ringen bag rigtige møbler endnu (ingen occluder).
* ±3 cm omkring ringens plan ser det ene øje ringen fra den anden side i et øjeblik.
* Ingen ankre: ringen sidder fast i sessionens koordinater.

## Den anden stue (organisk look)
* Mørk, fugtig og bevokset: overflader med mos-agtig støj og et svagt, langsomt pulserende net af lysende årer (i verdensrum, så en væg ser ens ud fra begge sider).
  Rummets scanning (hvis givet) får den samme hud, og et gulv under alt.
* Tynde, bøjede stilke med en lysende knop, bløde bælge på gulvet, langsomt svævende sporer, tåge og en tæt, kølig himmel. Alt svajer og lyser meget svagt.
* Rumlyset (WebXR `light-estimation`) gør den anden stue lysere eller mørkere, så den passer til dit rum. Tavlen viser værdien (`Rumlys`) og om
  `camera-access` blev givet (`Kamera`); kameraet bruges ikke endnu.

## Dimensioner (`worlds.js`)
Fire verdener: **din stue** (passthrough), **den grønne**, **metalverdenen** og **klippeverdenen**. Højre thumbstick (vip eller tryk; Q/E i preview)
vælger, hvad døren fører til, altid blandt de verdener du *ikke* står i. Går du igennem, bytter "her" og "der" plads, så inde fra en anden
verden kan døren føre til din stue eller til en af de andre.

* Alle verdener er det samme rum: rumscanningen tegnes med verdenens overflade. Overfladerne er procedurale i verdensrum (triplanare opslag i én
  lille flisebar støjtekstur + bump fra skærm-afledte), så de kræver ingen UV'er og passer på enhver scannet form.
* **Metal:** nittede stålplader i forbandt (børstet, plettet, rust i sømmene, enkelte glødende sømme), søjler, dragere, lamper, kobberrør med ir,
  en ventil, et stort langsomt hjul; en hal med lysbånd højt oppe, der forsvinder i dis.
* **Klippe:** lagdelt sandsten med revner, støv på det der vender opad, kampesten (støjformede kugler med brudflader), stensøjler, småsten
  (instancer); skumringshimmel med lav sol, tynde skyer og tre rækker taffelbjerge i disen.
* Tåge, farvetone og himmel er pr. verden; refleksioner kommer fra et lille bagt lys-sæt pr. verden. ACES tone mapping.
* Alle shadere kompileres ved start, så skift ikke hakker.

## Senere tilføjelser
* **Knivens greb:** midten af skaftet ligger nu i controllerens grebspunkt (før lå hånden på klingen). `?tilt=grader` vipper klingen i hånden, hvis den skal finjusteres.
* **Den grønne verden:** våd jord med mos på det der vender opad, vand i lavningerne, mosgroede sten, rødder hen over jorden, svampe i klynger
  (svagt lys under hatten), græstotter, få tynde stilke. Mindre mættet og mindre selvlysende end før.
* **Musik:** hver verden har sit nummer (128 kbit/s MP3): den grønne `music/doedt-trae.mp3` (Dødt Træ), metalverdenen `music/rotation.mp3` (Rotation), klippeverdenen
  `music/tidejord.mp3` (Skyggemennesket – Tidejord). Numrene er udgivet i 2025. Det toner ind og ud, og lidt af det siver gennem døren, når døren fører dertil.
* **Sikkerhed:** uden rumscanning advarer tavlen ("INGEN RUMSCANNING: pas på møbler!"), når man står i en anden verden.
* **Luk døren:** et snit hen over ringens kant (spidsen krydser ovalen i dørens plan, mens udløseren holdes) opløser døren. Står man i en anden
  verden, bliver man der, til man skærer en ny dør; stikken vælger stadig, hvor den nye dør fører hen.
* **Musik, ét nummer ad gangen:** kun den verden, man står i, spiller. Når man går ud, toner nummeret ud på cirka et sekund, og det nye toner
  først rigtigt ind, når det gamle er væk. Kun fra den rigtige stue siver lidt musik gennem døren.

## Historien, kapitel 1 (`story.js`)
Startsiden har to knapper: **Start historien** og **Fri leg** (kniven fra start, ingen historie). `?flat&free` er fri leg i preview.

1. Tom stue, tomme hænder. En brummende lyd (rumlig) kommer fra et **brev**. Peg og tryk (eller rør): brevet folder sig ud foran dig; tryk igen for at folde det sammen.
   Brevet er fra dig selv, lidt senere, og forklarer loopet. Det kan læses igen senere.
2. Først når brevet er læst, viser **kniven** sig; lyden flytter til den. Rør ved den, så er den din (i den hånd, der rørte).
3. Skær en dør og gå igennem. I hver verden: **luk døren bag dig** (snit over ringen); først da viser de **tre dele** sig. Rør en del (hånd eller knivspids)
   for at tage den; den følger med i et bælte ved hoften. Tavlen viser `Dele: n af 9` og den aktuelle opgave; korte beskeder svæver foran dig.
4. Når den første verdens tre dele er fundet, låses **højre joystick** op (vælg næste verden).
5. **Altid hjem til stuen:** indtil historien er slut, fører døren fra en anden verden altid hjem, og joysticken virker kun i stuen.
   Når en verdens tre dele er fundet, skærer man en dør hjem; hjemme låses joysticken op, så næste verden kan vælges.

### Kapitel 2
6. Når alle ni dele er fundet, står **tidsmaskinens stel** ("TEMPUS MK I", bordmodel ca. 62 × 38 × 36 cm) dér, hvor brevet lå, på bordet
   eller på et lille bord, hvis der ikke er noget at stille den på. Hver del har en plads, vist som et svagt lysende omrids. Tag en del fra bæltet
   (hold udløseren nær den), før den hen til sin plads, og den klikker i. Slipper man for tidligt, vender delen tilbage til bæltet. En lampe tændes pr. del.
7. Med alle ni dele på plads varmer maskinen op. Rør ved **håndtaget**: et hvidt glimt, og du er i **fortiden** (samme stue set gennem et varmt, ravgult slør).
   Kniven i din hånd er væk; den er ikke lavet endnu.
8. **Klinge, skaft og sten** ligger tre steder. Hold udløseren for at tage en; før to sammen, og de sætter sig sammen. Når kniven er hel, lyser en ring,
   hvor du fandt den. Læg kniven dér: loopet lukker sig, sløret letter, og historien er slut.
9. Bagefter er det **fri leg**: rør ved kniven igen, og joysticken virker overalt.

### Sikkerhed
* Brev, kniv, tidsmaskine og markeringer ses kun i den rigtige stue (ikke i de andre verdener).
* **Advarselslyd:** rumscanningen lægges i et gitter af 8 cm-celler; når hovedet er under ca. 38 cm fra noget scannet (over knæhøjde) i en anden verden,
  kommer en tone, der bliver højere og hurtigere, jo tættere man er. Den kan ikke advare mod det, scanningen ikke har set.
* Startsiden og første besøg i en anden verden advarer tydeligt: gå meget forsigtigt; scanningen fanger ikke alt, der stritter ud.

* Brev, kniv og dele lægges oven på det, rumscanningen har dér (bord, sofa, gulv), så de kan nås; uden scanning svæver de i håndhøjde.
* De ni dele: fluxspole, vakuumrør, tandhjulssæt (den grønne); tidskrystal, kronometer, kondensator (metal); antenne, styrehåndtag, kraftkerne (klippe).
* **Sikkerhed (valgfrit på startsiden, slået til som standard):** hvor den scannede stue er tæt på hoved eller hænder, tynder den ud, så den rigtige stue ses igennem.

## Ydelse
* Overfladerne bruger nu ét opslag (`tri1`, projektion langs den dominerende akse) til fine detaljer og kun den dyre triplanare blanding (`tri`)
  til de brede mønstre, der skal løbe ubrudt rundt om en sten. Klippe: fra 33 til 12 teksturopslag pr. pixel; stål: 18 til 6; mos: 18 til 9.
* Himlen tegnes efter verdenen med dybdetest, så dækkede pixels aldrig beregnes (i et scannet, lukket rum er det næsten alle).
* Opløsning: startsiden har *Grafik* (Skarp 1.0, Normal 0.85, Flydende 0.7 af headsettets opløsning) og fast foveation på højeste trin.

### Placering og hjælp til at finde delene
* Brev, kniv, dele og knivens tre stykker lægges kun et sted, der (1) har gulv eller møbel under sig i scanningen, så det ikke er på den anden side af en væg,
  (2) ikke er højere end et bord, (3) kan ses i fri sigtelinje fra dér, hvor man står (stråle mod scanningen), og (4) ikke ligger inde i eller klos op ad noget scannet.
  Findes der intet godt sted, lægges tingen lige foran en.
* Leder man længe i en verden: efter 3 minutter blinker de manglende dele kraftigt og kalder med en lyd fra deres position; efter 5 minutter sender de en lysstråle op,
  der ses hen over møbler.
* **Brevet tages med:** når brevet er læst første gang, ligger det ikke længere i rummet. Det sidder i bæltet (yderst til venstre) som de andre ting, man har.
  Læg hånden på det, og tryk på udløseren, så folder det sig ud igen. Arket er 66 × 90 cm og holder sig foran øjnene i øjenhøjde.

### Senere ændringer i historien
* **Én del ad gangen:** når døren er lukket, bliver én del til: den vokser ud af ingenting over ca. 3,6 sekunder, sitrende og med en lyd fra sit sted
  (to toner, der stiger og slår mod hinanden, ringmoduleret skær, et pust af støj, en lille klokke til sidst). Den kan først tages, når den er kommet helt frem.
  Når den er taget, bliver den næste til, så langt fra den forrige som rummet tillader.
* **Intet ligger lavt:** brev, kniv, dele og knivstykker ligger på et bord, hvis der er et, ellers svæver de i håndhøjde (ca. 95 cm), aldrig ved gulvet.
* **Tidsmaskinen på et bord:** der søges efter en plan flade i bordhøjde, stor nok til maskinen, med frit rum over; maskinen stilles vinkelret på bordet.
  Findes ingen, står den på sit eget lille bord på åbent gulv, mindst ca. 40 cm fra vægge og møbler.
* **Hjem igen:** når kniven er lagt i fortiden, går man hen til tidsmaskinen og trækker håndtaget tilbage. Først hjemme i ens egen tid er det fri leg, og kniven ligger, hvor man fandt den.
* **Fortidens skær:** ud over det ravgule slør får rummets scannede form et svagt glødende skær, stærkest langs kanterne, med en langsom bølge igennem.
* Søgning efter steder bruger scanningens 8 cm-gitter (få opslag) i stedet for stråler mod hele scanningen; kun det valgte sted finjusteres med én stråle.
* **Tidsrejsen** varer nu ca. 7 sekunder hver vej: en tunnel af ringe og eger, der farer forbi i spektrets farver og blinker, et hvidt glimt i vendepunktet,
  tidsmaskinens tandhjul, ur og kerne, der løber løbsk, lamperne der flimrer, controllerne der ryster; lyden er et stigende brøl, et ur der tikker hurtigere og hurtigere,
  et drøn når verden vender, og det hele der falder til ro.
* **Med eller uden hjælpetekster:** vælges på startsiden. Uden hjælpetekster vises kun brevet, sikkerhedsadvarslen, joystick-vinket og slutkortet; tavlen viser kun, hvor man er, og hvor mange dele man har.
  `?flat&nohelp` i preview.
* **Joystick-vinket:** når joysticken er låst op, man står i stuen, og der står en dør til en verden, man allerede har tømt, vises en lille joystick ved højre hånd,
  der vipper fra side til side, og højre controller dunker. Det forsvinder, første gang joysticken bruges.
* **Slutkortet:** "Du har gennemført spillet" vises foran en, når man er hjemme i sin egen tid.
* **Sikkerhedsvisningen af stuen nær møbler er altid slået til** og kan ikke vælges fra.

### Efter anden prøve i brillen
* **Kniven slap snittet:** et snit blev afbrudt, hver gang en hånd kom tæt på noget, der kunne tages (brevet i bæltet ved hoften). Nu afgøres det kun i det øjeblik,
  udløseren trykkes ned, og kun for knivhånden; et snit, der er i gang, afbrydes ikke.
* **Døren lukker sig selv:** i historien lukker den dør, man er gået igennem, efter ca. 10 sekunder (begge veje). Et lysende mærke på ringen i håndhøjde viser, hvor man selv kan skære den over.
  I fri leg lukker døre ikke af sig selv.
* **Kan man komme derhen?** Scanningen ses ovenfra i felter på 16 cm; et felt er spærret, hvis noget står i det mellem skinneben og hovedhøjde. En udfyldning fra dér, hvor man står
  (kun gennem felter uden spærrede naboer, så en passage skal være ca. en halv meter bred), giver alt, man kan gå til. Ting lægges kun inden for en armslængde (45 cm) af sådan et sted.
* **Brevet på et bord:** der søges først efter en plan flade i bordhøjde, som man kan komme hen til; ellers svæver brevet i håndhøjde.
* **Fremkomsten:** varer 6 sekunder med lyd fra stedet, og et lys som en lille sol svulmer op og falmer dér, hvor delen bliver til, og oplyser rummet derfra (punktlys uden skygger).
  Den lodrette lysstråle er fjernet; leder man længe, blinker delen, kalder og pulserer som et langsomt fyr.
* **Tavlen på controlleren** vises kun, når der er noget nyt at sige, og toner ud efter ca. 5 sekunder.
* **Joystick-vinket** vises nu, så snart man er hjemme og joysticken er låst op, indtil man har brugt den.

## Fraktale dimensioner (fri leg)
Tre uendelige rum, valgt blandt formlerne i Fractal Video (`vfx-apps/fractal-video/js/formulas.js`, afstandsfunktionerne er hentet derfra): **Apollonisk**
(kugle-inversioner, bobler i bobler), **Pseudo-Kleinsk** (endeløse haller og gitre) og **Hyperbolsk** ({4,3,5}-bikage af terninger i Poincaré-kuglen, med en langsom hyperbolsk drift).
De kan kun vælges med joysticken i fri leg, dvs. efter historien er gennemført eller via knappen *Fri leg*.

* **Tegning:** fraktalen stråle-marcheres ind i et lille billede pr. øje (med øjets egen position og projektion fra headsettet i samme frame), og det billede lægges bag alt andet
  (venstre/højre øje via lag 1/2). Prisen afhænger derfor ikke af headsettets opløsning. Gennem døren bruges samme billede, kun i dørens udsnit.
* **Billedhastighedsvagt:** er billederne langsomme i et sekund, skrumper fraktalbilledet (ned til 20 % af øjets opløsning); er de hurtige i fire sekunder, vokser det igen (op til 55 %).
* **Billigere stråler:** grovere mod kanten af synsfeltet, tolerance der vokser med afstanden, lys fra feltets hældning med ét ekstra opslag i stedet for en fuld normal.
* **Rummet:** den scannede form vises som et svagt omrids oven på fraktalen, og tæt på hoved og hænder åbner det sig mod den rigtige stue. Advarselslyden virker som i de andre verdener.
* Ingen musik og ingen dele i de fraktale dimensioner.

### Styring i de fraktale dimensioner
Kun når man står i en fraktal dimension:
* **Højre stick:** frem/tilbage flyver i den retning, man kigger (også op og ned: kig ned for at dykke); til siderne glider man sidelæns.
* **Venstre stick** frem/tilbage: farten, trin 1 til 9 (1 er en krybefart på 0,15 m/s, 9 ca. 8 m/s; aldrig nul). Trinnet vises foran en, mens man ændrer det.
* **Y / X** (venstre) og **B / A** (højre): hver sit par ændrer én parameter. Apollonisk: inversionens størrelse og antal gentagelser; Pseudo-Kleinsk: inversionens størrelse og kassens bredde;
  Hyperbolsk: antal terninger om hver kant (4,4 til 7) og kanternes tykkelse; Menger-labyrinten: gangenes bredde og antal niveauer; Koralsletten: revets form og højde; Kali-templet: skala og mindste radius.
* Seks fraktale dimensioner (Koralsletten er siden afløst af Mandelbox-byen, se nederst): Apollonisk, Pseudo-Kleinsk, Hyperbolsk, **Menger-labyrinten** (en Menger-svamp gentaget i det uendelige: firkantede gange inde i gange, i alle retninger), **Koralsletten** (Kali coral plain: et landskab af hylder og rev)
  og **Kali-templet** (KaliBox: et udskåret tempel tæt på, som man kan flyve hen til og ind i).
* **Begge greb holdt:** træk hænderne fra hinanden for at gøre fraktalen større, sammen for mindre (op til 12 gange hver vej; bevægelsen er forstærket).
* **Tryk på højre stick:** skift verden bag døren. Man drejer ved at dreje sig selv.
* Alt sker om det punkt, hvor hovedet er: fraktalen glider, drejer og vokser, mens man selv bliver stående i stuen. I den hyperbolske verden kan man ikke flyve ud gennem horisonten.
* **Møblernes omrids** toner ud, når hovedet er blevet inden for 80 cm af samme sted i to sekunder (oprindeligt 30 cm; det var for lidt: at kigge ned eller dreje sig på stedet flytter hovedet 20 til 40 cm), og kommer tilbage, når man går et skridt eller to. Stedet følger langsomt med hovedet, så små flytninger ikke lægger sig sammen.
  Åbningen mod den rigtige stue tæt på møbler og advarselslyden toner ikke ud.
* Første gang man kommer ind i en fraktal dimension, dunker og blinker begge controllere, indtil man kigger ned på dem (eller efter 40 sekunder). Der står intet; man opdager styringen ved at prøve.
* I preview: I/K og J/L flyv, U/O fart ned/op, N/M og V/B form, komma/punktum skala.

* **Skarpere i ro:** står man stille uden at røre styringen i halvandet sekund, må fraktalbilledet vokse (loftet hæves fra 55 % til 100 % af øjets opløsning, et trin ad gangen så længe
  billedhastigheden holder), og strålerne marcheres finere. Så snart man går eller styrer, falder det tilbage til det, brillen kunne bære før. Går billedhastigheden ned i ro, huskes det niveau som loft.

* **Bygget op over tid (forsøg, standard):** i ro tegnes det lille fraktalbillede hver gang med en ny forskydning på under en pixel (Halton-følge, 16 trin), og et
  større billede (2,2 gange bredden) samles af dem: hver pixel tager den nærmeste nye prøve, vægtet efter hvor tæt den faktisk ramte, og blander med sidste billedes værdi
  dér, hvor det samme punkt af fraktalen lå (fundet ud fra strålens længde, som gemmes i alfa, og begge billeders øjestillinger). Den gamle værdi klemmes ind i det, de nye prøver omkring tillader,
  så små hovedbevægelser ikke tværer ud. Prisen pr. billede er den samme lave opløsning plus et billigt samlepas. Fraktalens egen bevægelse står stille imens.
  Startsiden har valget *Fraktaler i ro*: bygges op over tid / større billede, hvis brillen kan / ingen skærpning (`?sharp=taa|res|off`).

## Spiralen, når døren skifter verden
Når man skifter verden bag døren med joysticket, bliver billedet i døren snoet ind i en spiral og trukket ned i et punkt (0,6 s); tilbage er sort rum med
stjerner, og så vikler den nye verden sig hurtigt ud af punktet igen (0,34 s). Skiftet af verden sker, mens der kun er stjerner.

* Vinduet i døren er ægte geometri og kan ikke bøjes. I det sekund viser døren derfor et **stillbillede**: den anden side, som den så ud derfra, hvor man
  står, taget i samme øjeblik (`worlds.snapshot`, 640 x 960, tegnet med de samme shadere som brillens eget billede, så intet skal oversættes), lagt på dørens
  flade og snoet af `vortexMat` i `portal.js`. Billedet er altså fladt, mens det snor sig; stjernerne bag er derimod tegnet pr. øje og ligger uendeligt langt væk.
* Skubber man til joysticket igen, mens spiralen er på vej ind, ændres kun målet. Går man gennem døren, eller lukker den, afbrydes spiralen.
* Fører døren til stuen (set fra en anden verden), er "billedet" et hul ind til den rigtige stue, som skrumper i samme spiralform.

## Verdensrummet (fri leg)
En dimension uden noget at stå på: stjerner i alle retninger. Åbner ligesom de fraktale dimensioner først, når historien er gennemført (eller i fri leg).

* **Himlen** er en shader (`SPACE_GLSL` i `worlds.js`): Mælkevejen som et bånd med en lys kerne, støvbaner og stjerneskyer, tre svagt lysende tåger, to fjerne
  galakser, og tre lag stjerner (flest langs båndet). Stjernerne laves uden tekstur: himmelkuglen skæres gennem et gitter af celler med et lyspunkt i hver.
* **De nære stjerner** er 1100 rigtige punkter i rummet, fra lige uden for rækkevidde til 70 m væk. Dem ser man i dybden (hvert øje sit billede), og de
  forskyder sig mod de fjerne, når man flytter hovedet.
* **Styring:** man kommer ingen vegne, men kan tumle. Højre stick drejer (om lodret) og vipper frem og tilbage, venstre stick ruller. Der er træghed: man
  bliver ved med at dreje lidt, efter man har sluppet (dør ud over cirka 5 s). Det er himlen og de nære stjerner, der drejer om hovedet; stuen står stille.
  Tryk på højre stick skifter verden bag døren.
* **Rummet og sikkerhed** som i de fraktale dimensioner: stuens scannede form som et svagt omrids, der toner ud, når man står stille, og tæt på hoved og hænder
  åbner det sig mod den rigtige stue. Advarselslyden virker. En dyb, svag drone som baggrund; ingen musik.

## Ting, der giver efter (de tre sang-dimensioner)
Små ting i hånd- og hoftehøjde reagerer på hænderne (controllernes position). Ingen fysikmotor: det er fjedre og en enkelt shader, så det koster næsten intet.

* **Den grønne:** stængler bøjer væk fra hånden og svirper tilbage (også de gamle syv); tuer af **højt græs** (35 til 85 cm) lægger sig væk fra hånden
  (i shaderen, også det korte græs); sporerne i luften viger.
* **Metalverdenen:** **ventilhjulet** kan drejes med hånden langs kanten og løber videre af sig selv, med klik; fire **kabler med krog** hænger ned fra disen
  og svinger langsomt og længe, når man skubber til dem.
* **Klippeverdenen:** tuer af **tørre strå**, hvor hvert strå giver efter for sig.
* Hver berøring giver en lille lyd (svirp, raslen, metalklang, klik) og et kort dunk i den controller, der rørte.
* Tingene sættes, første gang døren fører til verdenen, på frit gulv, man kan gå hen til (ud fra scanningen), nær døren men ikke i selve døråbningen.
  Er der ingen ledig plads, vises tingen ikke.
* Bevidst fravalgt: at samle ting op, kaste dem eller sparke til sten. Uden fysikmotor og uden at vide, hvor fødderne er, bliver det ikke overbevisende.

## Det firedimensionale rum
Egen knap på startsiden (døren fører dertil fra start; `?flat&four` i forhåndsvisning), og med i rækken af dimensioner i fri leg.

En møbleret stue med fire rumlige retninger: x og z langs gulvet, y op, og w. Man ser den tredimensionelle skive af rummet, som vores verden skærer ud.
Alt er faste legemer i 4D, beskrevet ved afstandsfunktioner, og strålen marcheres gennem 4D langs skiven (`FOUR_FS` i `worlds.js`, samme rørledning som
fraktalerne: lille billede pr. øje, opbygning over tid i ro, billedhastighedsvagt).

* **Hvad der står i rummet** (alt ser almindeligt ud i udgangsskiven, w = 0):
  bord med **otte ben** (fire ved w = 0, fire ved w = -0,74; imellem svæver pladen), en lysende **kugle** på bordet (4D-kugle: skrumper og forsvinder),
  en **taburet**, hvis ryglæn står ude i w (trækkes rummet 19 cm, er det en massiv klods; drejes rummet, bliver det et almindeligt ryglæn),
  en **søjle**, der er rund i tre retninger (bliver tyndere), med en **messingring**, som bliver til to kugler, når rummet drejes en kvart omgang,
  en **bogreol**, der er 3,2 m dyb i w (andre bøger for hver 33 cm), en **kasse** (terning i x, z, w stillet på et hjørne: sekskant, trekant, væk),
  et **rundt tæppe** (rundt i tre retninger), et **maleri**, der er et andet landskab i hver dybde, og en **loftslampe** (punktlys i 4D: længere væk i w = svagere lys).
* **Overflader:** egeplanker, puds og malede paneler, valnød, marmor, messing, bogrygge med guldtryk, uld. Lys fra lampen med bløde skygger, lidt lys fra kuglen,
  og genskin i blanke flader. Alt er procedurelt i 4D-koordinater, så mønstrene også ændrer sig rigtigt, når man trækker og drejer.
* **Styring (ændret efter ejerens ønske, så sticksene er fri til at gå rundt):** **Y/X** trækker rummet igennem langs w (0,4 m/s), **B/A** drejer i planet
  udspændt af ens højre side og w, om det punkt hvor hovedet er, så det, man ser lige på, bliver stående, mens det forandrer sig. Begge tager fart og løber ud
  (som fraktalernes parametre) og har hver sin lyd. **Venstre stick** går gennem rummet (langs blikket og sidelæns, 1,3 m/s; snittet glider langs sine egne tre
  retninger, så man aldrig forlader det, og ikke ud gennem rummets vægge), **højre stick** drejer en 30 grader ad gangen. **Tryk venstre stick** sætter den
  fjerde retning tilbage. Et lille skilt viser dybde og drejning. Går man ud af rummet, falder det tilbage. På computer: M/N trækker, B/V drejer, K nulstiller.
* Rummets vægge er en skal: kommer man uden for (stuen er større end rummet, eller efter en drejning), ser man rummet udefra i mørke.
* Sikkerhed som i fraktalerne: stuens omrids, åbning mod den rigtige stue tæt på hoved og hænder, advarselslyd.

## Ændringer i fraktalerne
* **Mandelbox-byen** har afløst Koralsletten: den klassiske Mandelbox (skala -1,8), én i hver celle af et uendeligt gitter, med gader imellem. B/A ændrer skalaen,
  Y/X den mindste radius.
* **Kali-templet** står nu lige foran døren (ca. 2,5 m væk, med pavillonen i øjenhøjde) og svæver i rummet: hvor strålen ikke rammer noget, tegnes stjernerne
  i fuld skarphed oven på fraktalens lille billede.
* **Rettet fejl:** fraktalerne blev lagt ud med forkert fortegn på dørens drejning, så "lige frem" i fraktalen kun passede, når døren vendte langs stuens z-akse.
  Det mærkedes ikke i de uendelige fraktaler, men var grunden til, at Kali-templet var svært at finde.

## Hænder (`hands.js`)
I alle dimensioner undtagen ens egen stue (hvor man ser sine rigtige hænder) sidder der en handskeklædt hånd om hver controller, lukket på samme måde som den
rigtige hånd er lukket om controlleren. Pegefingeren følger aftrækkeren, de tre andre fingre strammer med grebsknappen, tommelfingeren går ned, når den hviler
på stick eller knap, og hånden med kniven er helt lukket om skæftet. Hånden er WebXR's standard-håndmodel (`models/hand-left.glb`, `hand-right.glb` fra `@webxr-input-profiles/assets`, MIT; indlæses med three's `GLTFLoader` i `vendor/`). Modellen er en flad, åben hånd, hvis 25 led ikke hænger sammen i et skelet, så `hands.js` regner selv lukningen ud: hver finger er en kæde, hvert led bøjer om sin egen akse, og tommelfingeren svinges først om over for de andre fingre og bøjes så. En enkel hånd af kapsler står i stedet, indtil modellen er hentet. Modellen ligger i WebXR's "grip space" (stangen i næven langs Z, -Z ud ved
tommelfingeren, X gennem håndfladen, +X mod højre for begge hænder); venstre hånd er spejlbilledet. Ikke afprøvet i headset: sidder den forkert, er det
aksernes fortegn i toppen af `hands.js`, der skal vendes.

## Bandets mærker
I hver af de tre sang-dimensioner står en opret sten eller plade lidt væk fra døren (2 til 4 m), med seglet og navnet (`img/segl.png`, `img/tidejord.png`, lavet
som masker ud fra bandets logoer) sat på, som den verden selv ville have lavet dem: lysende lav på en mosgroet sten i den grønne, slidt skabelonmaling på en
stålplade i metalverdenen, kridt på en sandsten i klippeverdenen. Slid og tåge kommer fra verdenens egne værdier.

## Farveskemaer, flyvning og rumkrumning
* **Fraktalerne:** venstre stick helt til venstre eller højre skifter farveskema (seks skemaer: alle farver drejes 60 grader rundt i farvecirklen). Op/ned er stadig farten.
* **Verdensrummet** flyves nu som fraktalerne (højre stick: frem/tilbage langs blikket og sidelæns; venstre stick: fart 1 til 9, tre gange fraktalernes hastigheder).
  De nære stjerner er en terning på 60 m, der gentager sig uendeligt omkring én; den fjerne himmel står stille. Den tidligere drejning med træghed er fjernet.
* **B/A (warp):** aberration som ved høj fart, op til 93 % af lysets hastighed. Hele himlen og de nære stjerner fejes frem mod den retning, man kiggede i, da man
  startede warpen, blåligt forude og rødligt bagude.
* **Y/X (sort hul):** et sort hul et fast sted på himlen, der virker som en punktlinse: lyset bøjes omkring det, med en skygge og en lysende ring. De nære stjerner
  bøjes ikke (de er foran hullet).

## Døråbningen (de sidste centimeter)
Tæt på dørens plan gik to ting galt, som i et kort glimt viste den verden, man var på vej ud af (fra stuen: kamerabilledet): den del af vinduet, der var
nærmere end 5 cm, blev skåret væk, og det ene øje var igennem døren før det andet. Nu er vinduet **hele synsfeltet**, når hovedet er inden for 11 cm af
dørens plan på vej ind (`maskFull`), og de første 13 cm på den anden side er der **intet vindue** (ellers ville det ene øje se tilbage). Kniv og hænder
tegnes efter døren, så de ikke bliver malet over i døråbningen.

## Rumskibet (valgfrit, `ship.js`)
Et flueben på startsiden ("Rumskib i de fraktale dimensioner", `?ship` i forhåndsvisning). Er det slået til, flyver man ikke selv i de fraktale dimensioner
og i verdensrummet; man styrer et rumskib, der hænger i stuen foran én (set udefra).

* **Modellen** er bygget i kode (`ship.js`) efter ejerens tegninger (Spaceship A til D, ni vinkler af samme skib): bredt skrog med skovlformet stævn, hvis
  underlæbe stikker frem; højt tårn midtskibs, der vokser ud af skroget og ender i en kuppel og en mast; lang antenne nedad; lodret halefinne med en
  motorgondol på toppen; en gondol på en stubvinge på hver side af halen; en stor og en lille gondol på hver flanke; to skråtstillede finner under halen;
  maskinfyldte nicher langs flankerne; og bandets segl på flankegondolerne og halefinnen. Lyseblå plader i tre nuancer, orange gitre og tegnede konturer
  (samme form en anelse større, sort, set indefra). Alt malet er ét net med farver pr. hjørne, plus ét net til konturerne. Ejeren har også sendt to startpakker (samme Blender-script igen, og en grov three.js-skitse); de er vurderet og ikke brugt, fordi denne model ligger tættere på tegningerne. En første udgave fulgte
  `models/spaceship_model.py` (et Blender-script, gemt som reference); det ramte ikke tegningerne og er erstattet. Skibet vises som en model på ca. 86 cm.
* **Plasma:** ud af hver motor (halen, gondolen på finnen, de to ved halen og de to små på flankerne) står en stråle, der bliver længere og lysere med
  gashåndtaget: hvidblå ved motoren, over violet til orange i spidsen, med det stående stødmønster fra en hurtig stråle.
* **Styring med hånden:** hold højre greb inde, og skibet drejer, som hånden drejer, som om man holdt det (slip, drej håndleddet tilbage, og tag fat igen
  for at dreje videre). Det er den mest naturlige måde at styre en model på i VR og kræver ikke, at man kigger et bestemt sted hen. Sticken virker stadig.
* **Styring:** højre stick styrer skibet (venstre/højre drejer, frem tipper næsen ned, tilbage løfter den; det krænger i svingene). Venstre stick op/ned er
  gashåndtaget: 0 er stop, 1 til 9 som farten ellers. Skibet flyver, hvor næsen peger. Y/X, B/A, begge greb (skala, om skibet) og farveskift virker som uden skib.
* **Kun skibet drejer.** Dimensionen glider forbi, men roterer aldrig, så det er skånsomt for maven, og man kan gå rundt om skibet og se det fra alle sider.
  Går man mere end 2,6 m væk, kommer det efter én og lægger sig foran igen.
* Ikke i det firedimensionale rum og ikke i de tre sang-dimensioner.

## Flyv efter blikket eller efter hånden
Valg på startsiden ("Flyv efter", `?steer=hand`): når man selv flyver (uden rumskib) i de fraktale dimensioner og i verdensrummet, er "frem" enten dér,
hvor man kigger (standard, som hidtil), eller dér, hvor højre controller peger. Med hånden kan man kigge sig frit omkring, mens man flyver en anden vej.
Sidelæns er altid vandret og vinkelret på den retning. Warp i verdensrummet tager også sin retning derfra.

## Skibet kan støde ind i fraktalen
Fraktalerne er faste for skibet. De samme afstandsfunktioner, som shaderne tegner med, findes også i JavaScript (`DE` i `worlds.js`, `worlds.fractDist`),
så spillet kan måle, hvor langt der er fra et punkt i stuen til fraktalens overflade. Skibet måles i syv punkter (næse, skrog, hale, halefinne, mast, antenne),
og hvert billede flyver det frem i korte skridt (4 cm); det første skridt, der ville sætte et punkt ind i en væg, bliver ikke taget: gashåndtaget går på 0,
der lyder et bump, det glimter ved næsen, og controllerne dunker. Sidder skibet allerede i en væg (efter at man har ændret fraktalens form eller størrelse),
må det altid bevæge sig ud. Ændres en fraktals formel i shaderen, skal den også ændres i `DE`. Ikke i verdensrummet (der er intet at ramme).

## Lyd i de fraktale dimensioner og fra skibet (`ambience.js`)
Intet er optaget; alt bygges i Web Audio.

* **Seks lydbilleder**, ét pr. fraktal, hver med en bund (noget, der bare er der) og hændelser (enkelte lyde, der sker et sted omkring én):
  Apollonisk: anslåede syngeskåle i en lang, lys hal. Pseudo-Kleinsk: dyb orgeldrone, der ånder, og langsomme pust. Hyperbolsk: højt glasagtigt skær og
  fin tikken tæt på. Menger: vind i tunnelerne og metal, der falder langt væk, med ekkoer. Mandelbox-byen: dyb rumlen af sten, tørre vindstød og blokke, der
  skurrer i andre gader, med ét tilbageslag fra væggen overfor. Kali-templet: næsten stilhed, og en gong, sjældent, der klinger længe.
* **Rumlig lyd og afstand:** hændelserne placeres i rummet (HRTF) og sendes ind i en rumklang, der passer til stedet. Den direkte lyd falder med afstanden,
  rumklangen gør ikke, så en fjern lyd er mest klang og en nær lyd mest direkte. Det er sådan, øret bedømmer afstand.
* **Skibet:** en svag brummen, når det er fremme, og udstødningen: en brusen, der åbner sig og bliver kraftigere med gashåndtaget, over en dyb rumlen.
  Begge kommer fra dér, hvor skibet er. Udstødningen høres desuden én gang til, kastet tilbage fra fraktalens nærmeste væg: senere og svagere, jo længere
  væk væggen er (tiden er regnet, som om skibet var 19 m langt).
* **Verdensrummet:** warp (B/A) høres: en susen, der stiger i tonehøjde, og en tone, der stiger fire oktaver mod lysets hastighed og slår hurtigere og
  hurtigere; rummets egen drone stiger med. Det sorte hul (Y/X) er en dyb bas med to toner et par hertz fra hinanden, som kommer fra dér, hvor hullet er på
  himlen, tungere jo større det er. Lyd regnes på lydtråden og koster ikke billedhastighed.

## Bløde parameterskift, og skibet, der holder sig foran én
* **Parametre (Y/X og B/A i fraktalerne)** skifter ikke længere som en kontakt: ændringen tager fart, når man trykker, løber ud, når man slipper, og går af
  sig selv langsommere den sidste sjettedel af parameterens område (`easeParams` i `portal.js`). Gælder med og uden rumskib.
* **Lyd til hver af de fire:** den første parameter (Y/X) er en tone, som en skive, der drejes: tonehøjden er, hvor parameteren står. Opad er den lys med
  en kvint over og hurtig sitren; nedad mørk med oktaven under og langsom vaklen. Den anden (B/A) er en mekanisme af smalbåndet støj: opad en hård, hurtig
  skralde; nedad en spinden. Styrken følger farten, og hver giver et blødt bank, når den når enden.
* **Skibet holder sig foran én i flyveretningen**, så man ser det bagfra: drejer man det, svinger det om til sin nye plads, og man drejer med. Omvendt:
  vender man sig selv og lader styringen være (3 s), og skibet er bag én i 2 s, kommer det om foran og peger, hvor man ser hen. Skibets plads i stuen er
  kun dér, man ser det fra; det bliver, hvor det er i dimensionen (dimensionen glider med), så det kan ikke bæres gennem en væg på den måde.
* **Man kan skubbe til skibet:** en hånd mod skroget flytter det, drejer det lidt, hvis man rammer nær næse eller hale, og så glider det tilbage på plads.
  Et lille dunk i controlleren og et bump.

## Kamp (test): rumvæsner i fraktalerne (`combat.js`)
Et flueben på startsiden ("Kamp (test)", `?combat`); det slår rumskibet til. Kun i de seks fraktale dimensioner.

* **Insekterne** lever i fraktalens egne koordinater, ikke i stuen: deres plads, størrelse og fart er i fraktalens enheder. Derfor skaleres de med fraktalen
  (gør man den lille, bliver de små med den, og ingen knuses af det), og en formændring (Y/X, B/A) flytter væggene gennem fraktalens rum: en væg, der når
  et insekt, knuser det (tælles på skiltet over skibet). Ca. 14 ad gangen, sat ud i det frie rum 3 til 8 m fra skibet (ved fraktalens startskala).
* **De kravler langsomt mod skibet** (ca. 0,2 m/s ved startskala) og glider langs væggene i stedet for at gå ind i dem, men de flygter ikke fra en væg, der
  kommer imod dem. Når et når skibet, eksploderer det, og skroget tager ca. 10 % skade.
* **Skibet tager skade, når det støder ind i fraktalen:** 2 % ved krybefart, op til 60 % ved fuld fart. Ved 0 % sprænges skibet, og efter 3,5 s kommer et nyt
  med fuldt skrog; insekterne tæt på det fjernes.
* **Synlighed:** fraktalen er et billede, insekterne almindelige 3D-figurer tegnet ovenpå. Hvert insekt får derfor sin sigtelinje tjekket (nogle få pr. billede,
  ved at marchere fra hovedet gennem fraktalen på processoren); er det bag en væg eller væk i tågen, tegnes det ikke.
* **Ydelse:** tre tegnekald uanset antal; afstandsfunktionerne i JavaScript er skrevet om uden arrays (samme resultat, 19 gange hurtigere), da de nu kører
  et par hundrede gange pr. billede.
* Lyd: et tungt brag, når et insekt rammer skibet, et vådt knas, når en væg knuser et, og et stort brag, når skibet går tabt.
* Kendt: i nogle fraktaler og retninger når væggene aldrig insekterne (f.eks. bliver gaderne i Mandelbox-byen bredere, når skalaen øges); man må finde den
  formændring, der lukker rummet.

## Fraktalerne uden kamp ser ud som før
Billedet af fraktalerne er det samme som i #67 (sammenlignet pixel for pixel i forhåndsvisningen); farveskemaerne er neutrale, indtil man skifter dem, og
nulstilles, når man går ind i en fraktal. Men fraktalbilledets opløsning følger billedhastigheden, så alt, der koster tid hvert billede, gør fraktalerne
uskarpe. Derfor: hver lyd i lydbillederne lever kun, så længe den lyder (før 14 s med sin egen 3D-panorering, så op mod 20 kørte samtidig), højst 6 ad
gangen; rumklangene er kortere (en klangs længde er dens pris); kampens figurer og eksplosionernes gnister tegnes og opdateres kun, når de er i brug.
Farveskiftet på venstre stick kræver et tydeligt skub til siden, så et skråt skub for at ændre farten ikke skifter farver.

## Tesserakten og kleinflasken i det firedimensionale rum
* **Tesserakten** (firedimensional terning, halv kantlængde 24 cm) svæver over en marmorsøjle ved rummets bageste højre hjørne, som en trådmodel af
  egetræsstænger med messingkugler i hjørnerne. Ikke dens kanter (en kant møder vores tre dimensioner i højst ét punkt), men dens kvadratiske sideflader,
  hver gjort en centimeter tyk: en sideflade møder snittet i en linje, så snittet viser polyedrets kanter som stænger. Hvor snittet krydser en af dens kanter
  (polyedrets hjørner), sidder en messingkugle. Den står på et hjørne i w: en spejling fører w-aksen over i dens lange diagonal, så vores tre dimensioner
  skærer den på tværs af diagonalen. I udgangsskiven er snittet et oktaeder; trækker man rummet, bliver det et afskåret tetraeder, så et tetraeder, så et
  punkt (ved ±48 cm); drejer man rummet, kommer andre polyedre. Den drejer langsomt om lodret, hvilket kun drejer snittet. Den tegnes af rummets egen march
  (`tessQ`, `sort4`, materiale 13 i `FOUR_FS`).
* **Kleinflasken** står af glas på sin egen søjle. Glas kan ikke marcheres (én flade pr. stråle), så den er et rigtigt net (`fourGlass` i `worlds.js`), tegnet
  oven på rummets billede. Hvert punkt er et 4D-punkt; shaderen ophæver hvert billede det snit, rummet ses igennem, så punktet lander, hvor det er i snittet,
  og ved hvor langt det er fra snittet i w. Kun det, der er inden for 12 cm, ses, og det toner ud mod kanten. Halsen løftes 30 cm ud i w netop dér, hvor den i
  tre dimensioner ville gå gennem flaskens egen væg (regnet ud fra formen: u = 2,45 til 2,50): i fire dimensioner behøver en kleinflaske ikke skære sig selv.
  Trækker man rummet, kommer det manglende stykke hals til syne, mens resten toner ud. Flasken gemmer sig bag møblerne ved at sammenligne sin afstand med
  rummets (marchen gemmer, hvor langt hver stråle nåede, i billedets alfa). Formen er den klassiske parametriske "flaskeform" (x, y, z som på Wikipedias side om kleinflasken, u 0..π, v 0..2π)
  med kroppen gjort buttet og halsen lidt tykkere omkring rørets midterlinje; opblæsningen er nøjagtigt 1 i begge ender, hvor røret møder sig selv.
* Søjlerne er runde i x og z og dybe i w, så de bliver stående, mens man trækker.

## Skibet skyder (kamp)
* **Aftrækkeren på hånden uden kniven** (normalt venstre) affyrer skibets to kanoner, skiftevis, ca. syv skud i sekundet, så længe den holdes inde. I den flade
  forhåndsvisning: mellemrumstasten. Kun når kamp er slået til, og kun i de seks fraktale dimensioner. Kniven bruger stadig den anden hånds aftrækker.
* **Skuddene** er korte orange plasmastråler med et lysskær. De lever i fraktalens eget rum ligesom insekterne og flyver med skibets egen fart plus 9 m/s i
  op til 1,4 s. Den første væg stopper et skud med en lille gnist; det første insekt inden for rækkevidde springer og tælles på skiltet (samme tæller som de
  knuste).
* **Sigtehjælp:** insekterne er kun 6–8 cm, så et skud, der affyres inden for ca. 7 grader af et insekt, man kan se (højst 7 m væk), sigtes direkte mod det.
* Lyd: et kort, faldende "zap" fra kanonen (højst fire ad gangen), en kort summen i controlleren. To ekstra tegnekald, kun mens der er skud i luften.
* **Strålen: greb + aftrækker** på samme hånd (knyt hånden). Et kort, kraftigt stød (0,3 s) lige ud fra næsen til første væg (højst 12 m); alle insekter
  inden for et par centimeter af linjen springer. Samme sigtehjælp som skuddene. Derefter 3,5 s til at lade op. Blåhvid, med sin egen lyd. Mens grebet
  holdes, skyder aftrækkeren ikke plasma. Forhåndsvisning: C.
* **Raketten: tryk på venstre stick.** Én ad gangen. Den flyver ca. 3 m/s og styrer efter din hånd: den følger linjen, som controlleren peger (en svag
  orange sigtestreg fra hånden viser den), og drejer højst ca. 160 grader i sekundet. Den springer ved første væg, første insekt den rører, efter 7 s, eller
  når du trykker igen; alt inden for 40 cm springer med. Er skibet inden for 45 cm, tager det 25 % skade. Derefter 5 s til en ny. Forhåndsvisning: X, og
  den følger dit blik.
* Skiltet over skibet viser **STRÅLE** og **RAKET** tændt, når de er klar, og dæmpet, mens de lader op.

## Gemte idéer (ikke bygget)

### Drone option
Gemt 16. november 2026 som alternativ, hvis det viser sig for usikkert at bevæge sig rundt i de fraktale dimensioner. Efter en prøve i brillen samme dag
vurderede Peter, at der ikke umiddelbart er brug for den.

> Hvad med et mode hvor man kun kan kigge ind - passage til den fraktale verden må vi se om kan lade sig gøre. Jeg tænker at man får at vide at man
> ikke kan overleve i en fraktal dimension, men at man kan sende en drone ind. Når man aktiverer dronen, holder man den i venstre hånd, og skubber
> ind gennem portalen. Derved skifter perspektivet til dronen, og man styrer den nu med joystick som et lille rumskib. Man får at vide at man kan
> aborte dronemissionen ved at trykke venstre kontroller knap nede, "X" tror jeg den hedder.
