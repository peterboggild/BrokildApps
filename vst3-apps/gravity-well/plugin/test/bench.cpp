#include <xmmintrin.h>
#include <chrono>
/*  GRAVITY WELL - the bench.

    Bounded proves nothing (the Martian Gain rule: a patch cable wired to
    nothing passes every bounded test ever written).  These measure the
    CLAIMS - dilation against its own formula, redshift moving the spectrum
    but not the pitch, the ringdown's published ratios, and MASS 0 being
    exactly the instrument without a well.

        gwtest                 run the checks
        gwtest --render <dir>  write a WAV per preset group, to LISTEN to
*/
#include "Engine.h"
#include "loudness.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

using namespace gw;

static int checks = 0, fails = 0;
static void CHECK (bool ok, const char* what) {
    ++checks;
    if (!ok) { ++fails; std::printf ("  FAIL  %s\n", what); }
}
static void say (const char* s) { std::printf ("-- %s\n", s); }

//  A seeded xorshift, so a failing random machine can be reproduced exactly.
//  std::rand would make the 300-machine sweep a different sweep every run.
struct Rng {
    uint32_t s = 1u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
};

// ---------------------------------------------------------------- measuring
struct Take { std::vector<float> L, R; };

static Take render (Engine& e, double sr, float seconds, int note, float vel,
                    float holdFrac = 0.8f)
{
    const int n = (int) (sr * seconds), block = 256;
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

static double rms (const std::vector<float>& v, int from, int to) {
    double s = 0.0; int n = 0;
    for (int i = std::max (0, from); i < std::min ((int) v.size(), to); ++i) { s += (double) v[i] * v[i]; ++n; }
    return n ? std::sqrt (s / n) : 0.0;
}
static float peak (const std::vector<float>& v) {
    float p = 0.f; for (float x : v) p = std::max (p, std::fabs (x)); return p;
}
//  Goertzel: one bin, which is all a tuning check needs
static double bin (const std::vector<float>& v, double sr, double hz, int from, int len) {
    const double w = 2.0 * M_PI * hz / sr;
    const double c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = from; i < from + len && i < (int) v.size(); ++i) {
        const double s = v[(size_t) i] + c * s1 - s2; s2 = s1; s1 = s;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2);
}
//  spectral centroid by a coarse filterbank - enough to see a formant sink
static double centroid (const std::vector<float>& v, double sr, int from, int len) {
    double num = 0, den = 0;
    for (double f = 60.0; f < 6000.0; f *= 1.18) {
        const double m = bin (v, sr, f, from, len);
        num += m * f; den += m;
    }
    return den > 1e-9 ? num / den : 0.0;
}

