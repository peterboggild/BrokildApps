# Magneto Transport — the page ↔ native contract (stage 8)

The panel (`Source/ui/ui.html`) is a pure view. The C++ owns every value; the page
asks and draws. Every file the page loads is served from BinaryData by its BASENAME
(`PluginEditor.cpp: namedResource`), so `fetch("tips.json")`, `fetch("figdata.json")`
and `fetch("tips-numbers.json")` work, and so would an image `foo.png`. New files are
added to `juce_add_binary_data` in CMakeLists.txt by the native side.

Bridge: `window.__JUCE__.backend.emitEvent("mt", obj)` up; `addEventListener(name, f)`
down. The existing `NB.send / NB.on` wrapper in ui.html stays.

## Page → native (`emitEvent("mt", {...})`)

| message | meaning |
|---|---|
| `{k:"hello"}` | sent on every page boot; native replies with `initialState` |
| `{k:"stateack"}` | the page has applied `initialState` |
| `{k:"touch", id, down}` | a gesture begins/ends on a control (hosts need it for automation learn) |
| `{k:"p", id, v}` | set a parameter, **physical units** (cm, cm/s, dB, %, choice index, 0/1) |
| `{k:"patternSet"}` | HEAD PATTERN: snap heads 2/3 (and the non-tempo heads) to the pattern in parameter `pattern` around the TEMPO HEAD. Native writes `h1pos/h2pos/h3pos` once (with gestures) and replies `patternResult` |
| `{k:"presetList"}` | native replies `presets` |
| `{k:"presetLoad", factory:i}` | load factory preset i |
| `{k:"presetLoad", user:"name"}` | load a user preset by name (from the user folder) |
| `{k:"presetSave", name:"…"}` | save the current state as a user preset (native writes `Documents\Brokild patches\Magneto Transport\<name>.json`; refuses names with path characters); replies `presets` |
| `{k:"presetDelete", user:"name"}` | moves that file to a `Deleted` subfolder; replies `presets` |
| `{k:"quality", v}` | 0 ECO, 1 NORMAL, 2 HIGH. A fresh tape is threaded (the internal rate changes). Saved with the project |
| `{k:"settings", tooltips:bool, delay:seconds}` | user preferences (NOT the project): kept in `%APPDATA%\Brokild\Magneto Transport.settings` |

## Native → page (events)

### `initialState`
```
{ build:"261011.9", rate:96000,
  params:[ {id,name,kind:"lin"|"log"|"choice"|"toggle",lo,hi,def,unit,choices:"A|B",hint,v}, ... ],
  quality:1, settings:{tooltips:true, delay:0.8},
  preset:{name:"…", dirty:false} }
```
### `hostParam`  `{params:[{id,v}]}` — automation, host undo, state load, a pattern snap
### `presets`
```
{ factory:[{index,name,group}], user:[{name}], current:{name, factory:i|-1, dirty:bool} }
```
Groups, in order: `CLEAN`, `CLASSIC ECHO`, `MULTI-HEAD`, `CHARACTER`, `LOOPS`, `STRANGE`.
### `patternResult`  `{ok:bool, outMask:int, pos:[cm,cm,cm]}` — `outMask` bit i set: head i+1 would fall off the rail (1.5–24 cm) and was clamped
### `tele` (30 Hz) — everything in the stage 7 telemetry, plus:
| field | meaning |
|---|---|
| `vuIn`, `vuOut` | rms over the last interval, dBFS (for VU needles: apply 300 ms VU ballistics in the page; 0 VU = −18 dBFS) |
| `quality` | 0/1/2 as running |
| `latencyMs` | what the host is told (REPRO) |
| `patternFit` | `{mask}` for the CURRENT pattern parameter and tempo head, live, so the panel can say "head 3 out of reach" before SET |

## The tooltip cards (BUGLIST 6)

Three files, all generated, all fetched by the page at boot:

