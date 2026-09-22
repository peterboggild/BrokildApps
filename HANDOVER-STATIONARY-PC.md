# Handover — work that needs the stationary PC

Written 2026-09-22 from a cloud session on branch `claude/brokild-apps-gaps-p837x3`.

That session had a Linux container: it could build and run every offline bench,
render pages in Chromium and read the repo, but it had **no Windows, no JUCE,
no DAW and no PowerShell**. Everything below is what that ruled out, plus the
two decisions that need to be made by someone who knows what the JUCE setup is
licensed under.

Nothing here is urgent enough to rush. None of it is broken in a way a visitor
sees today, with one exception flagged as **stale**.

Kept current: rebased onto Thin Walls (2026-09-22) and every figure re-derived
from the repo at that point.

---

## What is already done (do not redo)

Branch `claude/brokild-apps-gaps-p837x3`, five commits, rebased onto `main` at
`6c5b1d3` (the Thin Walls publish), pushed, no PR opened:

| Commit | What |
|--------|------|
| `dc02a97` | The Clone Wars CI typo — a literal `\n` at `clone-wars.yml:38` that had made every run fail since 2026-08-28. Plus new CI for Legion and DSW. |
| `6b97002` | Open Graph / Twitter tags on every page, the `<noscript>` app list, a real Collection preview image. |
| `9bf89ff` | Hairfryer onto the front page, README/sitemap/robots/404, `tools/check-links.js`. |
| `e7cadb8` | Rebase onto the Rite of Passage publish; trimmed CI that had become duplicated. |
| `67ba96d` | This document, `contents.json`, and the Collection consistency check. |

Verified on Linux after the rebase:

```
rite-of-passage   506 checks   ALL CLEAR
BWFX              409 checks   ALL CLEAR
Legion            113 checks   ALL CLEAR
clone-wars        render test + DSP audit, ALL PASS
DSW               host and both experiment bundles build
site              31 pages, 17 apps, no dead targets
```

**This branch is not merged.** Everything below assumes it is merged first, or
at least that `tools/check-links.js` and `tools/sync-site.js` exist.

---

## 1. The Collection is stale — decide, then re-cut

**STALE. This is the only item a visitor can see today.**

**Two** plugins have been published since the archive was cut, and it contains
neither:

| Published | Plugin | Kind |
|-----------|--------|------|
| 2026-09-19 | Rite of Passage | effect |
| 2026-09-22 | Thin Walls | effect |

The zip still holds ten, and the site says "all ten" and "eight instruments and
two effects" in thirteen places across `vst3-apps/collection/app.json`,
`vst3-apps/collection/index.html` and `index.html`. A re-cut that takes both
would make it **twelve — eight instruments and four effects**.

The gap widened by one plugin in the three days it took to write this section,
which is the argument for the check below rather than for a reminder.

This is the failure the repo has already had twice — the commit that cut the
current archive says so itself:

> A collection is a DERIVED artefact: it goes stale the moment the fleet gains
> or renames a plugin, and this one has done so twice before without anyone
> noticing. Re-cut it whenever the fleet changes.

### It is now checked

`vst3-apps/collection/contents.json` is new. It declares which plugins are in
the collection and which are deliberately out, and `tools/check-links.js` fails
if:

- a plugin ships a `-VST3-win64.zip` but appears in neither list;
- the archive on disk disagrees with `includes`;
- the count in `claims` disagrees with the archive;
- the prose on any of the three pages quotes a different number;
- `BrokildWorldFX/tools/build-collection-zip.ps1` has a `$plugins` list that
  disagrees with `contents.json` — membership lives in three places and that
  script is the one this checker cannot run.

Right now the check **passes**, because `contents.json` records both Rite of
Passage and Thin Walls as explicit pending exclusions. That is the honest
current state, not a fix.

It already worked once unprompted: Thin Walls was published while this branch
sat unmerged, and the moment it was rebased on, the check named it —
*"thin-walls ships a zip but is neither included nor excluded — decide which."*
Nobody had to remember.

### The decision

Either is fine; both need recording. Settle both plugins in one re-cut.

**A — take both in (twelve).** On the PC:

1. Add `rite-of-passage` and `thin-walls` to `$plugins` in
   `BrokildWorldFX/tools/build-collection-zip.ps1` (the list is newest-first,
   the way the front page reads).
2. Run that script. It takes each plugin's **published zip**, not a build tree,
   and hash-verifies every staged file against the source zip and again after
   re-extracting the finished archive. Do not shortcut that.
