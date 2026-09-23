# Thirty Thousand Years — page ↔ native contract

The processor owns every value. The page (`Source/ui/ui.html`) is a view: it
draws what it is told, sends what the user does, and never keeps a truth of
its own. This file is the whole contract; the panel and the processor are
written against it in parallel.

## Transport

JUCE `WebBrowserComponent` native integration. The event name is `tty`.

```js
const NB = {
  send(o){ try { window.__JUCE__.backend.emitEvent("tty", o); } catch (e) {} },
  on(n, f){ try { window.__JUCE__.backend.addEventListener(n, f); } catch (e) {} },
  live(){ return !!(window.__JUCE__ && window.__JUCE__.backend); }
};
```

Several page→native messages may be batched as `{ b: [ {...}, {...} ] }`.

## Page → native (`k` = kind)

| message | meaning |
|---|---|
| `{k:"hello"}` | once at boot, before anything else; the native side then re-pushes `initialState` until it hears `ack` |
| `{k:"ack"}` | `initialState` has been applied |
| `{k:"ready"}` | the page is painted; the editor reveals it (a native splash covers it until then) |
| `{k:"p", id, v}` | set one host parameter. `v` in the parameter's own units: 0..1 for continuous kinds, an integer for LIST/INT/NOTE, 0/1 for SW |
| `{k:"note", n, on, v}` | on-screen keyboard: MIDI note `n`, `on` true/false, velocity `v` 0..1 |
| `{k:"bend", v}` | −1..1 |
| `{k:"wheel", v}` | 0..1 |
| `{k:"at", v}` | 0..1 aftertouch |
| `{k:"alloff"}` | release every key |
| `{k:"panic"}` | fade out in 20 ms, clear everything, switch DRONE off, stay stopped until a note or DRONE |
| `{k:"strike", v}` | inject energy into every STRUCTURE (`v` 0..1, optional) |
| `{k:"patch", i}` | load factory preset `i` |
| `{k:"mutate", amount, lock}` | a bounded mutation of the unlocked groups. `amount` 0..1. `lock` is a bitmask: 1 MASS, 2 SIGNAL, 4 MEMORY, 8 STRUCTURE, 16 ENVIRONMENT, 32 LIFE, 64 global (root/rhythm/gain/routing are never mutated) |
| `{k:"undo"}` | restore the values from before the last patch load / mutate / recall |
| `{k:"ab", op:"store"|"recall", slot:"A"|"B"}` | A/B comparison slots (values + rack + scenes) |
| `{k:"save"}` / `{k:"open"}` | native Save/Open dialogs for a `.json` patch |
| `{k:"presetScan"}` / `{k:"presetFolder"}` / `{k:"presetLoad", path}` | the house patch folder browser |
| `{k:"scene", op:"store"|"recall"|"clear"|"name", i, name}` | HISTORY scenes 0..3. `store` copies the current musical values into scene i; `recall` writes scene i into the host values and moves HISTORY to it; `name` renames |
| `{k:"macro", op:"set", m, d, id, depth}` | macro `m` 0..7, destination slot `d` 0..7 → parameter `id` with `depth` −1..1 (`id:""` clears the slot) |
| `{k:"macro", op:"clear", m}` | clear every destination of macro m |
| `{k:"slot", i, src, via, dst, depth, offset, curve, slew, lo, hi, on}` | matrix slot `i` 0..31. `src`/`via` are source indices (see `sources` in initialState), `dst` a parameter id (or `""`), `depth`/`offset` −1..1, `curve` 0 LINEAR 1 EXP 2 LOG 3 S, `slew` seconds, `lo`/`hi` −1..1 |
| `{k:"mseg", i, trig, loop, pts:[{t,l,c},...]}` | shape envelope `i` 0..3. `trig` 0 NOTE 1 DRONE 2 EVENT, `t` seconds (ascending), `l` 0..1, `c` −1..1 curve, up to 8 points |
| `{k:"capture", on}` | start / stop the MEMORY capture ring (30 s) |
| `{k:"remember"}` | make the last capture persistent (saved with the patch and the project) |
| `{k:"import"}` / `{k:"importPath", path}` | import a WAV into MEMORY (dialog / a path, for probes) |
| `{k:"scala"}` / `{k:"scalaPath", path}` | import a Scala `.scl` (dialog / a path). Selecting SCALE = SCALA FILE uses it |
| `{k:"bwfx", op, ...}` | forwarded to the Brokild World FX rack (the fragment builds these) |

## Native → page (events)

### `initialState`
```js
{
  product: "Thirty Thousand Years", build: "260923.1",
  params: [ { id, v, hi, n, kind, def, lo, fhi, flags, names? }, ... ],
  presets: [ { n, name, cat, note }, ... ],
  sources: [ "—", "LFO 1", ... ],          // modulation source names, index = source id
  macroNames: [ "MASS", "DREAD", "VIOLENCE", "INSTABILITY", "CONTAMINATION", "DISTANCE", "LIFE", "HUMANITY" ],
  latency: 96,                              // samples, the output limiter's lookahead
  memLatency: 2048,                         // samples, the MEMORY spectral path
  sr: 48000
}
```
- `hi` is the maximum of `v` (1 for continuous kinds, the last index for LIST/INT/NOTE).
- `kind` is the formatting law (below); `lo`/`fhi` the formatting range.
- `flags`: bit 1 = not in a HISTORY scene, bit 2 = not automatable, bit 4 = not a modulation destination.
- `names` is present for LIST kinds.

