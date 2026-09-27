#include <chrono>
/*  GRAVITY WELL - the probe.

    The bench asserts; this one PRINTS.  It is a separate target so that a
    failing bench never blocks a look inside, and so that a diagnosis can
    bisect by turning one thing off at a time instead of by reading code.

        gwprobe onset     the note-on click: where it starts and how loud
        gwprobe presets   onset cleanliness and loudness, preset by preset
        gwprobe 303       the acid voice, measured against what a 303 does
*/
#include "Engine.h"
#include "loudness.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace gw;

struct Take { std::vector<float> L, R; };

static Take render (Engine& e, double sr, float seconds, int note, float vel,
                    float holdFrac = 0.8f)
{
    const int n = (int) (sr * seconds), block = 64;
    Take t; t.L.assign ((size_t) n, 0.f); t.R.assign ((size_t) n, 0.f);
    e.noteOn (note, vel);
    const int off = (int) (n * holdFrac);
    for (int i = 0; i < n; i += block) {
        const int m = std::min (block, n - i);
        if (i <= off && i + m > off) e.noteOff (note);
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], m);
    }
    return t;
}

/*  A CLICK IS BROADBAND (the Thin Walls rule).  On a 41 Hz bass note almost
    nothing above 6 kHz is legitimate, so energy in that band at the onset IS
    the click.  A max-sample-step metric would measure the note's own slope
    instead and says nothing at all.                                         */
static double hf (const std::vector<float>& v, double sr, int from, int to)
{
    const double a = std::exp (-2.0 * M_PI * 6000.0 / sr);
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0, s = 0; int n = 0;
    for (int i = 0; i < (int) v.size() && i < to; ++i) {
        const double x = v[(size_t) i];
        const double y = a * (y1 + x - x1); x1 = x; y1 = y;
        const double z = a * (y2 + y - x2); x2 = y; y2 = z;
        if (i >= from) { s += z * z; ++n; }
    }
    return n ? std::sqrt (s / n) : 0.0;
}

static double pk (const std::vector<float>& v, int from, int to) {
    double m = 0; for (int i = std::max (0, from); i < std::min ((int) v.size(), to); ++i)
        m = std::max (m, (double) std::fabs (v[(size_t) i])); return m;
}
static double rmsOf (const std::vector<float>& v, int from, int to) {
    double s = 0; int n = 0;
    for (int i = std::max (0, from); i < std::min ((int) v.size(), to); ++i) { s += (double) v[(size_t) i] * v[(size_t) i]; ++n; }
    return n ? std::sqrt (s / n) : 0.0;
}

/*  The click number.  Onset HF against the SAME patch's settled HF: a patch
    that is legitimately bright has a high settled HF too, so the ratio asks
    "is there more top at the edge than this sound actually has", which is
    what a click is.  An absolute threshold would flag every bright preset. */
static double clickRatio (const Take& t, double sr)
{
    //  Normalised by the LOCAL level: at the onset the envelope is still
    //  ramping, so absolute HF is small however sharp the edge.  What a click
    //  really is, is a SPECTRAL TILT - far more top than this sound carries -
    //  so each window is divided by its own rms before they are compared.
    const int oA = 0, oB = (int) (sr * 0.006);
    //  THE SETTLED WINDOW MUST STILL HAVE THE NOTE IN IT.  At 150-450 ms a
    //  short pluck has already decayed, so the ratio divides by near-silence
    //  and reports a click that is only the probe running out of signal:
    //  TECHNO PLUCK read 236 and SHORT FALL 668 that way.  25-120 ms is
    //  inside every preset in the bank.
    const int sA = (int) (sr * 0.025), sB = (int) (sr * 0.120);
    const double on = hf (t.L, sr, oA, oB) / std::max (rmsOf (t.L, oA, oB), 1e-9);
    const double ss = hf (t.L, sr, sA, sB) / std::max (rmsOf (t.L, sA, sB), 1e-9);
    return on / std::max (ss, 1e-9);
}

