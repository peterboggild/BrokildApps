# THE SITE — cross-artefact coupling for the Proxima findings

*Specced 2026-09-03 from Peter's proposal ("I know VST3s can have shared data
space… I would like the temperature to be global… at lower temperatures they
should tend to share timing… Could there be a Cross-talk / Distance slider in
the global settings?"). Status: DESIGN, awaiting go. B2311.67 is owned by
another session — this document is written so that session can adopt the
protocol without talking to this one.*

---

## 1. The idea, and why it is thermodynamics

The four artefacts were recovered from one site. They already share one axis —
temperature — and the survey's records agree on what it does: cold, an object
is still, ordered, obedient; heated, it comes alive and goes its own way.

So the coupling law writes itself, and it is real physics, not a metaphor:
**low temperature is low entropy.** Cooled, the findings condense into a
common phase — they share the climate and fall into step in time. Heated,
they decouple into independent, disordered, self-active bodies. One axis
covers Peter's whole proposal: global temperature, timing that aligns when
cold, and wildness that is private when hot.

In-fiction: the survey notices that objects stored on the same bench drift
into step overnight when the shelter cools, and stop doing so when moved
apart. Hence THE SITE panel: the climate, and the distance between them.

## 2. The mechanism: one small named shared-memory block

Windows named file mapping (`CreateFileMapping(INVALID_HANDLE_VALUE, …,
"Local\\BrokildProximaSite")` + `MapViewOfFile`). This is the only sanctioned
way for four DIFFERENT plugin DLLs to see each other, and it also works
ACROSS PROCESSES in the same login session — DAW plus standalones together.
Header-only (`proxima_site.h`, living in the BWFX adapter include dir, which
every plugin already has on its path), compiled into each plugin — no runtime
DLLs (SAC, and the house rule).

    struct SiteBlock  (v1, ~2 KB, everything in lock-free atomics)
      magic 'PXBS', version, size          — future artefacts join by version
      coupleClimate, coupleTiming : u32    — the hidden settings
      distance : float                      — 0 same bench … 1 different rooms
      siteKelvin : float  + writeSeq : u32 — the shared climate
      pulsePhase : double, pulseHz : float — THE SITE PULSE (slow, 0.1–3 Hz)
      keeperId + keeperHeartbeat           — who advances the pulse
      slots[16] : { heartbeatMs, kind, activity, localKelvin }

Rules that keep it safe and cheap:
- **Audio thread only READS a handful of relaxed atomics per control tick.**
  No locks anywhere; a crashed instance cannot wedge the others (heartbeats
  expire, the next live slot takes the pulse over).
- **The pulse keeper** is the eldest live slot; it advances `pulsePhase` from
  its own sample clock. Followers never write it.
- **Settings** live in `%APPDATA%\Brokild\ProximaSite.json` (climate shared
  on/off, timing shared on/off, distance), surfaced identically in each
  plugin's hidden legend panel as THE SITE. Not per-instance host parameters —
  the site belongs to the site, not to an instrument.

## 3. What crosses, and how strongly

Coupling strength for everything below:
`k = coupling × (1 − distance) × (1 − warmth)` — cold and close couples,
hot or far decouples. (Climate sharing itself ignores warmth — the weather is
the weather.)

1. **Climate.** With climate-share on, AMBIENT is one value for the whole
   site: moving any instrument's slider (or automating its lane) moves them
   all; the others' thermometers visibly follow. Infuriating and mysterious,
   as ordered. Each instance still SAVES its own local ambient in the DAW
   project — the shared value is runtime-only, so switching the coupling off
   returns every project to exactly its dialed-in state (the Kemper rule).
2. **Timing.** No master clock grid is introduced anywhere. Each artefact
   keeps generating time its own way and merely LEANS on the site pulse with
   weight k, through a native joint it already has:
   - **B2311.1** — the easiest and the most honest: it is a pulse-coupled
     lattice whose entire thesis is entrainment. The site pulse enters as a
     second imposed pulse. Zero architectural violence.
   - **B2311.104** — traffic-packet spawn times, the chuff timers and the
     precession phase are already timers; each gets a weak phase pull.
   - **B2311.22** — the breathing sustain floors and the interlocutor's
     interjection timing lean; REVIVAL does NOT (exact recurrence is specimen
     identity and stays untouchable).
   - **B2311.67** — its session's call, against this protocol.
