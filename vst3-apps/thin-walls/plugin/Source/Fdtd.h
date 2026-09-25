#pragma once
/*  THIN WALLS - the low end as a wave (ULTRA + BASS).

    Below a couple of hundred hertz a room is not rays and images: it is a
    handful of standing waves, and sound bends round anything the size of a
    door. The geometric model cannot have room modes at all. So offline, the
    whole apartment is simulated as air on a 3-D grid - the standard leapfrog
    finite-difference scheme at the Courant limit (Kowalczyk and van Walstijn's
    "SLF"), about 10 cm per cell, one cell per 8 samples of the host rate - and
    the actual source signals are played into it at their actual positions, as
    they move, with the doors at their actual apertures as they swing. Two
    points 17.5 cm apart where the ears are read the pressure.

    Walls, floors, ceilings, furniture and closed door leaves are locally
    reacting boundaries whose admittance comes from the material's absorption
    in the two lowest octaves. Sound THROUGH walls and leaves is left to the
    geometric model, which has the measured transmission losses; this is the
    air only.

    A source is injected so that in free space the pressure at r is its signal
    over r, exactly the engine's direct path: the two halves meet at the
    crossover on the same scale.

    Plain C++, no JUCE.
*/

#include "Engine.h"
#include <atomic>
#include <vector>

namespace tw
{

struct FdtdStats
{
    double dx = 0;              // metres per cell
    double rate = 0;            // grid updates per second
    long long cells = 0;        // air cells
    long long steps = 0;
    int rebuilds = 0;           // geometry changes during the take
    double seconds = 0;         // wall-clock
};

/*  Simulate the take's low end. blocks: the full parameters at every parameter
    block of pblock samples; mono[s]: what source s is fed (empty when unused);
    n: samples. Writes the pressure at each ear, at the host rate, band-limited
    below about 1 kHz (the caller takes what it wants of it with its crossover).
    False when cancelled. */
bool fdtdLowBand (const std::vector<Params>& blocks, int pblock, const std::vector<float>* mono, int n, double fs,
                  std::vector<float>& L, std::vector<float>& R, FdtdStats* stats = nullptr,
                  std::atomic<float>* progress = nullptr, float p0 = 0, float p1 = 1,
                  const std::atomic<bool>* cancel = nullptr);

} // namespace tw
