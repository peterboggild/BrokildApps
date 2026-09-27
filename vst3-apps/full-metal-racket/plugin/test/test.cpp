/*  FULL METAL RACKET — offline bench.

    Plain C++, no JUCE, renders the same audio the plugin does. The house
    rule in this family: if it is not measured it is not done.

    Two traps this family keeps falling into, kept on the wall here:
      * render long enough to REACH the thing under test (the KIERANATOR
        bar-length bug — a test that never plays the step passes vacuously);
      * make sure the probe can SEE the effect (pink's 24.6 s LFO measured
        over 12 s). A vacuous pass is worse than a failure.                */

#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>

using namespace fmr;

static int checks = 0, fails = 0;
static std::vector<std::string> failMsgs;

static void ok (bool cond, const char* what, double got = 0.0, double want = 0.0)
{
    ++checks;
    if (! cond)
    {
        ++fails;
        char buf[512];
        std::snprintf (buf, sizeof (buf), "FAIL  %-58s got %.6g  want %.6g", what, got, want);
        failMsgs.push_back (buf);
        std::printf ("%s\n", buf);
    }
}

//==============================================================================
struct Buf
{
    std::vector<float> L, R;
    explicit Buf (int n) : L ((size_t) n, 0.0f), R ((size_t) n, 0.0f) {}
    int n() const { return (int) L.size(); }
};

static void render (Engine& e, Buf& b, int block = 256)
{
    int i = 0;
    while (i < b.n())
    {
        const int k = std::min (block, b.n() - i);
        e.process (b.L.data() + i, b.R.data() + i, k);
        i += k;
    }
}

static float peak (const Buf& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.n();
    float m = 0.0f;
    for (int i = from; i < to; ++i) { m = std::max (m, std::fabs (b.L[(size_t) i])); m = std::max (m, std::fabs (b.R[(size_t) i])); }
    return m;
}

static double rms (const Buf& b, int from, int to)
{
    if (to > b.n()) to = b.n();
    if (to <= from) return 0.0;
    double s = 0.0;
    for (int i = from; i < to; ++i) s += (double) b.L[(size_t) i] * b.L[(size_t) i] + (double) b.R[(size_t) i] * b.R[(size_t) i];
    return std::sqrt (s / (double) (2 * (to - from)));
}

static bool finiteAll (const Buf& b)
{
    for (int i = 0; i < b.n(); ++i)
        if (! std::isfinite (b.L[(size_t) i]) || ! std::isfinite (b.R[(size_t) i])) return false;
    return true;
}

/*  Magnitude at f over [from,to), by direct DFT with a Hann window.

    NOT a bare Goertzel: its rectangular window has -13 dB sidelobes, and over
    tens of thousands of samples of a signal that decayed away in the first
    few thousand, the sidelobe skirt of the transient buries the tone. The
    first version of this file measured every drum ~850 cents flat and it was
    the probe that was wrong, not the engine. Window it.                     */
static double goertzel (const Buf& b, double f, double fs, int from, int to)
{
    if (to > b.n()) to = b.n();
    const int N = to - from;
    if (N <= 8) return 0.0;
    const double w = 2.0 * M_PI * f / fs;
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (double) (N - 1));
        const double x = (double) b.L[(size_t) (from + i)] * win;
        re += x * std::cos (w * i);
        im -= x * std::sin (w * i);
        wsum += win;
    }
    return 2.0 * std::sqrt (re * re + im * im) / std::max (1.0, wsum);
}

// the dominant frequency in a window, by brute scan (coarse then fine)
static double dominant (const Buf& b, double fs, int from, int to, double lo, double hi)
{
    double bestF = lo, bestM = -1.0;
    for (double f = lo; f <= hi; f *= 1.01)
    {
        const double m = goertzel (b, f, fs, from, to);
        if (m > bestM) { bestM = m; bestF = f; }
    }
    for (double f = bestF * 0.97; f <= bestF * 1.03; f *= 1.0008)
    {
        const double m = goertzel (b, f, fs, from, to);
        if (m > bestM) { bestM = m; bestF = f; }
    }
    return bestF;
}

static void fresh (Engine& e, double fs = 48000.0)
{
    e.p = Params();
    e.prepare (fs, 256);
    e.reset();
}

//==============================================================================
static void groupTable()
{
    std::printf ("\n-- 1. the parameter table --------------------------------------\n");
    const int n = numParams();
    ok (n == NCH * NCP + NGP, "table size is 12 channels x 10 plus 8 globals", n, NCH * NCP + NGP);

    for (int i = 0; i < n; ++i)
    {
        const PSpec& s = paramSpec (i);
        ok (s.id != nullptr && s.id[0] != 0, "every row has an id");
        ok (paramIndex (s.id) == i, (std::string ("id round trips: ") + s.id).c_str(), paramIndex (s.id), i);
        ok (s.def >= 0.0f && s.def <= paramMax (s), (std::string ("default in range: ") + s.id).c_str(), s.def, paramMax (s));
        for (int j = i + 1; j < n; ++j)
            if (std::strcmp (s.id, paramSpec (j).id) == 0) ok (false, "ids are unique");
    }

    // the defaults really are what a fresh Params carries
    Params p;
    for (int i = 0; i < n; ++i)
    {
        const PSpec& s = paramSpec (i);
        ok (pvalue (p, s) == s.def, (std::string ("fresh Params matches def: ") + s.id).c_str());
    }

    // model lists exist for every channel and cover the model range
    for (int c = 0; c < NCH; ++c)
    {
        int cnt = 0;
        const char* const* names = listNames ((std::string (channelId (c)) + "_model").c_str(), cnt);
        ok (names != nullptr && cnt >= 3, (std::string ("models listed for ") + channelName (c)).c_str(), cnt, 3);
    }
}

//==============================================================================
static void groupSilence()
{
    std::printf ("\n-- 2. silence in, silence out -----------------------------------\n");
    for (int os = 0; os <= 2; ++os)
    {
        Engine e; fresh (e);
        e.p.g[GP_OS] = (float) os;
        Buf b (24000);
        render (e, b);
        float m = 0.0f;
        for (int i = 0; i < b.n(); ++i) m = std::max (m, std::max (std::fabs (b.L[(size_t) i]), std::fabs (b.R[(size_t) i])));
        ok (m == 0.0f, (std::string ("no triggers -> exact digital zero at ") + std::to_string (os == 0 ? 1 : (os == 1 ? 2 : 4)) + "x").c_str(), m, 0.0);
    }

    // muted channels are silent
    Engine e; fresh (e);
    for (int c = 0; c < NCH; ++c) e.p.ch[c][CP_MUTE] = 1.0f;
    for (int c = 0; c < NCH; ++c) e.trigger (c, 1.0f);
    Buf b (48000); render (e, b);
    ok (peak (b) < 1e-6f, "all channels muted -> silent", peak (b), 0.0);
}

//==============================================================================
static void groupVoices()
{
    std::printf ("\n-- 3. every channel, every model --------------------------------\n");
    std::printf ("    %-8s %-10s %8s %8s %10s\n", "CHANNEL", "MODEL", "PEAK", "dBFS", "DECAY");

    for (int c = 0; c < NCH; ++c)
    {
        int nm = 0;
        const char* const* names = listNames ((std::string (channelId (c)) + "_model").c_str(), nm);
        for (int m = 0; m < nm; ++m)
        {
            Engine e; fresh (e);
            e.p.ch[c][CP_MODEL] = (float) m;
            e.p.g[GP_BLEED] = 0.0f;         // this channel alone
            e.p.g[GP_BODY]  = 0.0f;
            e.p.g[GP_AGE]   = 0.0f;
            e.trigger (c, 1.0f);

            Buf b (48000 * 4);
            render (e, b);

            const float pk = peak (b);
            const double r1 = rms (b, 0, 8000);
            const double r2 = rms (b, 48000 * 3, 48000 * 4);
            const std::string tag = std::string (channelName (c)) + "/" + names[m];

            std::printf ("    %-8s %-10s %8.4f %8.1f %10.2e\n", channelName (c), names[m],
                         pk, 20.0 * std::log10 (std::max (1e-9f, pk)), r2);

            ok (finiteAll (b), (tag + ": finite").c_str());
            ok (pk <= 1.0f, (tag + ": bounded at full scale").c_str(), pk, 1.0);
            ok (pk > 0.02f, (tag + ": audible").c_str(), pk, 0.02);
            ok (pk < 0.999f, (tag + ": not slammed into the ceiling").c_str(), pk, 0.999);
            ok (r2 < r1 * 0.35, (tag + ": decays away").c_str(), r2, r1 * 0.35);
        }
    }
}