static void onset (double sr)
{
    std::printf ("\nTHE NOTE-ON CLICK: WHERE DOES IT START?  (E1 = 41.2 Hz)\n\n"
                 "  A bass note has no business above 6 kHz.  'click' is the HF\n"
                 "  in the first 6 ms over the same patch's own settled HF, so a\n"
                 "  legitimately bright patch does not read as a click.\n\n");

    std::printf ("  AMP ATTACK sweep (the floor in Engine.h is what stops the low end)\n");
    const float att[] = { 0.f, 0.02f, 0.05f, 0.08f, 0.11f, 0.14f, 0.17f, 0.20f, 0.25f, 0.30f, 0.40f };
    for (float a : att) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_e1_a] = a;
        Take t = render (e, sr, 0.6f, 28, 0.9f);
        //  the ATTACK ACTUALLY ASKED FOR, before the floor: 0.0005 * 12000^a
        const double asked = 0.0005 * std::pow (12000.0, (double) a);
        std::printf ("    e1_a %.2f -> %7.2f ms asked   click %8.2f   onset peak %.4f\n",
                     a, asked * 1000.0, clickRatio (t, sr), pk (t.L, 0, (int) (sr * 0.006)));
    }

    std::printf ("\n  RESONANCE sweep.  If the click is the filter being STRUCK by the\n"
                 "  oscillator switching on at full level - the VCA ramps AFTER the\n"
                 "  filter - then it must grow with Q, at any attack setting.\n");
    for (float r = 0.f; r <= 0.97f; r += 0.12f) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_fa_res] = r; e.p.v[P_fa_cut] = 0.45f;
        e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f;
        Take t = render (e, sr, 0.6f, 28, 0.9f);
        std::printf ("    A RESONANCE %.2f   click %8.2f\n", r, clickRatio (t, sr));
    }

    std::printf ("\n  the same for ENV2 (it sweeps the filter, and a filter edge clicks too)\n");
    for (float a : att) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_e2_a] = a; e.p.v[P_fa_env] = 0.8f;
        Take t = render (e, sr, 0.6f, 28, 0.9f);
        const double asked = 0.0005 * std::pow (12000.0, (double) a);
        std::printf ("    e2_a %.2f -> %7.2f ms asked   click %8.2f\n", a, asked * 1000.0, clickRatio (t, sr));
    }
}

static void presets (double sr)
{
    std::printf ("\nEVERY FACTORY PRESET: does it click, and how loud is it?\n\n");
    std::printf ("  %-3s %-18s %-14s %8s %10s %10s\n", "#", "name", "group", "click", "peak", "rms");
    double loudest = 0, quietest = 1e9; int li = 0, qi = 0;
    for (int i = 0; i < numFactory(); ++i) {
        Engine e; e.prepare (sr, 64);
        applyFactory (i, e.p);
        e.p.v[P_out_trim] = 0.f;
        Take t = render (e, sr, 2.0f, 28, 0.9f);
        const double c = clickRatio (t, sr);
        const double p = pk (t.L, 0, (int) t.L.size());
        const double r = rmsOf (t.L, (int) (sr * 0.05), (int) (sr * 1.4));
        if (r > loudest)  { loudest = r;  li = i; }
        if (r < quietest) { quietest = r; qi = i; }
        std::printf ("  %-3d %-18s %-14s %8.2f %10.4f %10.5f%s\n",
                     i, factoryName (i), factoryGroup (i), c, p, r, c > 3.0 ? "   <-- CLICKS" : "");
    }
    std::printf ("\n  loudest  %-18s rms %.5f\n  quietest %-18s rms %.5f\n  spread %.1f dB\n",
                 factoryName (li), loudest, factoryName (qi), quietest,
                 20.0 * std::log10 (loudest / std::max (quietest, 1e-9)));
}

/*  The 303.  What makes one credible is not the oscillator; it is that the
    resonance does NOT thin the bass, that the filter envelope is decay-only
    and fast, and that an accent both lifts the level and sharpens the sweep -
    and accumulates over consecutive accents.  Each of those is measurable.  */
static double goertzel (const std::vector<float>& v, double sr, double hz, int from, int to)
{
    const double w = 2.0 * M_PI * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0; int n = 0;
    for (int i = std::max (0, from); i < std::min ((int) v.size(), to); ++i) {
        const double s = v[(size_t) i] + c * s1 - s2; s2 = s1; s1 = s; ++n;
    }
    if (!n) return 0.0;
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / n;
}

/*  A filter is judged with NOISE, not with a 41 Hz saw.  A saw of that
    pitch has partials 41 Hz apart, so "the level at 600 Hz" is really "the
    15th partial" and the answer moves with the note rather than with the
    filter - which is how the first version of this probe reported 2 dB per
    octave for an 18 dB/oct filter.                                        */
