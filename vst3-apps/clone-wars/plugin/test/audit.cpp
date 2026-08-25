// Clone Wars — DSP quality audit.
//
//   g++ -O2 -std=c++17 -I../Source/Core audit.cpp ../Source/Core/cw_core.cpp -o audit
//   ./audit
//
// Measures (with a 64k-point FFT): oscillator/filter aliasing per quality
// tier, clean-path THD, click energy on hard parameter jumps, idle noise,
// DC per seed, stress headroom, tier render speed, per-tier determinism.
// Prints a table and asserts regressions. Exit 0 = pass.

#include "cw_core.h"

#include <chrono>
#include <complex>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

static int failures = 0;
static void check (bool ok, const char* what)
{
    printf ("  %-56s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}

//==============================================================================
static constexpr int kFft = 1 << 16;

static void fft (std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (int k = 0; k < len / 2; ++k)
            {
                const auto u = a[(size_t) (i + k)];
                const auto v = a[(size_t) (i + k + len / 2)] * w;
                a[(size_t) (i + k)] = u + v;
                a[(size_t) (i + k + len / 2)] = u - v;
                w *= wl;
            }
        }
    }
}

static std::vector<double> spectrum (const float* x)
{
    std::vector<std::complex<double>> a ((size_t) kFft);
    for (int i = 0; i < kFft; ++i)
    {
        const double w = 0.5 * (1.0 - std::cos (2.0 * M_PI * i / (kFft - 1))); // Hann
        a[(size_t) i] = x[i] * w;
    }
    fft (a);
    std::vector<double> mag ((size_t) kFft / 2);
    for (int i = 0; i < kFft / 2; ++i) mag[(size_t) i] = std::norm (a[(size_t) i]);
    return mag;
}

// Energy at non-harmonic bins relative to total, in dB. Harmonics of f0 are
// excluded with a ±4-bin skirt (Hann leakage); so is everything below 25 Hz.
static double aliasDb (const std::vector<double>& mag, double f0, double fs)
{
    const double binHz = fs / kFft;
    std::vector<char> isHarm (mag.size(), 0);
    for (int k = 1; k * f0 < fs / 2; ++k)
    {
        const int b = (int) std::lround (k * f0 / binHz);
        for (int d = -4; d <= 4; ++d)
            if (b + d >= 0 && b + d < (int) mag.size()) isHarm[(size_t) (b + d)] = 1;
    }
    double harm = 0, other = 0;
    for (size_t i = (size_t) (25.0 / binHz); i < mag.size(); ++i)
        (isHarm[i] ? harm : other) += mag[i];
    return 10.0 * std::log10 (other / (harm + other + 1e-30) + 1e-30);
}

static double thdDb (const std::vector<double>& mag, double f0, double fs)
{
    const double binHz = fs / kFft;
    auto peakNear = [&] (double f)
    {
        const int b = (int) std::lround (f / binHz);
        double p = 0;
        for (int d = -4; d <= 4; ++d)
            if (b + d >= 0 && b + d < (int) mag.size()) p += mag[(size_t) (b + d)];
        return p;
    };
    const double fund = peakNear (f0);
    double harm = 0;
    for (int k = 2; k <= 12 && k * f0 < fs / 2; ++k) harm += peakNear (k * f0);
    return 10.0 * std::log10 (harm / (fund + 1e-30) + 1e-30);
}

//==============================================================================
// A surgical lab setup: one clone, everything stochastic or coloured is off.
static void labPatch (cw::Engine& e, int wave, int temper, float cut, float res)
{
    using namespace cw;
    Patch p;
    defaultPatch (p);
    p.global[gTolerance] = 0; p.global[gTide] = 0; p.global[gDriftMaster] = 0;
    p.global[gEntrainA] = 0; p.global[gEntrainB] = 0;
    p.global[gSpread] = 0; p.global[gGlide] = 0;
    p.global[gTemperA] = (float) temper; p.global[gTemperB] = (float) temper;
    p.global[gWar] = 0;                    // army A only
    p.global[gSpringMix] = 0; p.global[gTapeMix] = 0; p.global[gTapeFdbk] = 0;
    p.global[gBbdDepth] = 0; p.global[gDriveAmt] = 0; p.global[gBusSat] = 0;
    p.global[gBassMono] = 0; p.global[gHpf] = 0; p.global[gWidth] = 0.5f;
    p.global[gMaster] = 0.55f;             // stays under the safety knee
    p.global[gDrone] = 0;
    for (int v = 0; v < kVoices; ++v)
    {
        for (int f = 0; f < numVoiceFields; ++f) p.voice[v][f] = 0;
        p.voice[v][vfMute] = 1;
        p.voice[v][vfLevel] = 0;
    }
    float* f = p.voice[0];
    f[vfMute] = 0; f[vfLevel] = 0.8f;
    f[vfWave] = (float) wave; f[vfFoot] = 2;   // 8'
    f[vfCut] = cut; f[vfRes] = res;
    f[vfEnvA] = 0; f[vfEnvF] = 0;              // fast attack
    f[vfNote] = 0;
    e.applyPatch (p);
    e.setUnitSeed (1);
}