//==============================================================================
static void groupTuning()
{
    std::printf ("\n-- 4. tuning -----------------------------------------------------\n");

    // BD1 PING: the resonator's frequency must be the TUNE setting
    for (float tv : { 0.15f, 0.45f, 0.80f })
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[0][CP_MODEL] = 0.0f;
        e.p.ch[0][CP_TUNE]  = tv;
        e.p.ch[0][CP_BEND]  = 0.0f;      // no pitch envelope in the way
        e.p.ch[0][CP_DRIVE] = 0.0f;
        e.p.ch[0][CP_DECAY] = 0.85f;
        e.trigger (0, 0.35f);            // gently: tension must not bend it

        Buf b (48000);
        render (e, b);
        const double want = (double) xmap (tv, 30.0f, 120.0f);
        // measure late, where the excitation and the click are long gone
        const double got = dominant (b, 48000.0, 12000, 40000, want * 0.6, want * 1.7);
        const double cents = 1200.0 * std::log2 (got / want);
        std::printf ("    BD1 PING tune %.2f -> %.2f Hz (want %.2f, %+.1f cents)\n", tv, got, want, cents);
        ok (std::fabs (cents) < 45.0, "BD1 PING rings at its TUNE setting", cents, 0.0);
    }

    // octave: doubling the frequency really doubles it
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[4][CP_BEND] = 0.0f; e.p.ch[4][CP_DECAY] = 0.8f;
        e.p.ch[4][CP_TUNE] = 0.2f;
        e.trigger (4, 0.3f);
        Buf b1 (48000); render (e, b1);
        const double f1 = dominant (b1, 48000.0, 9000, 40000, 40.0, 500.0);

        Engine e2; fresh (e2);
        e2.p.g[GP_BLEED] = 0.0f; e2.p.g[GP_BODY] = 0.0f; e2.p.g[GP_AGE] = 0.0f;
        e2.p.ch[4][CP_BEND] = 0.0f; e2.p.ch[4][CP_DECAY] = 0.8f;
        // one octave up in the exponential map
        const float lo = 60.0f, hi = 420.0f;
        const float step = std::log (2.0f) / std::log (hi / lo);
        e2.p.ch[4][CP_TUNE] = 0.2f + step;
        e2.trigger (4, 0.3f);
        Buf b2 (48000); render (e2, b2);
        const double f2 = dominant (b2, 48000.0, 9000, 40000, 40.0, 900.0);

        std::printf ("    TOM 1 octave: %.2f -> %.2f Hz (ratio %.4f)\n", f1, f2, f2 / f1);
        ok (std::fabs (f2 / f1 - 2.0) < 0.06, "one octave on the TUNE knob is a factor of two", f2 / f1, 2.0);
    }
}

//==============================================================================
static void groupTension()
{
    std::printf ("\n-- 5. BLOCK B: tension ------------------------------------------\n");

    /*  A struck membrane is stiffer the further it is displaced, so a hard hit
        must START SHARP and fall to pitch. Measure the early window against
        the late window on the same voice, then prove it does NOT happen with
        tension at zero (or the probe is measuring the pitch envelope). */
    auto measure = [] (float tension) -> double
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[4][CP_MODEL] = 0.0f;          // TOM MEMBRANE
        e.p.ch[4][CP_TUNE]  = 0.30f;
        e.p.ch[4][CP_DECAY] = 0.85f;
        e.p.ch[4][CP_BEND]  = tension;
        e.p.ch[4][CP_DRIVE] = 0.0f;
        e.p.ch[4][CP_SNAP]  = 0.0f;
        e.trigger (4, 1.0f);
        Buf b (48000 * 2); render (e, b);
        const double base = (double) xmap (0.30f, 60.0f, 420.0f);
        const double early = dominant (b, 48000.0, 600,  4200,  base * 0.7, base * 2.4);
        const double late  = dominant (b, 48000.0, 40000, 90000, base * 0.7, base * 2.4);
        return 1200.0 * std::log2 (early / late);
    };

    const double withT = measure (0.95f);
    const double without = measure (0.0f);
    std::printf ("    early-vs-late pitch: tension 95%% = %+.1f cents, tension 0 = %+.1f cents\n", withT, without);
    /*  Toms carry a deliberate fixed pitch envelope of their own (10%), so
        "tension zero" is not "pitch flat" — it is about +37 cents, and the
        first version of this check called that a failure. What TENSION must
        do is add a lot on top of it. */
    ok (withT > without + 60.0, "a hard hit starts sharp when TENSION is up", withT, without + 60.0);
    ok (without < 60.0, "and the pitch envelope alone stays modest", without, 60.0);
}

//==============================================================================
static void groupDecayIsQ()
{
    std::printf ("\n-- 6. DECAY really is the decay ---------------------------------\n");
    for (int c : { 0, 4, 8, 10 })
    {
        double prev = -1.0;
        for (float d : { 0.15f, 0.5f, 0.9f })
        {
            Engine e; fresh (e);
            e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
            e.p.ch[c][CP_DECAY] = d;
            e.trigger (c, 1.0f);
            Buf b (48000 * 6); render (e, b);

            // the sample at which the envelope has fallen 40 dB
            const float pk = peak (b);
            int t60 = b.n() - 1;
            for (int i = 0; i < b.n(); i += 64)
                if (rms (b, i, std::min (b.n(), i + 2048)) < pk * 0.01) { t60 = i; break; }
            const double sec = t60 / 48000.0;
            std::printf ("    %-6s decay %.2f -> -40 dB at %.3f s\n", channelName (c), d, sec);
            ok (sec > prev, (std::string (channelName (c)) + ": more DECAY rings longer").c_str(), sec, prev);
            prev = sec;
        }
    }
}

//==============================================================================
static void groupWeb()
{
    std::printf ("\n-- 7. THE WEB works, not merely is bounded ----------------------\n");

    /*  The Mars Wars lesson: a bench that proves BOUNDED proves nothing about
        WORKING. A bleed matrix wired to nothing at all would pass every other
        test in this file. So: hit the kick with every OTHER channel muted
        except the one under test, and measure whether that channel makes any
        sound at all. */
    for (int target : { 4, 9, 11 })
    {
        Engine e; fresh (e);
        e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.g[GP_BLEED] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != target) e.p.ch[c][CP_MUTE] = 1.0f;
        e.trigger (0, 1.0f);               // hit the kick; the kick is muted
        Buf dry (24000); render (e, dry);

        Engine e2; fresh (e2);
        e2.p.g[GP_BODY] = 0.0f; e2.p.g[GP_AGE] = 0.0f;
        e2.p.g[GP_BLEED] = 0.85f;
        for (int c = 0; c < NCH; ++c) if (c != target) e2.p.ch[c][CP_MUTE] = 1.0f;
        e2.trigger (0, 1.0f);
        Buf wet (24000); render (e2, wet);

        const float pd = peak (dry), pw = peak (wet);
        std::printf ("    kick -> %-6s : bleed 0 = %.2e, bleed 85%% = %.2e\n", channelName (target), pd, pw);
        ok (pd < 1e-6f, (std::string ("no bleed -> ") + channelName (target) + " stays silent").c_str(), pd, 0.0);
        ok (pw > 1e-3f, (std::string ("bleed -> ") + channelName (target) + " rings").c_str(), pw, 1e-3);
    }

    // and the shell it rings is tuned to that channel
    {
        Engine e; fresh (e);
        e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_BLEED] = 0.9f;
        for (int c = 0; c < NCH; ++c) if (c != 4) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[4][CP_TUNE] = 0.5f;
        e.trigger (0, 1.0f);
        Buf b (24000); render (e, b);
        const double want = (double) xmap (0.5f, 60.0f, 420.0f);
        const double got = dominant (b, 48000.0, 400, 12000, want * 0.5, want * 2.0);
        std::printf ("    the shell that rings is tuned to TOM 1: %.1f Hz (want %.1f)\n", got, want);
        ok (std::fabs (1200.0 * std::log2 (got / want)) < 120.0, "the sympathetic shell follows its channel's TUNE", got, want);
    }
}

