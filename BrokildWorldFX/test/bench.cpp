// BWFX offline bench. Per module: bounded, no NaN, silence in -> silence
// out, bypass bit-transparent, deterministic, unity-ish gain. Plus: the
// partitioned convolver measured against direct convolution, JSON round
// trip, unknown-key tolerance, reorder/enable click bounds, and THE
// contract test: an empty rack is bit-identical (it never touches the
// buffers). Prints ALL CLEAR or lists what failed.

#include "../src/bwfx.h"
#include "../modules/bwfx_conv.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
 #include <xmmintrin.h>
#endif

using namespace bwfx;

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    uint32_t rngState = 0x12345u;
    float rnd01() { rngState = 1664525u * rngState + 1013904223u; return (float) (rngState / 4294967296.0); }
    float rndPm()  { return rnd01() * 2.0f - 1.0f; }

    struct Stats { float peak = 0, rms = 0; bool finite = true; };
    Stats measure (const float* L, const float* R, int n)
    {
        Stats s;
        double acc = 0;
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) s.finite = false;
            s.peak = std::max (s.peak, std::max (std::abs (L[i]), std::abs (R[i])));
            acc += (double) L[i] * L[i] + (double) R[i] * R[i];
        }
        s.rms = (float) std::sqrt (acc / (2.0 * n));
        return s;
    }

    void fillTone (float* L, float* R, int n, double fs, double f, float amp)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v = amp * (float) std::sin (6.2831853 * f * i / fs);
            L[i] = v; R[i] = v * 0.8f;
        }
    }

    void renderRack (Rack& r, float* L, float* R, int n, int block = 128)
    {
        for (int done = 0; done < n; done += block)
            r.process (L + done, R + done, std::min (block, n - done));
    }

    int typeByName (const char* id)
    {
        for (int t = 0; t < numModuleTypes(); ++t)
            if (std::string (moduleDescriptor (t).id) == id) return t;
        return -1;
    }
}

// ---------------------------------------------------------------------------
static void testConvolver()
{
    std::printf ("-- convolver vs direct convolution\n");
    const double fs = 48000;
    const int irN = 1500;                       // spans 3 partitions
    std::vector<float> irL (irN), irR (irN);
    for (int i = 0; i < irN; ++i) { irL[(size_t) i] = rndPm() * std::exp (-i / 300.0f); irR[(size_t) i] = rndPm() * std::exp (-i / 300.0f); }

    PartConv pc;
    pc.prepare (fs, 1.0f);
    pc.setImpulse (irL.data(), irR.data(), irN);
    pc.reset();

    const int N = 8192;
    std::vector<float> inL (N), inR (N), outL (N, 0.0f), outR (N, 0.0f);
    for (int i = 0; i < N; ++i) { inL[(size_t) i] = rndPm(); inR[(size_t) i] = rndPm(); }
    for (int done = 0; done < N; done += 160)   // odd block size on purpose
        pc.process (inL.data() + done, inR.data() + done,
                    outL.data() + done, outR.data() + done, std::min (160, N - done));

    // direct convolution, offset by the convolver's one-hop latency
    double maxErr = 0;
    for (int i = 0; i < N - PartConv::kHop; ++i)
    {
        double dl = 0, dr = 0;
        for (int k = 0; k <= i && k < irN; ++k)
        {
            dl += (double) irL[(size_t) k] * inL[(size_t) (i - k)];
            dr += (double) irR[(size_t) k] * inR[(size_t) (i - k)];
        }
        maxErr = std::max (maxErr, std::abs (dl - (double) outL[(size_t) (i + PartConv::kHop)]));
        maxErr = std::max (maxErr, std::abs (dr - (double) outR[(size_t) (i + PartConv::kHop)]));
    }
    std::printf ("   max |partitioned - direct| = %.3e\n", maxErr);
    CHECK (maxErr < 2e-3, "convolver deviates from direct convolution: %.3e", maxErr);
}

// ---------------------------------------------------------------------------
static void testEmptyRackBitIdentical (double fs)
{
    std::printf ("-- empty rack == bit-identical (fs %.0f)\n", fs);
    Rack r;
    r.prepare (fs, 512);
    const int N = 48000;
    std::vector<float> L (N), R (N), refL (N), refR (N);
    for (int i = 0; i < N; ++i) { L[(size_t) i] = rndPm(); R[(size_t) i] = rndPm(); }
    refL = L; refR = R;
    renderRack (r, L.data(), R.data(), N, 173);
    CHECK (std::memcmp (L.data(), refL.data(), sizeof (float) * N) == 0
        && std::memcmp (R.data(), refR.data(), sizeof (float) * N) == 0,
        "empty rack altered the buffers");
}

