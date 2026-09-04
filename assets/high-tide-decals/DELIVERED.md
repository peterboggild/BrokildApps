# HIGH TIDE decal delivery

All twelve standalone assets are included at their exact requested dimensions. The three ground textures have been edge-matched for seamless tiling. Transparent component assets were alpha-trimmed before final scaling so circular pieces are centred and fill their square frames.

```json
{
  "version": 1,
  "set": "high-tide-decals",
  "machine": "High Tide",
  "files": [
    {"file": "ht-ground.png",    "size": [2048, 2048], "tileable": true,  "what": "Chart-table ground, sea-ink leather/linen"},
    {"file": "ht-paper.png",     "size": [2048, 1024], "tileable": true,  "what": "Cream laid tide-table paper"},
    {"file": "ht-wood.png",      "size": [4096, 256],  "tileable": true,  "what": "Dark oak rail, tiles left to right"},
    {"file": "ht-bezel.png",     "size": [512, 512],   "tileable": false, "what": "Brass instrument bezel ring, open centre"},
    {"file": "ht-knob.png",      "size": [512, 512],   "tileable": false, "what": "Brass knob cap, dark pointer at 12"},
    {"file": "ht-tack.png",      "size": [256, 256],   "tileable": false, "what": "Brass chart tack, from above"},
    {"file": "ht-flag.png",      "size": [160, 200],   "tileable": false, "what": "Red enamel pennant on brass pin, side view"},
    {"file": "ht-nameplate.png", "size": [1600, 360],  "tileable": false, "what": "Engraved brass nameplate, HIGH TIDE"},
    {"file": "ht-glass.png",     "size": [64, 512],    "tileable": false, "what": "Empty tide-gauge glass tube, brass ferrules"},
    {"file": "ht-pearl.png",     "size": [256, 256],   "tileable": false, "what": "Nacre pearl, lit upper left"},
    {"file": "ht-rose.png",      "size": [1024, 1024], "tileable": false, "what": "Compass rose, ink only, no lettering"},
    {"file": "ht-cover.png",     "size": [1600, 900],  "tileable": false, "what": "Manual cover painting, opaque"}
  ]
}
```

Production note: the source generations were normalized mechanically to the specified dimensions. The glass is especially narrow by design; inspect it in-panel at its actual rendered width, where its edge highlights read correctly.

## Redelivery after `REDO.md`

- `ht-ground.png` — regenerated full-frame sea-ink woven chart-table texture with visibly stronger material grain; seamless on both axes.
- `ht-paper.png` — regenerated full-frame cream laid-paper texture with visible fibres, pulp cloudiness and subtle age; seamless on both axes.
- `ht-nameplate.png` — regenerated full-frame engraved brass plate with intact `HIGH TIDE` capitals, four screws and wave line.
- `ht-rose.png` — regenerated as a complete, centred circular compass rose in ink-only transparency with no lettering.
