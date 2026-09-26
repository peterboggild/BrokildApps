# Battlestar Overdrive — list

House rule: things get **collected here** and built in batches when Peter says go.

---

## 0. SHIPPED in 260918.2

Peter played 260918.1, said it sounds great, and gave the go on the three
quality faults. All three are fixed and measured, and playing it turned up a
fourth that no bench had caught:

- **The shimmer was an oscillator** across SPACE 0.60–0.90 — Peter heard it as
  "a vibrating note that overshadows whatever comes through", from roughly 3
  o'clock up, which is exactly the band where `shimAmt` is high. The octave-up
  was added *on top of* the hall's feedback and injected **after** the damping
  filter, making it the one thing in the loop nothing ever attenuated. It grew
  to full scale and sat there — **and passed every bounds check while doing it,
  because the output ceiling keeps a runaway neatly inside full scale.**
  *Bounded is not stable, and only "stop the input and watch" can tell them
  apart.* Now in `quality.cpp` §7 as a permanent sweep.
- **The auto-gain is deleted, not slowed.** Drift after a 12 dB step:
  +1.81 / −3.65 dB → **+0.00 / −0.00**.
- **Soft ceiling** at full scale replaces the hard clamp.
- **SPACE mixes its wet** instead of summing it.
- Fuel rebuilt to Peter's spec (see §0b below).
- **`chokeEnv` now reads the louder of the two channels.**

### 0b. The fuel gag, as specified

One gauge step per **five seconds of playing** (measured 1.00 at 5 s, 4.00 at
20 s); silence costs nothing; **AUTOREFILL tops up at next-to-bottom** and the
tank never runs dry with it lit; with it off the tank dies at 71 s and the
engine **fizzles out over five seconds** while the screen blinks **FUEL EMPTY**
in red holographic letters. Pressing AUTOREFILL is the way back.

The delivered **empty** fuel frame was almost entirely transparent (mean alpha
18.7 against 252 for every other frame), so a dry tank rendered as a full one —
the panel's own amber tube showing through the glass. Backed at ingest.

---

## 1. Studio-grade sound-quality pass — MOSTLY DONE, remainder below

> "it's a tribute to a sound nerd, so making sure that the sound quality is
> excellent, studio-ready for a semi-professional, is worth doing… I would
> appreciate an assessment of what it would take to bring the sound quality up
> to the highest standard."

**Measured, not guessed** — `test/quality.cpp`, built as `boquality`. Run it
after any DSP change. What it found on build 260918.1:

### Already at standard, leave alone

| | measured |
|---|---|
| Aliasing (11 kHz in, non-harmonic content below it) | **−99.8 dB** worst (IDLE BURN); every other engine −129 dB or better |
| Harmonic purity (220+330 Hz, energy *between* the comb lines) | **−112 to −248 dB** — the products land on the harmonic grid and essentially nothing lands off it |
| Oversampler round trip, 4× | **±0.002 dB to 19 kHz** |
| Noise floor after a tone decays | **−221 dBFS** — nothing is injected anywhere |

### Three real defects, in order of how much they cost

**1. The auto-gain breathes. This is the one that matters.**
A 12 dB input step measures an **8.5 dB** output step followed by **+1.81 dB of
drift over the next two seconds**; on the way back down it drifts **−3.65 dB**.
That is plainly audible on sustained material and it is the difference between
"a pedal" and "a plugin with a compressor you did not ask for".
*Fix:* the per-engine trim table is now measured at prepare and carries the
bulk of the matching, so the auto-gain only has to carry a residual. Slow it by
an order of magnitude (or freeze it while the input is moving), and narrow its
range further. Half a day, plus re-running the level-match section of the bench.

**2. The final bound is a hard clip, and it is reached constantly.**
`clampf ±1.6`, and **116 of 300 random settings hit it**. Not a rare corner —
routine. On those settings you get hard clipping on top of the intended
distortion, which is the one kind of distortion nobody wants.
*Fix:* `ceilSoft` — transparent below ~70 % of full scale, asymptotic above —
the same function the rest of the fleet already uses. An hour.

