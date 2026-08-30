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

    // characters are found by ID, never by index — the registry grew once
    // already (phase B slotted four ahead of tape/insect) and will again
    int charByName (const char* id)
    {
        for (int c = 0; c < numCharacters(); ++c)
            if (std::string (characterDescriptor (c).id) == id) return c;
        return -1;
    }

    // let private reverbs build their IRs (rebuild waits for settled knobs)
    void serviceRack (Rack& r, int times = 5)
    {
        for (int i = 0; i < times; ++i) r.service();
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

    /*  6) PAGE B AND THE LAST STEP. Peter's model: BARS still says how long
        a PAGE is stretched over, and the second page is where the extra
        length comes from. So a pattern that does not use page B must be
        exactly what it was before page B existed. */
    {
        //  the stored string stays SIXTEEN characters while page B is empty
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tD, true);
        r.setExtra (tD, "1000300010002060");
        CHECK (r.getExtra (tD) == "1000300010002060",
               "page A alone did not stay 16 chars (got '%s')", r.getExtra (tD).c_str());
        CHECK (r.getExtra (tD).size() == 16, "16-step pattern grew to %d chars",
               (int) r.getExtra (tD).size());
        //  ...and grows to 32 the moment anything is written there
        r.setExtra (tD, "10003000100020600000000000000700");
        CHECK (r.getExtra (tD).size() == 32, "page B not stored (%d chars)",
               (int) r.getExtra (tD).size());
        CHECK (r.getExtra (tD) == "10003000100020600000000000000700",
               "page B round trip wrong ('%s')", r.getExtra (tD).c_str());
        //  through the blob, too
        Rack b2; b2.prepare (fs, 512); b2.fromJson (r.toJson());
        CHECK (b2.getExtra (tD) == "10003000100020600000000000000700", "page B lost in the blob");
    }

    //  a step on page B actually fires — and the window has to REACH it.
    //  Step 29 of a 1-bar page is 29 x 0.125 s in: render three bars.
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tD, true);
        std::string pgm (32, '0');
        pgm[29] = '7';                                  // GATE, unmistakable
        r.setExtra (tD, pgm);
        r.setParam (tD, 7, 20.0f);                      // duty 20%
        r.setParam (tD, 8, 32.0f);                      // LAST = 32
        const int N = (int) (fs * 6.0);
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i) { L[(size_t) i] = 0.5f; R[(size_t) i] = 0.5f; }
        renderWithTransport (r, L.data(), R.data(), N);
        const int a0 = (int) (29 * stepSec * fs), a1 = (int) (30 * stepSec * fs);
        double lo = 1.0;
        for (int i = a0; i < a1 && i < N; ++i) lo = std::min (lo, (double) L[(size_t) i]);
        std::printf ("   page B step 29: floor %.3f (gate must close)\n", lo);
        CHECK (lo < 0.05, "a step on page B did not fire (floor %.3f)", lo);
    }

    //  LAST shortens the loop: at LAST 4 the same gate returns every 4 steps
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tD, true);
        r.setExtra (tD, "7000000000000000");
        r.setParam (tD, 7, 50.0f);
        r.setParam (tD, 8, 4.0f);                       // LAST = 4
        const int N = (int) (fs * 6.0);
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i) { L[(size_t) i] = 0.5f; R[(size_t) i] = 0.5f; }
        renderWithTransport (r, L.data(), R.data(), N);
        std::vector<int> downs;
        for (int i = N / 3 + 1; i < N; ++i)
            if (L[(size_t) (i - 1)] > 0.25f && L[(size_t) i] <= 0.25f) downs.push_back (i);
        CHECK (downs.size() >= 4, "LAST 4: gate never fired");
        if (downs.size() >= 4)
        {
            double sum = 0;
            for (size_t k = 1; k < downs.size(); ++k) sum += downs[k] - downs[k - 1];
            const double period = sum / (double) (downs.size() - 1) / fs;
            std::printf ("   LAST 4: loop %.4f s (want %.4f)\n", period, 4 * stepSec);
            CHECK (std::abs (period - 4 * stepSec) < 0.006, "LAST did not shorten the loop (%.4f s)", period);
        }
    }

    /*  7) THE RANDOM BRUSH. Reproducible from the loop cycle and the step,
        so a bounce matches playback; never resolves to NONE, or it would
        read as a dropout rather than as a choice; and it must not sound
        like any single fixed brush. */
    {
        auto renderPgm = [&] (const std::string& pgm, float chaos, std::vector<float>& L)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tD, true);
            r.setExtra (tD, pgm);
            r.setParam (tD, 9, chaos);
            const int N = (int) (fs * 4.0);
            L.assign ((size_t) N, 0.0f);
            std::vector<float> R (N);
            fillTone (L.data(), R.data(), N, fs, 330.0, 0.6f);
            renderWithTransport (r, L.data(), R.data(), N);
        };
        std::vector<float> a, b, dry, fixed;
        renderPgm ("8888888888888888", 0.0f, a);
        renderPgm ("8888888888888888", 0.0f, b);
        CHECK (std::memcmp (a.data(), b.data(), a.size() * 4) == 0,
               "RANDOM brush is not reproducible");
        renderPgm ("0000000000000000", 0.0f, dry);
        double d = 0;
        for (size_t i = 0; i < a.size(); ++i) d = std::max (d, std::abs ((double) a[i] - dry[i]));
        CHECK (d > 0.05, "RANDOM brush inaudible (max diff %.3f) — resolving to NONE?", d);
        renderPgm ("1111111111111111", 0.0f, fixed);
        double f = 0;
        for (size_t i = 0; i < a.size(); ++i) f = std::max (f, std::abs ((double) a[i] - fixed[i]));
        CHECK (f > 0.05, "RANDOM brush is just RETRIG (max diff %.3f)", f);
        const Stats sr = measure (a.data(), a.data(), (int) a.size());
        CHECK (sr.finite && sr.peak < 1.3f, "RANDOM brush unbounded (peak %.3f)", (double) sr.peak);

        /*  8) CHAOS. Exactly inert at 0 — memcmp against a rack that has
            never had the parameter touched, which is the same test the
            empty-rack contract uses. Then audibly different above it, and
            still reproducible. */
        std::vector<float> c0, c0b, c60, c60b;
        renderPgm ("1030007000200060", 0.0f, c0);
        {
            //  a rack whose CHAOS was never written at all
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tD, true);
            r.setExtra (tD, "1030007000200060");
            const int N = (int) (fs * 4.0);
            c0b.assign ((size_t) N, 0.0f);
            std::vector<float> R (N);
            fillTone (c0b.data(), R.data(), N, fs, 330.0, 0.6f);
            renderWithTransport (r, c0b.data(), R.data(), N);
        }
        CHECK (std::memcmp (c0.data(), c0b.data(), c0.size() * 4) == 0,
               "CHAOS 0 is not bit-identical to the module without it");
        renderPgm ("1030007000200060", 60.0f, c60);
        renderPgm ("1030007000200060", 60.0f, c60b);
        CHECK (std::memcmp (c60.data(), c60b.data(), c60.size() * 4) == 0,
               "CHAOS is not reproducible");
        double cd = 0;
        for (size_t i = 0; i < c0.size(); ++i) cd = std::max (cd, std::abs ((double) c0[i] - c60[i]));
        std::printf ("   CHAOS 60 moves the render by %.3f\n", cd);
        CHECK (cd > 0.02, "CHAOS does nothing (max diff %.4f)", cd);
        const Stats sc = measure (c60.data(), c60.data(), (int) c60.size());
        CHECK (sc.finite && sc.peak < 1.3f, "CHAOS unbounded (peak %.3f)", (double) sc.peak);

        //  the second half of CHAOS: an all-OFF pattern fires anyway
        std::vector<float> off0, offC;
        renderPgm ("0000000000000000", 0.0f, off0);
        renderPgm ("0000000000000000", 90.0f, offC);
        double od = 0;
        for (size_t i = 0; i < off0.size(); ++i) od = std::max (od, std::abs ((double) off0[i] - offC[i]));
        std::printf ("   CHAOS 90 on an empty pattern: %.3f of uninvited glitch\n", od);
        CHECK (od > 0.05, "CHAOS never fires an uninvited step (%.4f)", od);
    }
}

