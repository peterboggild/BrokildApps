// "It never goes quite bright" — with every CUTOFF and MASTER CUT at max, how
// much top end does the chain still eat, and WHERE? Renders one clone (saw,
// 8'), all-bright, and measures third-octave energy vs progressively disabled
// stages: default FX -> FX off -> tolerance off.
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static double mag (const std::vector<float>& x, size_t from, size_t len, double f, int fs)
{
    const double w = 2.0 * M_PI * f / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < from + len && i < x.size(); ++i)
    { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) len;
}

static void render (cw::Engine& e, std::vector<float>& L, int fs, double secs)
{
    const size_t n = (size_t) (secs * fs);
    std::vector<float> R (n, 0.f); L.assign (n, 0.f);
    size_t done = 0;
    while (done < n) { const int m = (int) std::min<size_t> (512, n - done);
                       e.process (L.data() + done, R.data() + done, m); done += (size_t) m; }
}

static void setup (cw::Engine& e, int fs, bool fxOff, bool tolOff, float envf)
{
    e.prepare (fs, 512);
    e.setGlobal (cw::gDrone, 1);
    e.setGlobal (cw::gCutoff, 1.0f);          // MASTER CUT max
    e.setGlobal (cw::gEnvFilt, envf);
    e.setGlobal (cw::gHq, 2);
    if (tolOff) e.setGlobal (cw::gTolerance, 0.f);
    if (fxOff) {
        e.setGlobal (cw::gBusSat, 0.f);  e.setGlobal (cw::gDriveAmt, 0.f);
        e.setGlobal (cw::gSpringMix, 0.f); e.setGlobal (cw::gTapeMix, 0.f);
        e.setGlobal (cw::gBbdDepth, 0.f);
    }
    for (int v = 0; v < cw::kVoices; ++v) {
        e.setVoice (v, cw::vfMute, v == 0 ? 0.f : 1.f);
        e.setVoice (v, cw::vfWave, 0.f);      // saw
        e.setVoice (v, cw::vfFoot, 3.f);      // 8'
        e.setVoice (v, cw::vfCut, 1.0f);      // strip CUTOFF max
        e.setVoice (v, cw::vfRes, 0.f);
        e.setVoice (v, cw::vfLfoAmp, 0.f); e.setVoice (v, cw::vfLfoFlt, 0.f);
        e.setVoice (v, cw::vfDrift, 0.f);
    }
}

int main()
{
    const int fs = 48000;
    const double f0 = 110.0;               // A2 drone (baseA default 0.5 -> midi 42? use drone base)
    // measure band energies as dB relative to an ideal saw's own rolloff:
    // a saw's harmonic k has amplitude 1/k, so band energy ~ known; instead
    // compare configs against config[0] band by band.
    struct Cfg { const char* name; bool fxOff, tolOff; float envf; };
    const Cfg cfgs[] = {
        { "default FX, TOL 0.35, ENV>FILT 0.45", false, false, 0.45f },
        { "FX off                            ", true,  false, 0.45f },
        { "FX off + TOL 0                    ", true,  true,  0.45f },
        { "FX off + TOL 0 + ENV>FILT 0       ", true,  true,  0.0f },
    };
    const double bands[] = { 500, 1000, 2000, 4000, 8000, 12000, 16000 };
    std::vector<std::vector<double>> res;
    for (const auto& c : cfgs)
    {
        cw::Engine e;
        setup (e, fs, c.fxOff, c.tolOff, c.envf);
        std::vector<float> L;
        render (e, L, fs, 3.0);
        // find the drone fundamental first (base pitch dependent), scan 40..200
        double bf = 0, bm = 0;
        for (double f = 40; f < 200; f += 0.5) { const double m = mag (L, L.size()/2, 32768, f, fs); if (m > bm) { bm = m; bf = f; } }
        std::vector<double> row;
        row.push_back (bf);
        for (double b : bands) {
            // sum a few harmonics nearest each band centre
            double s = 0;
            for (int k = (int) (b / bf) - 1; k <= (int) (b / bf) + 1 && k >= 1; ++k)
                s += mag (L, L.size()/2, 32768, k * bf, fs);
            row.push_back (20.0 * std::log10 (s + 1e-12));
        }
        res.push_back (row);
    }
    printf ("one clone, saw 8', everything at MAX bright. Band level in dB (f0=%.1f Hz)\n\n", res[0][0]);
    printf ("  config                                   500   1k    2k    4k    8k   12k   16k\n");
    for (size_t i = 0; i < res.size(); ++i)
    {
        printf ("  %s", cfgs[i].name);
        for (size_t b = 1; b < res[i].size(); ++b) printf (" %5.1f", res[i][b]);
        printf ("\n");
    }
    printf ("\n  deltas vs the first row (positive = brighter without that stage):\n");
    for (size_t i = 1; i < res.size(); ++i)
    {
        printf ("  %s", cfgs[i].name);
        for (size_t b = 1; b < res[i].size(); ++b) printf (" %+5.1f", res[i][b] - res[0][b]);
        printf ("\n");
    }
    return 0;
}
