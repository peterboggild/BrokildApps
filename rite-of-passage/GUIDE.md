# RITE OF PASSAGE — what the controls are, and how to use it

*Written 2026-09-18 from the code as built (build 260918.1), not from the design.
Where the panel does not yet match the design, this says so. Pictures of the
real panel, rendered by `ropshot`: `docs/panel-empty.png`, `docs/panel-loaded.png`,
`docs/panel-midway.png`.*

## What it is, in one paragraph

An insert effect for the bar before a drop. You put it on a track (or a bus),
load up to six effects into six **slots**, and tell each slot *where* along one
master slider it should start and finish moving from its **A** setting to its
**B** setting. Then you automate that one slider — **POSITION** — from 0 to
100 % over the build. The slots ignite one after another as the slider passes
their entry points. At the end you fire **ARRIVAL**, a separate command, and
every slot lets go on the next bar line, to the sample, with a sub-bass impact
under it. Scrubbing the slider never fires the drop; only ARRIVAL does.

**A fresh instance does nothing.** Every slot is empty, so POSITION moves
nothing and the audio passes straight through. That is by design (an empty
rite is bit-transparent) but it is also why it can look broken at first.
Assign an effect to a slot and it comes alive.

## Signal path

    in → slot 1 → slot 2 → … → slot 6 → stereo stage (SPREAD / TURN / MONO GATE)
       → MIX (dry/wet) → OUTPUT → BWFX rack (post-chain) → out

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
| **MONO GATE** | 0–100 % | how far toward mono the mix collapses over the LAST 15 % of the travel; ARRIVAL releases it back to full width — the classic "narrow, then open on the drop" |
| **BWFX MACRO 1–5** | | the World FX rack's five macros (the rack itself sits post-chain, default empty) |

## Each slot (one lane on the panel)

From left to right on a lane:

| control | what it does |
|---|---|
| **tick box** | slot on/off |
| **effect menu** | which effect the slot holds ("—" = empty; duplicates are allowed) |
| **ENTER** (first slider) | the POSITION at which this slot starts moving from A toward B (0–100 % of the travel) |
| **EXIT** (second slider) | the POSITION at which it reaches B. Before ENTER the slot sits at A; after EXIT it holds at B. |
| **DEPTH** (third slider) | how far along the A→B route it actually gets by EXIT. 60 % means it is still on its way when the drop lands, which is often the better sound. |
| **PLACE** (ST / MID / SIDE / L / R) | which part of the stereo field the slot works on |
| **TAIL** (BYPASS / SPILL / CLEAR) | what ARRIVAL does to this slot: BYPASS stops it dead; SPILL stops feeding it but lets its tail ring on (the echo or wash spilling over the downbeat); CLEAR silences it and zeroes its buffers (the vacuum) |
| **CURVE** (LIN / ACCEL / DECEL / S / PEAK) | the shape of the A→B travel: linear; late then a rush (x³); fast then settling; S-curve; out and back (PEAK reaches B in the middle and returns to A) |

The lane's coloured band shows its ENTER–EXIT span; it turns to ember as the
marker crosses it. Click a lane to select it; the **A / B editor** below then
shows that slot's parameters — top row **A** (yellow), bottom row **B**
(ember). A parameter whose A equals its B does not move.

Not on the panel yet, but in the state: per-parameter curve overrides,
the stepped-parameter grid (bar / ½ / ¼ — a STUTTER division only changes
on a musical boundary), the ARRIVAL settings (grid, impact on/off, impact
tune 48 Hz, decay 700 ms, level −6 dB, restore-dry), the mono-gate span and the
bass-mono corner.

## The six effects that exist