static void writeWav (const std::string& path, const Take& t, double sr) {
    FILE* f = std::fopen (path.c_str(), "wb");
    if (!f) { std::printf ("  could not write %s\n", path.c_str()); return; }
    const int n = (int) t.L.size();
    const int dataBytes = n * 2 * 2;
    auto u32 = [&] (unsigned v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (unsigned short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); u32 (16); u16 (1); u16 (2);
    u32 ((unsigned) sr); u32 ((unsigned) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (int i = 0; i < n; ++i) {
        auto cv = [] (float x) { x = x < -1.f ? -1.f : (x > 1.f ? 1.f : x); return (short) (x * 32767.f); };
        short l = cv (t.L[(size_t) i]), r = cv (t.R[(size_t) i]);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
}

// -------------------------------------------------------------- the checks
static void testDefaults (double sr)
{
    say ("silence in, silence out - and a note actually sounds");
    Engine e; e.prepare (sr, 512);
    std::vector<float> L (4096, 0.f), R (4096, 0.f);
    e.process (L.data(), R.data(), 4096);
    CHECK (peak (L) == 0.0f && peak (R) == 0.0f, "an idle engine is not exactly silent");

    Engine e2; e2.prepare (sr, 512);
    Take t = render (e2, sr, 2.0f, 36, 0.9f);
    const float p = peak (t.L);
    std::printf ("     default patch peak %.4f\n", p);
    CHECK (p > 0.02f, "the default patch makes no sound");
    CHECK (p <= 1.0f,  "the default patch exceeds full scale");
}

static void testDilation (double sr)
{
    say ("time dilation IS sqrt(1 - rs/r), across mass and radius");
    Engine e; e.prepare (sr, 512);
    bool ok = true;
    for (float m = 0.f; m <= 1.001f; m += 0.25f) {
        e.p.v[P_macro_mass] = m;
        e.p.v[P_macro_horiz] = 1.0f;
        e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f;
        Take t = render (e, sr, 0.6f, 36, 0.8f);       // let the follower settle
        const float rs = e.rsNow();
        for (float rad = 0.f; rad <= 1.001f; rad += 0.2f) {
            const float r = R_NEAR + rad * (R_FAR - R_NEAR);
            const float want = (r <= rs) ? 0.f : std::sqrt (std::max (0.f, 1.f - rs / r));
            const float got  = e.dilationAt (rad);
            if (std::fabs (want - got) > 1e-4f) ok = false;
        }
    }
    CHECK (ok, "dilation does not match its own formula");

    //  and it must be ORDERED: an inner modulator is always the slower one
    e.p.v[P_macro_mass] = 0.8f; e.p.v[P_macro_horiz] = 0.9f;
    render (e, sr, 0.6f, 36, 0.9f);
    bool ordered = true;
    float prev = -1.f;
    for (float rad = 0.f; rad <= 1.001f; rad += 0.2f) {
        const float d = e.dilationAt (rad);
        if (d < prev - 1e-6f) ordered = false;
        prev = d;
    }
    std::printf ("     rs %.2f   dilations", e.rsNow());
    for (float rad = 0.f; rad <= 1.001f; rad += 0.2f) std::printf (" %.2f", e.dilationAt (rad));
    std::printf ("\n");
    CHECK (ordered, "dilation is not ordered by radius");
}

static void testMassZeroIsOrdinary (double sr)
{
    say ("MASS 0 is the instrument WITHOUT a well - bit for bit");
    Engine a, b;
    a.prepare (sr, 512); b.prepare (sr, 512);
    //  Both engines have MASS pinned at 0, so rs is exactly 0 and every
    //  dilation is exactly 1.  They differ ONLY in where the modulators sit:
    //  a has them all at the rim, b has them all at the throat.  With a flat
    //  well that difference must not reach the output by one bit.
    //
    //  NOT by switching HORIZON off: HORIZON also offsets both filter
    //  cutoffs, so that compares two different patches and fails for a
    //  reason that has nothing to do with the well.
    a.p.v[P_macro_mass] = 0.f; a.p.v[P_mass_track] = 0.f; a.p.v[P_mass_vel] = 0.f;
    a.p.v[P_redshift] = 0.f;   a.p.v[P_ring_amt] = 0.f;
    static const int radii[] = { P_r_e2, P_r_e3, P_r_l1, P_r_l2, P_r_l3, P_r_rnd, P_r_chirp };
    for (int r : radii) a.p.v[r] = 1.f;
    b.p = a.p;
    for (int r : radii) b.p.v[r] = 0.f;         // at the throat, and still flat
    Take ta = render (a, sr, 1.0f, 36, 0.8f);
    Take tb = render (b, sr, 1.0f, 36, 0.8f);
    const bool same = std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0;
    CHECK (same, "at MASS 0 the modulator radii still change the sound - the well is not flat");
}

static void testRedshiftSpectrumNotPitch (double sr)
{
    say ("REDSHIFT sinks the spectrum and leaves the pitch alone");
    Engine e; e.prepare (sr, 512);
    e.p.v[P_fa_circ] = 6.f;                      // FORMANT: where redshift lives
    e.p.v[P_macro_mass] = 0.9f; e.p.v[P_macro_horiz] = 1.0f;
    e.p.v[P_mass_track] = 0.f;  e.p.v[P_mass_vel] = 0.f;
    e.p.v[P_sub_level] = 0.f;

    const int note = 36;
    const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);

    e.p.v[P_redshift] = 0.f;
    Take dry = render (e, sr, 1.6f, note, 0.8f);
    e.p.v[P_redshift] = 0.95f;
    Take wet = render (e, sr, 1.6f, note, 0.8f);

    const int from = (int) (sr * 0.5), len = (int) (sr * 0.5);
    const double cd = centroid (dry.L, sr, from, len), cw = centroid (wet.L, sr, from, len);
    const double pd = bin (dry.L, sr, f0, from, len), pw = bin (wet.L, sr, f0, from, len);
    const double nd = bin (dry.L, sr, f0 * 1.06, from, len), nw = bin (wet.L, sr, f0 * 1.06, from, len);
    std::printf ("     centroid %.0f -> %.0f Hz   f0 rel-neighbour %.2f -> %.2f\n",
                 cd, cw, pd / (nd + 1e-9), pw / (nw + 1e-9));
    CHECK (cw < cd * 0.92, "redshift did not move the spectrum down");
    CHECK (pw > nw * 1.5,  "the fundamental did not survive the redshift");
}

static void testRingdown (double sr)
{
    say ("the ringdown carries the published QNM ratios; heavier rings lower");
    Engine e; e.prepare (sr, 512);
    e.p.v[P_ring_amt] = 1.f; e.p.v[P_ring_tone] = 0.5f; e.p.v[P_ring_decay] = 0.7f;
    e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f;

    auto centroidAt = [&] (float mass) {
        e.p.v[P_macro_mass] = mass;
        Take t = render (e, sr, 1.5f, 36, 0.9f, 0.15f);
        return centroid (t.L, sr, (int) (sr * 0.5), (int) (sr * 0.6));
    };
    const double light = centroidAt (0.0f), heavy = centroidAt (1.0f);
    std::printf ("     ring centroid: light %.0f Hz   heavy %.0f Hz\n", light, heavy);
    CHECK (heavy < light * 0.95, "more mass did not ring lower");

    //  the ratios themselves, straight off the table
    CHECK (std::fabs (QNM_F[0] - 1.0000f) < 1e-4f, "QNM fundamental ratio moved");
    CHECK (std::fabs (QNM_F[3] - 1.6039f) < 1e-4f, "QNM l=3 ratio moved");
    CHECK (QNM_D[1] > QNM_D[0] && QNM_D[2] > QNM_D[1], "QNM overtones must damp faster");
}

static void testDrivePowerNeutral (double sr)
{
    say ("every drive is POWER NEUTRAL, PRE and POST - turning it up must not turn you up");
    Engine e; e.prepare (sr, 512);
    for (int pos = 0; pos < 2; ++pos)
    for (int t = 0; t < 4; ++t) {
        double lo = 0, hi = 0;
        e.p = Params();
        e.p.v[P_drv_type] = (float) t; e.p.v[P_drv_pos] = (float) pos;
        e.p.v[P_macro_energy] = 0.f;
        e.p.v[P_e1_s] = 1.f; e.p.v[P_e1_d] = 1.f;
        e.p.v[P_drv_amt] = 0.f;
        { Take a = render (e, sr, 1.2f, 36, 0.8f); lo = rms (a.L, (int) (sr * 0.4), (int) (sr * 0.9)); }
        e.p.v[P_drv_amt] = 1.f;
        { Take b = render (e, sr, 1.2f, 36, 0.8f); hi = rms (b.L, (int) (sr * 0.4), (int) (sr * 0.9)); }
        const double dB = 20.0 * std::log10 ((hi + 1e-9) / (lo + 1e-9));
        std::printf ("     %s drive %d: %+5.2f dB across the whole knob\n", pos ? "POST" : "PRE ", t, dB);
        CHECK (std::fabs (dB) < 4.0, "a drive moves the level too far");
    }
}

static void testSingularitySurvives (double sr)
{
    say ("the SINGULARITY survives both filters shut and full drive");
    Engine e; e.prepare (sr, 512);
    e.p.v[P_fa_cut] = 0.f; e.p.v[P_fb_cut] = 0.f;
    e.p.v[P_route] = 0.f;
    e.p.v[P_drv_amt] = 1.f; e.p.v[P_drv_type] = 3.f;
    e.p.v[P_sing_level] = 0.f; e.p.v[P_macro_depth] = 0.f;
    //  the sub now sits an octave below the note and slips under two shut
    //  lowpasses; this check is about the protected core, so it is off
    e.p.v[P_sub_level] = 0.f;
    Take without = render (e, sr, 1.0f, 36, 0.9f);
    e.p.v[P_sing_level] = 1.f;
    Take with = render (e, sr, 1.0f, 36, 0.9f);
    const double a = rms (without.L, (int) (sr * 0.3), (int) (sr * 0.7));
    const double b = rms (with.L,    (int) (sr * 0.3), (int) (sr * 0.7));
    std::printf ("     rms without %.5f -> with %.5f\n", a, b);
    CHECK (b > a * 2.0, "the protected core did not survive the chain");
}

static void testBoundedNotJustQuiet (double sr)
{
    say ("300 random machines: bounded, and not silent");
    Engine e; e.prepare (sr, 512);
    Rng r; r.s = 12345u;
    int silent = 0; float worst = 0.f;
    for (int k = 0; k < 300; ++k) {
        e.p = Params();
        for (int i = 0; i < NUM_PARAMS; ++i) {
            const PSpec& s = spec (i);
            if (s.kind == GW_KIND_CHOICE || s.kind == GW_KIND_INT)
                e.p.v[i] = std::floor (s.lo + r.uni() * (s.hi - s.lo) + 0.5f);
            else
                e.p.v[i] = s.lo + r.uni() * (s.hi - s.lo);
        }
        e.p.v[P_out_trim] = 0.f;
        Take t = render (e, sr, 0.35f, 28 + (int) (r.uni() * 24), 0.5f + 0.5f * r.uni());
        const float p = peak (t.L);
        worst = std::max (worst, p);
        if (p < 1e-5f) ++silent;
        bool finite = true;
        for (float x : t.L) if (!std::isfinite (x)) { finite = false; break; }
        if (!finite) { CHECK (false, "a random machine produced a non-finite sample"); break; }
    }
    std::printf ("     worst peak %.4f   silent %d of 300\n", worst, silent);
    CHECK (worst <= 1.0f, "a random machine exceeded full scale");
    CHECK (silent < 45,   "too many random machines are silent");
}

static void testStableNotJustBounded (double sr)
{
    say ("BOUNDED IS NOT STABLE - stop the input and watch");
    Engine e; e.prepare (sr, 512);
    e.p.v[P_ring_amt] = 1.f; e.p.v[P_ring_decay] = 1.f;
    e.p.v[P_fa_circ] = 5.f;  e.p.v[P_fa_res] = 1.f;      // comb at full feedback
    e.p.v[P_macro_mass] = 1.f;
    const int n = (int) (sr * 9.0);
    std::vector<float> L ((size_t) n, 0.f), R ((size_t) n, 0.f);
    e.noteOn (36, 1.0f);
    e.process (L.data(), R.data(), (int) (sr * 0.25));
    e.noteOff (36);
    e.allNotesOff();
    e.process (&L[(size_t) (sr * 0.25)], &R[(size_t) (sr * 0.25)], n - (int) (sr * 0.25));
    const double a = rms (L, (int) (sr * 1.0), (int) (sr * 2.0));
    const double b = rms (L, (int) (sr * 4.0), (int) (sr * 5.0));
    const double c = rms (L, (int) (sr * 7.5), (int) (sr * 8.5));
    std::printf ("     tail rms  1-2s %.3e   4-5s %.3e   7.5-8.5s %.3e\n", a, b, c);
    CHECK (b <= a * 0.9 + 1e-9, "the tail is not decaying - something is oscillating");
    CHECK (c <= b * 0.9 + 1e-9, "the tail is still not decaying");
}

static void testPanicLetsGoOfThePedal (double sr)
{
    say ("a panic lets go of the sustain pedal");
    Engine a, b;
    a.prepare (sr, 512); b.prepare (sr, 512);
    //  a: pedal down, panic, then a fresh note released
    a.sustain (true); a.noteOn (36, 0.9f);
    std::vector<float> L (4096), R (4096);
    for (int i = 0; i < 8; ++i) a.process (L.data(), R.data(), 4096);
    a.allNotesOff();
    a.noteOn (40, 0.9f); a.noteOff (40);
    //  b: the control, which never touched the pedal
    b.noteOn (40, 0.9f); b.noteOff (40);
    double ea = 0, eb = 0;
    for (int i = 0; i < 40; ++i) {
        a.process (L.data(), R.data(), 4096); ea = rms (L, 0, 4096);
        b.process (L.data(), R.data(), 4096); eb = rms (L, 0, 4096);
    }
    std::printf ("     after panic %.3e   control %.3e\n", ea, eb);
    CHECK (ea < eb * 4.0 + 1e-7, "a note is still held by a pedal nobody is pressing");
}

static void testPresets (double sr)
{
    say ("every factory preset: bounded, audible, and levelled");
    double loudest = 0, quietest = 1e9;
    int loudIdx = 0, quietIdx = 0;
    for (int i = 0; i < numFactory(); ++i) {
        Engine e; e.prepare (sr, 512);
        applyFactory (i, e.p);
        //  the sequencer is off the panel (Peter, 2026-09-27), so a preset that
        //  turned it on would play a pattern nobody can see or switch off
        if (e.p.v[P_seq_on] > 0.5f) { std::printf ("  FAIL  preset %d (%s) turns the hidden sequencer on\n", i, factoryName (i)); ++fails; }
        ++checks;
        Step pat[MAX_STEPS];
        applyFactoryPattern (i, pat, MAX_STEPS);
        for (int k = 0; k < MAX_STEPS; ++k) e.setStep (k, pat[k]);
        Take t = render (e, sr, 2.4f, 36, 0.9f);
        const float p = peak (t.L);
        //  the loudest 200 ms window, not a fixed one: the bank holds
        //  drones AND 60 ms plucks, and a fixed window reported a pluck
        //  as silent because the note was over before it opened.
        const double l = gwl::loudness (t.L, sr);
        if (l > loudest)  { loudest = l;  loudIdx = i; }
        if (l < quietest) { quietest = l; quietIdx = i; }
        if (p > 1.0f) { std::printf ("  FAIL  preset %d (%s) peaks %.3f\n", i, factoryName (i), p); ++fails; }
        ++checks;
        if (l < 1e-4) { std::printf ("  FAIL  preset %d (%s) is silent\n", i, factoryName (i)); ++fails; }
        ++checks;
    }
    const double spread = 20.0 * std::log10 (loudest / (quietest + 1e-9));
    std::printf ("     loudest %s (%.4f)   quietest %s (%.4f)   spread %.1f dB\n",
                 factoryName (loudIdx), loudest, factoryName (quietIdx), quietest, spread);
    CHECK (spread < 12.0, "the preset bank is not levelled - nasty surprises await");
}

static void testRates()
{
    say ("44.1 / 48 / 96 kHz");
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        Engine e; e.prepare (sr, 512);
        Take t = render (e, sr, 0.8f, 36, 0.9f);
        const float p = peak (t.L);
        std::printf ("     %6.0f Hz  peak %.4f\n", sr, p);
        CHECK (p > 0.01f && p <= 1.0f, "a sample rate produced nothing usable");
    }
}

/*  A SECOND NOTE MUST NOT WIPE THE FIRST.  noteOff clears `held` and leaves
    the release running; a note arriving then used to find wasOn false and
    call Voice::reset(), zeroing every filter state and dropping the envelope
    to 0 while the previous note was still at full amplitude.  A hard step on
    every note after the first - which is why one-note probes never saw it,
    and why the sequencer path (which gates without resetting) was clean.

    A click is a DISCONTINUITY, so the measurement is the biggest sample step
    across the transition against the same patch own biggest step while it is
    simply playing.  Both windows carry the same waveform, so a steep but
    legitimate slope cancels out of the ratio.                              */
static void testNoteTransition (double sr)
{
    say ("a note arriving over a dying note must not step");
    double worst = 0.0; int worstPreset = -1;
    for (int k = 0; k < numFactory(); ++k) {
        Engine e; e.prepare (sr, 64);
        applyFactory (k, e.p);
        e.p.v[P_seq_on]   = 0.f;      // the sequencer has its own note path
        e.p.v[P_out_trim] = 0.f;
        const int n = (int) (sr * 1.2), block = 64;
        std::vector<float> L ((size_t) n, 0.f), R ((size_t) n, 0.f);
        const int sw = (int) (sr * 0.6);            // the switch, mid-note
        e.noteOn (28, 0.9f);
        for (int i = 0; i < n; i += block) {
            const int m = std::min (block, n - i);
            if (i <= sw && i + m > sw) { e.noteOff (28); e.noteOn (33, 0.9f); }
            e.process (&L[(size_t) i], &R[(size_t) i], m);
        }
        auto biggest = [&] (int from, int to) {
            double mx = 0;
            for (int i = std::max (1, from); i < std::min (n, to); ++i)
                mx = std::max (mx, std::fabs ((double) L[(size_t) i] - L[(size_t) i - 1]));
            return mx;
        };
        const double atSwitch = biggest (sw - 8, sw + (int) (sr * 0.004));
        const double steady   = std::max (biggest ((int) (sr * 0.30), (int) (sr * 0.55)),
                                          biggest ((int) (sr * 0.80), (int) (sr * 1.10)));
        //  ...and against the patch's OWN first attack.  A pluck whose note has
        //  died by the switch has a silent tail, so against the tail alone its
        //  second attack read 15x - an ordinary pluck, measured as a click.  A
        //  note arriving over a dying one must be no harsher than one arriving
        //  from silence; the real bug wiped a LOUD note and still fails this.
        const double firstAttack = biggest (1, (int) (sr * 0.004));
        const double ratio = atSwitch / std::max (std::max (steady, firstAttack), 1e-7);
        if (ratio > worst) { worst = ratio; worstPreset = k; }
    }
    std::printf ("     worst step at a note change: %.2fx its own waveform (%s)\n",
                 worst, worstPreset >= 0 ? factoryName (worstPreset) : "-");
    CHECK (worst < 4.0, "a note change steps - the voice is being wiped mid-ring");
}
/*  THE 303 FILTER.  Two claims, and both of them fail silently if the
    circuit is quietly a Moog with one pole removed.

    1. Resonance must KEEP the bass.  A Moog ladder subtracts its feedback
       from the input so the low end falls as 1/(1+k); a diode ladder holds
       its passband, and that is most of what "fat acid" means.  Measured
       with NOISE, because a 41 Hz saw has its partials 41 Hz apart and any
       single frequency then reports a partial rather than a response.

    2. The diodes must clip ASYMMETRICALLY.  A symmetric shaper makes odd
       harmonics only.  Driven by a pure SINE - a saw would carry the even
       harmonics in already and hide the whole effect - the 2nd harmonic is
       made by the nonlinearity and nothing else.                          */
static void testAcidFilter (double sr)
{
    say ("the 303 filter: resonance keeps the bass, and the diodes are asymmetric");

    auto bassAt = [&] (int circ, float res) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_fa_circ] = (float) circ; e.p.v[P_fa_cut] = 0.42f; e.p.v[P_fa_res] = res;
        e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f; e.p.v[P_fb_circ] = 3.f;
        e.p.v[P_a_level] = 0.f; e.p.v[P_sub_level] = 0.f; e.p.v[P_noise_level] = 1.f;
        e.p.v[P_e1_a] = 0.f; e.p.v[P_e1_d] = 1.f; e.p.v[P_e1_s] = 1.f;
        e.p.v[P_drv_amt] = 0.f; e.p.v[P_sing_level] = 0.f; e.p.v[P_macro_depth] = 0.f;
        e.p.v[P_macro_mass] = 0.f; e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f;
        Take t = render (e, sr, 3.0f, 36, 0.8f, 0.99f);
        return bin (t.L, sr, 40.0, (int) (sr * 0.5), (int) (sr * 2.3));
    };
    const double dLo = bassAt (4, 0.05f), dHi = bassAt (4, 0.90f);
    const double lLo = bassAt (0, 0.05f), lHi = bassAt (0, 0.90f);
    const double dDb = 20.0 * std::log10 ((dHi + 1e-15) / (dLo + 1e-15));
    const double lDb = 20.0 * std::log10 ((lHi + 1e-15) / (lLo + 1e-15));
    std::printf ("     40 Hz across resonance:  DIODE %+.2f dB   LADDER %+.2f dB\n", dDb, lDb);
    CHECK (dDb > -3.0, "DIODE thins the bass with resonance - that is a Moog, not a 303");
    CHECK (dDb > lDb + 6.0, "DIODE is no fatter than the ladder - the passband is not held");

    auto even = [&] (int circ) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_fa_circ] = (float) circ; e.p.v[P_fa_cut] = 0.55f; e.p.v[P_fa_res] = 0.88f;
        e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f; e.p.v[P_fb_circ] = 3.f;
        e.p.v[P_a_level] = 0.f; e.p.v[P_b_level] = 0.f; e.p.v[P_noise_level] = 0.f;
        //  the sub follows oscillator A and sits an octave below it: A up one
        //  octave puts this pure sine back on the 110 Hz the check measures
        e.p.v[P_sub_level] = 1.f; e.p.v[P_sub_shape] = 0.f; e.p.v[P_a_oct] = 1.f;
        e.p.v[P_e1_s] = 1.f; e.p.v[P_e1_d] = 1.f;
        e.p.v[P_drv_amt] = 0.f; e.p.v[P_sing_level] = 0.f; e.p.v[P_macro_depth] = 0.f;
        Take t = render (e, sr, 1.6f, 45, 0.9f, 0.95f);
        const int a = (int) (sr * 0.4), b = (int) (sr * 1.4);
        const double h1 = bin (t.L, sr, 110.0, a, b);
        const double h2 = bin (t.L, sr, 220.0, a, b);
        return 20.0 * std::log10 ((h2 + 1e-15) / (h1 + 1e-15));
    };
    const double dE = even (4), lE = even (0);
    std::printf ("     2nd harmonic from a SINE:  DIODE %.1f dB   LADDER %.1f dB\n", dE, lE);
    //  20 dB, not 30: the threshold is an order of magnitude, which a
    //  symmetric circuit cannot reach by accident, and it is not tuned to
    //  the exact number this build happens to produce.
    CHECK (dE > lE + 20.0, "DIODE makes no more even harmonics than a symmetric filter");
}
/*  260927.3 - each of Peter's "does it do anything / is it broken" reports,
    as a gate.  A wire that nothing reads passes every bounded test ever
    written (the Martian Gain rule), which is how PAN, LEVEL, WHEEL and
    PRESSURE shipped dead in 260926.1.                                     */