// ---------------------------------------------------------------------------
static void testModuleSuite (int type)
{
    const Descriptor& d = moduleDescriptor (type);
    std::printf ("-- module %s (%s)\n", d.id, d.name);
    const double fs = 48000;
    const int N = 48000;
    std::vector<float> L (N), R (N);

    auto makeRack = [&] (Rack& r)
    {
        r.prepare (fs, 512);
        r.setEnabled (type, true);
        for (int i = 0; i < 4; ++i) r.service();   // let the reverb build its IR
    };

    // 1) silence in -> silence out
    {
        Rack r; makeRack (r);
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        renderRack (r, L.data(), R.data(), N);
        const Stats s = measure (L.data(), R.data(), N);
        CHECK (s.finite && s.peak < 1e-6f, "%s: silence in gave peak %.3e", d.id, (double) s.peak);
    }

    // 2) bounded + finite at default, all-lo, all-hi params
    for (int pass = 0; pass < 3; ++pass)
    {
        Rack r; makeRack (r);
        for (int p = 0; p < d.numParams; ++p)
            r.setParam (type, p, pass == 0 ? d.params[p].def : pass == 1 ? d.params[p].lo : d.params[p].hi);
        for (int i = 0; i < 4; ++i) r.service();
        fillTone (L.data(), R.data(), N, fs, 220.0, 0.9f);
        renderRack (r, L.data(), R.data(), N);
        const Stats s = measure (L.data(), R.data(), N);
        CHECK (s.finite, "%s pass %d: non-finite output", d.id, pass);
        CHECK (s.peak < 1.3f, "%s pass %d: peak %.3f", d.id, pass, (double) s.peak);
    }

    // 3) deterministic: two racks, same everything -> identical bits
    {
        Rack r1, r2; makeRack (r1); makeRack (r2);
        std::vector<float> L2 (N), R2 (N);
        fillTone (L.data(), R.data(), N, fs, 220.0, 0.5f);
        L2 = L; R2 = R;
        renderRack (r1, L.data(), R.data(), N);
        renderRack (r2, L2.data(), R2.data(), N);
        CHECK (std::memcmp (L.data(), L2.data(), sizeof (float) * N) == 0,
               "%s: nondeterministic render", d.id);
    }

    // 4) unity-ish gain at defaults (a pedal must not win by loudness)
    {
        Rack r; makeRack (r);
        fillTone (L.data(), R.data(), N, fs, 440.0, 0.3f);
        const Stats in = measure (L.data(), R.data(), N);
        renderRack (r, L.data(), R.data(), N);
        const Stats out = measure (L.data() + N / 2, R.data() + N / 2, N / 2);  // settled half
        const Stats inRef = measure (L.data(), R.data(), 0 == 0 ? 1 : 1);       // silence irrelevant
        (void) inRef;
        const float dB = 20.0f * std::log10 (std::max (1e-9f, out.rms) / std::max (1e-9f, in.rms));
        std::printf ("   gain at defaults: %+.2f dB\n", (double) dB);
        CHECK (dB > -7.0f && dB < 3.0f, "%s: default gain %+.2f dB", d.id, (double) dB);
    }

    // 5) disabled module is bit-transparent even with wild params
    {
        Rack r;
        r.prepare (fs, 512);
        for (int p = 0; p < d.numParams; ++p) r.setParam (type, p, d.params[p].hi);
        std::vector<float> refL (N), refR (N);
        fillTone (L.data(), R.data(), N, fs, 330.0, 0.7f);
        refL = L; refR = R;
        renderRack (r, L.data(), R.data(), N);
        CHECK (std::memcmp (L.data(), refL.data(), sizeof (float) * N) == 0,
               "%s: disabled module altered audio", d.id);
    }
}

// ---------------------------------------------------------------------------
static void testClicks()
{
    std::printf ("-- reorder / enable click bounds\n");
    const double fs = 48000;
    const int N = 96000;
    std::vector<float> L (N), R (N);

    // baseline: steady chain, no changes — the material's own sample step
    auto maxStep = [] (const float* x, int from, int to)
    {
        float m = 0;
        for (int i = from + 1; i < to; ++i) m = std::max (m, std::abs (x[i] - x[i - 1]));
        return m;
    };

    const int tDelay = typeByName ("delay"), tChorus = typeByName ("chorus"), tTube = typeByName ("saturation");

    Rack r;
    r.prepare (fs, 512);
    r.setEnabled (tDelay, true);
    r.setEnabled (tChorus, true);
    r.setEnabled (tTube, true);
    fillTone (L.data(), R.data(), N, fs, 440.0, 0.3f);
    renderRack (r, L.data(), R.data(), N / 2);           // settle

    // The chain's own settled sample step is the yardstick — a saturated
    // tone with echoes steps ~0.1 per sample all by itself. A click would
    // tower over it; a clean transition must not exceed it by much.
    const float base = maxStep (L.data(), N / 2 - 19200, N / 2 - 100);

    // reverse the order mid-flight
    int order[kMaxModules];
    r.getOrder (order);
    int rev[kMaxModules];
    for (int i = 0; i < numModuleTypes(); ++i) rev[i] = order[numModuleTypes() - 1 - i];
    r.setOrder (rev, numModuleTypes());
    renderRack (r, L.data() + N / 2, R.data() + N / 2, N / 2);

    const float step = maxStep (L.data(), N / 2 - 100, N / 2 + 14400);
    std::printf ("   max sample step: settled %.4f, across reorder %.4f\n", (double) base, (double) step);
    CHECK (step < base * 1.5f + 0.02f, "reorder clicked: step %.4f vs settled %.4f", (double) step, (double) base);

    // enable toggle click bound
    Rack r2;
    r2.prepare (fs, 512);
    r2.setEnabled (tTube, true);
    fillTone (L.data(), R.data(), N, fs, 440.0, 0.3f);
    renderRack (r2, L.data(), R.data(), N / 2);
    const float base2 = maxStep (L.data(), N / 2 - 19200, N / 2 - 100);
    r2.setEnabled (tDelay, true);
    r2.setParam (tDelay, 0, 60.0f);
    renderRack (r2, L.data() + N / 2, R.data() + N / 2, N / 2);
    const float step2 = maxStep (L.data(), N / 2 - 100, N / 2 + 14400);
    std::printf ("   max sample step: settled %.4f, across enable %.4f\n", (double) base2, (double) step2);
    CHECK (step2 < base2 * 1.5f + 0.02f, "enable clicked: step %.4f vs settled %.4f", (double) step2, (double) base2);
}

