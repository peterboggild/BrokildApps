# 01 · Recon the prototype

Do this before writing a line of C++. The output is an inventory; the port is
mechanical once you have it.

## Read the whole thing

Read the prototype end to end — the HTML, the CSS and all of the JavaScript,
including the worklet files. A browser audio app of this kind is typically
2,000–6,000 lines. Do not skim it and do not sample it: the whole point of
this method is that you keep the parts you did not read, and you can only do
that safely if you know where the audio boundary is.

While reading, mark:

1. **Where the audio graph is built.** Usually one function
   (`buildChain`, `initAudio`, `setupGraph`) that creates every node and wires
   them. This function becomes your proxy factory.
2. **Where parameters are pushed.** Usually one or two functions
   (`applyParams`, `pushAudio`, `update`) that call `setTargetAtTime` /
   `setValueAtTime` on many params at once. These keep working unchanged.
3. **Worklet processors.** Each `registerProcessor(...)` class is a DSP block
   to port to C++ verbatim.
4. **Message protocols.** Every `port.postMessage({...})` shape — these become
   your voice/parameter messages.
5. **Anything scheduled ahead of time** (`setValueAtTime(v, when)`,
   frame-stamped event lists). These need a clock and a schedule queue.
6. **Anything the browser gives you that a plugin does not**: camera,
   microphone, `localStorage`, `IndexedDB`, downloads, `prompt`/`confirm`,
   `fetch`, service workers.

## Write the inventory

Produce a table before porting. For example:

| Web node | Count | Native equivalent |
|---|---|---|
| `GainNode` | 24 | one `NativeParam` each, applied inline |
| `BiquadFilterNode` | 11 | `Biquad` with WebAudio coefficient formulas |
| `DelayNode` | 4 | circular buffer + fractional read |
| `ConvolverNode` | 2 | `juce::dsp::Convolution`, crossfaded |
| `WaveShaperNode` | 3 | inline transfer function + oversampling |
| `StereoPannerNode` | 16 | pan law applied per voice |
| `DynamicsCompressor` | 1 | approximated; document the difference |
| `AudioWorklet` "voice" | 16 | `Voice` class, exact port |
| `AudioWorklet` "lofi" | 1 | `Lofi` struct, exact port |

Then enumerate every parameter that the UI writes to, and give each an integer
id. That id list is the wire protocol between the page and the engine — put it
in `Engine.h` as an enum and mirror it in `bridge.js` as an object. Keep the
two in the same order and never renumber without changing both.

## Decide what cannot come across

Be explicit and honest about it, in writing, in `docs/feature-parity.md`:

- **Camera / microphone capture** — a plugin has no business opening them;
  drop the feature and leave the buttons failing gracefully, or remove them.
- **Downloads** — a WebView cannot start one. Route exports through a native
  file dialog instead.
- **`prompt()` / `confirm()`** — suppressed in WebView2. Provide defaults.
- **`localStorage` / `IndexedDB`** — these do work in the WebView, but they
  live in the plugin's own profile, not in the DAW project. Anything the user
  expects to survive a project reload must be in `getStateInformation`.

Everything else should come across. If you are about to drop a feature for
convenience, don't — that is the difference between a port and a rewrite.

## Sanity-check the prototype first

Load the original in a browser and confirm it works before you change
anything. If it is broken to begin with you will spend the whole port chasing
a bug that was already there.
