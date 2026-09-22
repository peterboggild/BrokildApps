# RITE OF PASSAGE — what the controls are, and how to use it

*Written from the code as built (build 260918.2), not from the design. Pictures
of the real panel, rendered by `ropshot`: `docs/panel-empty.png`,
`docs/panel-loaded.png`, `docs/panel-midway.png`, `docs/panel-flat.png`.*

## What it is, in one paragraph

An insert effect for the bar before a drop. You put it on a track (or a bus),
load up to six effects into six **slots**, and tell each slot *where* along one
master slider it should travel from its **A** setting to its **B** setting.
Then you automate that one slider — **POSITION** — from 0 to 100 % over the
build. The slots ignite one after another as the slider passes their entry
points. At the end you fire **ARRIVAL**, a separate command, and every slot
lets go on the next bar line, to the sample, with a sub-bass impact under it.
Scrubbing the slider never fires the drop; only ARRIVAL does.

## READ THIS FIRST: why a new slot seems to do nothing

**A slot you have just filled cannot move, and that is deliberate.** Choosing
an effect from a lane's menu seeds *both* sides of that slot — A and B — with
the effect's own default settings. A equals B, so the slot resolves to the same
value at every position on the slider, and **ENTER, EXIT, DEPTH and CURVE all
do nothing until you change something in the B row.** This is on purpose:
loading an effect never changes your sound until you ask it to.

The panel now says so rather than leaving you to work it out. A slot that
cannot travel has its span **hatched out** on the lane, its **name in amber**
instead of ash, and a line under the A/B editor telling you to move a B knob.

So the first move after choosing an effect is always the same: **click the
lane, then change a knob in the bottom (B) row.**

## AUTO TRANSITION: let the plugin drive the slider

The strip under POSITION runs the sweep for you, off the host transport, so
you do not have to draw an automation lane at all.

| control | what it does |
|---|---|
| **AUTO** | on or off. While it is ON the plugin owns POSITION and the host parameter is ignored, so the slider is disabled and becomes a display. |
| **BARS** | the length of the cycle: 1, 2, 4, 8 or 16 bars. It repeats for ever. |
| **START** / **END** | where in that cycle the sweep runs. They are BAR LINES counted the way a DAW counts them, so in an 8-bar cycle the 8th bar is 8.00 to 9.00. They snap to quarter bars. |
| **0 to 100 / 100 to 0** | which way the sweep runs. |
| **then RESET / then HOLD** | what the position does between the end of the window and the end of the cycle. RESET drops it straight back to the start value; HOLD keeps it at the end value until the cycle wraps. RESET is the default. |
| **ARRIVE** | fire ARRIVAL when the sweep completes. Off by default, because an automatic drop is a bigger thing than an automatic sweep. |

Before the window the position sits at the start value. **After the window it
drops straight back**, which is what `then RESET` means and why it is the
default: put the window early in a long cycle and `then HOLD` leaves you at
100 % for several bars, which reads exactly like the sweep having run once and
stopped. `then HOLD` is still there for when you want the arrival to stay
landed until the cycle comes round. With no transport running it waits, and
the readout says so.

Measured, on an 8-bar cycle with the window set to the 8th bar:

| where the transport is | position |
|---|---|
| bar 4 | 0.00 |
| bar 8.0 | 0.00 |
| bar 8.5 | 0.50 |
| bar 9.0 | 0.99 |

**With AUTO off nothing changes at all** - the host parameter drives POSITION
exactly as it always did, which is checked so that no existing project moves.

The sweep repeats for as long as the transport rolls: measured over three
continuous 2-bar cycles it rises three times and returns to 0.00 each time.

## MIX GATE: the effects only while the slider moves

Most of the time you want the plugin present during the transition and absent
either side of it. The MIX GATE row does that without a second automation
lane - it scales MIX by where POSITION is.

| control | what it does |
|---|---|
| **FADE IN** | shuts the effects off while POSITION is at 0. |
| its length | how much of the travel the fade takes. At **0 the switch is instant** (the readout says `instant`): silent at exactly 0 %, fully open one step above it. At 20 % it is half open at 10 % and finished by 20 %. |
| **FADE OUT** | the same at the other end - shut at 100 %, and the length is measured back from there. |

Turn both on and the plugin is only there while the slider is moving; turn
both off and **MIX is exactly the number on the knob**, which is checked to be
an IEEE-exact multiply by one, so no existing project moves. There is no click
risk at a zero-length gate - the rack already smooths MIX with a 10 ms
one-pole.

Measured: a 20 % fade in gives 0.50 at 10 % and 1.00 from 20 % on; a 25 % fade
out gives 0.50 at 87.5 % and 0.00 at 100 %; both on gives 0.00 / 1.00 / 0.00 at
0 / 50 / 100 %.

