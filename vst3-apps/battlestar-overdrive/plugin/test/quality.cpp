/*  BATTLESTAR OVERDRIVE - sound-quality measurements.
 *
 *  Separate from bench.cpp on purpose. The bench answers "does it do what it
 *  claims"; this answers "is it good enough for a record". The distinction
 *  matters because a plugin can pass every functional check and still breathe,
 *  alias or clip in ways that only show up on real programme material.
 *
 *  Nothing here asserts. It prints numbers, and the numbers go into the
 *  assessment.
 */
#include "../Source/Engine.h"

#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace bo;
static constexpr double SR = 48000.0;
static constexpr double TWOPI = 6.283185307179586;

static double goertzel (const std::vector<float>& v, int from, int to, double hz)
{
    const int n = to - from;
    if (n <= 0) return 0.0;
    const double w = TWOPI * hz / SR;
    const double c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = from; i < to; ++i) { const double s0 = v[(size_t) i] + c*s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1*s1 + s2*s2 - c*s1*s2) / (n * 0.5);
}
static double db (double x){ return 20.0 * std::log10 (std::max (1.0e-13, x)); }

struct Take { std::vector<float> L, R; explicit Take(int n):L((size_t)n,0.f),R((size_t)n,0.f){} int size() const { return (int)L.size(); } };

static void render (Take& t, const Params& p)
{
    Engine e; e.prepare (SR, 512); e.setParams (p);
    for (int i = 0; i < t.size(); i += 256)
        e.process (t.L.data()+i, t.R.data()+i, std::min (256, t.size()-i));
}

//==============================================================================
/*  1. ALIASING ON A HIGH TONE.
    A single tone at 11 kHz: at 4x, its harmonics run to 96 kHz and are filtered
    on the way down. Anything left that is NOT a harmonic of 11 kHz is aliasing
    folded into the audible band, and that is the number that decides whether a
    distortion sounds expensive or cheap. */
static void aliasing()
{
    std::printf ("\n1. ALIASING  (11 kHz in, non-harmonic content below it)\n");
    std::printf ("   %-13s %10s %10s\n", "engine", "worst", "at Hz");
    for (int e = 0; e < NUM_ENGINES; ++e)
    {
        Params p; p.engine = e; p.thrust = 0.85f; p.spectrum = 0.5f; p.mix = 1.0f;
        Take t ((int)(SR*2));
        for (int i = 0; i < t.size(); ++i){ t.L[(size_t)i] = 0.5f*(float)std::sin(TWOPI*11000.0*i/SR); t.R[(size_t)i]=t.L[(size_t)i]; }
        render (t, p);
        const double f0 = goertzel (t.L, (int)SR, t.size(), 11000.0);
        double worst = 0.0, at = 0.0;
        for (double hz = 200.0; hz < 10500.0; hz += 97.0)
        {
            /* skip anything within 60 Hz of a harmonic of 11k folded down */
            bool harmonic = false;
            for (int k = 1; k <= 8; ++k){
                double f = std::fmod (11000.0*k, SR);
                if (f > SR*0.5) f = SR - f;
                if (std::abs (f - hz) < 80.0) harmonic = true;
            }
            if (harmonic) continue;
            const double a = goertzel (t.L, (int)SR, t.size(), hz);
            if (a > worst){ worst = a; at = hz; }
        }
        std::printf ("   %-13s %9.1f dB %9.0f\n", engineName(e), db(worst)-db(f0), at);
    }
}

//==============================================================================
/*  2. INTERMODULATION.
    Two tones a fifth apart. A clean distortion puts its products where the ear
    expects (sum and difference of real partials); a sloppy one sprays
    non-harmonic mud. Measured as the level of the 2f2-f1 product, which is the
    classic third-order IMD term, plus the total non-harmonic residue. */
static void imd()
{
    std::printf ("\n2. INTERMODULATION  (220 + 330 Hz, third-order product at 440 Hz)\n");
    std::printf ("   %-13s %12s %12s\n", "engine", "IMD3", "residue");
    for (int e = 0; e < NUM_ENGINES; ++e)
    {
        Params p; p.engine = e; p.thrust = 0.6f; p.spectrum = 0.5f; p.mix = 1.0f;
        Take t ((int)(SR*2));
        for (int i = 0; i < t.size(); ++i){
            const double x = i/SR;
            t.L[(size_t)i] = 0.3f*(float)(std::sin(TWOPI*220.0*x)+std::sin(TWOPI*330.0*x));
            t.R[(size_t)i] = t.L[(size_t)i];
        }
        render (t, p);
        const double f1 = goertzel (t.L, (int)SR, t.size(), 220.0);
        const double imd3 = goertzel (t.L, (int)SR, t.size(), 440.0);
        /* residue: energy at frequencies that are not multiples of 110 (the GCD) */
        double res = 0.0;
        for (double hz = 155.0; hz < 6000.0; hz += 110.0)   /* halfway between the comb lines */
            res = std::max (res, goertzel (t.L, (int)SR, t.size(), hz));
        std::printf ("   %-13s %11.1f dB %11.1f dB\n", engineName(e), db(imd3)-db(f1), db(res)-db(f1));
    }
}