3. **Presence** (small, optional in v1): each instance publishes an
   `activity` scalar (how much it is carrying). Siblings may respond gently —
   .104's traffic quickens when the site is busy, .22's second body has
   something to listen toward. Weak, warmth-gated like everything else.

## 4. Honest limitations (stated up front, not discovered later)

- **Determinism.** The house benches promise bit-identical renders; shared
  state across free-running instances cannot. The contract: coupling OFF ⇒
  memcmp-identical to today (a bench check, like the BWFX neutral-bus guard).
  Coupling ON is a live-performance mode and the docs say so.
- **Offline render.** During a bounce, instances run at unrelated speeds and
  wall-time is meaningless. Each instance checks `isNonRealtime()` and
  freezes its site inputs (climate held at last value, pulse pull = 0), so a
  bounce is stable and close to what was heard. Documented, not hidden.
- **Sandboxed hosts** that put plugins in separate processes are exactly why
  this is a named mapping and not process globals — it still works.
- **B2311.67** is another session's tree; it joins by adopting the header,
  not by this session editing it.
- **Two instances of the same plugin** also couple. That is correct fiction
  (two fragments of one grid) and needs no special case.

## 5. Why a DISTANCE slider (and the better version of it)

One slider is the right v1: it is physical, it degrades gracefully (far =
exactly today's standalone behavior), and it is one number a future artefact
can obey without negotiation. The better version, later: THE SITE panel draws
the four objects as marks on the bench and Peter DRAGS them — per-pair
distances, the survey rearranging its own shelf. v1 ships the slider; the map
is the v2 the panel is already shaped for.

## 6. Order of work when go is given

1. `proxima_site.h` (block, seqlock'd writes, keeper election, settings IO).
2. B2311.104: the temperature-law fix FIRST (its buglist item 1 — the site
   makes no sense while cold reads as wild), then climate + pulse + SITE panel.
3. B2311.1: climate + pulse-as-imposed-pulse + SITE section in its panel.
4. B2311.22: climate + breathing/interlocutor lean.
5. Bench per plugin: coupling-off memcmp guard; a two-engine in-process test
   that phase alignment actually tightens when cold and close (measured, not
   assumed); offline-freeze behavior.
6. Handover note for the .67 session; CLAUDE.md; push.

## 7. Status 2026-09-03 — built, measured, and the recipe for B2311.67

**Shipped.** `BrokildWorldFX/adapter/proxima_site.h` (header-only; the named
mapping `Local\BrokildProximaSite`; settings in `%APPDATA%\Brokild\ProximaSite.json`)
is wired into **B2311.104, B2311.1 and B2311.22**. Coupling defaults OFF and
every bench proves the uncoupled render is **byte-identical** to an engine that
never called `setSite` (a plain sample-by-sample compare). What each one does
when coupled, and the number that proves it:

| finding | the lean | measured, cold + distance 0 |
|---|---|---|
| .104 | the grid's traffic packets are scheduled on the site's wraps | 24/24 packets within 0.1 s of a wrap, vs 4 % free |
| .1 | the site is a SECOND imposed pulse: on each wrap it shoves the lattice like the host pulse, scaled by the pull and the cold gate | output concentration on the site period (see its bench) |
| .22 | the sustain floor breathes on a FOUR-wrap cycle (its modes reach the floor over 2–9 s, so a swell at 0.5 Hz filtered to nothing — measured 0.010 vs 0.010) and the interlocutor interjects only near a wrap | envelope concentration on the four-wrap cycle (see its bench) |

The law in every case is the header's one line: `pull = (1 − distance) × (1 − warmth)`
when TIMING is shared and somebody else is present; the climate is a host
parameter on each finding, so a shared move is visible, automatable and
undoable in any DAW. The SITE panel in ANY finding edits the same settings.

**Recipe for .67 (an afternoon; the header needs no change — kind 67 already
has its natural-rate multiplier, 1.012):**

1. `#include "proxima_site.h"` — the BWFX adapter dir is already on your
   include path (you include `bwfx_juce.h` from it). Member
   `proxima::Client site;` — `site.open (67)` in the processor ctor,
   `site.close()` in the dtor.
2. A `siteStep()` on the processor's **own** timer (so it runs with the editor
   closed). Copy .104's `siteStep()`/`emitSite()` from
   `ArtefactB2311_104/Source/PluginProcessor.cpp` — forty lines: warmth 0..1
   and kelvin from your temperature param; `if (isNonRealtime() || !site.isOpen())`
   hand the engine pull 0 and return; `auto v = site.sync (activity, myK, phase, warmth)`;
   climate = propose when YOU moved (guarded on the last applied value, or two
   instances chase each other round the rounding error), follow `v.climateMoved`
   through your host parameter; `pull = Client::pullStrength (v, warmth)`;
   `phase = Client::stepPhase (phase, site.naturalHz(), dt, v, pull)`; hand
   `(phase, v.pulseHz, pull)` to the engine.
3. Engine: `setSite (phase, hz, pull)` into three atomics, ONE mechanism that
   leans on the site phase, inside `if (pull > 0)` so pull 0 is never entered.
   Choose what in .67 is a TIMING — the thing that should fall into step with
   the bench. Ease a local phase onto the negotiated one (the .1/.22 pattern:
   `diff -= floor(diff+0.5); phase += diff*0.35 + hz*dt`).
4. UI: `{k:"site", climate|timing|distance}` up; a `site` event down with
   `{open, climate, timing, distance, others, coh, pull, kelvin, phase}`. The
   panel markup + JS is in .104's `ui.html` under "the site" (ids
   `siteClimate / siteTiming / siteDist / siteDistRd / siteStatus`, plus a
   one-line status readout). .22 has the same panel as a rail overlay.