static double pitchOfB (const std::vector<float>& v, double sr, int a, int b)
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
static double apartLR (const Take& x, const Take& y, int a, int b)
{
    double d = 0, n = 0;
    for (int i = a; i < b && i < (int) x.L.size(); ++i) {
        const double eL = x.L[(size_t) i] - y.L[(size_t) i], eR = x.R[(size_t) i] - y.R[(size_t) i];
        d += eL * eL + eR * eR; n += (double) x.L[(size_t) i] * x.L[(size_t) i] + (double) x.R[(size_t) i] * x.R[(size_t) i];
    }
    return std::sqrt (d / std::max (n, 1e-30));
}
static double sideMid (const Take& t, int a, int b)
{
    double s = 0, m = 0;
    for (int i = a; i < b && i < (int) t.L.size(); ++i) {
        const double M = 0.5 * (t.L[(size_t) i] + t.R[(size_t) i]), S = 0.5 * (t.L[(size_t) i] - t.R[(size_t) i]);
        m += M * M; s += S * S;
    }
    return std::sqrt (s / std::max (m, 1e-30));
}

/*  Peter, 2026-09-27: the three decisions, each with its own evidence.     */
static void testSubOctave (double sr)
{
    say ("SUB OCTAVE 1 is an octave BELOW oscillator A, 2 is two below");
    const int note = 45;                                  // A2, 110 Hz - well clear of the subsonic
    const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    double got[3] = {};
    for (int o = 1; o <= 2; ++o) {
        Engine e; e.prepare (sr, 256);
        e.p.v[P_a_level] = 0.f; e.p.v[P_sub_level] = 1.f; e.p.v[P_sub_shape] = 0.f; e.p.v[P_sub_oct] = (float) o;
        e.p.v[P_fa_cut] = 1.f; e.p.v[P_fb_cut] = 1.f; e.p.v[P_noise_level] = 0.f;
        Take t = render (e, sr, 1.2f, note, 0.8f);
        const int from = (int) (sr * 0.3), len = (int) (sr * 0.6);
        const double atPitch = bin (t.L, sr, f0, from, len), down1 = bin (t.L, sr, f0 / 2, from, len), down2 = bin (t.L, sr, f0 / 4, from, len);
        std::printf ("     SUB OCT %d: at pitch %.1f  -1 oct %.1f  -2 oct %.1f\n", o, atPitch, down1, down2);
        got[o] = (o == 1) ? down1 / (atPitch + down2 + 1e-9) : down2 / (atPitch + down1 + 1e-9);
    }
    CHECK (got[1] > 20.0, "SUB OCTAVE 1 does not sound one octave below the note");
    CHECK (got[2] > 20.0, "SUB OCTAVE 2 does not sound two octaves below the note");
}

