// Headless validation of the Clone Wars engine core.
//
//   g++ -O2 -std=c++17 -I../Source/Core render_test.cpp ../Source/Core/cw_core.cpp -o render_test
//   ./render_test [outdir]
//
// Renders several scenarios to WAV, asserts basic sanity (no NaN, no
// silence, no hard clipping, bounded DC), prints stats. Exit 0 = pass.

#include "cw_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static bool writeWav (const std::string& path, const std::vector<float>& L,
                      const std::vector<float>& R, int fs)
{
    FILE* f = fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint32_t n = (uint32_t) L.size();
    const uint32_t dataBytes = n * 2 * 2;
    auto u32 = [f] (uint32_t v) { fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { fwrite (&v, 2, 1, f); };
    fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); fwrite ("WAVE", 1, 4, f);
    fwrite ("fmt ", 1, 4, f); u32 (16); u16 (1); u16 (2);
    u32 ((uint32_t) fs); u32 ((uint32_t) fs * 4); u16 (4); u16 (16);
    fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        auto conv = [] (float x)
        {
            x = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
            return (int16_t) (x * 32767.0f);
        };
        int16_t s[2] = { conv (L[i]), conv (R[i]) };
        fwrite (s, 2, 2, f);
    }
    fclose (f);
    return true;
}

struct Stats { float peak = 0, rms = 0, dc = 0; bool nan = false; };

static Stats analyze (const std::vector<float>& L, const std::vector<float>& R,
                      size_t skip)
{
    Stats st;
    double sum2 = 0, sum = 0;
    size_t cnt = 0;
    for (size_t i = skip; i < L.size(); ++i)
        for (float x : { L[i], R[i] })
        {
            if (! std::isfinite (x)) st.nan = true;
            const float a = std::fabs (x);
            if (a > st.peak) st.peak = a;
            sum2 += (double) x * x;
            sum  += x;
            ++cnt;
        }
    st.rms = (float) std::sqrt (sum2 / (double) cnt);
    st.dc  = (float) (sum / (double) cnt);
    return st;
}

static int failures = 0;
static void check (bool ok, const char* what)
{
    printf ("  %-46s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}

static void render (cw::Engine& e, std::vector<float>& L, std::vector<float>& R,
                    int fs, double seconds)
{
    const size_t n = (size_t) (seconds * fs);
    const size_t start = L.size();
    L.resize (start + n); R.resize (start + n);
    size_t done = 0;
    while (done < n)
    {
        const int m = (int) std::min<size_t> (512, n - done);
        e.process (L.data() + start + done, R.data() + start + done, m);
        done += (size_t) m;
    }
}

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? std::string (argv[1]) + "/" : "";
    const int fs = 48000;

    // ---- scenario 1: power-on default drone -------------------------------
    {
        cw::Engine e;
        e.prepare (fs, 512);
        std::vector<float> L, R;
        render (e, L, R, fs, 10.0);
        writeWav (out + "cw-default-drone.wav", L, R, fs);
        const auto st = analyze (L, R, (size_t) fs / 2);
        printf ("default drone: peak %.3f rms %.4f dc %.5f\n", st.peak, st.rms, st.dc);
        check (! st.nan, "no NaN/inf");
        check (st.rms > 0.01f, "not silent");
        check (st.peak <= 1.01f, "no hard clipping");
        check (std::fabs (st.dc) < 0.02f, "no DC offset");
    }

    // ---- scenario 2: all five seed categories -----------------------------
    for (uint32_t seed : { 5u, 6u, 7u, 8u, 9u })
    {
        cw::Engine e;
        cw::Patch p;
        const char* cat = cw::generatePatch (seed, p);
        e.applyPatch (p);
        e.prepare (fs, 512);
        std::vector<float> L, R;
        render (e, L, R, fs, 8.0);
        char name[128];
        snprintf (name, sizeof name, "%scw-seed-%03u-%s.wav", out.c_str(), seed, cat);
        writeWav (name, L, R, fs);
        const auto st = analyze (L, R, (size_t) fs);
        printf ("seed %03u (%s): peak %.3f rms %.4f\n", seed, cat, st.peak, st.rms);
        check (! st.nan, "no NaN/inf");
        check (st.rms > 0.005f, "not silent");
        check (st.peak <= 1.01f, "no hard clipping");
    }

    // ---- scenario 3: MIDI chord, latch, then WAR sweep --------------------
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gDrone, 0);          // silent until played
        std::vector<float> L, R;
        render (e, L, R, fs, 0.5);
        const auto silent = analyze (L, R, 0);
        check (silent.rms < 0.001f, "drone off + no notes = silence");

        e.noteOn (36); e.noteOn (43); e.noteOn (48);   // C2 G2 C3
        render (e, L, R, fs, 3.0);
        e.noteOff (36); e.noteOff (43); e.noteOff (48); // latched: keeps sounding
        e.setGlobal (cw::gWarSlew, 0.15f);
        e.setGlobal (cw::gWar, 1.0f);                   // march to army B
        render (e, L, R, fs, 5.0);
        writeWav (out + "cw-chord-war.wav", L, R, fs);
        const auto st = analyze (L, R, (size_t) fs);
        printf ("chord+war: peak %.3f rms %.4f\n", st.peak, st.rms);
        check (! st.nan, "no NaN/inf");
        check (st.rms > 0.01f, "latched chord sustains");
        check (st.peak <= 1.01f, "no hard clipping");
    }

    // ---- scenario 4: every temper at high resonance stays stable ----------
    for (int temper = 0; temper < 3; ++temper)
    {
        cw::Engine e;
        e.prepare (fs, 512);
        e.setGlobal (cw::gTemperA, (float) temper);
        e.setGlobal (cw::gTemperB, (float) temper);
        for (int v = 0; v < cw::kVoices; ++v)
        {
            e.setVoice (v, cw::vfRes, 0.95f);
            e.setVoice (v, cw::vfCut, 0.75f);
        }
        std::vector<float> L, R;
        render (e, L, R, fs, 4.0);
        const auto st = analyze (L, R, (size_t) fs / 2);
        printf ("temper %d res=0.95: peak %.3f rms %.4f\n", temper, st.peak, st.rms);
        check (! st.nan, "no NaN/inf at high resonance");
        check (st.peak <= 1.01f, "stable at high resonance");
    }

    // ---- scenario 5: determinism — same seed twice is bit-identical -------
    {
        auto renderSeed = [fs] (std::vector<float>& L, std::vector<float>& R)
        {
            cw::Engine e;
            cw::Patch p;
            cw::generatePatch (42, p);
            e.applyPatch (p);
            e.setUnitSeed (777);
            e.prepare (fs, 512);
            render (e, L, R, fs, 2.0);
        };
        std::vector<float> L1, R1, L2, R2;
        renderSeed (L1, R1);
        renderSeed (L2, R2);
        check (std::memcmp (L1.data(), L2.data(), L1.size() * 4) == 0,
               "same seed → bit-identical render");
    }

    printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES",
            failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