5. Bench, two checks: uncoupled memcmp against an engine that never called
   `setSite`; coupled, cold, distance 0, the lean measurably present (fold the
   output envelope on the site period, or count events on wraps). Render long
   enough to hold several site periods — the vacuous-window trap.
6. `test/sitetest.cpp` in .104 exercises the header with two clients in one
   process (slots, climate propagation, Kuramoto convergence, hot → zero pull,
   coupling off → zero pull); reuse it if the header ever changes.
7. **Your page must echo host-parameter changes.** The climate arrives as a
   change to your temperature parameter made by the processor, not by the
   page; if the page only learns its values from `initialState`, the sound
   moves and the slider does not, and the whole thing reads as broken (Peter's
   first report on 260903.1 was exactly that — .104 and .1 had no echo, .22
   did). Diff `raw[]` against a `lastSent[]` on the timer and emit a
   `hostParam` batch; mark the page's own `"p"` moves in `lastSent` so they
   are not echoed back.
8. **Chosen here, global now.** When a panel turns CLIMATE on, propose that
   finding's own temperature at once, so the bench takes it; and for the
   first second after construction make no proposals (the host is still
   restoring state) while following the bench's climate if it has one — or a
   project load would rewrite the bench, and a restored value would sit off
   the bench forever.
9. **Seed an empty bench.** The bench's temperature lives in the block, and
   the block dies with the last instance — so opening a project with CLIMATE
   already shared gives a bench with no temperature at all, and nothing
   converges until somebody happens to move a slider. Every finding sits at
   its own value with the setting plainly switched on, which reads exactly
   like the sharing being broken. The first settled finding to find
   `siteKelvin <= 0` proposes its own.

## 8. How the site is verified — three levels, because one was not enough

Each level covers a link the others cannot reach. All three are in
`ArtefactB2311_104/test/`.

| level | what it drives | what only it can catch |
|---|---|---|
| `ab104siteclimate` | the climate state machine, **four findings in one process** | the DAW configuration: propose/follow/seed/settle, a dragged slider, ping-pong, an unwired finding present |
| `ab104host` (`test/host/`) | **the installed .vst3 bundles in a real JUCE host** | anything broken in the plug-in wrapper rather than the engine — the only link the other two never touch |
| `site-live.ps1` | three **standalones** over CDP | the panels: buttons, readouts, and the sliders actually moving |

Plus `ab104sitewatch`, which is not a test but a **window on the bench**: it
opens the shared block read-only and prints who is present, what each is
publishing, and how long ago they last spoke. Run it while a DAW is open and
it answers "is this finding on the bench" in one line — which is the question
every site bug reduces to. A finding that does not appear there is not wired,
whatever its panel says.
