# BrokildApps

A collection of small browser-based apps and experiments by Peter Bøggild.
Everything is a self-contained static page — no build step, no server, nothing
uploaded. The front page (`index.html`) lists all apps and is published with
GitHub Pages.

## Apps

| App | Folder | What it does |
|-----|--------|--------------|
| DSW | [`dsw`](dsw) | **Digital Science Workstation** (build from source): a native host that runs C++/HTML digital-experiment plugins the way a DAW runs VST3s — drop a bundle in `plugins/`, the browser is the GUI, the math runs native. Ships with Gray–Scott and double-slit wave-tank experiments. |
| Photo-Synth 2 | [`vst3-apps/photo-synth-2`](vst3-apps/photo-synth-2) | **Windows VST3 plugin** (download): play the colours of four photographs inside your DAW. Ships with a PDF manual, and with the HTML→VST3 developer kit that explains how it was built. |
| Photo Synth | [`music-apps/photo-synth`](music-apps/photo-synth) | Play two photos: colour becomes tone on one, filter on the other. |
| Sleep Noise App | [`health-apps/sleep-noise`](health-apps/sleep-noise) | Private browser-generated sleep noise with timers, fades and blackout mode. |

## How the front page works

The front page is data-driven, so adding an app never means editing HTML:

1. Put the app in its own folder (e.g. `music-apps/my-app/`) with an
   `index.html` inside.
2. Add an `app.json` to that folder with the card's metadata:

   ```json
   {
     "slug": "my-app",
     "name": "My App",
     "description": "One or two sentences for the card.",
     "status": "live",
     "url": "music-apps/my-app/index.html",
     "icon": "wave",
     "preview": "assets/app-previews/my-app.svg",
     "tags": ["music", "phone"]
   }
   ```

   `status` is `"live"` or `"soon"`; `preview` (a 16:9 image in
   `assets/app-previews/`), `note` and `cta` are optional. `icon` picks one of the
   inline icons in `index.html` (`wave`, `note`, `camera`, `keys`, `device`,
   `moon`, `chart`, `more`), and `tags` reference label ids from `manifest.json`.
   `cta` replaces the card's default "Open app →" link text — used by downloadable
   plugins, which are not "opened" the way a browser app is.

### Downloadable plugins

Not everything here runs in the browser. `vst3-apps/` holds native audio plugins:
each one keeps its own landing page (`index.html`) explaining what it is and how
to install it, next to the release `.zip` and its PDF manual, so a visitor can
download it straight from GitHub Pages.

3. Add the folder name to the `apps` list in `manifest.json`.

## Serving it

The apps use `fetch()` and (for Photo Synth) `AudioWorklet`, so the site must
be served over http(s) — GitHub Pages, or locally:

```sh
npx http-server -p 8080 .
```
