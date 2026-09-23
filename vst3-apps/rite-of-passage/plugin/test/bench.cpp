// RITE OF PASSAGE offline bench. Plain C++17, no JUCE, no audio device.
//
// Written before the plugin and failing before it works, which is the point:
// §15 of RITE-OF-PASSAGE-DESIGN.md is a list of promises, and this file is
// where they are collected. The most important test here is not an effect —
// it is that ARRIVAL lands on the bar to the sample at every buffer size,
// because everything else in the plugin is in service of that instant.

#include "../engine/rop_rack.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
 #include <xmmintrin.h>
#endif

using namespace rop;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
constexpr double kFs = 48000.0;

Rng rng;

// --- signals ---------------------------------------------------------------
void fillNoise (std::vector<float>& l, std::vector<float>& r, int n, float amp = 0.25f)
{
    l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
    //  pink-ish: three one-poles summed, which is close enough for loudness work
    float a = 0, b = 0, c = 0, a2 = 0, b2 = 0, c2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float w1 = rng.bip(), w2 = rng.bip();
        a = 0.99765f * a + w1 * 0.0990460f; b = 0.96300f * b + w1 * 0.2965164f; c = 0.57000f * c + w1 * 1.0526913f;
        a2 = 0.99765f * a2 + w2 * 0.0990460f; b2 = 0.96300f * b2 + w2 * 0.2965164f; c2 = 0.57000f * c2 + w2 * 1.0526913f;
        l[(size_t) i] = amp * (a + b + c + w1 * 0.1848f) * 0.2f;
        r[(size_t) i] = amp * (a2 + b2 + c2 + w2 * 0.1848f) * 0.2f;
    }
}

void fillSine (std::vector<float>& l, std::vector<float>& r, int n, double f, float amp = 0.4f)
{
    l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float v = amp * (float) std::sin (2.0 * M_PI * f * i / kFs);
        l[(size_t) i] = v; r[(size_t) i] = v;
    }
}

// --- measurement -----------------------------------------------------------
struct Stats { float peak = 0; double rms = 0; bool finite = true; };

Stats measure (const std::vector<float>& l, const std::vector<float>& r, int from = 0)
{
    Stats s; double acc = 0; int n = 0;
    for (size_t i = (size_t) from; i < l.size(); ++i, ++n)
    {
        if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) s.finite = false;
        s.peak = std::max (s.peak, std::max (std::abs (l[i]), std::abs (r[i])));
        acc += (double) l[i] * l[i] + (double) r[i] * r[i];
    }
    s.rms = std::sqrt (acc / std::max (1, 2 * n));
    return s;
}

// programme loudness over a whole buffer, BS.1770 — the metric §8 is stated in
double integratedLufs (const std::vector<float>& l, const std::vector<float>& r, int from = 0)
{
    KWeight k; k.prepare (kFs);
    double acc = 0; int n = 0;
    for (size_t i = 0; i < l.size(); ++i)
    {
        float kl, kr; k.process (l[i], r[i], kl, kr);
        if ((int) i < from) continue;
        acc += (double) kl * kl + (double) kr * kr; ++n;
    }
    if (n == 0 || acc <= 1e-20) return -200.0;
    return -0.691 + 10.0 * std::log10 (acc / n);
}

// octave-robust autocorrelation pitch, for §8.2
double measureF0 (const std::vector<float>& x, int from, int len, double lo = 80.0, double hi = 2000.0)
{
    const int minLag = (int) (kFs / hi), maxLag = std::min (len / 2, (int) (kFs / lo));
    if (maxLag <= minLag + 2) return 0.0;
    double e0 = 0;
    for (int i = 0; i < len / 2; ++i) e0 += (double) x[(size_t) (from + i)] * x[(size_t) (from + i)];
    if (e0 < 1e-12) return 0.0;
    std::vector<double> r ((size_t) (maxLag + 1), 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double num = 0, den = 0;
        for (int i = 0; i < len / 2; ++i)
        {
            const double a = x[(size_t) (from + i)], b = x[(size_t) (from + i + lag)];
            num += a * b; den += b * b;
        }
        r[(size_t) lag] = num / std::sqrt (e0 * den + 1e-20);
    }
    double best = 0; for (int lag = minLag; lag <= maxLag; ++lag) best = std::max (best, r[(size_t) lag]);
    if (best <= 0) return 0.0;
    int chosen = 0;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (r[(size_t) lag] >= 0.90 * best && r[(size_t) lag] > r[(size_t) (lag - 1)]
            && r[(size_t) lag] >= r[(size_t) (lag + 1)]) { chosen = lag; break; }
    if (chosen == 0) return 0.0;
    const double ym = r[(size_t) (chosen - 1)], y0 = r[(size_t) chosen], yp = r[(size_t) (chosen + 1)];
    const double den = ym - 2 * y0 + yp;
    const double d = std::abs (den) > 1e-12 ? 0.5 * (ym - yp) / den : 0.0;
    return kFs / (chosen + std::max (-0.5, std::min (0.5, d)));
}

double centsBetween (double a, double b) { return 1200.0 * std::log2 (a / b); }

//  magnitude at one frequency (Goertzel), Hann windowed so a neighbouring
//  partial cannot leak into the bin and be reported as an alias
double toneMag (const std::vector<float>& x, int from, int len, double hz)
{
    const double w = 2.0 * M_PI * hz / kFs;
    const double c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < len; ++i)
    {
        const double win = 0.5 * (1.0 - std::cos (2.0 * M_PI * i / (len - 1)));
        const double s0 = win * x[(size_t) (from + i)] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return 2.0 * std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / len;
}

//  two tones at once: one low enough to be untouched by a tone cue and one
//  high enough to be flattened by it, so LEVEL and COLOUR can be told apart
//  in a single take
void fillTwoTone (std::vector<float>& l, std::vector<float>& r, int n,
                  double f1, double f2, float amp = 0.35f)
{
    l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float v = amp * (float) (std::sin (2.0 * M_PI * f1 * i / kFs)
                                     + std::sin (2.0 * M_PI * f2 * i / kFs)) * 0.5f;
        l[(size_t) i] = v; r[(size_t) i] = v;
    }
}

//  where does b sit behind a? integer samples, by plain cross-correlation
int bestLag (const std::vector<float>& a, const std::vector<float>& b,
             int from, int len, int maxLag)
{
    double best = -1.0e30; int bl = 0;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double acc = 0.0;
        for (int i = 0; i < len; ++i)
            acc += (double) a[(size_t) (from + i)] * b[(size_t) (from + i + lag)];
        if (acc > best) { best = acc; bl = lag; }
    }
    return bl;
}

//  how far apart are two renders of the same input? 0 = identical.
//  (Martian Gain's apart(), for the same reason it exists there.)
double apart (const std::vector<float>& a, const std::vector<float>& b, int from)
{
    double num = 0.0, den = 0.0;
    for (size_t i = (size_t) from; i < a.size(); ++i)
    {
        const double d = (double) a[i] - (double) b[i];
        num += d * d; den += (double) a[i] * a[i];
    }
    return std::sqrt (num / std::max (1.0e-20, den));
}

// --- running the rack ------------------------------------------------------
struct Host
{
    Rack rack;
    double bpm = 128.0, ppq = 0.0;
    long long samplePos = 0;      // hosts count samples and derive ppq; drifting
    bool playing = true;          // accumulators are a test artefact, not a host

    double ppqStart = 0.0;
    void prepare (int block = 256) { rack.prepare (kFs, block); }

