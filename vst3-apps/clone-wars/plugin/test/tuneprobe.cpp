// Does the resonant peak / self-oscillation land on the cutoff frequency?
// Black Rider's prewarp is ported; this checks it survived the transplant.
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static double mag (const std::vector<float>& x, size_t from, double f, int fs)
{
    const double w = 2.0 * M_PI * f / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < x.size(); ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (x.size() - from);
}

int main()
{
    const int fs = 48000;
    const char* names[] = { "GROWL ", "SCREAM", "LADDER" };
    printf ("resonant peak vs the cutoff it was asked for (ENV>FILT 0, one voice)\n\n");
    printf ("   model   target      peak     error\n");
    for (int temper = 0; temper < 3; ++temper)
    for (double targetHz : { 110.0, 220.0, 440.0, 880.0 })
    {
        const float cut = (float) (std::log (targetHz / 30.0) / std::log (533.0));
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gDrone, 1);
        e.setGlobal (cw::gTemperA, (float) temper);
        e.setGlobal (cw::gTemperB, (float) temper);
        e.setGlobal (cw::gTolerance, 0.f);
        e.setGlobal (cw::gEnvFilt, 0.f);        // knob alone decides the cutoff
        e.setGlobal (cw::gDriftMaster, 0.f);
        e.setGlobal (cw::gBaseA, 0.f); e.setGlobal (cw::gBaseB, 0.f);
        e.setGlobal (cw::gSpread, 0.f);
        e.setGlobal (cw::gTide, 0.f);
        e.setGlobal (cw::gKbdA, 0.f); e.setGlobal (cw::gKbdB, 0.f);
        for (int v = 0; v < cw::kVoices; ++v)
        {
            e.setVoice (v, cw::vfWave, 3.f);   // SINE: no harmonics to be mistaken
            e.setVoice (v, cw::vfFoot, 0.f);   // and put it far below the cutoff
            e.setVoice (v, cw::vfCut, cut);
            e.setVoice (v, cw::vfRes, 1.0f);     // hard into self-oscillation
            e.setVoice (v, cw::vfLfoAmp, 0.f);
            e.setVoice (v, cw::vfLfoFlt, 0.f);
            e.setVoice (v, cw::vfMute, v == 0 ? 0.f : 1.f);   // one clone only
        }
        const size_t n = (size_t) (4.0 * fs);
        std::vector<float> L (n, 0.f), R (n, 0.f);
        size_t done = 0;
        while (done < n) {
            const int m = (int) std::min<size_t> (512, n - done);
            e.process (L.data() + done, R.data() + done, m); done += (size_t) m;
        }
        // scan +/- 6 semitones around the target for the strongest partial
        double best = 0, bestF = targetHz;
        for (int c = -600; c <= 600; c += 4) {
            const double f = targetHz * std::pow (2.0, c / 1200.0);
            const double m = mag (L, n / 2, f, fs);
            if (m > best) { best = m; bestF = f; }
        }
        printf ("  %s  %6.0f Hz  %7.1f Hz  %+6.0f cents\n", names[temper], targetHz, bestF,
                1200.0 * std::log2 (bestF / targetHz));
    }
    return 0;
}
