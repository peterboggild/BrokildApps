# Artefact B2311.67 — who does what

Two Claude sessions share this fleet. This file is the contract between them,
so neither has to hold it in context. Written 2026-08-30 by the session that
built B2311.22 and owns the fleet's release machinery.

**The split is by KIND OF WORK, not by plugin.** One session holds .67's
design — the quasiperiodic thesis, the ℤ⁴ cut, the panel, the decals, the
provenance. The other holds the fleet's release conventions — the installer,
the name check, the manual pipeline, the landing-page shell, the manifest, the
per-plugin zip structures, Smart App Control. Moving either body of knowledge
between sessions costs a full re-acquisition and buys nothing.

---

## The .67 session: what to do

### 1. Put the tree under version control — before anything else

`C:\Users\peter\b\ArtefactB2311_67` has no `.git`. It is the only synth in the
fleet in that state, and until it has one there is no restore point.

```
cd C:\Users\peter\b\ArtefactB2311_67
printf 'build/\ntest/build/\n' > .gitignore     # BEFORE the first add
git init && git add . && git commit -m "Artefact B2311.67, as built"
```

**The `.gitignore` must exist before the first `git add`.** `build/` is 770 MB
and `test/build/` a further 19 MB; adding them once puts them in history
permanently. Every sibling repo ignores exactly those two paths.

A source-only snapshot was taken as insurance and sits at
`C:\Users\peter\b\_backups\ArtefactB2311_67-20260830-125035` (23 files, 3.7 MB).
Delete it once the repo exists.

### 2. Finish the instrument, and write the manual's SUBSTANCE

The engine, panel, BWFX rack, macros, decals, provenance card and bench are
done. What is left that only this session can do well is the manual's content —
what the instrument is, what each control does, and why it sounds as it does.
Write it into `docs/manual/manual.html`; the PDF build is mechanical and can be
done by either session.

### 3. Then FREEZE and say so

Say "design frozen at build `<id>`" and stop editing the tree. The release pass
touches the same files, and two sessions in one tree is how the duplicate
bundle happened.

### 4. Either hand over, or run the release pass yourself

Everything needed is written down in
`C:\Users\peter\b\BrokildWorldFX\tools\RELEASE-CHECKLIST.md` — version control,
name consistency, install-and-prove-it-loads, manual, landing page, `app.json`
plus manifest entry, zip, publish, record. It carries the traps that have each
cost a debugging round. Running it yourself is fine and avoids a handoff
entirely; handing over is fine too. Just say which.

---

## Facts worth having

- **.22's bench is 1831 checks, not 1743.** That number grew when the
  interlocutor landed (a second listening body, a nonlinear membrane, Hebbian
  plasticity). It will be wrong if quoted from the older figure.
- **.22's tree is committed and clean** at `520e91f`, including this session's
  provenance-card work — build 260830.2, verified ALL CLEAR at 1831 checks
  after a full rebuild, and reinstalled. Nothing is left loose there.
- **The plugin codes are `Ab22` and `Ab67`**, confirmed in the built binaries.
  They cannot collide.
- **Install with `BrokildWorldFX\tools\install-fleet.ps1`**, never by hand. It
  places bundles in the right group folder, retires any loose copy at the VST3
  root, and probes the INSTALLED file past Smart App Control. `.67` is already
  in its table.
- **Stage by explicit path in BrokildApps — never `git add -A`.** Both sessions
  have untracked work in that repo; `-A` files the other's work under your
  commit message. It has already happened once.
- **Clone Wars' zip is never hand-built** — a `workflow_dispatch` of
  `clone-wars.yml` builds and commits it, so the shipped binary always matches
  a green CI run.

## Where the plugins live

| folder | contents |
|---|---|
| `Proxima Centauri B findings` | Artefact B2311.22, Artefact B2311.67 |
| `Brokild collection` | Black Rider, Blade Ruiner, Clone Wars, Escape Room, Full Metal Racket, Martian Gain, Photo Synth |
| `Experimental` | Hairfryer |

In both `C:\Program Files\Common Files\VST3\Brokild\` and
`C:\Users\peter\AudioDev\VST3\`. Patch folders are
`Documents\Brokild patches\<PRODUCT_NAME>`, one per plugin, name matching the
product exactly — `tools/check-names.js` enforces it.

---

## Update, 2026-08-30 afternoon — where everything lives now

Read this before your next commit or install; two things changed under you.

### 1. Your tree already has a remote. Use it, do not add another.

`C:\Users\peter\b\ArtefactB2311_67` now pushes to the PRIVATE repo
**`peterboggild/brokild-artefact-b2311-67`**. `origin` is configured and your
first commit is pushed. From here just `git commit` and `git push` — that IS
the backup, and it is the only one: `C:\Users\peter\b` is in neither Dropbox
nor OneDrive, so anything uncommitted exists on one disk and nowhere else.

All ten source trees were pushed to private repos this afternoon, named
`brokild-<product>`: artefact-b2311-22, artefact-b2311-67, black-rider,
blade-ruiner, escape-room, full-metal-racket, hairfryer, martian-gain,
photo-synth, world-fx. Do not create more; do not rename them.

When .67 starts producing a zip, add `dist/` to its `.gitignore` alongside
`build/` and `test/build/` — built artefacts belong on the website, not in the
source history.

### 2. NEVER keep a second copy of a tree

This is the failure mode that cost real work today, so it is worth being
concrete about. Clone Wars had its canonical source in the BrokildApps repo
and a working copy at `b\CloneWars`. They drifted: a panel fix made in the
canonical tree was never copied across, the local copy sat 41 lines stale, and
a CI build would have shipped the old file without complaining. The duplicate
is now retired to `b\_retired\`, Clone Wars is edited directly in
`BrokildApps/vst3-apps/clone-wars/`, and it builds with the build directory
outside Dropbox (see its `LOCAL-BUILD.md`).

So: **one tree per plugin, edited in place.** If you need a scratch copy for
an experiment, put it under `b\_retired\` or the scratchpad — never beside the
real one where a build script might find it.

### 3. Installing

Only ever `powershell -File C:\Users\peter\b\BrokildWorldFX\tools\install-fleet.ps1 -Only "Artefact B2311.67"`.
It puts the bundle in `Proxima Centauri B findings`, retires any loose copy at
the VST3 root, and probes the INSTALLED file past Smart App Control. Installing
by hand is how a duplicate bundle with identical class ids appears, and then
scan order decides which one your DAW loads.

### 4. Committing to BrokildApps

**Stage by explicit path. Never `git add -A`.** Both sessions have untracked
work in that repo; `-A` files the other's work under your commit message. It
has happened once already, in both directions of surprise.

### 5. Finishing .67

`C:\Users\peter\b\BrokildWorldFX\tools\RELEASE-CHECKLIST.md` is the whole
path from 'it works' to 'it is published', in dependency order, with the traps
that have each cost a debugging round. Run it yourself or hand over — either is
fine, just say which, and freeze the tree first if you hand over.