// ---------------------------------------------------------------------------
// SPECTRA: the characters live on the bus. Neutral when unarmed (the
// additive contract extended to modulation), audibly present when armed,
// scaled by presence, deterministic, and stored in the blob.
// ---------------------------------------------------------------------------
// MACROS: five host parameters, the only automatable part of the rack.
// Neutral at zero, additive, and invisible until something is assigned.
static void testMacros()
{
    std::printf ("-- MACROS: a mapping, owned destinations, blob-compatible\n");
    const double fs = 48000;
    const int N = 24000;
    const int tTube = typeByName ("saturation");
    const int tEcho = typeByName ("delay");

    auto render = [&] (Rack& r, std::vector<float>& L)
    {
        L.assign ((size_t) N, 0.0f);
        std::vector<float> R (N);
        fillTone (L.data(), R.data(), N, fs, 220.0, 0.5f);
        renderRack (r, L.data(), R.data(), N);
    };

    //  1) a fresh rack writes NO "m" key — a blob saved today is the blob
    //     that was saved before macros existed
    {
        Rack r;
        r.prepare (fs, 512);
        const std::string blob = r.toJson();
        CHECK (blob.find ("\"m\":") == std::string::npos,
               "a fresh rack emits an \"m\" key — every old blob just changed");
        //  ...and macro 5 is nonetheless holding the dry/wet
        CHECK (r.macroIsDefault(), "a fresh rack is not in the default wiring");
        const std::string mj = r.macroAssignJson();
        CHECK (mj.find ("mix") != std::string::npos,
               "macro 5 is not assigned to the dry/wet (%s)", mj.c_str());
    }

    //  2) every macro at 0 is EXACTLY the rack without macros
    {
        Rack a, b;
        a.prepare (fs, 512); b.prepare (fs, 512);
        for (Rack* r : { &a, &b })
        {
            r->setEnabled (tTube, true);
            r->setParam (tTube, 0, 14.0f);
            r->setEnabled (tEcho, true);
        }
        for (int m = 0; m < kMacros; ++m) b.setMacro (m, 0.0f);   // explicitly at rest
        std::vector<float> la, lb;
        render (a, la); render (b, lb);
        CHECK (std::memcmp (la.data(), lb.data(), la.size() * 4) == 0,
               "macros at 0 are not bit-identical to a rack that never touched them");
    }

    //  3) macro 5, the one that ships assigned, pulls the rack toward dry
    {
        Rack dry, wet, pulled;
        for (Rack* r : { &dry, &wet, &pulled }) r->prepare (fs, 512);
        for (Rack* r : { &wet, &pulled })
        {
            r->setEnabled (tTube, true);
            r->setParam (tTube, 0, 20.0f);          // hard drive, unmistakable
        }
        pulled.setMacro (kMacros - 1, 1.0f);        // full up = effects out
        std::vector<float> ld, lw, lp;
        render (dry, ld); render (wet, lw); render (pulled, lp);
        double dWet = 0, dPulled = 0;
        const size_t settled = ld.size() / 2;      // past the macro ramp
        for (size_t i = settled; i < ld.size(); ++i)
        {
            dWet    = std::max (dWet,    std::abs ((double) lw[i] - ld[i]));
            dPulled = std::max (dPulled, std::abs ((double) lp[i] - ld[i]));
        }
        std::printf ("   macro 5 up: distance from dry %.4f (wet is %.4f)\n", dPulled, dWet);
        CHECK (dWet > 0.02, "the probe's own effect is inaudible (%.4f) — bad test", dWet);
        CHECK (dPulled < dWet * 0.05, "macro 5 did not pull the rack dry (%.4f vs %.4f)",
               dPulled, dWet);
    }

    /*  4) a macro on a module parameter moves it, both polarities, and
        clamps. Every measurement gets a FRESH rack: rendering the same one
        twice leaves its delay line primed, and it would then be compared
        against a reference whose line is empty. */
    {
        auto echoRack = [&] (float mixPct, float depth, float macro, std::vector<float>& out)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tEcho, true);
            r.setParam (tEcho, 0, mixPct);          // ECHO mix is parameter 0
            if (std::fabs (depth) > 0.0f)
            {
                r.setMacroAssign (0, "delay.mix", depth);
                r.setMacro (0, macro);
            }
            render (r, out);
        };
        auto settledDiff = [] (const std::vector<float>& a, const std::vector<float>& b)
        {
            double d = 0;
            for (size_t i = a.size() / 2; i < a.size(); ++i)
                d = std::max (d, std::abs ((double) a[i] - b[i]));
            return d;
        };

        {   Rack r; r.prepare (fs, 512); r.setMacroAssign (0, "delay.mix", 1.0f);
            CHECK (! r.macroIsDefault(), "editing an assignment did not leave the default"); }

        /*  A MAPPING: the macro owns the destination and sweeps it end to
            end. Which end it starts from is the sign of the depth. Note
            what is NOT asserted here any more — that assigning changes
            nothing. Under a mapping it does, immediately, and that is the
            behaviour being asked for. */
        std::vector<float> lowEnd, atZero, up, topOut, down, botOut;
        echoRack (50.0f,  1.0f, 0.0f, atZero);      // owned, macro at rest
        echoRack (0.0f,   0.0f, 0.0f, lowEnd);      // the parameter at its bottom
        const double dZero = settledDiff (atZero, lowEnd);
        std::printf ("   +100%% at macro 0 vs the bottom: %.2e\n", dZero);
        CHECK (dZero < 2e-3, "a +100%% macro at rest is not at the bottom (%.2e)", dZero);

        echoRack (50.0f,  1.0f, 1.0f, up);          // ...and at the top when full
        echoRack (100.0f, 0.0f, 0.0f, topOut);
        const double dTop = settledDiff (up, topOut);
        std::printf ("   +100%% at macro 1 vs the top: %.2e\n", dTop);
        CHECK (dTop < 2e-3, "macro at 1.0 x +100%% did not reach the top (%.2e)", dTop);

        //  a NEGATIVE depth runs the other way: top at rest, bottom at full
        echoRack (50.0f, -1.0f, 0.0f, down);
        echoRack (100.0f, 0.0f, 0.0f, botOut);      // ...which is the TOP
        const double dNegRest = settledDiff (down, botOut);
        CHECK (dNegRest < 2e-3, "a -100%% macro at rest is not at the top (%.2e)", dNegRest);

        std::vector<float> negFull, lowAgain;
        echoRack (50.0f, -1.0f, 1.0f, negFull);
        echoRack (0.0f,   0.0f, 0.0f, lowAgain);
        const double dNegFull = settledDiff (negFull, lowAgain);
        CHECK (dNegFull < 2e-3, "a -100%% macro at full is not at the bottom (%.2e)", dNegFull);
    }

    //  5) presence is assignable, and an assignment to a module that is OFF
    //     is inert
    {
        Rack r, ref;
        r.prepare (fs, 512); ref.prepare (fs, 512);
        r.setEnabled (tTube, true); ref.setEnabled (tTube, true);
        r.setParam (tTube, 0, 18.0f); ref.setParam (tTube, 0, 18.0f);
        r.setMacroAssign (0, "saturation.pr", -1.0f);
        r.setMacro (0, 1.0f);                       // presence pulled to 0
        std::vector<float> lp, lr;
        render (r, lp); render (ref, lr);
        double d = 0;
        for (size_t i = 0; i < lp.size(); ++i) d = std::max (d, std::abs ((double) lp[i] - lr[i]));
        CHECK (d > 0.01, "a macro on PRESENCE did nothing (%.4f)", d);

        Rack off, offRef;
        off.prepare (fs, 512); offRef.prepare (fs, 512);
        off.setEnabled (tEcho, true); offRef.setEnabled (tEcho, true);
        off.setMacroAssign (1, "reverb.mix", 1.0f); // SPACE is not enabled
        off.setMacro (1, 1.0f);
        std::vector<float> lo, lor;
        render (off, lo); render (offRef, lor);
        CHECK (std::memcmp (lo.data(), lor.data(), lo.size() * 4) == 0,
               "an assignment to a disabled module was not inert");
    }

    //  5b) a destination belongs to exactly one macro
    {
        Rack r;
        r.prepare (fs, 512);
        r.setMacroAssign (0, "delay.mix", 1.0f);
        r.setMacroAssign (2, "delay.mix", -0.5f);   // takes it off macro 1
        const std::string j = r.macroAssignJson();
        //  it must appear exactly once across all five
        int count = 0;
        for (size_t i = j.find ("delay.mix"); i != std::string::npos; i = j.find ("delay.mix", i + 1)) ++count;
        CHECK (count == 1, "delay.mix is owned by %d macros, want 1", count);
    }

    //  6) assignments round-trip through the blob, and one this build cannot
    //     resolve is kept verbatim — the future-blob rule
    {
        Rack a;
        a.prepare (fs, 512);
        a.setMacroAssign (0, "delay.time", 0.5f);
        a.setMacroAssign (0, "reverb.mix", -0.25f);
        a.setMacroAssign (3, "mix", 1.0f);
        const std::string blob = a.toJson();
        CHECK (blob.find ("\"m\":") != std::string::npos, "assignments missing from the blob");

        Rack b;
        b.prepare (fs, 512);
        b.fromJson (blob);
        CHECK (b.macroAssignJson() == a.macroAssignJson(),
               "assignments lost in the round trip\n     a: %s\n     b: %s",
               a.macroAssignJson().c_str(), b.macroAssignJson().c_str());

        //  a destination from a BWFX that does not exist yet
        Rack c;
        c.prepare (fs, 512);
        c.setMacroAssign (2, "vocoder.formant", 0.75f);
        const std::string cj = c.toJson();
        Rack d;
        d.prepare (fs, 512);
        d.fromJson (cj);
        CHECK (d.macroAssignJson().find ("vocoder.formant") != std::string::npos,
               "an unresolvable destination was dropped instead of kept");
        //  and it must be silent, not crash or wander
        d.setMacro (2, 1.0f);
        d.setEnabled (tEcho, true);
        std::vector<float> l; render (d, l);
        const Stats st = measure (l.data(), l.data(), (int) l.size());
        CHECK (st.finite, "an unresolvable destination made the rack non-finite");
    }

    //  7) a morph leaves the wiring alone: assignments are structure
    {
        Rack a;
        a.prepare (fs, 512);
        a.setMacroAssign (0, "delay.time", 0.5f);
        const std::string before = a.macroAssignJson();
        Rack x, y;
        x.prepare (fs, 512); y.prepare (fs, 512);
        x.setEnabled (tTube, true);
        y.setEnabled (tEcho, true);
        a.applyMorph (x.toJson(), y.toJson(), 0.5f);
        CHECK (a.macroAssignJson() == before, "a morph moved the macro wiring");
    }
}

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

    const int cTape = charByName ("tape"), cInsect = charByName ("insect");
    CHECK (cTape >= 0 && cInsect >= 0, "tape/insect missing from the registry");

    // tape armed: sag and dulling appear, detune wobbles; deterministic
    {
        Rack r1, r2;
        r1.prepare (fs, 512); r2.prepare (fs, 512);
        r1.setCharArmed (cTape, true); r2.setCharArmed (cTape, true);
        renderRack (r1, L.data(), R.data(), N);
        renderRack (r2, L.data(), R.data(), N);
        const WorldMod a = r1.worldMod(), b = r2.worldMod();
        std::printf ("   tape bus: det %.3f c, sag %.3f st, filterMul %.3f\n",
                     (double) a.detuneCents, (double) a.pitchSag, (double) a.filterMul);
        CHECK (a.pitchSag == 0.0f, "tape bends the note at its defaults (%.3f)",
               (double) a.pitchSag);
        CHECK (a.filterMul < 0.95f, "tape character inert");
        CHECK (a.detuneCents == b.detuneCents && a.pitchSag == b.pitchSag,
               "character tick nondeterministic");
    }

    // presence scales the contribution to nothing
    {
        Rack r;
        r.prepare (fs, 512);
        r.setCharArmed (cTape, true);
        r.setCharPresence (cTape, 0.0f);
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
        r.setCharArmed (cTape, true);
        r.setCharArmed (cInsect, true);
        renderRack (r, L.data(), R.data(), N);
        const WorldMod w = r.worldMod();
        std::printf ("   tape+insect: tremDepth %.3f, tremRate %.2f Hz, pan %.3f\n",
                     (double) w.tremDepth, (double) w.tremRate, (double) w.panSpread);
        CHECK (w.tremDepth > 0.1f && w.tremRate > 2.0f, "insect tremolo missing");
        CHECK (w.panSpread > 0.2f, "insect pan spread missing");
        //  sag is opt-in now, so ask for it before checking it combines
        r.setCharParam (cTape, 1, 40.0f);
        renderRack (r, L.data(), R.data(), N);
        CHECK (r.worldMod().pitchSag > 0.25f, "tape sag lost in combination");
    }

    // the blob carries the spectra; unknown character ignored
    {
        Rack a;
        a.prepare (fs, 512);
        a.setCharArmed (cInsect, true);
        a.setCharParam (cInsect, 0, 80.0f);
        a.setCharPresence (cInsect, 0.6f);
        Rack b;
        b.prepare (fs, 512);
        b.fromJson (a.toJson());
        CHECK (b.getCharArmed (cInsect) && ! b.getCharArmed (cTape), "spectra enables lost in round trip");
        CHECK (std::abs (b.getCharParam (cInsect, 0) - 80.0f) < 1e-4f, "spectra param lost");
        CHECK (std::abs (b.getCharPresence (cInsect) - 0.6f) < 1e-4f, "spectra presence lost");
        Rack c;
        c.prepare (fs, 512);
        c.fromJson ("{\"spectra\":{\"ghost\":{\"on\":1},\"tape\":{\"on\":1}}}");
        CHECK (c.getCharArmed (cTape), "known character lost beside unknown one");
    }
}