**3. SPACE adds its wet signal rather than mixing it**, so turning the knob up
raises the output level, which is both a level surprise and a large part of why
finding 2 happens so often. *Fix:* mix rather than sum, or compensate the dry
path by the wet amount. Half a day including re-measuring the stage crossfades.

### Smaller, genuinely optional

- **The shimmer's octave-up is a two-tap crossfaded shifter** and warbles on
  sustained material. A longer window is cheap; a phase-locked shifter is not.
- **`chokeEnv` was taken from channel 0 alone** — a hard hit landing only on the
  right would not choke. Fixed in the first pass: one envelope fed by the louder
  of the two, so the image still cannot wander.

### Not a defect, worth knowing

IMD3 measures **−8 to −14 dB** across the engines and **+22.7 dB** on SUPERNOVA.
That is not a fault: intermodulation is what distortion *is*, and the residue
figures above show the products are landing where they should. Do not "fix" it.

## 1b. ANTITHRUST → width / 3D — SHIPPED in 260918.3 *(Peter, 2026-09-18)*

> "what is the antithrust doing? can this make it 3D superwide superstereo
> instead. I am not totally crazy about it as it is now — it sounds a bit
> underwhelming compared to other more awesome fx and options."

He is right, and the diagnosis is structural: **ANTITHRUST is currently the only
control that does not change the space.** The comb is identical on both channels
and the choke shares one envelope, so the whole thing lands as a tone-and-squash
control sitting next to five that are far more dramatic. The plugin also has no
width control at all, which is the gap.

**The recommendation: keep the comb, flip its polarity per channel.**
`L = x − g·d`, `R = x + g·d`. The notches on one side land on the peaks of the
other, so it is enormously wide in stereo and **sums to exactly flat in mono**
— the two combs cancel by construction. It reuses the comb that is already
there and turns the least interesting control into the widest one.

Then, rising with the same knob:
- a short **allpass decorrelation** chain with different coefficients per side
  (phase, not delay — magnitude untouched, so it stays mono-safe);
- a slight **spectral tilt** between channels, one side a shade darker, which
  is what reads as depth rather than mere width;
- the **choke** kept at the top, so the top of the sweep is still the braking,
  strangled sound — the name has to keep meaning something.

**Widening must come AFTER the drive, not before.** Distortion is nonlinear: two
slightly different signals through two distorters generate wildly different
harmonics and the image smears instead of widening. Distort once, widen after.

