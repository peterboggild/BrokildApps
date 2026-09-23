# 03 · Porting the DSP so it sounds the same

Treat the Web Audio API as a **specification you must reproduce**, not as an
approximation you are free to improve. Every place you "do it better" is a
place the plugin stops sounding like the prototype.

## Port worklets line by line

An `AudioWorkletProcessor` maps almost directly onto a C++ class. Keep:

- the same variable names, so the two can be diffed;
- the same constants (`0.9985`, `3.4657`, `0.75`, …) — do not round them;
- the same order of operations, including where clamps happen;
- the same state layout (a filter's `z[]`, an oscillator's phase).

`Math.tanh`, `Math.exp`, `Math.pow` → `std::tanh`, `std::exp`, `std::pow` on
`double`. Keep the JS `double` precision for state variables; use `float` only
at the buffer boundary.

### The 128-sample cadence matters

A worklet's `process()` runs once per 128-sample render quantum, so anything
computed at the top of `process()` updates at ~375 Hz, not per sample. If you
move that per-sample "for accuracy", you change the sound — smoothing
coefficients, LFO steps and drift walks all shift.

Replicate it explicitly:

```cpp
if (blockCounter == 0) {
    // everything the worklet computed once per process() call
    drift = clamp(drift * 0.9985 + (rand01() - 0.5) * 0.02, -1, 1);
    lfo  += 6.2831853 * rate * 128.0 / fs;
    …
    blockCounter = 128;
}
--blockCounter;
```

## Reproduce WebAudio node semantics

### `AudioParam.setTargetAtTime(target, startTime, timeConstant)`

A one-pole exponential approach, **not** a linear ramp:

```
v(t) = target + (v0 - target) * exp(-(t - startTime) / timeConstant)
```

Per block of `n` samples: `current += (target - current) * (1 - exp(-n / (tc * fs)))`.
`setValueAtTime` snaps. Both may be scheduled in the future, so a parameter
needs a small time-ordered schedule queue, not just a target.

### `BiquadFilterNode`

Use the WebAudio formulas, and note the two traps:

- For `lowpass` and `highpass`, **Q is in decibels**: `q = 10^(Q/20)`. For
  `bandpass`, `notch`, `allpass`, `peaking` it is linear. Getting this wrong
  makes every filter the wrong width.
- `peaking` gain is in dB: `A = 10^(gain/40)`.
- **Clamp the frequency away from 0 Hz.** At f = 0 the biquad degenerates and
  the signal can vanish. A swept all-pass (phaser) driven by an LFO will go
  below zero at high depth; the browser clamps internally, so if you do not,
  your phaser periodically goes silent. Clamp to ≥ ~20 Hz and ≤ `0.49 * fs`.

### `StereoPannerNode`

For a stereo input the pan law is:

```cpp
const float x = pan <= 0 ? pan + 1.0f : pan;
const float gL = std::cos (halfPi * x), gR = std::sin (halfPi * x);
if (pan <= 0) { outL = inL + inR * gL;  outR = inR * gR; }
else          { outL = inL * gL;        outR = inR + inL * gR; }
```

Mono input uses a different (equal-power) law. Use the one that matches how
the prototype fed the node.

### `ConvolverNode`

`normalize` defaults to **true**, and the normalisation is a specific
calibrated formula — an impulse ported without it is dramatically the wrong
level:

```cpp
power = sqrt(sum(ir^2) / (channels * frames));
scale = 0.00125 / max(power, 0.000125) * (44100.0 / fs);
```

Load with `Normalise::no` afterwards, since you have already done it.

### `WaveShaperNode`

A lookup table over [-1, 1] with `oversample: "4x"`. In C++, apply the transfer
function directly inside a `juce::dsp::Oversampling` block — same curve, same
oversampling factor.

### `DynamicsCompressorNode`

There is no exact public algorithm. Approximate it (threshold, knee, ratio,
attack, release, plus its automatic makeup gain) and **write down in the parity
document that it is an approximation**. If it is only a safety catcher, that is
fine; if it is doing musical work, consider replacing it with something you can
match on both sides.

## Randomness

Anything that must be reproducible — generated impulse responses, noise beds,
demo images — must use a seeded PRNG with the **same algorithm** as the
original. Prototypes commonly use an LCG:

```
s = (1664525 * s + 1013904223) >>> 0;   // JS
s = 1664525u * s + 1013904223u;         // C++ (uint32_t wraps identically)
value = s / 4294967296.0 * 2 - 1;
```

That gives bit-identical impulses. Where the original used `Math.random()` for
genuinely aleatoric things (analogue drift, crackle), any decent PRNG is fine —
the statistics are what matter.

## Gain staging

Copy the prototype's staging constants exactly (`OSC_GAIN`, `OUT_GAIN`, voice
normalisation like `1/sqrt(voices)`, per-note trims). These are the difference
between "the same patch" and "the same patch but 4 dB louder and clipping".

## Verifying parity

- Play both side by side on the same settings and listen for level, brightness
  and stereo width.
- For anything deterministic, compare numerically: render the same short
  sequence from the prototype (`OfflineAudioContext` → WAV) and from the
  plugin's offline render, and diff the files.
- When a difference appears, suspect in this order: Q units, missing clamp,
  per-block vs per-sample cadence, gain staging, smoothing time constants.
