/*  Thirty Thousand Years — engine probe: prints numbers, asserts nothing.
        ttyprobe tuning | halfband | sat | grains | modes | friction | signal | hilbert
*/
#include "Engine.h"
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace tty;

struct Take { std::vector<float> L, R; double sr; int n() const { return (int) L.size(); } };
static Take render (Engine& e, double seconds, int block = 256)
{
    Take t; t.sr = e.sr; const int n = (int) (seconds * e.sr); t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += block) { const int m = std::min (block, n - i); e.process (&t.L[(size_t) i], &t.R[(size_t) i], m); }
    return t;
}
static double goertzel (const std::vector<float>& x, double sr, double hz, int from, int to)
{
    const double w = TAU * hz / sr, c = 2.0 * std::cos (w); double s0 = 0, s1 = 0, s2 = 0;
    for (int i = from; i < to; ++i) { s0 = x[(size_t) i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / std::max (1, to - from);
}
static float rms (const std::vector<float>& x, int from = 0, int to = -1) { if (to < 0) to = (int) x.size(); double s = 0; for (int i = from; i < to; ++i) s += x[(size_t) i] * x[(size_t) i]; return (float) std::sqrt (s / std::max (1, to - from)); }
static void bareSine (Engine& e)
{
    Params& p = e.p;
    p[P_m_o1wave] = 0; p[P_m_o2lvl] = 0; p[P_m_sub] = 0; p[P_m_fmode] = 1; p[P_m_cut] = 1.0f; p[P_m_res] = 0; p[P_m_drift] = 0;
    p[P_m_a_atk] = 0.0f; p[P_m_a_rel] = 0.3f; p[P_m_send] = 0; p[P_e_rv_mix] = 0; p[P_e_distance] = 0; p[P_m_fdrive] = 0; p[P_m_width] = 0.5f;
    p[P_s_on] = 0; p[P_mem_on] = 0; p[P_st_on] = 0; p[P_bassmono_on] = 0;
}
static void scan (const Take& t, double lo, double hi, int from, int to, int top = 6)
{
    struct P { double f, p; }; std::vector<P> ps;
    for (double f = lo; f <= hi; f *= std::pow (2.0, 10.0 / 1200.0)) ps.push_back ({ f, goertzel (t.L, t.sr, f, from, to) });
    for (int k = 0; k < top; ++k)
    {
        size_t best = 0; for (size_t i = 1; i < ps.size(); ++i) if (ps[i].p > ps[best].p) best = i;
        if (ps[best].p <= 0) break;
        std::printf ("    %8.2f Hz  %10.3e\n", ps[best].f, ps[best].p);
        for (size_t i = 0; i < ps.size(); ++i) if (std::abs (std::log2 (ps[i].f / ps[best].f)) < 0.04) ps[i].p = 0;
    }
}

int main (int argc, char** argv)
{
    const std::string what = argc > 1 ? argv[1] : "tuning";
    if (what == "tuning")
    {
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            Engine e; bareSine (e); e.prepare (sr, 512); bareSine (e); e.noteOn (57, 0.8f); Take t = render (e, 1.5);
            std::printf ("sr %.0f, note 57, MASS sine: rms %.4f, hzCur %.2f\n", sr, rms (t.L, t.n() / 2), e.voices[0].hzCur);
            scan (t, 40, 2000, t.n() / 2, t.n());
        }
    }
    else if (what == "signal")
    {
        Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_s_on] = 1; e.p[P_s_wt1tab] = 0; e.p[P_s_wt1pos] = 0; e.p[P_s_wt2lvl] = 0; e.p[P_s_fmode] = 0; e.p[P_s_a_atk] = 0; e.p[P_s_send] = 0;
        e.prepare (48000.0, 512); e.noteOn (57, 0.8f); Take t = render (e, 1.5);
        std::printf ("SIGNAL sine table, note 57: rms %.4f\n", rms (t.L, t.n() / 2)); scan (t, 40, 3000, t.n() / 2, t.n());
        e.p[P_s_wt1lvl] = 0; e.p[P_s_addlvl] = 1.0f; e.p[P_s_addn] = 8; e.noteOff (57); render (e, 0.5); e.noteOn (57, 0.8f); t = render (e, 1.5);
        std::printf ("SIGNAL additive 8 partials: rms %.4f\n", rms (t.L, t.n() / 2)); scan (t, 40, 3000, t.n() / 2, t.n(), 9);
    }
    else if (what == "hilbert")
    {
        FreqShifter fs; fs.setHz (100.0f, 48000.0); std::vector<float> x (48000);
        for (int i = 0; i < 48000; ++i) x[(size_t) i] = fs (std::sin (TAU * 440.0f * i / 48000.0f));
        std::printf ("440 Hz sine through the shifter at +100: 340 %.3e   540 %.3e   440 %.3e\n", goertzel (x, 48000, 340, 24000, 48000), goertzel (x, 48000, 540, 24000, 48000), goertzel (x, 48000, 440, 24000, 48000));
    }
    else if (what == "osamp")
    {
        // a 12544 Hz sine through tanh(1.7 x) at 1x / 2x / 4x: the 3rd harmonic (37632) folds to 10368 at 48 k
        for (int factor : { 1, 2, 4 })
        {
            Oversampler os; os.factor = factor; os.reset(); std::vector<float> y (96000);
            for (int i = 0; i < 96000; ++i) { float l = 0.21f * std::sin (TAU * 12543.85f * i / 48000.0f), r = l; os.tick (l, r, [] (float& a, float& b) { a = std::tanh (8.0f * a); b = std::tanh (8.0f * b); }); y[(size_t) i] = l; }
            const double f = goertzel (y, 48000, 12543.85, 48000, 96000), a3 = goertzel (y, 48000, 48000 - 37631.55, 48000, 96000), a2 = goertzel (y, 48000, 48000 - 25087.7, 48000, 96000);
            std::printf ("  factor %d: folded 3rd %.1f dB, folded 2nd %.1f dB below the fundamental\n", factor, 10 * std::log10 (a3 / f), 10 * std::log10 (a2 / f));
        }
    }
    else if (what == "beat")
    {
        Engine e; bareSine (e); e.prepare (48000.0, 512); bareSine (e); e.p[P_m_o2lvl] = 0.8f; e.p[P_m_o2wave] = 0; e.p[P_m_beat] = xunmap (0.5f, 0.02f, 12.0f); e.p[P_m_o2fine] = 0.5f;
        e.noteOn (45, 0.8f); Take t = render (e, 8.0);
        for (int i = 0; i + 4800 <= t.n(); i += 4800) std::printf ("  %.1f s: rms %.4f\n", i / 48000.0, rms (t.L, i, i + 4800));
        std::printf ("  o1 inc %.6f o2 inc %.6f -> %.3f Hz apart\n", e.voices[0].mass.o1[0].inc, e.voices[0].mass.o2[0].inc, (e.voices[0].mass.o2[0].inc - e.voices[0].mass.o1[0].inc) * 48000.0f);
    }
    else if (what == "halfband")
    {
        HalfBand hb; std::printf ("halfband taps: centre %.4f, sum %.4f\n", hb.h[HalfBand::NT / 2], [&] { float s = 0; for (float v : hb.h) s += v; return s; }());
        for (double f : { 1000.0, 10000.0, 20000.0, 24000.0, 28000.0, 32000.0, 36000.0, 40000.0 })
        {
            HalfBand h; std::vector<float> y (48000);
            for (int i = 0; i < 48000; ++i) { const float a = std::sin (TAU * f * (2 * i) / 96000.0f), b = std::sin (TAU * f * (2 * i + 1) / 96000.0f); y[(size_t) i] = h.down (a, b); }
            const double fa = f < 24000 ? f : 48000 - f;
            std::printf ("  %6.0f Hz at 96 k -> %6.0f Hz at 48 k: %.1f dB\n", f, fa, 10 * std::log10 (goertzel (y, 48000, fa, 24000, 48000) / 0.25));
        }
        // the up path: a 1 kHz sine, then down again
        HalfBand h; std::vector<float> y (48000);
        for (int i = 0; i < 48000; ++i) { float a, b; h.up (std::sin (TAU * 1000.0f * i / 48000.0f), a, b); y[(size_t) i] = h.down (a, b); }
        std::printf ("  up then down, 1 kHz: %.2f dB, image at 47 kHz would be at 1 kHz anyway; rms %.4f\n", 10 * std::log10 (goertzel (y, 48000, 1000, 24000, 48000) / 0.25), rms (y, 24000));
    }
    else if (what == "sat")
    {
        for (int q = 0; q < 3; ++q)
        {
            Engine e; bareSine (e); e.p[P_quality] = (float) q; e.prepare (48000.0, 512); bareSine (e); e.p[P_e_a1] = 1;
            e.noteOn (57, 0.8f);
            for (float d : { 0.0f, 0.3f, 0.6f, 1.0f })
            {
                e.p[P_e_sat_drive] = d; render (e, 1.0); Take t = render (e, 1.0);
                std::printf ("quality %d drive %.1f: rms %.4f  lm gain %.3f  ein %.2e eout %.2e\n", q, d, rms (t.L, 0), e.laneA.sat.lm.gain(), e.laneA.sat.lm.ein, e.laneA.sat.lm.eout);
            }
        }
    }
    else if (what == "grains")
    {
        Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_mem_on] = 1; e.p[P_mem_src] = 0; e.p[P_mem_mode] = 0; e.p[P_mem_send] = 0; e.p[P_mem_atk] = 0; e.p[P_mem_keyfollow] = 0;
        e.prepare (48000.0, 512); e.noteOn (60, 0.8f);
        for (int k = 0; k < 6; ++k) { Take t = render (e, 0.5); std::printf ("grains: rms %.4f live %d srcLen %d act %.4f\n", rms (t.L), e.mem.uiGrains, e.mem.uiSrcLen, e.mem.uiActivity); }
        std::printf ("factory VOICE rms %.4f\n", rms (e.mem.factory[0]));
    }
    else if (what == "modes")
    {
        Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_st_on] = 1; e.p[P_st_exc] = 0; e.p[P_st_strikeon] = 1; e.p[P_st_send] = 0; e.p[P_st_damp] = 0.3f;
        e.prepare (48000.0, 512); e.noteOn (57, 0.8f); Take t = render (e, 2.0);
        std::printf ("strike: rms 0-100ms %.4f, 1-1.1s %.4f, nModes %d, g[0] %.5f hz[0] %.1f t60[0] %.2f\n", rms (t.L, 0, 4800), rms (t.L, 48000, 52800), e.voices[0].st.nModes, e.voices[0].st.modes[0].g, e.voices[0].st.modeHz[0], e.voices[0].st.modeT60[0]);
        scan (t, 40, 4000, 0, 48000, 6);
    }
    else if (what == "friction")
    {
        Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_st_on] = 1; e.p[P_st_exc] = 2; e.p[P_st_strikeon] = 0; e.p[P_st_send] = 0; e.p[P_st_damp] = 0.3f; e.p[P_st_sustain] = 0.7f; e.p[P_st_bowforce] = 0.6f;
        e.prepare (48000.0, 512); e.noteOn (57, 0.8f);
        for (int k = 0; k < 8; ++k) { Take t = render (e, 0.5); std::printf ("friction %.1f s: rms %.4f lastOut %.4f\n", 0.5 * (k + 1), rms (t.L), e.voices[0].st.lastOut); }
        Take t = render (e, 1.0); scan (t, 40, 4000, 0, 48000, 6);
    }
    else if (what == "level")
    {
        // every preset with the drone on: output peak, rms, limiter reduction, per-stratum activity, loop energy
        for (int i = 0; i < numPresets(); ++i)
        {
            Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg); e.prepare (48000.0, 512); e.p[P_drone] = 1;
            render (e, 3.0); Take t = render (e, 4.0);
            float pk = 0; for (int k = 0; k < t.n(); ++k) pk = std::max (pk, std::max (std::abs (t.L[(size_t) k]), std::abs (t.R[(size_t) k])));
            std::printf ("%-40s peak %.3f  rms %6.1f dBFS  lim %.2f  strata %.2f %.2f %.2f %.2f  loop %.2f\n", preset (i).name, pk, 20 * std::log10 (rms (t.L) + 1e-9), e.limReduction, e.stratumAct[0], e.stratumAct[1], e.stratumAct[2], e.stratumAct[3], e.loopEnergy);
        }
    }
    else if (what == "stages")
    {
        // every preset, drone on plus two keys: the peak at every internal stage
        std::printf ("%-34s", "preset");
        for (int k = 0; k < Engine::NUM_STAGES; ++k) std::printf ("%10s", Engine::stageName (k));
        std::printf ("%8s\n", "limit");
        for (int i = 0; i < numPresets(); ++i)
        {
            Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
            e.prepare (48000.0, 512); e.p[P_drone] = 1;
            e.noteOn (45, 0.9f); e.noteOn (52, 0.85f);
            render (e, 3.0);
            for (auto& v : e.stagePeak) v = 0.0f;
            render (e, 4.0);
            std::printf ("%-34s", preset (i).name);
            for (int k = 0; k < Engine::NUM_STAGES; ++k) std::printf ("%10.2f", e.stagePeak[k]);
            std::printf ("%8.2f\n", e.limReduction);
        }
    }
    else if (what == "levels")
    {
        /*  The limiter is bypassed, because the question is what the patch asks
            for and not what the limiter allows. 24 s FROM NOTE-ON, so a struck
            gesture is inside the window as well as a drone: the loudest 2 s rms
            is the patch's real loudness either way. */
        const float tgtRms = 0.16f, pkCeil = 0.75f;
        std::printf ("%-34s%9s%9s%9s%9s%8s\n", "preset", "peak", "loud2s", "crest", "trim dB", "trim");
        for (int i = 0; i < numPresets(); ++i)
        {
            float pk = 0.0f, loud = 0.0f;
            /*  A patch's loudness is its LOUDEST SCENE. Measuring only h_pos 0
                asks a piece written to grow over ten minutes how loud it is in
                its first bar, and then offers to boost it 24 dB. */
            for (float hp : { 0.0f, 1.0f })
            {
                Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
                e.prepare (48000.0, 512); e.p[P_drone] = 1; e.out.noLimit = true;
                e.p[P_h_on] = e.sceneSet[1] || e.sceneSet[2] || e.sceneSet[3] ? 1.0f : 0.0f;
                e.p[P_h_pos] = hp; e.p[P_h_hold] = 1.0f;
                e.out.preLimPeak = 0.0f;
                e.noteOn (45, 0.9f); e.noteOn (52, 0.85f);
                Take t = render (e, 14.0);
                const int n = t.n(), W = 96000, H = 12000;
                pk = std::max (pk, e.out.preLimPeak);
                for (int s0 = 0; s0 + W <= n; s0 += H)
                {
                    double e2 = 0; for (int k = s0; k < s0 + W; ++k) { const float l = t.L[(size_t) k], r = t.R[(size_t) k]; e2 += l * l + r * r; }
                    loud = std::max (loud, (float) std::sqrt (e2 / (2 * W)));
                }
            }
            /*  Match on the loudest sustained moment, but never let the peak
                past the ceiling: whichever asks for less attenuation wins. */
            const float dbR = 20 * std::log10 (tgtRms / std::max (1e-5f, loud));
            const float dbP = 20 * std::log10 (pkCeil / std::max (1e-5f, pk));
            /*  +6 dB is as far as the quiet end is lifted: matching it exactly
                would flatten pieces whose quietness is the point. */
            const float trimDb = std::max (-24.0f, std::min (6.0f, std::min (dbR, dbP)));
            std::printf ("%-34s%9.3f%9.4f%9.1f%9.1f%8.4f\n", preset (i).name, pk, loud,
                         20 * std::log10 ((pk + 1e-9f) / (loud + 1e-9f)), trimDb, (trimDb + 24.0f) / 32.0f);
        }
    }
    else if (what == "thd")
    {
        /*  A sine through MASS with the filter wide open and no drive: every
            harmonic here is distortion this instrument invented. */
        struct Case { const char* name; float o1; float o2; int fmode; float cut; float drive; float sub; };
        Case cases[] = {
            { "one sine, SVF wide open",          0.8f, 0.0f, 1, 1.0f, 0.0f, 0.0f },
            { "one sine, LADDER wide open",       0.8f, 0.0f, 0, 1.0f, 0.0f, 0.0f },
            { "one sine at FULL level, LADDER",   1.0f, 0.0f, 0, 1.0f, 0.0f, 0.0f },
            { "two sines at full, LADDER",        1.0f, 1.0f, 0, 1.0f, 0.0f, 0.0f },
            { "two sines + sub at full, LADDER",  1.0f, 1.0f, 0, 1.0f, 0.0f, 1.0f },
            { "two sines at full, DRIVE 100 %",   1.0f, 1.0f, 0, 1.0f, 1.0f, 0.0f },
        };
        std::printf ("MASS: a sine in, harmonics out (the instrument's own distortion)\n");
        for (const Case& c : cases)
        {
            Engine e; bareSine (e); e.prepare (48000.0, 512); bareSine (e);
            e.p[P_m_o1wave] = 0; e.p[P_m_o2wave] = 0;
            e.p[P_m_o1lvl] = c.o1; e.p[P_m_o2lvl] = c.o2; e.p[P_m_fmode] = (float) c.fmode;
            e.p[P_m_cut] = c.cut; e.p[P_m_fdrive] = c.drive; e.p[P_m_res] = 0.0f;
            e.p[P_m_sub] = c.sub > 0 ? 1.0f : 0.0f; e.p[P_m_sublvl] = c.sub;
            e.p[P_m_o2fine] = 0.5f; e.p[P_m_o2semi] = 0.5f;   // both oscillators on the SAME note
            e.noteOn (45, 1.0f); render (e, 1.0); Take t = render (e, 1.0);
            const double f0 = 110.0;
            double fund = goertzel (t.L, 48000, f0, 0, t.n()), harm = 0;
            for (int k = 2; k <= 12; ++k) harm += goertzel (t.L, 48000, f0 * k, 0, t.n());
            float pk = 0; for (int i = 0; i < t.n(); ++i) pk = std::max (pk, std::abs (t.L[(size_t) i]));
            std::printf ("  %-34s THD %6.1f dB   bus peak %.2f   out peak %.2f\n",
                         c.name, 10 * std::log10 ((harm + 1e-20) / (fund + 1e-20)), e.stagePeak[Engine::ST_MASS], pk);
        }

        /*  STRUCTURE: a bowed body at ordinary settings, and how much its own
            output saturator is taking off. */
        std::printf ("\nSTRUCTURE: a bowed plate, and what its output saturator does\n");
        for (float ex : { 0.3f, 0.5f, 0.7f, 1.0f })
        {
            Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_st_on] = 1; e.p[P_st_exc] = 2;
            e.p[P_st_strikeon] = 0; e.p[P_st_send] = 0; e.p[P_st_sustain] = 0.7f;
            e.p[P_st_bowforce] = 0.6f; e.p[P_st_exclvl] = ex; e.p[P_st_damp] = 0.3f;
            e.prepare (48000.0, 512); e.noteOn (45, 0.9f);
            render (e, 2.0); Take t = render (e, 2.0);
            float pk = 0; for (int i = 0; i < t.n(); ++i) pk = std::max (pk, std::abs (t.L[(size_t) i]));
            std::printf ("  EXCITER %.1f: bus peak %5.2f   raw body peak %5.2f   out %.2f\n",
                         ex, e.stagePeak[Engine::ST_STRUCT], e.voices[0].st.rawPeak, pk);
        }

        /*  Bounding the resonance differently is exactly the kind of change that
            trades one fault for another, so it is measured in the same run. */
        std::printf ("\nMASS: self-oscillation, no oscillators at all\n");
        for (int fm : { 0, 1 })
        {
            Engine e; bareSine (e); e.prepare (48000.0, 512); bareSine (e);
            /*  A noiseless digital ladder at k = 4 sits at exactly zero for ever:
                with no input there is nothing to start it, so measuring silence
                measures nothing. A quiet oscillator rings it, which is what a
                player hears as resonance anyway. */
            e.p[P_m_o1lvl] = 0.08f; e.p[P_m_o2lvl] = 0; e.p[P_m_sub] = 0;
            e.p[P_m_fmode] = (float) fm; e.p[P_m_res] = 1.0f; e.p[P_m_cut] = 0.55f; e.p[P_m_fdrive] = 0;
            e.noteOn (45, 1.0f); render (e, 2.0); Take t = render (e, 1.0);
            float pk = 0; for (int i = 0; i < t.n(); ++i) pk = std::max (pk, std::abs (t.L[(size_t) i]));
            Engine e2; bareSine (e2); e2.prepare (48000.0, 512); bareSine (e2);
            e2.p[P_m_o1lvl] = 0.08f; e2.p[P_m_o2lvl] = 0; e2.p[P_m_sub] = 0;
            e2.p[P_m_fmode] = (float) fm; e2.p[P_m_res] = 0.0f; e2.p[P_m_cut] = 0.55f; e2.p[P_m_fdrive] = 0;
            e2.noteOn (45, 1.0f); render (e2, 2.0); Take t2 = render (e2, 1.0);
            std::printf ("  %-8s RESONANCE 100 %%: out peak %5.2f  rms %.4f  lift over RES 0 %+5.1f dB\n",
                         fm == 0 ? "LADDER" : "SVF", pk, rms (t.L),
                         20 * std::log10 ((rms (t.L) + 1e-9) / (rms (t2.L) + 1e-9)));
        }

        /*  SIGNAL: two wavetables at full, which should not distort at all. */
        std::printf ("\nSIGNAL: two tables at full level\n");
        {
            Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_s_on] = 1;
            e.p[P_s_wt1tab] = 0; e.p[P_s_wt1pos] = 0; e.p[P_s_wt1lvl] = 1.0f;
            e.p[P_s_wt2tab] = 0; e.p[P_s_wt2pos] = 0; e.p[P_s_wt2lvl] = 1.0f; e.p[P_s_wt2fine] = 0.5f; e.p[P_s_wt2oct] = 2;
            e.p[P_s_fmode] = 0; e.p[P_s_a_atk] = 0; e.p[P_s_send] = 0;
            e.prepare (48000.0, 512); e.noteOn (45, 1.0f); render (e, 1.0); Take t = render (e, 1.0);
            double fund = goertzel (t.L, 48000, 110.0, 0, t.n()), harm = 0;
            for (int k = 2; k <= 12; ++k) harm += goertzel (t.L, 48000, 110.0 * k, 0, t.n());
            std::printf ("  THD %6.1f dB   bus peak %.2f\n", 10 * std::log10 ((harm + 1e-20) / (fund + 1e-20)), e.stagePeak[Engine::ST_SIGNAL]);
        }
    }
    return 0;
}
