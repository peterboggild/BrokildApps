# Drum decals delivered

Generated PNG assets for the shared industrial control family and four machine finishes. All crop rectangles use `[left, top, right, bottom]` pixel coordinates; right and bottom are exclusive. A companion `manifest.json` provides the same information in machine-readable form.

## Validation and deviations

- All 13 PNGs decode and have the specified canvas sizes. The four grounds are opaque 1024 × 1024; all other assets contain transparent pixels.
- The four grounds were made periodic along both axes; opposing terminal pixels match exactly. Their photographic details are not mathematically periodic in slope across the seam.
- All knobs and levers are overhead and point upward in their default state. The handwheel spinner lies toward the lower right while one spoke points upward, as specified.
- The 512 px sprite cells contain individual isolated parts with transparent separation. To avoid clipped silhouettes the parts have roughly 24 px padding, and the explicitly small knob is about 70–75% of its cell. This differs from a literal edge-to-edge nontransparent fill.
- The lamp on/off states reuse exactly the same bezel pixels outside their glass lens. The photographic switch/button pairs have been aligned to the same 512 px cells, but their housings and screws are separate generated drawings and are **not pixel-identical**. They will visibly jump if swapped as animation states. Sheet 2 therefore needs a further redraw or compositing pass before production use.
- The three label-tape strips use central uninterrupted portions of the generated artwork and have their horizontal end pixels blended; there are no photographed cut ends. They are horizontally repeatable, with slight differences in high-frequency texture at the seam.
- HATS OFF was generated with a checkerboard baked into its image; that outside region was masked away. The visible brass plate and exact lettering remain.

## Crop rectangles

### `01-knobs.png` — 2048 × 1536

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| bakelite-large | `[0, 0, 512, 512]` | `[24, 24, 488, 487]` |
| bakelite-small | `[512, 0, 1024, 512]` | `[588, 76, 948, 435]` |
| star-knob | `[1024, 0, 1536, 512]` | `[1048, 35, 1512, 476]` |
| chicken-head | `[1536, 0, 2048, 512]` | `[1675, 24, 1908, 488]` |
| handwheel | `[0, 512, 512, 1024]` | `[24, 548, 488, 987]` |
| steel-set-knob | `[512, 512, 1024, 1024]` | `[587, 561, 948, 976]` |
| crank-lever | `[1024, 512, 1536, 1024]` | `[1184, 536, 1376, 1000]` |
| red-bakelite | `[1536, 512, 2048, 1024]` | `[1562, 537, 2022, 1000]` |
| bronze-knob | `[0, 1024, 512, 1536]` | `[25, 1051, 488, 1508]` |
| pointer-skirt | `[512, 1024, 1024, 1536]` | `[538, 1048, 997, 1512]` |
| detent-plate | `[1024, 1024, 1536, 1536]` | `[1052, 1048, 1509, 1512]` |
| empty | `[1536, 1024, 2048, 1536]` | empty / opaque |

### `02-switches.png` — 2048 × 2048

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| bat-up | `[0, 0, 512, 512]` | `[47, 24, 464, 488]` |
| bat-down | `[512, 0, 1024, 512]` | `[540, 24, 996, 488]` |
| guard-closed | `[1024, 0, 1536, 512]` | `[1059, 24, 1500, 487]` |
| guard-open | `[1536, 0, 2048, 512]` | `[1604, 25, 1979, 487]` |
| hit-up | `[0, 512, 512, 1024]` | `[29, 536, 483, 999]` |
| hit-pressed | `[512, 512, 1024, 1024]` | `[536, 569, 1000, 966]` |
| estop-up | `[1024, 512, 1536, 1024]` | `[1048, 537, 1512, 999]` |
| estop-pressed | `[1536, 512, 2048, 1024]` | `[1560, 567, 2024, 968]` |
| square-up | `[0, 1024, 512, 1536]` | `[24, 1069, 488, 1490]` |
| square-pressed | `[512, 1024, 1024, 1536]` | `[537, 1069, 1000, 1491]` |
| cam-up | `[1024, 1024, 1536, 1536]` | `[1048, 1062, 1511, 1498]` |
| cam-right | `[1536, 1024, 2048, 1536]` | `[1560, 1084, 2024, 1476]` |
| slide-top | `[0, 1536, 512, 2048]` | `[113, 1560, 398, 2024]` |
| slide-bottom | `[512, 1536, 1024, 2048]` | `[606, 1560, 929, 2023]` |
| key-up | `[1024, 1536, 1536, 2048]` | `[1062, 1560, 1497, 2024]` |
| key-right | `[1536, 1536, 2048, 2048]` | `[1560, 1590, 2024, 1993]` |

