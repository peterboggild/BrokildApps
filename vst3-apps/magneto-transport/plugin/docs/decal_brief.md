# Magneto Transport decals

Image parts for a software tape-echo instrument called **Magneto Transport**. Each part is made on its own and the program assembles them, so **never paint a whole panel**: a program places every part at exact coordinates, and an image generator cannot hit exact coordinates. What matters is that each part looks right on its own canvas, at the stated size, with real transparency where asked.

There are 37 parts. Make them one at a time: one image per request.

## How to work

1. Every request starts with the style paragraph below, then the part's own text. The page version of this brief has a copy button that puts both together.
2. Use exactly the size given. Only three sizes are used: 1024 x 1024 (square), 1536 x 1024 (landscape) and 1024 x 1536 (portrait).
3. **Transparent means real alpha.** If an image comes back with a white, grey or checkerboard background where it should be transparent, ask for it again with a transparent background.
4. **Fill the canvas as described.** Earlier deliveries came back with the drawing in a thin strip of a larger canvas. That is a fail; make it again.
5. **Pairs must match.** For each pair (off/on, up/pressed) make the second from the first, at the same size and position, so that switching between them moves nothing but the light or the press.
6. Save each image as PNG with exactly the file name given, put them all in one zip, and add a short note listing anything that is not as asked.

## The style paragraph (it starts every request)

> A 1970s professional studio tape machine, photographed straight on, for a realistic software panel. Dark anodised steel and black crackle paint, with brushed aluminium reels, black rubber, and aged brass lettering and trim. It is a working machine that is well kept: fine scratches and a little dust, no rust, no dents, no grime. One soft light from the upper left at about 45 degrees, so highlights sit upper left and shadows fall lower right. Flat, orthographic, top-down view with no perspective, no tilt and no camera blur. No text, logos, numbers or labels unless this request lists exact lettering. No background scenery and no drop shadow outside the object unless asked.

**Palette (hex):**

| role | hex |
|---|---|
| near-black ink | #0b0d0c |
| panel steel | #1a1d1b |
| panel steel, raised | #232724 |
| edge / bevel | #33382f |
| bevel highlight | #4a5145 |
| brass lettering | #c9b27a |
| brass, dim | #8a7a52 |
| brass, low | #5a5138 |
| meter paper | #d8cfb4 |
| amber lamp | #ffb340 |
| red lamp | #d2412c |
| green lamp | #7fbf6a |
| tape oxide brown | #4a3220 |
| smoked glass | #0a0b0a |

**Pairs:** mt-key-down + mt-key-up; mt-button-down + mt-button-up; mt-lamp-amber-off + mt-lamp-amber-on; mt-lamp-red-off + mt-lamp-red-on; mt-lamp-green-off + mt-lamp-green-on.

## Surfaces

Opaque textures that repeat across the panel. They must tile: the left edge continues into the right edge and the top into the bottom, with no border, no vignette and no single feature that would show as a repeat.

### mt-deck.png

*1024 x 1024 (square), opaque, tiling. On screen: the upper half of the panel, 1600 x 436 px.*

