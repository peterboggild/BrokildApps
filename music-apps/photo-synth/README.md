# Photo Synth

A two-photo instrument that runs entirely in the browser. Drag across the first
photo and the colour under your finger becomes a tone; touch the second and its
colour drives a virtual-analog ladder filter. Photos come from the camera or the
camera roll, an on-screen keyboard or an imported MIDI file sets the base note,
and the output can be recorded or rendered to 24-bit WAV.

## Files

| File | What it is |
|------|------------|
| `index.html` | The whole app — markup, styles and logic in one file. |
| `synth-worklet.js` | The audio engine (`AudioWorklet`): oscillators, ladder filter, capture. |
| `icon.svg` | App icon, used by the page and the web manifest. |
| `manifest.webmanifest` | Lets the page be added to a phone home screen. |

All four must sit in the same folder — `index.html` loads `synth-worklet.js` by
relative path.

## Serving it

It is a static page, but it must be served over **http(s)**, not opened as a
`file://` URL: `AudioWorklet.addModule()` and `OfflineAudioContext` are blocked
on `file://`. Any static host works — GitHub Pages, or locally:

```sh
npx http-server -p 8080 .
# then open http://localhost:8080/photo-synth/
```

On GitHub Pages, drop the folder anywhere in a Pages-published branch and it is
live at `https://<user>.github.io/<repo>/photo-synth/`.

## Requirements

Any current browser. The analog engine needs `AudioWorklet` (Chrome 66+,
Safari 14.1+, Firefox 76+); without it the page falls back to an additive sine
engine on its own and says so in Settings. On iPhone, web audio follows the
ring/silent switch — if the pads look alive but there is no sound, flip it.

Nothing is uploaded: photos are decoded and sampled locally, and audio is
synthesised and encoded in the page.
