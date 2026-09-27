// Bench coverage for chunks A–C: sequencer timing, rebound, key mode, morph,
// and the two hundred kits. The timing checks are the point — a sequencer that
// is bounded and audible but a few milliseconds adrift is worse than useless.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(`int main (int argc, char** argv)`,
`//==============================================================================
/*  Onset detector: the sample at which the signal crosses a threshold having
    been quiet for a while. Used for every timing check below — measuring WHEN
    a drum happened is the only way to test a sequencer honestly. */
static std::vector<int> onsets (const Buf& b, float thresh = 0.02f, int minGap = 300)
{
    std::vector<int> out;
    int quiet = minGap;
    for (int i = 0; i < b.n(); ++i)
    {
        const float a = std::fabs (b.L[(size_t) i]) + std::fabs (b.R[(size_t) i]);
        if (a > thresh) { if (quiet >= minGap) out.push_back (i); quiet = 0; }
        else ++quiet;
    }
    return out;
}

static void seqOneChannel (Engine& e, int chan, int len, int div, bool everyStep)
{
    for (int p2 = 0; p2 < NPAT; ++p2)
        for (int c = 0; c < NCH; ++c)
            for (int k = 0; k < NSTEP; ++k) e.pat[p2].lane[c].step[k].on = 0;
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
    std::printf ("\\n-- 18. the sequencer --------------------------------------------\\n");

    //  a sixteenth at 120 BPM is 0.125 s — 6000 samples at 48 k
    {
        Engine e; fresh (e);
        seqOneChannel (e, 0, 16, 1, true);
        e.setTransport (120.0, 0.0, true);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        std::printf ("    16ths at 120 BPM: %d onsets, first at %d\\n", (int) on.size(), on.empty() ? -1 : on[0]);
        ok (on.size() >= 14, "the sequencer plays every step", (double) on.size(), 14);
        double worst = 0.0;
        for (size_t i = 0; i + 1 < on.size(); ++i)
            worst = std::max (worst, std::fabs ((double) (on[i + 1] - on[i]) - 6000.0));
        std::printf ("    worst interval error: %.1f samples (%.3f ms)\\n", worst, worst / 48.0);
        ok (worst <= 2.0, "every step lands within 2 samples of its beat", worst, 2.0);
        ok (on.empty() || on[0] <= 2, "the first step lands on the bar", on.empty() ? 999 : on[0], 2);
    }

    //  the sequencer must be inert when it is switched off
    {
        Engine e; fresh (e);
        seqOneChannel (e, 0, 16, 1, true);
        e.p.g[GP_SEQ] = 0.0f;
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        ok (peak (b) == 0.0f, "switched off, the sequencer is silent", peak (b), 0.0);
    }

    //  swing delays the odd steps, and only the odd steps
    {
        Engine e; fresh (e);
        seqOneChannel (e, 0, 16, 1, true);
        e.p.g[GP_SWING] = 0.80f;
        e.setTransport (120.0, 0.0, true);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        if (on.size() >= 5)
        {
            const double even = (double) (on[2] - on[0]);          // a full pair
            const double first = (double) (on[1] - on[0]);
            std::printf ("    swing 80%%: pair %.0f samples, first half %.0f (%.0f%%)\\n",
                         even, first, 100.0 * first / even);
            ok (std::fabs (even - 12000.0) < 4.0, "swing does not change the pair length", even, 12000.0);
            ok (first > 6300.0, "the odd step is pushed late", first, 6300.0);
        }
        else ok (false, "swing test produced enough onsets", (double) on.size(), 5);
    }

    //  per-lane length is polymeter, for free
    {
        Engine e; fresh (e);
        seqOneChannel (e, 0, 3, 1, true);        // a three-step lane
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
            seqOneChannel (e, 0, 16, div, true);
            e.setTransport (120.0, 0.0, true);
            Buf b (48000 * 2); render (e, b);
            auto on = onsets (b);
            const double want = 6000.0 * (div == 0 ? 0.5 : div == 1 ? 1.0 : div == 2 ? 2.0 : 4.0);
            if (on.size() >= 3)
            {
                const double got = (double) (on[2] - on[1]);
                std::printf ("    div %d: %.0f samples between steps (want %.0f)\\n", div, got, want);
                ok (std::fabs (got - want) <= 2.0, "division spacing is exact", got, want);
            }
            else ok (div == 3, "division produced onsets", (double) on.size(), 3);
        }
    }

    //  microtiming
    {
        Engine e; fresh (e);
        seqOneChannel (e, 0, 16, 1, false);       // step 0 only
        e.pat[0].lane[0].step[0].micro = 25;      // a quarter of a step late
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        auto on = onsets (b);
        ok (! on.empty(), "the micro-shifted step still plays");
        if (! on.empty())
        {
            std::printf ("    micro +25%%: fires at sample %d (want 1500)\\n", on[0]);
            ok (std::fabs ((double) on[0] - 1500.0) <= 3.0, "microtiming shifts by a quarter step", on[0], 1500.0);
        }
    }

    //  probability at zero means never, at a hundred means always
    {
        for (int prob : { 0, 100 })
        {
            Engine e; fresh (e);
            seqOneChannel (e, 0, 16, 1, true);
            for (int k = 0; k < 16; ++k) e.pat[0].lane[0].step[k].prob = (uint8_t) prob;
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
        seqOneChannel (e, 0, 16, 3, false);       // one quarter-note step
        e.pat[0].lane[0].step[0].ratchet = 4;
        e.setTransport (120.0, 0.0, true);
        Buf b (48000); render (e, b);
        auto on = onsets (b, 0.02f, 200);
        std::printf ("    ratchet 4: %d onsets in the step\\n", (int) on.size());
        ok (on.size() >= 4, "a ratchet of four plays four times", (double) on.size(), 4);
    }
}

//==============================================================================
static void groupRebound()
{
    std::printf ("\\n-- 19. REBOUND: a stick that bounces -----------------------------\\n");

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
        std::printf ("    rebound %.2f -> %d hits\\n", rb, (int) on.size());
        if (rb == 0.0f) ok (on.size() == 1, "no rebound is one hit", (double) on.size(), 1);
        else            ok (on.size() >= 2, "rebound bounces", (double) on.size(), 2);
        ok (finiteAll (b), "rebound stays finite");
        ok (peak (b) <= 1.0f, "rebound stays bounded", peak (b), 1.0);

        // the gaps must CLOSE UP, the way a dropped stick does
        if (on.size() >= 4)
        {
            const int g1 = on[1] - on[0], g2 = on[on.size() - 1] - on[on.size() - 2];
            std::printf ("      first gap %d samples, last gap %d\\n", g1, g2);
            ok (g2 < g1, "the bounces get closer together", g2, g1);
        }
    }
}

//==============================================================================
static void groupKeyMode()
{
    std::printf ("\\n-- 20. KEY MODE -------------------------------------------------\\n");

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
    std::printf ("    note 24 / 36 / 48 -> %.2f / %.2f / %.2f Hz\\n", f24, f36, f48);
    ok (std::fabs (f48 / f36 - 2.0) < 0.06, "an octave up is a factor of two", f48 / f36, 2.0);
    ok (std::fabs (f36 / f24 - 2.0) < 0.06, "an octave down likewise", f36 / f24, 2.0);

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
    std::printf ("    decay: note 24 = %.3f s, note 48 = %.3f s\\n", lo, hi);
    ok (lo > hi * 1.3, "a low note rings longer", lo, hi * 1.3);
}

//==============================================================================
static void groupMorph()
{
    std::printf ("\\n-- 21. KIT MORPH ------------------------------------------------\\n");

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
            if (s2.chan < 0 || s2.slot == CP_MUTE) continue;
            if (std::fabs (pvalue (w, s2) - pvalue (e.kitB, s2)) > 1e-6f) { isB = false; break; }
        }
        ok (isB, "morph at one is kit B exactly");
    }

    //  and the middle is genuinely between the two, not either of them
    {
        Buf A = renderAt (0.0f), M = renderAt (0.5f), B = renderAt (1.0f);
        auto diff = [] (const Buf& x, const Buf& y) {
            double d = 0.0; for (size_t i = 0; i < x.L.size(); ++i) d += std::fabs (x.L[i] - y.L[i]); return d;
        };
        const double dAB = diff (A, B), dAM = diff (A, M), dMB = diff (M, B);
        std::printf ("    A<->B %.0f, A<->mid %.0f, mid<->B %.0f\\n", dAB, dAM, dMB);
        ok (dAM > dAB * 0.05, "the midpoint is not kit A", dAM, dAB * 0.05);
        ok (dMB > dAB * 0.05, "nor kit B", dMB, dAB * 0.05);
        ok (finiteAll (M), "the midpoint is finite");
        ok (peak (M) <= 1.0f, "and bounded", peak (M), 1.0);
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
static void groupSeeds()
{
    std::printf ("\\n-- 22. the two hundred kits -------------------------------------\\n");

    ok (numSeeds() == 200, "two hundred kits", numSeeds(), 200);

    //  every category is represented, and its count is what the table says
    std::vector<int> perCat ((size_t) numSeedCategories(), 0);
    for (int s2 = 0; s2 < numSeeds(); ++s2) ++perCat[(size_t) seedCategory (s2)];
    for (int c = 0; c < numSeedCategories(); ++c)
    {
        std::printf ("    %-16s %3d kits\\n", seedCategoryName (c), perCat[(size_t) c]);
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
        std::printf ("    %d duplicate names\\n", dup);
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
    std::printf ("    peaks across the library: %.3f quietest, %.3f loudest\\n", quietest, worst);
    ok (worst <= 1.0f, "every kit stays inside full scale", worst, 1.0);
    ok (quietest > 0.05f, "no kit is inaudible", quietest, 0.05);

    std::vector<unsigned long long> ss = sig;
    std::sort (ss.begin(), ss.end());
    int same = 0;
    for (size_t i = 1; i < ss.size(); ++i) if (ss[i] == ss[i - 1]) ++same;
    std::printf ("    %d kits render identically to another\\n", same);
    ok (same == 0, "every kit sounds different", same, 0);

    //  a kit is reproducible from its integer, which is the whole point
    {
        Params a, b2;
        applySeed (137, a); applySeed (137, b2);
        ok (std::memcmp (&a, &b2, sizeof (Params)) == 0, "the same seed is the same kit");
    }
}

int main (int argc, char** argv)`, "new groups");

rep(`    groupKits();
    groupCpu();`,
`    groupSequencer();
    groupRebound();
    groupKeyMode();
    groupMorph();
    groupSeeds();
    groupCpu();`, "call new groups");

// the old kit group walked nine handmade kits; the library replaces it
const a = s.indexOf("//==============================================================================\nstatic void groupKits()");
const b = s.indexOf("//==============================================================================\nstatic void groupCpu()");
if (a < 0 || b < 0 || b <= a) miss.push("groupKits block not found");
else s = s.slice(0, a) + s.slice(b);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench extended — sequencer, rebound, key mode, morph, 200 kits");
