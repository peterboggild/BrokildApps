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