//==============================================================================
static void groupRailAndBody()
{
    std::printf ("\n-- 8. rail sag and the kit body ---------------------------------\n");

    auto renderPair = [] (int gp, float amt) -> std::pair<Buf, Buf>
    {
        Engine a; fresh (a); a.p.g[gp] = 0.0f; a.p.g[GP_AGE] = 0.0f;
        Engine b; fresh (b); b.p.g[gp] = amt;  b.p.g[GP_AGE] = 0.0f;
        if (gp == GP_SAG) { a.p.g[GP_BLEED] = 0.0f; b.p.g[GP_BLEED] = 0.0f; a.p.g[GP_BODY] = 0.0f; b.p.g[GP_BODY] = 0.0f; }
        else              { a.p.g[GP_BLEED] = 0.0f; b.p.g[GP_BLEED] = 0.0f; }
        Buf x (48000), y (48000);
        a.trigger (0, 1.0f); a.trigger (8, 1.0f); a.trigger (2, 1.0f);
        b.trigger (0, 1.0f); b.trigger (8, 1.0f); b.trigger (2, 1.0f);
        render (a, x); render (b, y);
        return { x, y };
    };

    {
        auto pr = renderPair (GP_SAG, 0.9f);
        double diff = 0.0, ref = 0.0;
        for (int i = 0; i < pr.first.n(); ++i)
        {
            diff += std::fabs (pr.first.L[(size_t) i] - pr.second.L[(size_t) i]);
            ref  += std::fabs (pr.first.L[(size_t) i]);
        }
        const double rel = diff / std::max (1e-9, ref);
        std::printf ("    RAIL SAG at 90%% changes the render by %.1f%%\n", rel * 100.0);
        ok (rel > 0.03, "RAIL SAG measurably ducks the kit", rel, 0.03);
        ok (peak (pr.second) <= peak (pr.first) + 1e-4f, "and it ducks rather than boosts", peak (pr.second), peak (pr.first));
    }

    {
        // sag at zero must be bit-identical to no sag at all
        Engine a; fresh (a); a.p.g[GP_SAG] = 0.0f; a.p.g[GP_AGE] = 0.0f; a.p.g[GP_BLEED] = 0.0f; a.p.g[GP_BODY] = 0.0f;
        Engine b; fresh (b); b.p.g[GP_SAG] = 0.0f; b.p.g[GP_AGE] = 0.0f; b.p.g[GP_BLEED] = 0.0f; b.p.g[GP_BODY] = 0.0f;
        a.trigger (0, 1.0f); b.trigger (0, 1.0f);
        Buf x (24000), y (24000); render (a, x); render (b, y);
        ok (std::memcmp (x.L.data(), y.L.data(), x.L.size() * sizeof (float)) == 0, "two identical engines render identically");
    }

    {
        auto pr = renderPair (GP_BODY, 0.9f);
        double diff = 0.0, ref = 0.0;
        for (int i = 0; i < pr.first.n(); ++i)
        {
            diff += std::fabs (pr.first.L[(size_t) i] - pr.second.L[(size_t) i]);
            ref  += std::fabs (pr.first.L[(size_t) i]);
        }
        std::printf ("    KIT BODY at 90%% changes the render by %.1f%%\n", diff / std::max (1e-9, ref) * 100.0);
        ok (diff / std::max (1e-9, ref) > 0.03, "KIT BODY is audible", diff / std::max (1e-9, ref), 0.03);
        ok (finiteAll (pr.second), "KIT BODY stays finite");
    }
}

//==============================================================================
static void groupChokeAndLink()
{
    std::printf ("\n-- 9. the hat pair ----------------------------------------------\n");

    // a closed hat stops an open one
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 9) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[9][CP_DECAY] = 0.95f;
        e.trigger (9, 1.0f);
        Buf a (24000); render (e, a);
        const double openTail = rms (a, 18000, 24000);

        Engine f2; fresh (f2);
        f2.p.g[GP_BLEED] = 0.0f; f2.p.g[GP_BODY] = 0.0f; f2.p.g[GP_AGE] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 9) f2.p.ch[c][CP_MUTE] = 1.0f;   // CH muted: only the choke can act
        f2.p.ch[9][CP_DECAY] = 0.95f;
        f2.trigger (9, 1.0f);
        Buf b (24000);
        f2.process (b.L.data(), b.R.data(), 4800);            // 100 ms later...
        f2.trigger (8, 1.0f);                                  // ...the closed hat
        f2.process (b.L.data() + 4800, b.R.data() + 4800, 24000 - 4800);
        const double chokedTail = rms (b, 18000, 24000);

        std::printf ("    open hat tail: free %.3e, choked %.3e\n", openTail, chokedTail);
        ok (openTail > 1e-4, "an open hat has a tail to choke", openTail, 1e-4);
        ok (chokedTail < openTail * 0.05, "a closed hat chokes it", chokedTail, openTail * 0.05);
    }

    // LINK: OH follows CH's tune
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.g[GP_HATLINK] = 1.0f;
        for (int c = 0; c < NCH; ++c) if (c != 9) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[8][CP_TUNE] = 0.9f;      // CH high
        e.p.ch[9][CP_TUNE] = 0.1f;      // OH low, and must be ignored
        e.trigger (9, 1.0f);
        Buf linked (24000); render (e, linked);

        Engine u; fresh (u);
        u.p.g[GP_BLEED] = 0.0f; u.p.g[GP_BODY] = 0.0f; u.p.g[GP_AGE] = 0.0f;
        u.p.g[GP_HATLINK] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 9) u.p.ch[c][CP_MUTE] = 1.0f;
        u.p.ch[8][CP_TUNE] = 0.9f;
        u.p.ch[9][CP_TUNE] = 0.1f;
        u.trigger (9, 1.0f);
        Buf free_ (24000); render (u, free_);

        /*  The exact test, rather than a proxy: a LINKed open hat must render
            EXACTLY as an unlinked one whose own TUNE was set to the closed
            hat's. Comparing dominant frequencies instead mostly measured the
            band filters, which LINK does not touch. */
        Engine m; fresh (m);
        m.p.g[GP_BLEED] = 0.0f; m.p.g[GP_BODY] = 0.0f; m.p.g[GP_AGE] = 0.0f;
        m.p.g[GP_HATLINK] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 9) m.p.ch[c][CP_MUTE] = 1.0f;
        m.p.ch[9][CP_TUNE] = 0.9f;                 // by hand, what LINK should do
        m.trigger (9, 1.0f);
        Buf manual (24000); render (m, manual);

        double dLinked = 0.0, dFree = 0.0;
        for (size_t i = 0; i < manual.L.size(); ++i)
        {
            dLinked += std::fabs (manual.L[i] - linked.L[i]);
            dFree   += std::fabs (manual.L[i] - free_.L[i]);
        }
        std::printf ("    OH vs a hand-tuned twin: LINKed %.3e, unlinked %.3e\n", dLinked, dFree);
        ok (dLinked < 1e-6, "LINKed, the open hat renders as the closed hat's tune", dLinked, 0.0);
        ok (dFree > 1.0, "unlinked, it keeps its own tune", dFree, 1.0);
    }
}

//==============================================================================
static void groupVelocity()
{
    std::printf ("\n-- 10. velocity does three things -------------------------------\n");

    for (int c : { 0, 2, 8 })
    {
        auto shot = [c] (float v) -> Buf
        {
            Engine e; fresh (e);
            e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
            for (int k = 0; k < NCH; ++k) if (k != c) e.p.ch[k][CP_MUTE] = 1.0f;
            e.trigger (c, v);
            Buf b (48000); render (e, b);
            return b;
        };
        Buf soft = shot (0.25f), hard = shot (1.0f);

        // louder
        ok (peak (hard) > peak (soft) * 1.6f, (std::string (channelName (c)) + ": harder is louder").c_str(),
            peak (hard), peak (soft) * 1.6f);

        // brighter: the ratio of high-band energy to total must rise
        /*  Spectral centroid, not "share above 2.5 kHz". The share measure
            flips sign on a kick: its click owns that band, and at low velocity
            the click is a LARGER fraction of a much smaller sound. The
            centroid asks the question actually being asked — where is the
            weight of this sound? */
        /*  Upper content RELATIVE to the body, not a bare centroid: a
            kick's centroid is pinned by its own fundamental, so opening its
            filter by a full octave still reads as "darker". This asks the
            question that was meant — how much is going on above the drum's
            own note compared with the note itself. */
        const double f0 = (double) xmap (paramSpec (paramIndex ((std::string (channelId (c)) + "_tune").c_str())).def,
                                         paramSpec (paramIndex ((std::string (channelId (c)) + "_tune").c_str())).lo,
                                         paramSpec (paramIndex ((std::string (channelId (c)) + "_tune").c_str())).hi);
        auto bright = [f0] (const Buf& b) -> double
        {
            double hi = 0.0, lo = 0.0;
            for (double f = f0 * 0.7; f < f0 * 1.6; f *= 1.04) lo += goertzel (b, f, 48000.0, 0, 12000);
            for (double f = f0 * 2.5; f < 14000.0; f *= 1.12)  hi += goertzel (b, f, 48000.0, 0, 12000);
            return hi / std::max (1e-12, lo);
        };
        const double bs = bright (soft), bh = bright (hard);
        std::printf ("    %-6s upper/body at %.0f Hz: soft %.3f, hard %.3f\n", channelName (c), f0, bs, bh);
        ok (bh > bs * 1.02, (std::string (channelName (c)) + ": harder is brighter").c_str(), bh, bs * 1.02);
    }
}

//==============================================================================
static void groupSnareWires()
{
    std::printf ("\n-- 11. the buzz threshold ---------------------------------------\n");

    /*  A real snare's ghost notes stop rattling on their own, which is most of
        why they sound like ghost notes. The wire energy must fall FASTER than
        the level does. */
    auto wireShare = [] (float vel) -> double
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 2) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[2][CP_MODEL] = 1.0f;      // WIRES
        e.p.ch[2][CP_BEND]  = 0.35f;     // a middling rattle, so the gate can bite
        e.p.ch[2][CP_SNAP]  = 0.30f;
        e.trigger (2, vel);
        Buf b (24000); render (e, b);

        double hi = 0.0, lo = 0.0;
        for (double f = 120.0; f < 9000.0; f *= 1.2)
        {
            const double m = goertzel (b, f, 48000.0, 0, 16000);
            if (f > 1200.0) hi += m; else lo += m;
        }
        return hi / std::max (1e-12, lo);
    };

    const double soft = wireShare (0.18f), hard = wireShare (1.0f);
    std::printf ("    wire share: ghost note %.3f, full hit %.3f\n", soft, hard);
    ok (hard > soft * 1.15, "the wires rattle on a full hit and hush on a ghost note", hard, soft * 1.15);
}

