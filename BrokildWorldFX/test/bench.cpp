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
    CHECK (wm.detuneCents == 0 && wm.tremolo == 1 && wm.filterMul == 1, "world-mod bus not neutral");
}

// ---------------------------------------------------------------------------
int main()
{
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
    testReverbLive();
    testFuzz();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    if (failures == 0) std::printf ("ALL CLEAR\n");
    return failures == 0 ? 0 : 1;
}