3. In `contents.json`: move both from `excludes` to `includes`, set
   `claims.count` to `12` and `claims.phrase` to what is then true — eight
   instruments and **four** effects.
4. Update the prose. `node tools/check-links.js` will name every file still
   saying "ten", including the one with a capital E that a previous patch
   walked past.
5. Check `BrokildWorldFX/tools/build-collection-page.js` too — it builds the
   cards on the collection page and has its own idea of the fleet.

**B — leave one or both out.** Replace the `PENDING A DECISION` text in
`contents.json` with the actual reason. Nothing else changes and the check goes
on passing.

Mixing is fine — include one, exclude the other — as long as `claims.count`
and the prose follow. The check will say if they do not.

> The builder hardcodes `c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps`.
> Worth making that a parameter while you are in there.

**Do §4 first.** A twelve-plugin Collection zip will be ~105 MB, and the repo
has already crossed 1 GB (see §4). Committing another copy makes the worst
offender worse — the Collection is already 538 MB of history on its own.
Attach the re-cut archive to a GitHub Release and point the download button at
it, rather than committing it. That is the same amount of work and it is the
last time the question comes up.

---

## 2. Legion is finished and unpublished

`vocal-harmonizer/` has a full engine, a 113-check bench that passes, a design
doc (`LEGION-DESIGN.md`) and a `shoteditor.cpp` that renders the real editor to
a PNG. It has no landing page, no zip, no `app.json` and no card. It is the
only finished plugin in the repo that nobody can download.

Publishing it needs Windows + JUCE:

1. Build the VST3 and standalone on the PC (or add a release workflow modelled
   on `.github/workflows/rite-of-passage.yml`, which already does exactly this
   for a JUCE plugin and is the newer of the two patterns).
2. Follow `BrokildWorldFX/tools/RELEASE-CHECKLIST.md` — it exists precisely for
   this and lists the traps that have each cost a round.
3. Create `vst3-apps/legion/` with `index.html`, `app.json`, the zip and the
   manual; add a 16:9 preview to `assets/app-previews/`.
4. Add `vst3-apps/legion` to `manifest.json`.
5. Run `node tools/sync-site.js` — this writes the Open Graph tags, the README
   row, the sitemap entry and the noscript link. Skipping it fails CI.
6. Decide whether it joins the Collection (§1) — `check-links.js` will insist.

Once Legion has its own release workflow, move its bench out of
`.github/workflows/engines.yml` into it; that file's header says so.

---

## 3. LICENSE — needs to be decided by someone who knows the JUCE terms

There is no LICENSE file. Ten plugins are given away with no stated terms,
which legally defaults to all rights reserved — an odd fit for *"given away
because there is no reason not to."*

The constraint, checked against `LICENSE.md` at tag `8.0.6` in the JUCE repo
rather than from memory:

> The JUCE Framework modules are dual-licensed under the **AGPLv3** and the
> commercial JUCE licence.

Note **AGPLv3**, not GPLv3 — JUCE 8 is the stronger copyleft. `rite-of-passage`,
`vocal-harmonizer` and `vst3-apps/clone-wars/plugin` all fetch JUCE 8.0.6 and
link it. If they are built under the free option, the plugins inherit AGPLv3
and publishing their source — which this repo does — means that is the licence.

If there is a commercial JUCE licence on the PC, none of that propagates and
the choice is free. **That is the fact this decision is waiting on.**

Whatever is decided, note that these link no JUCE at all and can be licensed
separately and permissively:

- `health-apps/sleep-noise`, `music-apps/photo-synth` — browser apps
- `dsw/` — headless host, Threads and optional OpenMP only
- `BrokildWorldFX/` — plain C++17
- `tools/` — Node

A split (AGPLv3 for the JUCE-linked plugins, MIT for the rest) is the usual
answer and keeps the browser apps and DSW reusable.

---

## 4. Repo size — just over 1 GiB, now past GitHub's line

| | |
|---|---|
| Working tree | 359 MB |
| `.git` packed | 989 MiB + 43 MiB loose |
| `.git` on disk | **1.1 GB** |
| Binary blobs in history | 380 blobs, ~1.24 GiB |

It was 695 MiB at the start of that session. Rite of Passage added most of the
rest and Thin Walls the last ~11 MB, which took it **past the 1 GB** GitHub
asks repositories to stay under. It is not a hard limit and nothing breaks
today, but the repo is now the thing the guidance is about.

