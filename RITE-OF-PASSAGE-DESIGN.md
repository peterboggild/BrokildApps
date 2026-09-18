# RITE OF PASSAGE — design

*A Brokild transition processor. Written 2026-09-18 from a conversation with
Peter, before any code, in the house manner. Two readings of the effect menu
were taken and merged; where the second reading won, §16 says so.*

*STATUS: NO CODE. Nothing here has been measured. Every number in this
document is a TARGET, and the bench in §14 is the list of promises that have
to come true before the plugin is allowed to exist. The LEGION doc could say
"measured" because it was written after the engine; this one cannot, and
saying so is the point of writing it first.*

---

## 0. The one-line thesis

**A transition is not an effect. It is a journey with a landing, and the
landing is the part everyone remembers.**

Everything else follows. One slider is the journey. Six things happen along
it, each on its own schedule. And the moment of arrival is a separate,
deliberate act — not something that falls out of the slider reaching the end.

## 1. What it refuses

- **No twelve always-on effects.** Six SLOTS, any of twelve effects in each,
  duplicates allowed. §2.
- **No effect that is a colour rather than a gesture.** If freezing the slider
  at a fixed position leaves you still wanting it, it belongs in BWFX, which
  this plugin hosts anyway. A bitcrusher in a transition slot is paying twice.
- **No lockstep morph.** One slider moving forty knobs on the same curve is a
  macro, not a transition. §3.
- **No drop fired by scrubbing.** ARRIVAL is its own command. §4.
- **No dynamic panning as the stereo story.** §6.
- **No growing parameter list.** One automatable slider, one arrival trigger,
  four macros. Everything else lives in an opaque blob, the BWFX doctrine.
  §10.

## 2. Six slots

Six slots in series. Each holds one of the twelve effects of §5, or nothing.
The same effect may appear in more than one slot.

This is better than twelve fixed effects for four reasons, and the fourth is
the one that matters:

1. **Duplicates are musical.** Two CLIMBs at different enter points. A slow
   STUTTER early and a tight one late. Two TAPEs sweeping against each other.
2. **The slots ARE the chain order.** There is no reorder gesture to design,
   no drag-to-sort, no order stored separately. You assign, and where you
   assigned it is where it sits.
3. **Six lanes fit on a panel.** Twelve do not. With §3's enter/exit this
   makes the panel a six-lane score read left to right, which is the whole
   plugin legible at a glance.
4. **Six is enough and not more.** A complete build is filter, riser, stutter,
   bloom, cut. Five. The sixth is room to be clever; a seventh would be room
   to make mud.

**Order does not morph.** BWFX's `applyMorph` swaps chain order at mid-morph
through a click-free dip (`bwfx_rack.cpp:794`) — correct for a patch morph,
wrong here. A transition whose chain re-orders itself halfway is not a
transition, it is two of them. The slot layout is fixed for the whole rite,
and A and B differ only in values.

**Assignment is not a performance gesture.** Changing what is in a slot
allocates, which is message-thread work, and it clicks. It is locked while the
slider is moving.

**Generators sum in at their slot**, which turns out to be expressive rather
than awkward. RISER in slot 2 gets swept by a CLIMB in slot 4; RISER in slot 5
does not. GAP before BLOOM lets the reverb tail ring on through the silence;
GAP after BLOOM makes the silence total. The ordering of six things is the
second instrument in this plugin.

## 3. The slider is a score, not a crossfade

Each slot carries **A** and **B** — two complete settings — and three things
that decide the route between them:

- **ENTER / EXIT**: where on the slider's travel this slot starts and finishes
  moving. Filtering from 0 %; stutter only from 70 %; the tape sweep in the
  last 15 %. Outside its range a slot sits at A (before) or B (after).
- **CURVE**: gradual, accelerating, decelerating, S, or peak-in-the-middle.
- **DEPTH**: how far along the A→B route this slot actually travels. A slot at
  60 % depth is still on its way when the slider arrives, which is a different
  and often better sound than arriving early.

**Per-parameter curves, one curve per slot by default.** The default is one
curve for the whole slot, because twelve knobs with twelve curves is a
spreadsheet. But a parameter can be given its own, and the reason is concrete:
a tape delay whose feedback RISES while its time FALLS is one gesture that
needs two opposite routes. Same for a reverb whose input fades as its decay
grows. Without per-parameter curves those have to be two slots.

