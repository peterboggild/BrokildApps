# Photo-Synth2 Project Status

This document is a factual status snapshot of the current JUCE VST3 port.
It is intended to track what has been completed, what currently works, what is still missing, and what is currently broken.

## Goal

Port the original browser Photo Synth from [reference/photo-synth](C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/photo-synth/plugin/reference/photo-synth) into a native JUCE VST3/Standalone instrument while preserving:

- the original look and layout
- the original interaction model
- the original sound behavior as closely as practical in native DSP
- project-state persistence for images and UI state

The only planned exclusion is browser-specific camera capture and related live browser media features.

## What Has Been Done

### Project and plugin shell

- JUCE plugin project exists and builds as VST3 and Standalone.
- Embedded web UI host exists through `WebBrowserComponent`.
- UI-to-native event bridge exists in the editor.
- Native processor, editor, and engine source files are present.

### Native parameter and state plumbing

- Core synth controls are exposed as native parameters.
- UI events are bridged to native parameter changes.
- Host state save and restore plumbing has been added.
- Image state persistence support has been added.
- UI-only JSON state persistence support has been added.

### Interaction work already added

- Tone, filter, envelope, and FX pads exist in the embedded UI.
- Keyboard interaction exists in the embedded UI.
- Latch, record-arm, freeze, halt, and panic controls have been wired.
- Per-pad motion capture and loop replay logic has been added to the current custom UI shell.
- FX routing and FX chain UI scaffolding has been added to the current custom UI shell.

### Packaging and deployment

- Release builds have been produced successfully during this porting work.
- The built VST3 bundle has been copied into `C:\Program Files\Common Files\VST3\Photo-Synth2.vst3`.

## What Currently Works

Based on the current source and prior successful builds, the following areas are implemented at least partially:

- VST3 packaging and installation
- embedded browser UI loading
- parameter editing from the UI
- native note on/off bridging
- image preview for at least the main photo area
- preset save/load hooks
- host-facing state serialization infrastructure

## What Is Currently Wrong

The current plugin state is not yet at true 1:1 parity with the original browser app.

### Major UI mismatch

- The current VST UI is still a simplified custom shell, not the original browser interface preserved in `reference/photo-synth/index.html`.
- This explains the visual mismatch between the plugin and the original app.
- It also explains why the control grouping and workflow differ from the original.

### Encoding / text issues

- The strange characters seen in the plugin UI are consistent with text-encoding problems in the embedded HTML/CSS/JS resource path.
- This must be cleaned up when the original HTML is embedded directly and written with consistent UTF-8 handling.

### Photo workflow mismatch

- The current UI exposes only a single main photo preview area.
- The original app is a multi-photo workflow with separate pads using separate images and image-driven interaction.
- The present VST does not yet reproduce that full per-pad image loading and interaction model.

### Current documentation overstates parity

- `docs/feature-parity.md` currently describes the port as much closer to complete than the current visible UI state actually is.
- That file should be treated as aspirational rather than authoritative until the original frontend is actually embedded and verified.

## Known Risks

- The current source tree has signs of active manual editing and partial transitions between a custom replacement UI and the original frontend.
- `Source/PluginProcessor.cpp` should be verified carefully before further parity claims are made.
- The current embedded UI resource path needs a controlled replacement so the original browser app can run inside the plugin without breaking the JUCE event bridge.

## What Still Needs To Be Done

### Highest priority

- Replace the current custom `PhotoSynthUi.h` shell with the real original browser frontend from `reference/photo-synth/index.html`.
- Preserve the original layout, typography, labels, and control organization.
- Keep the JUCE-native event bridge, but adapt the original frontend to talk to the plugin instead of browser-only audio code.

### Frontend parity work

- Restore the original multi-photo pad workflow.
- Restore per-pad photo loading behavior.
- Restore the original panel structure and control group arrangement.
- Remove text-encoding problems in embedded resources.
- Verify that the embedded UI renders identically enough at plugin sizes used in hosts.

### Native DSP parity work

- Continue matching the browser synth behavior to the native engine.
- Verify filter behavior, envelope behavior, detune/spread behavior, glide behavior, and FX behavior against the original.
- Confirm whether the current FX chain is only UI scaffolding or fully mapped into the native engine.

### State and asset parity

- Ensure all photo assets used by the original workflow are persisted in plugin state.
- Ensure preset export/import restores the same visual and sonic state.
- Ensure host save/load round-trips everything needed for the restored session to behave the same.

### Validation work

- Rebuild after the original frontend is embedded.
- Verify image loading on every relevant pad, not just the first preview field.
- Smoke-test in a real host after deployment.
- Compare side-by-side against the original HTML app using the same source image and the same gestures.

## Recommended Next Sequence

1. Swap the embedded UI resource to the original `reference/photo-synth/index.html`.
2. Adapt the original frontend so it uses the JUCE bridge instead of browser-only audio paths.
3. Rebuild and verify rendering in the plugin.
4. Restore per-pad photo loading behavior.
5. Re-test text encoding, labels, and layout.
6. Re-package and overwrite the installed VST3 bundle.

## Current Bottom Line

Important infrastructure has been built: native plugin shell, event bridge, parameters, state plumbing, image persistence support, and packaging.

However, the current VST is not yet visually or behaviorally identical to the original browser Photo Synth. The largest remaining gap is that the plugin is still showing a custom replacement frontend instead of the original preserved frontend, and the full multi-photo workflow is not yet restored.