//==============================================================================
static void groupExtremes()
{
    std::printf ("\n-- 12. extremes and random machines -----------------------------\n");

    // every parameter at both ends, one at a time, with everything firing
    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = paramSpec (i);
        for (float v : { 0.0f, paramMax (s) })
        {
            Engine e; fresh (e);
            pvalue (e.p, s) = v;
            if (s.slot == CP_MUTE && s.chan >= 0 && v > 0.5f) continue;
            for (int c = 0; c < NCH; ++c) e.trigger (c, 1.0f);
            Buf b (24000); render (e, b);
            ok (finiteAll (b), (std::string ("finite with ") + s.id + " at an extreme").c_str());
            ok (peak (b) <= 1.0f, (std::string ("bounded with ") + s.id + " at an extreme").c_str(), peak (b), 1.0);
        }
    }

    // 240 random machines, played hard
    Rng r; r.seed (0xBEEF);
    float worst = 0.0f;
    for (int t = 0; t < 240; ++t)
    {
        Engine e; fresh (e);
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s = paramSpec (i);
            if (s.slot == CP_MUTE && s.chan >= 0) continue;
            if (s.chan < 0 && s.slot == GP_OS) continue;
            pvalue (e.p, s) = s.kind == KP_LIST ? std::floor (r.uni() * (paramMax (s) + 0.999f)) : r.uni();
        }
        e.p.g[GP_VOLUME] = 0.72f;
        Buf b (48000);
        for (int k = 0; k < 16; ++k)
        {
            e.trigger ((int) (r.uni() * (NCH - 0.001f)), 0.4f + 0.6f * r.uni());
            e.process (b.L.data() + k * 3000, b.R.data() + k * 3000, 3000);
        }
        worst = std::max (worst, peak (b));
        if (! finiteAll (b)) ok (false, "random machine stays finite");
    }
    std::printf ("    worst peak over 240 random machines: %.4f\n", worst);
    ok (worst <= 1.0f, "240 random machines stay inside full scale", worst, 1.0);
}

//==============================================================================
static void groupRates()
{
    std::printf ("\n-- 13. sample rates and oversampling ----------------------------\n");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int os = 0; os <= 2; ++os)
        {
            Engine e; fresh (e, sr);
            e.p.g[GP_OS] = (float) os;
            for (int c = 0; c < NCH; ++c) e.trigger (c, 1.0f);
            Buf b ((int) sr);
            render (e, b);
            char tag[96];
            std::snprintf (tag, sizeof (tag), "%.0f Hz at %dx", sr, os == 0 ? 1 : (os == 1 ? 2 : 4));
            ok (finiteAll (b), (std::string ("finite at ") + tag).c_str());
            ok (peak (b) <= 1.0f, (std::string ("bounded at ") + tag).c_str(), peak (b), 1.0);
            ok (peak (b) > 0.05f, (std::string ("audible at ") + tag).c_str(), peak (b), 0.05);
        }

    // a tuned drum must be at the same pitch whatever the rate
    double f[3];
    int k = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Engine e; fresh (e, sr);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[0][CP_BEND] = 0.0f; e.p.ch[0][CP_DECAY] = 0.9f; e.p.ch[0][CP_TUNE] = 0.5f;
        e.trigger (0, 0.3f);
        Buf b ((int) sr); render (e, b);
        f[k++] = dominant (b, sr, (int) (sr * 0.25), (int) (sr * 0.85), 30.0, 200.0);
    }
    std::printf ("    BD1 at 44.1/48/96k: %.2f / %.2f / %.2f Hz\n", f[0], f[1], f[2]);
    ok (std::fabs (1200.0 * std::log2 (f[1] / f[0])) < 25.0, "same pitch at 44.1k and 48k");
    ok (std::fabs (1200.0 * std::log2 (f[2] / f[1])) < 25.0, "same pitch at 48k and 96k");
}

//==============================================================================
static void groupDeterminism()
{
    std::printf ("\n-- 14. determinism and AGE --------------------------------------\n");

    auto run = [] (float age) -> Buf
    {
        Engine e; fresh (e);
        e.p.g[GP_AGE] = age;
        Buf b (48000);
        for (int k = 0; k < 12; ++k)
        {
            e.trigger (k, 0.9f);
            e.process (b.L.data() + k * 4000, b.R.data() + k * 4000, 4000);
        }
        return b;
    };

    Buf a1 = run (0.0f), a2 = run (0.0f);
    ok (std::memcmp (a1.L.data(), a2.L.data(), a1.L.size() * sizeof (float)) == 0,
        "AGE at zero: two runs are bit-identical");

    Buf g1 = run (0.8f), g2 = run (0.8f);
    ok (std::memcmp (g1.L.data(), g2.L.data(), g1.L.size() * sizeof (float)) == 0,
        "AGE up: still deterministic within one instance");

    double diff = 0.0, ref = 0.0;
    for (size_t i = 0; i < a1.L.size(); ++i) { diff += std::fabs (a1.L[i] - g1.L[i]); ref += std::fabs (a1.L[i]); }
    std::printf ("    AGE 80%% moves the render by %.1f%%\n", diff / std::max (1e-9, ref) * 100.0);
    ok (diff / std::max (1e-9, ref) > 0.01, "AGE actually ages it", diff / std::max (1e-9, ref), 0.01);
    ok (finiteAll (g1), "AGE stays finite");
}

//==============================================================================
static void groupWorldMod()
{
    std::printf ("\n-- 15. the BWFX world-mod bus -----------------------------------\n");

    auto run = [] (bool set) -> Buf
    {
        Engine e; fresh (e);
        e.p.g[GP_AGE] = 0.0f;
        if (set) e.setWorldMod (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);   // the neutral bus
        for (int c = 0; c < NCH; ++c) e.trigger (c, 0.9f);
        Buf b (24000); render (e, b);
        return b;
    };
    Buf a = run (false), b = run (true);
    ok (std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0,
        "a neutral bus is memcmp-identical to no bus at all");

    Engine e; fresh (e);
    e.p.g[GP_AGE] = 0.0f;
    e.setWorldMod (35.0f, 0.6f, 0.5f, 3.0f, 0.4f, 1.4f);
    for (int c = 0; c < NCH; ++c) e.trigger (c, 0.9f);
    Buf w (48000); render (e, w);
    double diff = 0.0, ref = 0.0;
    for (size_t i = 0; i < a.L.size(); ++i) { diff += std::fabs (a.L[i] - w.L[i]); ref += std::fabs (a.L[i]); }
    std::printf ("    an armed character moves the render by %.1f%%\n", diff / std::max (1e-9, ref) * 100.0);
    ok (diff / std::max (1e-9, ref) > 0.02, "an armed character is audible", diff / std::max (1e-9, ref), 0.02);
    ok (finiteAll (w), "the world-mod bus stays finite");
    ok (peak (w) <= 1.0f, "and bounded", peak (w), 1.0);
}

//==============================================================================
static void groupCpu()
{
    std::printf ("\n-- 17. cost ------------------------------------------------------\n");
    for (int os = 0; os <= 2; ++os)
    {
        Engine e; fresh (e);
        e.p.g[GP_OS] = (float) os;
        const int N = 48000 * 4;
        Buf b (N);
        const clock_t t0 = clock();
        for (int i = 0; i < N; i += 256)
        {
            if ((i % 4800) == 0) for (int c = 0; c < NCH; ++c) e.trigger (c, 0.9f);
            e.process (b.L.data() + i, b.R.data() + i, std::min (256, N - i));
        }
        const double sec = (double) (clock() - t0) / CLOCKS_PER_SEC;
        std::printf ("    %dx: %.2f s of audio in %.3f s  =  %.1fx realtime  (~%.1f%% of a core)\n",
                     os == 0 ? 1 : (os == 1 ? 2 : 4), N / 48000.0, sec,
                     (N / 48000.0) / std::max (1e-6, sec), 100.0 * sec / (N / 48000.0));
        ok (sec < N / 48000.0, "faster than realtime with all twelve firing at 10 Hz", sec, N / 48000.0);
    }
}

//==============================================================================
/*  A spectrum dump, for when a pitch check disagrees with the code and one
    of the two is lying. Kept in the file: this family's benches have been
    wrong at least as often as the engines they measure. */
