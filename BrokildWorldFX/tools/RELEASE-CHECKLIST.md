# Shipping a Brokild plugin — the release pass

Everything between "the instrument works" and "it is published". Written down
so it is not held in one session's head: whoever finishes a synth can run this
without having lived through the previous nine.

Order matters — each step assumes the one above it.

---

## 0. Version control FIRST

A synth tree with no `.git` is hours of work with no restore point.

```
cd C:\Users\peter\b\<Plugin>
printf 'build/\ntest/build/\n' > .gitignore     # BEFORE the first add
git init && git add . && git commit -m "..."
```

**Write `.gitignore` before the first `git add`, never after.** A build tree is
several hundred megabytes (Artefact .67's was 789 MB); adding it once puts it
in history permanently.

## 1. Names must agree

```
node C:\Users\peter\b\BrokildWorldFX\tools\check-names.js
```

Asserts every plugin's patch folder is named exactly its `PRODUCT_NAME`, and
that no folder in `Documents\Brokild patches` is unclaimed. Add the new plugin
to the `DIRS` list in that script and to `install-fleet.ps1`'s `$plugins`
table, with the right group folder:

| folder | for |
|---|---|
| `Proxima Centauri B findings` | the artefacts |
| `Brokild collection` | the shipped instruments |
| `Experimental` | unlisted / in progress |

**Confirm `PLUGIN_CODE` is unique across the fleet.** Two plugins sharing a
code share a VST3 class id, and one silently replaces the other. Check the
built binary, not just the source: `grep -a -o` the code out of the DLL.

## 2. Install and prove it loads

```
powershell -File C:\Users\peter\b\BrokildWorldFX\tools\install-fleet.ps1 -Only "<Product Name>"
```

Probes the INSTALLED file at its own path and nudges past Smart App Control.
Never probe the build output: the SAC verdict is per copy, and it can change
after a successful install.

## 3. The manual

Source is `docs/manual/manual.html` in the plugin's tree.

```
powershell -File C:\Users\peter\b\PhotoSynth\devkit\tools\make-pdf.ps1 `
    -Html <...>\docs\manual\manual.html -Pdf <...>\<Product-Name>-Manual.pdf
```

- Plates are shot from the LIVE plugin over CDP, not from the raw ui.html —
  a page rendered outside the plugin has no state and lies about the panel.
- **Take the page count from the built PDF**, never carry it forward; the
  notes have drifted before (a 15-page claim on a 25-page manual).
- Keep the copy in `b/<Plugin>/docs/manual/` in sync with the website copy.
  Clone Wars had two out-of-date copies for exactly this reason.

## 4. The landing page

Build from a sibling's shell with the palette swapped — Black Rider's is the
usual donor (`tools/build-landing.js` in the FMR tree is the worked example).

**THE SPLIT-HEADING TRAP.** Covers are written as
`<h1>Photo-Synth <span>2</span></h1>`. A text replacement walks straight past
that and grep then reports "clean" while the old name is still on screen. It
has bitten four times. **Verify a name by RENDERING the page and reading the
heading back**, over http for anything that fetches JSON at runtime.

## 5. `app.json` + the manifest

`vst3-apps/<slug>/app.json`:

```json
{ "slug": "...", "name": "... (VST3)", "description": "...", "status": "live",
  "url": "vst3-apps/<slug>/index.html", "icon": "device",
  "cta": "Get the plugin →", "preview": "assets/app-previews/<slug>.jpg",
  "note": "Windows VST3 · build <id> · free download · manual included.",
  "tags": ["vst3", "synth"] }
```

Then add `"vst3-apps/<slug>"` to `manifest.json`. The front page builds its
cards from these at runtime — a plugin missing from the manifest is invisible.

## 6. The zip

**The structure differs per plugin** — copy the shipped one rather than
inventing it (`unzip -l` the old zip first). Black Rider / Blade Ruiner /
Hairfryer / Martian Gain have an inner folder; Escape Room and Photo Synth are
flat. Include the bundle, the standalone, the manual and README.txt.

Skip `moduleinfo.json` if the local build left a zero-byte one — SAC blocks
the helper that writes it, and an empty file is worse than none.

**Clone Wars is the exception: never hand-build its zip.** A manual
`workflow_dispatch` of `clone-wars.yml` builds and commits it, so the shipped
binary always matches a green CI run.

## 7. Publish

Preview image to `assets/app-previews/<slug>.jpg`. Then commit and push —
**staging by explicit path, never `git add -A`**: another session may have
untracked work in that repo, and `-A` files it under your commit message.

## 8. Record it

Append what was measured to `CLAUDE.md` — not what was done, but what the
numbers were and what surprised you. That file is the only thing that survives
a compaction.