### `tips.json` (built by `tools/build-tips.js` from `docs/tips/tips.src.json`)
```
{ "version": 1,
  "cards": {
    "<id>": { "title": "BIAS",
              "does": "<p>…one or two sentences, HTML…</p>",
              "hood": "<p>…</p> with MathML already converted, <figure data-fig=\"biascurve\"></figure> and <span data-num=\"ref.bias_erasure_db\"></span>",
              "notModelled": "<p>…</p> or \"\"" } } }
```
Card ids: every parameter id (`bias`, `h1pos`, …) and these parts of the machine drawing
and panel: `part:supply`, `part:takeup`, `part:looptank`, `part:erase`, `part:record`,
`part:play` (the three playback heads share one card; the page names the head),
`part:capstan`, `part:pinch`, `part:guides`, `part:tape`, `part:rail`, `read:wf` (the W&F
meter), `read:vu` (the VU meters), `read:tapemeter`, `read:loop` (the LOOP readout),
`set:tooltips`, `set:quality`, `set:presets`, `part:pattern` (HEAD PATTERN + SET).

### `tips-numbers.json` (written by the bench: `mttest --tips=<path>`)
```
{ "build":"261011.9", "numbers": { "<key>": { "value": -34.5, "unit": "dB", "text": "−34.5 dB" } } }
```
The page fills every `<span data-num="key">` with `text`. A key missing at runtime shows
`—` and logs to `window.__errors` (the probe fails on it).

### `figdata.json` (written by `tools/build-figdata.js` from the reference and the bench)
```
{ "<name>": { "title":"…",
              "x":{"label":"bias","unit":"Hc","log":false}, "y":{"label":"…","unit":"dB","log":false},
              "series":[ {"label":"1 kHz","points":[[x,y],…]} ],
              "marks":[ {"x":3.263,"label":"operating"} ],
              "live": {"param":"bias","map":"db_to_ratio","base":3.263} | {"param":"erase","map":"table","table":[[v,x],…]} | {"tele":"speed","map":"identity"} | null } }
```
The page draws each as a small SVG line plot (axes, ticks, labels, legend) and, when
`live` is set, a vertical marker at the CURRENT value mapped to x, updated while the
card is open. `map`: `identity` (x = v), `db_to_ratio` (x = base·10^(v/20)),
`table` (piecewise linear v → x).

Figure names (all exist in figdata.json): `hysteresis`, `anhysteretic`, `biascurve`,
`karlqvist`, `recordzone`, `playbackloss`, `eqcurves`, `erase`, `dub`, `noise`,
`wowflutter`, `delaygeometry`, `feedbackloop`, `speedhd`.

### Number keys (all exist in tips-numbers.json)

Reference model (the offline Preisach model the real time is proved against):
`ref.flux_mr_nwbm`, `ref.op_bias_hc`, `ref.hs0vu_hc`, `ref.mol_db_re185`, `ref.sigma_c`,
`ref.nu`, `ref.bias_erasure_db`, `ref.overdub3k_db`, `ref.halfbias_erasure_db`,
`ref.erase_100_db`, `ref.erase_125_db`, `ref.erase_150_db`, `ref.dub_take_db`,
`ref.dub_ghost_db`, `ref.dub_ghost12_db`, `ref.dub_record_db`, `ref.dub_10k_db`,
`ref.overwrite_depth_spread_db`.

Real time, measured by the bench on this build:
`eng.unity_db`, `eng.mol_db_re185`, `eng.level_err_db`, `eng.hd3_6db_pct`,
`eng.rec_loss_15k_db`, `eng.pb_flat_db`, `eng.delay_err_us`, `eng.doppler_err_cents`,
`eng.hiss_dba`, `eng.hiss_continuous_db`, `eng.modnoise_db`, `eng.wf_fresh_pct`,
`eng.wf_worn_pct`, `eng.azimuth_us`, `eng.feedback_onset_pct`, `eng.coast_err_pct`,
`eng.dub_take_db`, `eng.dub_ghost_db`, `eng.erase_150_db`, `eng.repro_err_samples`,
`eng.hunt_pp_pct`, `eng.clog_swing_db`, `eng.splice_ms`, `eng.wear_db`,
`eng.cost_pct`, `eng.cost_eco_pct`, `eng.cost_high_pct`, `eng.alias_db`, `eng.mech_dbfs`,
`eng.checks`.

## Settings behaviour (BUGLIST 6)
- TOOLTIPS on/off and HOVER DELAY 0.2–3 s, in a gear menu in the header.
- A card opens after the delay, BESIDE the hovered element (never over it, never under
  the pointer), styled with the panel, fixed max size with its own scroll.
- Click a card (or the element while its card is open) to pin it; Esc or a click
  elsewhere closes. Pinned cards keep their live figure markers updating.
- The one-line hint bar at the bottom stays (it shows `does` without the delay).
