#pragma once
/*  ONE definition of "how loud is this preset", shared by the bench and by
    the leveller.  Two copies of a rule like this drift, and then the tool
    that sets the trims and the check that polices them disagree for ever.

    The bank holds drones AND 60 ms plucks, so a fixed window cannot serve:
    rms over 0.2-1.8 s reported SHORT FALL at 0.0003 because the note was
    long gone before the window opened, which is the preset being short, not
    the preset being quiet.  The loudest sliding window measures the sound
    at its own moment, whenever that is.                                    */
#include <vector>
#include <cmath>
#include <algorithm>

namespace gwl {

//  200 ms: long enough to average a 41 Hz cycle eight times over, short
//  enough that a brief pluck is not diluted by the silence after it.
inline double loudness (const std::vector<float>& v, double sr)
{
    const int win = (int) (sr * 0.200);
    const int n   = (int) v.size();
    if (n < win || win <= 0) return 0.0;
    double s = 0.0;
    for (int i = 0; i < win; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
    double best = s;
    for (int i = win; i < n; ++i) {
        s += (double) v[(size_t) i] * v[(size_t) i];
        s -= (double) v[(size_t) i - win] * v[(size_t) i - win];
        if (s > best) best = s;
    }
    return std::sqrt (std::max (0.0, best) / win);
}

inline double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-9)); }

} // namespace gwl