// ---------------------------------------------------------------------------
// Phase B: the four PS2 characters — dark drone / pink / black / glass.
// Dark drone is a pure modulator; pink, black and glass own private audio
// DSP that must be additive (unarmed = bit-identical, presence 0 =
// bit-transparent), silence-preserving, bounded and deterministic.
static void testSpectraPhaseB()
{
    std::printf ("-- SPECTRA phase B: dark drone / pink / black / glass\n");
    const double fs = 48000;
    const int N = 96000;
    std::vector<float> L (N), R (N), refL (N), refR (N);

    const int cDark = charByName ("darkdrone"), cPink = charByName ("pink");
    const int cBlack = charByName ("black"),    cGlass = charByName ("glass");
    CHECK (cDark >= 0 && cPink >= 0 && cBlack >= 0 && cGlass >= 0,
           "phase B characters missing from the registry");

    // dark drone: cluster + sag land on the bus; the drift WANDERS the filter
    {
        Rack r;
        r.prepare (fs, 512);
        r.setCharArmed (cDark, true);
        float mulMin = 10.0f, mulMax = 0.0f, detMin = 1e9f, detMax = -1e9f;
        for (int done = 0; done < N * 5; done += 4800)
        {
            renderRack (r, L.data(), R.data(), 4800);
            const WorldMod w = r.worldMod();
            mulMin = std::min (mulMin, w.filterMul); mulMax = std::max (mulMax, w.filterMul);
            detMin = std::min (detMin, w.detuneCents); detMax = std::max (detMax, w.detuneCents);
        }
        const WorldMod w = r.worldMod();
        std::printf ("   dark drone: sag %.3f, det %.1f..%.1f c, filterMul %.3f..%.3f\n",
                     (double) w.pitchSag, (double) detMin, (double) detMax,
                     (double) mulMin, (double) mulMax);
        CHECK (w.pitchSag == 0.0f, "dark drone bends the note at its defaults (%.3f)",
               (double) w.pitchSag);
        //  CLUSTER is an ensemble width the hosts fan across voices, so it is
        //  allowed to be non-zero — but it must be STEADY. Drift wandering it
        //  was the tuning wandering, which is the bug.
        CHECK (std::abs (detMin - 24.0f) < 1e-3f && std::abs (detMax - 24.0f) < 1e-3f,
               "dark drone detune is not a steady cluster width (%.2f..%.2f)",
               (double) detMin, (double) detMax);
        CHECK (mulMax > mulMin + 0.005f && mulMin > 0.5f && mulMax < 2.0f,
               "dark drone drift does not wander the filter");
    }

    /*  BUGLIST 16, the rule itself: an effect may colour, widen, tremble and
        filter, but it may not put the instrument out of tune. At its own
        defaults NO character writes pitchSag, and the one pitch modulator we
        keep — tape WOW — is zero-mean, so the note always comes back. */
    {
        std::printf ("   -- no character bends the note at its defaults\n");
        for (int c = 0; c < numCharacters(); ++c)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setCharArmed (c, true);
            renderRack (r, L.data(), R.data(), N);
            const WorldMod w = r.worldMod();
            CHECK (w.pitchSag == 0.0f,
                   "%s bends the note at its defaults (sag %.3f)",
                   characterDescriptor (c).name, (double) w.pitchSag);
        }

        //  tape WOW: a full wow cycle is 2 s at 0.5 Hz — its mean must be ~0
        Rack r;
        r.prepare (fs, 512);
        const int tapeIdx = charByName ("tape");
        r.setCharArmed (tapeIdx, true);
        double sum = 0; int n = 0; float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < 40; ++i)                    // 40 x 0.1 s = 4 s
        {
            renderRack (r, L.data(), R.data(), (int) (fs * 0.1));
            const float d = r.worldMod().detuneCents;
            sum += d; ++n; lo = std::min (lo, d); hi = std::max (hi, d);
        }
        const double mean = sum / n;
        std::printf ("   tape wow over 4 s: %.2f .. %.2f c, mean %.3f c\n",
                     (double) lo, (double) hi, mean);
        CHECK (hi - lo > 1.0f, "tape wow is not modulating at all");
        CHECK (std::abs (mean) < 1.0, "tape wow is not zero-mean (%.2f c)", mean);
    }

    // pink: the three LFOs breathe width, tuning and filter at x1 / x0.618 / x0.29
    {
        Rack r;
        r.prepare (fs, 512);
        serviceRack (r);
        r.setCharArmed (cPink, true);
        float panMin = 1e9f, panMax = -1e9f, detMin = 1e9f, detMax = -1e9f, mulMin = 10, mulMax = 0;
        // 26 s: the slowest of the three LFOs (bloom, rate x0.29 = 0.041 Hz)
        // needs a full cycle in view or its lower half never gets sampled
        for (int done = 0; done < N * 13; done += 2400)
        {
            std::memset (L.data(), 0, sizeof (float) * 2400);
            std::memset (R.data(), 0, sizeof (float) * 2400);
            renderRack (r, L.data(), R.data(), 2400);
            const WorldMod w = r.worldMod();
            panMin = std::min (panMin, w.panSpread); panMax = std::max (panMax, w.panSpread);
            detMin = std::min (detMin, w.detuneCents); detMax = std::max (detMax, w.detuneCents);
            mulMin = std::min (mulMin, w.filterMul); mulMax = std::max (mulMax, w.filterMul);
        }
        std::printf ("   pink bus: pan %.2f..%.2f, det %.1f..%.1f c, mul %.2f..%.2f\n",
                     (double) panMin, (double) panMax, (double) detMin, (double) detMax,
                     (double) mulMin, (double) mulMax);
        CHECK (panMin < 0.08f && panMax > 0.25f, "pink swirl does not breathe the width");
        CHECK (detMin < -3.0f && detMax > 3.0f, "pink smear detune missing");
        CHECK (mulMin < 0.90f && mulMax > 1.10f, "pink bloom filter swing missing");
    }

    // glass: the barely-there breath on the tuning (±2.2 c * shine at 0.05 Hz)
    {
        Rack r;
        r.prepare (fs, 512);
        serviceRack (r);
        r.setCharArmed (cGlass, true);
        float detMin = 1e9f, detMax = -1e9f;
        for (int done = 0; done < N * 6; done += 4800)   // 12 s of a 20 s period
        {
            std::memset (L.data(), 0, sizeof (float) * 4800);
            std::memset (R.data(), 0, sizeof (float) * 4800);
            renderRack (r, L.data(), R.data(), 4800);
            const WorldMod w = r.worldMod();
            detMin = std::min (detMin, w.detuneCents); detMax = std::max (detMax, w.detuneCents);
        }
        std::printf ("   glass breath: det %.2f..%.2f c\n", (double) detMin, (double) detMax);
        CHECK (detMax - detMin > 1.2f && detMax <= 2.3f && detMin >= -2.3f,
               "glass breath out of character");
    }

    // the audio characters: each must be additive, present, silent-safe,
    // deterministic — and presence 0 must be bit-transparent
    const int audioChars[] = { cPink, cBlack, cGlass };
    const char* audioNames[] = { "pink", "black", "glass" };
    for (int k = 0; k < 3; ++k)
    {
        const int c = audioChars[k];

        // armed at presence 1: the output must actually change
        fillTone (L.data(), R.data(), N, fs, 220.0, 0.4f);
        refL = L; refR = R;
        Rack r;
        r.prepare (fs, 512);
        serviceRack (r);
        r.setCharArmed (c, true);
        renderRack (r, L.data(), R.data(), N);
        const Stats s = measure (L.data(), R.data(), N);
        double diff = 0;
        for (int i = 0; i < N; ++i) diff += std::abs ((double) L[(size_t) i] - refL[(size_t) i]);
        diff /= N;
        std::printf ("   %s audio: peak %.3f, mean|delta| %.4f\n", audioNames[k], (double) s.peak, diff);
        CHECK (s.finite && s.peak < 1.3f, "%s audio unbounded", audioNames[k]);
        CHECK (diff > 1e-3, "%s audio character changes nothing", audioNames[k]);

        // deterministic: a second identical rack renders the same bytes
        {
            std::vector<float> L2 (refL), R2 (refR);
            Rack r2;
            r2.prepare (fs, 512);
            serviceRack (r2);
            r2.setCharArmed (c, true);
            renderRack (r2, L2.data(), R2.data(), N);
            CHECK (std::memcmp (L.data(), L2.data(), sizeof (float) * N) == 0,
                   "%s audio nondeterministic", audioNames[k]);
        }

        // silence in -> silence out
        {
            std::vector<float> zL (N, 0.0f), zR (N, 0.0f);
            Rack rz;
            rz.prepare (fs, 512);
            serviceRack (rz);
            rz.setCharArmed (c, true);
            renderRack (rz, zL.data(), zR.data(), N);
            const Stats sz = measure (zL.data(), zR.data(), N);
            CHECK (sz.peak < 1e-5f, "%s emits sound from silence (peak %g)", audioNames[k], (double) sz.peak);
        }

        // presence 0: bit-transparent even while armed
        {
            std::vector<float> pL (refL), pR (refR);
            Rack rp;
            rp.prepare (fs, 512);
            serviceRack (rp);
            rp.setCharArmed (c, true);
            rp.setCharPresence (c, 0.0f);
            renderRack (rp, pL.data(), pR.data(), N);
            CHECK (std::memcmp (pL.data(), refL.data(), sizeof (float) * N) == 0,
                   "%s at presence 0 is not bit-transparent", audioNames[k]);
        }

        // disarm mid-note: no click beyond the settled chain's own step
        {
            std::vector<float> dL (refL), dR (refR);
            Rack rd;
            rd.prepare (fs, 512);
            serviceRack (rd);
            rd.setCharArmed (c, true);
            renderRack (rd, dL.data(), dR.data(), N / 2);
            rd.setCharArmed (c, false);
            renderRack (rd, dL.data() + N / 2, dR.data() + N / 2, N / 2);
            float maxStep = 0;
            for (int i = N / 2 - 4800; i < N / 2 + 4800; ++i)
                maxStep = std::max (maxStep, std::abs (dL[(size_t) i] - dL[(size_t) i - 1]));
            CHECK (maxStep < 0.30f, "%s disarm clicks (step %.3f)", audioNames[k], (double) maxStep);
        }
    }

    // black must publish NOTHING on the bus (pure FX character)
    {
        Rack r;
        r.prepare (fs, 512);
        serviceRack (r);
        r.setCharArmed (cBlack, true);
        fillTone (L.data(), R.data(), 4800, fs, 220.0, 0.4f);
        renderRack (r, L.data(), R.data(), 4800);
        const WorldMod w = r.worldMod();
        CHECK (w.detuneCents == 0 && w.panSpread == 0 && w.tremDepth == 0
            && w.pitchSag == 0 && w.filterMul == 1, "black leaks onto the bus");
    }
}

