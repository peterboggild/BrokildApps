# FULL METAL RACKET — buglist

Collected, not built. Nothing here is implemented until Peter says go.
Written against build 260827.22.

---

## 1. The transport should follow the DAW — FIXED 2026-08-30

Peter 2026-08-28:

> When DAW starts or stops the FMR should obey that. It should be possible to
> start/stop the FMR without the DAW. Space bar can stop and start the FMR both
> directly or via the DAW (space also starts stops Ableton).

**What it does today, and why it is wrong.** `GP_SEQ` is a single switch that
means "the sequencer plays". `runSequencer` returns immediately when it is off,
and when it is on it plays either way: bar-locked if the host is rolling, and
free-running at the host's tempo if the host is stopped. So stopping the DAW
does not stop the machine — which is exactly the report.

That behaviour was Peter's own earlier request ("'Running' should start it even
if daw is not running") and it is still wanted. What is missing is that a DAW
transport, once there IS one, should be in charge.

**The model to build.** Split the one switch into an intent and a gate:

| | |
|---|---|
| `seq` | ARMED. The user's intent. Stays an automatable, saved parameter, exactly as now. |
| free-run latch | Session-local, not a parameter, not saved. Set by the panel's ▶ when no host transport is present or rolling. |
| actually running | `armed && (host is rolling ‖ free-run latch)` |

Then:

* the DAW starts → it runs, if armed. The DAW stops → it stops, **and the
  free-run latch is cleared**, so it does not carry on by itself;
* with no DAW rolling, ▶ sets the latch and it plays on its own clock — the
  standalone and the auditioning case, unchanged;
* ⏹ clears the latch and calls `restartClock()`, as now.

**Do NOT let the host transport write `seq`.** It is an automatable parameter:
a transport that toggled it would push undo steps and fight any automation lane
on it. The gate is a separate runtime term, not a write.

Detail worth getting right: `playing` is currently `play && ppq >= 0.0`, so a
host that reports rolling without a position reads as stopped. Keep that, but
treat "host has a transport at all" and "host is rolling" as two different
questions — the first decides whether the latch is even relevant.

### SPACE, and why the fix is to stop handling it

The panel currently calls `preventDefault()` on SPACE and sends its own
transport message. That is why space does not start Ableton: **the plugin eats
the key and the host never sees it.**

With the gate above in place, the fix is to *stop being clever*: when a host
transport exists, let SPACE through. Ableton starts, FMR follows, and there is
one transport rather than two arguing. Only in the standalone — or in a host
that offers no transport at all — does the panel handle SPACE itself.

Caveats to check when building:

* whether the key even reaches the page depends on the host's "plugin gets
  keyboard focus" setting and on JUCE's `keyPressed`/`setWantsKeyboardFocus`;
  in Live a plugin window with focus swallows space unless the plugin declines
  it. Test in Live specifically, not only in the standalone;
* not while typing in the kit filter, and blur a focused button first — both
  already handled, keep them;
* if a host is present but reports no transport, fall back to handling SPACE
  ourselves rather than silently doing nothing.

---

## 2. Morph belongs to ONE patch, and must not step anything — FIXED 2026-08-30

Peter 2026-08-28:

> Morph function should work only within 1 patch… so when a patch is changed,
> both A and B clears. FX settings (i.e. which are on and which are off) should
> not be switched (as they will create discontinuities between A and B). Same
> with discontinuous choices (i.e. between type of kick) should always be
> according to what was set latest.

Two changes, and the second is a genuine reversal of a decision I made.

**2a. Clear A and B when the kit changes.** `applyKitIndex` already clears
`patchName` and restarts the clock; it should clear `haveA`/`haveB` too, and so
should loading a `.fmrkit` that carries no kits of its own. A morph between a
kit you are no longer on and one you never chose is not a morph, it is a
haunting.

**2b. Morph interpolates CONTINUOUS parameters only.** Today `applyMorph`
flips `KP_LIST` and `KP_SW` at staggered thresholds:

```cpp
const float th = 0.30f + 0.40f * (float) ((i * 37) % 100) / 100.0f;
d = t < th ? a : b;
```

