# PROXIMS CENTAURI B — ARTEFACT B2311.22
## Design document · commissioned 2026-08-29

Peter's brief, verbatim where it matters:

> Id like to create a synth that looks and feels like an alien artefact. Its
> design, the way the controls work, everything should be utterly alien.
> Ordinary sliders and knobs wont do […] The sounds should be as alien,
> foreign, outrageously weird as at all possible. The library and selection
> and sound generation should not just be pure noise, but offer a selection of
> sounds that dont resemble anything anyone has ever heard before. […] i am
> very interested in how you, as an AI trained on human input, will approach
> the task of coming up with something that dont resemble anything human. It
> should still be possible to operate by a human […] If it just looks like any
> other weird synth, like Escape Room, i will be a bit disappointed.

Name per instruction, twice-stated and taken verbatim: **Proxims Centauri B
· Artefact B2311.22** — recovered on a survey expedition to the nearest
potentially habitable world. ("Proxims" is Peter's spelling, kept exactly; if
it is a slip of the real star's name it is one keystroke to change before
anything ships — and if it stays, it reads as the expedition's own catalog
corruption, which suits the fiction.) DAW-facing product name
`Artefact B2311.22` (the full title lives on the panel and the landing
page). Plugin code `Ab22`, company Brokild, IS_SYNTH TRUE. Planned tree:
`C:\Users\peter\b\ArtefactB2311`.

---

## 0. How an AI approaches "nothing human" — the method, stated up front

This is the part of the commission Peter is most curious about, so it goes
first, and it is honest rather than grand.

**I cannot imagine like an alien.** Neither can any human — every "alien" in
circulation is a human genre with conventions (biomechanics, tentacles,
crystals, runes). An AI trained on the human corpus is, if anything, MORE
trapped in that library, not less. Any attempt to "picture the alien" returns
a catalogued style.

So the design never asks "what would an alien build?" It asks two answerable
questions instead:

1. **What does every human instrument assume?** Enumerate the assumptions —
   controls display their state, up means more, one knob moves one thing,
   sounds begin with an attack, modulation is periodic, timbre is the
   harmonic series, a library is a list of names. Then refuse each one **on
   principle, with a lawful replacement** — because refusal without law is
   just noise, and noise is the cheapest cop-out there is.

2. **What structures exist in mathematics that no earthly object has ever
   voiced?** Not invented gibberish — real, computable structure that has no
   acoustic realization on this planet, because nothing here is built in that
   shape. Sonify THAT, exactly, and the result is coherent (it is the sound
   of *something*) while resembling nothing (the something cannot exist
   here).

Peter sharpened the challenge mid-design (2026-08-29): *"you cannot do Giger
style, although I love it — this is a classic humanoid phantasy. You really
have to explore the negative space between human knowledge. Borderline
impossible for you — but maybe just borderline."* That phrase supplies the
operational definition the method needed:

3. **Negative space is not outside the human distribution — it is the
   low-density INTERIOR of it.** Nature and culture do not fill
   feature-space; they occupy thin manifolds. Materials come in bundles
   (wet↔soft, iridescent↔thin-film, granular↔dry), colors come with natural
   lighting statistics, forms carry either manufacturing logic (parting
   lines, fasteners) or developmental logic (symmetry, branching). Giger is
   a POINT on the manifold — catalogued the day it was painted. The gaps
   between the shelves are reachable **by construction**: take cues that
   each exist but never co-occur, and render them coherently anyway. A thing
   whose features are all familiar and whose combination has probability
   near zero is precisely the thing you cannot name. Section 9 turns this
   into pipeline rules, including the enforcement clause: *if you can name
   the material, the decal is rejected and reordered.*

The honest limit, admitted now rather than discovered later: moments of the
result will still catch on human references — a bell-ish instant, a vowel-ish
band. The design minimizes them, and the bench MEASURES the distance (§8): no
specimen ships whose spectrum sits near a harmonic series. Alienness here is
not asserted. It is enforced numerically. The same honesty applies to the
method itself: every move here is executed BY a system steeped in human
priors, aiming at the priors' gaps — asymptotically alien, never actually so.
What the pipeline guarantees structurally is narrower and real: **no step in
it ever selects a style**, so there is no step at which a shelf — Giger's or
any other — can be reached for.

---

## 1. The Escape Room bar — what must NOT happen

