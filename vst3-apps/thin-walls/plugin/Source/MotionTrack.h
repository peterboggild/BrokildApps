#pragma once
/*  THIN WALLS - smooth motion for the video export.

    WHY: while you walk, the panel sends the listener's position once per frame
    it DRAWS, and the take records exactly what arrives. When the panel draws
    slower than the video's frame rate (a big window, CINEMATIC, a slow GPU) the
    recorded position holds for several video frames and then jumps - measured
    at 56 fps against a 60 fps export, one frame in six or seven was a repeat.
    The export renders every frame faithfully; it was faithfully reproducing a
    walk that had been recorded in steps.

    WHAT: a recorded parameter is turned into a TRACK - one point at every grid
    block where its value changed - and a video frame reads the track at its own
    fractional time:
      * two points closer than maxGap are one movement: blend linearly between
        them, so every frame of a walk lands somewhere new;
      * a longer gap is a real STOP: hold the value;
      * a movement that starts after a stop eases in over one update interval
        (the gap to the point after it) instead of jumping at its first point;
      * an isolated change - a preset, a typed value, a teleport - has no
        neighbour close enough to blend with and stays a snap.
    A circular parameter (a facing, 0..1 = 0..360 degrees) blends the short way
    round and wraps.

    Only the PICTURE uses this. The audio of the export is rendered from the
    recorded values exactly as before, and the engine smooths its own paths.

    Plain C++ with no dependencies, so the bench can measure it.
*/

#include <cmath>
#include <string>
#include <vector>

namespace tw
{

struct MotionTrack
{
    std::vector<double> at;     // grid block of each point (first point is block 0)
    std::vector<float>  v;      // the value that arrived there
    bool circular = false;

    // series: the parameter's raw value per grid block, stride floats apart
    static MotionTrack build (const float* series, int stride, int nblocks, bool circ)
    {
        MotionTrack t;
        t.circular = circ;
        if (nblocks <= 0) return t;
        t.at.push_back (0.0);
        t.v.push_back (series[0]);
        for (int b = 1; b < nblocks; ++b)
        {
            const float x = series[(size_t) b * (size_t) stride];
            if (x != t.v.back()) { t.at.push_back ((double) b); t.v.push_back (x); }
        }
        return t;
    }

    float lerp (float a, float b, double u) const
    {
        if (! circular) return (float) (a + (b - a) * u);
        double d = (double) b - (double) a;
        d -= std::floor (d + 0.5);                     // the short way round, in [-0.5, 0.5)
        double r = (double) a + d * u;
        r -= std::floor (r);                           // wrap into [0, 1)
        return (float) r;
    }

    // tb: fractional grid-block time; maxGap: blocks - longer is a stop
    float valueAt (double tb, double maxGap) const
    {
        const int n = (int) at.size();
        if (n == 0) return 0.0f;
        if (tb <= at[0]) return v[0];
        // last point at or before tb
        int lo = 0, hi = n - 1;
        while (lo < hi) { const int mid = (lo + hi + 1) / 2; if (at[mid] <= tb) lo = mid; else hi = mid - 1; }
        const int k = lo;
        if (k == n - 1) return v[k];
        const double gap = at[k + 1] - at[k];
        if (gap <= maxGap)                              // inside a movement
            return lerp (v[k], v[k + 1], (tb - at[k]) / gap);
        // a stop - hold, unless the NEXT point starts a movement: then ease in
        // over one update interval, the spacing of the movement that follows
        if (k + 2 < n)
        {
            const double next = at[k + 2] - at[k + 1];
            if (next <= maxGap)
            {
                const double start = at[k + 1] - next;
                if (tb > start) return lerp (v[k], v[k + 1], (tb - start) / next);
            }
        }
        return v[k];
    }
};

// which recorded parameters are MOTION (drawn as things moving) and which of
// them are angles
inline bool isMotionParam (const std::string& id, bool& circular)
{
    circular = false;
    if (id == "lisx" || id == "lisy") return true;
    if (id == "lisyaw") { circular = true; return true; }
    if (id == "door1" || id == "door2" || id == "door3") return true;
    if (id.size() >= 3 && id[0] == 's' && id[1] >= '1' && id[1] <= '9')
    {
        const std::string rest = id.substr (2);
        if (rest == "x" || rest == "y" || rest == "z") return true;
        if (rest == "yaw") { circular = true; return true; }
    }
    return false;
}

} // namespace tw