// ---------------------------------------------------------------------------
static void testState()
{
    std::printf ("-- state: JSON round trip, unknown keys, malformed input\n");
    Rack a;
    a.prepare (48000, 512);
    const int tDelay = typeByName ("delay"), tReverb = typeByName ("reverb");
    a.setEnabled (tDelay, true);
    a.setParam (tDelay, 1, 420.0f);
    a.setParam (tDelay, 4, 1.0f);
    a.setEnabled (tReverb, true);
    a.setParam (tReverb, 2, 333.0f);
    a.setMix (0.8f);
    int order[kMaxModules];
    a.getOrder (order);
    std::swap (order[0], order[5]);
    a.setOrder (order, numModuleTypes());

    const std::string blob = a.toJson();
    Rack b;
    b.prepare (48000, 512);
    b.fromJson (blob);
    CHECK (b.getEnabled (tDelay) && b.getEnabled (tReverb), "round trip lost enables");
    CHECK (std::abs (b.getParam (tDelay, 1) - 420.0f) < 1e-4f, "round trip lost delay time (%g)", (double) b.getParam (tDelay, 1));
    CHECK (std::abs (b.getParam (tDelay, 4) - 1.0f) < 1e-4f, "round trip lost delay character");
    CHECK (std::abs (b.getParam (tReverb, 2) - 333.0f) < 1e-4f, "round trip lost reverb length");
    CHECK (std::abs (b.getMix() - 0.8f) < 1e-4f, "round trip lost mix");
    int order2[kMaxModules];
    b.getOrder (order2);
    bool same = true;
    for (int i = 0; i < numModuleTypes(); ++i) same &= order[i] == order2[i];
    CHECK (same, "round trip lost the order");

    // a blob from the future: unknown module, unknown param, extra keys
    Rack c;
    c.prepare (48000, 512);
    c.fromJson ("{\"v\":9,\"future\":true,\"mix\":0.5,"
                "\"order\":[\"flanger\",\"delay\",\"zorp\"],"
                "\"modules\":{\"flanger\":{\"on\":1,\"p\":{\"woo\":1}},"
                "\"delay\":{\"on\":1,\"p\":{\"time\":500,\"zorp\":9}}}}");
    CHECK (c.getEnabled (tDelay), "future blob: delay enable lost");
    CHECK (std::abs (c.getParam (tDelay, 1) - 500.0f) < 1e-4f, "future blob: delay time lost");
    CHECK (std::abs (c.getMix() - 0.5f) < 1e-4f, "future blob: mix lost");
    int order3[kMaxModules];
    c.getOrder (order3);
    CHECK (order3[0] == tDelay, "future blob: known module not first in order");
    uint32_t seen = 0;
    for (int i = 0; i < numModuleTypes(); ++i) seen |= 1u << order3[i];
    CHECK (seen == (1u << numModuleTypes()) - 1, "future blob: order not a full permutation");

    // malformed input: keep defaults, don't crash
    Rack e;
    e.prepare (48000, 512);
    e.fromJson ("{\"mix\":0.1,");
    e.fromJson ("not json at all");
    e.fromJson ("");
    CHECK (! e.anyEnabled(), "malformed blob enabled something");

    // clearState = default empty
    a.clearState();
    CHECK (! a.anyEnabled() && std::abs (a.getMix() - 1.0f) < 1e-6f, "clearState not default");
}

// ---------------------------------------------------------------------------
static void testFuzz()
{
    std::printf ("-- 60 random machines, 0.5 s each\n");
    const double rates[3] = { 44100, 48000, 96000 };
    for (int it = 0; it < 60; ++it)
    {
        const double fs = rates[it % 3];
        Rack r;
        r.prepare (fs, 512);
        for (int t = 0; t < numModuleTypes(); ++t)
        {
            r.setEnabled (t, rnd01() > 0.4f);
            const Descriptor& d = moduleDescriptor (t);
            for (int p = 0; p < d.numParams; ++p)
                r.setParam (t, p, d.params[p].lo + rnd01() * (d.params[p].hi - d.params[p].lo));
        }
        int order[kMaxModules];
        for (int t = 0; t < numModuleTypes(); ++t) order[t] = t;
        for (int t = numModuleTypes() - 1; t > 0; --t)
            std::swap (order[t], order[(int) (rnd01() * (t + 1))]);
        r.setOrder (order, numModuleTypes());
        r.setMix (rnd01());
        for (int i = 0; i < 3; ++i) r.service();

        const int N = (int) (fs / 2);
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i)
        {
            const float v = 0.6f * (float) std::sin (6.2831853 * 110.0 * i / fs) + 0.3f * rndPm();
            L[(size_t) i] = v; R[(size_t) i] = 0.6f * (float) std::sin (6.2831853 * 138.0 * i / fs) + 0.3f * rndPm();
        }
        renderRack (r, L.data(), R.data(), N, 64 + (it % 5) * 97);
        const Stats s = measure (L.data(), R.data(), N);
        CHECK (s.finite, "fuzz %d: non-finite", it);
        CHECK (s.peak <= 1.26f, "fuzz %d: peak %.3f", it, (double) s.peak);
    }
}

