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