static void response (Engine& e, double sr, double* mag, const double* hz, int nh)
{
    e.p.v[P_a_level] = 0.f; e.p.v[P_b_level] = 0.f; e.p.v[P_sub_level] = 0.f;
    e.p.v[P_noise_level] = 1.f; e.p.v[P_noise_col] = 0.5f;
    e.p.v[P_e1_a] = 0.f; e.p.v[P_e1_d] = 1.f; e.p.v[P_e1_s] = 1.f;   // hold it open
    e.p.v[P_drv_amt] = 0.f; e.p.v[P_sing_level] = 0.f; e.p.v[P_macro_depth] = 0.f;
    e.p.v[P_ring_amt] = 0.f; e.p.v[P_macro_mass] = 0.f;
    e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f;
    Take t = render (e, sr, 3.0f, 36, 0.8f, 0.99f);
    const int from = (int) (sr * 0.5), to = (int) (sr * 2.8);
    for (int i = 0; i < nh; ++i) mag[i] = goertzel (t.L, sr, hz[i], from, to);
}

static void acid (double sr)
{
    static const char* nm[] = { "LADDER","GROWL","SCREAM","SVF","DIODE","COMB","FORMANT" };
    //  cutoff is dialled to a knob value; measure where it actually lands by
    //  finding the -3 dB point rather than assuming the mapping
    static const double hz[] = { 40, 60, 90, 130, 190, 280, 400, 600, 900, 1300, 1900, 2800, 4000, 6000 };
    const int NH = (int) (sizeof (hz) / sizeof (hz[0]));

    std::printf ("\nTHE 303, MEASURED\n\n");
    std::printf ("  1. DOES RESONANCE KEEP THE BASS?\n"
                 "     A Moog ladder subtracts its feedback from the input, so the\n"
                 "     low end falls as 1/(1+k) and a resonant patch goes thin.  A\n"
                 "     303 stays fat while it screams.  Level at 40 Hz, resonance\n"
                 "     0.05 -> 0.90, with the cutoff parked at 400 Hz:\n\n");
    for (int circ = 0; circ <= 6; ++circ) {
        double m0[NH], m1[NH];
        { Engine e; e.prepare (sr, 64); e.p.v[P_fa_circ] = (float) circ;
          e.p.v[P_fa_cut] = 0.42f; e.p.v[P_fa_res] = 0.05f;
          e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f; e.p.v[P_fb_circ] = 3.f;
          response (e, sr, m0, hz, NH); }
        { Engine e; e.prepare (sr, 64); e.p.v[P_fa_circ] = (float) circ;
          e.p.v[P_fa_cut] = 0.42f; e.p.v[P_fa_res] = 0.90f;
          e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f; e.p.v[P_fb_circ] = 3.f;
          response (e, sr, m1, hz, NH); }
        std::printf ("     %-8s 40 Hz %+7.2f dB    90 Hz %+7.2f dB\n", nm[circ],
                     20.0 * std::log10 ((m1[0] + 1e-15) / (m0[0] + 1e-15)),
                     20.0 * std::log10 ((m1[2] + 1e-15) / (m0[2] + 1e-15)));
    }

    std::printf ("\n  2. SLOPE above the corner.  A 303 is 18 dB/oct, a Moog 24.\n\n");
    for (int circ = 0; circ <= 6; ++circ) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_fa_circ] = (float) circ; e.p.v[P_fa_cut] = 0.42f; e.p.v[P_fa_res] = 0.05f;
        e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f; e.p.v[P_fb_circ] = 3.f;
        double m[NH]; response (e, sr, m, hz, NH);
        //  the corner is where it has fallen 3 dB from the passband
        const double ref = m[0];
        int ci = NH - 1;
        for (int i = 0; i < NH; ++i) if (20.0 * std::log10 ((m[i] + 1e-15) / (ref + 1e-15)) < -3.0) { ci = i; break; }
        //  slope measured an octave and two octaves ABOVE the corner, where
        //  a lowpass is in its asymptote
        double best = 0; int found = 0;
        for (int i = ci; i + 1 < NH; ++i) {
            const double oct = std::log2 (hz[i + 1] / hz[i]);
            const double d   = 20.0 * std::log10 ((m[i + 1] + 1e-15) / (m[i] + 1e-15)) / oct;
            if (hz[i] > hz[ci] * 1.9) { best += d; ++found; }
        }
        std::printf ("     %-8s corner near %5.0f Hz   slope %6.1f dB/oct\n",
                     nm[circ], hz[ci], found ? best / found : 0.0);
    }

    std::printf ("\n  3. EVEN HARMONICS.  Symmetric clipping makes odd harmonics\n"
                 "     only; a diode ladder clips asymmetrically and the even\n"
                 "     ones are the growl.  2nd against 3rd, a 110 Hz note at\n"
                 "     full resonance:\n\n");
    for (int circ = 0; circ <= 6; ++circ) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_fa_circ] = (float) circ; e.p.v[P_fa_cut] = 0.55f; e.p.v[P_fa_res] = 0.88f;
        e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f; e.p.v[P_fb_circ] = 3.f;
        //  A SINE, not a saw.  A saw already carries every harmonic, so the
        //  2nd/3rd ratio was reporting the SAW spectrum (+3.5 dB) for every
        //  circuit and could not see the filter asymmetry at all.  With a pure
        //  sine going in, any even harmonic coming out was made by the diodes.
        e.p.v[P_a_level] = 0.f; e.p.v[P_b_level] = 0.f; e.p.v[P_noise_level] = 0.f;
        e.p.v[P_sub_level] = 1.f; e.p.v[P_sub_shape] = 0.f; e.p.v[P_sub_oct] = 1.f;
        e.p.v[P_e1_s] = 1.f; e.p.v[P_e1_d] = 1.f;
        e.p.v[P_drv_amt] = 0.f; e.p.v[P_sing_level] = 0.f; e.p.v[P_macro_depth] = 0.f;
        Take t = render (e, sr, 1.6f, 45, 0.9f, 0.95f);
        const int a = (int) (sr * 0.4), b = (int) (sr * 1.4);
        const double f0 = 110.0;   // measure against the tone that is actually there
        const double h1 = goertzel (t.L, sr, f0, a, b);
        const double h2 = goertzel (t.L, sr, f0 * 2, a, b);
        const double h3 = goertzel (t.L, sr, f0 * 3, a, b);
        std::printf ("     %-8s 2nd %+7.2f dB under f0   3rd %+7.2f dB\n", nm[circ],
                     20.0 * std::log10 ((h2 + 1e-15) / (h1 + 1e-15)),
                     20.0 * std::log10 ((h3 + 1e-15) / (h1 + 1e-15)));
    }

    std::printf ("\n  4. ACCENT MUST ACCUMULATE over consecutive accented steps\n\n");
    {
        Engine e; e.prepare (sr, 64);
        applyFactory (10, e.p);
        e.p.v[P_seq_on] = 1.f; e.p.v[P_seq_len] = 8.f; e.p.v[P_seq_accent] = 0.9f;
        e.p.v[P_out_trim] = 0.f;
        Step s;
        for (int i = 0; i < 8; ++i) { s = Step(); s.accent = 1; s.gate = 60; e.setStep (i, s); }
        e.setTempo (130.0);
        Take t = render (e, sr, 4.0f, 28, 0.8f, 0.99f);
        const double step = 60.0 / 130.0 / 4.0;
        for (int i = 0; i < 8; ++i)
            std::printf ("     step %d  peak %.4f\n", i,
                         pk (t.L, (int) (sr * step * i), (int) (sr * step * (i + 1))));
    }
}
/*  Peak by window, and the biggest sample step, for one preset.  Used while
    hunting the note-on click; kept because it is the quickest way to see
    what the first few milliseconds of a patch actually do.                */