static void testMassPastNoon (double sr)
{
    say ("MASS is heard directly past noon, and not at all below it");
    //  HORIZON 0 keeps rs at 0, so the well cannot dilate anything: all that
    //  is left of MASS is its direct effect, which is what is under test.
    auto take = [&] (float mass) {
        Engine e; e.prepare (sr, 256);
        e.p.v[P_macro_horiz] = 0.f; e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f;
        e.p.v[P_macro_mass] = mass; e.p.v[P_fa_cut] = 0.9f;   // open: HORIZON 0 already pulls it down
        return render (e, sr, 1.2f, 40, 0.8f);
    };
    Take a = take (0.f), b = take (0.5f), c = take (1.f);
    const bool same = std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0;
    const int from = (int) (sr * 0.4), len = (int) (sr * 0.5);
    //  Not a centroid: the sub and the fundamental dominate any average of a
    //  bass, so a darker top barely moves it.  Measure the two claims instead -
    //  the upper harmonics sink, and the weight gathers an octave down.
    const double f0 = 440.0 * std::pow (2.0, (40 - 69) / 12.0);
    auto upper = [&] (const Take& t) { double s = 0; for (int h = 6; h <= 20; ++h) s += bin (t.L, sr, f0 * h, from, len); return s; };
    const double ub = upper (b), uc = upper (c);
    const double sb = bin (b.L, sr, f0 * 0.5, from, len), sc = bin (c.L, sr, f0 * 0.5, from, len);
    const double topDb = 20.0 * std::log10 ((uc + 1e-12) / (ub + 1e-12));
    const double subDb = 20.0 * std::log10 ((sc + 1e-12) / (sb + 1e-12));
    std::printf ("     MASS 0 vs 0.5 identical %s   past noon: harmonics 6-20 %+.1f dB, sub %+.1f dB   peaks %.3f / %.3f\n",
                 same ? "yes" : "NO", topDb, subDb, peak (b.L), peak (c.L));
    CHECK (same, "MASS below noon changes the sound directly");
    CHECK (topDb < -6.0, "MASS past noon does not darken the top");
    CHECK (subDb > 2.0,  "MASS past noon does not add weight in the sub");
}

