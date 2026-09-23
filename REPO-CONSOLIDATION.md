# Repo consolidation and the three collections

*The plan, and every decision behind it. Written 2026-09-23 after Peter asked
for an opinion on two things: how the repo can hold the source as well as the
site, and how the plugins should be divided between collections.*

Each stage below is verifiable on its own and is approved before the next one
starts. Where a number appears it was measured, not estimated, unless it says
otherwise.

---

## 1. Why this is worth doing

The fleet is nineteen published plugins and one shared library, and it is kept
in three incompatible ways at once:

- **source in `b\` plus a private repo plus a site folder here** — twelve plugins
- **source in this repo under the site folder** — Clone Wars only, because CI builds it
- **source in this repo at the root with no site folder** — Rite of Passage, Legion

Three schemes is what let Clone Wars drift 41 lines from its own published
source without anything complaining, and it is why `b\BrokildWorldFX` has to be
copied into this repo by hand after every commit.

**The source is not what makes this repo heavy.** Measured:

| | |
|---|---|
| All 17 plugin trees, source as text | 9.9 MB |
| The same trees with decal art and plates | 109 MB |
| Plugin zips tracked in this repo today | 263 MB |
| This repo's `.git` directory | 1.9 GB |

So bringing every tree in costs about ten megabytes of code. The gigabyte and a
half is built binaries, and every re-cut since August is still in the history.
The two halves of the job are therefore independent: **source in, binaries out.**

---

## 2. The decisions

| question | decision |
|---|---|
| Licence | **AGPLv3** for the plugins, the free JUCE 8 option. Subtrees that link no JUCE are also offered under MIT. |
| Layout | The Clone Wars shape, everywhere: `vst3-apps/<slug>/` holds the page, `app.json`, `img/`, the manual and `plugin/`. |
| What moves in | Code, documents and **decal art**. Not `dist/`, not `build/`, not manual screenshots, which can be re-shot. |
| Downloads | Per-plugin **GitHub release assets**, declared in `app.json` with url, bytes and sha256, exactly as `contents.json` already does for the collection. |
| Manuals | **Stay in the repo.** They are what the landing pages link to, and a manual changes far less often than a binary. |
| History | **Stripped.** Old zip blobs removed with `git filter-repo`; every commit hash changes and existing clones must be re-cloned. |
| `b\` | Becomes **one checkout** of this repo. Build output stays outside Dropbox where it already is. |
| Old private repos | **Archived** on GitHub, not deleted. Read-only, visible, nothing can be pushed to the wrong place. |
| BrokildWorldFX | The copy in this repo becomes **the only one**. The mirror step disappears. |
| CI | **One plugin first**, proven, then the rest as each is next touched. |

Expected result: a clone of roughly **60 MB**, holding every plugin's source,
every landing page and every manual, with the binaries hanging off releases.

---

## 3. The target layout

```
vst3-apps/<slug>/
    index.html                  the landing page
    app.json                    the card, and the download's url + bytes + sha256
    img/                        plates for the page
    <Name>-Manual.pdf           the handbook
    plugin/
        CMakeLists.txt
        Source/                 engine, processor, editor, ui
        assets/decals/          the art the panel references by name
        test/                   bench, host test, panel probe, patch scripts
        tools/                  cdp.js, live.ps1, shoot-manual.ps1, ...
        docs/                   design record, manual source, PLATES.md
