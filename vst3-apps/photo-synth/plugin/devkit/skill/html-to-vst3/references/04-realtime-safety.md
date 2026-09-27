# 04 · Real-time safety

The audio thread has a hard deadline (a 128-sample buffer at 48 kHz is 2.7 ms).
Miss it and the user hears a click. Everything below is about never missing it.

## Forbidden in `processBlock` and anything it calls

- **Allocation or deallocation** — `new`, `delete`, `malloc`, `std::vector`
  growth, `juce::String`, `std::function` assignment, `std::map` insertion.
- **Locks** — mutexes, `CriticalSection`, anything that can block.
- **File or network I/O**, `DBG`, `printf`, logging.
- **Calls into the WebView or the message thread.**
- **Unbounded loops** — anything whose length depends on user input.

Allocate everything in `prepareToPlay`: buffers, delay lines, voice state,
schedule storage. `prepareToPlay` runs on the message thread with audio
stopped, so it may allocate freely.

## The command queue

One `juce::AbstractFifo` of POD commands, drained at the top of `processBlock`:

```cpp
void Engine::process (float* L, float* R, int n) {
    drainCommands();
    int done = 0;
    while (done < n) {                      // split into short sub-blocks so
        const int m = juce::jmin (32, n - done);   // parameter smoothing and
        processSub (L + done, R + done, m);        // scheduled events land
        done += m;                                  // close to their true time
        currentFrame += m;
    }
}
```

Sub-blocks of 32 samples give you sample-ish-accurate parameter changes without
per-sample overhead. Sample-accurate *events* (note on/off) are handled inside
the voice by comparing against an absolute frame counter.

## Bulk payloads

An event schedule can be thousands of entries — too big for the FIFO. Send a
pointer to a heap `EventBatch` that the **message thread owns**:

1. Message thread `new`s it, pushes the pointer, keeps it in a list.
2. Audio thread copies the events into preallocated per-voice storage and sets
   `batch->consumed = true` (an `std::atomic<bool>`).
3. Message thread, on its next timer tick, deletes everything marked consumed.

Never `delete` on the audio thread; never let the audio thread take ownership.

## Voices

Preallocate the full pool. Each voice needs:

- a **fixed-capacity** event array (`std::array<VoiceEvent, 4096>`), never a
  growing container;
- an **idle short-circuit**: a voice whose envelope has died and which has no
  event due contributes exact silence — return early and skip its DSP entirely.
  Snap its smoothers to their targets first so it wakes up in the right state,
  and require a couple of live blocks first so filter tails flush. This is what
  makes a 16-voice pool cost only what is actually sounding.

## MIDI

Preserve sample offsets — that is the whole point of native MIDI:

```cpp
for (const auto meta : midiMessages) {
    const auto msg = meta.getMessage();
    const int off = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
    if (msg.isNoteOn())  engine.hostNoteOn  (msg.getNoteNumber(), off);
    …
}
midiMessages.clear();
```

Note allocation (mono/unison, polyphonic stealing) mirrors whatever the
prototype's UI did, so the plugin and the on-screen keyboard behave alike.

## Denormals and index growth

- `juce::ScopedNoDenormals` at the top of `processBlock`.
- Circular-buffer write indices grow forever. Fold them periodically
  (`if (w > (1 << 30)) w -= (1 << 29);`) — an `int` overflowing to negative
  turns `& mask` into an out-of-bounds read.

## Sample-rate changes

`prepareToPlay` can be called again with a new rate while the plugin already
holds state. Keep atomic **shadow copies** of every parameter and voice field
on the message-thread side, and on a re-prepare restore them into the freshly
sized DSP instead of resetting to defaults. Otherwise changing the sample rate
in the DAW silently resets the patch.

## Checking your work

- Build with warnings on and read the ones in your own files.
- In the DAW, watch the CPU meter while switching effects on and off — a
  module that is "bypassed" but still processing shows up immediately.
- Automate a parameter hard and listen for zipper noise: that means a value is
  being applied per block instead of smoothed.
- Load and unload the plugin repeatedly, and open/close the editor repeatedly;
  leaks and dangling callbacks surface fast.