**At zero it must be ORDINARY STEREO, not mono** *(Peter, 2026-09-18: "can you
let the antithrust be neutral - ordinary stereo as default? just in case the
source signal is stereo")*. This is a real constraint, not just a default: it
rules out the obvious implementation. Summing to mono and re-spreading from the
sum would destroy an incoming stereo image, and would do it *worst* on exactly
the material that already sounds good. Everything above survives the rule — the
polarity-flipped comb is applied per channel and adds width on top of whatever
L/R content arrives, and allpass decorrelation leaves magnitude alone — but any
future idea has to be checked against it. The bench needs a **stereo-in,
stereo-preserved** case: a decorrelated pair in, ANTITHRUST at 0, output
bit-identical to the input path per channel.

**On doubling the engines:** CPU is not the objection — the whole plugin is 2.5 %
of a core, so a second engine is affordable. Mono compatibility is. A doubled,
drifted pair sounds spectacular and partially disappears when summed. Worth
asking Peter which he cares about before building; the polarity-flipped comb
needs no such trade.

## 1c. ANTITHRUST costs 6.8 dB — SHIPPED in 260918.3, now 2.47 dB *(Peter, 2026-09-18)*

> "antithrust changes the volume quite a lot, i dont think thats necessary"

Confirmed by measurement (`boquality` §8): **−15.2 dB at zero → −22.0 dB at
max**. He is right that it is unnecessary — and the same table explains why the
knob feels underwhelming, which is the more useful finding:

| knob | comb | 1st null | null | at the peaks | compression | level |
|---|---|---|---|---|---|---|
| 0.00 | off | — | — | — | −11.3 | **−15.2** |
| 0.50 | 2.08 ms | 481 Hz | −11.9 | −1.3 | −13.5 | **−16.5** |
| 1.00 | 0.36 ms | 2778 Hz | −17.6 | −10.0 | −14.7 | **−22.0** |

**Everything the knob does is subtractive.** The comb removes frequencies, the
lowpass closes 20 kHz → 2.4 kHz, and the level falls. The choke — the part that
was meant to be the braking character — contributes only **3.4 dB across the
whole range**, which is close to inaudible. Note the "at the peaks" column going
*negative*: a comb should BOOST at its peaks (+3.8 dB is what the transfer
function predicts at max) and instead they measure −10 dB, because the level
loss and the closing lowpass swamp them. Even the additive part subtracts.

Also unintended: at max the lowpass corner (2.4 kHz) lands on top of the comb's
first null (2.8 kHz), so the two fight over the same region.

**Peter then stated the real fault, and it is sharper than "the knob costs
level":**

> "when antithrust is on full, the level drops when mix is turned up — not what
> a drive should do"

The symptom shows up ON MIX: at ANTITHRUST full, turning MIX toward wet turns
the plugin DOWN, which on a drive is backwards.

**But MIX is not the bug, and Peter corrected me on this:**

> "mix IS matched, as long as antithrust is not turned up… because that lowers
> the level. Fix antithrust not the mix."

He is right and my first framing was over-reach. MIX is a correct linear blend
and it behaves at ANTITHRUST 0; the engines are already level-matched by a trim
table measured at prepare. **ANTITHRUST is the one block in the chain with no
level compensation at all**, so the fix belongs there and nowhere else. Widening
it to "level-match the whole wet path" would have meant rebuilding compensation
that already works.

**Where the loss actually is, measured.** It is NOT the comb: for
`y = x − g·w[n−D]`, `w = x + fb·w[n−D]`, the broadband power gain is
`(1 + a² − 2ab)/(1 − b²)` with `a = g+fb`, `b = fb`, which at full comes to
2.04 — the comb is **+3.1 dB**, louder, not quieter. The loss is the **choke**:
`gr = 1/(1 + depth·6·env)` is a pure attenuator with no makeup, so at full depth
it is roughly −9 dB on normal material, plus whatever the lowpass closing to
2.4 kHz takes off the top.

**So: give the choke makeup gain, and trim the lowpass.** A compressor should
reduce dynamic RANGE, not average level. Fixed makeup against a nominal level
(`1 + depth·6·NOMINAL`) keeps the braking on transients while leaving the
average where it was, and it is a constant — **not** a signal-derived tracker,
which is the auto-gain deleted this morning for breathing 3.65 dB after a step.
The lowpass's broadband loss gets a trim table measured at `prepare` across the
ANTITHRUST range, the same pattern the engines already use.

**Fix it as part of §1b, not before it** — the width rework replaces the comb,
so compensating this one first is wasted work. The compensation should follow
the house pattern already used for the engines: measure the wet path's broadband
level at `prepare` across the ANTITHRUST range and build a trim table, so it is
derived rather than guessed and cannot drift when the comb is retuned. **Not** a
signal-derived tracker — that is the auto-gain that was deleted for breathing.

The bench check this needs: sweep MIX 0 → 1 at several ANTITHRUST settings and
require the output level to stay within a stated tolerance, which is the test
that would have caught it.

## 1d. Screen and readout — SHIPPED in 260918.3 *(Peter, 2026-09-18)*

Four items, and the first two are the same element.

**(a) No explanations.** Clicking AUTOREFILL currently prints "AUTOREFILL ON ·
the tank stays full" / "…thrust burns fuel, the engine sags and misfires" in the
readout. It goes. The panel explains nothing — that is the whole aesthetic, and
it is the same instinct that keeps Escape Room cryptic.

**(b) Parameter values belong ON THE SCREEN, in blue holo.** What is there now
is `#hint` — a DOM text element in panel amber (`#ffb44a`), positioned at
`screen.y + screen.h + 10`, i.e. on the metal just below the chrome bezel, not
on the glass at all. That is exactly the inconsistency: it is HTML text lying on
a photograph, while everything else the instrument says is drawn inside the
tube. Move it into the CRT and render it through `holo()`, which already takes a
colour and already draws on its own transparent layer so the scan bands land on
the glyphs. Small and cheap — `holo()` exists because FUEL EMPTY needed it.

**(c) The starfield should tell you which engine you are on.** Peter's mapping,
and the engine names already carry it:

| engine | starfield |
|---|---|
| 1–6 IDLE BURN → WARP FOLD | speed ramps up, stars stay points |
| 7 HYPERDRIVE | **warp** — stars stretch into lines |
| 8 SUPERNOVA | **a star exploding from the centre** |

The existing field already draws a segment from each star's previous projected
position to its current one, so "stars become lines" is mostly a longer z-step
and a longer trail — the mechanism is there. SUPERNOVA wants a bright expanding
shell plus a flash, on a repeating cycle.

**(d) SPACE should show nebulae.** Pulsing translucent coloured star fogs whose
colour and shape follow the SPACE setting. It maps onto the four stages the knob
already crossfades — tape delay, hall, shimmer, harmonic tremolo — so each can
take its own hue and behaviour and the fog changes as the sound does. Large
radial gradients at low alpha, drifting and pulsing, composited with `lighter`.
The stage weights (`tapeAmt`, `hallAmt`, `shimAmt`, `tremAmt`) are already
computed in the engine and would need to ride along in the meter message.

## 2. Knob skin switch — orange set — DONE in 260918.1 *(this heading was stale)*

Both delivered knob sets are ingested and embedded (`knobs-chrome.png`,
`knobs-orange.png`). Chrome is the default. The page needs the toggle and the
choice needs to ride in the state blob (it is panel state, not a host
parameter).

## 3. Fuel burn calibration — SUPERSEDED by Peter's 5 s-per-notch spec, built in 260918.2

The equilibrium was tuned against a steady sine. Real playing is peakier, so the
tank may sit lower than intended. Worth a pass with a DI guitar take once there
is something to play through.

---

## 4. Release work — DONE 2026-09-19

Landing page, 12-page landscape manual, 15.3 MB zip, `app.json`, manifest entry and a
1200x794 preview all shipped (BrokildApps `1c9e4cb`). Verified 2026-09-26: the folder
`vst3-apps/battlestar-overdrive/` holds the manual, the zip, `app.json`, `img/` and
`index.html`. The original text follows as the record of what was outstanding.

Battlestar Overdrive is built, measured and installed, but it is **not shipped**
by the house definition: no manual, no landing page, no dist zip, no `app.json`
or manifest entry, and no preview. `RELEASE-CHECKLIST.md` in the BWFX tools
folder is the order of operations.

## 5. No backup repo — DONE 2026-09-19

`brokild-battlestar-overdrive`, private, created through the GitHub API with the token from
`git credential fill` (there is still no `gh` here) and verified from the remote own tree at
95 blobs. Verified again 2026-09-26: `origin` in `bBattlestarOverdrive` points at it.
The original text follows as the record.

Every other plugin in `b\` has a private GitHub repo, because `C:\Users\peter\b`
is in neither Dropbox nor OneDrive and **committing and pushing IS the backup**.
This tree has a local git repo and nothing else. One `gh repo create` away, if
`gh` were installed here — it is not, so it needs doing by hand.

## 6. Ideas raised and deliberately not built

- **Doubled, drifted engines** as an alternative width source. It sounds better
  than any mono-safe method and CPU is not the objection (the whole plugin is
  about 2.5 % of a core), but it partly vanishes when the track is summed.
  Offered; the mono-safe route was taken because a drive that disappears in mono
  is a liability on a record and Peter asked for studio-ready.
- **Lofi holographic visuals on the CRT**, waiting on the artist's footage. The
  architecture already suits it; the one real fork is embedded clips (a one-line
  addition) versus user-supplied files (needs a small native file-serving hook,
  perhaps twenty lines, better decided than retrofitted). WebView2 autoplays
  video only when muted, and everything must come through the plugin's own
  resource provider or `getImageData` taints the canvas and every pixel probe
  in `test/` stops working.


---

## SPACE PANTHER skin (2026-09-26, awaiting the art, then go)

A second PANEL skin from the band's Space Panther artwork, beside the chrome/orange knob sets.
The ChatGPT order is Downloads\BSO_Art\panther-skin-pack\PROMPT-FOR-CHATGPT.md (edit of the
shipped panel.png; livery repainted in teal/magenta/gold, the existing title kept and only
re-coloured, the panther as a worn airbrushed mural; every piece of hardware unchanged).

When panel-panther.png arrives:
- **Measure it against panel-geometry.json before trusting it** (	ools/find-features.ps1, composite-test.ps1).
  An image generator hits no exact coordinate. Where the hardware drifted, paste the ORIGINAL hardware
  (CRT bezel, knob scales + labels, fuel collars, button nut, screws) back from panel.png through a mask,
  so the geometry table stays the single source for both skins.
- **Re-cut the screen glass overlay from the new panel**, exactly to the screen rect (the 8 px overhang lesson).
- Plumbing: SET currently toggles knob sets only; make it cycle panel+knobs as skins
  (e.g. YELLOW/chrome, YELLOW/orange, PANTHER/chrome), persist via the existing {k:"skin"} message.
  Old projects saving chrome/orange must load unchanged.
- Bump the build id, re-shoot the product plates if the panther becomes a default anywhere.
## BAND-LOGO title (2026-09-26, trying it out)

Third option: the band's own logo (Downloads\BSO_Art\Logo.png, 9000x3600, transparent) replaces the
painted title. Mockups in Downloads\BSO_Art\logo-panel-mockups\ (A = one-line lockup on a black
nameplate, B = the stacked logo, which comes out too small in a 1220x168 band). A reads well; the
code-drawn plate does not match the photograph, so ChatGPT paints a BLANK plate (or paints the old title
out) and Claude composites the exact logo. The logo is never handed to the image generator.
Applies to either livery (yellow or panther): the title is an independent layer.
## SHIPPED in 260926.1: skin 2 (Space Panther) + the ghost transmission

- **Skin picked on the screen**: press the CRT, a terminal prompt types out `SELECT SKIN : 1 2`;
  click a number or type it, Esc / a click elsewhere / 8 s cancels. Skin 2 = ChatGPT's integrated-logo
  panther panel (`assets/decals/panel-panther.png`), with its OWN glass cut from it
  (`tools/ingest-skin2.ps1`). Measured before use: every pot, the CRT, tube and button align at 0,0.
- **Memory**: the project state carries `panelSkin` (a project saved before skins existed reopens on 1);
  the last pick is also written to `%APPDATA%\Brokild\Battlestar Overdrive.settings` and is what a
  FRESH instance opens with. Proven live: clean machine -> 1; pick 2 -> new instance opens 2; saved state
  beats a global default of 1.
- **Ghost transmission**: `art/ghost-1.jpg` (the press photo) drawn additively behind everything the
  tube says - flat backdrop keyed out, edges faded, 3 pre-blurred levels flicked between, 18 bands tearing
  sideways, cyan/magenta misregistration, snow, dropouts, vertical-hold slips. Strength = a slow wandering
  link + a clip follower on the output peak (fast in, slow out), so it firms up approaching clipping.
  A dry fuel tank loses the link.
- **Video later**: `GHOST.src` takes anything drawImage accepts; a `<video>` needs prepareGhost()
  re-run per frame at lower resolution, and the resource provider an `video/mp4` mime type.
- Build source moved to the repo copy; `install-fleet.ps1` now carries a `build` key for it.
## SHIPPED in 260926.2: Space Panther is the only panel

Peter: one skin, the panther; keep the original stored; no prompt. The panther now IS `assets/decals/panel.png` /
`art/panel.jpg` / `art/screen-glass.jpg`; the original panel and its own glass are in `assets/skins/original/`
(README there says how to put it back). Prompt and switching code removed - it is in 260926.1's history.
Manual re-shot (13 plates + a new `scr-ghost`), cover carries the band's real logo, headings yellow -> magenta,
landing page re-shot with a third row (idle / readout / transmission), zip + Brokild Collection re-cut and verified.

**Found on the way, NOT fixed (belongs to the collection tooling):** `BrokildWorldFX/tools/build-collection-pages.js`
copies its whole `<head>` from Black Rider's page, so every run rewrites the collection pages' canonical, og:url and
share image to BLACK RIDER's - undoing the hand-fixed heads. Reverted by hand this time (git checkout of the two pages).
The builder should keep each page's own head, or generate it.