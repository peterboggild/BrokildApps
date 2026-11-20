# DrumEQ - ideas and faults, collected (build only on Peter's go)

## 1. MASTER: tune the whole kit from one panel (Peter, 2026-11-20) - BUILT 261120.2 (see DRUMEQ-DESIGN.md)

Peter: "as long as LINK is pressed [any instance] can be nominated as MASTER. As long as its master, its panel get
another color, i.e. blue, and then changing to a different source does not change the source for that track, but
allow adjustments for that track ... I don't have 11 identical panels floating around, but can tune them all from
one VST3 panel."

Verdict: doable, and it fits the existing kit block (`Local\BrokildDrumEQKit`), which already holds one slot per
drum. The master edits those slots; each track playing that drum takes its slot. About a day of work.

**Behaviour**
- MASTER button beside LINK, only enabled while LINK is on. One master per session: nominating a second demotes the
  first. Turning LINK off, removing the track, or the master going stale (heartbeat > 2.5 s) clears the role.
- Master panel turns blue (frame, accents, the SOURCE row). The master's own drum is marked HOME on the SOURCE row;
  the drum being edited is lit blue. Clicking a drum edits it; clicking HOME returns to this track's own controls.
  The master's own audio always stays its own drum.
- Editing a remote drum: knobs, EQ nodes, presets and LOAD/SAVE preset act on that drum's slot. Every LINKED track on
  that drum (two OH tracks, say) follows within ~30 ms, through the normal 20 ms knob smoothing. Unlinked tracks ignore
  it, as they ignore kit loads.
- A drum with no track can still be edited: it is stored in the kit and picked up when a track joins. Its lamp says so.
- Followers write what they receive into their OWN parameters, so each track's project state holds its own settings.
  Reopening the set, freezing, bouncing or removing the master changes nothing - the master is an editor, never a
  dependency.
- A follower's own panel, if opened, shows the master's changes live, and edits made there go back to the master's
  view (last writer wins).

**What the master panel needs from the follower** (published to the block at ~20 Hz)
- the follower's input and output spectra (decimated, ~256 bins each) and its meters, so the EQ display and the gate
  /compressor meters show the drum actually being tuned, not the master's kick;
- FIND DRUM on a remote drum = a command flag in the slot: the follower captures and finds, the master sees the
  result. (Could ship first with FIND only on HOME.)

**Implementation notes**
- Block: `masterId`, per-slot `editSeq` + `editor` id (a follower applies when `editSeq` moves and `editor != self`),
  per-slot spectrum/meter area, per-slot FIND request/answer. Version the block (a new layout must not be misread by
  an older instance still loaded) - bump the mapping name.
- Timer to 30 Hz (10 Hz would step audibly while a knob is dragged).
- Editor: knobs bound to the APVTS for HOME, to a local mirror of the slot for a remote drum (attachments swapped on
  target change).
- After applying a remote edit, record it as already published (the kit-load rule), or the follower re-publishes and
  fights the master.
- Caveats to tell the user: host automation on a follower's parameter overrides the master on that track; the
  follower's parameter changes reach the host as plug-in changes (Ableton may add undo steps on that track, and
  will record automation if that track is armed for automation).

**Gates to add**: dehost - three instances, one master: master edits SNARE TOP's band 2 -> snare track's APVTS holds
it within one tick, kick track untouched, unlinked snare untouched; master removed -> followers keep values; a second
master demotes the first; save/reload of a follower without the master. deshot - blue panel, HOME mark, remote
target, nothing overlapping.

## 2. FIND DRUM also sets the levels (Peter's question, 2026-11-20) - BUILT 261120.2 (see DRUMEQ-DESIGN.md)

Today FIND DRUM only moves the body band's frequency (`finishFind()`); it changes no level. The capture already holds
what a level setting needs: the strokes and the bleed between them.

REVISED after Peter's point (most tracks are recorded quiet, and not in proportion to each other): **set the INPUT,
not the thresholds.** Thresholds chased down to a quiet track end at the bottom of their knobs and every preset's
numbers stop meaning anything; normalising the input makes every track arrive at the same level, so gate, EQ drive and
compressor work as the preset was written and stay tuneable by small moves.

From the same few hits:
- **IN trim** so the median stroke peaks at a fixed target (about -12 dBFS). Widen IN to -24..+36 dB (a mic peaking at
  -45 dBFS needs +33). Old projects keep their stored value, so widening the range must keep the dB value, not the
  normalised 0-1.
- Thresholds then stay as the preset has them; only a nudge if the measured hits sit far off the preset's assumption.
- **Gate THRESHOLD** between the bleed floor and the quietest stroke, measured after the new trim in the gate's own key
  band, only if the gate is on (bleed relative to hits differs per recording, so this one really is per track).
- **OUT**: leave the kit normalised by default (the recorded proportions were accidental; balance belongs on the
  faders). Option "keep track level": OUT takes back what IN added, so the fader balance the user already has stays.
- Applies to the kit too: with MASTER, FIND on each drum normalises each track, so the kit arrives in proportion.

Use the median stroke, not the loudest, so one rimshot does not set it. Report it on the status line ("BODY 82 Hz,
IN +18.5 dB, gate above bleed"). Gate: dehost plays the same strokes at -40 and at -6 dBFS and must land the same
post-trim level and the same compressor reduction on the hits.

## 3. LISTEN ALL: set up the whole kit in one go (Peter, 2026-11-20) - awaiting go

Peter: "can it run listen on all tracks at the same time, or one by one? I.e. set levels and EQ for the whole kit in
one go? Autosetup gain and EQ?"

Today: one track at a time (on the track, or from the MASTER panel one target drum at a time).

**Proposal: LISTEN ALL on the MASTER panel; every linked track listens AT THE SAME TIME.** Simultaneous is better than
one by one: every track hears the same passage of the song, it takes 6 s instead of a minute, and it is no harder - each
track has its own audio and its own capture already. The kit block already carries a find request per drum; the master
sets them all at once and shows a row of results (drum, note found, IN change, gate).
- Drums with no track are skipped and listed as such.
- KEEP TRACK LEVEL applies to all of them (one switch on the master).
- One UNDO for the whole run: each track keeps its values from before, and the master can send "restore".

**What "auto EQ" would honestly mean** (in rising order of ambition):
1. Already there: the body band goes on the drum's note (kick, snare, toms) and the levels/gate are set.
2. Ring and box: find a narrow resonance that sustains after the hit (tom ring, snare ring, kick boxiness 250-500 Hz)
   and put a narrow cut on it in the band meant for it. Measurable from the same capture (a peak that decays slower
   than its neighbours).
3. Kit balance: after every track arrives at -12 dBFS, set each OUT to a common starting balance (kick and snare
   forward, toms a little under, OH/room lower). A starting point, not a mix.
4. Polarity: Kick In vs Kick Out, Snare Top vs Snare Bottom, close mics vs OH - only possible with all tracks listening
   at once, because it compares the tracks against each other. Needs a short piece of each track's audio in the shared
   block (about 1 MB per drum). Flips POLARITY where the pair cancels. (Time alignment needs a delay, which DrumEQ does
   not have.)

Recommendation: 1 + LISTEN ALL first, then 4 (polarity, cheap and clearly right or wrong), then 2; 3 only as an option.

**Rule (Peter, 2026-11-21): the SOURCE is always the user's choice.** "I would like to manually set source type; I don't
want the VST3 to decide this." Nothing in DrumEQ changes a track's SOURCE today (checked: only that track's own source
row does). LISTEN ALL, FIND and any auto EQ work INSIDE the drum the user chose - they never guess what is on the track.