The intent was that the kit should change *character* rather than cross-fade.
The cost is that a fader sweep contains a dozen invisible cliffs — a kick model
changing mid-note, a channel's whole circuit swapping under a held decay. Peter
is right: for a control you would automate across eight bars, that is a defect,
not a feature. Stepped parameters should stay **wherever they were last set**,
and only the continuous ones should move.

Implementation is a two-line deletion: drop the `KP_LIST`/`KP_SW` branch and
`continue` on those kinds. The panel's `morphView()` in `ui.html` computes the
same staggered thresholds for display and must lose them at the same time, or
the picture and the sound part company again.

**Compatibility.** No shipped content changes — the two hundred seed kits do
not carry A/B, and morph is only reachable after you have captured something in
this session. A `.fmrkit` saved with kits under the old rule will morph more
smoothly than it used to, which is the point.

**"FX settings … which are on and which are off"** — worth being precise about
which FX. FMR's own kit morph does not touch the BWFX rack at all today, and
should not start: `Rack::applyMorph` is a separate mechanism with its own
enable-envelope crossfades. What 2b fixes inside FMR is the per-channel MODEL
selectors and switches. If a rack morph is ever wanted here it is a separate
item, and the same rule should apply to it.

---

## 3. A patch can carry its morph — FIXED 2026-08-30

Peter 2026-08-28:

> Morphs should be saved with patch? DONT change any existing ones, but user
> patches can have morph settings — that ONLY applies to continuous knobs, not
> discrete settings.

**Already half true, and the half that is missing is small.** A `.fmrkit`
already stores `"kits"` (the packed A and B and their `have` flags) through
`kitsVar`/`kitsApply`, and so does the DAW project state. So:

* **user patches** — nothing to add beyond what 2a implies: a patch that
  carries kits loads them, a patch that does not carries clears them. Saving
  after 2b means the stored A and B are only ever *used* for continuous
  parameters, which is exactly the restriction asked for; the stepped values
  are still packed, harmlessly, and ignored on playback;
* **the two hundred seed kits** are generated in C++ and carry no A/B at all,
  so "don't change any existing ones" costs nothing — dialling a kit clears
  them, per 2a, and that is the whole of it.

One decision to make when building: should `pack()`/`unpack` stop storing
stepped parameters altogether? **No** — keep them. Storing a value nobody reads
is free, and a later change of mind about 2b would otherwise be unrecoverable
from files already saved.

No manual change needed (Peter said so explicitly), though the morph hint line
on the panel may want a word about the kit dial clearing the slots.

---

## 4. The sample layer — still not built

The one named feature from `FULL-METAL-RACKET-DESIGN.md` §3 that never got
made. Not a bug; recorded here so it stops being invisible.

---

## 5. PUNCH — a transient control. SHIPPED 2026-08-30

Peter 2026-08-29: "Can you build a simple transient controller button into
the FMR? Something to make it slam and punch through the mix when needed? Is
it consistent, and a good idea?"

**Yes, and it is a better idea here than in most plugins** — for one reason
that is specific to this machine.

A transient designer normally has to GUESS. It watches the audio, decides
where a hit started, and shapes an envelope around that estimate; every
artefact those things have — false triggers on a ride, pumping on a busy bar,
smearing when two drums land together — comes from the guess being wrong.

Full Metal Racket does not have to guess. It TRIGGERS the voices, and
`Voice::age` already counts samples since each hit. A transient control keyed
to that is exact, per voice, sample-accurate, and cannot false-trigger. Twelve
drums landing on the same step each get their own correct envelope, which no
plugin on the master bus can do.

That is the version worth building. A generic attack/sustain shaper bolted on
the output is not — BWFX already carries STRIP and TUBE, and a second
compressor-shaped thing on the master would be duplication.

### The shape

One global knob, `GP_PUNCH`, alongside RAIL SAG / KIT BODY / BLEED / AGE /
KIT TUNE. Per voice, per sample, from the age it already has:

```
    a   = exp(-age / tAtt)              // 1 at the hit, ~0 after tAtt
    g   = (1 + punch * kAtt * a) * (1 - punch * kSus * (1 - a))
```

The first bracket lifts the strike, the second leans on the body — which is
what "slam" is: a bigger front and a tighter back, so the hit occupies less
time and more of the moment it is in.

