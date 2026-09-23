# Prompt To Finish The Photo-Synth2 Port

Use this prompt verbatim or adapt it slightly for the next coding agent.

---

You are continuing work on an in-progress JUCE VST3/Standalone port of the browser app Photo Synth.

Project root:
`C:\Users\peter\b\PhotoSynth`

Your job is to finish the port from the current state, not to restart it from scratch.

## Core objective

Make the plugin match the original browser Photo Synth as closely as possible in:

- visual layout
- interaction model
- photo workflow
- musical behavior
- state persistence

The only intended exclusion is browser-only camera capture and related live browser media features.

Everything else should be preserved, restored, or adapted for a DAW-safe native plugin.

## What matters most right now

The plugin currently does **not** visually match the original browser app.

The current embedded UI is still a simplified custom shell rather than the preserved original frontend. This is visible in the plugin screenshot and is the main regression to fix first.

There are also text/encoding issues in the current embedded UI, and the multi-photo workflow is not properly restored yet.

Do not claim parity until the plugin actually renders and behaves like the original app inside the JUCE host.

## Current repository facts

- Original browser app is preserved at:
  `reference/photo-synth/`
- Main original frontend file:
  `reference/photo-synth/index.html`
- Original worklet DSP reference:
  `reference/photo-synth/synth-worklet.js`
- Current embedded UI wrapper:
  `Source/PhotoSynthUi.h`
- Editor/browser bridge:
  `Source/PluginEditor.cpp`
- Native processor:
  `Source/PluginProcessor.h`
  `Source/PluginProcessor.cpp`
- Native engine:
  `Source/Engine.h`
  `Source/Engine.cpp`

## Important current status

Read these first before editing:

- `docs/project-status.md`
- `docs/feature-parity.md`
- `README.md`

Important: `docs/feature-parity.md` currently overstates completion. Trust the actual code and current UI behavior over that document.

## Current known problems

1. The VST UI does not match the original browser UI.
2. Strange characters appear in the plugin UI, indicating encoding/resource handling problems.
3. Photo loading is not correctly restored for the original multi-photo workflow.
4. The current custom UI shell only partially imitates the original app.
5. A previous attempt to generate an embedded header from the original HTML failed because Bash-style heredoc syntax was mistakenly used in PowerShell 5.1.
6. `Source/PluginProcessor.cpp` has been edited recently and must be re-read before changing anything around processor state and bridge behavior.

## Constraints

- Keep the original browser app intact under `reference/photo-synth/`.
- Do not modify JUCE itself.
- Do not silently drop features.
- Use the original frontend as the starting point wherever practical instead of re-creating it manually.
- Preserve DAW-safe native audio processing in C++.
- Keep browser UI code as presentation and gesture logic, not audio generation.
- Camera capture may remain omitted, but image loading from disk must work.

## Required approach

Follow this sequence unless you hit a real blocker:

### Phase 1: Re-establish the real frontend

1. Read `reference/photo-synth/index.html` carefully.
2. Read `Source/PhotoSynthUi.h` carefully.
3. Read `Source/PluginEditor.cpp` carefully.
4. Identify the smallest clean way to replace the current custom HTML shell with the original frontend while keeping a JUCE bridge.
5. Embed the original HTML as a resource used by the plugin.
6. Preserve UTF-8 correctly so characters like `→` and other labels render properly.

Goal of phase 1:
The plugin should visually resemble the original browser app again before further parity claims are made.

### Phase 2: Adapt the original browser frontend to the plugin bridge

The original frontend was built around browser-only audio and browser capabilities. Replace only the parts that must change.

You should:

1. Remove or bypass browser-only audio engine startup from the embedded frontend.
2. Replace browser audio actions with JUCE bridge events.
3. Keep the original layout, control text, control ordering, and visual behavior whenever possible.
4. Replace browser-only camera actions with disabled controls or plugin-appropriate file-loading actions.
5. Restore the original multi-photo workflow, not a single-photo placeholder.

Do not redesign the UI unless absolutely necessary.

### Phase 3: Restore image and interaction parity

1. Make sure each original relevant photo/pad area can load or receive the correct image state.
2. Ensure project reload restores the correct image state and UI state.
3. Ensure drag gestures on tone/filter/env/fx behave like the original.
4. Ensure motion recording and loop playback remain available if they were present in the original workflow.

### Phase 4: Verify native audio mapping against the original app

Use `reference/photo-synth/synth-worklet.js` as the behavior reference.

Check and tune:

- pitch mapping
- envelope behavior
- filter mode behavior
- spread / stereo behavior
- drive / saturation behavior
- glide behavior
- detune / sub behavior
- freeze / halt behavior
- FX chain parameter mapping and ordering behavior

If the current native engine is incomplete, continue porting from the original worklet behavior into `Engine.cpp` and related processor code.

### Phase 5: Validate and package

After each meaningful phase, rebuild in Release mode.

Build commands:

```powershell
cmake -S C:\Users\peter\b\PhotoSynth -B C:\Users\peter\b\PhotoSynth\build -G "Visual Studio 18 2026" -A x64
cmake --build C:\Users\peter\b\PhotoSynth\build --config Release
```

Then update the installed bundle at:

`C:\Program Files\Common Files\VST3\Photo-Synth2.vst3`

Use a non-interactive copy command and verify the installed timestamp after deployment.

## First things to inspect before the first edit

Read these files:

- `Source/PhotoSynthUi.h`
- `Source/PluginEditor.cpp`
- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/Engine.h`
- `Source/Engine.cpp`
- `reference/photo-synth/index.html`
- `reference/photo-synth/synth-worklet.js`
- `docs/project-status.md`

## Likely first concrete implementation step

The most likely correct first milestone is:

1. Create a dedicated embedded HTML header or resource sourced from the original `reference/photo-synth/index.html`.
2. Replace the custom HTML in `Source/PhotoSynthUi.h` with a thin wrapper that returns the original embedded HTML string.
3. Add only the minimal bridge shims required for JUCE integration.
4. Build immediately.

Do not keep iterating on the custom fake UI shell if the goal is true parity.

## Validation standards

Do not say the work is complete unless all of the following are true:

1. The plugin visually matches the original browser app closely.
2. Text renders correctly without mojibake or broken characters.
3. Photo loading works for the original relevant workflow, not just one preview area.
4. Gestures and note behavior work in the plugin.
5. State restore works for both parameters and images.
6. The project builds successfully in Release mode.
7. The installed VST3 bundle in `C:\Program Files\Common Files\VST3\Photo-Synth2.vst3` has been refreshed.

## Deliverables expected from you

When done, provide:

1. A brief summary of what was changed.
2. A truthful list of any remaining differences from the original app.
3. The exact validation performed.
4. Confirmation that the installed VST3 bundle was updated.

## Additional caution

- Prefer small, verifiable edits.
- Re-read files before editing if another tool or person may have changed them.
- Do not trust stale assumptions from earlier iterations.
- If documentation conflicts with runtime behavior, trust runtime behavior and fix the documentation later.

Your mission is to finish the port from the current codebase state, not to describe what should happen.

Implement the changes, build the plugin, validate the result, and update the installed VST3 bundle.

---

Suggested companion context for the next agent:

- Current factual project snapshot: [project-status.md](C:/Users/peter/b/PhotoSynth/docs/project-status.md)
- Existing parity tracker: [feature-parity.md](C:/Users/peter/b/PhotoSynth/docs/feature-parity.md)
- Original app reference: [index.html](C:/Users/peter/b/PhotoSynth/reference/photo-synth/index.html)