# BEET — one kit from Kickstart, Snare Tactics and Hats Off

Status: **BUILT 2026-09-26 as BEETMACHINE, build 260926.1** - installed in both houses ("Beetmachine collection"), not yet published. Source `vst3-apps/beetmachine/plugin/`, build `C:\Users\peter\b\_build\Beetmachine\plugin`.

## The ask (Peter)

A wrapper with slots. Default: two Kickstart, two Snare Tactics, four Hats Off. Empty slots cost no CPU.
A MIDI pattern from C3 upwards plays a whole kit (no toms). Ideally it LINKS to the three plug-ins
rather than incorporating them.

## Recommendation: link at SOURCE level, not by hosting the installed plug-ins

**Hosting the installed VST3s inside Beet (plug-in-in-plug-in) is the bad version:**
- It is a plug-in host inside a plug-in: scanning, loading DLLs by path, their state as opaque blobs,
  their editors as child windows. Fragile, and all of it already exists, done better, in Ableton's Drum Rack.
- Smart App Control judges a DLL per file AND per path on this machine (see CLAUDE.md), so Beet loading
  Kickstart could be refused while Live loading the same file is not - the "vanished plug-in" class of bug,
  one level deeper and harder to see.
- Version skew: a project saved with Beet + Kickstart 260925.1 reopens against whatever Kickstart is installed.