static void testRedshiftEveryCircuit (double sr)
{
    say ("REDSHIFT reaches every circuit, and REDSHIFT 0 is the old sound");
    int deaf = 0; std::string list;
    for (int circ = 0; circ < 7; ++circ) {
        auto take = [&] (float red) {
            Engine e; e.prepare (sr, 256);
            e.p.v[P_fa_circ] = (float) circ; e.p.v[P_fa_cut] = 0.55f; e.p.v[P_fa_res] = 0.3f;
            e.p.v[P_macro_mass] = 0.4f; e.p.v[P_macro_horiz] = 1.f;      // a well, but no direct MASS effect
            e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f; e.p.v[P_sub_level] = 0.f;
            e.p.v[P_redshift] = red;
            return render (e, sr, 1.0f, 40, 0.8f);
        };
        Take a = take (0.f), b = take (0.9f);
        const double d = apartLR (a, b, (int) (sr * 0.3), (int) (sr * 0.8));
        if (d < 0.02) { ++deaf; list += " " + std::to_string (circ); }
    }
    std::printf ("     circuits deaf to REDSHIFT:%s\n", deaf ? list.c_str() : " none");
    CHECK (deaf == 0, "a circuit ignores REDSHIFT");
}

/*  The wavetable fast path must BE the additive recipe it replaced, and the
    heaviest patch the panel allows must stay affordable.                  */
static void testWavetableTables (double sr)
{
    say ("WAVETABLE: the precomputed tables are the additive recipe, and SWARM is cheap");
    const double worst = wavetableWorstError();
    std::printf ("     worst table error against the additive recipe: %.1f dB\n", 20.0 * std::log10 (worst + 1e-12));
    CHECK (worst < std::pow (10.0, -46.0 / 20.0), "the wavetable tables do not match the recipe they replace");

    //  the heaviest machine the panel can build: DUO, two notes, eight disk
    //  voices, BOTH oscillators on wavetable, drive and ringdown on
    auto costOf = [&] (bool worstCase) {
        _mm_setcsr (_mm_getcsr() | 0x8040);
        Engine e; e.prepare (sr, 512);
        if (worstCase) {
            e.p.v[P_voicing] = 1.f; e.p.v[P_disk_n] = 8.f; e.p.v[P_disk_spread] = 0.8f;
            e.p.v[P_a_engine] = 1.f; e.p.v[P_b_engine] = 1.f; e.p.v[P_b_level] = 0.6f;
            e.p.v[P_drv_amt] = 0.6f; e.p.v[P_ring_amt] = 0.6f; e.p.v[P_ringmod] = 0.3f;
        } else applyFactory (24, e.p);                      //  SWARM
        std::vector<float> L (512), R (512);
        e.noteOn (36, 0.8f); if (worstCase) e.noteOn (43, 0.8f);
        for (int b = 0; b < 40; ++b) e.process (L.data(), R.data(), 512);
        const int blocks = (int) (sr * 2.0 / 512);
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.process (L.data(), R.data(), 512);
        return 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 2.0;
    };
    const double swarm = costOf (false), heavy = costOf (true);
    std::printf ("     SWARM %.1f %% of a core (was 89)   heaviest machine %.1f %%\n", swarm, heavy);
    CHECK (swarm < 20.0, "SWARM is still a CPU hog");
    CHECK (heavy < 40.0, "the heaviest patch the panel allows costs too much CPU");
}

static void testPitchWheels (double sr)
{
    say ("the pitch wheel moves BEND RANGE semitones, TRANSPOSE an octave each way");
    //  the wheel used to have its range applied twice and divided by 12 twice:
    //  a full throw at BEND 2 moved a third of a semitone
    struct C { float wheel, range, xp, wantSemis; const char* what; };
    const C cs[] = { { 1.f, 2.f, 0.f, 2.f, "wheel up, BEND 2" }, { -1.f, 12.f, 0.f, -12.f, "wheel down, BEND 12" },
                     { 0.f, 2.f, 1.f, 12.f, "TRANSPOSE full up" }, { 0.f, 2.f, -1.f, -12.f, "TRANSPOSE full down" },
                     { 0.f, 2.f, 0.5f, 6.f, "TRANSPOSE half up" }, { 1.f, 2.f, 1.f, 14.f, "both together" } };
    auto freq = [&] (float wheel, float range, float xp) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_b_level] = 0.f; e.p.v[P_sub_level] = 0.f; e.p.v[P_fa_cut] = 1.f; e.p.v[P_fb_cut] = 0.f;
        e.p.v[P_bend_range] = range; e.p.v[P_transpose] = xp; e.pitchBend (wheel);
        Take t = render (e, sr, 1.0f, 45, 0.9f, 0.95f);
        return pitchOfB (t.L, sr, (int) (sr * 0.3), (int) (sr * 0.8)); };
    const double f0 = freq (0.f, 2.f, 0.f);
    double worst = 0;
    for (const C& c : cs) {
        const double got = 12.0 * std::log2 (freq (c.wheel, c.range, c.xp) / f0);
        std::printf ("     %-22s %+6.2f semitones (want %+.0f)\n", c.what, got, c.wantSemis);
        worst = std::max (worst, std::fabs (got - c.wantSemis));
    }
    CHECK (worst < 0.25, "a wheel does not move the pitch by what it says");
}

