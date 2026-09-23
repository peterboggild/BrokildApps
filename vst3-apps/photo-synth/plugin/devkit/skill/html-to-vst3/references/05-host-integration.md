# 05 · Host integration

What turns a working audio engine into a plugin someone can actually use in a
project.

## Parameters (APVTS)

Expose the controls a user would automate — typically every slider in the
prototype's settings panels. Use the **slider's own units and range**, so the
automation lane reads like the UI:

```cpp
layout.add (std::make_unique<juce::AudioParameterFloat> (
    juce::ParameterID { "delayTime", 1 }, "delayTime",
    juce::NormalisableRange<float> (40.0f, 900.0f, 0.0f), 260.0f));
```

Then handle the two directions:

- **UI → host.** When the user moves a control in the page, mirror it into the
  parameter (`setValueNotifyingHost`) so the host records automation. Guard
  with a `suppressEcho` flag so this does not bounce back.
- **Host → UI.** In `parameterChanged` (which can be called from the audio
  thread) only record the id as dirty; on the editor's timer, read the value
  and push it to the page, which applies it through its own slider handler so
  the readout, the curve and the audio all update the way they normally do.

**With the editor closed there is no page to push to.** Apply those parameters
natively instead — reimplement the slider's value mapping in C++
(`0.002 + (v/100)^2` and so on). Any parameter whose effect genuinely requires
the UI (things that re-sample an image, or re-run a layout) cannot work with
the window closed; list those in the parity document rather than pretending.

## State

`getStateInformation` must capture **everything needed to reproduce what the
user hears and sees**, including assets:

```
{ "apvts":  <ValueTree XML>,
  "engine": <parameter + voice shadow snapshot>,
  "ui":     <the page's own preset JSON, including downscaled images> }
```

For assets (photos, samples, wavetables) let the page produce a compact copy —
a ≤640 px JPEG data URL is a few tens of kB and is enough for both display and
sampling — and push it to the processor on a debounce whenever it changes. The
processor stores the JSON blob opaquely; the page parses it back on load.

Restore in `setStateInformation` by (a) replacing the APVTS tree, (b) applying
the engine snapshot through the same command queue, and (c) if an editor is
open, handing the UI blob to the page. When an editor opens later it asks for
the blob itself (`{k:"getstate"}` → `initialState`), which covers the common
case of a project loading with the window closed.

**Never restore a held gate/note.** Filter those fields out or a reopened
project starts screaming.

## The editor

- Resizable, with sensible limits, and a default size matching the layout the
  page was designed for.
- A timer (~30 Hz) that pushes the clock, drains the MIDI-to-UI queue, relays
  dirty parameters and collects consumed payloads.
- The processor holds a `std::function` the editor sets on construction and
  **clears in its destructor** — that pointer is how the processor knows
  whether a UI exists at all.

## Bus layout and reporting

```cpp
bool isBusesLayoutSupported (const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
```

Set `getTailLengthSeconds()` to cover your longest reverb/delay tail, or hosts
will cut off the release when rendering.

## File dialogs

A WebView cannot download or open files. Anything the prototype did with an
`<a download>` or an `<input type=file>` becomes a native
`juce::FileChooser` launched from the processor, with `launchAsync` (never the
modal form) and the chooser kept alive in a member.

## Long operations

Offline rendering, image decoding and preset scanning must not block the
message thread. Run them on a `juce::ThreadPool` and return the result via
`juce::MessageManager::callAsync`. For an offline render, instantiate a
**second, private engine** — never reuse the live one.