static void shape (double sr, int which)
{
    Engine e; e.prepare (sr, 64);
    if (which >= 0) applyFactory (which, e.p);
    e.p.v[P_out_trim] = 0.f;
    Take t = render (e, sr, 1.0f, 28, 0.9f);
    std::printf ("\n  %-18s peak by window (ms):\n", which >= 0 ? factoryName (which) : "default patch");
    static const double Wd[][2] = { {0,1},{1,3},{3,6},{6,10},{10,20},{20,40},{40,80},{80,160},{160,400} };
    const double sustain = pk (t.L, (int) (sr * 0.160), (int) (sr * 0.400));
    for (auto& w : Wd)
        std::printf ("    %4.0f-%4.0f ms  %.5f\n", w[0], w[1],
                     pk (t.L, (int) (sr * w[0] * 0.001), (int) (sr * w[1] * 0.001)));
    std::printf ("    sustain (160-400 ms) %.5f\n", sustain);
    double ms = 0; double at = 0;
    for (int i = 1; i < (int) (sr * 0.05); ++i) {
        const double d = std::fabs ((double) t.L[(size_t) i] - t.L[(size_t) i - 1]);
        if (d > ms) { ms = d; at = i * 1000.0 / sr; }
    }
    double msLate = 0;
    for (int i = (int) (sr * 0.2) + 1; i < (int) (sr * 0.4); ++i)
        msLate = std::max (msLate, std::fabs ((double) t.L[(size_t) i] - t.L[(size_t) i - 1]));
    std::printf ("    biggest step in first 50 ms %.5f at %.2f ms   (late: %.5f)\n", ms, at, msLate);
}