## Signal path

    in -> slot 1 -> slot 2 -> ... -> slot 6 -> stereo stage (SPREAD / TURN / MONO GATE)
       -> MIX (dry/wet) -> OUTPUT -> BWFX rack (post-chain) -> out

Slots are in series, top to bottom. Slot order is chain order.

## The host parameters (what a DAW can automate)

Only these are host parameters. Everything else is stored in the plugin state
("the rite") and is not automatable — on purpose, so re-assigning a slot never
orphans an automation lane.

| control | range | what it does |
|---|---|---|
| **POSITION** | 0–100 % | the master slider: where along the build you are. **Automate this.** |
| **ARRIVAL** | off/on | a trigger: on its rising edge the plugin arms, and fires at the next grid boundary (default: the next bar). It stays "on" after firing; set it back to off before the next build so the next rising edge can arm again. The rack re-arms all slots by itself once POSITION drops below 2 %. |
| **MIX** | 0–100 % | global dry/wet across the whole chain (default 100 %) |
| **OUTPUT** | −24…+12 dB | output trim |
| **SPREAD** | 0–100 % | the left and right channels run the score slightly apart, so the two sides of the transition arrive at different moments — width without panning |
| **TURN** | −100…+100 % | rotates the stereo field, up to ±45°, energy preserving |
| **MONO GATE** | 0–100 % | how far toward mono the mix collapses over the LAST 15 % of the travel; ARRIVAL releases it back to full width |
| **BWFX MACRO 1–5** | | the World FX rack's five macros. The **BWFX** button in the header opens the rack's face. |

## Each slot (one lane on the panel)

Left to right, and the panel now prints these as column headings:

| control | what it does |
|---|---|
| **tick box** | slot on/off |
| **socket** | the effect's own mark, struck into iron. It warms to ember as the lane catches. |
| **EFFECT** | which effect the slot holds ("EMPTY" = none; duplicates are allowed). The eighteen are the list; **BROKILD WORLD FX** at the bottom opens a submenu with ten of the World rack's modules. **Amber means the slot cannot travel: A still equals B.** |
| **ENTER** | the POSITION at which this slot starts moving from A toward B |
| **EXIT** | the POSITION at which it reaches B. Before ENTER the slot sits at A; after EXIT it holds at B. |
| **DEPTH** | how far along the A→B route it actually gets by EXIT. 60 % means it is still on its way when the drop lands, which is often the better sound. |
| **PLACE** (ST / MID / SIDE / L / R) | which part of the stereo field the slot works on |
| **ON ARRIVAL** (STOP / SPILL / CLEAR) | what ARRIVAL does to this slot: STOP kills it dead; SPILL stops feeding it but lets its tail ring on over the downbeat; CLEAR silences it and zeroes its buffers (the vacuum) |
| **CURVE** (LIN / ACCEL / DECEL / S / PEAK) | the shape of the A→B travel: linear; late then a rush; fast then settling; S-curve; out and back (PEAK reaches B in the middle and returns to A) |

The lane's band shows its ENTER–EXIT span and turns to ember as the marker
crosses it. Click a lane to select it; the **A / B editor** below shows that
slot's parameters, top row **A** (yellow), bottom row **B** (ember). A
parameter whose A equals its B does not move.

## The eighteen effects

