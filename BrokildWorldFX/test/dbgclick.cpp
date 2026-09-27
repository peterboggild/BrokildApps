// scratch probe: locate the click sample in the enable transition
#include "../src/bwfx.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>

using namespace bwfx;

int main()
{
    const double fs = 48000;
    const int N = 96000;
    std::vector<float> L (N), R (N);
    auto fill = [&] { for (int i = 0; i < N; ++i) { L[(size_t) i] = 0.3f * (float) std::sin (6.2831853 * 440.0 * i / fs); R[(size_t) i] = L[(size_t) i] * 0.8f; } };

    int tTube = -1, tDelay = -1;
    for (int t = 0; t < numModuleTypes(); ++t)
    {
        if (std::string (moduleDescriptor (t).id) == "saturation") tTube = t;
        if (std::string (moduleDescriptor (t).id) == "delay") tDelay = t;
    }

    Rack r;
    r.prepare (fs, 512);
    r.setEnabled (tTube, true);
    fill();
    for (int done = 0; done < N / 2; done += 128) r.process (L.data() + done, R.data() + done, 128);
    r.setEnabled (tDelay, true);
    r.setParam (tDelay, 0, 60.0f);
    for (int done = N / 2; done < N; done += 128) r.process (L.data() + done, R.data() + done, 128);

    // biggest steps around the transition
    struct S { int i; float d; };
    std::vector<S> steps;
    for (int i = N / 2 - 100; i < N / 2 + 14400; ++i)
        steps.push_back ({ i, std::abs (L[(size_t) i] - L[(size_t) (i - 1)]) });
    std::sort (steps.begin(), steps.end(), [] (const S& a, const S& b) { return a.d > b.d; });
    for (int k = 0; k < 8; ++k)
        std::printf ("step %.4f at i=%d (rel %+d)\n", (double) steps[(size_t) k].d, steps[(size_t) k].i, steps[(size_t) k].i - N / 2);
    const int c = steps[0].i;
    for (int i = c - 6; i <= c + 6; ++i)
        std::printf ("  L[%+d] = %+.5f\n", i - N / 2, (double) L[(size_t) i]);
    return 0;
}