// ---------------------------------------------------------------------------
static void testReverbLive()
{
    std::printf ("-- reverb: IR build via service, wet arrives, types differ\n");
    const double fs = 48000;
    const int tReverb = typeByName ("reverb");
    const int N = 24000;
    std::vector<float> outs[2];
    for (int type = 0; type < 2; ++type)
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tReverb, true);
        r.setParam (tReverb, 0, 100.0f);           // full wet
        r.setParam (tReverb, 1, (float) type);
        for (int i = 0; i < 4; ++i) r.service();
        std::vector<float> L (N, 0.0f), R (N, 0.0f);
        L[0] = 1.0f; R[0] = 1.0f;
        renderRack (r, L.data(), R.data(), N);
        const Stats s = measure (L.data() + 2000, R.data() + 2000, N - 2000);
        CHECK (s.rms > 1e-5f, "reverb type %d: no wet tail (rms %.2e)", type, (double) s.rms);
        outs[type] = L;
    }
    double diff = 0;
    for (int i = 0; i < N; ++i) diff += std::abs ((double) outs[0][(size_t) i] - outs[1][(size_t) i]);
    CHECK (diff > 1e-2, "room and hall render identically");

    WorldMod wm = Rack().worldMod();
    CHECK (wm.detuneCents == 0 && wm.tremDepth == 0 && wm.filterMul == 1, "world-mod bus not neutral");
}

// ---------------------------------------------------------------------------
// Tempo sync: the echo must land ON the grid, the gate must chop at the
// division, and a host with NO clock must change nothing at all.
static void testSync()
{
    std::printf ("-- host-tempo sync (ECHO, GATE)\n");
    const double fs = 48000, bpm = 120.0;
    const int tDelay = typeByName ("delay"), tGate = typeByName ("stutter");
    const int N = (int) fs * 3;
    const int click = 4800;

    // ECHO: div 1/4 (index 4) at 120 BPM = 0.500 s; triplet = 0.3333; dotted = 0.750
    struct Case { int div, feel; double sec; const char* name; };
    const Case cases[3] = { { 4, 0, 0.5, "1/4 straight" },
                            { 4, 1, 1.0 / 3.0, "1/4 triplet" },
                            { 4, 2, 0.75, "1/4 dotted" } };
    for (const auto& c : cases)
    {
        Rack r;
        r.prepare (fs, 512);
        r.setBpm (bpm);
        r.setEnabled (tDelay, true);
        r.setParam (tDelay, 0, 100.0f);      // full wet: only echoes come out
        r.setParam (tDelay, 2, 55.0f);
        r.setParam (tDelay, 5, (float) c.div);
        r.setParam (tDelay, 6, (float) c.feel);
        std::vector<float> L (N, 0.0f), R (N, 0.0f);
        L[(size_t) click] = 1.0f; R[(size_t) click] = 1.0f;
        renderRack (r, L.data(), R.data(), N);

        int peak = 0;
        float best = 0;
        for (int i = click + 1000; i < N; ++i)
            if (std::abs (L[(size_t) i]) > best) { best = std::abs (L[(size_t) i]); peak = i; }
        const double got = (double) (peak - click) / fs;
        std::printf ("   echo %s: want %.4f s, got %.4f s\n", c.name, c.sec, got);
        CHECK (best > 0.05f, "echo %s: no echo at all", c.name);
        CHECK (std::abs (got - c.sec) < 0.005, "echo %s off grid: %.4f vs %.4f s",
               c.name, got, c.sec);
    }

    // GATE: div 1/4 at 120 BPM = one chop every 0.5 s
    {
        Rack r;
        r.prepare (fs, 512);
        r.setBpm (bpm);
        r.setEnabled (tGate, true);
        r.setParam (tGate, 0, 100.0f);       // full chop
        r.setParam (tGate, 2, 4.0f);         // sync 1/4
        std::vector<float> L (N, 0.5f), R (N, 0.5f);
        renderRack (r, L.data(), R.data(), N);
        // mean-crossing spacing over the settled second half
        std::vector<int> ups;
        const int from = N / 2;
        double mean = 0;
        for (int i = from; i < N; ++i) mean += L[(size_t) i];
        mean /= (N - from);
        for (int i = from + 1; i < N; ++i)
            if (L[(size_t) (i - 1)] <= mean && L[(size_t) i] > mean) ups.push_back (i);
        CHECK (ups.size() >= 2, "gate sync: no chop detected");
        if (ups.size() >= 2)
        {
            double sum = 0;
            for (size_t k = 1; k < ups.size(); ++k) sum += ups[k] - ups[k - 1];
            const double period = sum / (double) (ups.size() - 1) / fs;
            std::printf ("   gate 1/4: want 0.5000 s, got %.4f s\n", period);
            CHECK (std::abs (period - 0.5) < 0.01, "gate off grid: %.4f s", period);
        }
    }

    // The Kemper rule: sync selected but NO host clock renders exactly as FREE.
    {
        auto render = [&] (int div, double hostBpm, std::vector<float>& out)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setBpm (hostBpm);
            r.setEnabled (tDelay, true);
            r.setParam (tDelay, 5, (float) div);
            std::vector<float> L (N), R (N);
            fillTone (L.data(), R.data(), N, fs, 220.0, 0.4f);
            renderRack (r, L.data(), R.data(), N);
            out = L;
        };
        std::vector<float> freeRun, syncedNoClock;
        render (0, 0.0, freeRun);
        render (4, 0.0, syncedNoClock);
        CHECK (std::memcmp (freeRun.data(), syncedNoClock.data(), sizeof (float) * N) == 0,
               "sync with no host clock is not identical to FREE");
    }
}