static void spectrumDump (int chan, float tune, int from, int to)
{
    Engine e; fresh (e);
    e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
    e.p.ch[chan][CP_TUNE] = tune;
    e.p.ch[chan][CP_BEND] = 0.0f;
    e.p.ch[chan][CP_DECAY] = 0.85f;
    e.p.ch[chan][CP_DRIVE] = 0.0f;
    e.trigger (chan, 0.35f);
    Buf b (48000 * 2); render (e, b);
    std::printf ("spectrum of %s tune %.2f, buffer peak %.4f\n", channelName (chan), tune, peak (b));
    for (int w = 0; w < 3; ++w)
    {
        const int f0 = w == 0 ? 500 : (w == 1 ? 4000 : from);
        const int f1 = w == 0 ? 6000 : (w == 1 ? 20000 : to);
        std::vector<std::pair<double, double>> pk;
        double prev = 0.0, prev2 = 0.0, prevF = 0.0;
        for (double f = 15.0; f < 600.0; f *= 1.01)
        {
            const double m = goertzel (b, f, 48000.0, f0, f1);
            if (prev > prev2 && prev > m && prevF > 0.0) pk.push_back ({ prev, prevF });
            prev2 = prev; prev = m; prevF = f;
        }
        std::sort (pk.begin(), pk.end(), [] (auto& a, auto& c) { return a.first > c.first; });
        std::printf ("  window %6d..%-6d rms %.3e   peaks:", f0, f1, rms (b, f0, f1));
        for (size_t i = 0; i < pk.size() && i < 6; ++i) std::printf ("  %.1f Hz (%.2e)", pk[i].second, pk[i].first);
        std::printf ("\n");
    }
}

//==============================================================================
/*  Onset detector: the sample at which the signal crosses a threshold having
    been quiet for a while. Used for every timing check below — measuring WHEN
    a drum happened is the only way to test a sequencer honestly. */
static std::vector<int> onsets (const Buf& b, float thresh = 0.02f, int minGap = 300)
{
    /*  An ENVELOPE, not the raw sample. The first version thresholded the
        instantaneous value, so a 44 Hz kick fell below the threshold twice per
        cycle and every cycle counted as a fresh hit — which made all seven
        sequencer checks fail against a sequencer that was already sample
        accurate. The release has to be long enough to bridge one cycle of the
        lowest drum in the test. */
    const float rel = std::exp (-1.0f / (0.020f * 48000.0f));
    std::vector<float> env ((size_t) b.n(), 0.0f);
    float e = 0.0f;
    for (int i = 0; i < b.n(); ++i)
    {
        const float a = std::max (std::fabs (b.L[(size_t) i]), std::fabs (b.R[(size_t) i]));
        e = a > e ? a : e * rel;
        env[(size_t) i] = e;
    }

    std::vector<int> out;
    int last = -minGap;
    for (int i = 1; i < b.n(); ++i)
    {
        if (env[(size_t) i] < thresh) continue;
        if (i - last < minGap) continue;
        //  a hit is a RISE, so compare against a moment ago rather than zero —
        //  that way a new strike is caught even while the last one still rings
        const int back = std::max (0, i - 64);
        if (env[(size_t) i] > env[(size_t) back] * 1.6f || (last < 0 && env[(size_t) i] > thresh))
        {
            out.push_back (i);
            last = i;
        }
    }
    return out;
}

static void seqOneChannel (Engine& e, int chan, int len, int div, bool everyStep)
{
    for (int p2 = 0; p2 < NPAT; ++p2)
        for (int c = 0; c < NCH; ++c)
            for (int k = 0; k < NSTEP; ++k) e.pat[p2].lane[c].step[k].on = 0;
    //  a short, bright channel, so two steps 125 ms apart are two events and
    //  not one ring with a bump in it
    e.p.ch[chan][CP_DECAY] = 0.06f;
    e.p.ch[chan][CP_REBOUND] = 0.0f;
    Lane& L = e.pat[0].lane[chan];
    L.len = (uint8_t) len; L.div = (uint8_t) div; L.dir = 0; L.swing = 0; L.mute = 0;
    for (int k = 0; k < len; ++k) L.step[k].on = everyStep ? 1 : (k == 0 ? 1 : 0);
    for (int c = 0; c < NCH; ++c) if (c != chan) e.p.ch[c][CP_MUTE] = 1.0f;
    e.p.g[GP_SEQ] = 1.0f;
    e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
}

//==============================================================================
static void groupSequencer()
{
    std::printf ("\n-- 18. the sequencer --------------------------------------------\n");

    //  a sixteenth at 120 BPM is 0.125 s — 6000 samples at 48 k
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.setTransport (120.0, 0.0, true);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        std::printf ("    16ths at 120 BPM: %d onsets, first at %d\n", (int) on.size(), on.empty() ? -1 : on[0]);
        ok (on.size() >= 14, "the sequencer plays every step", (double) on.size(), 14);
        double worst = 0.0;
        for (size_t i = 0; i + 1 < on.size(); ++i)
            worst = std::max (worst, std::fabs ((double) (on[i + 1] - on[i]) - 6000.0));
        std::printf ("    worst interval error: %.1f samples (%.3f ms)\n", worst, worst / 48.0);
        ok (worst <= 2.0, "every step lands within 2 samples of its beat", worst, 2.0);
        ok (on.empty() || on[0] <= 40, "the first step lands on the bar", on.empty() ? 999 : on[0], 40);
    }

    /*  THE HOST'S TEMPO IS USED EVEN WHEN THE TRANSPORT IS STOPPED. It used
        to fall back to the internal tempo knob the moment the DAW stopped
        rolling, which in a DAW is most of the time you are editing. */
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_TEMPO] = 0.0f;                    // internal clock at its slowest
        e.setTransport (90.0, -1.0, false);        // host has a tempo, is not rolling
        e.setFreeRun (true);                       // ...and the player pressed RUN
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        if (on.size() >= 3)
        {
            const double got = (double) (on[2] - on[1]);
            const double want = 48000.0 * 60.0 / 90.0 / 4.0;      // a 16th at 90 BPM
            std::printf ("    stopped host at 90 BPM: %.0f samples between steps (want %.0f)\n", got, want);
            ok (std::fabs (got - want) <= 3.0, "a stopped host still sets the tempo", got, want);
        }
        else ok (false, "the sequencer runs with the host stopped", (double) on.size(), 3);
    }

    /*  RUN must start it with NO host transport at all — no playhead, no
        tempo, nothing ever reported. That is the standalone, and it is also a
        DAW whose transport has not rolled since the plugin loaded. */
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_TEMPO] = 0.5f;                    // the internal clock
        e.setFreeRun (true);                       // which is what RUN does
        //  setTransport is deliberately never called
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        std::printf ("    no host at all: %d onsets on the internal clock\n", (int) on.size());
        ok (on.size() >= 6, "RUN starts the sequencer with no host transport", (double) on.size(), 6);
    }

    /*  BUGLIST 1: the DAW is in charge. A machine that is ARMED must still
        stop when the transport stops — arming is an intent, not a clock —
        and the free-run latch must be dropped with it, so it does not carry
        on by itself afterwards. */
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.setTransport (120.0, 0.0, true);           // the DAW rolls
        { Buf b (24000); render (e, b);
          ok (onsets (b).size() > 0, "a rolling DAW plays the armed machine"); }
        e.setTransport (120.0, 2.0, false);          // the DAW stops
        ok (! e.isFreeRunning(), "a stopping DAW did not drop the free-run latch");
        //  let the voice that was already sounding ring out, so we measure
        //  whether it keeps SEQUENCING rather than whether it is silent
        { Buf settle (16000); render (e, settle); }
        { Buf b (48000); render (e, b);
          const int n = (int) onsets (b).size();
          std::printf ("    after the DAW stops: %d onsets\n", n);
          ok (n == 0, "the machine ran on after the DAW stopped", (double) n, 0.0); }
    }

    /*  ...and a host that merely EXISTS is not a clock: armed, host present,
        never rolling, no latch -> silence until the player presses RUN. */
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.setTransport (120.0, -1.0, false, true);   // present, not rolling
        { Buf b (48000); render (e, b);
          ok (onsets (b).size() == 0, "armed alone started the machine"); }
        e.setFreeRun (true);                         // the player presses RUN
        { Buf b (48000); render (e, b);
          ok (onsets (b).size() > 0, "RUN did not start it without the DAW"); }
    }

    //  the sequencer must be inert when it is switched off
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_SEQ] = 0.0f;
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        ok (peak (b) == 0.0f, "switched off, the sequencer is silent", peak (b), 0.0);
    }

    //  swing delays the odd steps, and only the odd steps
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_SWING] = 0.80f;
        e.setTransport (120.0, 0.0, true);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        if (on.size() >= 5)
        {
            const double even = (double) (on[2] - on[0]);          // a full pair
            const double first = (double) (on[1] - on[0]);
            std::printf ("    swing 80%%: pair %.0f samples, first half %.0f (%.0f%%)\n",
                         even, first, 100.0 * first / even);
            ok (std::fabs (even - 12000.0) < 4.0, "swing does not change the pair length", even, 12000.0);
            ok (first > 6300.0, "the odd step is pushed late", first, 6300.0);
        }
        else ok (false, "swing test produced enough onsets", (double) on.size(), 5);
    }

    //  per-lane length is polymeter, for free
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 3, 1, true);        // a three-step lane
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        auto on = onsets (b);
        ok (on.size() >= 7, "a three-step lane keeps running", (double) on.size(), 7);
        double worst = 0.0;
        for (size_t i = 0; i + 1 < on.size(); ++i)
            worst = std::max (worst, std::fabs ((double) (on[i + 1] - on[i]) - 6000.0));
        ok (worst <= 2.0, "and still on the grid", worst, 2.0);
    }

    //  divisions
    {
        for (int div = 0; div <= 3; ++div)
        {
            Engine e; fresh (e);
            seqOneChannel (e, 8, 16, div, true);
            e.setTransport (120.0, 0.0, true);
            Buf b (48000 * 2); render (e, b);
            auto on = onsets (b);
            const double want = 6000.0 * (div == 0 ? 0.5 : div == 1 ? 1.0 : div == 2 ? 2.0 : 4.0);
            if (on.size() >= 3)
            {
                const double got = (double) (on[2] - on[1]);
                std::printf ("    div %d: %.0f samples between steps (want %.0f)\n", div, got, want);
                ok (std::fabs (got - want) <= 2.0, "division spacing is exact", got, want);
            }
            else ok (div == 3, "division produced onsets", (double) on.size(), 3);
        }
    }

    //  microtiming
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, false);       // step 0 only
        e.pat[0].lane[8].step[0].micro = 25;      // a quarter of a step late
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        auto on = onsets (b);
        ok (! on.empty(), "the micro-shifted step still plays");
        if (! on.empty())
        {
            std::printf ("    micro +25%%: fires at sample %d (want 1500)\n", on[0]);
            ok (std::fabs ((double) on[0] - 1500.0) <= 45.0, "microtiming shifts by a quarter step", on[0], 1500.0);
        }
    }

    //  probability at zero means never, at a hundred means always
    {
        for (int prob : { 0, 100 })
        {
            Engine e; fresh (e);
            seqOneChannel (e, 8, 16, 1, true);
            for (int k = 0; k < 16; ++k) e.pat[0].lane[8].step[k].prob = (uint8_t) prob;
            e.setTransport (120.0, 0.0, true);
            Buf b (48000); render (e, b);
            const size_t got = onsets (b).size();
            if (prob == 0) ok (got == 0, "probability 0 never plays", (double) got, 0);
            else           ok (got >= 7, "probability 100 always plays", (double) got, 7);
        }
    }

    //  a ratchet is more hits inside one step
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 3, false);       // one quarter-note step
        e.pat[0].lane[8].step[0].ratchet = 4;
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        auto on = onsets (b, 0.02f, 200);
        std::printf ("    ratchet 4: %d onsets in the step\n", (int) on.size());
        ok (on.size() >= 4, "a ratchet of four plays four times", (double) on.size(), 4);
    }
}

