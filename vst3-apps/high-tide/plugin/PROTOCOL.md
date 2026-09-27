# HIGH TIDE — the page/engine contract

The panel (`Source/ui/ui.html`) and the native side (`Source/PluginProcessor.cpp`,
`Source/Engine.*`) are built against THIS file. Anything not written here does
not exist on the other side. Design: `BrokildApps/HIGH-TIDE-DESIGN.md`.

## 0. Transport

Page → native: `window.__JUCE__.backend.emitEvent("hightide", { b: [msg, msg, ...] })`
— batched into a microtask (the `NB.send` helper in every fleet page). Each msg
is `{k: "...", ...}`.

Native → page: `window.__JUCE__.backend.addEventListener(name, fn)` with the
event names of §2.

The page sends `{k:"hello"}` once at boot and the native side answers with
`initialState`, `terrain`, `lanes`, `bwfx`, `presetTree`, `patch`. A page
reload (devtools, WebView crash recovery) re-sends hello and gets everything
again — the Hairfryer handshake.

## 1. Page → native messages

| msg | meaning |
|---|---|
| `{k:"hello"}` | boot handshake |
| `{k:"p", id, v}` | set a host parameter, `v` 0..1 normalised |
| `{k:"note", n, on, vel}` | panel keyboard; `vel` 0..1 optional (0.8) |
| `{k:"drop", z, e, n, on}` | the sculptor's audition: strike a ball at position `z` (0..1) with energy `e` (0..1) on MIDI note `n`; `on:false` releases it. While it sounds, its tether target is `z` (pins ignored for this voice). |
| `{k:"panic"}` | all notes off |
| `{k:"terr", x0, nx, z0, nz, d}` | write a terrain region: `d` = base64 of Float32 row-major `[nz][nx]`, values U ≥ 0 |
| `{k:"terrAll", d}` | replace the whole terrain: base64 Float32 `[NZ][NX]` |
| `{k:"lanes", j}` | replace the lanes object (§4) |
| `{k:"factory", i}` | load factory patch `i` (terrain + lanes + params + rack) |
| `{k:"presetScan"}` `{k:"presetLoad", path}` `{k:"save"}` `{k:"open"}` `{k:"presetFolder"}` | the house patch-file vocabulary (Black Rider) |
| `{k:"pngExport"}` `{k:"pngImport"}` | native file dialogs; a 16-bit grey PNG of the terrain (§3) |
| `{k:"bwfx", op, ...}` | forwarded to `bwfx_juce::handleMessage` |

## 2. Native → page events

| event | payload |
|---|---|
| `initialState` | `{params:[{id,name,gloss,kind,v,lo,hi,list:[...]}], build, factory:[names], patch}` |
| `hostParam` | `{b:[{id,v}]}` — a parameter the page did not move itself (automation, patch, macro); the control follows |
| `terrain` | `{nx,nz,xmin,xmax,umax,d}` — `d` base64 Float32 `[nz][nx]`, whole terrain |
| `lanes` | `{j}` — the lanes object (§4) |
| `balls` | 30 Hz, `{t, lvl, scope, v:[...]}` (§5) |
| `notice` | `{msg}` — one line for the ledger |
| `patch` | `{name, user}` — what the display should call the current patch (`user` true for a loaded .json) |
| `presetTree` | `{folder, exists, items:[{n,p} \| {n,d:true,i:[...]}]}` |
| `bwfx` | the rack state (bwfx-rack.js consumes it) |

## 3. The terrain

- `NX = 512` samples along x, `NZ = 128` columns along z (a column is one "frame").
- `x ∈ [XMIN, XMAX] = [−1.5, 1.5]`, `z ∈ [0, 1]`. Index `i` ↔ `x = XMIN + i·(XMAX−XMIN)/(NX−1)`; `j` ↔ `z = j/(NZ−1)`.
- `U ≥ 0`, float. `UMAX = 4.0` is the PNG scaling: 16-bit grey = `round(U/UMAX·65535)`.
- The **reference bowl** is `U = ½x²`. A ball in it swings at one radian per unit of ball time and a note at `f` Hz runs the ball's clock at `2πf`. Full-scale energy `E = ½` reaches `x = ±1`.
- Per column: `floor(j) = argmin_i U`, `R(j) = min_i U` (the relief).
- TIDE `T` (0..1) is a water line on the relief: `Tabs = Rmin + T·(Rmax − Rmin + 1e-3)`. Effective terrain `U_eff = U − R + max(R, Tabs)` — bowls untouched, passes flooded.
- The page owns sculpting and sends regions; the native side owns the canonical copy (state, patches) and recomputes `floor`/`R` per written column.

## 4. Lanes (the timeline)