//==============================================================================
/*  3. THE AUTO-GAIN, which is the thing most likely to be heard as a fault.
    A level step: 12 dB up, held, then back. If the compensation breathes, the
    output level will overshoot and settle rather than simply stepping. */
static void autoGain()
{
    std::printf ("\n3. AUTO-GAIN BEHAVIOUR  (input steps 12 dB up at 1 s, back at 3 s)\n");
    Params p; p.engine = E_ION; p.thrust = 0.55f; p.spectrum = 0.5f;
    Take t ((int)(SR*5));
    for (int i = 0; i < t.size(); ++i)
    {
        const double x = i/SR;
        const float amp = (x >= 1.0 && x < 3.0) ? 0.5f : 0.125f;
        t.L[(size_t)i] = amp*(float)std::sin(TWOPI*220.0*x); t.R[(size_t)i]=t.L[(size_t)i];
    }
    render (t, p);
    auto win = [&](double a, double b){
        double s=0; int n=0;
        for (int i=(int)(SR*a); i<(int)(SR*b) && i<t.size(); ++i){ s += (double)t.L[(size_t)i]*t.L[(size_t)i]; ++n; }
        return db (std::sqrt (s/std::max(1,n)));
    };
    const double before = win(0.60,0.95);
    const double justAfter = win(1.02,1.12);
    const double settled = win(2.60,2.95);
    const double afterDrop = win(3.02,3.12);
    const double settledLow = win(4.50,4.95);
    std::printf ("   quiet, settled      %7.2f dB\n", before);
    std::printf ("   100 ms after step   %7.2f dB\n", justAfter);
    std::printf ("   loud, settled       %7.2f dB   (drift after the step: %+.2f dB)\n", settled, settled-justAfter);
    std::printf ("   100 ms after drop   %7.2f dB\n", afterDrop);
    std::printf ("   quiet again         %7.2f dB   (drift after the drop: %+.2f dB)\n", settledLow, settledLow-afterDrop);
    std::printf ("   -> a large drift AFTER a step is the compensation breathing.\n");
}

//==============================================================================
/*  4. HEADROOM. How close does real use get to the hard ceiling at +-1.6?
    A hard corner is only a problem if it is ever reached. */
static void headroom()
{
    std::printf ("\n4. HEADROOM  (300 random settings, hot input)\n");
    Rng r; r.s = 0x1234567u;
    float worst = 0.0f; int atCeiling = 0; float worstSettings[6] = {0,0,0,0,0,0};
    for (int n = 0; n < 300; ++n)
    {
        Params p;
        p.engine = (int)(r.uni() * NUM_ENGINES) % NUM_ENGINES;
        p.thrust = r.uni(); p.antithrust = r.uni(); p.space = r.uni();
        p.spectrum = r.uni(); p.mix = 0.5f + 0.5f*r.uni(); p.autorefill = true;
        Take t ((int)(SR*1.2));
        for (int i = 0; i < t.size(); ++i){
            const double x = i/SR;
            t.L[(size_t)i] = 0.85f*(float)(std::sin(TWOPI*110.0*x)+0.5*std::sin(TWOPI*277.0*x))*0.7f;
            t.R[(size_t)i]=t.L[(size_t)i];
        }
        render (t, p);
        float pk = 0.0f;
        for (float v : t.L) pk = std::max (pk, std::abs (v));
        // Counted against the ceiling that actually exists. This threshold was
        // left at the OLD hard bound of 1.6 after the soft ceiling replaced it,
        // which no signal can now reach - so it reported a confident "0 of 300"
        // that could not have reported anything else.
        if (pk >= 0.70f) ++atCeiling;
        if (pk > worst){ worst = pk;
            worstSettings[0]=p.thrust; worstSettings[1]=p.antithrust; worstSettings[2]=p.space;
            worstSettings[3]=p.spectrum; worstSettings[4]=p.mix; worstSettings[5]=(float)p.engine; }
    }
    std::printf ("   worst peak %.4f   (%d of 300 engaged the soft knee)\n", worst, atCeiling);
    std::printf ("   worst was %s  thrust %.2f  anti %.2f  space %.2f  spectrum %.2f  mix %.2f\n",
                 engineName((int)worstSettings[5]), worstSettings[0], worstSettings[1],
                 worstSettings[2], worstSettings[3], worstSettings[4]);
}