After `initialState` the native side also emits `bwfx`, `patchinfo`, `macros`, `slots`, `mseg`, `scenes`.

### `hostParam`
`{ p: [ {id, v}, ... ] }` — host values that changed on the native side (automation, a patch load, a recall, HISTORY). The page's own `p` sends are not echoed back. **Every consumer of a value must redraw on this** — a control can be working and still look broken.

### `eff` (30 Hz)
`{ v: [ ... ] }` — the EFFECTIVE value of every parameter, in parameter order (index = position in `params`). This is base + modulation + macros + HISTORY. The page draws it as a second ring/mark on every control so modulation is visible; it never sends it back.

### `meter` (30 Hz while the editor is open)
```js
{ out, peak, lim,                 // rms 0..1, peak 0..1, limiter gain reduction 0..1
  loop, space,                    // feedback loop energy, space energy (0..1-ish)
  strata: [4],                    // MASS SIGNAL MEMORY STRUCTURE activity 0..1
  notes: [8], levels: [8],        // per voice: MIDI note (-1 silent), level 0..1
  held: [ ... ],                  // MIDI notes currently held (keys)
  history,                        // the HISTORY position in use, 0..1 (AUTO moves it)
  grains, memAct, erosion,        // live grains, MEMORY activity, the network's erosion accumulator
  voices,                         // voices in use
  spectrum: [48],                 // output spectrum, 48 log bands 30 Hz..18 kHz, 0..1 (dB scaled)
  scope: [256],                   // last 256 output samples, oldest first
  mods: { lfo:[8], env:[4], shape:[4], rnd:[4], fol:[2], evt:[4], net:{ e:[5], b:[5], t:[5] } },
  fracture,                       // true if a STRUCTURE fractured since the last meter
  capturing, capLen, hasCapture, hasImport, importName,
  stopped,                        // after PANIC, until restarted
  bend, wheel, at }
```

### `notice`  `{ msg }` — one line for the display.
### `patchinfo`  `{ i, name, cat, note }` — the factory preset on the dial (or `i: -1, name` for a loaded user patch / edited state).
### `macros`  `{ maps: [ [ {id, depth}, ... ] × 8 ] }`
### `slots`  `{ slots: [ {i, src, via, dst, depth, offset, curve, slew, lo, hi, on} × 32 ] }`
### `mseg`  `{ shapes: [ {trig, loop, pts:[{t,l,c}...]} × 4 ] }`
### `scenes`  `{ set: [bool × 4], names: [string × 4] }`
### `presetTree`  `{ folder, exists, items:[ {n, p} | {n, d:true, i:[...]} ] }`
### `bwfx`  — the rack state; hand it to `BWFX.onState(p)`.

## Parameter kinds (formatting laws)

| kind | name | display |
|---|---|---|
| 0 | PCT | `round(v*100) %` |
| 1 | SW | ON / OFF |
| 2 | HZ | `lo * (fhi/lo)^v` Hz (2 decimals under 100, 0 under 1000, else kHz with 2). Below 1 Hz show 3 decimals. |
| 3 | MS | `lo * (fhi/lo)^v` ms (1 decimal under 100, 0 under 1000, else s with 2) |
| 4 | LIST | `names[round(v)]` |
| 5 | SEMI | `(v-0.5)*lo` semitones, signed, 1 decimal |
| 6 | CENT | `(v-0.5)*lo` cents, signed |
| 7 | CENTU | `v*lo` cents |
| 8 | BIPOL | `(v-0.5)*200 %`, signed |
| 9 | GLIDE | v < 0.01 → OFF, else `lo * (fhi/lo)^v` ms |
| 10 | VOL | gain `2v²` → dB (`-inf` at 0) |
| 11 | PW | `50 + 45v %` |
| 12 | INT | `round(v)` |
| 13 | SEC | `lo * (fhi/lo)^v` s (2 decimals; minutes and seconds above 60) |
| 14 | SHZ | signed hertz, `sgn(x)*|x|^3*lo` with `x = 2v-1`, 1 decimal (fine near zero) |
| 15 | DB | `lo + v*(fhi-lo)` dB, 1 decimal |
| 16 | NOTE | `round(v)` as a note name (C2 = 36) |

## The parameter table

The ids are in `Source/Params.h` (`TTY_PARAMS`). Groups by prefix: global
(`volume ceiling quality bassmono bassmono_on outwidth vmode voices glide tune
fine bend scale rootlock mpe drone drone_root drone_chord drone_latch
drone_spread drone_vel ext_mode ext_gain duck_amt duck_rel seed determin
patch`), MASS `m_`, SIGNAL `s_`, MEMORY `mem_`, STRUCTURE `st_`, ENVIRONMENT
`e_`, LIFE `l1_..l8_` (LFOs), `e1_..e4_` (envelopes), `r1_..r4_` (random),
`f1_ f2_` (followers), `v1_..v4_` (event lanes), `l_` (the network), HISTORY
`h_`, macros `mac_`. Every stratum has the channel strip `_on _gain _pan
_width _hp _lp _send _loop _mute _solo _drone _keys`. Plus the five BWFX macros
`bwfx_macro1..5`, which the rack fragment owns.

The page never hard-codes this list: it builds itself from `initialState`
and only decides WHERE each id is drawn. An id it does not know goes into a
spare "MORE" strip rather than being dropped.

## Debug hook

The page exposes `window.__TTY = { get(id), set(id, v), eff(id), state(), meter(), view(name), version }`
— the model lives in a closure and a probe has no other way to steer or read it.
