# HTML → VST3 Developer Kit

Everything needed to turn a working browser audio app (HTML + CSS + JS, Web
Audio, AudioWorklets) into a native **VST3 plugin** that looks, feels and
sounds like the original — with the time-critical DSP rewritten in
real-time-safe C++.

It is written to be driven by an AI coding assistant. Point Claude Code,
ChatGPT or a VS Code chat extension at the skill, give it a prototype, and it
has the method, the templates and the traps.

## What is in the box

```
HTML-to-VST3-Devkit-Manual.pdf   the manual — read this first (or the skill below)
skill/
  html-to-vst3/
    SKILL.md                     the method, as a loadable Claude Code skill
    references/                  one short file per phase, read on demand
      00-setup.md                install and verify the toolchain
      01-recon.md                read the prototype, build the inventory
      02-bridge.md               the proxy layer — the core idea
      03-dsp-parity.md           porting DSP so it sounds identical
      04-realtime-safety.md      what the audio thread may never do
      05-host-integration.md     parameters, state, assets, the editor
      06-verification.md         probe, build, screenshot, compare
      07-packaging.md            zip, manual, parity document
      08-gotchas.md              the hours-costing list — read early
  PROMPT.md                      the same method as a portable system prompt
template/                        working skeletons to copy, not to admire
  CMakeLists.txt
  Source/Engine.{h,cpp}          AudioParam + biquad semantics, command FIFO
  Source/PluginProcessor.{h,cpp} bridge messages, APVTS, state
  Source/PluginEditor.{h,cpp}    WebView2 host for the page
  Source/ui/bridge.js            the JS proxy layer
tools/
  check-toolchain.ps1            verify the machine can build plugins
  splice-bridge.ps1              replace the page's audio functions repeatably
  probe-ui.ps1                   headless smoke test of the bridged page
  capture-screenshots.ps1        repeatable UI screenshots
  make-pdf.ps1                   render a print-styled manual to PDF
  package-release.ps1            build the distributable zip
```

## Start here

```powershell
powershell -ExecutionPolicy Bypass -File tools\check-toolchain.ps1
```

It reports what is missing and prints the CMake generator string for your
Visual Studio. Then read `HTML-to-VST3-Devkit-Manual.pdf`, or go straight to
`skill/html-to-vst3/SKILL.md`.

## Using the skill

**Claude Code** — copy the skill folder into your project or your user profile:

```
<project>\.claude\skills\html-to-vst3\      (project-local)
%USERPROFILE%\.claude\skills\html-to-vst3\  (all projects)
```

It then appears as `/html-to-vst3`, and is offered automatically when you ask
for a prototype to be turned into a plugin.

**ChatGPT, Copilot Chat, a VS Code extension, a local model** — paste
`skill/PROMPT.md` as the system or custom instruction, and attach the
`references/` files (or paste the relevant one at the start of each phase).

Either way, the opening request is roughly:

> Here is a browser audio app I want as a VST3: `<path or GitHub URL>`.
> Follow the html-to-vst3 method: keep the page, bridge its audio layer to a
> C++ engine, port the DSP exactly, build in Release after every phase.
> Start with phase 1 — read the whole prototype and give me the inventory and
> the list of things that cannot come across.

The prototype can be a folder on the PC or a GitHub page; if it is a
single self-contained HTML file, so much the better.

## The idea in one paragraph

Do not rewrite the interface. Embed the original page in a JUCE
`WebBrowserComponent` and hand its JavaScript **proxy objects with the same
shape as the Web Audio nodes it already uses**, so calls like
`node.gain.setTargetAtTime(v, t, 0.03)` keep working but marshal over a
lock-free queue into a C++ engine. The look, the layout and the app logic are
then identical by construction — because they are the same code — and the
engineering effort goes where it belongs: reproducing the Web Audio semantics
exactly, and making the audio thread real-time safe.

## Worked example

**Photo-Synth 2** — a four-photo browser instrument ported to VST3 with this
method, interface unchanged: `https://github.com/peterboggild/BrokildApps`
(`vst3-apps/photo-synth-2/`). Its `docs/feature-parity.md` shows the level of
honesty the parity document is meant to have.

## Scope and limits

- **Windows / VST3 / WebView2.** The method transfers to macOS and AU (JUCE
  supports both, and WKWebView replaces WebView2), but the tooling scripts and
  the setup guide here are Windows-specific.
- **Audio apps.** It suits instruments and effects whose UI is already
  browser-based. A prototype with no meaningful UI would be better served by a
  plain JUCE plugin.
- **A WebView is not free.** It costs memory and startup time compared with a
  native UI. The trade is fidelity and speed of porting.

Licence: use it freely. It was written alongside one real port; if something
here is wrong for your prototype, trust your prototype.