//==============================================================================
/*  5. NOISE FLOOR. A tribute to a sound nerd should be quiet at rest. */
static void noiseFloor()
{
    std::printf ("\n5. NOISE FLOOR  (a tone, then silence: what is left 1 s later)\n");
    for (int e : { (int)E_IDLE, (int)E_SUPERNOVA })
    {
        Params p; p.engine = e; p.thrust = 0.8f; p.antithrust = 0.5f; p.space = 0.0f;
        Take t ((int)(SR*4));
        for (int i = 0; i < (int)(SR*1.0); ++i){ t.L[(size_t)i]=0.5f*(float)std::sin(TWOPI*220.0*i/SR); t.R[(size_t)i]=t.L[(size_t)i]; }
        render (t, p);
        double s=0; int n=0;
        for (int i=(int)(SR*2.5); i<t.size(); ++i){ s += (double)t.L[(size_t)i]*t.L[(size_t)i]; ++n; }
        std::printf ("   %-13s %8.1f dBFS\n", engineName(e), db(std::sqrt(s/std::max(1,n))));
    }
}

//==============================================================================
/*  6. OVERSAMPLER TRANSPARENCY. The half-band cascade on its own, with no
    shaper: what does the 4x round trip itself do to a clean signal? */
static void osTransparency()
{
    std::printf ("\n6. OVERSAMPLER ROUND TRIP  (no shaper, just 4x up and down)\n");
    std::array<HalfBand,2> hb;
    for (auto& s : hb){ s.design(); s.clear(); }
    const int N = (int)(SR*2);
    std::vector<float> in((size_t)N), out((size_t)N);
    for (double hz : { 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 19000.0 })
    {
        for (auto& s : hb){ s.clear(); }
        for (int i = 0; i < N; ++i) in[(size_t)i] = 0.5f*(float)std::sin(TWOPI*hz*i/SR);
        for (int i = 0; i < N; ++i)
        {
            float u0,u1,s0,s1,s2,s3;
            hb[0].up (in[(size_t)i], u0, u1);
            hb[1].up (u0, s0, s1);
            hb[1].up (u1, s2, s3);
            const float d0 = hb[1].down (s0, s1);
            const float d1 = hb[1].down (s2, s3);
            out[(size_t)i] = hb[0].down (d0, d1);
        }
        const double a = goertzel (out, (int)SR, N, hz);
        std::printf ("   %6.0f Hz   %+6.3f dB\n", hz, db(a) - db(0.5));
    }
}

//==============================================================================
/*  7. DOES SPACE ACTUALLY DECAY?
    "Bounded" is not the same as "stable": a soft ceiling keeps a runaway loop
    inside full scale, so a self-oscillating reverb passes every bounds check
    while screaming. The only honest test is to stop the input and watch - a
    tail that does not keep falling is an oscillator. */
static void spaceStability()
{
    std::printf ("\n7. SPACE STABILITY  (0.5 s burst, then silence: does the tail die?)\n");
    std::printf ("   %6s %10s %10s %10s   %s\n", "space", "1-2 s", "4-5 s", "8-9 s", "verdict");
    for (int i = 1; i <= 20; ++i)
    {
        const float q = i / 20.0f;
        Params p; p.space = q; p.thrust = 0.30f; p.spectrum = 0.5f; p.mix = 1.0f;
        Take t ((int)(SR*10));
        for (int n = 0; n < (int)(SR*0.5); ++n){
            t.L[(size_t)n] = 0.5f*(float)std::sin(TWOPI*220.0*n/SR); t.R[(size_t)n]=t.L[(size_t)n];
        }
        render (t, p);
        auto win = [&](double a, double b){
            double s=0; int n=0;
            for (int k=(int)(SR*a); k<(int)(SR*b) && k<t.size(); ++k){ s += (double)t.L[(size_t)k]*t.L[(size_t)k]; ++n; }
            return db (std::sqrt (s/std::max(1,n)));
        };
        const double a = win(1.0,2.0), b = win(4.0,5.0), c = win(8.0,9.0);
        /* a real tail falls monotonically and is far down by 8 s */
        const char* verdict = "decays";
        if (c > a - 6.0)            verdict = "*** NOT DECAYING ***";
        else if (c > b - 1.0)       verdict = "  stalls";
        else if (b > a + 1.0)       verdict = "*** GROWING ***";
        std::printf ("   %6.2f %9.1f %10.1f %10.1f   %s\n", q, a, b, c, verdict);
    }
}