//==============================================================================
static void groupRebound()
{
    std::printf ("\n-- 19. REBOUND: a stick that bounces -----------------------------\n");

    for (float rb : { 0.0f, 0.25f, 0.6f, 1.0f })
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 7) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[7][CP_MODEL] = 1.0f;               // WOOD: short, so bounces separate
        e.p.ch[7][CP_DECAY] = 0.1f;
        e.p.ch[7][CP_REBOUND] = rb;
        e.trigger (7, 1.0f);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b, 0.02f, 120);
        std::printf ("    rebound %.2f -> %d hits\n", rb, (int) on.size());
        if (rb == 0.0f) ok (on.size() == 1, "no rebound is one hit", (double) on.size(), 1);
        else            ok (on.size() >= 2, "rebound bounces", (double) on.size(), 2);
        ok (on.size() <= 14, "and it is a bounce, not a runaway", (double) on.size(), 14);
        ok (finiteAll (b), "rebound stays finite");
        ok (peak (b) <= 1.0f, "rebound stays bounded", peak (b), 1.0);

        // the gaps must CLOSE UP, the way a dropped stick does
        if (on.size() >= 4)
        {
            const int g1 = on[1] - on[0], g2 = on[on.size() - 1] - on[on.size() - 2];
            std::printf ("      first gap %d samples, last gap %d\n", g1, g2);
            ok (g2 < g1, "the bounces get closer together", g2, g1);
        }
    }
}

//==============================================================================
static void groupKeyMode()
{
    std::printf ("\n-- 20. KEY MODE -------------------------------------------------\n");

    auto shot = [] (int note) -> double
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 0) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[0][CP_KEY] = 1.0f;
        e.p.ch[0][CP_MODEL] = 0.0f;
        e.p.ch[0][CP_BEND] = 0.0f;
        e.p.ch[0][CP_DECAY] = 0.85f;
        e.p.ch[0][CP_DRIVE] = 0.0f;
        e.noteOn (note, 0.35f);
        Buf b (48000 * 2); render (e, b);
        const double base = (double) xmap (0.28f, 30.0f, 120.0f);
        return dominant (b, 48000.0, 8000, 60000, base * 0.2, base * 5.0);
    };

    const double f36 = shot (36), f48 = shot (48), f24 = shot (24);
    std::printf ("    note 24 / 36 / 48 -> %.2f / %.2f / %.2f Hz\n", f24, f36, f48);
    ok (std::fabs (f48 / f36 - 2.0) < 0.06, "an octave up is a factor of two", f48 / f36, 2.0);
    ok (std::fabs (f36 / f24 - 2.0) < 0.06, "an octave down likewise", f36 / f24, 2.0);

    /*  KEY MODE must not steal notes that belong to other channels. With BD1
        chromatic, its range covers the snare and the hat — and they have to go
        on sounding, or turning KEY on silences most of the kit. */
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[0][CP_KEY] = 1.0f;                       // BD1 chromatic, root 36
        for (int c = 0; c < NCH; ++c) if (c != 2) e.p.ch[c][CP_MUTE] = 1.0f;   // hear SD1 only
        e.noteOn (38, 1.0f);                            // the snare's own note
        Buf b (24000); render (e, b);
        std::printf ("    with BD1 chromatic, note 38 still reaches SD 1: peak %.3f\n", peak (b));
        ok (peak (b) > 0.05f, "KEY MODE does not steal another channel's note", peak (b), 0.05);
    }
    {
        //  and a note NOT on the map still reaches the chromatic channel
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[0][CP_KEY] = 1.0f;
        for (int c = 0; c < NCH; ++c) if (c != 0) e.p.ch[c][CP_MUTE] = 1.0f;
        e.noteOn (31, 1.0f);                            // unmapped
        Buf b (24000); render (e, b);
        ok (peak (b) > 0.05f, "an unmapped note still plays the chromatic channel", peak (b), 0.05);
    }

    //  and a low note must ring LONGER, as a bigger drum does
    auto tail = [] (int note) -> double
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 0) e.p.ch[c][CP_MUTE] = 1.0f;
        e.p.ch[0][CP_KEY] = 1.0f; e.p.ch[0][CP_DECAY] = 0.6f; e.p.ch[0][CP_BEND] = 0.0f;
        e.noteOn (note, 1.0f);
        Buf b (48000 * 3); render (e, b);
        const float pk = peak (b);
        for (int i = 0; i < b.n(); i += 128)
            if (rms (b, i, std::min (b.n(), i + 2048)) < pk * 0.01) return i / 48000.0;
        return 3.0;
    };
    const double lo = tail (24), hi = tail (48);
    std::printf ("    decay: note 24 = %.3f s, note 48 = %.3f s\n", lo, hi);
    ok (lo > hi * 1.3, "a low note rings longer", lo, hi * 1.3);
}