/*  Bisect one preset: render it with a single thing neutralised at a time.
    Whatever makes the symptom collapse is the thing causing it.           */
static void bisect (double sr, int which)
{
    struct K { const char* name; int pid; float to; };
    static const K ks[] = {
        { "(as it is)",      -1,            0.f },
        { "ALL SOURCES OFF", -2,            0.f },
        { "SING LEVEL 0",    P_sing_level,  0.f },
        { "DEPTH 0",         P_macro_depth, 0.f },
        { "RINGDOWN 0",      P_ring_amt,    0.f },
        { "SUB LEVEL 0",     P_sub_level,   0.f },
        { "A LEVEL 0",       P_a_level,     0.f },
        { "DRIVE AMOUNT 0",  P_drv_amt,     0.f },
        { "A RESONANCE 0",   P_fa_res,      0.f },
        { "AMP ATTACK 0.35", P_e1_a,        0.35f },
        { "MASS TRACK 0",    P_mass_track,  0.f },
        { "MACRO MASS 0",    P_macro_mass,  0.f },
    };
    std::printf ("\n  bisecting %s\n", which >= 0 ? factoryName (which) : "the default patch");
    for (const K& k : ks) {
        Engine e; e.prepare (sr, 64);
        if (which >= 0) applyFactory (which, e.p);
        e.p.v[P_out_trim] = 0.f;
        if (k.pid >= 0) e.p.v[k.pid] = k.to;
        if (k.pid == -2) { e.p.v[P_a_level] = 0.f; e.p.v[P_b_level] = 0.f;
                           e.p.v[P_sub_level] = 0.f; e.p.v[P_noise_level] = 0.f;
                           e.p.v[P_ringmod] = 0.f; }
        Take t = render (e, sr, 0.6f, 28, 0.9f);
        double ms = 0, at = 0;
        for (int i = 1; i < (int) (sr * 0.05); ++i) {
            const double d = std::fabs ((double) t.L[(size_t) i] - t.L[(size_t) i - 1]);
            if (d > ms) { ms = d; at = i * 1000.0 / sr; }
        }
        std::printf ("    %-18s step %.5f at %6.2f ms   peak 0-3 ms %.5f\n",
                     k.name, ms, at, pk (t.L, 0, (int) (sr * 0.003)));
    }
}

/*  What tools/level-presets.js reads: one line per preset, its loudness by
    the SHARED rule in loudness.h, the trim it carries, and its peak.      */
static void levels (double sr)
{
    for (int i = 0; i < numFactory(); ++i) {
        Engine e; e.prepare (sr, 64);
        applyFactory (i, e.p);
        Step pat[MAX_STEPS];
        applyFactoryPattern (i, pat, MAX_STEPS);
        for (int k = 0; k < MAX_STEPS; ++k) e.setStep (k, pat[k]);
        const float trim = e.p.v[P_out_trim];
        Take t = render (e, sr, 2.4f, 36, 0.9f);
        float pkv = 0.f; for (float x : t.L) pkv = std::max (pkv, std::fabs (x));
        std::printf ("%d\t%.4f\t%.4f\t%.4f\t%s\n", i, gwl::db (gwl::loudness (t.L, sr)),
                     trim, pkv, factoryName (i));
    }
}
/*  The parameter table as the processor sends it, law included - printed by
    the ENGINE's own code, so the panel probe builds its fixture from the
    truth.  Its first fixture was typed by hand, used the same wrong kind
    numbers as the page, and passed while choices were drawn as sliders.   */
static void table ()
{
    std::printf ("[");
    for (int i = 0; i < numParams(); ++i) {
        const PSpec& s = spec (i);
        float b = 0.f, sp = 1.f;
        const bool law = expLaw (i, b, sp);
        std::printf ("%s{\"id\":\"%s\",\"name\":\"%s\",\"def\":%g,\"lo\":%g,\"hi\":%g,\"kind\":%d,\"v\":%g",
                     i ? "," : "", s.id, s.label, s.def, s.lo, s.hi, s.kind, s.def);
        if (s.choices) std::printf (",\"choices\":\"%s\"", s.choices);
        if (law) std::printf (",\"lawBase\":%g,\"lawSpan\":%g", b, sp);
        if (std::strncmp (s.id, "seq_", 4) == 0) std::printf (",\"hidden\":true");
        std::printf ("}");
    }
    std::printf ("]\n");
}
/*  "Does DEPTH / TIME / SPACE do anything?"  Answered by measurement: the
    same note at the macro's 0 and at its 1, and how far apart they are -
    in level, in brightness, and in stereo width (side over mid).        */