A parameter whose A equals its B does not move. That is the whole of the
"parameter lock" feature — a button that copies one value to the other side,
not a mechanism.

**Stepped parameters step on bar lines.** BWFX staggers stepped parameters at
pseudo-random thresholds so four choice-knobs do not all defect on the same
frame — right for a patch morph, wrong here. A STUTTER travelling ¼ → ⅛ → ¹⁄₁₆
has to change division *on a musical boundary* or it sounds broken. Stepped
parameters quantise their defection to the beat grid, and the grid is
settable per slot (bar / ½ / ¼).

## 4. ARRIVAL

The landing is a separate, automatable command. **HOLD B is what the slider
does; ARRIVAL is what you fire.**

This is the second reading's best point and it caught a real fault: if the
landing fires when the slider reaches 1.0, then scrubbing the slider while
editing fires a drop. Separating them costs one more automation lane and buys
an instrument you can touch without setting it off. For people who do not want
two lanes there is an opt-in **ARRIVE ON FULL** that fires once when the
slider reaches 1.0 travelling forward.

ARRIVAL is **sample-accurate and bar-aligned**. Given the host's ppq it
computes the exact sample offset of the next boundary inside the current
block and fires there. Landing "on the bar" to the nearest buffer is landing
half a beat late at 64 samples and nobody would ship it.

What ARRIVAL does, per slot, is one of three **tail modes**:

| mode | what happens |
|---|---|
| **BYPASS** | the slot stops processing immediately; whatever was in it is gone |
| **SPILL** | the slot stops being FED but keeps ringing — the reverb tail, the last echoes |
| **CLEAR** | the slot is silenced and its buffers are zeroed |

Those three sound entirely different and the distinction has to be in the UI,
not buried. A BLOOM set to SPILL is the classic wash spilling over the
downbeat; the same BLOOM set to CLEAR is the vacuum.

ARRIVAL also, optionally:

- **restores the dry signal** in one block (the default — this is the drop);
- **fires IMPACT** — a tuned sub with a noise transient. An impact does not
  need a slot: it happens once, at one instant, and that instant is exactly
  what ARRIVAL is;
- **releases the MONO GATE** (§6).

## 5. The twelve

| | Effect | The gesture | What A→B moves |
|---|---|---|---|
| 1 | **CLIMB** | the sweep; the spine of every build | cutoff, resonance |
| 2 | **TAPE** | head moves, pitch bends, feedback runs away into howl | time, feedback |
| 3 | **STUTTER** | capture and roll, tightening into a pitched buzz | division ¼ → ¹⁄₃₂ |
| 4 | **CHOP** | live-signal gating that accelerates | rate, duty |
| 5 | **GRAIN** | the music breaks into particles, then a cloud | grain size, density, scatter |
| 6 | **BLOOM** | a small room growing into an enormous wash | size, feedback, damping |
| 7 | **FREEZE** | the harmonic fingerprint held while the rhythm dissolves | blur, hold |
| 8 | **REVERSE** | fragments rushing backwards into their next boundary | slice length |
| 9 | **BRAKE** | the whole signal grinding to a halt, or launching | speed |
| 10 | **DIVE** | the source itself bending into the drop | semitones |
| 11 | **RISER** | a rush even when the source is sparse or silent | filter / pitch travel |
| 12 | **GAP** | the cut; the silence before the drop | depth, length |

Eleven processors and one generator (RISER). IMPACT is not here: it happens
once, at one instant, and that instant is ARRIVAL. The ones that need more
than a row:

**TAPE.** Changing the delay time must change the READ RATE, not crossfade
between two taps. The pitch bend *is* the effect — Ableton's Delay draws the
same distinction between Repitch and Fade and Repitch is the one that belongs
here. **MOTOR** is a second-order lag on the read rate with settable damping,
so a heavy motor overshoots and wobbles into place instead of arriving. Fade
mode exists too; it costs ten lines and is a different, useful sound.

**STUTTER vs CHOP** are separate because they are different in kind. STUTTER
captures a slice and repeats it, so time stops. CHOP gates the live signal, so
time continues. Roll versus chop; a build usually wants both, at different
points.

**RISER** absorbed the downlifter and the Shepard tone: SOURCE is
NOISE / TONE / SHEPARD and DIRECTION is up or down. It is the one effect that
works when the bar before the drop is nearly empty, which is when you need it
most.