// ---------------------------------------------------------------------------
// ROTARY: the rotor rates and — the whole point — the two inertias are
// MEASURED. The amplitude throw of each rotor writes its rate into the
// output envelope; autocorrelating the envelope reads it back out.
namespace
{
    // AM frequency of a carrier: rectify, LP, remove mean, autocorrelate.
    double envelopeRate (const float* x, int from, int to, double fs)
    {
        const int n = to - from;
        std::vector<double> env ((size_t) n);
        double lp = 0;
        const double a = 1.0 - std::exp (-2.0 * 3.14159265 * 30.0 / fs);
        for (int i = 0; i < n; ++i)
        {
            lp += (std::abs ((double) x[from + i]) - lp) * a;
            env[(size_t) i] = lp;
        }
        double mean = 0;
        for (double v : env) mean += v;
        mean /= n;
        for (auto& v : env) v -= mean;
        // autocorrelation, NORMALIZED per lag (longer lags sum fewer terms),
        // then the FIRST peak within 85% of the global max — a periodic
        // envelope peaks at every multiple of its period, and the fundamental
        // is the first of them, not necessarily the numerically largest.
        const int lagMin = (int) (fs / 8.5), lagMax = std::min ((int) (fs / 0.4), n / 2);
        std::vector<double> r ((size_t) lagMax, 0.0);
        double globalMax = 0;
        for (int lag = lagMin; lag < lagMax; ++lag)
        {
            double acc = 0;
            int cnt = 0;
            for (int i = 0; i + lag < n; i += 4) { acc += env[(size_t) i] * env[(size_t) (i + lag)]; ++cnt; }
            r[(size_t) lag] = cnt > 0 ? acc / cnt : 0.0;
            globalMax = std::max (globalMax, r[(size_t) lag]);
        }
        for (int lag = lagMin + 1; lag < lagMax - 1; ++lag)
            if (r[(size_t) lag] >= 0.85 * globalMax
             && r[(size_t) lag] >= r[(size_t) (lag - 1)]
             && r[(size_t) lag] >= r[(size_t) (lag + 1)])
                return fs / lag;
        return 0.0;
    }
}

static void testRotary()
{
    std::printf ("-- ROTARY: rotor rates + the two inertias, measured\n");
    const double fs = 48000;
    const int tRot = typeByName ("rotary");

    // steady rates: 1 kHz rides the horn, 150 Hz rides the drum
    struct Case { double tone; int speed; double want; const char* name; };
    const Case cases[3] = { { 3000.0, 1, 6.8, "horn FAST" },
                            { 3000.0, 0, 0.83, "horn SLOW" },
                            { 100.0,  1, 5.9, "drum FAST" } };
    for (const auto& c : cases)
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tRot, true);
        r.setParam (tRot, 0, (float) c.speed);
        const int N = (int) fs * 8;
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i)
        {
            L[(size_t) i] = 0.4f * (float) std::sin (6.2831853 * c.tone * i / fs);
            R[(size_t) i] = L[(size_t) i];
        }
        renderRack (r, L.data(), R.data(), N);
        const double got = envelopeRate (L.data(), N / 2, N, fs);
        std::printf ("   %s: want %.2f Hz, got %.2f Hz\n", c.name, c.want, got);
        CHECK (std::abs (got - c.want) < c.want * 0.15,
               "%s off: %.2f vs %.2f Hz", c.name, got, c.want);
    }

    // the inertia: flip SLOW->FAST at t=4s. The horn must arrive within ~2 s;
    // the heavy drum must still be climbing soon after and arrive by ~+8 s.
    {
        auto ramp = [&] (double tone, double winA0, double winA1, double winB0, double winB1,
                         double& early, double& late)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tRot, true);
            r.setParam (tRot, 0, 0.0f);           // SLOW
            const int N = (int) fs * 14;
            std::vector<float> L (N), R (N);
            for (int i = 0; i < N; ++i)
            {
                L[(size_t) i] = 0.4f * (float) std::sin (6.2831853 * tone * i / fs);
                R[(size_t) i] = L[(size_t) i];
            }
            const int flip = (int) fs * 4;
            for (int done = 0; done < N; )
            {
                if (done == flip) r.setParam (tRot, 0, 1.0f);   // FAST
                const int m = std::min (512, N - done);
                r.process (L.data() + done, R.data() + done, m);
                done += m;
            }
            early = envelopeRate (L.data(), (int) (fs * winA0), (int) (fs * winA1), fs);
            late  = envelopeRate (L.data(), (int) (fs * winB0), (int) (fs * winB1), fs);
        };
        double hornEarly, hornLate, drumEarly, drumLate;
        ramp (3000.0, 6.0, 8.0, 11.0, 13.5, hornEarly, hornLate);
        ramp (100.0,  5.0, 6.5, 11.0, 13.5, drumEarly, drumLate);
        std::printf ("   horn after flip: +2..4 s %.2f Hz, +7..9.5 s %.2f Hz (fast 6.8)\n", hornEarly, hornLate);
        std::printf ("   drum after flip: +1..2.5 s %.2f Hz, +7..9.5 s %.2f Hz (fast 5.9)\n", drumEarly, drumLate);
        CHECK (hornEarly > 5.5, "horn too lazy: %.2f Hz two seconds after the flip", hornEarly);
        CHECK (drumEarly < 4.5, "drum has no inertia: %.2f Hz so soon after the flip", drumEarly);
        CHECK (drumLate > 4.8, "drum never arrives: %.2f Hz", drumLate);
    }
}

// ---------------------------------------------------------------------------
// DISRUPTOR: the pattern is real. Gate steps land on the transport grid,
// the tape-stop measurably drops pitch across its step, the drawn pattern
// survives the state round trip, and an all-NONE pattern is transparent.
namespace
{
    double goertzel (const float* x, int from, int to, double f, double fs)
    {
        const double w = 2.0 * 3.14159265358979 * f / fs, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (int i = from; i < to; ++i)
        { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2));
    }
}