Escape Room's strangeness is skin: cryptic LABELS on ordinary knobs and
sliders, a terminal that prints values, noise cells into a filter. Underneath
it is a normal synth wearing a mask. B2311.22 shares nothing with it:

| Escape Room did | B2311.22 refuses it because |
|---|---|
| knobs and sliders, relabelled | the artefact has **organs**, not controls; nothing rotates, nothing slides |
| cryptic text (ALEPH, BETH) | **no text on the artefact at all** — cryptic labels are still labels |
| a terminal printing real values | the artefact never displays state; it displays **response** (§6) |
| noise sources → filter | no oscillators, no filters in the subtractive sense — a spectral organism (§3) |
| a preset number dial | no list; a dark **field** you search with a probe (§5) |
| rectangular panel of modules | one asymmetric object in a void; no grid, no privileged up |

---

## 2. The fiction that organizes everything

The plugin is two things laid over each other, and the split solves the
central contradiction of the brief (utterly alien, yet operable by a human
with a mouse):

* **THE ARTEFACT** — the recovered object. Alien end to end: no text, no
  numbers, no familiar control shapes, its own interaction grammar, its own
  agenda. It is never explained *by itself*.

* **THE HARNESS** — what the expedition bolted onto it: clipped-on sensor
  tags, a field journal, human annotations in a human hand. Toggleable, like
  Escape Room's legend, but diegetic — these are OUR sticky notes on THEIR
  object, visibly ours, visibly added later. Fine values, learnable names,
  and the manual-as-expedition-report all live on this layer.

Everything human about the product — host automation names, the manual, the
MIDI keyboard, the DAW itself — is framed as the harness. The artefact stays
clean. A researcher who tears the tags off (harness hidden) is left alone
with the object, which is exactly the experience the brief asks for.

---

## 3. The sound — spectral biology of impossible objects

**One paradigm, taken seriously,** rather than a grab-bag of effects.

### 3.1 A specimen is a graph, and its sound is the graph's spectrum

Every physical resonator on Earth — string, membrane, bell, pipe, room — is a
low-dimensional object, and its overtone structure is dictated by that
geometry (this is Weyl's law: mode counts follow dimension). The harmonic
series, and therefore human pitch and timbre, is a *geometric accident of
living in 3D space with strings and tubes*.

A **graph** — nodes and edges — is a resonator with no dimension at all. Its
vibrational modes are the eigenvectors of its Laplacian; its "overtone
series" is the eigenvalue spectrum. For trees, rings-with-chords, expanders,
bipartite and signed graphs, those spectra have structures **no buildable
object produces**: dense clustered bands (formant-like clumps that are
pitched and noisy at once), isolated pure outliers, and — for bipartite
specimens — spectra that are exactly mirror-symmetric about their centre,
which no earthly resonator has ever had.

So: a specimen is a seeded graph (30–96 nodes, topology family varying
across the catalog). At load, the engine diagonalizes its Laplacian (a
symmetric matrix; Jacobi iteration, milliseconds, message thread,
double-buffered to audio — the KIERANATOR/macro publication pattern). The
eigenvalues are the partial frequencies; the eigenvectors give each partial
its amplitude, its decay class, and (§3.5) its position in the stereo field.
The voice is an additive bank of 64–96 partials on rotation-recurrence
oscillators (~4 mul-adds per partial per sample; 8 voices ≈ well inside the
house CPU budget).

This is not FM-inharmonic or stretched-bell territory. It is the exact modal
answer of an object that cannot exist here.

### 3.2 Timbre is anatomy: energy migrates along the edges

In a real object, modes decay independently. In the artefact, energy **flows
between modes along the graph's own edges** (a slow coupling pass at control
rate — diffusion on the graph). Excite a leaf node and the sound percolates
inward over seconds; strike the hub and it floods out to the extremities.
What you hear over a held note is *the object exploring its own anatomy*. No
LFO, no envelope — the timbral motion IS the specimen's shape, which is why
every specimen moves differently and none of it resembles modulation.

### 3.3 Revival: the sound that reassembles

Human sounds disperse — they die outward and never come back; that is what
"decay" means to an ear evolved here. Quantum wave packets on certain
spectra do something no passive earthly object does: they disperse into
apparent noise and then **refocus** — full revivals, and at half and a third
of the revival time, *fractional revivals*: ghost copies of the original
sound reassembling briefly before dissolving again.