**DIVE** is LEGION's engine with one voice and a moving ratio — pitch that
bends while the groove keeps its tempo, which is a different animal from
BRAKE's varispeed. Reusing it means the pitch quality is already measured
(§7).

**GAP** is the only effect whose job is absence, and it is placeable anywhere
on the score — a ¹⁄₈ hole at 85 % of the travel, not only at the end.

Reserve, if a slot frees up: **SWARM** (tuned resonators converging into a
chord — the most distinctive thing not in the twelve), then EROSION, JET,
WIDTH. Note that menu size now costs development time, not CPU or panel space:
only six instantiate. Twelve is a starting point, not a cap, and a thirteenth
effect later is purely additive and breaks no saved rite.

## 6. Stereo, and not by panning

Dynamic panning is the obvious answer and the weakest one. Five mechanisms,
none of which is a pan:

**1 · PLACE, per slot.** STEREO / MID / SIDE / LEFT / RIGHT. A CLIMB sweeping
only the SIDES narrows the music without gutting the centre — the vocal and
the kick stay put while everything around them closes in. A STUTTER on SIDE
only leaves the lead intact and rolls the room. This one control changes what
half the effects mean.

**2 · SPREAD, global.** The same gesture happens at slightly different TIMES
on each side: every slot's enter and exit are offset between L and R by
±SPREAD × a fixed per-slot constant. The two channels run the same score a few
percent apart, so the image opens *because* the transition is happening, not
because something was panned. At SPREAD 0 it is dead centre and mono-safe; at
SPREAD 1 the build tears itself apart horizontally.

**3 · FAN, on GRAIN and FREEZE.** Particles and partials are placed in the
field **by frequency** — lows held at the centre, highs scattered outward,
with the scatter growing along the slider. This is the thing panning cannot
do: a cloud that is wide in its treble and solid in its bass. It is also the
single most spectacular-sounding item in this document and it is nearly free,
because both effects already have the material split by frequency.

**4 · TURN, global, on the score.** A mid/side rotation matrix, not a pan. The
phantom image *turns* — energy preserving, both channels always occupied. A
pan moves a fader; a turn moves the room.

**5 · THE MONO GATE, at ARRIVAL.** Width collapses toward mono over the last
beat of the build and is released at the arrival. Nothing makes a drop sound
bigger for less work, and it costs no slot because it belongs to ARRIVAL.
Optionally it releases *past* unity for a moment and settles back.

**Bass is mono below a corner (default 120 Hz), always, with no switch.** And
mono compatibility is a measured contract, not a hope: §14 requires the
mono sum of any rite to lose no more than 3 dB against its stereo RMS at every
point on the travel. A transition that vanishes on a phone is not spectacular.

## 7. The standard the algorithms are held to

"Industry standard" is a claim, so here is what it means in code:

- **CLIMB is a zero-delay-feedback TPT state-variable filter**, not a
  direct-form biquad. This is the single determining choice in the plugin. A
  biquad swept fast at high resonance zippers, mistunes and can blow up; a TPT
  structure takes new coefficients every sample and stays stable to
  self-oscillation. Everything about how a build feels lives in this filter.
- **Every moving delay tap is read with cubic Catmull-Rom at minimum** — the
  house rule from BWFX, where linear was measured smearing the top octave.
  BRAKE's large speed changes need more than that: reading a line fast
  aliases, so it runs through the polyphase half-band cascade already in
  `bwfx_dsp.h`.
- **Nonlinearities are oversampled 4×**, same cascade. Saturation that
  aliases is the thing that makes a build sound cheap on the top end.
