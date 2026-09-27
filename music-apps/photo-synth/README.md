# Photo Synth

A three-photo instrument that runs entirely in the browser. Drag across the
first photo and the colour under your finger becomes a tone. Two voice modes
share a sixteen-voice pool: Mono/Unison stacks 1–12 voices on one note, and
Polyphonic plays up to 4 simultaneous notes (keyboard chords and real MIDI
chords) with 1–4 photo-sampling oscillators per note — idle voices sleep, so
only what you play costs CPU. Touch the second photo and its colour drives a
virtual-analog ladder filter;
the third shapes the envelope (red attack, green sustain, blue decay). Each pad
has its own motion Rec button — record a gesture or define a line/circle with
two fingers and it loops immediately. Polyphonic voices can pan by sampled hue
or cursor position, with a width-zero mono mode. The envelope can be sent to
amplitude, to a filter sweep, or any blend of the two, and the ladder offers
five modes (low/band/high pass, notch, and a tuned feedback comb). A
seven-module stereo effects chain — loudness-compensated tube saturation,
vintage phaser, vintage chorus, stutter gate, a lofi stage (crush, grit,
noise, dirt), a delay with clean/tape characters and an L/R time offset, and
convolution reverb with room/hall/plate/spring — is freely reorderable, and
every module has a power switch that removes it from the audio graph entirely
(real CPU savings), switched under a short master dip so it never pops. The **Spectrum** panel, below Effects, holds character modules -- things that change how
every voice is built rather than sitting in the effects chain, each with an enclosure of its
own. Dark Drone is the first: one switch arms a zero-cost-when-off drone engine with sub-oscillator,
microtonal cluster detune, slow random drift, pitch sag on decay, a Freeze
gesture that holds the delay tail forever, a Halt tape-stop, delay feedback
past unity into a soft limiter, and a reverse reverb character. A
fourth photo pad drives three selectable effect parameters from the sampled
colour's red, green and blue strengths — black is all off, white is all full.
Presets capture everything including small copies of the photos, stored only
on the device (IndexedDB) with single-file export/import. Photos come from the
camera, the camera roll or a live camera feed. The base note comes from an
on-screen keyboard, the computer keys, an imported MIDI file, or a hardware
MIDI keyboard over Web MIDI -- with velocity, sustain pedal and a two-semitone
pitch bend, per-device and per-channel filtering, and hot-plug detection. The
output can be recorded or rendered to 24-bit WAV.

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