    void run (std::vector<float>& l, std::vector<float>& r, int block = 256,
              float posFrom = 0.0f, float posTo = 0.0f)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += block)
        {
            const int m = std::min (block, n - i);
            const float t = posFrom + (posTo - posFrom) * (float) i / (float) std::max (1, n - 1);
            rack.setPosition (t);
            ppq = ppqStart + (double) samplePos * bpm / (60.0 * kFs);
            rack.setTransport (bpm, playing ? ppq : -1.0, playing);
            rack.process (l.data() + i, r.data() + i, m);
            samplePos += m;
        }
    }
};

void setAB (Rack& rk, int slot, int p, float a, float b)
{
    rk.state (slot).A[p] = a;
    rk.state (slot).B[p] = b;
}

void setFlat (Rack& rk, int slot, int type)
{
    const auto& d = effectDescriptor (type);
    for (int p = 0; p < d.numParams; ++p) setAB (rk, slot, p, d.params[p].def, d.params[p].def);
}

} // namespace

// ===========================================================================
int main()
{
   #if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
    _mm_setcsr (_mm_getcsr() | 0x8040);
   #endif
    rng.seed (0xC0FFEEu);

    std::printf ("RITE OF PASSAGE bench — fs %.0f, %d effects, %d slots\n\n",
                 kFs, numEffects(), kSlots);

    const int nLen = (int) (kFs * 4.0);

    // -- 1. THE CONTRACT: nothing assigned, nothing changed -----------------
    {
        Host h; h.prepare();
        std::vector<float> l, r; fillNoise (l, r, nLen);
        auto l0 = l, r0 = r;
        h.rack.arrival.fireImpact = false;
        h.run (l, r, 256, 0.0f, 1.0f);
        float worst = 0;
        for (int i = 0; i < nLen; ++i)
            worst = std::max (worst, std::max (std::abs (l[(size_t) i] - l0[(size_t) i]),
                                               std::abs (r[(size_t) i] - r0[(size_t) i])));
        CHECK (worst == 0.0f, "an empty rack is not bit-transparent (%.3g)", worst);
        std::printf ("  empty rack               bit-identical\n");
    }

    // -- 2. ARRIVAL LANDS ON THE BAR, TO THE SAMPLE -------------------------
    /*  The most important test in this file. Silence in, every slot empty,
        ARRIVAL armed: the only thing that can make a sound is the impact, so
        the first non-zero sample IS the arrival. */
    {
        std::printf ("  arrival, first non-zero sample vs the bar:\n");
        int worstErr = 0;
        for (double bpm : { 120.0, 128.0, 174.0 })
            for (int block : { 64, 256, 1024 })
                for (double startPpq : { 0.37, 1.02, 2.5, 3.99 })
                {
                    Host h; h.prepare (block);
                    h.bpm = bpm; h.ppqStart = startPpq;
                    h.rack.arrival.fireImpact = true;
                    h.rack.arrival.grid = 0;            // the bar
                    h.rack.armArrival();

                    const int n = (int) (kFs * 3.0);
                    std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
                    h.run (l, r, block);

                    int first = -1;
                    for (int i = 0; i < n; ++i)
                        if (l[(size_t) i] != 0.0f || r[(size_t) i] != 0.0f) { first = i; break; }

                    //  computed exactly as the rack computes it, so the test
                    //  measures the plugin and not the two of us rounding
                    //  a half-sample in opposite directions
                    const double beatsAway = std::ceil (startPpq / 4.0) * 4.0 - startPpq;
                    const int expect = (int) std::llround (beatsAway / (bpm / (60.0 * kFs)));
                    const int err = (first < 0) ? 999999 : std::abs (first - expect);
                    worstErr = std::max (worstErr, err);
                    CHECK (err <= 1, "bpm %.0f block %d ppq %.2f: fired at %d, the bar is at %d",
                           bpm, block, startPpq, first, expect);
                }
        std::printf ("    worst error over 36 cases: %d sample%s\n", worstErr, worstErr == 1 ? "" : "s");
    }

    // -- 3. THE SCORE IS OBEYED ---------------------------------------------
    {
        Host h; h.prepare();
        const int climb = effectTypeByName ("climb");
        h.rack.setSlotEffect (0, climb);
        setAB (h.rack, 0, 1, 200.0f, 8000.0f);        // cutoff
        h.rack.state (0).enter = 0.7f;
        h.rack.state (0).exit  = 0.9f;

        float p[kMaxParams] {};
        h.rack.resolve (0, 0.0f, p);  CHECK (p[1] == 200.0f, "before ENTER the slot is not at A (%.1f)", p[1]);
        h.rack.resolve (0, 0.69f, p); CHECK (p[1] == 200.0f, "at ENTER-epsilon the slot has moved (%.1f)", p[1]);
        h.rack.resolve (0, 0.8f, p);
        CHECK (p[1] > 900.0f && p[1] < 1600.0f, "mid-lane log travel is wrong (%.1f, wanted ~1265)", p[1]);
        h.rack.resolve (0, 0.9f, p);  CHECK (std::abs (p[1] - 8000.0f) < 1.0f, "at EXIT the slot is not at B (%.1f)", p[1]);
        h.rack.resolve (0, 1.0f, p);  CHECK (std::abs (p[1] - 8000.0f) < 1.0f, "after EXIT the slot left B (%.1f)", p[1]);

        //  DEPTH stops it short, on purpose
        h.rack.state (0).depth = 0.5f;
        h.rack.resolve (0, 1.0f, p);
        CHECK (p[1] > 1200.0f && p[1] < 1350.0f, "DEPTH 0.5 did not stop half way (%.1f)", p[1]);
        std::printf ("  the score                enter/exit/depth/log travel obeyed\n");
    }

    // -- 4. STEPPED PARAMETERS STEP ON THE GRID -----------------------------
    {
        Host h; h.prepare (64);
        h.bpm = 120.0; h.ppqStart = 0.0;
        const int chop = effectTypeByName ("chop");
        h.rack.setSlotEffect (0, chop);
        setFlat (h.rack, 0, chop);
        setAB (h.rack, 0, 0, 0.0f, 5.0f);          // 1/4 -> 1/32 across the travel
        h.rack.state (0).quantise = 2;             // adopt on the beat
        h.rack.arrival.fireImpact = false;

        //  walk the slider and watch when the held division changes: every
        //  change must land on a beat
        const int block = 64;
        const int n = (int) (kFs * 8.0);
        std::vector<float> l, r; fillNoise (l, r, n);
        float lastDiv = -1.0f;
        int changes = 0, offGrid = 0;
        for (int i = 0; i < n; i += block)
        {
            const int m = std::min (block, n - i);
            const float t = (float) i / (float) n;
            h.rack.setPosition (t);
            h.rack.setTransport (h.bpm, h.ppqStart + (double) h.samplePos * h.bpm / (60.0 * kFs), true);
            const double ppqBefore = h.ppqStart + (double) h.samplePos * h.bpm / (60.0 * kFs);
            h.rack.process (l.data() + i, r.data() + i, m);
            h.samplePos += m;
            const double ppqAfter = h.ppqStart + (double) h.samplePos * h.bpm / (60.0 * kFs);

            float p[kMaxParams] {};
            h.rack.resolve (0, t, p);
            const float want = std::round (p[0]);
            if (want != lastDiv && lastDiv >= 0.0f)
            {
                ++changes;
                //  the adoption happened inside this block; the block must
                //  contain a beat boundary
                const double b0 = std::floor (ppqBefore), b1 = std::floor (ppqAfter);
                if (b0 == b1) ++offGrid;
            }
            lastDiv = want;
        }
        std::printf ("  stepped parameters       %d division changes\n", changes);
        CHECK (changes >= 4, "the division never accelerated (%d changes)", changes);
    }

    // -- 5. LOUDNESS SWEEP: the §8 contract ---------------------------------
    {
        std::printf ("  loudness sweep (BS.1770, a NEUTRAL knob may move +-1.0 dB):\n");
        for (int type = 0; type < numEffects(); ++type)
        {
            const auto& d = effectDescriptor (type);
            if (d.level == Level::Intentional) { std::printf ("    %-8s INTENTIONAL, exempt\n", d.name); continue; }

            for (int p = 0; p < d.numParams; ++p)
            {
                if (d.params[p].levelKnob) continue;

                double lo = 1e9, hi = -1e9;
                for (int s = 0; s <= 4; ++s)
                {
                    const float v = d.params[p].lo + (d.params[p].hi - d.params[p].lo) * (float) s / 4.0f;
                    Host h; h.prepare();
                    h.rack.arrival.fireImpact = false;
                    h.rack.setSlotEffect (0, type);
                    setFlat (h.rack, 0, type);
                    setAB (h.rack, 0, p, v, v);
                    rng.seed (0x1234u);
                    std::vector<float> l, r; fillNoise (l, r, nLen);
                    h.run (l, r, 256, 1.0f, 1.0f);
                    const double L = integratedLufs (l, r, (int) (kFs * 1.0));
                    lo = std::min (lo, L); hi = std::max (hi, L);
                }
                const double spread = hi - lo;
                if (spread > 0.6)
                    std::printf ("    %-8s %-8s %5.2f dB\n", d.name, d.params[p].name, spread);
                CHECK (spread <= 1.0,
                       "%s: sweeping %s moved the loudness %.2f dB — it is declared %s",
                       d.name, d.params[p].name, spread,
                       d.level == Level::Neutral ? "NEUTRAL" : "SPECTRAL");
            }
        }
    }

    // -- 6. PITCH EXACTNESS for every declared non-mover (§8.2) -------------
    {
        std::printf ("  pitch exactness (a non-mover must stay within +-2 cents):\n");
        for (int type = 0; type < numEffects(); ++type)
        {
            const auto& d = effectDescriptor (type);
            if (d.movesPitch || d.generator || d.reordersTime) continue;

            double worst = 0.0;
            for (int p = 0; p < d.numParams; ++p)
                for (int s = 0; s <= 1; ++s)
                {
                    const float v = s ? d.params[p].hi : d.params[p].lo;
                    Host h; h.prepare();
                    h.rack.arrival.fireImpact = false;
                    h.rack.setSlotEffect (0, type);
                    setFlat (h.rack, 0, type);
                    setAB (h.rack, 0, p, v, v);
                    std::vector<float> l, r; fillSine (l, r, nLen, 440.0);
                    h.run (l, r, 256, 1.0f, 1.0f);
                    const auto st = measure (l, r, (int) kFs);
                    if (st.rms < 1e-4) continue;             // the knob silenced it; nothing to measure
                    const double f = measureF0 (l, (int) kFs, 32768);
                    if (f < 100.0) continue;
                    worst = std::max (worst, std::abs (centsBetween (f, 440.0)));
                }
            std::printf ("    %-8s worst %.2f cents\n", d.name, worst);
            CHECK (worst < 2.0, "%s is declared pitch-exact but moved %.2f cents", d.name, worst);
        }
    }

    // -- 7. STUTTER at PITCH 0 loops at exactly rate 1 -----------------------
    {
        Host h; h.prepare();
        h.bpm = 120.0; h.ppqStart = 0.0;
        const int st = effectTypeByName ("stutter");
        h.rack.setSlotEffect (0, st);
        setFlat (h.rack, 0, st);
        setAB (h.rack, 0, 0, 0.0f, 0.0f);      // 1/4
        setAB (h.rack, 0, 1, 100.0f, 100.0f);  // fully wet
        setAB (h.rack, 0, 2, 0.0f, 0.0f);      // no decay
        setAB (h.rack, 0, 3, 0.0f, 0.0f);      // PITCH 0 — the exact path
        setAB (h.rack, 0, 4, 1.0f, 1.0f);      // HOLD
        h.rack.arrival.fireImpact = false;

        std::vector<float> l, r; fillNoise (l, r, nLen);
        h.run (l, r, 256, 1.0f, 1.0f);

        //  one beat at 120 BPM
        const int sliceLen = (int) (60.0 / 120.0 * kFs);
        /*  The loop re-syncs to the beat, so its period can jitter by a
            sample; what is being tested is the READ RATE, which is exactly 1
            only if some alignment matches to the bit. */
        const int from = (int) (kFs * 2.0);
        double best = 1e9;
        for (int off = -2; off <= 2; ++off)
        {
            double worst = 0;
            for (int i = from; i < from + sliceLen - 4; ++i)
                worst = std::max (worst, (double) std::abs (l[(size_t) i]
                                                          - l[(size_t) (i + sliceLen + off)]));
            best = std::min (best, worst);
        }
        std::printf ("  stutter loop at pitch 0  period error %.3g\n", best);
        CHECK (best < 1e-6, "STUTTER at PITCH 0 does not loop at exactly rate 1 (%.3g)", best);
    }

    // -- 8. STEREO IS ENERGY PRESERVING (§6, §8.1) ---------------------------
    {
        std::printf ("  stereo energy (turn / mono gate, must stay within 0.5 dB):\n");
        for (float turn : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
        {
            Host h; h.prepare();
            h.rack.arrival.fireImpact = false;
            h.rack.turn = turn;
            h.rack.bassMonoHz = 20.0f;      // isolate the rotation from §6's bass mono
            rng.seed (0x777u);
            std::vector<float> l, r; fillNoise (l, r, nLen);
            const auto before = measure (l, r);
            h.run (l, r, 256, 0.0f, 0.0f);
            const auto after = measure (l, r, (int) kFs);
            const double d = 20.0 * std::log10 ((after.rms + 1e-12) / (before.rms + 1e-12));
            CHECK (std::abs (d) < 0.5, "TURN %.1f changed the energy %.2f dB", turn, d);
        }
        for (float gate : { 0.5f, 1.0f })
        {
            Host h; h.prepare();
            h.rack.arrival.fireImpact = false;
            h.rack.bassMonoHz = 20.0f;
            h.rack.monoGate = gate;
            h.rack.monoGateSpan = 1.0f;
            rng.seed (0x777u);
            std::vector<float> l, r; fillNoise (l, r, nLen);
            const auto before = measure (l, r);
            h.run (l, r, 256, 1.0f, 1.0f);
            const auto after = measure (l, r, (int) kFs);
            const double d = 20.0 * std::log10 ((after.rms + 1e-12) / (before.rms + 1e-12));
            std::printf ("    mono gate %.1f          %+.2f dB\n", gate, d);
            CHECK (std::abs (d) < 0.5, "the MONO GATE at %.1f changed the energy %.2f dB", gate, d);
        }
    }

    // -- 9. MONO COMPATIBILITY (§6) ------------------------------------------
    {
        Host h; h.prepare();
        h.rack.arrival.fireImpact = false;
        h.rack.spread = 1.0f;
        h.rack.setSlotEffect (0, effectTypeByName ("climb"));
        setFlat (h.rack, 0, effectTypeByName ("climb"));
        setAB (h.rack, 0, 1, 200.0f, 12000.0f);
        setAB (h.rack, 0, 2, 60.0f, 60.0f);
        std::vector<float> l, r; fillNoise (l, r, nLen);
        h.run (l, r, 256, 0.0f, 1.0f);

        double stereo = 0, mono = 0;
        for (int i = (int) kFs; i < nLen; ++i)
        {
            stereo += 0.5 * ((double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i]);
            const double m = 0.5 * ((double) l[(size_t) i] + r[(size_t) i]);
            mono += m * m;
        }
        const double loss = 10.0 * std::log10 ((mono + 1e-20) / (stereo + 1e-20));
        std::printf ("  mono compatibility       %+.2f dB at SPREAD 1\n", loss);
        CHECK (loss > -3.0, "the mono sum loses %.2f dB — it vanishes on a phone", loss);
    }

    // -- 10. NO ZIPPER: CLIMB swept hard ------------------------------------
    {
        Host h; h.prepare (64);
        h.rack.arrival.fireImpact = false;
        const int climb = effectTypeByName ("climb");
        h.rack.setSlotEffect (0, climb);
        setFlat (h.rack, 0, climb);
        setAB (h.rack, 0, 1, 20.0f, 18000.0f);
        setAB (h.rack, 0, 2, 100.0f, 100.0f);      // full resonance
        const int n = (int) (kFs * 0.2);           // 20 Hz -> 18 kHz in 200 ms
        std::vector<float> l, r; fillNoise (l, r, n);
        h.run (l, r, 64, 0.0f, 1.0f);
        const auto st = measure (l, r);
        std::printf ("  climb, 20 Hz->18 kHz/200 ms  peak %.3f\n", st.peak);
        CHECK (st.finite, "a hard CLIMB sweep produced a non-finite sample");
        CHECK (st.peak < 4.0f, "a hard CLIMB sweep reached %.2f", st.peak);
    }

    // -- 11. EVERY EFFECT, BOTH EXTREMES, BOUNDED AND FINITE -----------------
    {
        for (int type = 0; type < numEffects(); ++type)
        {
            const auto& d = effectDescriptor (type);
            for (int p = 0; p < d.numParams; ++p)
            {
                Host h; h.prepare();
                h.rack.setSlotEffect (0, type);
                setFlat (h.rack, 0, type);
                setAB (h.rack, 0, p, d.params[p].lo, d.params[p].hi);
                std::vector<float> l, r; fillNoise (l, r, nLen, 0.7f);
                h.run (l, r, 256, 0.0f, 1.0f);
                const auto st = measure (l, r);
                CHECK (st.finite, "%s sweeping %s produced a non-finite sample", d.name, d.params[p].name);
                CHECK (st.peak <= 1.01f, "%s sweeping %s reached %.3f — the ceiling leaked",
                       d.name, d.params[p].name, st.peak);
            }
        }
        std::printf ("  every effect, both ends  bounded and finite\n");
    }

    // -- 11b. STACK: layers must not compound (§15) --------------------------
    /*  Peter's complaint, stated as a test. One NEUTRAL effect should not
        change the loudness; three of them in series should not change it
        three times as much, and that is the failure everybody has heard —
        a chain that gets louder for every box you switch on. */
    {
        std::printf ("  stacking (a NEUTRAL effect, 1 to 3 slots):\n");
        for (int type = 0; type < numEffects(); ++type)
        {
            const auto& d = effectDescriptor (type);
            if (d.level != Level::Neutral) continue;
            /*  An effect with a level knob is EXCUSED, and TAPE is the reason:
                three delays at 35 % wet are three times the echo, and that is
                what a delay is for, not a fault. What must not compound is an
                effect that claims to be transparent at every setting. */
            bool hasLevelKnob = false;
            for (int p = 0; p < d.numParams; ++p) hasLevelKnob |= d.params[p].levelKnob;
            if (hasLevelKnob) { std::printf ("    %-8s has a level knob, excused\n", d.name); continue; }

            double lo = 1e9, hi = -1e9;
            for (int count = 1; count <= 3; ++count)
            {
                Host h; h.prepare();
                h.rack.arrival.fireImpact = false;
                for (int i = 0; i < count; ++i) { h.rack.setSlotEffect (i, type); setFlat (h.rack, i, type); }
                rng.seed (0x33445566u);
                std::vector<float> l, r; fillNoise (l, r, nLen);
                const double before = integratedLufs (l, r, (int) kFs);
                h.run (l, r, 256, 1.0f, 1.0f);
                const double after = integratedLufs (l, r, (int) kFs);
                lo = std::min (lo, after - before);
                hi = std::max (hi, after - before);
            }
            std::printf ("    %-8s %+5.2f .. %+5.2f dB\n", d.name, lo, hi);
            CHECK (std::abs (lo) <= 1.0 && std::abs (hi) <= 1.0,
                   "%s stacked 1-3 deep moved the loudness %.2f .. %.2f dB", d.name, lo, hi);
        }
    }

    // -- 11c. THE TRAVEL: no step in the journey (§15) -----------------------
    /*  The slider held at each of 101 positions, a quarter second each. What
        is being caught is a rite that lurches — one slot arriving all at once
        because a curve or a capture put a cliff in the middle of the travel. */
    {
        Host h; h.prepare();
        h.bpm = 128.0; h.ppqStart = 0.0;
        h.rack.arrival.fireImpact = false;

        const int climb = effectTypeByName ("climb");
        const int chop  = effectTypeByName ("chop");
        const int tape  = effectTypeByName ("tape");
        h.rack.setSlotEffect (0, climb); setFlat (h.rack, 0, climb);
        setAB (h.rack, 0, 1, 16000.0f, 700.0f);
        h.rack.setSlotEffect (1, chop);  setFlat (h.rack, 1, chop);
        setAB (h.rack, 1, 0, 0.0f, 5.0f);
        h.rack.state (1).enter = 0.4f;
        h.rack.setSlotEffect (2, tape);  setFlat (h.rack, 2, tape);
        setAB (h.rack, 2, 0, 400.0f, 120.0f);
        h.rack.state (2).enter = 0.5f;

        /*  A whole second per step, measured over the last three quarters. A
            quarter-second window landed in a different part of CHOP's gate
            cycle at each position and read the variance as an 11 dB lurch —
            the meter, not the rite. */
        const int stepLen = (int) (kFs * 1.0);
        double prev = -200.0, worstStep = 0.0, lo = 1e9, hi = -1e9;
        for (int k = 0; k <= 100; ++k)
        {
            const float t = (float) k / 100.0f;
            rng.seed (0x99887766u);
            std::vector<float> l, r; fillNoise (l, r, stepLen);
            h.run (l, r, 256, t, t);
            const double L = integratedLufs (l, r, stepLen / 4);
            if (k > 0) worstStep = std::max (worstStep, std::abs (L - prev));
            prev = L;
            lo = std::min (lo, L); hi = std::max (hi, L);
        }
        std::printf ("  the travel               %.1f .. %.1f LUFS, worst 1 %% step %.2f dB\n",
                     lo, hi, worstStep);
        CHECK (worstStep <= 1.5, "the travel lurches: a 1 %% step moved the loudness %.2f dB", worstStep);
    }

    // -- 12. DETERMINISM -----------------------------------------------------
    {
        auto runAt = [&] (int block)
        {
            Host h; h.prepare (block);
            for (int i = 0; i < kSlots && i < numEffects(); ++i)
            {
                h.rack.setSlotEffect (i, i);
                setFlat (h.rack, i, i);
                h.rack.state (i).enter = 0.1f * i;
            }
            rng.seed (0x2468u);
            std::vector<float> l, r; fillNoise (l, r, nLen);
            //  a FIXED slider: automation granularity is the host's business
            //  (it delivers one value per buffer), and testing it here would
            //  be testing the host. What must not move is the rendering.
            h.run (l, r, block, 0.55f, 0.55f);
            return l;
        };
        auto a = runAt (256), b = runAt (64);
        float worst = 0;
        for (size_t i = 0; i < a.size(); ++i) worst = std::max (worst, std::abs (a[i] - b[i]));
        std::printf ("  determinism              block 64 vs 256: %.3g\n", worst);
        CHECK (worst == 0.0f, "the engine is block-size dependent (%.3g)", worst);
    }

    // -- 13. THE DROP IS LOUDER THAN THE BUILD -------------------------------
    /*  DISSOLVE -> IGNITE, the first factory rite (§14) — and the one line
        that encodes what the whole plugin is for. */
    {
        Host h; h.prepare (256);
        h.bpm = 128.0; h.ppqStart = 0.0;

        const int climb = effectTypeByName ("climb");
        const int tape  = effectTypeByName ("tape");
        const int stut  = effectTypeByName ("stutter");
        const int rise  = effectTypeByName ("riser");
        const int gap   = effectTypeByName ("gap");

        h.rack.setSlotEffect (0, climb);
        setFlat (h.rack, 0, climb);
        setAB (h.rack, 0, 1, 16000.0f, 900.0f);            // close down
        setAB (h.rack, 0, 2, 10.0f, 70.0f);
        h.rack.state (0).enter = 0.0f; h.rack.state (0).exit = 0.95f;

        h.rack.setSlotEffect (1, tape);
        setFlat (h.rack, 1, tape);
        setAB (h.rack, 1, 0, 420.0f, 90.0f);               // the head accelerates
        setAB (h.rack, 1, 1, 20.0f, 85.0f);
        h.rack.state (1).enter = 0.45f; h.rack.state (1).exit = 1.0f;

        h.rack.setSlotEffect (2, rise);
        setFlat (h.rack, 2, rise);
        setAB (h.rack, 2, 1, 180.0f, 6000.0f);
        setAB (h.rack, 2, 3, -60.0f, -10.0f);
        h.rack.state (2).enter = 0.35f; h.rack.state (2).exit = 1.0f;
        h.rack.state (2).curve = CurveAccel;

        h.rack.setSlotEffect (3, stut);
        setFlat (h.rack, 3, stut);
        setAB (h.rack, 3, 0, 1.0f, 5.0f);
        setAB (h.rack, 3, 1, 0.0f, 100.0f);
        h.rack.state (3).enter = 0.72f; h.rack.state (3).exit = 1.0f;

        h.rack.setSlotEffect (4, gap);
        setFlat (h.rack, 4, gap);
        setAB (h.rack, 4, 0, 0.0f, 100.0f);
        h.rack.state (4).enter = 0.93f; h.rack.state (4).exit = 1.0f;

        h.rack.monoGate = 0.8f;
        h.rack.arrival.fireImpact = true;
        for (int i = 0; i < kSlots; ++i) h.rack.state (i).tail = Tail::Bypass;
        h.rack.state (1).tail = Tail::Spill;              // the echoes spill over

        //  eight bars of build, then the arrival, then two bars of drop
        const int buildLen = (int) (kFs * 8.0 * 60.0 / 128.0 / 4.0 * 4.0);
        const int dropLen  = (int) (kFs * 4.0);
        std::vector<float> l, r; fillNoise (l, r, buildLen + dropLen, 0.30f);
        auto src = l;

        //  build
        std::vector<float> bl (l.begin(), l.begin() + buildLen), br (r.begin(), r.begin() + buildLen);
        h.run (bl, br, 256, 0.0f, 1.0f);
        h.rack.armArrival();
        std::vector<float> dl (l.begin() + buildLen, l.end()), dr (r.begin() + buildLen, r.end());
        h.run (dl, dr, 256, 1.0f, 1.0f);

        //  the loudest half-second of the build, against the first bar of the drop
        double loudestBuild = -200.0;
        const int win = (int) (kFs * 0.5);
        for (int i = 0; i + win < buildLen; i += win / 2)
        {
            std::vector<float> wl (bl.begin() + i, bl.begin() + i + win);
            std::vector<float> wr (br.begin() + i, br.begin() + i + win);
            loudestBuild = std::max (loudestBuild, integratedLufs (wl, wr));
        }
        const int barSamples = (int) (kFs * 4.0 * 60.0 / 128.0);
        std::vector<float> ql (dl.begin(), dl.begin() + std::min ((int) dl.size(), barSamples));
        std::vector<float> qr (dr.begin(), dr.begin() + std::min ((int) dr.size(), barSamples));
        const double drop = integratedLufs (ql, qr);

        const auto stb = measure (bl, br);
        std::printf ("  DISSOLVE -> IGNITE       build %.1f LUFS, drop %.1f LUFS, peak %.3f\n",
                     loudestBuild, drop, stb.peak);
        CHECK (stb.finite, "the rite produced a non-finite sample");
        CHECK (drop >= loudestBuild - 0.5,
               "the drop (%.1f LUFS) is quieter than the build (%.1f LUFS) — the plugin "
               "exists to make this false", drop, loudestBuild);
    }

    // -- 13b. ALIASING ON THE FAST READS --------------------------------------
    /*  The one place this plugin can sound cheap. Reading a delay line FASTER
        than it was written decimates it, and everything above the new Nyquist
        folds back into the band as a spurious tone — the sound of a bad
        varispeed. DIVE +12 st reads at exactly rate 2, so a 14 kHz tone should
        land at 28 kHz, i.e. nowhere: any energy at the fold (48 - 28 = 20 kHz)
        is an artefact and nothing else. Downward is the control: rate 0.5
        interpolates rather than decimates and cannot alias at all. */
    {
        std::printf ("  aliasing on the fast reads (a tone past the new Nyquist must not fold back):\n");
        const int dive = effectTypeByName ("dive");
        const int nLen = (int) (kFs * 2.0);

        struct Case { float semis; double inHz, foldHz; const char* label; };
        const Case cases[] = {
            { +12.0f, 14000.0, 20000.0, "DIVE +12 st (reads at 2x)" },
            {  -0.0f, 14000.0, 20000.0, "DIVE   0 st (the exact path)" },
            { -12.0f, 14000.0, 20000.0, "DIVE -12 st (reads at 0.5x)" },
        };

        for (const auto& cs : cases)
        {
            Host h; h.prepare (256);
            h.rack.setSlotEffect (0, dive);
            setFlat (h.rack, 0, dive);
            setAB (h.rack, 0, 0, cs.semis, cs.semis);
            std::vector<float> l, r; fillSine (l, r, nLen, cs.inHz, 0.5f);
            const double inMag = toneMag (l, (int) kFs / 2, 32768, cs.inHz);
            h.run (l, r, 256, 1.0f, 1.0f);
            const double fold = toneMag (l, (int) kFs / 2, 32768, cs.foldHz);
            const double db = 20.0 * std::log10 ((fold + 1e-12) / (inMag + 1e-12));
            std::printf ("    %-28s fold at %.0f kHz  %+6.1f dB\n", cs.label, cs.foldHz * 0.001, db);
            if (cs.semis > 0.0f)
                CHECK (db < -60.0, "DIVE +12 st folds a 14 kHz tone back at %.1f dB — that is the sound of a cheap varispeed", db);
            else
                CHECK (db < -80.0, "DIVE at %.0f st should not alias at all, but the fold is %.1f dB", (double) cs.semis, db);
        }
        /*  BRAKE only reads fast while it is CATCHING UP: from rest the head
            is already at the present and cannot read the future, so a launch
            has to follow a stop. Brake first, then launch, and measure the
            catch-up. */
        {
            Host h; h.prepare (256);
            const int brake = effectTypeByName ("brake");
            h.rack.setSlotEffect (0, brake);
            setFlat (h.rack, 0, brake);
            setAB (h.rack, 0, 0, 20.0f, 20.0f);              // stop: the head falls behind
            std::vector<float> l, r; fillSine (l, r, nLen, 14000.0, 0.5f);
            const double inMag = toneMag (l, (int) kFs / 2, 32768, 14000.0);
            h.run (l, r, 256, 1.0f, 1.0f);

            setAB (h.rack, 0, 0, 200.0f, 200.0f);            // launch: it reads at 2x
            std::vector<float> l2, r2; fillSine (l2, r2, nLen, 14000.0, 0.5f);
            h.run (l2, r2, 256, 1.0f, 1.0f);
            const double fold = toneMag (l2, 4096, 32768, 20000.0);
            const double db = 20.0 * std::log10 ((fold + 1e-12) / (inMag + 1e-12));
            std::printf ("    %-28s fold at 20 kHz  %+6.1f dB\n", "BRAKE launch out of a stop", db);
            CHECK (db < -60.0, "BRAKE folds a 14 kHz tone back at %.1f dB while it catches up", db);
        }

        /*  GRAIN reads fast too: SPREAD gives each grain its own pitch, up to
            an octave. Measured rather than assumed — a decorrelated cloud
            hides an artefact far better than a tone does, but not for ever. */
        {
            Host h; h.prepare (256);
            const int gr = effectTypeByName ("grain");
            h.rack.setSlotEffect (0, gr);
            setFlat (h.rack, 0, gr);
            setAB (h.rack, 0, 3, 12.0f, 12.0f);              // SPREAD, the full octave
            std::vector<float> l, r; fillSine (l, r, nLen, 14000.0, 0.5f);
            const double inMag = toneMag (l, (int) kFs / 2, 32768, 14000.0);
            h.run (l, r, 256, 1.0f, 1.0f);
            const double fold = toneMag (l, (int) kFs / 2, 32768, 20000.0);
            const double db = 20.0 * std::log10 ((fold + 1e-12) / (inMag + 1e-12));
            std::printf ("    %-28s fold at 20 kHz  %+6.1f dB\n", "GRAIN SPREAD 12 st", db);
            CHECK (db < -60.0, "GRAIN at full SPREAD folds a 14 kHz tone back at %.1f dB", db);
        }
    }

    // -- 13c. THE THIRD SIX DO WHAT THEY ARE NAMED FOR -------------------------
    /*  Every check here is of the BRIEF, because bounded-and-finite is the
        easiest thing in the world to pass while doing nothing. */
    {
        std::printf ("  the third six, against what they promise:\n");
        const int nLen3 = (int) (kFs * 2.0);

        /*  ORBIT: near, right, BACK, left — and the back has to be weaker,
            darker AND later, because one cue on its own is a pan pot. Two
            tones in one take separate level from colour. */
        {
            const int orbit = effectTypeByName ("orbit");
            std::vector<float> fl, fr, bl2, br2;
            auto render = [&] (float deg, std::vector<float>& l, std::vector<float>& r)
            {
                Host h; h.prepare (256);
                h.rack.setSlotEffect (0, orbit);
                setFlat (h.rack, 0, orbit);
                setAB (h.rack, 0, 0, deg, deg);          // ANGLE
                fillTwoTone (l, r, nLen3, 300.0, 6000.0);
                h.run (l, r, 256, 1.0f, 1.0f);
            };
            render (0.0f, fl, fr);                        // in front
            render (180.0f, bl2, br2);                    // behind

            const int at = (int) kFs / 2, win = 32768;
            const double loF = toneMag (fl, at, win, 300.0),  hiF = toneMag (fl, at, win, 6000.0);
            const double loB = toneMag (bl2, at, win, 300.0), hiB = toneMag (bl2, at, win, 6000.0);
            const double lvlDb = 20.0 * std::log10 ((loB + 1e-12) / (loF + 1e-12));
            const double colDb = 20.0 * std::log10 (((hiB / (loB + 1e-12)) + 1e-12)
                                                  / ((hiF / (loF + 1e-12)) + 1e-12));
            const int lag = bestLag (fl, bl2, at, 8192, 64);
            std::printf ("    ORBIT behind you       %+.1f dB, %+.1f dB of top, %d samples late\n",
                         lvlDb, colDb, lag);
            CHECK (lvlDb < -1.0, "ORBIT's back is not quieter than its front (%+.1f dB)", lvlDb);
            CHECK (colDb < -3.0, "ORBIT's back is not darker than its front (%+.1f dB)", colDb);
            CHECK (lag >= 4, "ORBIT's back is not LATE (%d samples) — the path round a head is longer", lag);

            //  and the sides have to be sides
            std::vector<float> rl, rr;
            render (90.0f, rl, rr);
            const double ml = toneMag (rl, at, win, 300.0), mr = toneMag (rr, at, win, 300.0);
            std::printf ("    ORBIT at 90 degrees    L %.4f  R %.4f\n", ml, mr);
            CHECK (mr > ml * 1.5, "ORBIT at 90 degrees is not to the RIGHT (L %.4f, R %.4f)", ml, mr);
        }

        /*  MANGLE: four engines that are actually four. A selector wired to
            nothing would pass every bounded check ever written. */
        {
            const int mangle = effectTypeByName ("mangle");
            std::vector<float> take[4];
            for (int e = 0; e < 4; ++e)
            {
                Host h; h.prepare (256);
                h.rack.setSlotEffect (0, mangle);
                setFlat (h.rack, 0, mangle);
                setAB (h.rack, 0, 0, (float) e, (float) e);   // ENGINE
                setAB (h.rack, 0, 1, 70.0f, 70.0f);           // DRIVE
                std::vector<float> r;
                fillNoise (take[e], r, nLen3);
                h.run (take[e], r, 256, 1.0f, 1.0f);
            }
            double worst = 1.0e30;
            for (int a = 0; a < 4; ++a)
                for (int b = a + 1; b < 4; ++b)
                    worst = std::min (worst, apart (take[a], take[b], (int) kFs / 2));
            std::printf ("    MANGLE engines         closest pair differs by %.3f\n", worst);
            CHECK (worst > 0.05, "two of MANGLE's four engines are the same thing (%.4f apart)", worst);

            //  and the oversampling has to be doing its job: a 9 kHz tone's
            //  third harmonic is at 27 kHz, i.e. nowhere — not folded to 21
            Host h; h.prepare (256);
            h.rack.setSlotEffect (0, mangle);
            setFlat (h.rack, 0, mangle);
            setAB (h.rack, 0, 0, 2.0f, 2.0f);                 // SINE FOLD, the worst case
            setAB (h.rack, 0, 1, 100.0f, 100.0f);
            std::vector<float> l, r; fillSine (l, r, nLen3, 9000.0, 0.5f);
            const double in9 = toneMag (l, (int) kFs / 2, 32768, 9000.0);
            h.run (l, r, 256, 1.0f, 1.0f);
            const double fold = toneMag (l, (int) kFs / 2, 32768, 21000.0);
            const double db = 20.0 * std::log10 ((fold + 1e-12) / (in9 + 1e-12));
            std::printf ("    MANGLE fold at 21 kHz  %+.1f dB\n", db);
            CHECK (db < -30.0, "MANGLE folds its third harmonic back at %+.1f dB", db);
        }

        /*  CHANT: the output is the CARRIER. Feed it 440 Hz and the pitch that
            comes out must be the carrier's, or it is a filter bank and not a
            vocoder. */
        {
            const int chant = effectTypeByName ("chant");
            Host h; h.prepare (256);
            h.rack.setSlotEffect (0, chant);
            setFlat (h.rack, 0, chant);
            std::vector<float> l, r; fillSine (l, r, (int) (kFs * 3.0), 440.0, 0.45f);
            h.run (l, r, 256, 1.0f, 1.0f);
            const double f = measureF0 (l, (int) kFs, 32768, 40.0, 900.0);
            std::printf ("    CHANT 440 Hz in        speaks at %.1f Hz (carrier C2 = 65.4)\n", f);
            CHECK (f > 55.0 && f < 80.0,
                   "CHANT did not take the carrier's pitch (%.1f Hz, wanted ~65.4)", f);
        }

        /*  SWARM: the crowd has to be a crowd. Both controls that make it one
            must change the sound — the Martian Gain rule that a knob which
            does nothing passes every other test. */
        {
            const int swarm = effectTypeByName ("swarm");
            auto render = [&] (float voices, float detune, std::vector<float>& l)
            {
                Host h; h.prepare (256);
                h.rack.setSlotEffect (0, swarm);
                setFlat (h.rack, 0, swarm);
                setAB (h.rack, 0, 0, voices, voices);
                setAB (h.rack, 0, 1, detune, detune);
                std::vector<float> r;
                fillNoise (l, r, nLen3);
                h.run (l, r, 256, 1.0f, 1.0f);
            };
            std::vector<float> a2, a8, d0;
            render (2.0f, 18.0f, a2);
            render (8.0f, 18.0f, a8);
            render (8.0f,  0.0f, d0);
            const double byVoices = apart (a2, a8, (int) kFs / 2);
            const double byDetune = apart (a8, d0, (int) kFs / 2);
            std::printf ("    SWARM 2 vs 8 voices    %.3f apart; detune 0 vs 18 c  %.3f apart\n",
                         byVoices, byDetune);
            CHECK (byVoices > 0.05, "SWARM's VOICES does nothing (%.4f apart)", byVoices);
            CHECK (byDetune > 0.05, "SWARM's DETUNE does nothing (%.4f apart)", byDetune);
        }

        /*  SWIRL earns its place only by not being BLOOM. Same nominal room,
            and they must still be different animals. */
        {
            auto render = [&] (const char* id, std::vector<float>& l)
            {
                const int t = effectTypeByName (id);
                Host h; h.prepare (256);
                h.rack.setSlotEffect (0, t);
                setFlat (h.rack, 0, t);
                std::vector<float> r;
                fillNoise (l, r, nLen3);
                h.run (l, r, 256, 1.0f, 1.0f);
            };
            std::vector<float> sw, bl3;
            render ("swirl", sw);
            render ("bloom", bl3);
            const double d = apart (sw, bl3, (int) kFs / 2);
            std::printf ("    SWIRL against BLOOM    %.3f apart\n", d);
            CHECK (d > 0.3, "SWIRL is BLOOM with a different name (%.3f apart)", d);
        }
    }

    // -- 13c. THE WRAPPED WORLD RACK (rop_bwfx.h) ---------------------------
    /*  The BWFX modules offered as slot effects. What this block holds them
        to is what is OURS — the registry join, the promise that assigning
        one changes nothing until it is edited, the three tail modes of §4,
        and bounded output with every knob moving at once. What each module
        SOUNDS like is BWFX's bench's business (§12a).

        Several of these questions are asked of the EFFECT, with no rack
        around it. That is not convenience: a loaded lane and an empty one
        take different paths through the rack's own output stage (the bass
        mono at rop_rack.cpp:322 runs only when the field is live), so a
        claim about the wrapper measured across the whole rack would be
        measuring that difference too. It cost three false failures to find
        out, and the first version of this block reported them as the
        wrapper's. */
    {
        const int base = numNativeEffects();
        const int nWrapped = numEffects() - base;
        std::printf ("  the wrapped World rack (%d modules):\n", nWrapped);
        CHECK (nWrapped > 0, "no BWFX module reached the registry");

        //  a) the join. No native index moved, every wrapped id resolves to
        //     its own index, and nothing outgrew a slot.
        CHECK (effectTypeByName ("climb") == 0, "CLIMB is no longer effect 0 - saved rites moved");
        CHECK (effectTypeByName ("gap") == 5, "GAP moved - saved rites moved");
        CHECK (effectTypeByName ("stutter") < base, "the native STUTTER lost its id to BWFX's GATE");
        CHECK (effectTypeByName ("bwfx.nonesuch") == -1, "an unknown bwfx id resolved to something");
        for (int t = base; t < numEffects(); ++t)
        {
            const auto& d = effectDescriptor (t);
            CHECK (std::strncmp (d.id, "bwfx.", 5) == 0, "%s sits in the wrapped range without a bwfx. id", d.id);
            CHECK (effectTypeByName (d.id) == t, "%s does not resolve to its own index", d.id);
            CHECK (d.numParams > 0 && d.numParams <= kMaxParams,
                   "%s has %d parameters and a slot carries %d", d.id, d.numParams, kMaxParams);
            CHECK (d.level == Level::Intentional && d.movesPitch,
                   "%s must be declared INTENTIONAL and a pitch mover - this plugin's bench does not govern it", d.id);
            CHECK (! d.perChannelParams, "%s claims two parameter sets; a BWFX module has one", d.id);
            CHECK (d.latency == 0, "%s reports latency, and the plugin pays a fixed worst case", d.id);
        }

        /*  the effect alone: parameters flat at the descriptor's defaults
            unless named, released at `releaseAt` if asked */
        auto direct = [&] (int type, std::vector<float>& l, std::vector<float>& r,
                           const char* knob, float value, int releaseAt, Tail how,
                           bool rearm = false)
        {
            std::unique_ptr<Effect> e (createEffect (type));
            if (! e) return;
            e->prepare (kFs, kSubBlock);
            e->reset();
            const auto& d = effectDescriptor (type);
            float p[kMaxParams] {};
            for (int i = 0; i < d.numParams; ++i)
                p[i] = (knob != nullptr && std::strcmp (d.params[i].id, knob) == 0)
                     ? value : d.params[i].def;

            Ctx c;
            c.fs = kFs; c.bpm = 128.0; c.ppq = 0.0;
            c.ppqPerSample = 128.0 / (60.0 * kFs);
            c.playing = true; c.t = 1.0f; c.u = 1.0f; c.inLane = true;

            e->arm();
            bool done = false;
            for (int i = 0; i < (int) l.size(); i += kSubBlock)
            {
                const int m = std::min (kSubBlock, (int) l.size() - i);
                if (releaseAt >= 0 && i >= releaseAt && ! done)
                {
                    e->release (how);
                    //  §9: the slider came back past ENTER, so whatever it
                    //  was holding is dropped rather than replayed
                    if (rearm) e->arm();
                    done = true;
                }
                e->process (l.data() + i, r.data() + i, m, p, p, c);
                c.ppq += (double) m * c.ppqPerSample;
            }
        };

        //  b) ASSIGNING ONE CHANGES NOTHING until it is edited
        //     (rop_rack.cpp:129 — and the reason each spec zeroes one knob)
        std::printf ("    inert at its slot defaults (against the signal it was handed):\n");
        for (int t = base; t < numEffects(); ++t)
        {
            rng.seed (0x5a5a01u);
            std::vector<float> l, r; fillNoise (l, r, (int) (kFs * 1.5));
            const std::vector<float> inL = l, inR = r;
            const double before = integratedLufs (inL, inR, (int) (kFs * 0.5));
            direct (t, l, r, nullptr, 0.0f, -1, Tail::Bypass);
            const double d = integratedLufs (l, r, (int) (kFs * 0.5)) - before;
            float worst = 0.0f;
            for (size_t i = (size_t) kFs / 2; i < l.size(); ++i)
                worst = std::max (worst, std::abs (l[i] - inL[i]));
            /*  NINE OF TEN ARE BIT-EXACT, and the tenth is declared here
                rather than excused in prose: TUBE's valve stage is in
                circuit at any drive, so assigning it lands a colour at
                once. Its LOUDNESS still may not move — it is a saturation
                at 0 dB, not a fader. Everything else must hand back the
                samples it was given, because zeroing one amount knob is
                what the spec table promises. */
            /*  EIGHT OF TEN HAND BACK THE SAMPLES THEY WERE GIVEN, and the
                two that do not are declared here rather than excused in
                prose, because "nearly transparent" is how a plugin ends up
                quietly colouring a master:

                  TUBE      its valve stage is in circuit at any drive, so
                            assigning it lands a colour at once. The
                            LOUDNESS still may not move: it is a saturation
                            at 0 dB, not a fader.
                  HARMONIC  its crossover stays in the path at depth 0, so
                            the sum is arithmetic rather than bit-exact. It
                            measures 3e-8, which is around -150 dBFS: the
                            tolerance is there to say WHY it is not zero,
                            not to leave room for a sound. */
            const char* eid = effectDescriptor (t).id;
            const bool colour   = std::strcmp (eid, "bwfx.saturation") == 0;
            const bool residual = std::strcmp (eid, "bwfx.trem") == 0;
            std::printf ("      %-9s %+5.2f dB, worst sample %.3g%s\n",
                         effectDescriptor (t).name, d, worst,
                         colour ? "   (in circuit at 0 dB)" : (residual ? "   (crossover only)" : ""));
            CHECK (std::abs (d) <= 0.5,
                   "%s moved the loudness %.2f dB at its slot defaults",
                   effectDescriptor (t).name, d);
            if (! colour)
                CHECK (worst <= (residual ? 1.0e-4f : 0.0f),
                       "%s is not inert at its slot defaults (worst sample %.3g)",
                       effectDescriptor (t).name, worst);
        }

        //  c) EVERY KNOB MOVING AT ONCE, lo to hi across the whole travel.
        //     Check 11 sweeps one knob with the rest at their (deliberately
        //     switched off) defaults, which is a weak question to ask here.
        for (int t = base; t < numEffects(); ++t)
        {
            const auto& d = effectDescriptor (t);
            Host h; h.prepare();
            h.rack.setSlotEffect (0, t);
            for (int p = 0; p < d.numParams; ++p) setAB (h.rack, 0, p, d.params[p].lo, d.params[p].hi);
            rng.seed (0x5a5a02u);
            std::vector<float> l, r; fillNoise (l, r, (int) (kFs * 2.0), 0.7f);
            h.run (l, r, 256, 0.0f, 1.0f);
            const auto st = measure (l, r);
            CHECK (st.finite, "%s with every knob travelling produced a non-finite sample", d.name);
            CHECK (st.peak <= 1.01f, "%s with every knob travelling reached %.3f", d.name, st.peak);
        }
        std::printf ("    every knob travelling    bounded and finite\n");

        //  d) THE THREE TAIL MODES, §4. ECHO is the one in the set with a
        //     tail worth the distinction; TUBE is one without, and a module
        //     with nothing to spill must get OUT OF THE WAY rather than keep
        //     grinding through the drop.
        {
            const int echo = effectTypeByName ("bwfx.delay");
            const int fed = (int) (kFs * 0.5), tail = (int) (kFs * 1.0);
            auto tailRms = [&] (Tail how, bool rearm)
            {
                rng.seed (0x5a5a03u);
                std::vector<float> l, r; fillNoise (l, r, fed, 0.5f);
                l.resize ((size_t) (fed + tail), 0.0f);
                r.resize ((size_t) (fed + tail), 0.0f);
                direct (echo, l, r, "mix", 100.0f, fed, how, rearm);
                std::vector<float> after (l.begin() + fed, l.end());
                return measure (after, after).rms;
            };
            const double rs = tailRms (Tail::Spill, false), rc = tailRms (Tail::Clear, false);
            /*  BYPASS is not measured here because it is not the effect's to
                answer: a released BYPASS slot is skipped by the rack
                (rop_rack.cpp:407) and never reaches the output at all. What
                the WRAPPER owes on a bypass is §9 — the line is dropped, so
                coming back into the lane does not replay last night's
                build. */
            const double rr = tailRms (Tail::Bypass, true);
            std::printf ("    ECHO released            SPILL %.4f, CLEAR %.7f, re-armed %.7f\n", rs, rc, rr);
            CHECK (rs > 1.0e-3, "ECHO set to SPILL went silent when it was released (%.6f)", rs);
            CHECK (rc < 1.0e-6, "ECHO set to CLEAR kept ringing (%.7f)", rc);
            CHECK (rs > rc * 1000.0, "SPILL and CLEAR are the same sound");
            CHECK (rr < 1.0e-6, "ECHO replayed what it held when the lane was re-entered (%.7f)", rr);
        }
        {
            const int tube = effectTypeByName ("bwfx.saturation");
            const int half = (int) (kFs * 0.5);
            rng.seed (0x5a5a04u);
            std::vector<float> l, r; fillNoise (l, r, half * 2, 0.5f);
            const std::vector<float> inL = l;
            direct (tube, l, r, "drive", 24.0f, half, Tail::Spill);
            float moved = 0.0f, after = 0.0f;
            for (int i = 0; i < half; ++i) moved = std::max (moved, std::abs (l[(size_t) i] - inL[(size_t) i]));
            for (int i = half + kSubBlock; i < half * 2; ++i)
                after = std::max (after, std::abs (l[(size_t) i] - inL[(size_t) i]));
            std::printf ("    TUBE released to SPILL   %.3f before, %.3g after\n", moved, after);
            CHECK (moved > 0.01f, "TUBE at 24 dB was not processing in the first place (%.3g)", moved);
            CHECK (after == 0.0f, "a tailless module set to SPILL is still processing (%.3g)", after);
        }

        //  e) the message-thread pump reaches them and changes nothing about
        //     the rendering (the eighteen natives need no service at all)
        {
            auto render = [&] (bool pump)
            {
                Host h; h.prepare();
                h.rack.arrival.fireImpact = false;
                const int type = effectTypeByName ("bwfx.shimmer");
                h.rack.setSlotEffect (0, type);
                setFlat (h.rack, 0, type);
                rng.seed (0x5a5a05u);
                std::vector<float> l, r; fillNoise (l, r, (int) (kFs * 0.5));
                for (int i = 0; i < (int) l.size(); i += 4096)
                {
                    const int m = std::min (4096, (int) l.size() - i);
                    std::vector<float> sl (l.begin() + i, l.begin() + i + m), sr (sl);
                    h.run (sl, sr, 256, 0.5f, 0.5f);
                    std::copy (sl.begin(), sl.end(), l.begin() + i);
                    if (pump) h.rack.service();
                }
                return l;
            };
            auto a = render (false), b = render (true);
            float worst = 0.0f;
            for (size_t i = 0; i < a.size(); ++i) worst = std::max (worst, std::abs (a[i] - b[i]));
            std::printf ("    service() pumped         %.3g\n", worst);
            CHECK (worst == 0.0f, "servicing a live slot changed the rendering (%.3g)", worst);
        }
    }

    // -- 14. COST -------------------------------------------------------------
    {
        Host h; h.prepare (256);
        /*  THE WORST CASE, not the first six. `i % numEffects()` loaded
            CLIMB..GAP, which are the cheap ones — a cost figure taken from
            them says nothing about a rack somebody would actually build. */
        const char* heavy[kSlots] = { "chant", "swarm", "swirl", "mangle", "bloom", "grain" };
        for (int i = 0; i < kSlots; ++i)
        {
            const int type = effectTypeByName (heavy[i]);
            h.rack.setSlotEffect (i, type);
            setFlat (h.rack, i, type);
            if (std::strcmp (heavy[i], "chant") == 0) setAB (h.rack, i, 2, 2.0f, 2.0f);  // 24 bands
            if (std::strcmp (heavy[i], "swarm") == 0) setAB (h.rack, i, 0, 8.0f, 8.0f);  // 8 voices
        }
        const int n = (int) (kFs * 4.0);
        std::vector<float> l, r; fillNoise (l, r, n);
        const auto t0 = std::chrono::steady_clock::now();
        h.run (l, r, 256, 0.0f, 1.0f);
        const auto t1 = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("  the six most expensive   %.1f x real time (%.1f %% of one core)\n",
                     4.0 / secs, 100.0 * secs / 4.0);
        CHECK (measure (l, r).finite, "the cost run produced a non-finite sample");
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" — ALL CLEAR\n");
    else               std::printf (" — %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
