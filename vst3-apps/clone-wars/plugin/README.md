# Clone Wars — plugin source

A 16-voice analogue drone monster: two 8-voice armies, per-clone channel
strips, TOLERANCE/ENTRAIN/TIDE, the WAR crossfader, seed patches, and a
panel that ages with use. Built on the BrokildApps **HTML→VST3 devkit**
conventions (JUCE 8, WebView2 UI compiled into the binary, native C++
engine).

## Layout

```
Source/Core/cw_core.{h,cpp}   the engine — pure C++17, no JUCE, no allocation
                              after prepare(); voices, filters, entrain, tide,
                              note slots, buses, FX, seed patch generator
Source/PluginProcessor.*      JUCE shell: ~320-parameter APVTS, MIDI slots,
                              per-instance age/wear in project state, UI bridge
Source/PluginEditor.*         WebView2 editor per the devkit template
Source/ui/ui.html             the panel (the mockup + a bridge script; still
                              works standalone in a browser as the mockup)
test/render_test.cpp          headless validation — renders WAVs, asserts
CMakeLists.txt                VST3 + Standalone; JUCE fetched if JUCE_DIR unset
```

## Validate the engine anywhere (no JUCE needed)

```sh
cd test
g++ -O2 -std=c++17 -I../Source/Core render_test.cpp ../Source/Core/cw_core.cpp -o render_test
./render_test wavs   # writes WAVs, prints stats, exit 0 = all checks pass
```

Checks: no NaN, no silence, no hard clipping, no DC, silence when idle,
latched chords sustain, all three filter tempers stable at 95 % resonance,
and the same seed renders **bit-identically** — the seed-is-the-patch
guarantee.

## Build the plugin (Windows, per devkit)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR=C:/AudioDev/JUCE
cmake --build build --config Release
```

Keep the checkout path short (e.g. `C:\b\CloneWars`) — deep paths hit MSVC's
path-length limit inside JUCE builds. Without `-DJUCE_DIR`, CMake fetches
JUCE 8 automatically (what CI does).

CI: `.github/workflows/clone-wars.yml` runs the engine test on Linux, then
builds win64 VST3 + Standalone on every push touching this folder and
uploads them as the `Clone-Wars-VST3-win64` artifact.

## Design notes

- **The hull remembers.** `ageSamples`, `wearPoints`, `wearSeed` live in the
  instance state (`getStateInformation`), so every instance ages with its
  DAW project. Age/wear accrue only while the unit is audibly playing.
  REPAIR (service bay, shift-click the odometer) resets both.
- **Unit seed.** Each fresh instance draws a `unitSeed`; TOLERANCE scatters
  component values (detune, cutoff, envelope time, level, LFO rate) from it,
  so no two instances are quite the same machine.
- **Note slots.** Three slots, arrival order, steal-oldest; per-clone NOTE
  1·2·3 picks the slot. LATCH per army keeps released notes sounding. With
  DRONE on, an empty slot 1 falls back to the army base pitch, so the synth
  sounds the moment it is instantiated.
- **Quality tiers.** The ENGINE button cycles LOW / HQ / XHQ: the filter
  and saturation nonlinearities run 1× / 2× / 4× oversampled (linear-interp
  upsampling, averaging decimation). Measured on the audit: saw-through-
  resonant-filter aliasing −40.6 / −46.7 / −47.0 dB. **Offline render is
  always forced to XHQ** (`isNonRealtime()`), whatever the switch says.
  All tiers render bit-deterministically.
- **Audit.** `test/audit.cpp` measures aliasing per tier, clean-path THD
  (growl −46.6 dB; ladder colour −29.9 dB by design), click energy on hard
  parameter jumps, idle silence, DC per seed, full-blast headroom (≤ 0.999
  into the slope-continuous safety knee at 0.7), and tier speed (LOW 11×,
  HQ 10×, XHQ 7× realtime for all 16 voices with FX, single thread). CI
  runs it on every push.
- Damage sprite integration (photographic decals) is pending the sprite set;
  the UI's procedural patina is the placeholder.
