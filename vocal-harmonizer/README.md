# LEGION — vocal harmoniser (VST3)

*"My name is Legion, for we are many."* One voice in, a choir of it out: up to
four copies, each with its own pitch AND its own body size, because a voice
shifted without separating those two is a chipmunk and not a singer.

Design authority: [`LEGION-DESIGN.md`](../LEGION-DESIGN.md) at the repo root —
read it before changing anything structural here. Every number it quotes is
something `test/bench.cpp` measures against the real engine.

**Status: built and measured, not yet heard.** 113 engine checks and 22 wrapper
checks pass; nobody has put a scream through it in a DAW. The name is
provisional.

## What it does

- **PITCH** ±24 semitones, **FINE** ±100 cents — per voice.
- **FORMANT** ±12 semitones, *independent of pitch*. Move the singer's body
  without moving the note, or the note without the body.
- **FOLLOW** 0–100 % — how much the body rides the pitch. 0 = the same person
  singing higher; 100 = plain resampling, the chipmunk, on purpose.
- **LEVEL / PAN / DELAY** per voice, **HUMANISE** across them: uncorrelated slow
  detune, level shimmer and timing stagger, which is the difference between four
  singers and one singer that got louder.
- **MIX** — equal-power dry/wet. At MIX 0 the output is the input, delayed by
  the reported latency and otherwise untouched, to the bit.
- **DETAIL** — TIGHT / NATURAL / SMOOTH analysis window (21 / 43 / 85 ms at
  48 k). Lower voices need a longer one; see the table in the design doc.
- **BWFX** on the **HARMONY** bus by default, so the rack grinds the second
  voice and leaves the lead alone. `BWFX ON` switches it to MASTER.

Sounds it is meant to survive: sustained singing, and screaming — rough,
inharmonic, clipped material that a pitch tracker cannot hold. Nothing in the
signal path needs to know what note you are on.

## Layout

    engine/     the DSP. Plain C++17, JUCE-free, benchable offline.
                legion_fft        radix-2 complex FFT, one table for all sizes
                legion_analysis   per-frame analysis: true envelope, f0, peaks
                legion_shifter    one harmony voice's spectral re-draw
                legion_harmonizer four voices, one analysis, two buses out
    src/        the JUCE wrapper: parameters, buses, state, BWFX, the panel.
    test/       bench.cpp    the engine bench, no JUCE (107 checks)
                hosttest.cpp the wrapper harness, no DAW (20 checks)
    tools/      build-win.ps1

## Building

**The bench first, always.** It needs nothing but a compiler:

```sh
cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Release
cmake --build test/build
./test/build/legiontest          # must print ALL CLEAR
```

**The plugin.** JUCE is fetched on first configure; point at a local checkout
with `-DLEGION_JUCE_DIR=...` to stay offline or to pin the version a release
was built with. BWFX is compiled IN from `../BrokildWorldFX` — never a DLL
(BWFX-DESIGN.md: Smart App Control blocks fresh DLL hashes per file, and
patches must stay a promise on every machine).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/legionhost                                   # must print ALL CLEAR
```

On Windows, `tools\build-win.ps1` does the bench, refuses to build the plugin
unless it is clear, then builds the VST3 and tells you where it is. Install by
copying `Legion.vst3` to `C:\Program Files\Common Files\VST3\`.

Linux needs the usual JUCE X11 set to build the plugin (`libx11-dev
libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev
libfontconfig1-dev libasound2-dev libgl1-mesa-dev`). The bench needs none of it.

## Cost and latency

| | |
|---|---|
| four voices, 48 k | ~5.8 × real time (about 17 % of one core) |
| latency | the analysis window: 21.4 / 42.7 / 85.4 ms at 48 k |
| unity transparency | −93.9 dB |
| worst shifted-tone spur | −39 dB |

The latency is reported to the host, so a DAW with delay compensation lines it
up. This is not a live-monitoring plugin.

## Changing the engine

1. Run the bench before touching anything, so you know what clear looks like.
2. Make the change.
3. Run the bench. If a number moved, find out why before deciding it is fine.
4. If the change is a choice between two ways of doing something, MEASURE BOTH
   and put the two numbers in the comment. Half the code in `legion_shifter.cpp`
   is there because the other way was built first and measured worse; the
   comments say by how much, so nobody re-runs the experiment by accident.
