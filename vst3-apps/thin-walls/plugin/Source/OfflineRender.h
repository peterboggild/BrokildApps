#pragma once
/*  THIN WALLS - rendering a take offline, at a chosen quality.

    The live engine is tuned to fit the audio thread. Offline there is no such
    limit, so a take (the input it heard and everything that moved) can be
    rendered again with less approximation:

      LIVE        the live engine, exactly
      HIGH        image sources to 4th order (the late field hands over what
                  the extra reflections carry - measured to 0.1 dB)
      ULTRA       6th order, and room-specific tails traced through the real
                  geometry in place of the statistical late field
      ULTRA+BASS  ULTRA, with the sound below the crossover simulated as a
                  wave field (room modes, low-frequency diffraction)

    Plain C++, no JUCE, so the bench measures the same code the plug-in runs.
*/

#include "Engine.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace tw
{

enum class SoundQuality { Live = 0, High = 1, Ultra = 2, UltraBass = 3 };

/*  What a take is, seen from the renderer: input channels, the host
    parameters on a fixed grid, furniture/panel layouts as they changed. */
struct TakeView
{
    double rate = 48000.0;
    int length = 0;                            // samples
    const float* in[4] = { nullptr, nullptr, nullptr, nullptr };   // main L, main R, aux L, aux R (aux may be null)
    int pblock = 256;                          // the parameter grid
    int nblocks = 0;
    std::function<void (int block, Params&)> paramsAt;             // fills everything but the furniture
    struct Layout { int sample = 0; int n = 0; const FurnItem* items = nullptr; const float* panelArea = nullptr; };
    std::vector<Layout> layouts;               // in time order
};

struct RenderOptions
{
    SoundQuality quality = SoundQuality::Live;
    std::shared_ptr<const Hrtf> head;          // null: the built-in set
    int fps = 0;                               // > 0: also report the lamps' light per video frame
};

struct RenderOutput
{
    std::vector<float> L, R;
    std::vector<float> light;                  // NUM_ROOMS per video frame, when fps > 0
    std::string note;                          // what the render did, for the status line
    int keyframes = 0;                         // ULTRA: how many times the late field was traced
    double raySeconds = 0;                     // and how long that took
    double fdtdSeconds = 0, fdtdDx = 0;        // ULTRA+BASS: the wave simulation's wall-clock and cell size
    long long fdtdCells = 0;
};

// false when cancelled
bool renderTake (const TakeView& take, const RenderOptions& opt, RenderOutput& out,
                 std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr);

RenderQuality engineQualityFor (SoundQuality q);

} // namespace tw