```json
{ "ver": 1,
  "pin":  { "on": [{"t":0.0,"v":0.2,"e":2}], "off": [], "loop": null },
  "tide": { "on": [], "off": [], "loop": null },
  "rock": { "on": [], "off": [], "loop": null } }
```
- `t` seconds, `v` 0..1, `e` ease INTO this point from the previous one: `0` hold (step at this point's time), `1` linear, `2` smooth (smoothstep).
- `on` points are timed from note-on; `off` points from note-off. `loop = {a, b}` (seconds, on the note-on timeline): while the note is HELD and `t ≥ a`, `t' = a + ((t − a) mod (b − a))`; points inside `[a, b]` cycle.
- Before the first `on` point the lane holds that point's value; after the last (no loop) it holds the last. At release the `off` curve starts from the value the lane HAD at the moment of release and eases to the first `off` point (no jump); after the last `off` point it holds.
- **Empty lane = no effect.** `pin` empty → the target is the POSITION parameter. `tide` / `rock` empty → the TIDE / ROCK parameters alone. A non-empty `tide`/`rock` lane is ABSOLUTE: the lane owns the value for the whole note.
- Values are per voice (each voice runs its own timeline from its own note-on).

## 5. The balls stream (30 Hz)

```
{ t: seconds of engine time,
  lvl: output peak 0..1,
  scope: base64 Float32[256] — the newest voice's mono output, base rate, most recent samples,
  v: [ { id, n, h, age, z, zt, x, xa, e, tide, rock, u:[x,z, x,z, ...] } ] }
```
- `h` 1 while held (or sustained), `age` seconds since note-on, `z` the voice's lead ball position, `zt` its tether target, `x` the lead ball's x right now (aliased — an honest sample), `xa` its swing amplitude over the last tick, `e` energy 0..1 relative to full scale, `tide`/`rock` the effective values this voice is under, `u` the other unison balls.
- The page builds the WAKE from this stream (z against age) for the most recent voice and draws it over the pin lane and on the terrain.

## 6. Parameters (`SPECS[]` in Engine.cpp — ids are forever)

kind: `0 PCT, 1 INT, 2 LIST, 3 BIPOL, 4 HZ, 5 VOL, 6 SEMI, 7 SEC`

| id | name | kind | default | meaning |
|---|---|---|---|---|
| level | LEVEL | VOL | 0.7 | output |
| tapPos | POSITION TAP | PCT | 1.0 | the ball's position — the round tap |
| tapVel | VELOCITY TAP | PCT | 0.0 | its velocity — the bright tap |
| tapFrc | FORCE TAP | PCT | 0.0 | the wall's push — the hard tap |
| tone | TONE | PCT | 0.85 | radiation lowpass, 200 Hz .. 20 kHz |
| strike | STRIKE | PCT | 0.6 | energy of a full-velocity hit (E = ½ · strike²·1.6) |
| velSens | VELOCITY | PCT | 0.7 | how much velocity scales the strike |
| friction | FRICTION | PCT | 0.12 | energy loss while held (0 = frictionless) |
| release | RELEASE | PCT | 0.45 | energy loss after note-off |
| servo | SERVO | PCT | 0.0 | 0 = the bowl's own period; 1 = the measured period is corrected each cycle |
| position | POSITION | PCT | 0.0 | the tether target when the pin lane is empty |
| hold | HOLD | PCT | 0.6 | tether strength |
| tide | TIDE | PCT | 0.5 | the water line on the relief |
| rock | ROCK | PCT | 0.0 | drive depth (see rockMode) |
| rockRatio | ROCK RATIO | LIST | 0 | `1/1, 2/1, 1/2, 1/3, 3/2, 3/1, FREE` |
| rockHz | ROCK RATE | HZ | 0.4 | 0.1 .. 60 Hz, used when FREE |
| rockMode | ROCK MODE | LIST | 0 | `ROCK` (see-saw force), `BREATH` (bowl steepens and relaxes) |
| unison | UNISON | INT | 0 | 1 .. 4 balls (v → 1 + round(v·3)) |
| spread | SPREAD | PCT | 0.3 | energy scatter between the balls, and their repulsion |
| width | WIDTH | PCT | 0.5 | stereo of the balls |
| ampA ampD ampS ampR | AMP ATTACK/DECAY/SUSTAIN/RELEASE | SEC/SEC/PCT/SEC | 0.01, 0.4, 0.8, 0.5 | the output VCA |
| modA modD modS modR | MOD ATTACK/DECAY/SUSTAIN/RELEASE | | 0.05, 0.6, 0.0, 0.4 | the mod envelope |
| modTarget | MOD TARGET | LIST | 0 | `POSITION, TIDE, ROCK, HOLD, TONE, PITCH` |
| modAmt | MOD AMOUNT | BIPOL | 0.5 | −100 .. +100 % |
| lfo1Rate lfo1Sync lfo1Shape lfo1Target lfo1Amt | LFO 1 … | HZ/LIST/LIST/LIST/BIPOL | 0.3, 0, 0, 0, 0.5 | sync `FREE, 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/8T, 1/16T`; shape `SINE, TRI, SAW, SQUARE, S&H`; target as modTarget |
| lfo2… | LFO 2 … | | | same |
| tune | TUNE | SEMI | 0.5 | ±12 |
| glide | GLIDE | PCT | 0.0 | mono/legato portamento |
| voiceMode | VOICES | LIST | 0 | `POLY, MONO, LEGATO` |
| ceiling | CEILING | PCT | 0.5 | the hard-knee ceiling's threshold |

SEC kind: `0.002 · (4000)^v` seconds (2 ms .. 8 s). HZ kind: `lo·(hi/lo)^v`. BIPOL: `v·2−1`. SEMI: `(v·2−1)·12`.
Plus the five `bwfx_macro1..5` host parameters from the adapter.

## 7. Patch file (`Documents/Brokild patches/High Tide/*.json`)

```json
{ "app":"high-tide", "kind":"patch", "version":1, "name":"...", "build":"...",
  "params": { "id": value, ... },
  "terrain": "<base64 of a 16-bit grey PNG, NX×NZ>",
  "lanes": { ... },
  "bwfx": "<rack blob>" }
```