//==============================================================================
static void groupMorph()
{
    std::printf ("\n-- 21. KIT MORPH ------------------------------------------------\n");

    Engine e; fresh (e);
    applySeed (7, e.kitA);   e.haveA = true;
    applySeed (140, e.kitB); e.haveB = true;

    auto renderAt = [&e] (float t) -> Buf
    {
        Engine x; fresh (x);
        x.kitA = e.kitA; x.kitB = e.kitB; x.haveA = true; x.haveB = true;
        x.p.g[GP_MORPH] = t;
        x.applyMorph (x.p);
        Buf b (48000);
        for (int k = 0; k < 12; ++k) { x.trigger (k, 0.9f); x.process (b.L.data() + k * 4000, b.R.data() + k * 4000, 4000); }
        return b;
    };

    //  the endpoints must be the kits themselves, exactly
    {
        Params a = e.kitA; a.g[GP_MORPH] = 0.0f;
        Params z = e.kitA; z.g[GP_MORPH] = 0.0f;
        e.applyMorph (z);
        bool same = true;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s2 = paramSpec (i);
            if (s2.chan < 0) continue;
            if (pvalue (a, s2) != pvalue (z, s2)) { same = false; break; }
        }
        ok (same, "morph at zero is kit A untouched");

        Params w = e.kitA; w.g[GP_MORPH] = 1.0f;
        e.applyMorph (w);
        bool isB = true;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s2 = paramSpec (i);
            if (! morphable (s2)) continue;
            //  stepped choices deliberately do not travel (buglist 2b), so
            //  the endpoint is kit B in everything CONTINUOUS
            if (s2.kind == KP_LIST || s2.kind == KP_SW) continue;
            if (std::fabs (pvalue (w, s2) - pvalue (e.kitB, s2)) > 1e-6f) { isB = false; break; }
        }
        ok (isB, "morph at one is kit B in every continuous value");
    }

    //  and the middle is genuinely between the two, not either of them
    {
        Buf A = renderAt (0.0f), M = renderAt (0.5f), B = renderAt (1.0f);
        auto diff = [] (const Buf& x, const Buf& y) {
            double d = 0.0; for (size_t i = 0; i < x.L.size(); ++i) d += std::fabs (x.L[i] - y.L[i]); return d;
        };
        const double dAB = diff (A, B), dAM = diff (A, M), dMB = diff (M, B);
        std::printf ("    A<->B %.0f, A<->mid %.0f, mid<->B %.0f\n", dAB, dAM, dMB);
        ok (dAM > dAB * 0.05, "the midpoint is not kit A", dAM, dAB * 0.05);
        ok (dMB > dAB * 0.05, "nor kit B", dMB, dAB * 0.05);
        ok (finiteAll (M), "the midpoint is finite");
        ok (peak (M) <= 1.0f, "and bounded", peak (M), 1.0);
    }

    /*  BUGLIST 2b: a morph never STEPS. Across the whole sweep every
        KP_LIST / KP_SW must sit exactly where it was last set — a fader you
        would automate over eight bars must not hide cliffs inside it. */
    {
        int stepped = 0, moved = 0;
        Params base = e.kitA;
        for (int q = 0; q <= 20; ++q)
        {
            Params w = base;
            w.g[GP_MORPH] = (float) q / 20.0f;
            e.applyMorph (w);
            for (int i = 0; i < numParams(); ++i)
            {
                const PSpec& s2 = paramSpec (i);
                if (s2.kind != KP_LIST && s2.kind != KP_SW) continue;
                if (q == 0) ++stepped;
                if (pvalue (w, s2) != pvalue (base, s2)) ++moved;
            }
        }
        std::printf ("    %d stepped parameters, %d moved across a 21-point sweep\n",
                     stepped, moved);
        ok (stepped > 0, "no stepped parameters found to check");
        ok (moved == 0, "a morph stepped something", (double) moved, 0.0);
    }

    /*  ...and the five globals the kit generator writes DO travel, or a
        morph moves the twelve voices and leaves the machine behind. */
    {
        Params w = e.kitA;
        w.g[GP_MORPH] = 0.5f;
        e.applyMorph (w);
        int movedG = 0;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s2 = paramSpec (i);
            if (s2.chan >= 0 || ! morphable (s2)) continue;
            if (std::fabs (pvalue (e.kitA, s2) - pvalue (e.kitB, s2)) < 1e-6f) continue;
            if (pvalue (w, s2) != pvalue (e.kitA, s2)) ++movedG;
        }
        ok (movedG > 0, "the kit globals do not morph");
    }

    //  with only one kit captured there is nothing to morph toward, and the
    //  fader must do NOTHING rather than sweep to silence
    {
        Engine u; fresh (u);
        applySeed (7, u.kitA); u.haveA = true; u.haveB = false;
        Params before = u.p; before.g[GP_MORPH] = 1.0f;
        Params after = before;
        u.applyMorph (after);
        bool untouched = true;
        for (int i = 0; i < numParams(); ++i)
            if (pvalue (before, paramSpec (i)) != pvalue (after, paramSpec (i))) { untouched = false; break; }
        ok (untouched, "one kit captured: the morph fader is inert");
    }
}

//==============================================================================
static void groupPunch()
{
    std::printf ("\n-- 26. PUNCH ----------------------------------------------------\n");

    auto hit = [] (int ch, float punch, int n) -> Buf
    {
        Engine e; fresh (e);
        e.p.g[GP_PUNCH] = punch;
        Buf b (n);
        e.trigger (ch, 0.9f);
        render (e, b);
        return b;
    };
    auto rms = [] (const Buf& b, int from, int to) -> double
    {
        double a = 0; int n = 0;
        for (int i = from; i < to && i < (int) b.L.size(); ++i) { a += (double) b.L[i] * b.L[i]; ++n; }
        return n ? std::sqrt (a / n) : 0.0;
    };

    /*  EXACTLY ABSENT AT ZERO. The same contract RAIL SAG, BLEED, AGE and
        the BWFX macros keep: g is an IEEE-exact 1.0f, so the machine is
        bit-identical to one whose PUNCH was never touched. */
    {
        Engine a; fresh (a);
        Engine b; fresh (b); b.p.g[GP_PUNCH] = 0.0f;
        Buf x (24000), y (24000);
        a.trigger (0, 0.9f); render (a, x);
        b.trigger (0, 0.9f); render (b, y);
        ok (std::memcmp (x.L.data(), y.L.data(), x.L.size() * sizeof (float)) == 0,
            "PUNCH at zero is not bit-identical");
    }

    //  the strike lifts and the body leans: that is what slam is
    {
        Buf off = hit (0, 0.0f, 24000), on = hit (0, 1.0f, 24000);
        const double aOff = rms (off, 0, 300), aOn = rms (on, 0, 300);
        const double bOff = rms (off, 4000, 20000), bOn = rms (on, 4000, 20000);
        const double dAtt = 20.0 * std::log10 ((aOn + 1e-12) / (aOff + 1e-12));
        const double dSus = 20.0 * std::log10 ((bOn + 1e-12) / (bOff + 1e-12));
        std::printf ("    kick: strike %+.2f dB, body %+.2f dB\n", dAtt, dSus);
        ok (dAtt > 1.5, "PUNCH does not lift the strike", dAtt, 1.5);
        ok (dSus < -0.5, "PUNCH does not lean on the body", dSus, -0.5);
    }

    /*  ...but a cymbal ringing for seconds must keep its decay. The sustain
        half is nearly off on the metal, or the control reads as a broken
        release rather than as punch. */
    {
        Buf off = hit (10, 0.0f, 96000), on = hit (10, 1.0f, 96000);
        const double tOff = rms (off, 40000, 90000), tOn = rms (on, 40000, 90000);
        const double d = 20.0 * std::log10 ((tOn + 1e-12) / (tOff + 1e-12));
        std::printf ("    cymbal tail: %+.2f dB\n", d);
        ok (d > -1.5, "PUNCH collapses the cymbal decay", d, -1.5);
    }

    /*  PUNCH and RAIL SAG are opposite gestures acting on the same first
        milliseconds — the spec says to measure the interaction rather than
        assume it. Together they must stay bounded and must not cancel. */
    {
        Engine e; fresh (e);
        e.p.g[GP_PUNCH] = 1.0f;
        e.p.g[GP_SAG]   = 1.0f;
        Buf b (48000);
        for (int k = 0; k < 12; ++k) e.trigger (k, 1.0f);
        render (e, b);
        std::printf ("    punch + full rail sag, whole kit: peak %.3f\n", (double) peak (b));
        ok (finiteAll (b), "punch with rail sag went non-finite");
        ok (peak (b) <= 1.0f, "punch with rail sag broke the ceiling", peak (b), 1.0);
        Buf q = hit (0, 0.0f, 48000);
        ok (std::fabs ((double) peak (b) - (double) peak (q)) > 1e-4,
            "punch and rail sag cancelled each other exactly");
    }

    //  and into the ceilings, at the loudest thing the machine can do
    {
        Engine e; fresh (e);
        e.p.g[GP_PUNCH]  = 1.0f;
        e.p.g[GP_VOLUME] = 1.0f;
        Buf b (48000);
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 12; ++k) e.trigger (k, 1.0f);
        render (e, b);
        std::printf ("    punch into the ceilings: peak %.3f\n", (double) peak (b));
        ok (finiteAll (b), "punch into the ceilings went non-finite");
        ok (peak (b) <= 1.0f, "punch into the ceilings clipped", peak (b), 1.0);
    }
}

