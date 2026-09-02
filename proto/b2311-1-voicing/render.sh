#!/bin/sh
#  render.sh — the seven takes the voicing findings are written from.
#
#  Each one changes ONE thing from the take above it, so the difference you
#  hear is the difference named in the filename and nothing else. Same
#  specimen throughout, so the body is the same body in all seven.
set -e
OUT=${1:-renders}
mkdir -p "$OUT"
S="--seconds 8 --sr 44100 --specimen 0"

#  1. The object as it ships: cascades resolve in an instant, one kernel band
#     high up, parity sign on. This is the take the complaint is about.
./counting $S --speed 0     --kernello 1000 --kernelhi 3000 --parity 1 \
    --wav "$OUT/01-as-shipped.wav"

#  2. Conduction given a speed. Nothing else moves. Cascades now take as long
#     as they are large.
./counting $S --speed 0.002 --kernello 1000 --kernelhi 3000 --parity 1 \
    --wav "$OUT/02-cascades-take-time.wav"

#  3. The kernel floor dropped from 1 kHz to 30 Hz — a register the object
#     has never had.
./counting $S --speed 0.002 --kernello 30   --kernelhi 4000 --parity 1 \
    --wav "$OUT/03-low-register.wav"

#  4. The parity sign flip off. With cascades spread in time, a coherent
#     cascade is now allowed to add rather than cancel.
./counting $S --speed 0.002 --kernello 30   --kernelhi 4000 --parity 0 \
    --wav "$OUT/04-parity-off.wav"

#  5. Slow conduction: events of a quarter of a second, and the object stops
#     being percussive at all.
./counting $S --speed 0.006 --kernello 20   --kernelhi 3000 --parity 0 \
    --wav "$OUT/05-long-and-slow.wav"

#  6. The rate band raised and narrowed until the body locks. The counting is
#     the tone; there is still no oscillator anywhere in it.
./counting $S --speed 0.0002 --ratelo 30 --ratehi 90 --kernello 30 --kernelhi 4000 --parity 0 \
    --wav "$OUT/06-crossing-into-pitch.wav"

#  7. Led by an outside pulse at three a second, gently.
./counting $S --speed 0.002 --kernello 30   --kernelhi 4000 --parity 0 \
    --pulse 3 --grip 0.05 --wav "$OUT/07-led-by-a-pulse.wav"

ls -la "$OUT"
