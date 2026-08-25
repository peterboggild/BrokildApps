// Real oscillator aliasing: ONE voice, no detune/drift/tolerance/spread, and
// the filter wide open. The audit's table runs all 16 clones, so every other
// clone's fundamental lands off the harmonic grid and counts as "aliasing" -
// which floors that measurement around -47 dB whatever the oscillator does.
#include "cw_core.h"
#include <complex>
#include <cstdio>
#include <cmath>
#include <vector>

static constexpr int kFft = 1 << 16;

static void fft (std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w (1.0, 0.0);
            for (int k = 0; k < len / 2; ++k) {
                const auto u = a[(size_t) (i + k)], v = a[(size_t) (i + k + len / 2)] * w;
                a[(size_t) (i + k)] = u + v;
                a[(size_t) (i + k + len / 2)] = u - v;
                w *= wl;
            }
        }
    }
}

static double aliasDb (const std::vector<float>& x, size_t from, double f0, double fs)
{
    std::vector<std::complex<double>> a (kFft);
    for (int i = 0; i < kFft; ++i) {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (kFft - 1));   // Hann
        a[(size_t) i] = std::complex<double> (x[from + (size_t) i] * w, 0.0);
    }
    fft (a);
    const double binHz = fs / kFft;
    const size_t half = kFft / 2;
    std::vector<char> isHarm (half, 0);
    for (int k = 1; k * f0 < fs / 2; ++k) {
        const int b = (int) std::lround (k * f0 / binHz);
        for (int d = -5; d <= 5; ++d)
            if (b + d >= 0 && b + d < (int) half) isHarm[(size_t) (b + d)] = 1;
    }
    double harm = 0, other = 0;
    for (size_t i = (size_t) (25.0 / binHz); i < half; ++i) {
        const double m = std::abs (a[i]);
        (isHarm[i] ? harm : other) += m * m;
    }
    return 10.0 * std::log10 (other / (harm + other + 1e-30) + 1e-30);
}

int main()
{
    const int fs = 48000;
    const char* wn[] = { "saw", "pulse", "tri" };
    printf ("aliasing, ONE clone, nothing detuned, filter open (dB, lower better)\n\n");
    printf ("  wave    foot         LOW        HQ       XHQ\n");
    for (int wave = 0; wave < 3; ++wave)
    for (int foot = 4; foot <= 5; ++foot)            // 4' and 2': where it bites
    {
        printf ("  %-6s  %s   ", wn[wave], foot == 5 ? "2'" : "4'");
        for (int q = 0; q < 3; ++q)
        {
            cw::Engine e;
            e.prepare (fs, 512);
            e.setGlobal (cw::gHq, (float) q);
            e.setGlobal (cw::gTolerance, 0.f);
            e.setGlobal (cw::gDriftMaster, 0.f);
            e.setGlobal (cw::gSpread, 0.f);
            e.setGlobal (cw::gTide, 0.f);
            e.setGlobal (cw::gEnvFilt, 0.f);
            e.setGlobal (cw::gBusSat, 0.f);
            e.setGlobal (cw::gDriveAmt, 0.f);
            e.setGlobal (cw::gSpringMix, 0.f);
            e.setGlobal (cw::gTapeMix, 0.f);
            e.setGlobal (cw::gBbdDepth, 0.f);
            e.setGlobal (cw::gKbdA, 0.f); e.setGlobal (cw::gKbdB, 0.f);
            for (int v = 0; v < cw::kVoices; ++v) {
                e.setVoice (v, cw::vfMute, v == 0 ? 0.f : 1.f);
                e.setVoice (v, cw::vfWave, (float) wave);
                e.setVoice (v, cw::vfFoot, (float) foot);
                e.setVoice (v, cw::vfTune, 0.f);
                e.setVoice (v, cw::vfDrift, 0.f);
                e.setVoice (v, cw::vfCut, 1.f);
                e.setVoice (v, cw::vfRes, 0.f);
                e.setVoice (v, cw::vfLfoAmp, 0.f);
                e.setVoice (v, cw::vfLfoFlt, 0.f);
                e.setVoice (v, cw::vfEnvA, 1.f);
            }
            e.noteOn (69);                            // A4 -> 440 Hz base
            const size_t n = (size_t) fs * 3;
            std::vector<float> L (n, 0.f), R (n, 0.f);
            size_t done = 0;
            while (done < n) {
                const int m = (int) std::min<size_t> (512, n - done);
                e.process (L.data() + done, R.data() + done, m); done += (size_t) m;
            }
            const double f0 = 440.0 * (foot == 5 ? 4.0 : 2.0);
            printf ("%8.1f  ", aliasDb (L, n - kFft - 1000, f0, fs));
        }
        printf ("\n");
    }
    return 0;
}