static double sideOverMid (const Take& t, int a, int b)
{
    double s = 0, m = 0;
    for (int i = a; i < b && i < (int) t.L.size(); ++i) {
        const double M = 0.5 * (t.L[(size_t) i] + t.R[(size_t) i]), S = 0.5 * (t.L[(size_t) i] - t.R[(size_t) i]);
        m += M * M; s += S * S;
    }
    return std::sqrt (s / std::max (m, 1e-30));
}
static double apart (const Take& x, const Take& y, int a, int b)
{
    double d = 0, n = 0;
    for (int i = a; i < b && i < (int) x.L.size(); ++i) {
        const double e = x.L[(size_t) i] - y.L[(size_t) i];
        d += e * e; n += (double) x.L[(size_t) i] * x.L[(size_t) i];
    }
    return std::sqrt (d / std::max (n, 1e-30));
}
static void macros (double sr)
{
    struct M { const char* name; int pid; };
    const M ms[] = { { "MASS", P_macro_mass }, { "DEPTH", P_macro_depth }, { "ENERGY", P_macro_energy },
                     { "HORIZON", P_macro_horiz }, { "TIME", P_macro_time }, { "SPACE", P_macro_space } };
    const int presets[] = { -1, 0, 10, 16 };
    std::printf ("\nWHAT EACH MACRO DOES, 0 -> 1  (apart = rms difference / rms)\n");
    for (int pi : presets) {
        std::printf ("\n  %s\n", pi < 0 ? "default patch" : factoryName (pi));
        for (const M& m : ms) {
            Take t[2];
            for (int k = 0; k < 2; ++k) {
                Engine e; e.prepare (sr, 64);
                if (pi >= 0) applyFactory (pi, e.p);
                e.p.v[P_seq_on] = 0.f;
                e.p.v[m.pid] = (float) k;
                t[k] = render (e, sr, 1.5f, 28, 0.9f, 0.9f);
            }
            const int a = (int) (sr * 0.2), b = (int) (sr * 1.3);
            std::printf ("    %-8s apart %6.3f   level %+6.1f dB   width %.3f -> %.3f\n", m.name,
                         apart (t[0], t[1], a, b),
                         20.0 * std::log10 ((rmsOf (t[1].L, a, b) + 1e-12) / (rmsOf (t[0].L, a, b) + 1e-12)),
                         sideOverMid (t[0], a, b), sideOverMid (t[1], a, b));
        }
    }
}

/*  "CROSS ON changes pitch quite a lot."  The pitch of the same note with
    SYNC off and on, by autocorrelation, for several A/B tunings.        */