| effect | what it is | parameters (A and B each) | level rule | pitch |
|---|---|---|---|---|
| **CLIMB** | resonant filter sweep to self-oscillation — the spine of a build | MODE LP/BP/HP · CUTOFF 20 Hz–20 kHz · RESO 0–100 % · DRIVE 0–100 % | spectral: loudness follows what the filter removes, resonance adds none | exact |
| **TAPE** | analogue echo whose head moves: changing TIME bends the pitch (REPITCH) or crossfades (FADE); MOTOR is how heavy the head is, so it overshoots and wobbles into place; FEEDBACK past 100 % runs away | TIME 20–1500 ms · FEEDBACK 0–120 % · MOTOR 1–2000 ms · MODE REPITCH/FADE · DRIVE · WOW · MIX | neutral on the dry path | moves, in REPITCH |
| **STUTTER** | captures a slice and repeats it, so time stops; tighten the division toward a buzz; PITCH per repeat; CAPTURE REFRESH takes a new slice each time, HOLD keeps the first | DIV 1/4 · 1/8 · 1/8T · 1/16 · 1/16T · 1/32 · DEPTH · DECAY · PITCH ±12 st · CAPTURE | neutral: the slice is matched to what it replaced | exact at PITCH 0 |
| **CHOP** | gates the LIVE signal rhythmically, so time continues; accelerate RATE from A to B | RATE 1/4 … 1/32 (same six divisions) · DEPTH · DUTY 10–90 % · SLEW 0.1–50 ms | neutral: duty is compensated, three CHOPs in series stay the same loudness | exact |
| **RISER** | a generator: noise, tone or a Shepard (endless) climb, mixed IN — works even when the bar before the drop is silent | SOURCE NOISE/TONE/SHEPARD · FREQ 50 Hz–12 kHz · RESO · LEVEL −60…+6 dB · WIDTH | intentional: LEVEL is the point | moves |
| **GAP** | the silence before the drop, placeable anywhere on the score — a ⅛ hole at 85 % of the travel, not only at the end | DEPTH 0–100 % · EDGE 0.3–80 ms | intentional | exact |

Not built yet (designed, named in the README as "to come"): GRAIN, BLOOM,
FREEZE, REVERSE, BRAKE, DIVE.

## A first rite, step by step

1. Insert Rite of Passage on the drum bus or the master. Nothing changes yet.
2. Lane 1: choose **CLIMB**. Click the lane. In the A/B editor set A CUTOFF
   20000 and B CUTOFF 300, B RESO 60. Leave ENTER at 0 and EXIT at 100 %.
3. Lane 2: choose **CHOP**. Set ENTER to about 50 %, EXIT 95 %. A RATE = 1/4,
   B RATE = 1/16. Set CURVE to ACCEL so the rate rushes at the end.
4. Lane 3: choose **RISER**. ENTER 30 %, EXIT 100 %. A LEVEL −60 dB,
   B LEVEL −6 dB, B FREQ 8000. SOURCE NOISE.
5. Lane 4: choose **GAP**. ENTER 92 %, EXIT 100 %. A DEPTH 0, B DEPTH 100.
   TAIL CLEAR.
6. Set **MONO GATE** to 60 %.
7. In the DAW draw a POSITION ramp 0 → 100 % over the last four bars before
   the drop. Draw an ARRIVAL blip that goes to "on" just BEFORE the bar line
   of the drop (up to a beat before is fine: it fires on the next bar
   boundary, sample-accurately). Draw it back to "off" after.
8. Play. The filter closes across the four bars, the chop starts halfway and
   accelerates, the riser rises from bar two, the mix narrows to mono in the
   last 15 %, the gap cuts the last half-beat, and on the bar the chain lets
   go: dry signal back in one block, full width restored, a 48 Hz impact
   under the first hit.

Without a host clock (a standalone test, or a host that reports none) the
plugin free-runs at 120 BPM, so ARRIVAL still lands on a bar of its own clock.

## What is verified, and what is not

- Engine bench: 133 checks, all clear on this machine (the score is obeyed,
  loudness contracts held per effect, ARRIVAL sample-accurate, rendering
  identical at 64 and 256-sample blocks, six slots at 0.7 % of a core).
- Wrapper harness: 20 checks, all clear (parameters, buses, state round trip,
  a rite from a newer build tolerated, scrubbing fires nothing, ARRIVAL fires).
- Panel snapshot: 3 checks, all clear; the pictures in `docs/`.
- **Not verified: nobody has heard it in a DAW yet.**

## Panel faults seen on the rendered picture (to fix)

- The empty-slot entry shows as mojibake ("â□□") — the em dash went through
  `juce::String(const char*)`, which is Latin-1 (the High Tide lesson).
- The three lane sliders carry no labels; nothing says which is ENTER, EXIT
  or DEPTH.
- The A/B knobs are 42 px with value boxes that read "800.0000…" and
  stepped choices that read "0" instead of "LP".
- "BWFX" is drawn as text only; there is no button and no rack overlay, so
  the World FX rack cannot be opened from the panel (its macros are still
  host parameters).
- The PLACE / TAIL / CURVE menus are unlabelled and TAIL is truncated to "BYPA…".