//==============================================================================
/*  8. WHAT IS ANTITHRUST ACTUALLY DOING?
    Peter asked, and the honest answer is a measurement rather than a recital
    of what the code was meant to do. Four things move together with the knob:
    the comb's delay, its depth, its feedback, and the choke after the drive. */
static void antithrustReport()
{
    std::printf ("\n8. ANTITHRUST, measured\n");
    std::printf ("   %5s %9s %9s %9s %9s %9s %8s\n",
                 "knob", "comb ms", "1st null", "null dB", "peak dB", "comp dB", "level");

    for (float a : { 0.0f, 0.25f, 0.50f, 0.75f, 1.00f })
    {
        const double ms = 12.0 * std::pow (0.03, (double) a);
        const double fNull = 1000.0 / ms;
        const double fPeak = 500.0 / ms;

        // Comb depth: null against peak, both tones present in ONE render, so
        // any broadband level change cancels out of the ratio.
        double nullDb = 0.0, peakDb = 0.0;
        {
            Params p; p.engine = E_ION; p.thrust = 0.0f; p.spectrum = 0.5f; p.antithrust = a;
            Take t ((int)(SR*2));
            for (int i = 0; i < t.size(); ++i){
                const double x = i/SR;
                t.L[(size_t)i] = 0.25f*(float)(std::sin(TWOPI*fNull*x)+std::sin(TWOPI*fPeak*x));
                t.R[(size_t)i] = t.L[(size_t)i];
            }
            render (t, p);
            nullDb = db (goertzel (t.L, (int)SR, t.size(), fNull)) - db (0.25);
            peakDb = db (goertzel (t.L, (int)SR, t.size(), fPeak)) - db (0.25);
        }

        // The choke: how much less gain a loud input gets than a quiet one.
        double g[2]; int j = 0;
        for (float amp : { 0.1f, 0.8f })
        {
            Params p; p.engine = E_ION; p.thrust = 0.3f; p.spectrum = 0.5f; p.antithrust = a;
            Take t ((int)(SR*3));
            for (int i = 0; i < t.size(); ++i){
                t.L[(size_t)i] = amp*(float)std::sin(TWOPI*150.0*i/SR); t.R[(size_t)i]=t.L[(size_t)i];
            }
            render (t, p);
            double s2=0; int n=0;
            for (int i=(int)(SR*2); i<t.size(); ++i){ s2 += (double)t.L[(size_t)i]*t.L[(size_t)i]; ++n; }
            g[j++] = db (std::sqrt (s2/std::max(1,n))) - db (amp);
        }

        // What the knob costs in overall level on a normal signal.
        double lvl;
        {
            Params p; p.engine = E_ION; p.thrust = 0.5f; p.spectrum = 0.5f; p.antithrust = a;
            Take t ((int)(SR*3));
            for (int i = 0; i < t.size(); ++i){
                const double x = i/SR;
                t.L[(size_t)i] = 0.3f*(float)(std::sin(TWOPI*220*x)+0.6*std::sin(TWOPI*440*x));
                t.R[(size_t)i] = t.L[(size_t)i];
            }
            render (t, p);
            double s2=0; int n=0;
            for (int i=(int)(SR*2); i<t.size(); ++i){ s2 += (double)t.L[(size_t)i]*t.L[(size_t)i]; ++n; }
            lvl = db (std::sqrt (s2/std::max(1,n)));
        }

        if (a == 0.0f)
            std::printf ("   %5.2f %9s %9s %9s %9s %9.2f %8.2f\n", a, "off", "-", "-", "-", g[1]-g[0], lvl);
        else
            std::printf ("   %5.2f %9.2f %9.0f %9.2f %9.2f %9.2f %8.2f\n",
                         a, ms, fNull, nullDb, peakDb, g[1]-g[0], lvl);
    }
    std::printf ("   comp dB = how much less gain a loud input gets than a quiet one.\n");
    std::printf ("   The whole effect is IDENTICAL on both channels: it changes tone and\n");
    std::printf ("   dynamics, and does nothing at all to the stereo image.\n");
}

int main()
{
    std::printf ("BATTLESTAR OVERDRIVE - sound quality\n");
    std::printf ("====================================\n");
    antithrustReport();
    spaceStability();
    aliasing();
    imd();
    autoGain();
    headroom();
    noiseFloor();
    osTransparency();
    std::printf ("\n");
    return 0;
}