//==============================================================================
static void groupSeeds()
{
    std::printf ("\n-- 22. the two hundred kits -------------------------------------\n");

    ok (numSeeds() == 200, "two hundred kits", numSeeds(), 200);

    //  every category is represented, and its count is what the table says
    std::vector<int> perCat ((size_t) numSeedCategories(), 0);
    for (int s2 = 0; s2 < numSeeds(); ++s2) ++perCat[(size_t) seedCategory (s2)];
    for (int c = 0; c < numSeedCategories(); ++c)
    {
        std::printf ("    %-16s %3d kits\n", seedCategoryName (c), perCat[(size_t) c]);
        ok (perCat[(size_t) c] > 0, (std::string ("category populated: ") + seedCategoryName (c)).c_str());
    }

    //  names must all differ — a library with two kits of the same name is a
    //  library you cannot talk about
    {
        std::vector<std::string> names;
        for (int s2 = 0; s2 < numSeeds(); ++s2) names.push_back (seedName (s2));
        std::vector<std::string> sorted = names;
        std::sort (sorted.begin(), sorted.end());
        int dup = 0;
        for (size_t i = 1; i < sorted.size(); ++i) if (sorted[i] == sorted[i - 1]) ++dup;
        std::printf ("    %d duplicate names\n", dup);
        ok (dup == 0, "all two hundred names are distinct", dup, 0);
    }

    //  and every one is bounded, audible, and not the same render as another
    std::vector<unsigned long long> sig;
    float worst = 0.0f, quietest = 1.0f;
    for (int s2 = 0; s2 < numSeeds(); ++s2)
    {
        Engine e;
        applySeed (s2, e.p);
        e.prepare (48000.0, 256);
        e.reset();
        Buf b (26000);
        for (int k = 0; k < 12; ++k) { e.trigger (k, 0.92f); e.process (b.L.data() + k * 2000, b.R.data() + k * 2000, 2000); }
        e.process (b.L.data() + 24000, b.R.data() + 24000, 2000);

        const float pk = peak (b);
        worst = std::max (worst, pk);
        quietest = std::min (quietest, pk);
        if (! finiteAll (b)) ok (false, (std::string ("finite: ") + seedName (s2)).c_str());
        if (pk > 1.0f)  ok (false, (std::string ("bounded: ") + seedName (s2)).c_str(), pk, 1.0);
        if (pk < 0.05f) ok (false, (std::string ("audible: ") + seedName (s2)).c_str(), pk, 0.05);

        unsigned long long h = 1469598103934665603ull;
        for (int i = 0; i < b.n(); i += 37)
        {
            const unsigned int q = (unsigned int) (b.L[(size_t) i] * 32768.0f);
            h = (h ^ q) * 1099511628211ull;
        }
        sig.push_back (h);
    }
    std::printf ("    peaks across the library: %.3f quietest, %.3f loudest\n", quietest, worst);
    ok (worst <= 1.0f, "every kit stays inside full scale", worst, 1.0);
    ok (quietest > 0.05f, "no kit is inaudible", quietest, 0.05);

    std::vector<unsigned long long> ss = sig;
    std::sort (ss.begin(), ss.end());
    int same = 0;
    for (size_t i = 1; i < ss.size(); ++i) if (ss[i] == ss[i - 1]) ++same;
    std::printf ("    %d kits render identically to another\n", same);
    ok (same == 0, "every kit sounds different", same, 0);

    /*  No kit may dial in more than a flam. REBOUND is a performance garnish;
        a generator reaching for a five-bounce roll turns a hat lane into a
        blast beat, and before it had a knob there was no way to even see it. */
    {
        float worstRb = 0.0f; int withAny = 0, n = 0;
        for (int s2 = 0; s2 < numSeeds(); ++s2)
        {
            Params q; applySeed (s2, q);
            bool any = false;
            for (int c = 0; c < NCH; ++c)
            {
                worstRb = std::max (worstRb, q.ch[c][CP_REBOUND]);
                if (q.ch[c][CP_REBOUND] > 0.0f) { any = true; ++n; }
            }
            if (any) ++withAny;
        }
        std::printf ("    rebound: worst %.2f, on %d of %d channels across %d kits\n",
                     worstRb, n, numSeeds() * NCH, withAny);
        ok (worstRb <= 0.20f, "no kit dials in more than a flam", worstRb, 0.20);
        ok (n < numSeeds() * NCH / 12, "and it stays a garnish", n, numSeeds() * NCH / 12.0);
    }

    //  a kit is reproducible from its integer, which is the whole point
    {
        Params a, b2;
        applySeed (137, a); applySeed (137, b2);
        ok (std::memcmp (&a, &b2, sizeof (Params)) == 0, "the same seed is the same kit");
    }
}

//==============================================================================
static void groupKitTune()
{
    std::printf ("\n-- 23. KIT TUNE -------------------------------------------------\n");

    //  measure the sounding pitch of one channel at three KIT TUNE settings
    auto pitchOf = [] (int c, float kt) -> double
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
        e.p.g[GP_KITTUNE] = kt;
        for (int k = 0; k < NCH; ++k) if (k != c) e.p.ch[k][CP_MUTE] = 1.0f;
        e.p.ch[c][CP_BEND] = 0.0f;
        e.p.ch[c][CP_DECAY] = 0.85f;
        e.p.ch[c][CP_DRIVE] = 0.0f;
        e.trigger (c, 0.35f);
        Buf b (48000 * 2); render (e, b);
        const int i = paramIndex ((std::string (channelId (c)) + "_tune").c_str());
        const double base = (double) xmap (paramSpec (i).def, paramSpec (i).lo, paramSpec (i).hi);

        /*  A hat has no fundamental worth finding — its pitch is a bandpassed
            cluster spread over several kilohertz — so for the metal channels
            the honest measure is where the weight of the sound sits. */
        if (channelFamily (c) == FAM_METAL)
        {
            double num = 0.0, den = 0.0;
            for (double f = 200.0; f < 16000.0; f *= 1.06)
            {
                const double m = goertzel (b, f, 48000.0, 2000, 20000);
                num += m * f; den += m;
            }
            return num / std::max (1e-12, den);
        }
        return dominant (b, 48000.0, 4000, 40000, base * 0.35, base * 5.0);
    };

    for (int c : { 0, 4, 6, 8 })
    {
        const double lo = pitchOf (c, 0.0f), mid = pitchOf (c, 0.5f), hi = pitchOf (c, 1.0f);
        const double semis = 12.0 * std::log2 (std::max (1e-9, hi / std::max (1e-9, lo)));
        std::printf ("    %-6s  KIT TUNE 0%% = %7.2f Hz   50%% = %7.2f   100%% = %7.2f   span %+.1f semitones%s\n",
                     channelName (c), lo, mid, hi, semis,
                     channelFamily (c) == FAM_METAL ? "   (spectral centroid)" : "");
        /*  A transpose, so every channel moves. The PITCHED channels move by
            the full two octaves. A metal channel measured by centroid moves
            less, and that is real rather than a defect: transposing a hat up
            pushes the top of its spectrum past Nyquist, where the anti-alias
            filter genuinely removes it. Asserting 24 there would be asserting
            something that cannot physically happen. */
        if (channelFamily (c) == FAM_METAL)
        {
            ok (std::fabs (semis) > 8.0,
                (std::string ("KIT TUNE transposes ") + channelName (c)).c_str(), semis, 8.0);
        }
        else
        {
            ok (std::fabs (semis) > 20.0,
                (std::string ("KIT TUNE transposes ") + channelName (c)).c_str(), semis, 20.0);
            ok (std::fabs (semis - 24.0) < 2.0,
                (std::string ("...by the full two octaves: ") + channelName (c)).c_str(), semis, 24.0);
        }
    }
}

int main (int argc, char** argv)
{
    if (argc > 1 && std::strcmp (argv[1], "--spec") == 0)
    {
        spectrumDump (0, 0.45f, 12000, 40000);
        return 0;
    }
    if (argc > 1 && std::strcmp (argv[1], "--vel") == 0)
    {
        std::printf ("peak against velocity (the nonlinear damping is meant to squash, but not this much)\n");
        for (int c : { 0, 2, 4, 8, 11 })
        {
            std::printf ("  %-6s", channelName (c));
            float first = 0.0f;
            for (float v : { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                Engine e; fresh (e);
                e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
                for (int k = 0; k < NCH; ++k) if (k != c) e.p.ch[k][CP_MUTE] = 1.0f;
                e.trigger (c, v);
                Buf b (48000); render (e, b);
                if (first == 0.0f) first = peak (b);
                std::printf ("   v%.2f %.4f (%.2fx)", v, peak (b), peak (b) / std::max (1e-6f, first));
            }
            std::printf ("\n");
        }
        return 0;
    }
    std::printf ("================================================================\n");
    std::printf ("  FULL METAL RACKET  --  offline bench\n");
    std::printf ("================================================================\n");

    groupTable();
    groupSilence();
    groupVoices();
    groupTuning();
    groupTension();
    groupDecayIsQ();
    groupWeb();
    groupRailAndBody();
    groupChokeAndLink();
    groupVelocity();
    groupSnareWires();
    groupExtremes();
    groupRates();
    groupDeterminism();
    groupWorldMod();
    groupSequencer();
    groupRebound();
    groupKeyMode();
    groupMorph();
    groupPunch();
    groupSeeds();
    groupKitTune();
    groupCpu();

    std::printf ("\n================================================================\n");
    if (fails == 0) std::printf ("  ALL CLEAR  --  %d checks\n", checks);
    else
    {
        std::printf ("  %d of %d checks FAILED\n", fails, checks);
        for (const auto& m : failMsgs) std::printf ("    %s\n", m.c_str());
    }
    std::printf ("================================================================\n");
    return fails == 0 ? 0 : 1;
}