### `03-lamps-gauges.png` — 2048 × 1536

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| amber-off | `[0, 0, 512, 512]` | `[24, 57, 487, 454]` |
| amber-on | `[512, 0, 1024, 512]` | `[536, 56, 1000, 455]` |
| red-off | `[1024, 0, 1536, 512]` | `[1048, 56, 1512, 455]` |
| red-on | `[1536, 0, 2048, 512]` | `[1560, 55, 2024, 456]` |
| green-off | `[0, 512, 512, 1024]` | `[24, 568, 488, 968]` |
| green-on | `[512, 512, 1024, 1024]` | `[536, 567, 1000, 967]` |
| bullseye-off | `[1024, 512, 1536, 1024]` | `[1048, 567, 1512, 968]` |
| bullseye-on | `[1536, 512, 2048, 1024]` | `[1560, 567, 2024, 969]` |
| pressure-gauge | `[0, 1024, 512, 1536]` | `[24, 1048, 487, 1512]` |
| pressure-needle | `[512, 1024, 1024, 1536]` | `[710, 1072, 826, 1488]` |
| panel-meter | `[1024, 1024, 1536, 1536]` | `[1048, 1117, 1512, 1442]` |
| meter-needle | `[1536, 1024, 2048, 1536]` | `[1743, 1072, 1840, 1487]` |

### `04-labels-plates.png` — 2048 × 1024

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| black-tape | `[0, 32, 1024, 160]` | `[0, 32, 1024, 160]` |
| red-tape | `[0, 192, 1024, 320]` | `[0, 192, 1024, 320]` |
| olive-tape | `[0, 352, 1024, 480]` | `[0, 352, 1024, 480]` |
| black-enamel-plate | `[0, 640, 512, 896]` | `[11, 646, 501, 890]` |
| brass-plate | `[512, 640, 1024, 896]` | `[526, 646, 1010, 890]` |
| olive-stencil-plate | `[1024, 640, 1536, 896]` | `[1036, 646, 1524, 890]` |

### `kickstart-ground.png` — 1024 × 1024

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| seamless-ground | `[0, 0, 1024, 1024]` | empty / opaque |

### `snare-tactics-ground.png` — 1024 × 1024

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| seamless-ground | `[0, 0, 1024, 1024]` | empty / opaque |

### `hats-off-ground.png` — 1024 × 1024

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| seamless-ground | `[0, 0, 1024, 1024]` | empty / opaque |

### `beet-ground.png` — 1024 × 1024

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| seamless-ground | `[0, 0, 1024, 1024]` | empty / opaque |

### `kickstart-nameplate.png` — 1600 × 400

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| kickstart | `[0, 0, 1600, 400]` | `[138, 5, 1462, 396]` |

### `snare-tactics-nameplate.png` — 1600 × 400

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| snare-tactics | `[0, 0, 1600, 400]` | `[97, 4, 1503, 396]` |

### `hats-off-nameplate.png` — 1600 × 400

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| hats-off | `[0, 0, 1600, 400]` | `[210, 4, 1390, 396]` |

### `beet-nameplate.png` — 1600 × 400

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| beet | `[0, 0, 1600, 400]` | `[112, 4, 1488, 396]` |

### `beet-machine-bay.png` — 1024 × 1400

| Part | Crop `[L,T,R,B]` | Alpha bounds `[L,T,R,B]` |
|---|---:|---:|
| machine-bay | `[0, 0, 1024, 1400]` | `[0, 0, 1024, 1394]` |