A seamless tiling texture of black crackle-finish paint over steel, very fine and even, slightly lighter than near-black (average about #1d201e). Fill the whole square edge to edge. No screws, no seams, no features, no vignette. It must tile in both directions. It must tile seamlessly in both directions.

### mt-steel.png

*1024 x 1024 (square), opaque, tiling. On screen: the control panel behind the plates.*

A seamless tiling texture of dark anodised steel (average #1a1d1b) with a very faint horizontal brush grain. Fill the whole square edge to edge. No features, no vignette. It must tile in both directions. It must tile seamlessly in both directions.

### mt-plate.png

*1024 x 1024 (square), opaque, tiling. On screen: eight control plates, 228 to 528 x 170 px each.*

A seamless tiling texture of a steel plate face (average #232724) with a fine VERTICAL brush grain, lines running top to bottom. Plain face only: no edges, no bevel, no screws (the program draws those). Fill the whole square edge to edge. It must tile in both directions. It must tile seamlessly in both directions.

### mt-header.png

*1024 x 1024 (square), opaque, tiling. On screen: the strip along the top, 1600 x 64 px.*

A seamless tiling texture of satin black anodised aluminium (average #1e211f) with a fine HORIZONTAL brush grain, lines running left to right. Even lighting, no gradient (the program adds one). Fill the whole square edge to edge. It must tile in both directions. It must tile seamlessly in both directions.

### mt-headplate.png

*1024 x 1024 (square), opaque, tiling. On screen: the plate under the tape heads, 380 x 83 px.*

A seamless tiling texture of thick machined aluminium, darker than polished reels, with faint circular fly-cut machining marks. Plain surface only: no bevel, no screws, no edges. Fill the whole square edge to edge. It must tile in both directions. It must tile seamlessly in both directions.

## The tape machine

Parts of the deck. Each one is a separate object on a transparent background. Anything that turns is centred on a square canvas, with its pivot at the exact centre.

### mt-reel-flange.png

*1024 x 1024 (square), transparent. On screen: 327 px across.*

The front flange of a 10.5 inch professional aluminium tape reel (NAB style) with three large windows, seen straight on. Brushed aluminium with a circular brush pattern. The flange is a circle centred on the canvas and fills it to within about 15 px of each edge. The three windows are cut right through and fully transparent, because the tape on the reel shows through them. The centre hole is also cut through, fully transparent, 43 percent of the flange's diameter, because the hub sits there. One window points straight up. No tape, no label, no hub.

### mt-reel-hub.png

*1024 x 1024 (square), transparent. On screen: 141 px across.*

An NAB reel hub seen straight on: machined aluminium, with three short drive keys at 120 degrees (one pointing straight up) and a dark centre spindle. The hub is centred on the canvas and fills it to within about 20 px of each edge. The keys must be clearly visible, because they show the reel turning.

### mt-capstan.png

*1024 x 1024 (square), transparent. On screen: 12 px across.*

A tape machine capstan, a polished steel shaft seen end on: a small bright steel disc with one faint machining mark on its face so that turning is visible. Centred, filling the canvas to within about 30 px of each edge.

### mt-pinch.png

*1024 x 1024 (square), transparent. On screen: 30 px across.*

A pinch roller seen end on: a matte black rubber tyre on a small steel hub, with one bolt head off centre so that rotation is visible. Centred, filling the canvas to within about 25 px of each edge.

### mt-roller.png

*1024 x 1024 (square), transparent. On screen: 15 px across.*

A tape guide roller seen end on: chrome steel with a darker centre and one small mark on its face. Centred, filling the canvas to within about 30 px of each edge.

### mt-arm.png

*1024 x 1024 (square), transparent. On screen: about 55 px long.*

A tape tension arm seen from above: a flat steel bar with a round pivot boss at one end and a round roller boss at the other. The pivot boss is centred exactly at the centre of the canvas (512, 512); the bar runs straight to the RIGHT; the roller boss is centred at (960, 512). The bar is about 80 px wide, the pivot boss about 140 px across, the roller boss about 120 px across. Nothing else on the canvas.

### mt-head-erase.png

*1024 x 1536 (portrait), transparent. On screen: 18 x 28 px.*

A tape erase head seen from above, the tape running across its top edge: a satin dark-steel shield can with a polished head face along the top edge and a thin vertical gap line in the middle of that face. A dull red band (#a8442f) just below the face. The head fills the portrait canvas to within about 30 px of each edge. No letters.

### mt-head-record.png

*1024 x 1536 (portrait), transparent. On screen: 18 x 28 px.*

A tape record head seen from above, the tape running across its top edge: a satin dark-steel shield can with a polished head face along the top edge and a thin vertical gap line in the middle of that face. A brass band (#c9b27a) just below the face. Same shape and size as the erase head. The head fills the portrait canvas to within about 30 px of each edge. No letters.

### mt-head-play.png

*1024 x 1536 (portrait), transparent. On screen: 18 x 28 px.*

A tape playback head seen from above, the tape running across its top edge: a satin dark-steel shield can with a polished head face along the top edge and a thin vertical gap line in the middle of that face. A steel-grey band (#4a4f45) just below the face. Same shape and size as the other heads. The head fills the portrait canvas to within about 30 px of each edge. No letters.

### mt-carriage.png

*1024 x 1024 (square), transparent. On screen: 26 x 30 px.*

A small sliding carriage that carries a tape head along a rail, seen from above: a black anodised sled with two visible slider shoes at the bottom. It fills the canvas to within about 25 px of each edge.

### mt-rail.png

*1536 x 1024 (landscape), transparent, tiling. On screen: 345 x 10 px.*

A ground steel guide rail seen from above: one straight horizontal bar about 64 px tall, centred vertically, running the full width of the canvas from the left edge to the right edge, with a polished top edge. It must tile left to right (the left end continues into the right end). Everything above and below the bar is fully transparent. No scale marks. It must tile seamlessly left to right.

### mt-looptank.png

*1024 x 1024 (square), semi-transparent. On screen: 273 x 248 px.*

The front of a tape loop tank, seen straight on: a dark steel frame about 120 px wide all round, filling the canvas edge to edge, with smoked glass inside the frame. The glass is see-through at about 35 percent opacity (alpha about 90), so the folded tape drawn behind it shows through, with one faint diagonal reflection in its upper left. The frame is fully opaque. No text.

## Controls

Knobs, keys, buttons, lamps and meters. Keys, buttons and frames are stretched by the program to fit their labels, so their edges must stay inside the stated border band and the middle must be plain.

### mt-knob.png

*1024 x 1024 (square), transparent. On screen: 38, 46 and 58 px.*

A control knob seen from straight above: a black knurled skirt with a dark aluminium cap. The knob is a circle 676 px across, centred on the canvas, so the program can draw its scale and position arc around it. The skirt has fine radial ridges; the cap is about 70 percent of the knob's width, slightly domed and satin. A cream pointer line (#ffcf7a) runs from the cap's centre straight UP to the skirt's edge. Nothing outside the knob's circle.

### mt-key-up.png

*1536 x 1024 (landscape), opaque, paired with mt-key-down. On screen: about 26 px tall, width set by its label.*

A raised black selector key with a soft highlight along its top edge, filling the canvas edge to edge. The bevel and edges stay inside the outer 160 px on every side; everything inside that band is a plain flat key face, because the program stretches the middle. No lettering.

### mt-key-down.png

*1536 x 1024 (landscape), opaque, paired with mt-key-up. On screen: about 26 px tall.*

The same selector key as mt-key-up, at the same size and position, now pressed in and lit: a pale brass face (#e2cd95 at the top to #b39a5c at the bottom). Same edges inside the outer 160 px band, plain face inside it. No lettering.

### mt-button-up.png

*1536 x 1024 (landscape), opaque, paired with mt-button-down. On screen: 26 to 28 px tall.*

A raised dark steel push button with a slight dome, filling the canvas edge to edge. The bevel and edges stay inside the outer 160 px on every side; everything inside that band is a plain face, because the program stretches the middle. No lettering.

### mt-button-down.png

*1536 x 1024 (landscape), opaque, paired with mt-button-up. On screen: 26 to 28 px tall.*

The same push button as mt-button-up, at the same size and position, pushed in: darker along its top edge, as if pressed down a few millimetres. Same edges inside the outer 160 px band, plain face inside it. No lettering.

### mt-lamp-amber-off.png

*1024 x 1024 (square), transparent, paired with mt-lamp-amber-on. On screen: 8 to 10 px.*

A round panel indicator lamp, OFF: a dark amber glass dome lens 640 px across in a thin black bezel, centred on the canvas. Nothing outside the bezel.

### mt-lamp-amber-on.png

*1024 x 1024 (square), transparent, paired with mt-lamp-amber-off. On screen: 8 to 10 px.*

The same panel lamp as mt-lamp-amber-off, same size and position, now ON: lit from inside in amber (#ffb340), with a soft glow around the bezel that fades to fully transparent before it reaches the edge of the canvas.

### mt-lamp-red-off.png

*1024 x 1024 (square), transparent, paired with mt-lamp-red-on. On screen: 8 to 10 px.*

A round panel indicator lamp, OFF: a dark red glass dome lens 640 px across in a thin black bezel, centred on the canvas. Nothing outside the bezel.

### mt-lamp-red-on.png

*1024 x 1024 (square), transparent, paired with mt-lamp-red-off. On screen: 8 to 10 px.*

The same panel lamp as mt-lamp-red-off, same size and position, now ON: lit from inside in red (#d2412c), with a soft glow that fades to fully transparent before it reaches the edge of the canvas.

### mt-lamp-green-off.png

*1024 x 1024 (square), transparent, paired with mt-lamp-green-on. On screen: 8 to 10 px.*

A round panel indicator lamp, OFF: a dark green glass dome lens 640 px across in a thin black bezel, centred on the canvas. Nothing outside the bezel.

### mt-lamp-green-on.png

*1024 x 1024 (square), transparent, paired with mt-lamp-green-off. On screen: 8 to 10 px.*

The same panel lamp as mt-lamp-green-off, same size and position, now ON: lit from inside in green (#7fbf6a), with a soft glow that fades to fully transparent before it reaches the edge of the canvas.

### mt-slider-track.png

*1536 x 1024 (landscape), opaque. On screen: 176 x 10 px.*

A sunken black fader slot, filling the canvas edge to edge: a recessed channel with a dark inner shadow along its top edge. The edges stay inside the outer 160 px on every side; the middle is plain, because the program stretches it.

### mt-slider-thumb.png

*1024 x 1536 (portrait), transparent. On screen: 12 x 20 px.*

A brushed steel fader cap seen from above, with one dark centre line running top to bottom. It fills the portrait canvas to within about 50 px of each edge.

### mt-screw.png

*1024 x 1024 (square), transparent. On screen: 9 px across.*

A slotted panel screw head seen straight on, dark steel with a soft highlight upper left. The slot runs left to right through the centre. Centred, filling the canvas to within about 40 px of each edge.

### mt-vu-face.png

*1536 x 1024 (landscape), opaque. On screen: 112 x 56 px.*

A VU meter face, filling the canvas edge to edge: a black bezel about 160 px wide all round, and inside it a warm cream paper face (#d8cfb4), slightly aged and evenly lit, with a faint inner shadow under the bezel's top edge. Leave the paper completely blank: no scale, no numbers, no letters, no needle (the program draws them exactly).

### mt-vu-needle.png

*1024 x 1536 (portrait), transparent. On screen: about 60 px long.*

A thin black meter needle pointing straight UP, with its pivot at the bottom centre of the canvas (512, 1500) and tapering to a fine point near the top (about 512, 40). About 16 px wide at the pivot. Nothing else on the canvas.

### mt-glass.png

*1536 x 1024 (landscape), semi-transparent. On screen: readout windows, 44 to 150 px wide.*

Smoked glass for a readout window, filling the canvas edge to edge: dark glass at about 25 percent opacity (alpha about 64) with a faint highlight along the top edge. It is laid over glowing amber digits, so it must stay see-through. The highlight stays inside the outer 160 px band; the middle is even, because the program stretches it.

## Lettering

Only these parts carry text. If the image tool cannot spell the text exactly, deliver the part blank (or not at all, where stated) and the program letters it.

### mt-plaque-left.png

*1024 x 1024 (square), opaque. On screen: 112 x 104 px.*

A small engraved plate of dark aged brass, filling the canvas edge to edge, with a fine bevel and two small screws (top centre and bottom centre). Engraved and filled with dark paint, centred, uppercase, in a plain engineering typeface, on separate lines: BROKILD (larger), then MAGNETO, TRANSPORT, MODEL MT-1, FLUX MEMORY, TAPE ECHO. If the spelling cannot be made exact, deliver the plate blank.

### mt-plaque-right.png

*1024 x 1024 (square), opaque. On screen: 112 x 104 px.*

A small engraved plate of dark aged brass, matching mt-plaque-left, filling the canvas edge to edge, with a fine bevel and two small screws (top centre and bottom centre). Engraved and filled with dark paint, centred, uppercase, in a plain engineering typeface, on separate lines: CAUTION (larger), then KEEP HEADS, CLEAN, DEMAGNETISE, WEEKLY, 6.3 mm TAPE. If the spelling cannot be made exact, deliver the plate blank.

### mt-wordmark.png

*1536 x 1024 (landscape), transparent. On screen: about 250 x 30 px.*

The name Magneto Transport, lettered in polished brass (#c9b27a), slightly raised, with a thin dark edge, in a condensed, slightly wide-spaced engineering typeface. Mixed case exactly as written: Magneto Transport. One line, centred vertically, spanning the canvas width to within about 40 px of each side. Everything else fully transparent. If the spelling cannot be made exact, do not deliver this part.