`tAtt` wants to be per family, not global: about 4 ms on a kick, 8 on a
snare, and the SUSTAIN half wants to be nearly off on the metal channels. A
cymbal that rings for four seconds does not want its body pulled down by a
control aimed at a kick — that reads as a broken decay rather than as punch.

### Is it consistent?

Three tests, and it passes all three, but only if built this way:

  * **exactly absent at zero.** `g = 1.0f` when punch is 0, an IEEE-exact
    multiply, the same contract RAIL SAG, BLEED, AGE and the BWFX macros
    keep. The bench must memcmp it, like the others;
  * **a mechanism, not an effect.** Keyed to the trigger it belongs to the
    machine's own thesis — the six mechanisms in `Engine.h` are all things a
    real kit does. Keyed to a detector it would just be a plugin that
    happens to live inside;
  * **it must not fight RAIL SAG.** Sag is the OPPOSITE gesture: the rail
    dips on a hard hit and everything sinks. Punch lifts the front of each
    hit. Used together they should read as "hits harder AND the room ducks",
    which is a real drum-bus sound — but the interaction needs measuring, not
    assuming, because both act on the first few milliseconds.

### A knob, not a button

He asked for a button. The other five globals are knobs, and a lone button in
that row would read as the odd one out — and "how much" is exactly the
question a transient control has to answer. It is automatable like everything
else in this plugin, so a host can flip it hard when a chorus lands, which is
the thing a button would have been for.

### The honest caveats

  * **It overlaps with SNAP, and the panel must not pretend otherwise.**
    SNAP shapes the excitation — it changes what the drum IS. PUNCH shapes
    the envelope after the fact — it changes how the drum SITS. Related, not
    the same, and if the note under them does not say so people will assume
    one of them is broken.
  * **It raises peaks into the limiters.** Each channel has its own CEILING
    and the master has `ceilSoft`. A transient boost pushed into a limiter is
    exactly how "slam" works in practice — the peak is caught and the average
    comes up — but that means PUNCH and CEILING are coupled by design and
    should be measured together, not discovered later.
  * **The marginal value is smaller here than on sampled drums.** SNAP,
    DECAY and DRIVE already give punch per channel; someone patient can get
    there today. What PUNCH buys is doing it to the whole kit at once,
    consistently, without editing twelve strips — which is worth a knob, but
    it is a convenience-plus-coherence argument, not a new capability.

### Cost

Small. One parameter in the table, one multiply in `renderVoice` from a value
the voice already has, one knob on the panel. The bench additions are the
interesting part: absent-at-zero by memcmp, the attack lift measured in dB
over the first few ms, the sustain cut measured on the tail, and a check that
the metal channels are not being shortened.


---

## As built, 2026-08-30 (items 1, 2, 3, 5)

Bench 1635 checks ALL CLEAR.

* **1** — `seq` is the ARMED intent (still automatable, still saved); a
  session-local `freeRun` latch is the gate; running = armed && (host
  rolling || latch). A DAW that stops drops the latch. The host transport
  never writes `seq`. The playhead was only read inside the `getBpm()`
  test, so a host reporting no tempo looked exactly like the standalone —
  'has a transport' and 'is rolling' are now separate questions. SPACE is
  no longer eaten when a host transport exists, which is why it never
  started Ableton. Measured: 0 onsets after the DAW stops; silent when
  armed with a present-but-stopped host until RUN raises the latch.
* **2** — 39 stepped parameters, 0 moved across a 21-point sweep. Dialling
  a kit clears A and B. The panel's `morphView()` lost the same branch in
  the same commit.
* **3** — `pack()`/`unpack()` now ask `morphable()` instead of `chan >= 0`,
  so the five kit globals travel with a saved morph. Old `.fmrkit` files
  have no globals to read and behave exactly as before.
* **5** — PUNCH: kick strike +2.06 dB with the body -2.26 dB, cymbal tail
  only -0.52 dB, bit-identical at zero, bounded with full RAIL SAG (peak
  0.595) and into the ceilings (0.915). First calibration gave +0.98 dB —
  too subtle to deserve a knob — so it was raised until the measurement
  said otherwise. PUNCH and SNAP tooltips now say which is which.

Still open: **4, the sample layer** — unspecced and much the largest.