**Compiling the three ENGINES into Beet from their own source files is the good version.**
The engines were built for it: plain C++ with no JUCE, each in its own namespace (``ks``, ``st``, ``ho``),
the identical interface ``prepare / reset / noteOn(note, vel) / process(params, out, n)``. Beet's
CMakeLists lists ``../kickstart/plugin/engine/*.cpp`` etc. directly - no copies, so a fix to Kickstart
reaches Beet on its next build (the BWFX pattern). This still honours "link, don't incorporate" in the way
that matters: one source of truth per drum.
- Empty slot = no engine object = exactly zero CPU.
- The three engines' own benches keep guarding them; Beet adds a kit bench (all slots, mapping, chokes).

## Shape (proposed)

- **Eight slots** (Peter wrote "six slots" and 2 + 2 + 4 = 8 - see questions). Each slot: type
  (empty / Kickstart / Snare Tactics / Hats Off), preset from that engine's own factory list, and a small
  common strip: LEVEL, PAN, TUNE, DECAY, and a CHOKE group (open hat choked by closed/pedal - a kit without
  it sounds wrong immediately).
- **MIDI**: one note per slot, default C3 upwards (Ableton's C3 = MIDI 60) with a GM switch (kick 36,
  snare 38, closed hat 42, pedal 44, open 46, ...) because almost every groove and MIDI file is written
  to GM. Beet translates the incoming note to the note each engine expects.
- **Outputs**: stereo mix first; optional multi-out (one pair per slot) later - Live can route those.
- **Host parameters**: the common strip per slot only (8 x 5 = 40), NOT every engine parameter
  (that would be several hundred lanes). Engine settings live in the slot's state, like the BWFX rack blob.
- **Panel**: native JUCE, same visual family as the three. The three existing editors are bound to their
  own processors and cannot simply be embedded; Beet gets a kit view plus a per-slot page. This is the
  biggest single piece of work.

## The zero-code alternative, worth knowing

In Live, a Drum Rack with the three plug-ins on pads does most of this today (empty pads cost nothing,
per-pad chokes, multi-out, any note map). A saved ``.adg`` kit could even ship with the plug-ins. It is
Ableton-only and cannot give a single "Beet" instrument, which is the case for building Beet at all.

## Decided (Peter, 2026-09-26)

1. **Eight slots, free to configure** - any slot can hold any of the three engines, or nothing.
2. **C3 upwards by default, with a General MIDI switch.**
3. **A CHOKE switch on every slot, with a "choked by" menu**: tick any of the other seven slots, and a
   stroke on any ticked slot silences this one. So an open hat choked by the closed and pedal slots is
   one setting, and so is a crash grabbed by a stop hit. Stored as an 8-bit mask per slot. The fade is a
   few milliseconds, not a hard cut, or it clicks. (Hats Off also keeps its OWN internal choke, which works
   inside one instance; Beet's is between slots.)
4. **Presets are themed kits** and follow a convention without enforcing it: kicks in slots 1-2, snares
   in 3-4, hats / ride / crash / splash (whatever suits the kit) in 5-8. Hats Off covers all of that range
   (size runs from an 8-inch splash to a 24-inch ride; BLOOM is the crash swell; OPEN goes from closed hat
   to free cymbal). Kits use the choke menu for the open hat.

5. **Both stereo mix and multi-out, in v1** (Peter: "can i have both?"). The main stereo bus always
   carries the mix; eight extra stereo output buses, one per slot, for hosts that route them
   (in Live: another track's "Audio From: Beet -> Slot n"). Each slot has an OUT choice:
   MIX (main only, default) / OWN (its own pair only, taken out of the mix so it is not heard twice) /
   BOTH. **Stereo is the default** (Peter): every slot starts on MIX, and the eight extra buses are
   declared inactive by default so a host only brings them up when one is routed. Costs a buffer copy, nothing more.
   Peter confirmed the per-slot MIX / OWN / BOTH convention ("follow the convention").
6. **Look**: raw vintage MACHINE controls, not audio equipment. Art brief for Beet and the three drums:
   ``assets/drum-decals/BRIEF.md``. Bench: MIX-only must equal the stereo sum exactly, and a slot
   on OWN must be absent from the main bus.

7. **Full editing, with the drums' OWN panels** (Peter: "i need full editing capabilities. Can i have the
   same panels preserved as in the child vst3s?"). Yes, and it is simpler than building new ones:
   - Each occupied slot holds a real instance of that drum's PROCESSOR class (KickstartProcessor,
     SnareTacticsProcessor, HatsOffProcessor), compiled from its own source. Beet feeds it MIDI and pulls
     its audio, the way a host would. JUCE's own AudioProcessorGraph nests processors the same way.
   - Selecting a slot shows THAT DRUM'S OWN EDITOR inside Beet, unchanged: every control, its hit display,
     its preset browser, its load/save. Each editor only reaches its processor through ~13 members
     (apvts, presets, meters, triggerFromUI, patchFolder...), all of which exist on the nested instance.
   - A reskin of a drum (assets/drum-decals) therefore reaches Beet for free.
   - Chokes are done by Beet on the slot's OUTPUT (a few-ms fade), so no engine needs a choke API.
   - Empty slot = no processor object = zero CPU. An occupied slot costs what that drum costs on its own.
   Costs and limits, honestly:
   - The three processors need a small guard when built inside Beet: each defines the plug-in entry
     point (createPluginFilter), which would collide three times at link, and the patch folder is named
     after the product, which inside Beet would be "Beet" instead of the drum's own folder. A BEET_HOSTED
     define handles both; the standalone plug-ins are unchanged.
   - Inner drum parameters are NOT host-automatable one by one (8 slots x 21-31 parameters is hundreds of
     lanes, and they change with the slot's type). Automation is the per-slot strip (level, pan, tune,
     decay) plus Beet's own; a macro scheme like BWFX's can come later if needed.
   - Window: the drum panels are 1120 x 716 (Kickstart) and 1400 x 724 (Snare Tactics, Hats Off). Beet is
     its kit strip (eight slots, chokes, outputs, MIDI map) above a 1400 x 724 panel area, so about
     1400 x 950. Kickstart sits centred in that area.
   - Snare Tactics and Hats Off are being built by another session right now; the BEET_HOSTED guard touches
     their processors, so it waits until they are settled, or is agreed with that session.


## What was built (260926.1)

- Eight slots, each empty (zero CPU: 0.011 % of a core for eight empty slots) or a REAL instance of
  KickstartProcessor / SnareTacticsProcessor / HatsOffProcessor compiled from the drums' own folders.
  **No drum source was edited.** Three names collide across the drums and are renamed per drum from
  Beetmachine's CMakeLists: the entry point createPluginFilter, and the panel classes HitPad and
  HitDisplay (different classes under one name - the linker refused them, which is the good outcome;
  a new global name in a drum's panel will surface the same way).
- The selected slot shows that drum's OWN editor, unchanged. Replaced drums are parked and deleted only
  once no editor can point at them.
- Latency: kick 95, snare 95, hats 103 samples; every slot is aligned to 103 and the host told so.
- **The bug the bench caught: Snare Tactics and Hats Off default KEYS to GM KIT, where note 42 IS the closed
  hi-hat** - so sending Hats Off its "reference" note turned every crash and ride into a short closed hit.
  Beetmachine reads each drum's KEYS live and sends the note meaning "dialled sound" (Hats: 60 on GM,
  42 on CHROMATIC/FIXED; Snare 38; Kick 36).
- Ten themed kits (STUDIO, 808, 909, EIGHTIES, BOOM BAP, TECHNO, DUB, INDUSTRIAL, TRAP, LO-FI), presets
  named by NAME, open hat choked by closed hat. A kit never changes notes or output routing.
- Gates: eettest 25 checks ALL CLEAR (routing exact to 1e-7, solo exact, choke -62 dB in 15 ms and
  click-free, alignment to the sample, state round trip incl. each drum's own settings); eetshot
  renders the real panel (all clear). 	ools/run-sac.ps1 runs a fresh test exe past Smart App Control.
- Decals from ssets/drum-decals (delivered 2026-09-26) baked in via 	ools/ingest-decals.ps1: cabinet
  ground, nine-slice machine bays, bakelite knobs (red for MASTER), palm HIT and red E-STOP (the UP
  drawings only - the delivered pressed drawings are not registered and would jump; pressing is drawn),
  registered jewel lamps, label tape. The delivered nameplate reads BEET (ordered before the rename): the
  blank enamel plate is lettered BEETMACHINE in code until a proper plate is ordered.
- Cost: a full kit hit on every 16th on all eight slots is ~40-46 % of one core - the drums' own cost
  (4x oversampled each), not Beetmachine's.

## Still to do

- **Manual**: waits until the design has settled (Peter, 2026-09-26).
- **Publishing and the collection**: follow the other thread's handover exactly (below).
- **The three drums' own reskins** with their grounds and nameplates from ssets/drum-decals - that is
  work in THEIR panels, to agree with the session that owns them. Beetmachine picks it up for free.
- A proper BEETMACHINE nameplate (the delivered one says BEET).
- Ideas: per-slot ARTICULATION (send Hats Off's closed/pedal/open/bell/edge from one preset across slots);
  audition/choke from the drum's own HIT pad (today its pad plays but does not choke other slots).

## Handover from the collection thread (2026-09-26) - do this when Beetmachine ships

1. st3-apps/beetmachine/ as a published plugin page: app.json (tags ["vst3","synth"]), index.html,
   README.txt, manual PDF, manifest.json entry.
2. Beetmachine-VST3-win64.zip there, holding exactly ONE .vst3, ONE standalone .exe, ONE manual .pdf.
3. Publish from BASH: 
ode tools/publish-downloads.js vst3-apps/beetmachine/Beetmachine-VST3-win64.zip,
   then 
ode tools/wire-downloads.js; fetch the public URL and compare sha256.
4. install-fleet.ps1 and check-names.js entries (DONE 2026-09-26); check-groups.js all clear (DONE).
5. st3-apps/collection/contents.json (CRLF - use the Edit tool) under collections.beetmachine: append the
   slug to "includes", claims.count 4, claims.phrase e.g. "three drum synthesisers and the kit that plays
   them", update blurb and lede, delete "_pending".
6. uild-collection.ps1 -Collection beetmachine, publish the collection zip, uild-collection-pages.js,
   	ools/sync-site.js, 	ools/check-links.js - publish BEFORE building pages; sync-site AFTER.
   The builder must say "verified: 4 of 4 ... and nothing else in the archive".
7. Card image ssets/app-previews/beetmachine-collection.jpg: add a Beetmachine band; no count as text.
8. Verify by rendering over HTTP: "The Beetmachine Collection (all 4)", four working links, public sha256.
9. Stage by naming paths; git fetch first; add a line under "### THE BEETMACHINE COLLECTION" in CLAUDE.md.