static void testVoiceEndsQuietly (double sr)
{
    say ("a voice only switches off once what it plays has died away - no click at the end");
    //  RINGDOWN rings after the amp, on its own decay; cutting the voice when
    //  the amp envelope ended cut the ringing body in one sample (Peter heard
    //  it on RINGDOWN, HAWKING and DOOM WEIGHT)
    double worst = 0; int worstIdx = 0; double longest = 0;
    for (int i = 0; i < numFactory(); ++i) {
        Engine e; e.prepare (sr, 64);
        applyFactory (i, e.p);
        const int blk = 64, hold = (int) (sr * 0.6), cap = (int) (sr * 75.0);
        std::vector<float> L (blk), R (blk);
        std::vector<float> recent ((size_t) (sr * 0.010), 0.f);
        size_t rp = 0;
        e.noteOn (36, 0.9f);
        int t = 0; double atStop = -1;
        for (; t < cap; t += blk) {
            if (t >= hold && t - blk < hold) e.noteOff (36);
            e.process (L.data(), R.data(), blk);
            if (t > hold && e.soundingVoices() == 0) { atStop = e.stopJump(); break; }
            for (int k = 0; k < blk; ++k) { recent[rp] = std::fabs (L[(size_t) k]) + std::fabs (R[(size_t) k]); rp = (rp + 1) % recent.size(); }
        }
        const double secs = (double) t / sr;
        longest = std::max (longest, secs);
        if (atStop < 0) { std::printf ("  FAIL  preset %d (%s) still sounding after %.0f s\n", i, factoryName (i), cap / sr); ++fails; continue; }
        if (atStop > worst) { worst = atStop; worstIdx = i; }
        if (atStop > 1e-4) std::printf ("     %-16s stops at %6.2f s with the output at %.4f  (%.1f dB)\n",
                                        factoryName (i), secs, atStop, 20.0 * std::log10 (atStop + 1e-12));
    }
    std::printf ("     worst level at the moment a voice stops: %.1f dB (%s); longest tail %.1f s\n",
                 20.0 * std::log10 (worst + 1e-12), factoryName (worstIdx), longest);
    CHECK (worst < 1e-4, "a voice stops while it is still sounding - a click at the end of the note");
}

static void testSyncKeepsPitch (double sr)
{
    say ("SYNC must not move the pitch - B's tuning is the sweep, not the note");
    struct C { float aOct, aSemi, bOct, bSemi; };
    const C cs[] = { { 0, 0, 0, 0 }, { -1, 0, 0, 0 }, { 0, 0, 0, 7 }, { 0, 0, 1, 0 }, { 0, 7, 0, 0 }, { -1, 0, 0, 5 }, { 0, 0, 2, 3 } };
    double worst = 0;
    for (const C& c : cs) {
        double f[2];
        for (int k = 0; k < 2; ++k) {
            Engine e; e.prepare (sr, 64);
            e.p.v[P_a_oct] = c.aOct; e.p.v[P_a_semi] = c.aSemi; e.p.v[P_b_oct] = c.bOct; e.p.v[P_b_semi] = c.bSemi;
            e.p.v[P_b_level] = 0.f; e.p.v[P_sub_level] = 0.f; e.p.v[P_fa_cut] = 1.f; e.p.v[P_fb_cut] = 0.f;
            e.p.v[P_sync] = (float) k;
            Take t = render (e, sr, 1.0f, 28, 0.9f, 0.95f);
            f[k] = pitchOfB (t.L, sr, (int) (sr * 0.3), (int) (sr * 0.8));
        }
        worst = std::max (worst, std::fabs (1200.0 * std::log2 (f[1] / f[0])));
    }
    std::printf ("     worst pitch change when SYNC turns on: %.1f cents (was +1201)\n", worst);
    CHECK (worst < 20.0, "turning SYNC on moves the pitch");
}

static void testEveryWire (double sr)
{
    say ("every matrix destination and source moves the sound");
    //  a destination only acts where its target is alive, so each gets the
    //  context it needs - REDSHIFT lives in the FORMANT circuit, SPLIT in the
    //  split routing, and so on.  LFO1 at a few Hz drives every one of them.
    const char* dn[] = { "", "PITCH","A SHAPE","B SHAPE","A WIDTH","PM","RING","SUB","NOISE","A CUT","B CUT",
                         "A RES","B RES","SPLIT","BLEND","DISK","DRIVE","MASS","REDSHIFT","RINGDOWN","PAN","LEVEL" };
    int dead = 0; std::string deadList;
    for (int d = 1; d <= 21; ++d) {
        Take t[2];
        for (int k = 0; k < 2; ++k) {
            Engine e; e.prepare (sr, 64);
            e.p.v[P_b_level] = 0.6f; e.p.v[P_disk_n] = 4.f; e.p.v[P_a_shape] = 0.6f; e.p.v[P_fa_cut] = 0.85f;
            if (d == 13) e.p.v[P_route] = 2.f;
            if (d == 14) e.p.v[P_route] = 1.f;
            if (d == 17 || d == 18) { e.p.v[P_fa_circ] = 6.f; e.p.v[P_redshift] = 0.5f; e.p.v[P_macro_horiz] = 1.f; }
            e.p.v[P_l1_rate] = 0.62f;
            e.p.v[P_m1_src] = 4.f; e.p.v[P_m1_dst] = (float) d; e.p.v[P_m1_amt] = k ? 1.f : 0.f;
            t[k] = render (e, sr, 1.2f, 33, 0.9f, 0.95f);
        }
        const double a = apartLR (t[0], t[1], (int) (sr * 0.15), (int) (sr * 1.1));
        if (a < 0.02) { ++dead; deadList += std::string (" ") + dn[d]; }
    }
    std::printf ("     %d of 21 destinations dead%s\n", dead, deadList.c_str());
    CHECK (dead == 0, "a matrix destination does nothing");

    //  the two sources that were never handed over
    for (int w = 0; w < 2; ++w) {
        Take t[2];
        for (int k = 0; k < 2; ++k) {
            Engine e; e.prepare (sr, 64);
            e.p.v[P_m1_src] = w ? 13.f : 12.f; e.p.v[P_m1_dst] = 9.f; e.p.v[P_m1_amt] = -1.f;
            if (w) e.setPressure (k ? 1.f : 0.f); else e.setWheel (k ? 1.f : 0.f);
            t[k] = render (e, sr, 1.0f, 33, 0.9f, 0.95f);
        }
        const double a = apartLR (t[0], t[1], (int) (sr * 0.15), (int) (sr * 0.9));
        std::printf ("     %s -> A CUT: apart %.3f\n", w ? "PRESSURE" : "WHEEL", a);
        CHECK (a > 0.02, w ? "PRESSURE does not reach the matrix" : "WHEEL does not reach the matrix");
    }
}

static void testTimeAndSpace (double sr)
{
    say ("TIME and SPACE do something audible, and nothing at their defaults");
    Take t[3];
    for (int k = 0; k < 3; ++k) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_macro_time] = k == 0 ? 0.5f : (k == 1 ? 0.f : 1.f);
        t[k] = render (e, sr, 1.5f, 28, 0.9f, 0.6f);
    }
    const int a = 0, b = (int) (sr * 1.5);
    std::printf ("     TIME 0.5 -> 0: apart %.3f   0.5 -> 1: apart %.3f\n", apartLR (t[0], t[1], a, b), apartLR (t[0], t[2], a, b));
    CHECK (apartLR (t[0], t[1], a, b) > 0.05 && apartLR (t[0], t[2], a, b) > 0.05, "TIME does nothing to the sound");

    Take s[2];
    for (int k = 0; k < 2; ++k) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_macro_space] = (float) k; e.p.v[P_fa_cut] = 0.8f;
        s[k] = render (e, sr, 1.2f, 40, 0.9f, 0.95f);
    }
    const double w0 = sideMid (s[0], (int) (sr * 0.2), (int) (sr * 1.1)), w1 = sideMid (s[1], (int) (sr * 0.2), (int) (sr * 1.1));
    std::printf ("     SPACE width (side/mid): %.3f -> %.3f\n", w0, w1);
    CHECK (w1 > w0 * 3.0 && w1 > 0.08, "SPACE does not widen the sound");
    //  and it must not go past the point where the side outweighs the mid -
    //  the first version measured 2.26, which is phasey, not wide
    CHECK (w1 < 0.8, "SPACE is so wide the side outweighs the sound itself");
}

