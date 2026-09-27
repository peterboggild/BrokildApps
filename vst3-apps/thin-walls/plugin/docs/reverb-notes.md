# The late field: what was wrong and how it was found

*2026-09-22. Peter, after playing the built plugin: "the sound quality of the studio
is a bit lacking. The reverb and reflections are somewhat unpleasant... perhaps that
is a limit of the simulation strategy. But can you have a look? You dont have ears,
but perhaps you can see something in the model or code that could make the result
more lofi than necessary."*

It was not a limit of the strategy. It was three faults, all measurable, one of them
introduced the day before.

## The two numbers that decide whether a reverberator sounds like a room

**Modal density.** A feedback delay network's own resonances sit `1/L` apart, where
`L` is the total delay in the loop, and each is `2.2/RT60` wide. When the spacing
exceeds the width they are individually resolvable, which is heard as pitched ringing
rather than as a decay. So the loop must satisfy `L > RT60/2.2`.

**Echo density.** Abel and Huang's normalised measure: the fraction of samples in a
window exceeding that window's own standard deviation, divided by `erfc(1/sqrt2)`. It
converges to 1.0 when the individual echoes have merged into a Gaussian wash. Below
that they are still countable, which is heard as grain.

And the direct correlate of "metallic": for a Gaussian impulse response the magnitude
is Rayleigh distributed, so the standard deviation of `|H|` in dB is **5.57 dB**, a
fixed number independent of the room. Anything above that is resolvable structure.

## What was measured

| | before | after |
|---|---|---|
| spectral ripple, worst of nine cases | 10.71 dB | 6.60 dB |
| spectral ripple, typical | 7.6 dB | 5.7 dB |
| total loop delay | 95–242 ms | 288–733 ms, sized per decay |
| required by the criterion | 135–1392 ms | same |
| echo density at 50 ms, the hall | 0.51 | 0.61 |
| decay error against Eyring, worst | +20 % | a few per cent |
| cost, one source | 10 % of a core | 19 % |

## The three faults

1. **The network was too small for its decays.** 95–242 ms of loop against the
   135–321 ms the studio needs and the 1.4 s bare plaster needs.

2. **The delay modulation had been removed the day before.** It was there to keep the
   resonances moving so they cannot be heard as pitch; it was taken out because
   interpolating inside the loop cost 3–5 dB of the field's energy per pass. The
   diagnosis was right and the trade was wrong — with a windowed-sinc reader,
   modulation is free. **Stationary modes ring; moving ones do not.**

3. **The hall's tail started sparse**, because the injection was smeared by only two
   allpasses before entering the loop.

## The cure, and why it is the right one

Allpass diffusers inside every line. An allpass has unity gain at every frequency, so
it **cannot change the decay**, but its length counts towards the loop's total, which
is the modal density, and it splits every echo into a train, which is the echo
density. It buys both of the things that were short without touching the thing that
was already right.

## Four things it cost to get there

**A diffuser must not outring its room.** An allpass of length `La` and gain `g` rings
at `-20 log10(g)` dB per `La`, so it has a decay of its own: `60La/4.15 = 14.5 La` at
`g = 0.62`. At `La = 7 ms` that is 100 ms, longer than the 85 ms an absorbing room
should take — so the diffusers, not the room, set the tail. Measured 0.22 s where
Eyring says 0.09. The chain is now sized to the decay and switched off entirely for a
room that barely reverberates, with a hard cap of `RT60/43.5` on any one allpass.

**A Schroeder allpass holds the signal for exactly its own length.** The
energy-weighted mean delay is `sum_k k La (1-g^2)^2 g^(2k-2) = La`, with the
`(1-g^2)` cancelling, independent of `g`. Charging the loop for `La/(1-g^2)` instead
made every live decay 27 % short. Worth remembering because it is the sort of factor
one is tempted to put back.

**What is left over, measure.** The decays still ran ~10 % long from a second-order
effect (an allpass spreads energy, and the latest arrivals have had the loss applied
fewer times per second than the earliest). Rather than model it, each room now runs
its own network at four design decays when it starts, with the chain sized for each as
it would be in use, and records how far the measured decay misses the design and how
much energy the steady-state formula really delivers. Both are interpolated in log
decay at runtime, and the result is cached per room and sample rate because it is a
pure function of them. Every one of the fifteen room-and-material cases is now within
a few per cent of Eyring.

**Two of the diagnostics were lying**, both of the misplaced-window family:
- The probe fired its impulse ten samples in, which lands inside a fresh path's gain
  ramp, so every close-range reading it had given was understated — by 11 dB at 0.2 m.
  The bench had been fixed for this weeks earlier and the probe had not.
- The ripple was measured over a fixed window 300 ms after the impulse, which for a
  0.30 s decay is past the end of the tail. And no untreated window works: short
  enough not to span the decay is too short to resolve modes a few hertz apart. The
  decay is divided back out now (`exp(+13.8 t / RT60)`) so the window holds a
  stationary signal.

## And one thing that was not wrong

The reverberant level measured 3–5 dB dry broadband, and the hunt for that was
wasted: broadband mixes octaves whose absorption differs by many dB, and the top
octaves, which carry most of a flat spectrum, are the most absorbent. `16 pi / A` is a
statement about one frequency. Measured in the 1 kHz octave the direct-to-reverberant
ratio at the critical distance lands within about a decibel of theory for all five
materials. **Two bench checks were then corrected rather than the engine**: the ratio
is measured with the parts rendered in isolation rather than through a 2.5 ms window
that clipped a 2.9 ms head response, and the decay check asserts decay rather than an
absolute floor the party-wall transmission legitimately exceeds.

## Still open

- Echo density in the hall's first 50 ms is 0.61 rather than 1.0. The early
  reflections cover that region in the full render, so it may not matter; it has not
  been judged by ear.
- Sixteen lines is few. A larger network would satisfy the modal criterion for bare
  plaster and glazed tile without leaning on the modulation, which currently carries
  those two.
- Every room's field runs even when nobody is in it, because the coupling needs it.
  Skipping a silent, empty room would give most of the 19 % back.