Where it actually went, by total bytes across all revisions in history:

```
  538 MB   7 revs  vst3-apps/collection/Brokild-Collection-win64.zip
  142 MB   9 revs  rite-of-passage/docs/panel-midway.png
   59 MB   5 revs  vst3-apps/proxima-centauri-b/Artefact-B2311-67-win64.zip
   37 MB   5 revs  vst3-apps/brain-scan/Brain-Scan-VST3-win64.zip
   34 MB   5 revs  vst3-apps/black-rider/Black-Rider-VST3-win64.zip
```

**The Collection is over half the problem on its own.** It is the largest file
in the repo *and* the most frequently re-cut, because by design it changes
every time any plugin does — seven revisions at 76–93 MB each. It is also the
one file that is pure duplication: every byte in it already exists in the repo
inside the individual plugin zips.

Second is a screenshot: nine revisions of one panel PNG, 142 MB, most of them
the ~41 MB version before it was replaced by a 1 MB one. The small version is
what is checked out; all nine are still in every clone.

So, in order of value:

1. **Stop committing the Collection zip.** Attach it to a GitHub Release
   instead and point the download button at the asset URL. One change, removes
   the biggest and fastest-growing item, and needs no history rewrite. Do this
   before the next re-cut (§1) rather than after.
2. **Future plugin releases as Release assets too.** Same move for the other
   zips; the two release workflows upload instead of committing. Stops the
   growth for good.
3. **Compress what goes into `docs/`.** A 41 MB PNG screenshot is a 1 MB JPEG
   with no visible loss — the replacement already proves it. Worth a line in
   `RELEASE-CHECKLIST.md`.
4. **Rewrite history** with `git-filter-repo`. Dropping just the Collection and
   that PNG would reclaim ~680 MB of the 989 MiB. It also invalidates every
   existing clone and every commit SHA quoted in the design docs. Only if 1–3
   are not enough, and not casually.

None of this needs the PC — it is a decision, not a capability.

---

## 5. Smaller things, none blocking

- **Eleven plugins ship a zip with no release workflow.** Only `clone-wars` and
  `rite-of-passage` have one; the other eleven were built and packaged by hand on
  the PC. Adding workflows modelled on `rite-of-passage.yml` would move that
  onto free Windows runners, and is the single biggest reduction in PC-only
  work available. It does not need the PC to write — only to verify the first
  run of each.
- **The Windows half of Clone Wars CI has not run since 2026-08-28.**
  `windows-build` has `needs: engine-test`, and `engine-test` was the job
  failing on the typo, so the Windows build and the release-zip step have been
  unreachable for four weeks. The fix is on the branch, but nobody has seen
  that job succeed in a month and the runner images have moved since — the
  workflow's own comment notes the 2026 images dropped VS 2022. **Expect it to
  need a round.** Trigger it with a manual dispatch and watch, rather than
  assuming green.
- **Hairfryer's `app.json` says `"status": "live"` while its name says
  "(in development)".** Harmless, but the card shows a live badge on a plugin
  the copy describes as unfinished. Decide which is true.
- **15 `patch-*.js` scratch files** sit in `BrokildWorldFX/test/` — one-off
  fixups from past debugging sessions, committed. Left alone deliberately: they
  may still be wanted. Worth a look when someone has context on them.
- **The two browser apps have no link back to the front page.** Deliberate if
  they are meant to be full-screen apps; worth a thought if not.
- **No changelog.** A visitor who downloaded build 260826.5 has no way to know
  260919.2 exists. The per-plugin build numbers are in each `app.json` `note`,
  so a generated page is cheap if it is wanted.

---

## The one rule that will bite first

Adding or renaming an app now means running:

```sh
node tools/sync-site.js
```

It regenerates the Open Graph tags, the README table, the sitemap and the
front page's noscript list from `manifest.json` and the `app.json` files. CI
(`.github/workflows/site.yml`) fails if it has not been run.

`node tools/sync-site.js --check` reports what is stale without writing, and
`node tools/check-links.js` checks every local link, every preview image, every
`app.json` tag, and the Collection consistency described in §1.

Both are plain Node with no dependencies and run anywhere, PC included.

The published base URL is the `SITE` constant at the top of `sync-site.js`.
There is no `CNAME` in the repo, so the site serves from the default
project-pages URL; if a custom domain is ever added, change that line and
re-run.