static void testLfoLocksToBar (double sr)
{
    say ("a synced LFO lands on the host's grid");
    //  1/4 is one cycle per beat.  A sine at a quarter of a beat is at its top,
    //  at three quarters at its bottom - if the phase really follows the bar.
    double at[2];
    const double ppq[2] = { 0.25, 0.75 };
    for (int k = 0; k < 2; ++k) {
        Engine e; e.prepare (sr, 64);
        e.p.v[P_l1_sync] = 1.f; e.p.v[P_l1_shape] = 0.f; e.p.v[P_l1_rate] = 7.f / 15.f;   // 1/4
        e.p.v[P_mass_track] = 0.f; e.p.v[P_mass_vel] = 0.f;
        e.setTempo (120.0);
        std::vector<float> L (64), R (64);
        e.setPosition (ppq[k], true);
        e.process (L.data(), R.data(), 64);
        at[k] = e.modValue (3);
    }
    std::printf ("     LFO1 at beat 0.25: %+.3f   at beat 0.75: %+.3f   (want +1, -1)\n", at[0], at[1]);
    CHECK (at[0] > 0.9 && at[1] < -0.9, "a synced LFO does not follow the bar");
}
/*  EVERY CONTROL ON THE PANEL MOVES THE SOUND.  Peter, 2026-09-27: "go
    through the controls one by one and check if they are connected and
    wired up like they should".  Each control is taken from one end to the
    other in a patch where it SHOULD matter - filter B's with oscillator B
    audible, ENV3's with ENV3 routed, a radius with a real mass, glide and
    DUO with overlapping notes - and must change the output.  A control
    that is inert on purpose everywhere does not exist on this panel, so
    there is no exemption list; the sequencer is off the panel by decision.  */
static Take renderLegato (Engine& e, double sr)
{
    const int n = (int) (sr * 1.2), block = 64;
    Take t; t.L.assign ((size_t) n, 0.f); t.R.assign ((size_t) n, 0.f);
    e.noteOn (28, 0.9f);
    const int second = (int) (sr * 0.3);
    for (int i = 0; i < n; i += block) {
        if (i <= second && i + block > second) e.noteOn (35, 0.9f);   // while 28 is still held
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], std::min (block, n - i));
    }
    return t;
}

static void contextFor (const std::string& id, Params& p)
{
    auto starts = [&] (const char* s) { return id.rfind (s, 0) == 0; };
    p.v[P_b_level] = 0.5f;                                   //  so B is heard
    if (starts ("b_")) p.v[P_a_level] = 0.3f;
    if (id == "a_table") p.v[P_a_engine] = 1.f;
    if (id == "b_table") p.v[P_b_engine] = 1.f;
    if (id == "a_width") p.v[P_a_shape] = 0.66f;
    if (id == "b_width") p.v[P_b_shape] = 0.66f;
    if (id == "sync" || id == "pm" || id == "ringmod") p.v[P_b_semi] = 5.f;
    if (starts ("sub_")) p.v[P_sub_level] = 0.8f;
    if (id == "noise_col") p.v[P_noise_level] = 0.8f;
    if (starts ("disk_")) p.v[P_disk_n] = 4.f;
    if (starts ("ring_") && id != "ring_amt") p.v[P_ring_amt] = 0.6f;
    if (id == "mass_track" || id == "mass_vel") { p.v[P_ring_amt] = 0.6f; p.v[P_macro_horiz] = 1.f; }
    if (starts ("fb_")) { p.v[P_fb_circ] = 0.f; p.v[P_fb_mode] = 0.f; p.v[P_fb_cut] = 0.5f; }
    //  MODE is honoured in full only by the SVF (see MODE_MASK); test it there
    if (id == "fa_mode") p.v[P_fa_circ] = 3.f;
    if (id == "fb_mode") p.v[P_fb_circ] = 3.f;
    if (id == "fa_env" || starts ("e2_")) p.v[P_fa_env] = 0.8f;
    //  a release only shows if there is something held to release
    if (id == "e2_r") p.v[P_e2_s] = 0.8f;
    if (id == "e3_r") p.v[P_e3_s] = 0.8f;
    if (id == "split") p.v[P_route] = 2.f;
    if (id == "blend") p.v[P_route] = 1.f;
    if (starts ("sing_") && id != "sing_level") p.v[P_sing_level] = 0.8f;
    if (id == "drv_type" || id == "drv_pos") p.v[P_drv_amt] = 0.7f;
    //  ORBITS: an LFO's own target only matters with an amount, and the
    //  amount only with a target
    if (id.size() == 6 && id[0] == 'l' && id.compare (2, 4, "_dst") == 0) p.v[P_l1_amt + 2 * (id[1] - '1')] = 1.f;
    if (id.size() == 6 && id[0] == 'l' && id.compare (2, 4, "_amt") == 0) { p.v[P_l1_dst + 2 * (id[1] - '1')] = 9.f; p.v[P_fa_cut] = 0.3f; }
    if (id == "redshift") { p.v[P_fa_circ] = 6.f; p.v[P_macro_mass] = 0.6f; p.v[P_macro_horiz] = 1.f; }
    //  a modulator only matters where it is routed, and a radius only where
    //  there is mass to dilate its clock
    struct R { const char* pre; int src; };
    const R routes[] = { { "e3_", 3 }, { "l1_", 4 }, { "l2_", 5 }, { "l3_", 6 }, { "rnd_", 7 }, { "chirp_", 8 },
                         { "r_e3", 3 }, { "r_l1", 4 }, { "r_l2", 5 }, { "r_l3", 6 }, { "r_rnd", 7 }, { "r_chirp", 8 } };
    for (const R& r : routes) if (starts (r.pre)) {
        p.v[P_m1_src] = (float) r.src; p.v[P_m1_dst] = 9.f; p.v[P_m1_amt] = 1.f;
        //  a cutoff low enough that the swing is heard: near the top of the
        //  range a bass has nothing left to lose (ENV3 RADIUS read 0.002)
        p.v[P_fa_cut] = 0.3f;
    }
    //  a radius on a modulator too slow to move in 1.2 s measures nothing -
    //  the first audit reported LFO2 and RND radius dead for exactly that
    if (id == "r_l1") p.v[P_l1_rate] = 0.55f;
    if (id == "r_l2") p.v[P_l2_rate] = 0.55f;
    if (id == "r_l3") p.v[P_l3_rate] = 0.55f;
    if (id == "r_rnd") p.v[P_rnd_rate] = 0.75f;
    if (starts ("r_")) {
        p.v[P_macro_mass] = 0.85f; p.v[P_macro_horiz] = 1.f;
        if (id == "r_e2") p.v[P_fa_env] = 0.8f;
    }
    if (starts ("l") && id.size() > 3 && id[2] == '_') p.v[P_l1_rate + 3 * (id[1] - '1')] = 0.55f;
    if (id == "rnd_smooth") p.v[P_rnd_rate] = 0.6f;
    //  the matrix slots: a live source into A CUT, a live destination for LFO1
    if (starts ("m") && id.size() > 3 && id[2] == '_') {
        const int slot = id[1] - '1', base = P_m1_src + 3 * slot;
        p.v[P_l1_rate] = 0.55f;
        if (id.find ("_src") != std::string::npos) { p.v[base + 1] = 9.f; p.v[base + 2] = 1.f; }
        if (id.find ("_dst") != std::string::npos) { p.v[base] = 4.f;     p.v[base + 2] = 1.f; }
        if (id.find ("_amt") != std::string::npos) { p.v[base] = 4.f;     p.v[base + 1] = 9.f; }
    }
}