static void render (cw::Engine& e, std::vector<float>& L, std::vector<float>& R, int fs, double sec)
{
    const size_t n = (size_t) (sec * fs), start = L.size();
    L.resize (start + n); R.resize (start + n);
    size_t done = 0;
    while (done < n)
    {
        const int m = (int) std::min<size_t> (512, n - done);
        e.process (L.data() + start + done, R.data() + start + done, m);
        done += (size_t) m;
    }
}

int main()
{
    const int fs = 48000;
    const int note = 81;                       // A5; 8' → f0 = 880 Hz... x2 foot? 8' = unison
    const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);   // 880 Hz

    // ---- 1. aliasing per wave and quality tier (filter active) -------------
    printf ("aliasing (non-harmonic energy, dB — lower is better)\n");
    printf ("  %-18s %8s %8s %8s\n", "scenario", "LOW", "HQ", "XHQ");
    const char* wnames[4] = { "saw", "pulse", "tri", "sine" };
    for (int wave = 0; wave < 4; ++wave)
    {
        double a[3];
        for (int q = 0; q < 3; ++q)
        {
            cw::Engine e;
            labPatch (e, wave, 0, 0.72f, 0.55f);   // growl, resonating: tanh active
            e.setGlobal (cw::gHq, (float) q);
            e.prepare (fs, 512);
            e.noteOn (note);
            std::vector<float> L, R;
            render (e, L, R, fs, 3.0);
            a[q] = aliasDb (spectrum (L.data() + L.size() - kFft), f0, fs);
        }
        printf ("  %-18s %8.1f %8.1f %8.1f\n", wnames[wave], a[0], a[1], a[2]);
        if (wave == 3)
        {
            check (a[1] <= a[0] + 0.5, "sine+filter: HQ no worse than LOW");
            check (a[2] <= a[1] + 0.5, "sine+filter: XHQ no worse than HQ");
            check (a[2] < -34.0,       "sine+filter: XHQ aliasing < -34 dB");
        }
    }

    // ---- 2. clean-path THD (character stages measured, no surprises) -------
    printf ("clean-path THD (sine, filter open, res 0)\n");
    for (int temper : { 0, 2 })
    {
        cw::Engine e;
        labPatch (e, 3, temper, 1.0f, 0.0f);
        e.setGlobal (cw::gHq, 2);
        e.prepare (fs, 512);
        e.noteOn (note);
        std::vector<float> L, R;
        render (e, L, R, fs, 3.0);
        const double thd = thdDb (spectrum (L.data() + L.size() - kFft), f0, fs);
        printf ("  temper %-12s %8.1f dB\n", temper == 0 ? "growl" : "ladder", thd);
        check (thd < (temper == 0 ? -40.0 : -22.0),
               temper == 0 ? "growl clean path THD < -40 dB"
                           : "ladder colour bounded (< -22 dB)");
    }

    // ---- 3. clicks on hard parameter jumps ---------------------------------
    {
        cw::Engine e;
        labPatch (e, 3, 0, 0.6f, 0.1f);
        e.prepare (fs, 512);
        e.noteOn (note);
        std::vector<float> L, R;
        render (e, L, R, fs, 2.0);
        auto maxDelta = [&] (size_t from, size_t to)
        {
            double m = 0;
            for (size_t i = std::max (from, (size_t) 1); i < to && i < L.size(); ++i)
                m = std::max (m, (double) std::fabs (L[i] - L[i - 1]));
            return m;
        };
        const double base = maxDelta (L.size() - fs, L.size());

        struct Jump { const char* name; std::function<void()> fire; };
        const Jump jumps[] =
        {
            { "cutoff 0.6->1.0", [&] { e.setVoice (0, cw::vfCut, 1.0f); } },
            { "footage 8'->32'", [&] { e.setVoice (0, cw::vfFoot, 0.0f); } },
            { "mute on",         [&] { e.setVoice (0, cw::vfMute, 1.0f); } },
            { "mute off",        [&] { e.setVoice (0, cw::vfMute, 0.0f); } },
            { "note jump",       [&] { e.noteOff (note); e.noteOn (note + 7); } },
        };
        for (const auto& j : jumps)
        {
            const size_t mark = L.size();
            j.fire();
            render (e, L, R, fs, 0.5);
            const double d = maxDelta (mark, mark + (size_t) fs / 4);
            char buf[96];
            snprintf (buf, sizeof buf, "no click: %s (delta %.3f vs base %.3f)", j.name, d, base);
            check (d < std::max (0.05, base * 3.5), buf);
        }
    }

    // ---- 4. idle noise floor ----------------------------------------------
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gDrone, 0);
        std::vector<float> L, R;
        render (e, L, R, fs, 2.0);
        double peak = 0;
        for (size_t i = (size_t) fs; i < L.size(); ++i)
            peak = std::max (peak, (double) std::fabs (L[i]));
        char buf[80];
        snprintf (buf, sizeof buf, "idle output is digital silence (peak %.2e)", peak);
        check (peak < 1.0e-5, buf);
    }

    // ---- 5. DC per seed ----------------------------------------------------
    for (uint32_t seed : { 11u, 12u, 13u, 14u, 15u })
    {
        cw::Engine e;
        cw::Patch p;
        const char* cat = cw::generatePatch (seed, p);
        e.applyPatch (p);
        e.prepare (fs, 512);
        std::vector<float> L, R;
        render (e, L, R, fs, 5.0);
        double sum = 0;
        for (size_t i = (size_t) fs; i < L.size(); ++i) sum += L[i];
        const double dc = sum / (double) (L.size() - fs);
        char buf[80];
        snprintf (buf, sizeof buf, "seed %u (%s) DC %.4f", seed, cat, dc);
        check (std::fabs (dc) < 0.01, buf);
    }

    // ---- 6. stress: everything cranked at XHQ ------------------------------
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gHq, 2);
        e.setGlobal (cw::gBusSat, 1);
        e.setGlobal (cw::gDriveAmt, 1);
        e.setGlobal (cw::gMaster, 1);
        e.setGlobal (cw::gSpringMix, 1);
        e.setGlobal (cw::gSpringDwell, 1);
        e.setGlobal (cw::gTapeMix, 1);
        e.setGlobal (cw::gTapeFdbk, 1);
        e.setGlobal (cw::gBbdDepth, 1);
        for (int v = 0; v < cw::kVoices; ++v)
        {
            e.setVoice (v, cw::vfLevel, 1);
            e.setVoice (v, cw::vfRes, 1);
        }
        std::vector<float> L, R;
        render (e, L, R, fs, 6.0);
        double peak = 0;
        bool nan = false;
        for (float x : L) { if (! std::isfinite (x)) nan = true; peak = std::max (peak, (double) std::fabs (x)); }
        char buf[80];
        snprintf (buf, sizeof buf, "full-blast XHQ bounded (peak %.3f)", peak);
        check (! nan && peak <= 1.0001, buf);
    }

    // ---- 7. tier render speed ---------------------------------------------
    printf ("render speed (x realtime, default drone, 16 voices)\n");
    for (int q = 0; q < 3; ++q)
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gHq, (float) q);
        std::vector<float> L, R;
        render (e, L, R, fs, 1.0);        // warm up
        const auto t0 = std::chrono::steady_clock::now();
        render (e, L, R, fs, 6.0);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        printf ("  %-6s %6.1fx realtime\n", q == 0 ? "LOW" : q == 1 ? "HQ" : "XHQ", 6.0 / secs);
        if (q == 2) check (6.0 / secs > 1.5, "XHQ still comfortably faster than realtime");
    }

    // ---- 8. determinism per tier ------------------------------------------
    for (int q = 0; q < 3; ++q)
    {
        auto one = [&] (std::vector<float>& L, std::vector<float>& R)
        {
            cw::Engine e;
            cw::Patch p;
            cw::generatePatch (42, p);
            e.applyPatch (p);
            e.setUnitSeed (777);
            e.setGlobal (cw::gHq, (float) q);
            e.prepare (fs, 512);
            render (e, L, R, fs, 2.0);
        };
        std::vector<float> L1, R1, L2, R2;
        one (L1, R1); one (L2, R2);
        char buf[64];
        snprintf (buf, sizeof buf, "tier %d bit-identical renders", q);
        check (std::memcmp (L1.data(), L2.data(), L1.size() * 4) == 0, buf);
    }

    printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES",
            failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
