# Fleet triage — the host is crashing, and what the shipped binaries actually are

Two tools and one verdict, written 2026-09-02 from the music PC after Ableton
began crashing at startup.

| | |
|---|---|
| `ableton-triage.ps1` | inventories what is really installed, finds duplicate class ids, compares installed against published, reads Ableton's own log, and can quarantine the fleet to prove the point in one restart |
| `install-from-archives.ps1` | installs the fleet from the published archives on a machine with no source tree, retiring by plug-in code rather than by name |
| `recover-panel.js` | pulls a plug-in's panel source back out of a shipped binary — the whole HTML document, byte-exact |

---

## The verdict on the published Artefact B2311.1

**The archive in `vst3-apps/proxima-centauri-b/` is not faulty.** Everything
checkable without Windows was checked, and it is intact:

| check | Artefact B2311.1 | its two siblings |
|---|---|---|
| bundle layout | `Contents/x86_64-win/Artefact B2311.1.vst3` | same |
| module | PE32+ DLL, x86-64 | same |
| VST3 entry points | `GetPluginFactory`, `InitDll`, `ExitDll` | same |
| bytes past the last PE section | **0** | 0 |
| linked | 2026-09-02 06:54:32 UTC | 08-30 |
| plug-in code | `BrkdAb01` | `BrkdAb22`, `BrkdAb67` |
| imports | identical set to both siblings | — |

The plug-in codes are what VST3 class ids are derived from, and the eleven
individual archives carry eleven distinct codes — `Ab01 Ab22 Ab67 BlkR BldR
Cwar EscR FmRk Hfry MrsW Psy2`. Nothing in B2311.1 can collide with any of them.

### But the published fleet does contain one collision, and it is not B2311.1

Checking only the individual archives says the fleet is clean. It is not. The
**collection** archive was checked too, and it ships a twelfth module:

| archive | bundle | linked | code |
|---|---|---|---|
| `Brokild-Collection-win64.zip` | `Photo-Synth2.vst3` | 2026-08-27 08:55 | `BrkdPsy2` |
| `Photo-Synth-VST3-win64.zip` | `Photo Synth.vst3` | 2026-08-30 09:59 | `BrkdPsy2` |

**Two bundle names, one plug-in code, two different builds three days apart.**
The product was renamed — `brokild_paths.h` carries `formerNames` for exactly
this reason — and the collection archive still carries the pre-rename copy.

Install both archives and two bundles with **identical VST3 class ids** sit in
the scan path under different names, and scan order decides which one the host
loads. `install-fleet.ps1` cannot catch it: it retires a loose copy of the same
*product name*, and it has a row for "Photo Synth", so a bundle called
`Photo-Synth2.vst3` is invisible to it.

That is a live hazard in what is published today, whether or not it is what is
crashing Ableton. `install-from-archives.ps1` resolves it by matching on the
**code** rather than the name: one module per code, newest wins, and anything
already on disk claiming that code is retired first.

Three more archives are stale rather than colliding — Black Rider, Blade Ruiner
and Escape Room are a day NEWER inside the collection archive than in their own
downloads. Same names, so they overwrite rather than duplicate, but a visitor
downloading the individual plug-in gets the older build.

The zero overlay figure is worth keeping in mind, because it is the one that
will trip you up later: `install-fleet.ps1` appends a few random bytes to an
**installed** bundle when Smart App Control blocks it, so **an installed file is
expected to differ from the published one**. Hashing the two against each other
proves nothing. `ableton-triage.ps1` compares the end of the last PE section and
the link timestamp instead — both survive the nudge — and reports the overlay
separately rather than as a fault.

So the crash is about **this machine's state**, not about what was published.
That is a much better position to be in, and it is testable in one restart.

## What is most likely, in order

1. **A bundle that is not the published build.** `install-fleet.ps1` installs
   from `build\...\Release\...`, so whatever was last compiled is what is on
   disk. The last session was midway through reworking B2311.1's voicing —
   opening the rate range into audio and scaling cascade duration with size —
   when it stopped being reachable. An unbenched engine may well be installed.
   `-Reference` answers this outright: point it at the downloaded archives and
   it says SAME BUILD or DIFFERENT BUILD per plug-in.