// ---------------------------------------------------------------------------
// TUBE must hold its level across the whole DRIVE range at more than one
// input level — the Mars Wars lesson, relearned live: the equal-power blend
// summed correlated dry+wet to +3 dB at mid drive (Peter heard it).
static void testTubeDriveNeutral()
{
    std::printf ("-- TUBE gain neutrality across DRIVE\n");
    const double fs = 48000;
    const int N = 120000;                  // 2.5 s: measure the SETTLED level
    const int tTube = typeByName ("saturation");
    std::vector<float> L (N), R (N);
    for (int lvl = 0; lvl < 2; ++lvl)
    {
        const float amp = lvl == 0 ? 0.25f : 0.6f;
        double refRms = 0;
        double worst = 0;
        for (int d = 0; d <= 24; d += 4)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tTube, true);
            r.setParam (tTube, 0, (float) d);
            fillTone (L.data(), R.data(), N, fs, 220.0, amp);
            renderRack (r, L.data(), R.data(), N);
            double acc = 0;
            for (int i = 72000; i < N; ++i) acc += (double) L[(size_t) i] * L[(size_t) i];
            const double rmsV = std::sqrt (acc / (N - 72000));
            if (d == 0) refRms = rmsV;
            const double dB = 20.0 * std::log10 (rmsV / std::max (1e-12, refRms));
            worst = std::max (worst, std::abs (dB));
        }
        std::printf ("   amp %.2f: worst deviation %.2f dB\n", (double) amp, worst);
        CHECK (worst < 1.5, "TUBE drive not level-neutral at amp %.2f (%.2f dB)", (double) amp, worst);
    }
}

