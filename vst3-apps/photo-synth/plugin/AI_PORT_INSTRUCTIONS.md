# Photo Synth VST3 Conversion Instructions

This document describes how to complete a near 1:1 conversion of the original Photo Synth HTML synthesizer into a native JUCE VST3 and Standalone instrument.

The goal is to preserve the original look, feel, workflow, and musical behavior as closely as possible while moving all time-critical audio and host-facing functionality into native C++.

## Non-negotiable goals

- Preserve the original HTML/CSS/JS frontend as closely as possible.
- Remove camera capture from scope if needed, but keep image loading from file and image persistence.
- Reimplement all time-critical audio logic in native C++.
- Keep the browser UI as the presentation and gesture layer only.
- Make the plugin work offline, with no localhost server, no web dependency, and no external files at runtime.
- Support VST3 and Standalone.
- Ensure the plugin is DAW-safe, real-time safe, and fully state-restorable in Ableton Live.

## Repository facts

- Project root: `C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\photo-synth\plugin`
- JUCE source used by this project: `C:\Users\peter\AudioDev\Projects\BrokildVSTTemplate\external\JUCE`
- Reference copy of the original browser app: `reference/photo-synth/`
- Current native plugin scaffold exists and already builds successfully.

## Working rules

- Do not ask the user questions unless a truly blocking ambiguity remains.
- Do not stop after partial progress; keep moving until the next buildable milestone is complete.
- Stage work into small, verifiable phases if the full conversion is too large for one pass.
- Build after each major phase.
- Keep the original browser app intact under `reference/photo-synth/`.
- Do not modify `external/JUCE`.
- Do not add third-party dependencies unless there is a clear technical need and it is explained.

## Architecture target

### Frontend

- Use JUCE `WebBrowserComponent` with WebView2 on Windows.
- Recreate the original UI almost verbatim: layout, colors, typography, controls, hover/pressed states, and interaction model.
- Keep the frontend as HTML/CSS/JS only.
- The frontend may manage UI-only state, canvas drawing, gestures, and non-audio presentation.
- The frontend must not generate audio.

### Audio engine

- Port `synth-worklet.js` behavior into native C++.
- Use C++ for oscillators, envelope generation, filter, modulation, FX, voice allocation, smoothing, and MIDI handling.
- Keep the audio path real-time safe.
- No memory allocation in `processBlock` or code called by it.
- No logging, file access, locks, blocking, JavaScript calls, or WebView calls on the audio thread.
- Preserve MIDI sample offsets.
- Queue UI note events through a bounded lock-free or otherwise real-time-safe queue.

### State and persistence

- Store all automatable parameters in `AudioProcessorValueTreeState`.
- Keep visual-only UI state separate from DSP state.
- Embed loaded images in the plugin state so Ableton project reloads restore them.
- Support preset save/load from the plugin UI.
- Ensure host project save/load round-trips the same sound and UI-relevant state.

## Recommended implementation order

1. Recreate the original frontend structure inside the plugin editor.
2. Replace the placeholder UI with the actual Photo Synth HTML/CSS/JS.
3. Reimplement the core synth voice, envelope, and filter in native C++.
4. Add image import from disk and store the image bytes in plugin state.
5. Add preset save/load.
6. Port the remaining modulation and FX modules.
7. Add parity tests or smoke tests for DSP state, serialization, and plugin startup.
8. Verify the plugin in Ableton Live.

## Fidelity guidance

- Prefer faithful behavior over a simplified redesign.
- If a feature must be adapted for DAW reality, document the adaptation explicitly in `docs/feature-parity.md`.
- Do not silently remove original functionality.
- Only omit camera capture if necessary; everything else should be retained or ported.

## Validation requirements

- A change is not complete until it has been configured and built successfully.
- Prefer the build outside the repository to avoid path-length issues.
- Continue iterating until the project builds in Release configuration.
- If a phase introduces a new feature, validate it with the narrowest practical build or runtime check.

## Current build commands

Use the out-of-tree build directory:

```powershell
cmake -S C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\photo-synth\plugin -B C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\photo-synth\plugin\build -G "Visual Studio 18 2026" -A x64
cmake --build C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\photo-synth\plugin\build --config Release
```

## Expected outcome

The final result should be a native Photo Synth plugin that:

- looks like the original browser app,
- behaves like the original where DAW-safe,
- sounds equal or better than the web version,
- saves and restores full project state in Ableton Live,
- and ships as a clean VST3 and Standalone build.