| effect | what it is | parameters (A and B each) | level | pitch |
|---|---|---|---|---|
| **CLIMB** | resonant filter sweep to self-oscillation — the spine of a build | MODE LP/BP/HP · CUTOFF · RESO · DRIVE | spectral | exact |
| **TAPE** | analogue echo whose head moves: changing TIME bends the pitch (REPITCH) or crossfades (FADE) | TIME · FEEDBACK · MOTOR · MODE · DRIVE · WOW · MIX | neutral | moves in REPITCH |
| **STUTTER** | captures a slice and repeats it, so time stops | DIV · DEPTH · DECAY · PITCH · CAPTURE | neutral | exact at PITCH 0 |
| **CHOP** | gates the LIVE signal rhythmically, so time continues | RATE · DEPTH · DUTY · SLEW | neutral | exact |
| **RISER** | a generator: noise, tone or a Shepard climb, mixed IN | SOURCE · FREQ · RESO · LEVEL · WIDTH | intentional | moves |
| **GAP** | the silence before the drop, placeable anywhere on the score | DEPTH · EDGE | intentional | exact |
| **GRAIN** | the music breaks into particles, then a cloud | SIZE · DENSITY · SCATTER · SPREAD · MIX | neutral, computed | exact at SPREAD 0 |
| **BLOOM** | a small room growing into an enormous wash; FREEZE at 100 % holds it for ever | SIZE · DECAY · DAMP · MOD · FREEZE · MIX | neutral, measured | exact |
| **FREEZE** | the harmonic fingerprint held while the rhythm dissolves | BLUR · HOLD · CAPTURE REFRESH/HOLD | neutral | exact |
| **REVERSE** | each division played back to front over the next one | DIV · DEPTH · EDGE | neutral | exact |
| **BRAKE** | the whole signal grinding to a halt, or launching | SPEED 0–200 % | SPEED is a level knob | moves |
| **DIVE** | the source itself bending into the drop | SEMIS −24…+12 · GRAIN | SEMIS is a level knob | moves |
| **SWIRL** | a room that will not hold still: the lines are chorused twenty times as deep as BLOOM's and the wet field turns with them | SIZE · DECAY · WARP · RATE · TONE · MIX | measured; DECAY and TONE are level knobs | moves, by design |
| **MANGLE** | four ways to break it, from Martian Gain. Travelling ENGINE across the build walks warm, ugly, folded, destroyed | ENGINE VALVE/FUZZ/SINE FOLD/ANNIHILATE · DRIVE · BIAS · CHAR · TONE · MIX | measured, K-weighted; DRIVE is held | exact |
| **SWARM** | one voice becomes a crowd: up to eight copies, each at its own pitch offset and its own wander | VOICES · DETUNE · DEPTH · RATE · SPREAD · MIX | computed | moves |
| **DUST** | older than the record: quantise, hold, wobble the transport, add the room the tape was in | BITS · RATE · WOW · NOISE · TONE · MIX | measured; NOISE and TONE are level knobs | moves |
| **ORBIT** | round the head, and behind it. ANGLE is where on the circle the source sits, and the score sweeps it | ANGLE · SPIN · WIDTH · REAR · SHADE · MIX | ANGLE and SPIN are level knobs | moves |
| **CHANT** | the track is made to speak. What arrives is the modulator; the carrier is made here | CARRIER · PITCH · BANDS · FORMANT · RESPONSE · TONE · MIX | measured | moves |

**ORBIT's back is measured, not asserted.** Behind the head it is 3.3 dB quieter,
6.6 dB darker at the top and 24 samples (0.5 ms) late, because the path round a
skull is longer than the path to the front of it. All three cues together are
what sell the circle; a pan pot gives you only the first.

**CHANT has no second input, so it makes its own carrier.** An insert carries one
signal, so what arrives is the modulator and the carrier is generated inside with
its own PITCH. Feed it a 440 Hz tone at PITCH 0 and it speaks at 65.4 Hz, the
carrier's own note. Sweep PITCH from the score and the machine rises with the build.

**MANGLE's four engines are a road, not a menu.** VALVE flatters, FUZZ ruins, SINE
FOLD turns the wave inside out, ANNIHILATE leaves gravel. ENGINE is stepped, so
travelling it from A to B changes engine on a musical boundary rather than
mid-phrase.

**BRAKE only launches out of a stop.** Above 100 % the playback head is trying
to catch up with the present, and from rest there is nothing to catch up to —
it cannot read the future. Brake first, then let it go: that is the gesture.

**BLOOM's FREEZE is exact.** At 100 % the loop gain is exactly 1.0, the
modulation stops and the delays are rounded to whole samples, so a frozen wash
neither creeps nor decays. The bench holds it flat over sixty seconds.

## The World rack in a slot (BWFX)

The bottom of every EFFECT menu opens **BROKILD WORLD FX** — ten modules of
the World rack, insertable in a slot like any of the eighteen, and swept from
A to B like any of the eighteen:

**TUBE · SWEEP · ENSEMBLE · HARMONIC · GATE · GRIT · STRIP · ECHO · SHIMMER ·
ROTARY**

The rack is already on this plugin, post-chain, with its five macros
automatable — so this is not about *reaching* those modules. It is about
**where they sit**. A slot puts one INSIDE the chain, with its own PLACE and
its own ON ARRIVAL. That buys three things the post-chain rack cannot:

- **Order.** A GRIT before CLIMB is a dirty filter; after it, a clean filter
  into dirt. Same two boxes, different instrument.
- **Field.** GATE set to SIDE rolls the room and leaves the lead alone.
- **The landing.** An ECHO set to SPILL rings on through the drop while
  everything around it is CLEARed.

And because BWFX's SYNC and FEEL are stepped parameters, a slot travelling
ECHO from 1/4 to 1/32 changes division **on the bar line**, the same way
STUTTER does. That is the thing worth building a rite around.

**They start switched off.** Assigning one changes nothing until you edit it
— each arrives with its one amount knob (MIX, DRIVE, DEPTH, CRUSH, COMP) at
zero, so A is dry and the first thing you reach for is B's amount. Eight of
the ten are bit-exact at rest; TUBE's valve stage is in circuit at any drive,
so that one colours the moment you assign it.

**What is different about them, said plainly:**

