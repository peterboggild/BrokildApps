---
name: html-to-vst3
description: Convert an HTML/CSS/JS audio prototype (WebAudio, AudioWorklet, canvas UI) into a native JUCE VST3 plugin that looks, feels and behaves identically, with all time-critical DSP rewritten in real-time-safe C++. Use when asked to "make a VST3 out of this web app", "port this browser synth to a plugin", "turn this prototype into a DAW instrument", or to set up a Windows machine for VST3 development.
---

# HTML prototype → native VST3

You are porting a working browser audio app into a VST3 plugin. The goal is
**1:1 fidelity**: same look, same interaction, same sound — with the audio
engine rewritten in C++ so it is real-time safe and DAW-hosted.

## The one architectural decision that makes this work

**Keep the prototype's HTML/CSS/JS. Replace only its audio layer.**

Do not rewrite the interface in C++ or in a new framework. Do not rewrite the
app's logic. Embed the original page in a `WebBrowserComponent` and give its
JavaScript **proxy objects with the same shape as the WebAudio nodes it already
uses**. `g.master.gain.setTargetAtTime(v, t, 0.03)` keeps working — it just
reaches a C++ engine instead of WebAudio.

Everything follows from this:

| Layer | Where it lives | Why |
|---|---|---|
| Markup, CSS, canvas drawing, gestures, app logic | Unchanged original page, embedded | Fidelity is free; you cannot accidentally redesign it |
| Parameter values, node graph shape, scheduling | Thin JS bridge (proxies) | The app's own code drives it |
| Oscillators, filters, effects, voices, mixing | C++ engine | Real-time safety, host integration |
| MIDI, automation, state, presets | C++ processor | Only the host can do this |

If you find yourself reimplementing a slider, a canvas, or a layout rule in
C++, stop — you have taken a wrong turn.

## Phases

Work in this order and **build after every phase**. Each reference file is
short; read the one for the phase you are in.

0. **Set the machine up** → `references/00-setup.md`
   Visual Studio + CMake + JUCE + WebView2, verified by a build.
1. **Recon the prototype** → `references/01-recon.md`
   Read it completely. Inventory every audio node, param and worklet message.
2. **Build the bridge** → `references/02-bridge.md`
   Proxy objects in JS, a command FIFO in C++. Start with the page rendering.
3. **Port the DSP** → `references/03-dsp-parity.md`
   Line by line. WebAudio semantics are a specification you must reproduce.
4. **Make it real-time safe** → `references/04-realtime-safety.md`
   No allocation, locks or logging on the audio thread. Sample-accurate MIDI.
5. **Integrate with the host** → `references/05-host-integration.md`
   APVTS parameters, full state (assets included), editor-open vs closed.
6. **Verify** → `references/06-verification.md`
   Headless probes of the page, a real build, a screenshot of it running.
7. **Package and document** → `references/07-packaging.md`
   Zip, manual, install instructions.

**Read `references/08-gotchas.md` early.** It is a list of things that will
cost you hours if you meet them cold: WebView2 suppressing `prompt()`, Chrome
headless not returning stdout, PowerShell eating your arguments, a locked DLL
when the DAW is open, and a biquad that goes silent at 0 Hz.

## Non-negotiable rules

- **Never edit the reference prototype.** Copy it into the plugin source tree
  and modify the copy. Keep the original for diffing.
- **Never guess at DSP.** If the original computes
  `1 - Math.exp(-1 / (0.006 * sampleRate))`, the C++ computes exactly that.
  Same constants, same order, same per-block cadence.
- **Never allocate, lock, or log in `processBlock`** or anything it calls.
- **A change is not done until it is built.** Configure and compile in Release
  after every phase; fix warnings that touch your own code.
- **Verify before claiming.** Load the page headlessly and assert on the DOM;
  build; run the standalone and screenshot it. Report what actually happened.

## Getting the prototype

The source may be a local folder or a GitHub page. Either way:

```
<project>/
  reference/<app>/        # untouched copy of the prototype — never edited
  Source/ui/ui.html       # the working copy that gets the bridge spliced in
  Source/Engine.{h,cpp}   # the C++ engine
  Source/Plugin*.{h,cpp}  # processor, editor, bridge message handling
  CMakeLists.txt
```

If it is a single self-contained HTML file, `Source/ui/ui.html` is that file
plus a bridge. If it loads `worklet.js` and assets, inline or embed them —
the plugin must run offline with no external files.

## Splice, don't retype

You will replace a handful of named functions in the page (the ones that build
the audio graph). Do it with a **script that cuts between markers**, not by
hand-editing thousands of lines, so the operation is repeatable when the
prototype is updated. `tools/splice-bridge.ps1` in this kit does exactly that;
keep the replacement bodies in separate files next to it.

## Starting files

`template/` holds working skeletons you should copy and adapt rather than
write from scratch:

- `Source/Engine.h` — `NativeParam` (WebAudio `AudioParam` semantics),
  `Biquad` (WebAudio coefficient formulas), the command struct, engine shell.
- `Source/Engine.cpp` — command FIFO drain, block splitting, parameter ticking.
- `Source/PluginProcessor.{h,cpp}` — bridge message parsing, APVTS, state.
- `Source/PluginEditor.{h,cpp}` — WebView2 with a resource provider.
- `Source/ui/bridge.js` — the JS proxy layer.
- `CMakeLists.txt` — JUCE plugin target with embedded UI.

A complete worked example of everything in this kit is **Photo-Synth 2**
(`https://github.com/peterboggild/BrokildApps`): a four-photo browser
instrument ported to VST3 with the UI byte-identical to the original page.