static double pitchOf (const std::vector<float>& v, double sr, int a, int b)
{
    const int lo = (int) (sr / 800.0), hi = (int) (sr / 20.0);
    double best = -1e30; int lag = lo;
    for (int L = lo; L < hi; ++L) {
        double c = 0, e0 = 0, e1 = 0;
        for (int i = a; i + L < b; i += 2) { c += v[(size_t) i] * v[(size_t) i + L]; e0 += v[(size_t) i] * v[(size_t) i]; e1 += v[(size_t) i + L] * v[(size_t) i + L]; }
        const double r = c / std::sqrt (std::max (e0 * e1, 1e-30));
        if (r > best + 0.02) { best = r; lag = L; }
    }
    return sr / lag;
}
static void syncTest (double sr)
{
    struct C { const char* what; float aOct, aSemi, bOct, bSemi, bFine; };
    const C cs[] = { { "A = B",               0, 0, 0, 0, 0 },
                     { "A -1 oct, B 0",      -1, 0, 0, 0, 0 },
                     { "B +7 semi",           0, 0, 0, 7, 0 },
                     { "B +1 oct",            0, 0, 1, 0, 0 },
                     { "A +7 semi (a sweep)", 0, 7, 0, 0, 0 },
                     { "B +12 cents",         0, 0, 0, 0, 12 } };
    std::printf ("\nSYNC: the pitch of E1 (41.2 Hz) with SYNC off and on\n\n");
    for (const C& c : cs) {
        double f[2];
        for (int k = 0; k < 2; ++k) {
            Engine e; e.prepare (sr, 64);
            e.p.v[P_a_oct] = c.aOct; e.p.v[P_a_semi] = c.aSemi;
            e.p.v[P_b_oct] = c.bOct; e.p.v[P_b_semi] = c.bSemi; e.p.v[P_b_fine] = c.bFine;
            e.p.v[P_b_level] = 0.f; e.p.v[P_sub_level] = 0.f;
            e.p.v[P_fa_cut] = 1.f; e.p.v[P_fb_cut] = 0.f;
            e.p.v[P_sync] = (float) k;
            Take t = render (e, sr, 1.0f, 28, 0.9f, 0.95f);
            f[k] = pitchOf (t.L, sr, (int) (sr * 0.3), (int) (sr * 0.8));
        }
        std::printf ("    %-22s off %7.2f Hz   on %7.2f Hz   %+7.0f cents\n", c.what, f[0], f[1],
                     1200.0 * std::log2 (f[1] / f[0]));
    }
}
//  CPU per factory preset: one held note, the engine alone (no panel), timed
//  against real time.  Percent of ONE core, at the host rate given.
//  Does the AMP envelope change the brightness?  The amp envelope multiplies
//  the signal BEFORE the filters and the drive, both nonlinear, so a louder
//  moment may come out brighter.  Measured with the FILTER envelope depth at
//  0: brightness = harmonics 6-20 over the fundamental, at the amp peak and
//  at the amp sustain.  A linear chain would read the same in both.
static void envCouple (double sr)
{
    const int note = 36;
    const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    struct Case { const char* name; int circ; float res, drv; };
    const Case cases[] = { { "LADDER res .2 drive 0", 0, 0.2f, 0.f }, { "LADDER res .6 drive 0", 0, 0.6f, 0.f },
                           { "DIODE  res .6 drive 0", 4, 0.6f, 0.f }, { "LADDER res .2 drive .6", 0, 0.2f, 0.6f },
                           { "SVF    res .2 drive 0", 3, 0.2f, 0.f } };
    for (const Case& c : cases) {
        Engine e; e.prepare (sr, 256);
        e.p.v[P_fa_circ] = (float) c.circ; e.p.v[P_fa_res] = c.res; e.p.v[P_fa_cut] = 0.55f; e.p.v[P_fa_env] = 0.f;
        e.p.v[P_drv_amt] = c.drv; e.p.v[P_sub_level] = 0.f; e.p.v[P_fb_cut] = 1.f;
        if (std::getenv ("GW_BARE")) { e.p.v[P_sing_level] = 0.f; e.p.v[P_macro_depth] = 0.f; e.p.v[P_ring_amt] = 0.f; e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f; e.p.v[P_macro_energy] = 0.f; }
        e.p.v[P_e1_a] = 0.f; e.p.v[P_e1_d] = 0.62f; e.p.v[P_e1_s] = 0.15f;       //  a loud strike falling to a quiet sustain
        if (std::getenv ("GW_FLAT")) e.p.v[P_e1_s] = 1.f;                         //  control: no amp movement at all
        std::vector<float> L ((size_t) (sr * 1.5)), R (L.size());
        e.noteOn (note, 0.9f);
        for (size_t i = 0; i < L.size(); i += 256) e.process (&L[i], &R[i], (int) std::min<size_t> (256, L.size() - i));
        auto bright = [&] (double t0) {
            const int a = (int) (sr * t0), n = (int) (sr * 0.06);
            double up = 0; for (int h = 6; h <= 20; ++h) up += goertzel (L, sr, f0 * h, a, a + n);
            return 20.0 * std::log10 ((up + 1e-12) / (goertzel (L, sr, f0, a, a + n) + 1e-12));
        };
        const double early = bright (std::getenv ("GW_T0") ? std::atof (std::getenv ("GW_T0")) : 0.02), late = bright (1.2);
        std::printf ("  %-24s brightness at the strike %+6.1f dB   at sustain %+6.1f dB   the amp env moved it %+5.1f dB\n",
                     c.name, early, late, early - late);
    }
}