- **SPREAD does not reach them.** A World module has one set of parameters, so
  both channels read the same score. ENTER, EXIT, CURVE, DEPTH, PLACE, TURN
  and the MONO GATE all work normally.
- **They are not held to §8.** The eighteen declare what they may do to
  loudness and pitch and this plugin's bench holds them to it. The World
  modules are declared as guests — BWFX's own bench governs their DSP. What
  this plugin still guarantees is that they stay bounded and finite at both
  ends of every knob, and that nothing leaves above 0 dBFS.
- **SPILL on a module with no tail is STOP.** Only ECHO and SHIMMER have
  memory worth ringing on. A TUBE has nothing to spill, so from the arrival it
  is simply out of the way, exactly.
- **Two modules are deliberately not offered.** **SPACE** rebuilds its reverb
  between blocks, so its CHARACTER and LENGTH cannot travel — it stays in the
  post-chain rack, which is where a colour you set once belongs.
  **KIERANATOR** is a drawn step pattern, and the grid that draws it is on the
  BWFX face, not in a lane.
- **They cost what they cost.** Six SHIMMERs is six SHIMMERs. The post-chain
  rack's copy is a separate instance from a slot's.

## A first rite, step by step

1. Insert Rite of Passage on the drum bus or the master. Nothing changes yet.
2. Lane 1: choose **CLIMB**. Click the lane. In the B row set CUTOFF to 300 Hz
   and RESO to 60 %. (Until you do, the lane stays hatched.) Leave ENTER at 0
   and EXIT at 100 %.
3. Lane 2: choose **CHOP**. ENTER about 50 %, EXIT 95 %. A RATE 1/4, B RATE
   1/16. CURVE = ACCEL so the rate rushes at the end.
4. Lane 3: choose **RISER**. ENTER 30 %, EXIT 100 %. A LEVEL −60 dB,
   B LEVEL −6 dB, B FREQ 8000. SOURCE NOISE.
5. Lane 4: choose **GAP**. ENTER 92 %, EXIT 100 %. A DEPTH 0, B DEPTH 100.
   ON ARRIVAL = CLEAR.
6. Set **MONO GATE** to 60 %.
7. In the DAW draw a POSITION ramp 0 → 100 % over the last four bars before the
   drop. Draw an ARRIVAL blip that goes "on" just before the bar line of the
   drop, and back to "off" after.
8. Play. The filter closes across four bars, the chop starts halfway and
   accelerates, the riser rises from bar two, the mix narrows in the last 15 %,
   the gap cuts the last half-beat, and on the bar the chain lets go.

Two more worth trying once that works: **DIVE** on the last bar with B SEMIS at
−12 (the whole track bending into the drop), and **BLOOM** with B FREEZE at
100 % and ON ARRIVAL = SPILL, so the wash is caught and hangs over the downbeat.

Without a host clock the plugin free-runs at 120 BPM, so ARRIVAL still lands on
a bar of its own clock.

## What is verified

- **Engine bench, 506 checks, all clear**: the score is obeyed, the loudness
  contract holds per effect, ARRIVAL is sample-accurate, rendering is identical
  at 64 and 256-sample blocks, the six most expensive effects loaded at once
  cost about 10 % of one core.
- **Anti-aliasing.** BRAKE and DIVE read a delay line faster than it was
  written, which folds everything above the new Nyquist back into the band.
  Both run on a 2x oversampled line. Measured fold of a 14 kHz tone: DIVE at
  +12 st **−64.6 dB** (it was 0.0 dB before), BRAKE launching out of a stop
  −64.6 dB, GRAIN at full SPREAD −103.3 dB.
- **Wrapper harness, 57 checks**: parameters, buses, state round trip, a rite
  from a newer build tolerated, scrubbing fires nothing, ARRIVAL fires — and a
  rite carrying World modules comes back as the same modules with the same
  knobs.
- **Panel snapshot, 37 checks**: a travelling lane carries heat, a lane whose
  A equals its B carries none, and the lane menu is the eighteen with the ten
  World modules behind their own door, every one of them selectable.
- **The World modules in a slot**: inert until edited (eight of the ten
  bit-exact, and the bench names the two that are not and why), bounded and
  finite with every knob travelling, SPILL rings and CLEAR does not, and a
  re-entered lane drops what it was holding.
- **Not verified: nobody has heard it in a DAW yet.**

## What the panel still does not have

- **Seven effects have no mark of their own.** The glyph sheet carries eleven marks
  and the empty brackets; GAP falls back to its name in type. A World module in
  a lane has none either, and draws its name in the BWFX teal instead.
- **The wordmark is not used.** The delivered plank is shorter than its own
  lettering, so every letter is cut off at the bottom. The panel draws the
  title in type. See `assets/rite-decals/REDO.md`.
