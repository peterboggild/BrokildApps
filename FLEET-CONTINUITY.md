# Where the work lives, and how to keep it

Written 2026-09-02 from the music PC, after the session that built Artefact
B2311.1 stopped being reachable mid-task.

The short version: **the fleet is nine-tenths safe and one-tenth on a single
disk.** This file says exactly which tenth, what to do about it, and what has
been recovered in the meantime.

---

## 1 · What exists, and where

| tree | source lives | remote | reachable from a Claude session here |
|---|---|---|---|
| Clone Wars | `vst3-apps/clone-wars/plugin/` in **this** repo | this repo | yes |
| BrokildWorldFX | `C:\Users\peter\b\BrokildWorldFX`, mirrored at `BrokildWorldFX/` here | `brokild-world-fx` + the mirror | the mirror, yes |
| Black Rider, Blade Ruiner, Escape Room, Full Metal Racket, Hairfryer, Martian Gain, Photo Synth, Artefact B2311.22, Artefact B2311.67 | `C:\Users\peter\b\<Tree>` | private `brokild-<product>` | **no** |
| **Artefact B2311.1** | `C:\Users\peter\b\ArtefactB2311_1` | **none** | **no** |

Two separate problems hide in that table, and they need different fixes.

### Problem one — B2311.1 has no second copy anywhere

Nine trees were pushed to private repos on 30 August. B2311.1 was built on
2 September, after that pass, and its own handover note records the
consequence plainly: *"its own git repo, **no remote yet**, unlike its
siblings."*

`C:\Users\peter\b` is in neither Dropbox nor OneDrive. So the engine, the
bench, the parameter declarations, the manual and the design of the newest
object in the collection exist **on one disk, in one house** — the house that
is currently unreachable. The published archive is the only other copy of any
part of it, and a `.vst3` is not source.

### Problem two — the private repos are invisible to Claude

The nine private `brokild-*` repos are real, but a Claude session started from
this machine cannot see them:

```
add_repo peterboggild/brokild-artefact-b2311-67
  -> you don't have access to peterboggild/brokild-artefact-b2311-67
```

`BrokildApps` is the only repository in scope. So even sitting at the music PC,
no session can read, build or continue any of the nine — the work is backed up,
but it is not **workable** from here. That is the difference the title of this
file is about: having the work and being able to continue it are not the same
thing, and only one of them is currently true.

---

## 2 · Two things to do, in this order

### Grant the Claude GitHub App access to the `brokild-*` repos

This is the whole fix for problem two, and it is a settings change, not work:

1. **claude.ai → Settings → Connectors → GitHub** — reconnect, and when GitHub
   asks which repositories, either grant **all repositories** or tick the ten
   `brokild-*` ones alongside `BrokildApps`.
2. Start a session and confirm with `add_repo peterboggild/brokild-world-fx`.

After that, any session from this PC can clone, read and build any of the nine —
which is what "the project continues from the music PC" actually requires.

### Push B2311.1 the moment the work PC is reachable

Nothing on this machine can do this; the bytes are over there. When you get to
it, from that PC:

```powershell
cd C:\Users\peter\b\ArtefactB2311_1
type .gitignore          # must already list build/ and test/build/ — check first
git status               # commit anything loose, including the voicing rework
git remote -v            # expect nothing; that is the problem

gh repo create peterboggild/brokild-artefact-b2311-1 --private --source=. --push
#  or, without gh: create the empty private repo on github.com, then
#  git remote add origin https://github.com/peterboggild/brokild-artefact-b2311-1.git
#  git push -u origin main
```

Name it `brokild-artefact-b2311-1` — the ninth sibling is
`brokild-artefact-b2311-67`, and the convention is `brokild-<product>`. Then
grant the app access to it too.

**Check `.gitignore` before the first `add`.** `build/` is around 770 MB and
`test/build/` another 19 MB. Adding those once puts them in the history
permanently, and every sibling repo ignores exactly those two paths.

Also worth doing in the same pass: `install-fleet.ps1` in this repo's BWFX
mirror has no row for `Artefact B2311.1` — its table stops at `.67`. If the
work PC's copy has one, the mirror is stale and should be refreshed; if it does
not, `.1` was installed by hand, which is the documented way a duplicate bundle
with identical class ids appears. Either way it is worth knowing, and it is one
of the crash suspects — see `tools/fleet-triage/README.md`.

---

## 3 · What has been recovered in the meantime

`tools/fleet-triage/recover-panel.js` pulls a plug-in's panel back out of its
shipped binary. Every Brokild face is one HTML document compiled into the module
as a string, so the published archive carries a byte-exact copy of it. Run on
B2311.1 it returns **23 778 bytes**: the markup, the styling, the milled-plate
face, the dial drawing, the drag behaviour, the catalogue of 256 specimens, the
thermometer, the comments — and, most usefully, the whole message protocol
between page and processor.

That protocol is worth more than it sounds. It names every parameter the engine
declares, in its four groups:

| group | parameters |
|---|---|
| THE COUNTING | `ratelo` `ratehi` `couple` `dead` `leak` |
| THE IMPOSED PULSE | `division` `grip` `reach` `freehz` |
| THE SECTION | `projx` `projy` `projz` `projw` `spanx` `tilt` |
| THE BODY | `shape` `damp` `space` `sat` `level` |

and every message the processor sends up — phase, heat and rate fields for the
face, cascade size, the lean figure, the specimen number. Between that, the
design document with its as-built measurements, and `ARTEFACT-B2311-1-NOTES-FOR-THE-MANUAL.md`,
the C++ could be written again from a specification rather than from memory.

**It would still be a rewrite.** The engine, the bench's 13 checks, the
specimen table and the parameter declarations are machine code and do not come
back. Recovering the panel makes a rebuild survivable; it does not make one
unnecessary. Push the tree.

---

## 4 · The work that was in flight

The session that stopped was partway through a voicing rebuild, agreed with
Peter after he listened to the shipped object and said the sounds were
*"relatively uniformly tiny noise pops"*. That verdict was accepted, and the
diagnosis with it: the instrument only ever occupies the rhythm end of its own
thesis.

Four changes were planned, in this order:

1. **Open the rate range into audio.** Unit rates top out around 40 Hz, so
   cascades recur a few times a second and every event is a transient. Pitch
   needs recurrence in the hundreds. Until that range exists, "rhythm, pitch and
   colour are one quantity" is a claim the instrument cannot demonstrate.
2. **Scale a cascade's duration with its size.** A cascade currently resolves
   inside a fraction of one lattice step — about 1.1 ms — whether it involves
   five units or all 9216. A whole-lattice discharge is therefore not longer
   than a small one, only denser, which is why everything reads as a click.
3. **Give each unit its own kernel**, keyed to its position in the unseen axes,
   so a cascade sweeps colour as it travels and specimens differ in tone rather
   than only in timing.
4. **Take the kernel range far lower.** The two-pole sits around 1–3 kHz and
   bottoms out near 100 Hz. There is no deep register in the object at all.

A fifth was identified with them: the **parity sign-flip** cancels exactly the
coherent structure that would give a cascade a note, and works against (1).

None of this needs the work PC to design or to measure — only to ship. A
standalone prototype of the mechanism, with the numbers, is in
`ARTEFACT-B2311-1-VOICING.md`.
