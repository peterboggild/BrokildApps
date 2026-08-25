// Voice-chain frequency response at CUT max: play a SINE at rising pitches
// through one clone and measure the fundamental's level. Any slope is the
// chain's own transfer (filter + decimator). Per quality tier.
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

int main()
{
    const int fs = 48000;
    printf ("sine through one clone, CUT 1.0, MASTER CUT 1.0, RES 0, level of the\n");
    printf ("fundamental in dB rel. its level at 220 Hz - flat would be all zeros\n\n");
    printf ("   note freq      LOW      HQ     XHQ\n");
    const int notes[] = { 57, 69, 81, 93, 99, 105, 111 };   // 220..7040ish
    double ref[3] = { 0, 0, 0 };
    for (int ni = 0; ni < 7; ++ni)
    {
        const int note = notes[ni];
        const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        printf ("  %5.0f Hz   ", f0);
        for (int q = 0; q < 3; ++q)
        {
            cw::Engine e;
            e.prepare (fs, 512);
            e.setGlobal (cw::gHq, (float) q);
            e.setGlobal (cw::gCutoff, 1.f);
            e.setGlobal (cw::gEnvFilt, 0.f);
            e.setGlobal (cw::gTolerance, 0.f); e.setGlobal (cw::gDriftMaster, 0.f);
            e.setGlobal (cw::gSpread, 0.f); e.setGlobal (cw::gTide, 0.f);
            e.setGlobal (cw::gBusSat, 0.f); e.setGlobal (cw::gDriveAmt, 0.f);
            e.setGlobal (cw::gSpringMix, 0.f); e.setGlobal (cw::gTapeMix, 0.f);
            e.setGlobal (cw::gBbdDepth, 0.f); e.setGlobal (cw::gKbdA, 0.f); e.setGlobal (cw::gKbdB, 0.f);
            e.setGlobal (cw::gNoteMode, 0.f);
            for (int v = 0; v < cw::kVoices; ++v) {
                e.setVoice (v, cw::vfMute, v == 0 ? 0.f : 1.f);
                e.setVoice (v, cw::vfWave, 3.f);        // sine
                e.setVoice (v, cw::vfFoot, 3.f);        // 8' = played pitch
                e.setVoice (v, cw::vfCut, 1.f); e.setVoice (v, cw::vfRes, 0.f);
                e.setVoice (v, cw::vfLfoAmp, 0.f); e.setVoice (v, cw::vfLfoFlt, 0.f);
                e.setVoice (v, cw::vfDrift, 0.f); e.setVoice (v, cw::vfEnvA, 1.f);
            }
            e.noteOn (note);
            const size_t n = (size_t) fs * 2;
            std::vector<float> L (n, 0.f), R (n, 0.f);
            size_t done = 0;
            while (done < n) { const int m = (int) std::min<size_t> (512, n - done);
                               e.process (L.data() + done, R.data() + done, m); done += (size_t) m; }
            const double db = 20.0 * std::log10 (mag (L, n / 2, 32768, f0, fs) + 1e-12);
            if (ni == 0) ref[q] = db;
            printf (" %+6.1f ", db - ref[q]);
        }
        printf ("\n");
    }
    return 0;
}