```

Two deliberate exceptions:

- **`vst3-apps/proxima-centauri-b/`** keeps the shared page, the Field Findings
  document and the audio, and holds `b2311-1/`, `b2311-22/`, `b2311-67/` and
  `b2311-104/` beside them. The four were built to share no mechanisms, but they
  share a fiction, and the survey is what makes them read as one find.
- **`BrokildWorldFX/`** stays at the repo root. It is a library, not a plugin,
  and seventeen `CMakeLists.txt` already point at it through `BWFX_DIR`.

---

## 4. The three collections

**They wait for Thirty Thousand Years**, which another session is still
building. Nothing else in this plan waits for it.

### Brokild Collection — eleven

Instruments: 1984, Black Rider, Blade Ruiner, Clone Wars, Escape Room,
Full Metal Racket, Thirty Thousand Years.
Effects: Battlestar Overdrive, Martian Gain, Thin Walls, Rite of Passage.

Seven instruments and four effects. Every folder holds a bundle, an application
and a handbook, which is the promise that makes the archive worth taking whole.
**Rite of Passage needs a standalone and a manual before it qualifies**; it
ships a bundle and a GUIDE.md today.

### Experimental Collection — five

Brain Scan, High Tide, Photo Synth, Hairfryer, Legion.

**The framing is purpose, not polish.** These exist to ask a question about how
sound can be made, and each is a thesis: a synth played by photographs, a synth
that reads a scanned volume, a ball rolling on terrain, surgery on a singing
voice, one voice becoming four. Not products, and the source is in the repo for
anyone who wants to take one further.

Calling them unfinished would be inaccurate and would undersell them. Photo
Synth carries the largest manual in the fleet at 38 pages, and Brain Scan and
High Tide both ship full handbooks. What they are not is *useful in the way a
compressor is useful*, and that is the thing to say.

The shape rule is relaxed here: a bundle plus whatever documentation exists is
enough. **Legion gets a landing page and a front-page card** so it is findable
outside the archive, carrying the same in-development note Hairfryer's does.

### Proxima Centauri B findings — four

Artefact B2311.1, .22, .67 and .104, as one combined download of about 33 MB,
so the three collections are the same kind of thing. The four already share one
findings document, so the archive has an obvious shape.

### Why this makes the site and the disk agree

`install-fleet.ps1` already creates three folders: `Brokild collection`,
`Proxima Centauri B findings` and `Experimental`. After this, the collections
on the site and the folders on disk are the same idea, and moving Photo Synth,
High Tide, Brain Scan and Legion into the Experimental group is the whole of
the installer change.

---

## 5. The stages

**Stage 0 — backups. DONE 2026-09-23.**
Artefact B2311.1 and B2311.104 had no remote at all and sat on a disk in
neither Dropbox nor OneDrive. Both pushed to new private repos and verified
from the remote's own tree: 28 and 37 blobs, heads matching, both private.
Thirty Thousand Years gets the same treatment the moment the other session
lets go of it — **it is the last tree in the fleet with no copy anywhere.**

**Stage 1 — the licence and the layout. DONE 2026-09-23, bar the build sweep.**

`LICENSE` is the AGPLv3 text fetched from gnu.org and checked section by
section; `LICENSING.md` says what that means in practice, which subtrees are
also MIT, and that the artwork is reserved.

Eighteen trees now live beside their pages. Sixteen came in from `b\` through
`tools/migrate-plugin.js`; Rite of Passage and Legion were already here at the
repository root and moved with `git mv`, so their history came with them.
Thirty Thousand Years is the one tree still outside, because another session
is building it.

| | |
|---|---|
| Plug-ins building from the repo | 17 of 17 |
| Benches passing from the repo | 18 of 18 |
| Tracked source brought in | about 90 MB |
| Files left behind per tree | built output, bench renders, demo masters, loose screenshots |
| Tool scripts repointed at their own tree | 70 |
| Live absolute paths left | 5, every one deliberate |

**The tools, all kept:**

- `tools/migrate-plugin.js` takes a tree from `git archive HEAD`, drops the
  built artefacts, and repoints CMake. It refuses a dirty source tree, because
  archiving HEAD would migrate silently without the edits you are looking at.
  `--dest=` places a plug-in anywhere, which is what the Artefacts need.
- `tools/repoint-paths.js` rewrites a tree's own tooling to find itself, and
  parses every `.js` it touches, putting the file back if the rewrite broke it.
- `tools/verify-migrated.js` configures or builds every plug-in in the repo.
- `tools/run-exe-past-sac.ps1` runs a freshly linked bench, nudging a copy's
  hash past Smart App Control rather than failing on a dice roll.

**Four things that cost a round each, worth not repeating:**

1. **PowerShell binds a `param()` block's defaults before the body runs**, so a
   `$BrokildRoot` assigned in the body is empty exactly where these scripts use
   it most: `"$BrokildRoot\test\jobs.json"` binds as `"\test\jobs.json"`. Proven
   with a one-line repro before the fix. `$PSScriptRoot` works during binding
   and is the only form correct in both places. How deep the script sits decides
   how many hops back, so `docs/manual/` gets two where `tools/` gets one.
2. **`fs.rmSync(dir, {recursive:true})` inside Dropbox** deletes the contents
   and then throws EPERM on the folder, leaving a half-migrated tree. Walk the
   children instead. This was already written down and was walked into anyway.
3. **`juceaide` blocked at configure time is Smart App Control**, not a broken
   tree: it cleared on the second attempt every time. The gate retries and says
   that it did, rather than crying wolf on a good tree.
4. **A blanket path replacement cannot catch a relative path that was correct
   before the move**, because there is no old string to search for. It happened
   three times: the Rite of Passage workflow copying a README relative to its
   working directory, the same plug-in's BENCH build file reaching BWFX two
   levels up, and three of B2311.104's site checks including a header relative
   to the source file. **Only running the benches found the last two.** A build
   sweep proves the plug-ins; it says nothing about the tests beside them.

**And the sweep found one fault that had nothing to do with the move.** Two
Clone Wars scratch probes read `cw::vfEnvA`, which the 260826.5 core removed on
2026-08-26. They have not compiled for a month, and nothing noticed because CI
builds the plug-in and `render_test` and never the probe targets. They are kept,
excluded from the default build, with the date and the reason on them. **A
target nothing ever builds is a target that rots silently.**

**Stage 2 — binaries out, history stripped.**
Zips to per-plugin release assets, `app.json` declaring url, bytes and sha256.
`tools/check-links.js` extended to verify every declared download the way it
already verifies the collection's. Then `git filter-repo` over the old zip
blobs, and a fresh clone measured.

**Stage 3 — Rite of Passage gets a standalone and a manual.**
The standalone is a CMake format flag and a rebuild. GUIDE.md is already 19 KB
of good prose, so the manual is typesetting it into the house landscape format
and shooting plates from the live panel through the usual overflow gate.

**Stage 4 — Legion gets a page and a card.**
Same shell as the others, built from its README and a snapshot of the real
editor. It has no WebView and no standalone, so the plate comes from
`legionshot`, the console harness that renders the JUCE editor directly.

**Stage 5 — the three collections.** *Blocked on Thirty Thousand Years.*
`contents.json` becomes three files, and `build-collection-zip.ps1` takes the
collection as an argument and reads them, so there is one code path and three
data files rather than three scripts. Pages, `manifest.json`, `app.json` counts
and the installer groups all follow.

**Stage 6 — `b\` becomes one checkout, old repos archived.**

**Stage 7 — CI.** One plugin proven, then the rest as each is next touched.

---

## 6. What could go wrong

- **A history rewrite invalidates every clone.** Affects one machine and CI, so
  it is cheap here, but it has to be done once and deliberately rather than
  discovered later.
- **Another session may hold a tree.** Thirty Thousand Years is held right now;
  Artefact B2311.67 has been in the past. `ListAgents` does not show those
  sessions, so stat-and-re-stat before touching any tree.
- **A moved tree can build and still be wrong.** Every plugin's gates run from
  the new location before its move is accepted, and the installed bundle is
  probed, not the build output.
- **AGPLv3 obliges the source to be offered with the binary.** Every release
  asset's README carries the notice and a link to the folder it was built from.