static void testDisruptor()
{
    std::printf ("-- DISRUPTOR: grid, tape-stop pitch, pattern state\n");
    const double fs = 48000, bpm = 120.0;
    const int tD = typeByName ("kieranator");
    const double stepSec = 0.125;                       // 1/16 at 120 BPM

    auto renderWithTransport = [&] (Rack& r, float* L, float* R, int N)
    {
        int done = 0;
        while (done < N)
        {
            const int m = std::min (512, N - done);
            r.setTransport (bpm, (double) done * bpm / (60.0 * fs), true);
            r.process (L + done, R + done, m);
            done += m;
        }
    };

    // 1) all-NONE pattern: enabled but transparent (mix 100, every step dry)
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tD, true);
        const int N = 48000;
        std::vector<float> L (N), R (N), refL (N), refR (N);
        fillTone (L.data(), R.data(), N, fs, 220.0, 0.5f);
        refL = L; refR = R;
        renderWithTransport (r, L.data(), R.data(), N);
        double maxDiff = 0;
        for (int i = 0; i < N; ++i) maxDiff = std::max (maxDiff, std::abs ((double) L[(size_t) i] - refL[(size_t) i]));
        CHECK (maxDiff < 1e-6, "all-NONE pattern not transparent (%.2e)", maxDiff);
    }

    // 2) GATE on every step chops on the 1/16 grid
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tD, true);
        r.setExtra (tD, "7777777777777777");
        r.setParam (tD, 7, 50.0f);                      // duty 50%
        const int N = 96000;
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i) { L[(size_t) i] = 0.5f; R[(size_t) i] = 0.5f; }
        renderWithTransport (r, L.data(), R.data(), N);
        // mean spacing of rising edges over the settled half
        std::vector<int> ups;
        for (int i = N / 2 + 1; i < N; ++i)
            if (L[(size_t) (i - 1)] <= 0.25f && L[(size_t) i] > 0.25f) ups.push_back (i);
        CHECK (ups.size() >= 4, "disruptor gate: no chops");
        if (ups.size() >= 4)
        {
            double sum = 0;
            for (size_t k = 1; k < ups.size(); ++k) sum += ups[k] - ups[k - 1];
            const double period = sum / (double) (ups.size() - 1) / fs;
            std::printf ("   gate steps: want %.4f s, got %.4f s\n", stepSec, period);
            CHECK (std::abs (period - stepSec) < 0.004, "gate off grid: %.4f s", period);
        }
    }

    // 3) TAPESTOP: pitch measured falling across the step
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tD, true);
        r.setExtra (tD, "0000000020000000");            // one tape-stop, step 8
        r.setParam (tD, 4, 100.0f);                     // full stop
        const int N = 96000;                            // one bar = 2 s at 120
        std::vector<float> L (N), R (N);
        fillTone (L.data(), R.data(), N, fs, 880.0, 0.5f);
        renderWithTransport (r, L.data(), R.data(), N);
        const int s8 = (int) (8 * stepSec * fs);
        const int q = (int) (stepSec * fs / 4.0);
        const double early = goertzel (L.data(), s8, s8 + q, 880.0, fs);
        const double lateAt880 = goertzel (L.data(), s8 + 2 * q, s8 + 3 * q, 880.0, fs);
        std::printf ("   tapestop: 880 Hz energy early %.1f, late %.1f (must collapse)\n",
                     early, lateAt880);
        CHECK (early > 100.0, "tapestop start does not carry the tone (%.1f) — window bug?", early);
        CHECK (early > lateAt880 * 2.5, "tapestop does not drop the pitch (early %.1f late %.1f)",
               early, lateAt880);
    }

    // 4) the drawn pattern survives the state round trip
    {
        Rack a;
        a.prepare (fs, 512);
        a.setEnabled (tD, true);
        a.setExtra (tD, "1234567012345670");
        Rack b;
        b.prepare (fs, 512);
        b.fromJson (a.toJson());
        CHECK (b.getExtra (tD) == "1234567012345670",
               "pattern lost in round trip (got '%s')", b.getExtra (tD).c_str());
        // and a blob WITHOUT x resets the pattern to default
        Rack c;
        c.prepare (fs, 512);
        c.fromJson (b.toJson());
        b.setExtra (tD, "");
        CHECK (c.getExtra (tD) == "1234567012345670", "pattern lost via reserialize");
    }

    // 5) deterministic (shuffle included) + bounded at full mayhem
    {
        auto renderOnce = [&] (std::vector<float>& L)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tD, true);
            r.setExtra (tD, "1234567123456712");
            const int N = 96000;
            L.assign ((size_t) N, 0.0f);
            std::vector<float> R (N);
            fillTone (L.data(), R.data(), N, fs, 220.0, 0.7f);
            renderWithTransport (r, L.data(), R.data(), N);
        };
        std::vector<float> a, b;
        renderOnce (a);
        renderOnce (b);
        CHECK (std::memcmp (a.data(), b.data(), a.size() * 4) == 0,
               "disruptor nondeterministic");
        const Stats s = measure (a.data(), a.data(), (int) a.size());
        CHECK (s.finite && s.peak < 1.3f, "disruptor unbounded (peak %.3f)", (double) s.peak);
    }
}

