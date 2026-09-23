# Photo-Synth2

Native JUCE VST3 and Standalone port of the original browser-based Photo Synth.
Named **Photo-Synth2** (plugin code `Psy2`) to avoid clashing with the parallel port built with ChatGPT.

## Architecture

- `Source/ui/ui.html` — the **original app's `index.html` almost verbatim**
  (four photo pads with generated demo photos, motion recording, keyboard,
  MIDI player, presets, reorderable 7-module FX rack with both skins, scope,
  Dark Drone). Its WebAudio layer is replaced by a native bridge: proxy
  "AudioParams"/voice ports forward the identical parameter targets, smoothing
  time-constants and voice messages to C++. Embedded via `juce_add_binary_data`.
- `Source/Engine.h/.cpp` — the C++ engine: an exact port of the
  `synth-worklet.js` voice (16-voice pool, PolyBLEP morph oscillators, ZDF
  ladder incl. comb mode, sub osc, tape stop, 2× oversampling), the additive
  fallback engine, and the full FX graph from `buildChain()` (tube, delay with
  tape/freeze/wow, dual-convolver reverb with the same generated impulses,
  phaser, chorus, stutter, lofi), plus host-MIDI note allocation.
- `Source/PluginProcessor.*` — bridge message handling, ~39 automatable
  parameters (APVTS, matching the Edit sliders), native recorder (24-bit WAV),
  native offline MIDI→WAV render, full project state incl. photo snapshots.
- `Source/PluginEditor.*` — WebView2 editor (resizable, 1300×900 default).

See `docs/feature-parity.md` for the exact parity status.

## Build

Use a build directory outside the repository:

```powershell
cmake -S C:\Users\peter\b\PhotoSynth -B C:\Users\peter\b\PhotoSynth\build -G "Visual Studio 18 2026" -A x64
cmake --build C:\Users\peter\b\PhotoSynth\build --config Release
```

Install: copy `build\PhotoSynth_artefacts\Release\VST3\Photo-Synth2.vst3` to
`C:\Program Files\Common Files\VST3\Brokild\`.

## Reference copy

The original web app is preserved unchanged under `reference/photo-synth/`.
The UI modifications live only in `Source/ui/ui.html` (diff against the
reference to see the bridge).