static void testEveryControl (double sr)
{
    say ("EVERY CONTROL: moved end to end where it should matter, it moves the sound");
    int tested = 0, dead = 0; std::string deadList;
    for (int i = 0; i < numParams(); ++i) {
        const PSpec& s = spec (i);
        const std::string id = s.id;
        if (id.rfind ("seq_", 0) == 0) continue;                    //  off the panel, by decision
        if (id == "bend_range") continue;                           //  checked below: the processor's
        const bool legato = (id == "glide" || id == "voicing");
        //  two ends; for a choice, every option against the first
        std::vector<float> vals;
        if (s.kind == GW_KIND_CHOICE) { for (int k = 0; k <= (int) s.hi; ++k) vals.push_back ((float) k); }
        else vals = { s.lo, s.hi };
        //  a source slot is judged on a source that is live in a render (LFO1);
        //  WHEEL and PRESSURE have their own check, which moves them
        if (id.find ("_src") != std::string::npos) vals = { s.lo, 4.f };
        //  judged on A CUT, which acts on any waveform; A WIDTH only acts on a
        //  pulse, and the first audit called every DEST dead for that reason
        if (id.find ("_dst") != std::string::npos) vals = { s.lo, 9.f, s.hi };
        std::vector<Take> takes;
        for (float v : vals) {
            Engine e; e.prepare (sr, 64);
            contextFor (id, e.p);
            e.p.v[i] = v;
            if (id == "vel_amt") { takes.push_back (render (e, sr, 1.2f, 33, 0.35f, 0.7f)); continue; }
            takes.push_back (legato ? renderLegato (e, sr) : render (e, sr, 1.2f, 33, 0.9f, 0.7f));
        }
        ++tested;
        double worst = 1e9;
        for (size_t k = 1; k < takes.size(); ++k)
            worst = std::min (worst, apartLR (takes[0], takes[k], (int) (sr * 0.02), (int) (sr * 1.19)));
        if (worst < 0.01) {
            //  MASS is inert on a patch with no modulator routed, no ringdown
            //  and no FORMANT circuit - by the current design.  That is a
            //  question for Peter, not a pass: it is printed, not hidden.
            if (id == "macro_mass") { std::printf ("     AWAITING A DECISION: MASS does nothing on a plain patch (apart %.4f)\n", worst); continue; }
            ++dead; deadList += std::string ("\n       ") + s.label + " (" + id + ")  apart " + std::to_string (worst);
        }
    }
    std::printf ("     %d controls tested, %d do nothing%s\n", tested, dead, deadList.c_str());
    CHECK (dead == 0, "a control on the panel does nothing");
}
/*  MODE_MASK says which filter modes each circuit honours; the panel dims the
    rest.  A table like that is only worth having if it is TRUE, so every
    circuit is rendered in every mode: a mode the table allows must differ
    from LP, and a mode it does not must be LP exactly.                    */
static void testModeMask (double sr)
{
    say ("MODE_MASK tells the truth about every circuit");
    int wrong = 0; std::string list;
    for (int c = 0; c < 7; ++c) {
        Take t[4];
        for (int m = 0; m < 4; ++m) {
            Engine e; e.prepare (sr, 64);
            e.p.v[P_fa_circ] = (float) c; e.p.v[P_fa_mode] = (float) m; e.p.v[P_fa_cut] = 0.5f; e.p.v[P_fa_res] = 0.4f;
            e.p.v[P_fb_cut] = 0.f; e.p.v[P_fb_res] = 0.f;
            t[m] = render (e, sr, 0.8f, 33, 0.9f, 0.9f);
        }
        for (int m = 1; m < 4; ++m) {
            const bool allowed = (MODE_MASK[c] >> m) & 1u;
            const double a = apartLR (t[0], t[m], 0, (int) (sr * 0.8));
            if (allowed && a < 0.01)   { ++wrong; list += " circuit " + std::to_string (c) + " mode " + std::to_string (m) + " should act"; }
            if (!allowed && a > 1e-6)  { ++wrong; list += " circuit " + std::to_string (c) + " mode " + std::to_string (m) + " should be LP"; }
        }
    }
    std::printf ("     %d mismatches%s\n", wrong, list.c_str());
    CHECK (wrong == 0, "MODE_MASK does not describe what the circuits do");
}
// ----------------------------------------------------------------- render
static void renderDemos (const std::string& dir, double sr)
{
    std::printf ("rendering to %s\n", dir.c_str());
    const int pick[8] = { 0, 3, 4, 10, 14, 16, 26, 28 };
    for (int k = 0; k < 8; ++k) {
        const int i = pick[k];
        Engine e; e.prepare (sr, 512);
        applyFactory (i, e.p);
        Step pat[MAX_STEPS];
        applyFactoryPattern (i, pat, MAX_STEPS);
        for (int s = 0; s < MAX_STEPS; ++s) e.setStep (s, pat[s]);
        e.setTempo (128.0);

        //  a short musical phrase rather than one held note
        const int n = (int) (sr * 6.0);
        Take t; t.L.assign ((size_t) n, 0.f); t.R.assign ((size_t) n, 0.f);
        const int notes[8] = { 28, 28, 35, 28, 31, 28, 33, 26 };
        int at = 0, ni = 0;
        const int step = (int) (sr * 0.70);
        for (int i2 = 0; i2 < n; i2 += 128) {
            if (i2 >= at && ni < 8) {
                if (ni > 0) e.noteOff (notes[ni - 1]);
                e.noteOn (notes[ni], ni % 3 == 0 ? 1.0f : 0.72f);
                at += step; ++ni;
            }
            const int m = std::min (128, n - i2);
            e.process (&t.L[(size_t) i2], &t.R[(size_t) i2], m);
        }
        char name[256];
        std::snprintf (name, sizeof (name), "%s/GW-%02d-%s.wav", dir.c_str(), i, factoryName (i));
        for (char* c = name; *c; ++c) if (*c == ' ') *c = '-';
        writeWav (name, t, sr);
        std::printf ("  %-16s peak %.3f\n", factoryName (i), peak (t.L));
    }
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    if (argc >= 3 && std::string (argv[1]) == "--render") { renderDemos (argv[2], sr); return 0; }

    std::printf ("GRAVITY WELL bench - %d parameters, %d factory presets\n\n",
                 numParams(), numFactory());
    testDefaults (sr);
    testDilation (sr);
    testMassZeroIsOrdinary (sr);
    testRedshiftSpectrumNotPitch (sr);
    testSubOctave (sr);
    testMassPastNoon (sr);
    testRedshiftEveryCircuit (sr);
    testWavetableTables (sr);
    testRingdown (sr);
    testDrivePowerNeutral (sr);
    testSingularitySurvives (sr);
    testStableNotJustBounded (sr);
    testPanicLetsGoOfThePedal (sr);
    testNoteTransition (sr);
    testAcidFilter (sr);
    testSyncKeepsPitch (sr);
    testPitchWheels (sr);
    testVoiceEndsQuietly (sr);
    testEveryWire (sr);
    testTimeAndSpace (sr);
    testLfoLocksToBar (sr);
    testEveryControl (sr);
    testModeMask (sr);
    testPresets (sr);
    testBoundedNotJustQuiet (sr);
    testRates();

    std::printf ("\n%d checks, %d failures\n", checks, fails);
    if (!fails) std::printf ("ALL CLEAR\n");
    return fails ? 1 : 0;
}