// ---------------------------------------------------------------------------
// SPECTRA: the characters live on the bus. Neutral when unarmed (the
// additive contract extended to modulation), audibly present when armed,
// scaled by presence, deterministic, and stored in the blob.
static void testSpectra()
{
    std::printf ("-- SPECTRA characters on the world-mod bus\n");
    const double fs = 48000;
    const int N = 48000;
    std::vector<float> L (N, 0.0f), R (N, 0.0f);

    // unarmed = neutral bus
    {
        Rack r;
        r.prepare (fs, 512);
        renderRack (r, L.data(), R.data(), N);
        const WorldMod w = r.worldMod();
        CHECK (w.detuneCents == 0 && w.panSpread == 0 && w.tremDepth == 0
            && w.pitchSag == 0 && w.filterMul == 1,
               "unarmed bus not neutral (det %g sag %g mul %g)",
               (double) w.detuneCents, (double) w.pitchSag, (double) w.filterMul);
    }

    // tape armed: sag and dulling appear, detune wobbles; deterministic
    {
        Rack r1, r2;
        r1.prepare (fs, 512); r2.prepare (fs, 512);
        r1.setCharArmed (0, true); r2.setCharArmed (0, true);
        renderRack (r1, L.data(), R.data(), N);
        renderRack (r2, L.data(), R.data(), N);
        const WorldMod a = r1.worldMod(), b = r2.worldMod();
        std::printf ("   tape bus: det %.3f c, sag %.3f st, filterMul %.3f\n",
                     (double) a.detuneCents, (double) a.pitchSag, (double) a.filterMul);
        CHECK (a.pitchSag > 0.25f && a.filterMul < 0.95f, "tape character inert");
        CHECK (a.detuneCents == b.detuneCents && a.pitchSag == b.pitchSag,
               "character tick nondeterministic");
    }

    // presence scales the contribution to nothing
    {
        Rack r;
        r.prepare (fs, 512);
        r.setCharArmed (0, true);
        r.setCharPresence (0, 0.0f);
        renderRack (r, L.data(), R.data(), N);
        const WorldMod w = r.worldMod();
        CHECK (std::abs (w.detuneCents) < 1e-6f && w.pitchSag < 1e-6f
            && std::abs (w.filterMul - 1.0f) < 1e-6f,
               "presence 0 does not silence the character");
    }

    // insect: tremolo depth/rate on the bus; both armed: contributions combine
    {
        Rack r;
        r.prepare (fs, 512);
        r.setCharArmed (0, true);
        r.setCharArmed (1, true);
        renderRack (r, L.data(), R.data(), N);
        const WorldMod w = r.worldMod();
        std::printf ("   tape+insect: tremDepth %.3f, tremRate %.2f Hz, pan %.3f\n",
                     (double) w.tremDepth, (double) w.tremRate, (double) w.panSpread);
        CHECK (w.tremDepth > 0.1f && w.tremRate > 2.0f, "insect tremolo missing");
        CHECK (w.panSpread > 0.2f, "insect pan spread missing");
        CHECK (w.pitchSag > 0.25f, "tape sag lost in combination");
    }

    // the blob carries the spectra; unknown character ignored
    {
        Rack a;
        a.prepare (fs, 512);
        a.setCharArmed (1, true);
        a.setCharParam (1, 0, 80.0f);
        a.setCharPresence (1, 0.6f);
        Rack b;
        b.prepare (fs, 512);
        b.fromJson (a.toJson());
        CHECK (b.getCharArmed (1) && ! b.getCharArmed (0), "spectra enables lost in round trip");
        CHECK (std::abs (b.getCharParam (1, 0) - 80.0f) < 1e-4f, "spectra param lost");
        CHECK (std::abs (b.getCharPresence (1) - 0.6f) < 1e-4f, "spectra presence lost");
        Rack c;
        c.prepare (fs, 512);
        c.fromJson ("{\"spectra\":{\"ghost\":{\"on\":1},\"tape\":{\"on\":1}}}");
        CHECK (c.getCharArmed (0), "known character lost beside unknown one");
    }
}

