# 02 · The bridge

The bridge is the whole trick. It lets the prototype's own JavaScript keep
running unchanged while the sound is made in C++.

## Shape of it

```
original page JS            bridge (JS)                 C++
──────────────────          ─────────────               ──────────────
g.master.gain               mkParam(ctx, PID.master)    NativeParam[pMaster]
  .setTargetAtTime(v,t,tc)  → NB.send({k:"p", i, m:1,   → Command{setParamTarget}
                               v, tc, t})                  → lock-free FIFO
voice.port.postMessage(     NVoice.postMessage          → Command{setVoiceField}
  {type:"params", p:{...}}) → NB.send({k:"v", v, f:[…]})   → Voice::setField
```

Nothing in the page above that first column changes.

## JS side (`template/Source/ui/bridge.js`)

Four kinds of proxy:

1. **`mkParam(ctx, id)`** — an object with `value`, `setValueAtTime`,
   `setTargetAtTime`, `linearRampToValueAtTime`, `cancelScheduledValues`.
   Every call marshals to one message.
2. **`gainNode(id)` / node stubs** — objects with `.gain`, `.frequency`,
   `.delayTime`, `.connect()` and whatever else the page touches. `connect()`
   is usually a no-op: the C++ graph is fixed, and only the *order* of effects
   is data (send that separately).
3. **`mkBiquadProxy(...)`** — as above plus `type` and, crucially,
   **`getFrequencyResponse()`**, which many prototypes use to draw a filter
   curve. Implement it in JS from the same coefficient formulas the engine
   uses so the drawn curve matches what you hear.
4. **`NVoice(ctx, index)`** — an object with `.port.postMessage()` that
   translates the worklet message protocol into voice field/event messages.

Plus:

- **Batching.** Never send one IPC message per parameter. Queue them and flush
  once per microtask: `Promise.resolve().then(flush)`. A single UI gesture can
  touch 200 parameters; that must be one message.
- **A clock.** The page needs `ctx.currentTime` for scheduling. The engine
  pushes `{t, fs}` about 30×/s; interpolate between pushes with
  `performance.now()` so `currentTime` advances smoothly:
  ```js
  get currentTime() { return NATIVE.t + (performance.now() - NATIVE.at) / 1000; }
  ```
- **A collector mode.** Give the same factory an alternative context that
  *records* every parameter and event instead of sending it. You get offline
  rendering for free: the page builds its schedule exactly as for live
  playback, and you hand the recording to a private engine instance.

## C++ side

A single `Command` struct through a lock-free FIFO
(`juce::AbstractFifo`), drained at the top of `processBlock`:

```cpp
struct Command {
    enum Type : uint8_t { setParamValue, setParamTarget, cancelParam,
                          setVoiceField, voiceEvents, voiceClear, … };
    Type type; int16_t param; int32_t i1, i2; float f1, f2; double d1;
    EventBatch* ptr;   // heap payload for bulk schedules, freed by the sender
};
```

Rules:

- The **message thread** writes; the **audio thread** reads. Never the reverse.
- Bulk payloads (an event schedule of thousands of items) go by pointer. The
  audio thread copies them and sets `consumed = true`; the message thread frees
  them on its next timer tick. Never `delete` on the audio thread.
- Make the FIFO generous (64k entries) and have `pushCommand` retry briefly
  rather than drop: applying a preset can burst thousands of commands, and a
  dropped one is a wrong parameter forever.

## Wiring the page into the plugin

- Embed the page with `juce_add_binary_data`, serve it from a
  `WebBrowserComponent` resource provider, and enable native integration:
  ```cpp
  .withNativeIntegrationEnabled()
  .withResourceProvider([](auto path) { … return makeUiResource(); })
  .withEventListener("ps", [&p](juce::var payload) { p.handleUiMessage(payload); })
  ```
- On Windows pick the WebView2 backend and give it a **writable user-data
  folder** under `%APPDATA%\<Vendor>\<Product>`; the default is next to the
  host executable, which is often not writable.
- JS→C++ is `window.__JUCE__.backend.emitEvent(name, payload)`; C++→JS is
  `browser->emitEventIfBrowserIsVisible(name, payload)` with
  `window.__JUCE__.backend.addEventListener(name, fn)` on the other side.
- Wrap every bridge call in `try/catch` so the page still runs in a plain
  browser (that is how you test and screenshot it).

## Splicing the bridge into the page

Replace whole named functions between markers with a script
(`tools/splice-bridge.ps1`). Typical replacements:

| Original | Becomes |
|---|---|
| `buildChain(ctx, …)` | builds proxies instead of WebAudio nodes |
| `wireFXOrder(g)` | sends one order/enabled-bits message |
| `refreshReverb(g)` | asks the engine to regenerate its impulse |
| `makeContext()` / `initAudio()` | returns the always-running native context |
| recorder setup | native record start/stop/save |
| `renderMidi()` | builds a collector payload, hands it to the engine |

Keep the replacement bodies as separate `.js` files so they are diffable, and
re-run the splice whenever the prototype changes.