This is trivially synthesizable and profoundly unheard: snap the eigenvalues
toward a rotor grid (λ ∝ k(k+1)) and the partial phases realign periodically
— the sound un-rings. One organ (the researchers call it REVIVAL) morphs the
spectrum continuously between fully dispersive (raw irrational eigenvalues,
never returns) and commensurate (dies, then reassembles, with fractional
ghosts on the way). The bench measures the reassembly time against the
prediction.

### 3.4 No onsets: apertures onto processes that were already running

A note does not start a sound. The specimen's modes **run continuously,
phase never reset**; a key press opens an *aperture* onto the running
process (a raised-cosine gate, width = the APERTURE organ). Play the same
key twice and you catch the process at different moments — never identical,
yet bit-for-bit deterministic for a given MIDI sequence, so offline renders
and the bench still reproduce exactly. Human instruments are struck, plucked,
blown — evented. This one is *tuned into*, like a transmission that was
already there.

### 3.5 The stereo field is not a stage

Each partial takes its left/right amplitude and phase from the eigenvector
sampled at **two different nodes** — the two ears listen at two points on
the graph's body. There is no panning and no stage: every mode has its own
placement and the image reorganizes as energy migrates (§3.2). Cheap (two
amplitude sets), and it produces space that does not behave like sources in
a room.

### 3.6 The artefact warms to you: lawful hysteresis

Modes that were recently excited open slightly more easily (a slow, seeded,
deterministic bias with a fixed relaxation law — state is a pure function of
note history, so determinism and the bench survive). Repetition is answered
with familiarity; a phrase played twice sits differently the second time.
Materials with memory exist on Earth; instruments with memory do not. A
RESET gesture (and patch load) clears it.

### 3.7 Substrate

