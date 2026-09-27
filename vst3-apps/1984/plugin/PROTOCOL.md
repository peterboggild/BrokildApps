# 1984 — page ↔ native contract

The processor owns every value. The page (`Source/ui/ui.html`) is a view: it
draws what it is told, sends what the user does, and never keeps a truth of
its own. This file is the whole contract; the panel and the processor were
written against it in parallel.

## Transport

JUCE `WebBrowserComponent` native integration.

```js
const NB = {
  send(o){ try { window.__JUCE__.backend.emitEvent("n84", o); } catch (e) {} },
  on(n, f){ try { window.__JUCE__.backend.addEventListener(n, f); } catch (e) {} },
  live(){ return !!(window.__JUCE__ && window.__JUCE__.backend); }
};
```

Several page→native messages may be batched as `{ b: [ {...}, {...} ] }`.

## Page → native (`k` = kind)

| message | meaning |
|---|---|
| `{k:"hello"}` | sent once at boot, before anything else; the native side then re-pushes `initialState` until it hears `ack` |
| `{k:"ack"}` | `initialState` has been applied |
| `{k:"ready"}` | the page is painted; the editor reveals it (until then a native splash covers it) |
| `{k:"p", id, v}` | set one parameter. `v` in the parameter's own units: 0..1 for continuous kinds, an integer index for LIST/INT, 0/1 for SW |
| `{k:"note", n, on, v}` | on-screen keyboard: MIDI note `n`, `on` true/false, velocity `v` 0..1 |
| `{k:"bend", v}` | −1..1 |
| `{k:"wheel", v}` | 0..1 (mod wheel) |
| `{k:"at", v}` | 0..1 (channel aftertouch from the panel) |
| `{k:"alloff"}` | release every note |
| `{k:"panic"}` | reset the engine |
| `{k:"patch", i}` | load factory patch `i` (0..numPatches−1) |
| `{k:"random"}` | roll a random instrument |
| `{k:"save"}` / `{k:"open"}` | native Save/Open dialogs for a `.json` patch |
| `{k:"presetScan"}` / `{k:"presetFolder"}` / `{k:"presetLoad", path}` | the house patch folder browser (same as every Brokild synth) |
| `{k:"bwfx", op, ...}` | forwarded to the Brokild World FX rack (the fragment builds these itself) |

## Native → page (events)

### `initialState`
```js
{
  product: "1984", build: "260922.1",
  params: [ { id, v, hi, n, kind, def, lo, fhi, rank, names? }, ... ],
  patches: [ { n: 0, name: "BLADE BRASS", cat: "BRASS" }, ... ]
}
```
- `hi` is the maximum of `v` (1 for continuous kinds, the last index for LIST/INT).
- `kind` is the formatting law (below); `lo`/`fhi` are the formatting range.
- `rank` is −1 for a global parameter, 0 for rank I, 1 for rank II. Rank ids
  are `a_…` and `b_…`.
- `names` is present for LIST kinds.

After `initialState`, the native side also emits `bwfx` (the rack) and
`patchinfo`.

### `hostParam`
`{ p: [ {id, v}, ... ] }` — values that changed on the native side
(automation, a patch load, a randomise). The page's own `p` sends are not
echoed back. **Every consumer of a value must redraw on this** (the sliders,
the value readouts, the tape reels, the VFD) — a control can be working and
still look broken.

### `meter` (30 Hz while the editor is open)
```js
{ out, peak,                 // rms 0..1, peak 0..1
  feg: [f1, f2],             // the two filter envelopes, −1..1
  aeg: [a1, a2],             // the two amplifier envelopes, 0..1
  cut: [hz1, hz2],           // the two low-pass cutoffs, Hz
  lfo,                       // the sub-oscillator, −1..1
  wow,                       // tape wow+flutter excursion, about −1..1
  drop,                      // true during a tape dropout
  notes: [8],                // MIDI note per voice, −1 when silent
  levels: [8],               // amplifier level per voice, 0..1
  held: [ ... ],             // MIDI notes currently held
  scope: [512],              // the last 512 output samples, oldest first
  bend, wheel, at }          // the performance controls as the host has them
```

### `notice`  `{ msg }` — one line for the display.
### `patchinfo`  `{ i, name, cat }` — the factory patch on the dial (or `i: -1, name` for a loaded user patch / edited state).
### `presetTree`  `{ folder, exists, items:[ {n, p} | {n, d:true, i:[...]} ] }`
### `bwfx`  — the rack state; hand it to `BWFX.onState(p)`.

## Parameter kinds (formatting laws)

| kind | name | display |
|---|---|---|
| 0 | PCT | `round(v*100) %` |
| 1 | SW | ON / OFF |
| 2 | HZ | `lo * (fhi/lo)^v` Hz (2 decimals under 100, 0 under 1000, else kHz with 2) |
| 3 | MS | `lo * (fhi/lo)^v` ms (1 decimal under 100, 0 under 1000, else s with 2) |
| 4 | LIST | `names[round(v)]` |
| 5 | SEMI | `round((v-0.5)*24)` semitones, signed |
| 6 | CENT | `(v-0.5)*lo` cents, signed |
| 7 | CENTU | `v*lo` cents |
| 8 | BIPOL | `(v-0.5)*200 %`, signed |
| 9 | GLIDE | v < 0.01 → OFF, else `lo * (fhi/lo)^v` ms |
| 10 | VOL | gain `2v²` → dB (`-inf` at 0) |
| 11 | PW | `50 + 45v %` |
| 12 | INT | `round(v)` |
| 13 | SEC | `lo * (fhi/lo)^v` s (2 decimals) |

`ring_speed` is HZ, but when `ring_key` is on the engine reads it as a
ratio of the note, `0.25 * 32^v` (0.25×…8×); the page shows the ratio then.

## The parameter table

Global: `mode glide gliss legato tune fine bend vintage os volume brill reso
ktrack spread unidet patch`. Rank I `a_` and rank II `b_`: `saw pulse pw pwm
tri sine noise oct semi fine hpf hpq lpf lpq fmode il al fa fd fr va vd vs vr
lvl pan vel velb`, plus `b_sync`. Ring: `ring_mode ring_speed ring_key
ring_depth ring_a ring_d ring_mod`. Poly-mod: `pm_o2pitch pm_o2pw pm_o2filt
pm_envpitch pm_envpw`. Sub-oscillator: `lfo_wave lfo_rate lfo_pitch lfo_pw
lfo_vcf lfo_vca lfo_delay lfo_mode`. Wheel/touch: `wheel_lfo wheel_brill
wheel_rate at_pitch at_brill at_lvl at_lfo`. Chain: `drv_mode drv_amt
drv_tone ens_mode ens_rate ens_depth ens_mix choir_mix choir_vowel choir_reg
choir_air tape_mode tape_wow tape_wowrate tape_flut tape_sat tape_age
tape_drop tape_hiss hall_mix hall_pre hall_size hall_decay hall_damp hall_mod
hall_shim`. Plus the five BWFX macros, `bwfx_macro1..5`, which the rack
fragment owns.

The page never hard-codes this list: it builds itself from `initialState`
and only decides WHERE each id is drawn. An id it does not know goes into a
spare "MORE" strip rather than being dropped.

## Debug hook

The page exposes `window.__N84 = { get(id), set(id, v), state(), notes(),
meter(), version }` — the model lives in a closure and a probe has no other
way to steer or read it.