- **Never inject noise inside a feedback loop** (BWFX's inherited rule).
  TAPE's loop gets saturation and a band-limit so that feedback past unity
  *howls* musically instead of detonating.
- **FREEZE, GRAIN and DIVE reuse LEGION's spectral core** — the FFT, the true
  envelope, the peak-locked phase machinery. That code is already benched:
  −93.9 dB unity transparency, worst shifted-tone spur −39 dB, formants held
  within 0.3 st. It is a strong recommendation that
  `legion_fft / legion_analysis / legion_shifter` be promoted out of
  `vocal-harmonizer/` into a shared Brokild spectral core rather than copied.
- **GRAIN** uses equal-power windowed grains with dithered positions — evenly
  spaced grains comb-filter, and the comb is the sound of a cheap granular.
- **BLOOM** is an FDN with modulated delay lengths (a static FDN rings
  metallic), and its freeze must be **exactly** unity: a reverb that creeps
  0.2 dB per second is unusable, and it is measurable.
- **Control rate is the 32-sample sub-block** BWFX already uses, and the
  smoothing constants **scale with the transition length**. A fixed constant
  that suits a sixteen-bar build makes a one-bar build mushy, and the last
  100 ms of a build is the part the whole plugin exists for.
- `process()` allocates nothing, locks nothing, logs nothing. Denormals off.
  Internal float headroom, one safety limiter last, and nothing else clips.

And the part that is not DSP: **spectacular is contrast, not loudness.**
Twelve effects stacking resonance and saturation can put the build well over
the drop, which inverts the entire point. Per-slot auto-makeup, a global
output meter, and a deliberate **DUCK** over the last beat so the arrival has
somewhere to arrive from. Silence, width and level all change at the same
instant — that is what makes a drop land, and it is why GAP, the MONO GATE and
the tail modes are three parts of one idea.

## 8. Capture, tails, and going backwards

A transition plugin is unusual in that the slider can move *back*, and the
host can jump the playhead into the middle of a rite that never started.

- **Capture.** STUTTER, FREEZE and REVERSE all hold material. Each declares
  when it grabs (on entering its lane / on every bar / on ARRIVAL arm), and
  whether it refreshes. An uncaptured effect passes audio through unchanged.
- **Backwards.** Moving the slider back past a slot's ENTER releases that slot
  and, if it holds material, drops it. A rite scrubbed backwards should sound
  like nothing is happening, not like last night's build.
- **Playhead jumps** are detected from a ppq discontinuity: every slot resets,
  buffers clear, capture re-arms. No tail survives a jump.
- **Tails at t=0.** With every slot before its ENTER and A empty, the plugin
  is **bit-transparent** — the house contract, and the first thing §14
  measures.

## 9. Latency

**Two effects carry latency** — FREEZE and DIVE, both windowed. With
assignable slots the honest latency depends on what is loaded, and a plugin
whose latency changes when you assign a slot will fight the DAW's delay
compensation mid-session. So: **report the worst case always and pay it
always**, with a DETAIL-style window choice that sets what the worst case is.
It is a transition plugin; it is not for live monitoring, and pretending
otherwise would cost more than it buys.

## 10. The automatable surface, and nothing more

**One slider. One ARRIVAL trigger. Four macros.** Everything else — six slot
assignments, two full settings each, enter, exit, curve, depth, place, tail
mode — lives in one opaque string blob stored in plugin state and in the rite
file, keyed by slot index and effect id, unknown keys ignored.

This is BWFX's doctrine applied unchanged, and it solves a problem the second
reading raised without having to manage it: if slot parameters were host
parameters, re-assigning a slot would orphan every automation lane bound to
it. They are not host parameters, so there is nothing to orphan.

## 11. BWFX

The rack inserts as one stage, post-slots, pre-limiter, default empty and
therefore bit-transparent. Four calls, exactly as LEGION does it.

One addition specific to this plugin: **the five BWFX macros get their own
lanes on the score** — enter, exit, curve, the lot. That makes every World
module timeline-sequenceable without spending a slot, and it is the reason
EROSION, JET and WIDTH do not need to be in the twelve. TUBE and GRIT already
exist in BWFX; a transition that wants progressive destruction drives a macro
at them.

## 12. The rite

**The panel is a threshold, and the transition is something crossing it.**

The visual language is an **invented** one. That is a hard rule, not a
sensibility: nothing on this panel may be traceable to any real culture,
living or historical — no borrowed script, no borrowed symbol system, no
motif that belongs to someone. What it is built from instead is the set of
marks every human group has made independently: **tally strokes, circles,
chevrons, dots, notches, a doorway.** Abstract geometry and honest materials.
The rite is fictional and must stay unidentifiable, which is also the only way
it stays *ours*.

**Materials**: charred wood, fired clay, ochre pigment, ash, bone, waxed cord,
hammered dark iron. Nothing polished. Everything has been used.

**Palette** — warm, which sets it apart from High Tide's sea-ink and LEGION's
cold grey:

| | | |
|---|---|---|
| ground | `#141010` | charred wood, warm black |
| ochre red | `#b3452a` | dried pigment |
| ochre yellow | `#d39b3a` | |
| ash / bone | `#e6ddcd` | the lettering and the marks at rest |
| smoke | `#6b625a` | inactive, receded |
| **ember** | `#ff6a2a` | **heat — only ever on what is moving** |
| BWFX teal | `#35c9c0` | the globe, once, mandated |

**The one idea that makes it standout**: the panel **takes on heat as the
transition proceeds**. At rest everything is cold ash and smoke. As the slider
crosses the threshold each slot's mark ignites in turn, in its own lane, at
its own enter point — so the score visibly *catches*, left to right, in the
order you wrote it. At ARRIVAL: one frame of white, then everything goes cold
at once.

That is not decoration. It is the plugin's state rendered as the thing the
plugin is named after, and it is drawn entirely from data that exists anyway.

**The threshold** frames the main slider: two uprights and a lintel across the
top of the panel, the slider handle a bound marker travelling through the
gate. Six lanes below, notched like a tally, one per slot.

Decals are ordered through `assets/rite-decals/BRIEF.md` — twelve parts, the
house format, self-contained to paste in one message.

## 13. Presets

A **rite** is the whole thing: six assignments, twelve settings, the score,
the arrival. Named and shareable as one file.

The first one, and the spec for the first bench case, is **DISSOLVE → IGNITE**:
the bass leaves, the music fragments into grains, tape echoes accelerate, a
short stutter takes over, and everything clears on the downbeat. Five slots,
staggered enter points, an ARRIVAL with SPILL on the bloom. If that lands on
the bar and sounds like one gesture rather than five, the score model works.

## 14. What the bench will measure

None of this is measured yet. The bench is written first and fails first.

- **The contract**: every slot empty, or every slot sitting before its ENTER
  with A empty — output bit-identical to the input, sample for sample.
- **ARRIVAL lands on the bar** to the sample, at 120 / 128 / 174 BPM, at
  buffer sizes 64 / 256 / 1024, with the trigger arriving at every offset
  within the block. This is the single most important test in the file.
- **The score is obeyed**: a slot with ENTER 0.7 does nothing measurable
  before 0.7, and has fully arrived at EXIT.
- **Stepped parameters step on bar lines**, never between them.
- **Mono compatibility**: the mono sum loses no more than 3 dB against the
  stereo RMS at every point on the travel, for every preset, at every SPREAD.
- **No zipper**: a CLIMB swept 20 Hz → 18 kHz in 200 ms at full resonance
  stays bounded, finite, and free of steps above −60 dB.
- **BRAKE and TAPE do not alias**: a 1 kHz tone through a 2× speed change
  keeps its worst spur below −60 dB.
- **BLOOM's freeze is exactly unity**: frozen for 60 s, level drift under
  0.1 dB.
- **Backwards and jumps**: scrub the slider back, jump the playhead — no tail,
  no capture, no sound survives.
- **Bounded and finite** for all twelve effects at both extremes of every
  parameter, on music, on noise, on silence and on a scream.
- **Deterministic**: byte-identical at block size 64 and 256.
- **Cost**, six slots loaded, the worst six.

## 15. What is deferred

- **Parallel routing.** Serial only. Six slots in a line is already a
  combinatorial instrument.
- **Per-slot host automation.** §10. Four macros is the pressure valve.
- **MIDI trigger for ARRIVAL.** Obvious, easy, not v1.
- **A curve editor.** Five named curves, not a drawable one.
- **The reserve four effects.** §5.

## 16. Provenance

The six-slot architecture is Peter's, 2026-09-18, and it is better than the
twelve fixed effects it replaced.

A second reading of the effect menu contributed four things that are now load
bearing: **GRANULAR DISINTEGRATION**, which nothing else in the twelve does —
FREEZE holds a spectrum and STUTTER repeats a slice, but neither dissolves;
**ARRIVAL as a separate command**, which caught a real fault in the first
design, where scrubbing the slider fired a drop; **per-parameter curves**,
with the convincing example of feedback rising while time falls; and the
**three tail modes**, which are sharper than the single "release" they
replaced. Its framing — *effects that transform the music itself* — is also a
better selection rule than the "gesture, not colour" it replaced, because it
admits things that change what the material IS.

Where it was not followed: its twelve had no noise riser (demoted to its own
reserve list, though it is the one build device that works when the bar is
empty), no sound at the arrival, and it starred a distortion that BWFX already
ships twice. Its routing section described a reorderable serial chain, which
Peter's six slots supersede.