//  Every factory preset rendered to <dir>/NN.f32 (left channel, raw float):
//  two notes, a held one and a short one, so a change can be measured preset
//  by preset against the same bank rendered by an earlier build.
static void dump (double sr, const char* dir)
{
    for (int i = 0; i < numFactory(); ++i) {
        Engine e; e.prepare (sr, 256); applyFactory (i, e.p);
        const int n = (int) (sr * 2.4);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        e.noteOn (36, 0.8f);
        for (int k = 0; k < n; k += 256) {
            if (k == (int) (sr * 1.2)) e.noteOff (36);
            if (k == (int) (sr * 1.6)) e.noteOn (43, 0.9f);
            if (k == (int) (sr * 1.8)) e.noteOff (43);
            e.process (&L[(size_t) k], &R[(size_t) k], std::min (256, n - k));
        }
        char path[512]; std::snprintf (path, sizeof path, "%s/%02d.f32", dir, i);
        if (FILE* f = std::fopen (path, "wb")) { std::fwrite (L.data(), sizeof (float), L.size(), f); std::fclose (f); }
    }
    std::printf ("dumped %d presets to %s\n", numFactory(), dir);
}

//  How much does the level move across the DRIVE knob, PRE vs POST, with the
//  filter half-open and fully open?  Separates the two possible causes of a
//  PRE drive losing level: the hotter unfiltered input, or the filter
//  removing the harmonics the drive made.
static void drivePos (double sr)
{
    for (int pos = 0; pos < 2; ++pos)
        for (float cut : { 0.6f, 1.0f })
            for (int t = 0; t < 4; ++t) {
                double lv[2];
                for (int k = 0; k < 2; ++k) {
                    Engine e; e.prepare (sr, 256);
                    e.p.v[P_drv_type] = (float) t; e.p.v[P_drv_pos] = (float) pos; e.p.v[P_fa_cut] = cut;
                    e.p.v[P_macro_energy] = 0.f; e.p.v[P_e1_s] = 1.f; e.p.v[P_e1_d] = 1.f; e.p.v[P_drv_amt] = (float) k;
                    std::vector<float> L ((size_t) (sr * 1.0)), R (L.size());
                    e.noteOn (36, 0.8f);
                    for (size_t i = 0; i < L.size(); i += 256) e.process (&L[i], &R[i], (int) std::min<size_t> (256, L.size() - i));
                    lv[k] = rmsOf (L, (int) (sr * 0.4), (int) (sr * 0.9));
                }
                std::printf ("  %s  cut %.1f  drive %d: %+6.2f dB\n", pos ? "POST" : "PRE ", cut, t, 20.0 * std::log10 ((lv[1] + 1e-12) / (lv[0] + 1e-12)));
            }
}

#include <xmmintrin.h>
static void cost (double sr)
{
    std::printf ("%-16s %8s\n", "preset", "% core");
    _mm_setcsr (_mm_getcsr() | 0x8040);   //  FTZ+DAZ, as ScopedNoDenormals gives the plug-in
    std::vector<float> L (512), R (512);
    for (int i = 0; i < numFactory(); ++i) {
        Engine e; e.prepare (sr, 512); applyFactory (i, e.p);
        e.noteOn (36, 0.8f);
        const int blocks = (int) (sr * 3.0 / 512);
        for (int b = 0; b < 40; ++b) e.process (L.data(), R.data(), 512);       // warm
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.process (L.data(), R.data(), 512);
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("%-16s %7.1f%%\n", factoryName (i), 100.0 * s / 3.0);
    }
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const char* what = argc > 1 ? argv[1] : "onset";
    if      (!std::strcmp (what, "onset"))   onset (sr);
    else if (!std::strcmp (what, "presets")) presets (sr);
    else if (!std::strcmp (what, "303"))     acid (sr);
    else if (!std::strcmp (what, "levels"))  levels (sr);
    else if (!std::strcmp (what, "table"))   table ();
    else if (!std::strcmp (what, "macros"))  macros (sr);
    else if (!std::strcmp (what, "sync"))    syncTest (sr);
    else if (!std::strcmp (what, "cost"))    cost (sr);
    else if (!std::strcmp (what, "drivepos")) drivePos (sr);
    else if (!std::strcmp (what, "dump") && argc > 2) dump (sr, argv[2]);
    else if (!std::strcmp (what, "envcouple")) envCouple (sr);
    else if (!std::strcmp (what, "bisect"))  bisect (sr, argc > 2 ? std::atoi (argv[2]) : -1);
    else if (!std::strcmp (what, "shape")) {
        if (argc > 2) shape (sr, std::atoi (argv[2]));
        else { const int w[] = { -1, 13, 3, 11, 30, 6, 10 };
               for (int i : w) shape (sr, i); }
    }
    else { std::printf ("gwprobe onset | presets | 303\n"); return 1; }
    return 0;
}