2. **A duplicate bundle.** One is published — the Photo Synth collision above,
   which is on this machine if both archives were ever installed. Separately,
   `Artefact B2311.1` **is not in `install-fleet.ps1`'s table** in this repo's BWFX mirror — the table stops at `.67`. So either the
   work PC's copy of that script is ahead of the mirror, or `.1` was installed
   by hand. Installing by hand is the documented way a second bundle carrying
   identical class ids appears, after which scan order decides which one the
   host loads. The script lists every bundle in every scan root and groups them
   by code.

3. **Something that is not the fleet at all.** Ableton scans VST3s in its own
   process, so any vendor's bad plug-in kills it at startup the same way. The
   quarantine test settles this in one restart, and it is worth doing before
   any more time goes on the theory that this is ours.

Two things were *checked and ruled out* rather than left as suspicions:

- **Not the missing patch folder.** B2311.1 is the one plug-in in the fleet
  that does not link `brokild_paths.h` at all — the strings `Brokild patches`,
  `.migrated` and `User presets` are present in `.22` and `.67` and absent from
  `.1`. There is no patch-folder code in it to fault, so the known "no user
  patch folder" gap cannot be the crash.
- **Not a corrupt download.** Zero overlay bytes and a clean section table on
  all three artefacts.

## Doing it

```powershell
# put the verified B2311.1 on this machine, from the archive rather than a build
powershell -ExecutionPolicy Bypass -File install-from-archives.ps1 `
    -Archives C:\Users\peter\Downloads -Only "Artefact B2311.1"

# read-only: what is installed, what collides, what Ableton's log says
powershell -ExecutionPolicy Bypass -File ableton-triage.ps1

# add: is what is installed the build that was published and tested?
powershell -ExecutionPolicy Bypass -File ableton-triage.ps1 -Reference C:\Users\peter\Downloads

# then the one restart that halves the problem
powershell -ExecutionPolicy Bypass -File ableton-triage.ps1 -Quarantine
#   Ableton starts  -> it is the fleet. Restore, put bundles back one at a
#                      time, restart between each.
#   Ableton crashes -> it is not the fleet. The log section is where to look.
powershell -ExecutionPolicy Bypass -File ableton-triage.ps1 -Restore
```

Quarantining what lives under `Program Files` needs an administrator prompt;
the script says so rather than failing quietly. Everything else runs as you.

## Recovering a panel from a binary

```sh
node recover-panel.js Artefact-B2311-1-win64.zip -o recovered/
```

Every Brokild face is one HTML document compiled into the module as a string,
so a shipped binary carries a byte-exact copy of it. Verified on all eleven:

| plug-in | recovered |
|---|---|
| Clone Wars | 1 573 950 bytes |
| Photo Synth | 369 016 |
| Artefact B2311.67 | 98 563 |
| Black Rider | 91 187 |
| Martian Gain | 87 912 |
| Full Metal Racket | 84 458 |
| Escape Room | 71 467 |
| Blade Ruiner | 69 140 |
| Artefact B2311.22 | 51 948 |
| Hairfryer | 32 001 |
| **Artefact B2311.1** | **23 778** |

All eleven end on their own closing tag. That last figure is the point of the
tool: see `FLEET-CONTINUITY.md` for why B2311.1's panel needed recovering, and
what is still only on one disk.

**What comes back is the panel and nothing else.** The C++ — the counting
lattice, the voicing, the parameter declarations, the bench — is machine code
and does not return. This is a partial restore and should be quoted as one.

One trap, recorded because it cost a round: the compiler lays the resource name
table down a few NULs after the document, so cutting the run at the first
padding appends `ui_html txplanet_png bwfxrack_js` to the end of the file. It
did that silently on two of the eleven. The tool now reads past short padding
and trims back to the last closing tag, and accepts `</script>` as an ending
because B2311.1's panel has no `<html>` element at all.