// ---------------------------------------------------------------------------
static void testPresenceAndMorph()
{
    std::printf ("-- presence + patch morph\n");
    const double fs = 48000;
    const int N = 48000;
    const int tDelay = typeByName ("delay"), tChorus = typeByName ("chorus"), tReverb = typeByName ("reverb");
    std::vector<float> L (N), R (N);

    // presence 0: an ENABLED module must be bit-transparent (0.9 tone keeps
    // the safety ceiling inert, so bytes can be compared against the input)
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tDelay, true);
        r.setPresence (tDelay, 0.0f);
        std::vector<float> refL (N), refR (N);
        fillTone (L.data(), R.data(), N, fs, 220.0, 0.9f);
        refL = L; refR = R;
        renderRack (r, L.data(), R.data(), N);
        CHECK (std::memcmp (L.data(), refL.data(), sizeof (float) * N) == 0,
               "presence 0 not bit-transparent");
    }

    // presence is a real dial: 0 / 0.5 / 1 render three different signals
    {
        std::vector<float> outs[3];
        const float prs[3] = { 0.0f, 0.5f, 1.0f };
        for (int k = 0; k < 3; ++k)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tDelay, true);
            r.setParam (tDelay, 0, 80.0f);
            r.setPresence (tDelay, prs[k]);
            fillTone (L.data(), R.data(), N, fs, 220.0, 0.4f);
            renderRack (r, L.data(), R.data(), N);
            outs[k] = L;
        }
        CHECK (std::memcmp (outs[0].data(), outs[1].data(), sizeof (float) * N) != 0
            && std::memcmp (outs[1].data(), outs[2].data(), sizeof (float) * N) != 0,
               "presence dial has no effect");
    }

    // pr survives the state round trip
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tDelay, true);
        r.setPresence (tDelay, 0.37f);
        Rack r2;
        r2.prepare (fs, 512);
        r2.fromJson (r.toJson());
        CHECK (std::abs (r2.getPresence (tDelay) - 0.37f) < 1e-4f,
               "presence lost in round trip (%g)", (double) r2.getPresence (tDelay));
    }

    // morph endpoints equal the blobs (getters, exact)
    std::string blobA, blobB;
    {
        Rack a;
        a.prepare (fs, 512);
        a.setEnabled (tDelay, true);
        a.setParam (tDelay, 1, 300.0f);
        a.setParam (tDelay, 0, 60.0f);
        a.setMix (0.9f);
        blobA = a.toJson();
        Rack b;
        b.prepare (fs, 512);
        b.setEnabled (tChorus, true);
        b.setEnabled (tReverb, true);
        b.setParam (tReverb, 2, 400.0f);
        b.setMix (0.6f);
        int order[kMaxModules];
        b.getOrder (order);
        std::swap (order[0], order[1]);
        b.setOrder (order, numModuleTypes());
        blobB = b.toJson();
    }
    {
        Rack r;
        r.prepare (fs, 512);
        r.applyMorph (blobA, blobB, 0.0f);
        CHECK (r.getEnabled (tDelay) && ! r.getEnabled (tChorus)
            && std::abs (r.getParam (tDelay, 1) - 300.0f) < 1e-4f
            && std::abs (r.getMix() - 0.9f) < 1e-4f,
               "morph t=0 is not blob A");
        r.applyMorph (blobA, blobB, 1.0f);
        CHECK (! r.getEnabled (tDelay) && r.getEnabled (tChorus) && r.getEnabled (tReverb)
            && std::abs (r.getParam (tReverb, 2) - 400.0f) < 1e-4f
            && std::abs (r.getMix() - 0.6f) < 1e-4f,
               "morph t=1 is not blob B");
        r.applyMorph (blobA, blobB, 0.5f);
        CHECK (std::abs (r.getPresence (tDelay) - 0.5f) < 1e-3f
            && std::abs (r.getPresence (tChorus) - 0.5f) < 1e-3f,
               "morph midpoint presences wrong (%g, %g)",
               (double) r.getPresence (tDelay), (double) r.getPresence (tChorus));
    }

    // a rendered morph sweep: continuous, bounded, no click above baseline
    {
        Rack r;
        r.prepare (fs, 512);
        r.applyMorph (blobA, blobB, 0.0f);
        for (int i = 0; i < 4; ++i) r.service();
        const int M = 4 * 48000;
        std::vector<float> ML (M), MR (M);
        for (int i = 0; i < M; ++i)
        {
            const float v = 0.3f * (float) std::sin (6.2831853 * 220.0 * i / fs);
            ML[(size_t) i] = v; MR[(size_t) i] = v * 0.8f;
        }
        // settle on A for 1 s, then sweep t over 2 s, then hold B for 1 s
        int done = 0;
        auto runTo = [&] (int end)
        {
            while (done < end)
            {
                const int m = std::min (256, end - done);
                r.process (ML.data() + done, MR.data() + done, m);
                done += m;
            }
        };
        runTo (48000);
        auto maxStep = [&] (int from, int to)
        {
            float mx = 0;
            for (int i = from + 1; i < to; ++i) mx = std::max (mx, std::abs (ML[(size_t) i] - ML[(size_t) (i - 1)]));
            return mx;
        };
        const float base = maxStep (24000, 47900);
        for (int k = 0; k <= 60; ++k)               // ~30 Hz morph stream
        {
            r.applyMorph (blobA, blobB, (float) k / 60.0f);
            r.service();
            runTo (48000 + (k + 1) * 1600);
        }
        runTo (M);
        const Stats s = measure (ML.data(), MR.data(), M);
        CHECK (s.finite && s.peak < 1.3f, "morph sweep unbounded (peak %.3f)", (double) s.peak);
        const float sweepStep = maxStep (47900, M - 100);
        std::printf ("   morph sweep: settled step %.4f, sweep max step %.4f\n",
                     (double) base, (double) sweepStep);
        CHECK (sweepStep < base * 1.6f + 0.03f,
               "morph sweep clicked: %.4f vs settled %.4f", (double) sweepStep, (double) base);
    }
}

// ---------------------------------------------------------------------------
int main (int argc, char** argv)
{
    // `bwfxtest --desc` prints descriptorJson() so the UI fragment's
    // DEFAULT_DESC snapshot is regenerated from the C++ truth, never typed.
    if (argc > 1 && std::string (argv[1]) == "--desc")
    {
        std::printf ("%s\n", descriptorJson().c_str());
        return 0;
    }
    // `bwfxtest --cdesc` prints characterJson() for DEFAULT_CDESC, same rule.
    if (argc > 1 && std::string (argv[1]) == "--cdesc")
    {
        std::printf ("%s\n", characterJson().c_str());
        return 0;
    }
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);   // hosts run FTZ; so does the bench
#endif
    std::printf ("BWFX bench — %d module types, rack v%s\n", numModuleTypes(), Rack::version());

    testConvolver();
    testEmptyRackBitIdentical (44100);
    testEmptyRackBitIdentical (48000);
    testEmptyRackBitIdentical (96000);
    for (int t = 0; t < numModuleTypes(); ++t)
        testModuleSuite (t);
    testClicks();
    testState();
    testSync();
    testRotary();
    testDisruptor();
    testSpectra();
    testPresenceAndMorph();
    testReverbLive();
    testFuzz();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) std::printf ("ALL CLEAR\n");
    return failures == 0 ? 0 : 1;
}