// ---------------------------------------------------------------------------
// The built-in presets: every one must load through fromJson, render bounded,
// deterministic and silence-preserving, and never change a rack it isn't
// applied to (they are data, not code).
static void testPresets()
{
    std::printf ("-- built-in presets\n");
    const double fs = 48000;
    const int N = 48000;
    std::vector<float> L (N), R (N);

    CHECK (numPresets() >= 8, "preset library too small (%d)", numPresets());
    const std::string js = presetsJson();
    CHECK (js.size() > 100 && js[0] == '[', "presetsJson malformed");

    for (int i = 0; i < numPresets(); ++i)
    {
        Rack r;
        r.prepare (fs, 512);
        r.fromJson (presetBlob (i));
        serviceRack (r);
        r.setTransport (120.0, 0.0, true);       // pattern presets need a clock

        fillTone (L.data(), R.data(), N, fs, 220.0, 0.4f);
        renderRack (r, L.data(), R.data(), N);
        const Stats s = measure (L.data(), R.data(), N);
        CHECK (s.finite && s.peak < 1.3f, "preset '%s' unbounded (peak %g)",
               presetName (i), (double) s.peak);

        std::vector<float> zL (N, 0.0f), zR (N, 0.0f);
        Rack rz;
        rz.prepare (fs, 512);
        rz.fromJson (presetBlob (i));
        serviceRack (rz);
        rz.setTransport (120.0, 0.0, true);
        renderRack (rz, zL.data(), zR.data(), N);
        const Stats sz = measure (zL.data(), zR.data(), N);
        CHECK (sz.peak < 1e-5f, "preset '%s' emits sound from silence (peak %g)",
               presetName (i), (double) sz.peak);
    }

    // presets must actually DO something: IRON WORKS arms black + enables
    // strip (the live 260826.6 check caught params arriving without enables —
    // never let a preset test pass on bounded+silence alone)
    {
        Rack r;
        r.prepare (fs, 512);
        int idx = -1;
        for (int i = 0; i < numPresets(); ++i)
            if (std::string (presetName (i)) == "IRON WORKS") idx = i;
        CHECK (idx >= 0, "IRON WORKS missing");
        r.fromJson (presetBlob (idx));
        CHECK (r.getCharArmed (charByName ("black")), "IRON WORKS does not arm black");
        CHECK (r.getEnabled (typeByName ("strip")), "IRON WORKS does not enable strip");
        CHECK (std::abs (r.getCharParam (charByName ("black"), 0) - 70.0f) < 1e-4f,
               "IRON WORKS grind wrong");
    }

    // the pattern preset carries its KIERANATOR grid through the blob
    {
        Rack r;
        r.prepare (fs, 512);
        int idx = -1;
        for (int i = 0; i < numPresets(); ++i)
            if (std::string (presetName (i)) == "BROKEN TRANSMISSION") idx = i;
        CHECK (idx >= 0, "BROKEN TRANSMISSION missing");
        r.fromJson (presetBlob (idx));
        CHECK (r.getExtra (typeByName ("kieranator")) == "1000300010002060",
               "preset pattern lost");
    }
    std::printf ("   %d presets, all bounded + silence-safe\n", numPresets());
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
    // `bwfxtest --presets` prints presetsJson() for DEFAULT_PRESETS, same rule.
    if (argc > 1 && std::string (argv[1]) == "--presets")
    {
        std::printf ("%s\n", presetsJson().c_str());
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
    testMacros();
    testSpectra();
    testSpectraPhaseB();
    testTubeDriveNeutral();
    testPresets();
    testPresenceAndMorph();
    testReverbLive();
    testFuzz();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) std::printf ("ALL CLEAR\n");
    return failures == 0 ? 0 : 1;
}