Beneath the modes, at low level and gated by the WAKE organ: deterministic
chaotic textures (iterated maps) pushed *through the specimen's own mode
bank* — the planet's atmosphere carried inside the object. At WAKE = 0 the
artefact is exactly inert (IEEE-exact, memcmp'd — the house contract).

### 3.8 Tuning: the specimen's own scale

MIDI works normally in COMPLIANT tuning (12-TET transposition of the whole
spectrum — playable in any track). In SIDEREAL tuning the keyboard walks the
specimen's **intrinsic eigen-intervals** — every specimen carries its own
scale, derived from the same spectrum you are hearing, so melody and timbre
stop being separate ideas. A switch on the harness, because switching
tunings is a human act.

---

## 4. What is deliberately refused

No oscillator section, no filter section, no ADSR, no LFO. No knobs, no
sliders, no switches on the artefact. No text, numbers, or meters on the
artefact. No preset list, no names. No reverb-as-alienness (a drone in a big
hall is a human cliché). No randomness-as-alienness — everything is seeded,
lawful, learnable, and the bench proves determinism. No Giger, no bones, no
tentacles, no greebles (§9 steers the decal generator away hard).

---

## 5. The catalog: a field you search, not a list you scroll

~300 specimens, generated deterministically from catalog numbers (the house
seed tradition: same number, same being, forever — bench-enforced all
distinct, §8).

Selection is **search**: a dark viewport — the survey field. Specimens sit
in it as faint bioluminescent sigils, positioned by *actual spectral
similarity* (a fixed, deterministic projection of each spectrum's feature
vector — neighbours genuinely sound related, distance is honest). Moving the
probe (cursor) toward one fades its transmission up — audition-as-approach,
like a hydrophone in dark water. Click collects it into the artefact.

**Sigils are synesthetically honest**: each specimen's mark is drawn FROM its
spectrum and graph (mode positions → strokes; band structure → density), so
the sigil is the sound written differently, not a decoration. A researcher
learns "the three-lobed one that reassembles" — recognition without names.
The harness (§6) can pin human tags on collected specimens; the field itself
never shows text.

---

## 6. The interface: organs, attention, gesture

### 6.1 The body

One asymmetric object, drawn large, in a void — no panel, no grid, no
privileged up (up-means-more is a human metaphor from piles of things; the
artefact's organs are arranged by its own anatomy, radially and unevenly).
Regions of it are **organs**: visibly differentiated tissue — a pore field, a
membrane, a seam, a cluster of filaments. ChatGPT decals supply the
*materials* (§9); geometry and all motion are canvas/SVG, per the house rule
that a generator cannot hit coordinates.

### 6.2 It attends before it responds

The artefact notices the cursor the way deep-sea life notices light: the
nearest organ orients toward it, brightens faintly, opens slightly on dwell.
State is never displayed — **response is displayed**. You learn what an organ
does the way you learn an animal: by watching how it reacts to you, and it
always reacts the same lawful way (Escape Room's one good lesson kept:
deterministic = learnable).

### 6.3 The gesture grammar

A mouse carries more than position — path, curvature, speed, dwell, rhythm —
and human interfaces throw all of it away. The artefact reads it. On any
organ:

* **stir** (circular strokes) — accumulate: sustained circular motion winds a
  quantity up or down; direction matters; it holds where you leave it;
* **stroke** (straight, fast) — perturb: a transient push the organ then
  relaxes from, at the specimen's own relaxation rate;
* **press** (dwell, motionless) — the organ yields under pressure, deepening
  the longer you hold;
* **rhythm** (repeated taps) — the one entrainable organ locks its internal
  motion to the rhythm you tap into it.

Four verbs, consistent across every organ — a grammar, not a puzzle. All are
single-mouse operations; none is a knob in disguise (no organ maps a linear
pixel distance to a value — the mapping runs through accumulation, yielding
or relaxation, so working the artefact feels like handling something with
its own material response).

### 6.4 The internal economy

Inside the engine, each voice's spectral energy is **conserved**: pushing a
specimen brighter takes body; opening the aperture wider thins the density.
Human engineering decouples; organisms trade. Crucially this law lives in the
DSP, not across host parameters — automation lanes stay orthogonal and
host-sane (§7), while the *sound* always behaves like a creature with a
metabolism, not a rack with sections.

### 6.5 The harness layer

Toggle: thin human hardware clipped onto the object — sensor tags wired to
small readouts, journal notes in a human hand, the tuning switch, fine
numeric entry for the obsessive. Visually unmistakable as OUR additions.
This is where usability lives, where the values are, and where nothing alien
is compromised, because it is explicitly the human layer.

### 6.6 WAKE

Untouched, at WAKE > 0, the artefact lives a little — organs drift on the
slow chaotic substrate, deterministically from the seed. At WAKE = 0 it is a
dead object and renders bit-identically. Automation-safe, bench-checked.

---

## 7. Host integration — the sanity contract

However alien the surface, underneath it is a disciplined Brokild VST3:

* native-first, APVTS single source of truth, SPECS[]-style table (the
  Hairfryer pattern) — the read-order bug stays inexpressible;
* host parameters carry **harness names** (the researchers' terms), because
  automation is a human instrument: SPECIMEN (catalog number, saved, not
  automatable — the mood-organ precedent), APERTURE, METABOLISM, REVIVAL,
  DEPTH, MEMBRANE, GRAVITY, WAKE, tuning switch, master. ~30–40 params
  total; every continuous one automatable;
* full MIDI: poly (6–8 voices), sustain, pitch bend (bends the whole
  spectrum, not a root — there is no root);
* BWFX world rack behind the standard globe (the harness explains it as the
  expedition's signal chain); world-mod bus mapped per-partial — SPECTRA
  characters possessing an alien organism is exactly what the bus was for;
* patches to `Documents\Brokild patches\Artefact B2311.22`; deterministic
  state; offline render identical to realtime.

---

## 8. The bench — alienness measured, not asserted

`test/` in the house style, plain C++, no JUCE. Beyond the standard slate
(bounded, silence-in-silence-out, all rates, determinism incl. the
hysteresis-from-history rule, WAKE=0 and empty-state memcmp, every specimen
distinct, CPU budget):

* **eigen-solver correctness**: residuals ‖Av − λv‖ under tolerance for
  every specimen; degenerate eigenvalues handled;
* **the non-harmonicity floor**: for every shipped specimen, the best-fit
  harmonic series over the audible partials must miss by a stated margin
  (cents-RMS). Any specimen that drifts near a harmonic stack is regenerated
  at catalog-build time. *No specimen resembling an earthly overtone series
  ships* — the brief's central claim, enforced by a number;
* **revival timing**: at full REVIVAL, autocorrelation reassembly measured
  against the predicted rotor period; fractional revivals present at T/2;
* **anatomy audible**: exciting hub vs leaf of the same specimen must differ
  by a stated spectral-flux margin (the coupling engine provably does what
  §3.2 claims);
* **conservation**: per-voice spectral energy constant under organ movement
  within tolerance;
* **field honesty**: embedding distance vs actual spectral distance rank
  correlation above a floor — the search field may not lie.

---

## 9. Graphics — ordered from ChatGPT, after geometry lock

Same pipeline as Clone Wars / FMR (BRIEF.md with isolated parts + DELIVERED
crop manifests; stretch-to-aspect ingest; never a registered faceplate).
Division of labour, revised per §0.3 — the earlier draft's direction
("deep-sea + mineral + iridescent") was itself a reach for a shelf, and is
withdrawn:

* **FORM IS COMPUTED, NEVER DRAWN.** The body on screen is the specimen's
  own spectral geometry (§11.2) rendered by fixed rules — no one, human or
  AI or image model, ever chooses a shape. Decals supply only surface
  micro-texture, applied to generated geometry. A style cannot enter
  through a door that does not exist.
* **Materials must refuse to resolve.** The brief demands violated bundles —
  cues that never co-occur in nature rendered in one surface: subsurface
  glow WITH brushed anisotropy WITH granular self-shadowing; wet-reading and
  dry-reading at once; mineral and tissue cues simultaneously. Heavy
  negative prompting, because image models regress to their attractors:
  **no Giger, no bones, no tentacles, no chitin, no insectoid plates, no
  chrome, no coral, no crystal, no flesh, no sci-fi greebles, no glyphs**
  (glyphs are human writing pretending otherwise) — banned as words and as
  looks. Enforcement at ingest, not at ordering: **if a delivered texture
  can be NAMED, it is rejected and reordered.** The nameability test is the
  acceptance test.
* **Color sits off the natural-statistics manifold.** Palettes built on
  equiluminant hue boundaries — edges the eye's luminance channel cannot
  see, so contours shimmer and refuse to lock. Human design avoids them
  because they destabilize human vision; that rule exists for our eyes'
  convenience, which is exactly why an object not made for our eyes breaks
  it. Costs nothing: this is palette discipline in the renderer, not an
  asset.

Parts list (finalized when geometry locks): material micro-textures for
tissue states (resting / attending / condensing / evaporating), the void
backdrop, and the HARNESS hardware — clips, tags, cabling, journal scraps —
which, pointedly, IS allowed to look human, manufactured and nameable,
because it is ours. Peter runs the generation, as before.

---

## 10. Build order

1. Engine core + specimen generator + eigen pipeline; bench slate incl.
   non-harmonicity floor (the sound must earn the brief before any UI);
2. coupling, revival, aperture, hysteresis, substrate, binaural field —
   each landing with its measurement;
3. catalog build + embedding + sigil generator; field honesty check;
4. panel: body, organs, attention, gesture grammar; geometry lock;
5. decal BRIEF → Peter runs ChatGPT → ingest;
6. harness layer, manual-as-expedition-report, landing page, zip, homepage.

---

## 11. Addendum, same day — THE SECTION: an instrument larger than its interface

Peter, 2026-08-29:

> another thing that could be interesting is if the interface changes
> depending on what the user does. I.e. as if the very synth is adjustable.
> Think beyond merely different frontplate […] like this thing exists in
> multiple dimensions, and we just see the cross section of the fourth
> dimensional object with our three dimensional universe, and can only
> operate whatever exists in that — in this case — two dimensional projection
> of a three dimensional cross-section of a fourth dimensional musical
> instrument […] Think about how that could be materialised, and still be a
> useful instrument for humans (if not understandable or controllable).

This is the strongest refusal in the whole design, so it gets promoted to a
first-class principle rather than a feature. The deepest assumption in every
human interface is not knobs or labels — it is **total presence**: the
instrument is all there, all the time, fully inventoried. B2311.22 refuses
it. You never see the instrument. You see where it currently intersects you.

### 11.1 What makes it lawful instead of a shuffling UI

The anti-pattern is explicit: pages, modes, skins, or a panel that
rearranges itself are all HUMAN devices (a frontplate that changes is still
a frontplate). A cross-section is different because it obeys geometry, and
geometry is learnable:

* **continuity** — nothing teleports. Organs bloom from a point, grow,
  shrink, vanish to a point, exactly as a 4D body's slices do;
* **conservation** — it is ONE persistent object. What left the slice still
  exists and can return; the journal (§11.6) even documents organs "the
  previous shift saw" that are currently outside the section;
* **merge and split for free** — a single connected 4D body can intersect
  the slice as several disconnected pieces. Two organs drifting together
  and FUSING — their functions combining — is not a designed feature, it is
  what sections do.

### 11.2 The implementation is already in the mathematics — this is the point

The specimen is a graph, and a graph's Laplacian eigenvectors give a
canonical **spectral embedding**: coordinates for every node, intrinsic to
the specimen, no art direction involved. Take eigenvectors 2–5 as the four
spatial coordinates and the whole design closes like a loop:

**The artefact's four spatial dimensions ARE its first four vibrational
modes.** Body-space and sound-space are the same space. Which yields, with
no additional machinery:

* **playing it moves it** — a note excites modes; mode excitation IS
  displacement along those axes. Play low and dark and the body drifts one
  way through the section plane; play sparse and high and it rotates
  another; go silent and it relaxes (or holds — hysteresis, §3.6). The
  interface changes *depending on what the user does* — Peter's exact
  request — through the physics of the object, not through UI logic;
* **where the slice sits is a timbre** — modes are weighted by their
  distance from the section slab, so the reachable sound IS the visible
  anatomy. Navigating the object and navigating its sound are one act.
  Crossing a dense eigen-cluster is passing through a gland of formants;
* **the catalog gains a second meaning** — every specimen is a different 4D
  body with different geography to travel, not just a different spectrum.

Concretely, v1 keeps two section degrees of freedom: a translation along
dimension four and one rotation in the 3–4 plane. Both are **deterministic
functions of note history** (leaky integrals of per-mode excitation) plus
the seeded WAKE drift. Same MIDI in, same journey, same sound — bit-exact,
bench-checked. Visible set = nodes inside the slab after rotation; organs =
connected components of the visible subgraph, recomputed at control rate
(N ≤ 96 nodes: trivial).

### 11.3 Materializing 2D-of-3D-of-4D on a screen

The chain of projections Peter specifies is rendered as a chain:

* the 3D cross-section is drawn **as a volume** — depth shading, occlusion,
  a slow breathing parallax — so the screen reads as a window onto a solid,
  not a diagram;
* the fourth dimension is rendered as **material phase, not direction**
  (there is no direction to point). Organs entering the slice *condense* —
  matter arriving from nowhere visible; organs leaving *evaporate*.
  Iridescence keys to w-velocity: tissue moving through the section shimmers
  differently from tissue resting in it;
* **approach ghosts** — organs nearing the slab cast a faint refracted
  pre-image before they arrive, like a fish rising toward the underside of
  the water's surface. This is the learnability concession, and it is
  diegetically clean: the surface of the water is the slice.

### 11.4 Useful for humans, exactly as far as Peter allows

"Useful (if not understandable or controllable)" — the split of §2 carries
all of it:

* **harness taps are bolted to the LAB FRAME, not to the artefact.** The
  host parameters (APERTURE, METABOLISM, REVIVAL…) are field quantities
  applied to *whatever tissue is currently in the slice*. Automation never
  chases a moving organ; lanes stay orthogonal and sane while the thing
  they act on lives;
* **steering, not setting** — the player influences the journey by HOW they
  play, like riding an animal. And because humans are humans, the
  expedition bolted a **winch** onto it: one harness control (automatable,
  saved) that drags the section toward a stored position. Recall for a mix
  session, without pretending the artefact itself is controllable;
* **a patch is a photograph** — patches store the full section state, so a
  saved sound is a saved *place on the body*, and loading one returns
  there;
* **grip pins locally** — the organ under active manipulation is held while
  gripped (the section keeps moving elsewhere). Your organ cannot evaporate
  out from under a gesture in progress: the one frustration that would kill
  the instrument, removed by a rule that reads as holding one limb of a
  moving creature;
* **tempo of the motion** — metabolic, seconds to tens of seconds, never
  flicker. The object travels like weather, not like a screensaver.

### 11.5 Bench additions

Section trajectory bit-deterministic from MIDI history at all rates;
continuity bound (visible set changes only by slab-boundary crossings — no
organ ever appears except by growth from the boundary); pinned-organ
invariant under section motion; winch recall lands exactly; slice position
audibly matters (same note, two section states, spectral distance above a
floor — the §11.2 claim, measured); WAKE = 0 and no-notes ⇒ section static
and render bit-identical.

### 11.6 The fiction earns it

The expedition's journal entry that frames the whole feature: *the
artefact's response mass is inconsistent with its visible volume* — it is
larger than its extent, and the parts take turns existing here. The manual
(expedition report) documents organs observed on previous shifts that
cannot currently be found, with sketches. Researchers name places on the
body the way sailors name coasts; the winch log records the places worth
returning to.

---

*Status: design committed (incl. §11), awaiting Peter's go. Nothing built.*
