# Portalkniven (teknisk test)

Teknisk test til Meta Quest 3 (WebXR, `immersive-ar`): en grøn knitrende ring midt i rummet er en åben dør til en anden
version af rummet. Se ind, gå igennem, gå tilbage.

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
* **Venstre stick** frem/tilbage: flyv (0,9 m/s i den retning, man kigger, vandret). **Højre stick** til siden: drej. **B / A** (højre): op / ned.
* **Y / X** (venstre): ændr fraktalens form, én parameter pr. fraktal (Apollonisk: inversionens størrelse; Pseudo-Kleinsk: inversionens størrelse; Hyperbolsk: antal terninger om hver kant, 4,4 til 7).
* **Begge greb holdt:** træk hænderne fra hinanden for at gøre fraktalen større, sammen for mindre (højst 1,8 gange hver vej).
* **Tryk på højre stick:** skift verden bag døren (et vip drejer nu i stedet).
* Alt sker om det punkt, hvor hovedet er: fraktalen glider, drejer og vokser, mens man selv bliver stående i stuen. I den hyperbolske verden kan man ikke flyve ud gennem horisonten.
* **Møblernes omrids** toner ud, når man har stået stille i to sekunder, og kommer straks tilbage, når man går (hovedet flytter sig mere end ca. 0,2 m/s).
  Åbningen mod den rigtige stue tæt på møbler og advarselslyden toner ikke ud.
* Første gang man kommer ind i en fraktal dimension, dunker og blinker begge controllere, indtil man kigger ned på dem (eller efter 40 sekunder). Der står intet; man opdager styringen ved at prøve.
* I preview: I/K flyv, J/L drej, U/O ned/op, N/M form, komma/punktum skala.

* **Skarpere i ro:** står man stille uden at røre styringen i halvandet sekund, må fraktalbilledet vokse (loftet hæves fra 55 % til 100 % af øjets opløsning, et trin ad gangen så længe
  billedhastigheden holder), og strålerne marcheres finere. Så snart man går eller styrer, falder det tilbage til det, brillen kunne bære før. Går billedhastigheden ned i ro, huskes det niveau som loft.

* **Bygget op over tid (forsøg, standard):** i ro tegnes det lille fraktalbillede hver gang med en ny forskydning på under en pixel (Halton-følge, 16 trin), og et
  større billede (2,2 gange bredden) samles af dem: hver pixel tager den nærmeste nye prøve, vægtet efter hvor tæt den faktisk ramte, og blander med sidste billedes værdi
  dér, hvor det samme punkt af fraktalen lå (fundet ud fra strålens længde, som gemmes i alfa, og begge billeders øjestillinger). Den gamle værdi klemmes ind i det, de nye prøver omkring tillader,
  så små hovedbevægelser ikke tværer ud. Prisen pr. billede er den samme lave opløsning plus et billigt samlepas. Fraktalens egen bevægelse står stille imens.
  Startsiden har valget *Fraktaler i ro*: bygges op over tid / større billede, hvis brillen kan / ingen skærpning (`?sharp=taa|res|off`).

## Gemte idéer (ikke bygget)

### Drone option
Gemt 16. november 2026 som alternativ, hvis det viser sig for usikkert at bevæge sig rundt i de fraktale dimensioner. Efter en prøve i brillen samme dag
vurderede Peter, at der ikke umiddelbart er brug for den.

> Hvad med et mode hvor man kun kan kigge ind - passage til den fraktale verden må vi se om kan lade sig gøre. Jeg tænker at man får at vide at man
> ikke kan overleve i en fraktal dimension, men at man kan sende en drone ind. Når man aktiverer dronen, holder man den i venstre hånd, og skubber
> ind gennem portalen. Derved skifter perspektivet til dronen, og man styrer den nu med joystick som et lille rumskib. Man får at vide at man kan
> aborte dronemissionen ved at trykke venstre kontroller knap nede, "X" tror jeg den hedder.
