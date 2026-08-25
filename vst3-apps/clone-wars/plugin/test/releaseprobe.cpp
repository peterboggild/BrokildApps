// (1) Does a released note slide down to the army base? Track the pitch of a
//     single clone through note-on, hold, release.
// (2) Does MASTER CUT sweep the whole machine the way a filter knob should?
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static double mag (const std::vector<float>& x, size_t from, size_t len, double f, int fs)
{
    const double w = 2.0 * M_PI * f / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < from + len && i < x.size(); ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) len;
}

int main()
{
    const int fs = 48000;

    // ---- (1) pitch through the release --------------------------------------
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gTolerance, 0.f); e.setGlobal (cw::gDriftMaster, 0.f);
        e.setGlobal (cw::gSpread, 0.f);    e.setGlobal (cw::gTide, 0.f);
        e.setGlobal (cw::gDrone, 0.f);     e.setGlobal (cw::gNoteMode, 0.f);   // unison
        for (int v = 0; v < cw::kVoices; ++v) {
            e.setVoice (v, cw::vfMute, v == 0 ? 0.f : 1.f);
            e.setVoice (v, cw::vfFoot, 3.f);            // 8' = as played
            e.setVoice (v, cw::vfWave, 3.f);            // sine: easy to track
            e.setVoice (v, cw::vfCut, 1.f); e.setVoice (v, cw::vfRes, 0.f);
            e.setVoice (v, cw::vfLfoAmp, 0.f); e.setVoice (v, cw::vfLfoFlt, 0.f);
        }
        const size_t n = (size_t) fs * 6;
        std::vector<float> L (n, 0.f), R (n, 0.f);
        auto run = [&] (size_t from, size_t len) {
            size_t done = from;
            while (done < from + len) {
                const int m = (int) std::min<size_t> (512, from + len - done);
                e.process (L.data() + done, R.data() + done, m); done += (size_t) m;
            }
        };
        e.noteOn (69);                       // A4 = 440
        run (0, (size_t) fs * 2);
        e.noteOff (69);
        run ((size_t) fs * 2, (size_t) fs * 4);

        printf ("one clone, UNISON, DRONE off, 8' - A4 held 2 s then released\n");
        printf ("   time    strongest partial\n");
        for (double t : { 1.0, 2.5, 3.5, 4.5, 5.5 })
        {
            const size_t from = (size_t) (t * fs);
            double best = 0, bestF = 0;
            for (int c = -3600; c <= 1200; c += 10) {          // 3 oct down .. 1 up
                const double f = 440.0 * std::pow (2.0, c / 1200.0);
                const double m = mag (L, from, 8192, f, fs);
                if (m > best) { best = m; bestF = f; }
            }
            printf ("   %.1f s   %7.1f Hz  %+6.0f cents vs A4%s\n", t, bestF,
                    1200.0 * std::log2 (bestF / 440.0), t < 2.0 ? "  (held)" : "  (released)");
        }
    }

    // ---- (2) MASTER CUT sweep ------------------------------------------------
    printf ("\nMASTER CUT, default drone, all 16 clones (share of energy above 2 kHz)\n");
    for (double mc : { 0.0, 0.25, 0.5, 0.75, 1.0 })
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gDrone, 1);
        e.setGlobal (cw::gCutoff, (float) mc);
        const size_t n = (size_t) fs * 3;
        std::vector<float> L (n, 0.f), R (n, 0.f);
        size_t done = 0;
        while (done < n) { const int m = (int) std::min<size_t> (512, n - done);
                           e.process (L.data() + done, R.data() + done, m); done += (size_t) m; }
        double num = 0, den = 0, hi = 0;
        for (int b = 0; b < 48; ++b) {
            const double f = 40.0 * std::pow (16000.0 / 40.0, b / 47.0);
            const double m = mag (L, n / 2, 32768, f, fs);
            num += m * f; den += m; if (f > 2000.0) hi += m;
        }
        printf ("   MASTER CUT %.2f : centroid %6.0f Hz   >2kHz %5.1f %%\n",
                mc, den > 0 ? num / den : 0.0, den > 0 ? 100.0 * hi / den : 0.0);
    }
    return 0;
}
