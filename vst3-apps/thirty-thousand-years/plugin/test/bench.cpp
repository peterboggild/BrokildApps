/*  Thirty Thousand Years — the bench.

    Plain C++, no JUCE. Every claim in the design record is a number here.
    A check that cannot fail proves nothing, so most of these compare two
    renders against each other rather than against a hope.

        ttytest              run everything
        ttytest --cost       the cost table only
        ttytest --presets    list the presets and their measured levels
*/
#include "Engine.h"
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <string>

using namespace tty;

static int passed = 0, failed = 0;
static void check (bool ok, const char* what, const char* detail = "")
{
    if (ok) ++passed; else ++failed;
    std::printf ("  %s  %s%s%s\n", ok ? "ok  " : "FAIL", what, detail[0] ? "  -- " : "", detail);
}
static std::string fmt (const char* f, double a, double b = 0, double c = 0, double d = 0) { char b2[256]; std::snprintf (b2, sizeof b2, f, a, b, c, d); return b2; }

struct Take { std::vector<float> L, R; double sr; int n() const { return (int) L.size(); } };

static Take render (Engine& e, double seconds, int block = 256, const std::vector<float>* extL = nullptr)
{
    Take t; t.sr = e.sr; const int n = (int) (seconds * e.sr); t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], m, extL ? &(*extL)[(size_t) i] : nullptr, extL ? &(*extL)[(size_t) i] : nullptr);
    }
    return t;
}
static float rms (const Take& t, int from = 0, int to = -1)
{
    if (to < 0) to = t.n(); double s = 0; int c = 0;
    for (int i = from; i < to; ++i) { s += t.L[(size_t) i] * t.L[(size_t) i] + t.R[(size_t) i] * t.R[(size_t) i]; c += 2; }
    return c ? (float) std::sqrt (s / c) : 0.0f;
}
static float peak (const Take& t) { float p = 0; for (int i = 0; i < t.n(); ++i) p = std::max (p, std::max (std::abs (t.L[(size_t) i]), std::abs (t.R[(size_t) i]))); return p; }
static bool finite (const Take& t) { for (int i = 0; i < t.n(); ++i) if (bad (t.L[(size_t) i]) || bad (t.R[(size_t) i])) return false; return true; }
// Goertzel power at hz over [from,to)
static double goertzel (const Take& t, double hz, int from, int to)
{
    const double w = TAU * hz / t.sr, c = 2.0 * std::cos (w); double s0 = 0, s1 = 0, s2 = 0;
    for (int i = from; i < to; ++i) { const double x = 0.5 * (t.L[(size_t) i] + t.R[(size_t) i]); s0 = x + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return p / std::max (1, to - from);
}
// the strongest frequency in [lo,hi] by a fine Goertzel scan, refined by parabolic interpolation
static double peakHz (const Take& t, double lo, double hi, int from, int to, double stepCents = 10.0)
{
    // a coarse scan, then a 0.5-cent scan over +-1.5 steps: Goertzel evaluates the DFT exactly at any
    // frequency, so no interpolation is needed (a parabola on a log grid was 7 cents off)
    double best = lo, bestP = -1; const double ratio = std::pow (2.0, stepCents / 1200.0);
    for (double f = lo; f <= hi; f *= ratio) { const double p = goertzel (t, f, from, to); if (p > bestP) { bestP = p; best = f; } }
    double fine = best, fineP = bestP; const double r2 = std::pow (2.0, 0.5 / 1200.0);
    for (double f = best * std::pow (ratio, -1.5); f <= best * std::pow (ratio, 1.5); f *= r2) { const double p = goertzel (t, f, from, to); if (p > fineP) { fineP = p; fine = f; } }
    return fine;
}
static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

static void loadPreset (Engine& e, int i)
{
    applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
}
static Engine* fresh (double sr = 48000.0, int preset = -1)
{
    Engine* e = new Engine();
    if (preset >= 0) loadPreset (*e, preset);
    e->prepare (sr, 512);
    return e;
}
// a MASS sine only, everything else off, no space
static void bareSine (Engine& e)
{
    Params& p = e.p;
    p[P_m_o1wave] = 0; p[P_m_o2lvl] = 0; p[P_m_sub] = 0; p[P_m_fmode] = 1; p[P_m_cut] = 1.0f; p[P_m_res] = 0; p[P_m_drift] = 0;
    p[P_m_a_atk] = 0.0f; p[P_m_a_rel] = 0.3f; p[P_m_send] = 0; p[P_e_rv_mix] = 0; p[P_e_distance] = 0; p[P_m_fdrive] = 0; p[P_m_width] = 0.5f;
    p[P_s_on] = 0; p[P_mem_on] = 0; p[P_st_on] = 0; p[P_bassmono_on] = 0;
}

//==============================================================================
static void testTable()
{
    std::printf ("\n[1] the parameter table\n");
    bool uniq = true, inRange = true;
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        const PSpec& s = paramSpec (i);
        for (int j = i + 1; j < NUM_PARAMS; ++j) if (! std::strcmp (s.id, paramSpec (j).id)) uniq = false;
        if (s.def < 0 || s.def > paramMax (s) || bad (s.def)) inRange = false;
        if (s.kind == KP_LIST) { int n = 0; if (listNames (s.id, n) == nullptr || n != (int) s.hi + 1) { inRange = false; std::printf ("    list mismatch: %s (%d names, hi %g)\n", s.id, n, s.hi); } }
    }
    check (uniq, "every parameter id is unique", fmt ("%d params", NUM_PARAMS).c_str());
    check (inRange, "every default is within its range and every LIST has its names");
    check (paramIndex ("m_cut") == P_m_cut && paramIndex ("nonsense") == -1, "paramIndex by id");
}

static void testSilenceAndDrone()
{
    std::printf ("\n[2] silence, drone, notes\n");
    Engine* e = fresh (48000.0, 0);
    Take t = render (*e, 2.0);
    check (finite (t) && peak (t) == 0.0f, "a fresh instance with the drone off is EXACTLY silent", fmt ("peak %g", peak (t)).c_str());
    e->p[P_drone] = 1;
    t = render (*e, 3.0);
    check (rms (t, t.n() / 2) > 0.01f, "the drone switch makes sound", fmt ("rms %.3f", rms (t, t.n() / 2)).c_str());
    e->p[P_drone] = 0;
    t = render (*e, 12.0);
    check (rms (t, t.n() - 4800) < 1.0e-3f, "the drone switch off releases to silence", fmt ("rms %.5f after 12 s", rms (t, t.n() - 4800)).c_str());
    e->noteOn (45, 0.8f); t = render (*e, 2.0);
    const float on = rms (t, t.n() / 2);
    e->noteOff (45); Take t2 = render (*e, 15.0);
    check (on > 0.01f && rms (t2, t2.n() - 4800) < 1.0e-3f, "a note sounds and releases", fmt ("on %.3f, 15 s after off %.5f", on, rms (t2, t2.n() - 4800)).c_str());
    delete e;
}

static void testTuning()
{
    std::printf ("\n[3] tuning, beat, shift\n");
    Engine* e = fresh(); bareSine (*e);
    e->noteOn (45, 0.8f); Take t = render (*e, 2.0);
    const double f = peakHz (t, 80, 160, t.n() / 2, t.n());
    check (std::abs (cents (f, 110.0)) < 2.0, "MASS sine at A2 (note 45) measures 110 Hz", fmt ("%.3f Hz, %.2f cents", f, cents (f, 110.0)).c_str());
    // BEAT in hertz: two sines, beat 0.5 Hz -> envelope period 2 s regardless of note
    e->p[P_m_o2lvl] = 0.8f; e->p[P_m_o2wave] = 0; e->p[P_m_beat] = xunmap (0.5f, 0.02f, 12.0f); e->p[P_m_o2fine] = 0.5f;
    e->noteOff (45); render (*e, 1.0); e->noteOn (45, 0.8f);
    t = render (*e, 8.0);
    // the sum of two sines 0.5 Hz apart: the power beats at 0.5 Hz; measure the envelope period by the spacing of minima
    std::vector<float> env; const int win = 2400;
    for (int i = 0; i + win < t.n(); i += win / 4) { float s = 0; for (int k = 0; k < win; ++k) s += t.L[(size_t) (i + k)] * t.L[(size_t) (i + k)]; env.push_back (std::sqrt (s / win)); }
    std::vector<int> minima; for (size_t i = 40; i + 40 < env.size(); ++i) { bool m = true; for (size_t k = i - 40; k <= i + 40; ++k) if (k != i && env[k] <= env[i]) m = false; float mx = 0; for (size_t k = i - 40; k <= i + 40; ++k) mx = std::max (mx, env[k]); if (m && env[i] < 0.3f * mx) minima.push_back ((int) i); }
    double period = minima.size() >= 2 ? (minima.back() - minima.front()) / double (minima.size() - 1) * (win / 4) / t.sr : 0.0;
    check (std::abs (period - 2.0) < 0.15, "BEAT 0.5 Hz beats once every 2.0 s", fmt ("%.3f s (%.0f minima)", period, (double) minima.size()).c_str());
    // the same beat at a different note stays 0.5 Hz (that is the point of hertz)
    e->noteOff (45); render (*e, 1.0); e->noteOn (57, 0.8f); t = render (*e, 8.0);
    env.clear(); for (int i = 0; i + win < t.n(); i += win / 4) { float s = 0; for (int k = 0; k < win; ++k) s += t.L[(size_t) (i + k)] * t.L[(size_t) (i + k)]; env.push_back (std::sqrt (s / win)); }
    minima.clear(); for (size_t i = 40; i + 40 < env.size(); ++i) { bool m = true; for (size_t k = i - 40; k <= i + 40; ++k) if (k != i && env[k] <= env[i]) m = false; float mx = 0; for (size_t k = i - 40; k <= i + 40; ++k) mx = std::max (mx, env[k]); if (m && env[i] < 0.3f * mx) minima.push_back ((int) i); }
    period = minima.size() >= 2 ? (minima.back() - minima.front()) / double (minima.size() - 1) * (win / 4) / t.sr : 0.0;
    check (std::abs (period - 2.0) < 0.15, "...and still 2.0 s an octave up", fmt ("%.3f s", period).c_str());
    delete e;

    // SIGNAL: a sine table, frequency SHIFT +100 Hz vs PITCH SHIFT +12
    e = fresh(); bareSine (*e); e->p[P_m_on] = 0; e->p[P_s_on] = 1; e->p[P_s_wt1tab] = 0; e->p[P_s_wt1pos] = 0; e->p[P_s_wt2lvl] = 0; e->p[P_s_fmode] = 0; e->p[P_s_a_atk] = 0; e->p[P_s_send] = 0;
    e->p[P_s_shift] = 0.5f + 0.5f * std::cbrt (100.0f / 2000.0f); e->p[P_s_a_rel] = 0.0f;
    e->noteOn (45, 0.8f); t = render (*e, 2.0);
    double f1 = peakHz (t, 150, 260, t.n() / 2, t.n());
    const double pAt210 = goertzel (t, 210.0, t.n() / 2, t.n()), pAt110 = goertzel (t, 110.0, t.n() / 2, t.n());
    check (std::abs (f1 - 210.0) < 2.0 && pAt210 > 30.0 * pAt110, "FREQ SHIFT +100 Hz moves 110 Hz to 210 Hz (a constant, not a ratio)", fmt ("peak %.2f Hz, 210/110 power %.0fx", f1, pAt210 / std::max (1e-12, pAt110)).c_str());
    e->p[P_s_shift] = 0.5f; e->p[P_s_pshift] = 0.5f + 12.0f / 48.0f;
    e->noteOff (45); render (*e, 1.0); e->noteOn (45, 0.8f); t = render (*e, 2.0);
    f1 = peakHz (t, 150, 300, t.n() / 2, t.n());
    check (std::abs (cents (f1, 220.0)) < 3.0, "PITCH SHIFT +12 moves 110 Hz to 220 Hz (a ratio)", fmt ("%.2f Hz", f1).c_str());
    delete e;
}

static void testAdditive()
{
    std::printf ("\n[4] the additive bank\n");
    Engine* e = fresh(); bareSine (*e); e->p[P_m_on] = 0; e->p[P_s_on] = 1; e->p[P_s_wt1lvl] = 0; e->p[P_s_wt2lvl] = 0; e->p[P_s_addlvl] = 1.0f; e->p[P_s_addn] = 8; e->p[P_s_fmode] = 0; e->p[P_s_a_atk] = 0; e->p[P_s_send] = 0;
    e->p[P_s_addspread] = 0.5f; e->p[P_s_addtilt] = 0.5f; e->p[P_s_a_rel] = 0.0f;
    e->noteOn (45, 0.8f); Take t = render (*e, 2.0);
    const int a = t.n() / 2, b = t.n();
    const double h2 = goertzel (t, 220.0, a, b), h3 = goertzel (t, 330.0, a, b), off = goertzel (t, 270.0, a, b);
    check (h2 > 100 * off && h3 > 100 * off, "SPREAD 0: partials are harmonic (220, 330 Hz present; 270 absent)", fmt ("h2/off %.0fx", h2 / std::max (1e-12, off)).c_str());
    e->p[P_s_addspread] = 0.75f;   // s = +0.5 -> ratio_k = k^(1.3): partial 2 at 2^1.3*110 = 270.8 Hz
    e->noteOff (45); render (*e, 0.6f); e->noteOn (45, 0.8f); t = render (*e, 2.0);
    const double p2 = peakHz (t, 240, 300, a, b), f1 = peakHz (t, 90, 130, a, b);
    check (std::abs (cents (p2, 110.0 * std::pow (2.0, 1.3))) < 8.0, "SPREAD +50 %: partial 2 lands at 110 * 2^1.3 = 270.8 Hz", fmt ("%.2f Hz", p2).c_str());
    check (std::abs (cents (f1, 110.0)) < 3.0, "...while FUNDAMENTAL 100 % holds partial 1 at 110 Hz", fmt ("%.2f Hz", f1).c_str());
    e->p[P_s_addfund] = 0.0f; e->p[P_s_addcluster] = 0.0f;
    e->noteOff (45); render (*e, 0.6f); e->noteOn (45, 0.8f); t = render (*e, 2.0);
    check (std::abs (cents (peakHz (t, 90, 130, a, b), 110.0)) < 3.0, "...and with FUNDAMENTAL 0 partial 1 is still 110 Hz (k=1 has no stretch)", "");
    e->p[P_s_addgaps] = 0.999f; e->noteOff (45); render (*e, 0.6f); e->noteOn (45, 0.8f); t = render (*e, 2.0);
    const double g2 = goertzel (t, 270.8, a, b), g1 = goertzel (t, 110.0, a, b);
    check (g1 > 50 * g2, "GAPS 100 % removes every partial but the fundamental", fmt ("p1/p2 %.0fx", g1 / std::max (1e-12, g2)).c_str());
    delete e;
}

static void testStructure()
{
    std::printf ("\n[5] STRUCTURE\n");
    Engine* e = fresh(); bareSine (*e); e->p[P_m_on] = 0; e->p[P_st_on] = 1; e->p[P_st_exc] = 0; e->p[P_st_strikeon] = 1; e->p[P_st_send] = 0; e->p[P_st_damp] = 0.3f; e->p[P_st_rel] = 0.9f;
    e->noteOn (57, 0.8f); Take t = render (*e, 3.0);
    const float early = rms (t, 0, 4800), late = rms (t, 96000, 100800);
    check (early > 0.02f && late < early * 0.5f && late > 1.0e-4f, "a STRIKE on a plate rings and decays", fmt ("0-0.1 s %.4f, 2-2.1 s %.4f", early, late).c_str());
    // friction sustains without a strike
    e->p[P_st_exc] = 2; e->p[P_st_strikeon] = 0; e->p[P_st_sustain] = 0.7f; e->p[P_st_bowforce] = 0.6f;
    e->noteOff (57); render (*e, 2.0); e->noteOn (57, 0.8f); t = render (*e, 6.0);
    const float sus1 = rms (t, 48000, 96000), sus2 = rms (t, 240000, 288000);
    check (sus1 > 0.01f && sus2 > 0.5f * sus1, "FRICTION sustains a bowed plate (5-6 s at least half of 1-2 s)", fmt ("%.4f -> %.4f", sus1, sus2).c_str());
    // the models differ from each other
    e->p[P_st_exc] = 0; e->p[P_st_strikeon] = 1; e->noteOff (57); render (*e, 3.0);
    double centroid[7];
    for (int m = 0; m < 7; ++m)
    {
        e->p[P_st_model] = (float) m; e->noteOn (57, 0.8f); Take tm = render (*e, 1.0); e->noteOff (57); render (*e, 3.0);
        double num = 0, den = 0; for (double f = 60; f < 8000; f *= 1.06) { const double p = goertzel (tm, f, 0, 24000); num += p * f; den += p; }
        centroid[m] = den > 0 ? num / den : 0;
    }
    bool distinct = true; for (int i = 0; i < 7; ++i) for (int j = i + 1; j < 7; ++j) if (std::abs (centroid[i] - centroid[j]) < 0.03 * std::max (centroid[i], centroid[j])) distinct = false;
    check (distinct, "the seven bodies have distinct spectral centroids", fmt ("PLATE %.0f CABLE %.0f BEAM %.0f Hz ...", centroid[0], centroid[1], centroid[2]).c_str());
    // stress fractures
    e->p[P_st_model] = 0; e->p[P_st_stress] = 0.9f; e->p[P_st_exc] = 2; e->p[P_st_strikeon] = 0; e->p[P_st_sustain] = 0.9f; e->p[P_st_bowforce] = 0.9f;
    e->noteOn (57, 1.0f); e->uiFracture = false; t = render (*e, 6.0);
    check (e->uiFracture && finite (t) && peak (t) < 1.5f, "STRESS 90 % under a hard bow fractures, bounded", fmt ("peak %.3f", peak (t)).c_str());
    delete e;
}

static void testMemory()
{
    std::printf ("\n[6] MEMORY\n");
    Engine* e = fresh(); bareSine (*e); e->p[P_m_on] = 0; e->p[P_mem_on] = 1; e->p[P_mem_src] = 0; e->p[P_mem_mode] = 0; e->p[P_mem_send] = 0; e->p[P_mem_atk] = 0; e->p[P_mem_keyfollow] = 0;
    e->noteOn (60, 0.8f); Take t = render (*e, 3.0);
    check (rms (t, t.n() / 2) > 0.005f && finite (t), "grains over the VOICE source make sound", fmt ("rms %.4f, %.0f grains live", rms (t, t.n() / 2), (double) e->uiGrains).c_str());
    e->p[P_mem_mode] = 1; t = render (*e, 3.0);
    check (rms (t, t.n() / 2) > 0.005f && finite (t), "the SPECTRAL path makes sound", fmt ("rms %.4f", rms (t, t.n() / 2)).c_str());
    // freeze: the spectrum stops changing
    e->p[P_mem_freeze] = 1; render (*e, 1.0); Take f1 = render (*e, 0.5); Take f2 = render (*e, 0.5);
    double d = 0, s = 0; for (double hz = 100; hz < 6000; hz *= 1.12) { const double a = goertzel (f1, hz, 0, f1.n()), b = goertzel (f2, hz, 0, f2.n()); d += std::abs (a - b); s += a + b; }
    e->p[P_mem_freeze] = 0; render (*e, 1.0); Take g1 = render (*e, 0.5); Take g2 = render (*e, 0.5);
    double d2 = 0, s2 = 0; for (double hz = 100; hz < 6000; hz *= 1.12) { const double a = goertzel (g1, hz, 0, g1.n()), b = goertzel (g2, hz, 0, g2.n()); d2 += std::abs (a - b); s2 += a + b; }
    check (d / s < 0.5 * d2 / s2, "FREEZE holds the spectrum (frame-to-frame change at least halved)", fmt ("frozen %.3f vs live %.3f", d / s, d2 / s2).c_str());
    // erosion: bandwidth falls
    e->p[P_mem_mode] = 0; e->p[P_mem_erosion] = 0; render (*e, 1.0); Take c0 = render (*e, 2.0);
    e->p[P_mem_erosion] = 1.0f; e->p[P_mem_ero_bw] = 1.0f; e->p[P_mem_ero_drop] = 0; e->p[P_mem_ero_frag] = 0; e->p[P_mem_ero_simp] = 0; render (*e, 1.0); Take c1 = render (*e, 2.0);
    auto hfShare = [] (const Take& tk) { double lo = 0, hi = 0; for (double hz = 80; hz < 350; hz *= 1.05) lo += goertzel (tk, hz, 0, tk.n()); for (double hz = 700; hz < 5000; hz *= 1.05) hi += goertzel (tk, hz, 0, tk.n()); return hi / std::max (1e-12, lo + hi); };
    check (hfShare (c0) > 0.02 && hfShare (c1) < 0.5 * hfShare (c0), "EROSION (bandwidth) removes the top (HF share at least halved)", fmt ("HF share %.3f -> %.3f", hfShare (c0), hfShare (c1)).c_str());
    // capture -> a stable source
    e->p[P_mem_erosion] = 0; e->p[P_mem_src] = 4; e->p[P_mem_capsrc] = 2; e->p[P_ext_mode] = 1;
    std::vector<float> ext ((size_t) (48000 * 2)); for (size_t i = 0; i < ext.size(); ++i) ext[i] = 0.5f * std::sin (TAU * 330.0f * i / 48000.0f);
    e->setCapturing (true); render (*e, 2.0, 256, &ext); e->setCapturing (false);
    Clip snap; snap.data.assign (e->mem.capRing.begin(), e->mem.capRing.begin() + (int) e->mem.capLen); snap.rate = 48000.0;
    e->mem.captured.store (&snap);
    e->p[P_mem_pitch] = 0.5f; render (*e, 1.0); t = render (*e, 2.0);
    const double at330 = goertzel (t, 330.0, 0, t.n()), at200 = goertzel (t, 200.0, 0, t.n());
    check (rms (t) > 0.003f && at330 > 20 * at200, "CAPTURE of the AUX input becomes a source the grains read", fmt ("rms %.4f, 330 Hz %.0fx over 200 Hz", rms (t), at330 / std::max (1e-12, at200)).c_str());
    e->mem.captured.store (nullptr);
    delete e;
}

static void testEnvironment()
{
    std::printf ("\n[7] ENVIRONMENT\n");
    // the feedback loop at its most dangerous settings, 30 s
    Engine* e = fresh (48000.0, 0); e->p[P_drone] = 1;
    e->p[P_e_fb_send] = 1.0f; e->p[P_e_fb_ret] = 1.0f; e->p[P_e_fb_sat] = 1.0f; e->p[P_e_fb_damp] = 0.0f; e->p[P_e_fb_delay] = 0.3f; e->p[P_e_fb_shift] = 0.6f; e->p[P_e_fb_to] = 0;
    e->p[P_m_loop] = 1.0f; e->p[P_st_loop] = 1.0f; e->p[P_e_rv_mix] = 0.8f; e->p[P_e_rv_decay] = 1.0f; e->p[P_e_rv_freeze] = 0;
    Take t = render (*e, 30.0);
    const float r1 = rms (t, 48000 * 5, 48000 * 10), r2 = rms (t, 48000 * 25, 48000 * 30);
    check (finite (t) && peak (t) <= 1.5f && r2 < r1 * 2.5f, "the LOOP at full send/return/saturation for 30 s stays bounded and does not grow", fmt ("peak %.3f, rms 5-10 s %.3f, 25-30 s %.3f", peak (t), r1, r2).c_str());
    e->p[P_drone] = 0; e->p[P_e_fb_send] = 0; e->p[P_m_loop] = 0; e->p[P_st_loop] = 0;
    // space RT60
    delete e; e = fresh(); bareSine (*e); e->p[P_m_send] = 1.0f; e->p[P_e_rv_mix] = 1.0f; e->p[P_e_rv_decay] = xunmap (2.0f, 0.2f, 60.0f); e->p[P_e_rv_damp] = 0.0f; e->p[P_e_rv_early] = 0.0f; e->p[P_e_rv_size] = 0.5f; e->p[P_e_scale] = 0.5f;
    e->noteOn (57, 0.8f); render (*e, 2.0); e->noteOff (57); e->p[P_m_a_rel] = 0.0f; t = render (*e, 4.0);
    const float d1 = rms (t, 48000 * 1, 48000 * 1 + 9600), d2 = rms (t, 48000 * 2, 48000 * 2 + 9600);
    const double dbPerS = 20.0 * std::log10 (std::max (1e-9f, d2) / std::max (1e-9f, d1));
    const double rt60 = -60.0 / dbPerS;
    check (rt60 > 1.2 && rt60 < 3.2, "SPACE decay 2 s measures within 1.2-3.2 s (RT60 from the 1-2 s slope)", fmt ("%.2f s (%.1f dB/s)", rt60, dbPerS).c_str());
    // SCALE changes the tail's structure without changing its amount: early spacing
    e->p[P_e_rv_early] = 1.0f; e->p[P_e_scale] = 0.1f; e->noteOn (57, 0.8f); render (*e, 0.5); e->noteOff (57); Take s1 = render (*e, 0.5);
    e->p[P_e_scale] = 0.9f; e->noteOn (57, 0.8f); render (*e, 0.5); e->noteOff (57); Take s2 = render (*e, 0.5);
    // the last early tap lands at eTap[7]*(0.35+1.3*scale): 0.097*0.48=47 ms vs 0.097*1.52=147 ms after the note ends
    const float eA = rms (s1, 4800, 6000), eB = rms (s2, 4800, 6000), eC = rms (s1, 9600, 10800), eD = rms (s2, 9600, 10800);
    check (eB > eA * 3.0f + 0.001f, "APPARENT SCALE moves the early reflections later (100-125 ms after the note: large scale carries energy, small does not)", fmt ("100-125 ms small %.4f large %.4f; 200-225 ms small %.4f large %.4f", eA, eB, eC, eD).c_str());
    delete e;
    // saturator level match and oversampling
    e = fresh(); bareSine (*e); e->p[P_m_a_atk] = 0; e->p[P_e_a1] = 1; e->p[P_e_sat_drive] = 0.0f;
    e->noteOn (57, 0.8f); render (*e, 1.0); Take a0 = render (*e, 1.0);
    e->p[P_e_sat_drive] = 1.0f; render (*e, 1.0); Take a1 = render (*e, 1.0);
    const double dbDiff = 20.0 * std::log10 (rms (a1) / rms (a0));
    check (std::abs (dbDiff) < 2.5, "SAT DRIVE 0 -> 100 % changes the level by under 2.5 dB (measured level match)", fmt ("%+.2f dB", dbDiff).c_str());
    delete e;
    // aliasing: a 15 kHz sine into the saturator, 1x vs 4x; the 2nd harmonic at 30 kHz folds to 18 kHz at 48 k
    for (int q = 0; q < 3; q += 2)
    {
        e = fresh(); bareSine (*e); e->p[P_quality] = (float) q; e->prepare (48000.0, 512); bareSine (*e); e->p[P_m_a_atk] = 0; e->p[P_e_a1] = 1; e->p[P_e_sat_drive] = 0.8f; e->p[P_e_sat_asym] = 0.9f;
        e->noteOn (127, 1.0f);   // 12544 Hz: the 2nd harmonic 25088 folds to 22912, the 3rd 37632 folds to 10368
        render (*e, 0.5); Take ta = render (*e, 1.0);
        const double fund = goertzel (ta, 12543.85, 0, ta.n()), fold3 = goertzel (ta, 48000.0 - 37631.6, 0, ta.n());
        const double dbFold = 10.0 * std::log10 (fold3 / std::max (1e-20, fund));
        std::printf ("  info  saturator %s: the folded 3rd harmonic sits at %.1f dB below the fundamental\n", q == 0 ? "1x (LIVE)  " : "4x (RENDER)", dbFold);
        if (q == 2) check (dbFold < -40.0, "at RENDER (4x) the folded harmonic is under -40 dB", fmt ("%.1f dB", dbFold).c_str());
        delete e;
    }
    // the ceiling limiter
    e = fresh(); bareSine (*e); e->p[P_volume] = 1.0f; e->p[P_m_o1lvl] = 1.0f; e->p[P_m_gain] = 1.0f; e->p[P_ceiling] = 0.5f;   // -6 dB
    e->noteOn (57, 1.0f); render (*e, 0.5); t = render (*e, 1.0);
    check (peak (t) <= dbGain (-6.0f) * 1.02f, "the ceiling limiter holds a hot signal at -6 dB", fmt ("peak %.3f (ceiling %.3f)", peak (t), dbGain (-6.0f)).c_str());
    delete e;
    // bass mono
    e = fresh(); bareSine (*e); e->p[P_m_a_atk] = 0; e->p[P_bassmono_on] = 1; e->p[P_bassmono] = xunmap (120.0f, 20.0f, 400.0f); e->p[P_m_pan] = 0.0f; e->p[P_outwidth] = 0.5f;
    e->noteOn (33, 0.8f); render (*e, 0.5); t = render (*e, 1.0);
    double side = 0, mid = 0; for (int i = 0; i < t.n(); ++i) { side += (t.L[(size_t) i] - t.R[(size_t) i]) * (t.L[(size_t) i] - t.R[(size_t) i]); mid += (t.L[(size_t) i] + t.R[(size_t) i]) * (t.L[(size_t) i] + t.R[(size_t) i]); }
    check (side < 0.02 * mid, "BASS MONO folds a hard-left 55 Hz to the centre", fmt ("side/mid %.4f", side / std::max (1e-12, mid)).c_str());
    delete e;
}

static void testLifeHistory()
{
    std::printf ("\n[8] LIFE and HISTORY\n");
    Engine* e = fresh(); bareSine (*e);
    // a matrix slot: LFO1 -> m_cut, the effective value must move
    e->life.slots[0].on = true; e->life.slots[0].src = MS_LFO1; e->life.slots[0].dst = P_m_cut; e->life.slots[0].depth = 0.3f;
    e->p[P_l1_rate] = xunmap (2.0f, 0.0008f, 50.0f); e->p[P_m_cut] = 0.5f;
    float lo = 9, hi = -9; e->noteOn (57, 0.8f);
    for (int i = 0; i < 100; ++i) { render (*e, 0.01); lo = std::min (lo, e->eff[P_m_cut]); hi = std::max (hi, e->eff[P_m_cut]); }
    check (hi - lo > 0.4f && hi <= 0.8f + 1e-4f && lo >= 0.2f - 1e-4f, "a matrix slot LFO -> CUTOFF sweeps the effective value +-0.3 around the base", fmt ("%.3f .. %.3f", lo, hi).c_str());
    // range clamp
    e->life.slots[0].lo = 0.0f; e->life.slots[0].hi = 0.1f; lo = 9; hi = -9;
    for (int i = 0; i < 100; ++i) { render (*e, 0.01); lo = std::min (lo, e->eff[P_m_cut]); hi = std::max (hi, e->eff[P_m_cut]); }
    check (lo >= 0.5f - 1e-4f && hi <= 0.6f + 1e-4f, "...and a range clamp 0..0.1 keeps it inside 0.5..0.6", fmt ("%.3f .. %.3f", lo, hi).c_str());
    // HOLD LIFE freezes the modulator
    e->life.slots[0].lo = -1; e->life.slots[0].hi = 1; e->p[P_l_hold] = 1; render (*e, 0.05); const float held = e->eff[P_m_cut]; render (*e, 0.3);
    check (std::abs (e->eff[P_m_cut] - held) < 1e-5f, "HOLD LIFE freezes the modulation state", fmt ("%.4f == %.4f", held, e->eff[P_m_cut]).c_str());
    e->p[P_l_hold] = 0; e->life.slots[0].on = false;
    // macro
    e->p[P_mac_mass] = 1.0f; render (*e, 0.05);
    check (e->eff[P_m_sublvl] > e->p[P_m_sublvl] + 0.3f, "MACRO MASS raises the effective SUB LEVEL", fmt ("%.2f -> %.2f", e->p[P_m_sublvl], e->eff[P_m_sublvl]).c_str());
    e->p[P_mac_mass] = 0;
    // event lane: probability 1 at 4 Hz -> about 4 fires a second
    e->p[P_v1_prob] = 1.0f; e->p[P_v1_rate] = xunmap (4.0f, 0.01f, 20.0f); e->p[P_v1_refract] = xunmap (50.0f, 10.0f, 20000.0f); e->p[P_v1_target] = 5;
    e->life.evt[0].fireCount = 0; render (*e, 5.0); int fires = e->life.evt[0].fireCount;
    check (fires >= 16 && fires <= 24, "an event lane at 4 Hz, chance 100 %, fires about 20 times in 5 s", fmt ("%.0f", (double) fires).c_str());
    // refractory period limits the rate
    e->p[P_v1_refract] = xunmap (1000.0f, 10.0f, 20000.0f); e->life.evt[0].fireCount = 0; render (*e, 5.0); fires = e->life.evt[0].fireCount;
    check (fires >= 4 && fires <= 6, "...and a 1 s refractory period holds it to about 5", fmt ("%.0f", (double) fires).c_str());
    e->p[P_v1_prob] = 0;
    // the network: structure energy -> grain density offset
    /*  HISTORY OFF: this measures the LIFE network, and preset 0 ships four
        scenes with HISTORY armed, so the scene interpolation would quite
        correctly overwrite the l_coupling this test is setting. Set every
        parameter a probe does not mean to test. */
    delete e; e = fresh (48000.0, 0); for (auto& sl : e->life.slots) sl.on = false; e->p[P_h_on] = 0;
    e->p[P_drone] = 1; e->p[P_st_on] = 1; e->p[P_st_exclvl] = 1.0f; e->p[P_l_coupling] = 1.0f; e->p[P_l_autonomy] = 1.0f;
    render (*e, 3.0);
    check (e->eff[P_mem_dens] > e->p[P_mem_dens] + 0.01f && e->life.det[3].energy > 0.03f, "the LIFE NETWORK: STRUCTURE energy raises MEMORY density", fmt ("structure energy %.2f, density %.3f -> %.3f", e->life.det[3].energy, e->p[P_mem_dens], e->eff[P_mem_dens]).c_str());
    e->p[P_l_autonomy] = 0.0f; render (*e, 0.5);
    check (std::abs (e->eff[P_mem_dens] - e->p[P_mem_dens]) < 1e-5f, "...and AUTONOMY 0 removes every network offset", "");
    delete e;
    // HISTORY: two scenes
    e = fresh(); bareSine (*e);
    e->scene[0] = e->p; e->scene[0][P_m_cut] = 0.2f; e->sceneSet[0] = true;
    e->scene[3] = e->p; e->scene[3][P_m_cut] = 0.8f; e->sceneSet[3] = true;
    e->p[P_h_on] = 1; e->p[P_h_pos] = 0.0f; render (*e, 0.3);
    const float c0 = e->eff[P_m_cut]; e->p[P_h_pos] = 1.0f; render (*e, 0.3); const float c1 = e->eff[P_m_cut]; e->p[P_h_pos] = 0.5f; render (*e, 0.3); const float ch = e->eff[P_m_cut];
    check (std::abs (c0 - 0.2f) < 1e-3f && std::abs (c1 - 0.8f) < 1e-3f && std::abs (ch - 0.5f) < 1e-3f, "HISTORY interpolates CUTOFF between AWAKENING and AFTERMATH", fmt ("%.2f / %.2f / %.2f", c0, ch, c1).c_str());
    e->p[P_h_on] = 0; render (*e, 0.3);
    check (std::abs (e->eff[P_m_cut] - e->p[P_m_cut]) < 1e-3f, "HISTORY OFF returns the host value", "");
    // auto mode runs the position
    e->p[P_h_on] = 0; e->p[P_h_pos] = 0.0f; render (*e, 0.05); e->p[P_h_on] = 1; e->p[P_h_mode] = 1; e->p[P_h_dur] = xunmap (2.0f, 1.0f, 1800.0f); e->p[P_h_loop] = 0; render (*e, 1.0);
    const float hp1 = e->historyPos; render (*e, 1.5); const float hp2 = e->historyPos;
    check (hp1 > 0.4f && hp1 < 0.6f && hp2 >= 0.999f, "AUTO HISTORY over 2 s reaches 0.5 at 1 s and stops at 1", fmt ("%.3f then %.3f", hp1, hp2).c_str());
    e->p[P_h_on] = 0; e->p[P_h_pos] = 0.0f; render (*e, 0.05); e->p[P_h_hold] = 1; e->p[P_h_loop] = 1; e->p[P_h_on] = 1; render (*e, 0.5);
    check (e->historyPos < 0.05f, "HOLD HISTORY stops the traversal", fmt ("%.3f", e->historyPos).c_str());
    delete e;
}

static void testPanicDeterminismRates()
{
    std::printf ("\n[9] panic, determinism, rates, blocks\n");
    Engine* e = fresh (48000.0, 0); e->p[P_drone] = 1; render (*e, 2.0);
    e->panic(); Take t = render (*e, 0.2);
    check (rms (t, 4800) == 0.0f, "PANIC fades within 100 ms and is then EXACTLY silent", fmt ("rms after 100 ms %g", rms (t, 4800)).c_str());
    e->p[P_drone] = 0;   // the processor does this
    t = render (*e, 1.0);
    check (peak (t) == 0.0f && e->isStopped(), "...and stays stopped", "");
    e->noteOn (57, 0.8f); t = render (*e, 1.0);
    check (rms (t, t.n() / 2) > 0.005f && ! e->isStopped(), "a note restarts it", fmt ("rms %.3f", rms (t, t.n() / 2)).c_str());
    delete e;
    // determinism: two fresh engines, the same seed, DETERMINISTIC on, transport rolling
    Take ta, tb;
    for (int k = 0; k < 2; ++k)
    {
        Engine* x = fresh (48000.0, 15); x->p[P_determin] = 1; x->p[P_seed] = 42; x->prepare (48000.0, 512); x->p[P_drone] = 1; x->p[P_l_coupling] = 0.8f;
        x->setTransport (120.0, 0.0, true);
        Take tt = render (*x, 6.0);
        (k == 0 ? ta : tb) = tt; delete x;
    }
    double maxd = 0; for (int i = 0; i < ta.n(); ++i) maxd = std::max (maxd, (double) std::abs (ta.L[(size_t) i] - tb.L[(size_t) i]));
    check (maxd == 0.0, "DETERMINISTIC: two renders of the flagship from the same start are bit-identical", fmt ("max |diff| %g over 6 s", maxd).c_str());
    // and with DETERMINISTIC off they are not required to be (free-running): just check both are sane
    // sample rates: tuning holds
    for (double sr : { 44100.0, 88200.0, 96000.0 })
    {
        Engine* x = fresh (sr); bareSine (*x); x->noteOn (45, 0.8f); Take tt = render (*x, 2.0);
        const double f = peakHz (tt, 80, 160, tt.n() / 2, tt.n());
        check (std::abs (cents (f, 110.0)) < 3.0 && finite (tt), fmt ("at %.0f Hz A2 is still 110 Hz", sr).c_str(), fmt ("%.3f Hz", f).c_str());
        delete x;
    }
    // block sizes: 64, 512 and an irregular 37 give the same loudness
    float r[3]; int k = 0;
    for (int bs : { 64, 512, 37 })
    {
        Engine* x = fresh (48000.0, 0); x->p[P_drone] = 1; Take tt = render (*x, 3.0, bs); r[k++] = rms (tt, tt.n() / 2); delete x;
    }
    check (std::abs (20 * std::log10 (r[0] / r[1])) < 1.0 && std::abs (20 * std::log10 (r[2] / r[1])) < 1.0, "block sizes 64 / 512 / 37 agree within 1 dB", fmt ("%.4f %.4f %.4f", r[0], r[1], r[2]).c_str());
    // MPE: a per-channel bend moves only that voice
    Engine* x = fresh(); bareSine (*x); x->p[P_mpe] = 1; x->noteOn (45, 0.8f, 2); x->noteOn (45, 0.8f, 3); x->setBend (0.25f, 2);   // +12 semitones on channel 2
    Take tt = render (*x, 1.5);
    const double p110 = goertzel (tt, 110.0, tt.n() / 2, tt.n()), p220 = goertzel (tt, 220.0, tt.n() / 2, tt.n());
    check (p110 > 1e-4 && p220 > 1e-4 && p110 < 50 * p220 && p220 < 50 * p110, "MPE: a bend on channel 2 moves only that voice (110 and 220 Hz both present)", fmt ("110: %.2e  220: %.2e", p110, p220).c_str());
    delete x;
    // Scala: 5-TET
    x = fresh(); bareSine (*x); const float sc[4] = { 240.0f, 480.0f, 720.0f, 960.0f }; x->setScala (sc, 4, 1200.0f); x->p[P_scale] = 9;
    x->noteOn (63, 0.8f); tt = render (*x, 3.0);
    const double f5 = peakHz (tt, 350, 450, tt.n() / 3, tt.n(), 2.0);
    check (std::abs (cents (f5, 261.6256 * std::pow (2.0, 720.0 / 1200.0))) < 3.0, "SCALA 5-TET: note 63 is three steps of 240 cents above C4", fmt ("%.2f Hz", f5).c_str());
    delete x;
}

static void testRandomAndSoak()
{
    std::printf ("\n[10] random machines, soak, presets\n");
    Rng r; r.seed (2026);
    int bad_ = 0; float worstPeak = 0;
    for (int k = 0; k < 120; ++k)
    {
        Engine* e = fresh (48000.0, k % numPresets());
        for (int i = 0; i < NUM_PARAMS; ++i)
        {
            const PSpec& s = paramSpec (i);
            if (s.flags & F_NS) continue;
            if (paramStepped (s)) { if (r.uni() < 0.3f) e->p[i] = (float) (int) (r.uni() * (paramMax (s) + 0.999f)); }
            else if (r.uni() < 0.5f) e->p[i] = r.uni();
        }
        e->p[P_volume] = 0.5f; e->p[P_drone] = 1; e->p[P_e_rv_freeze] = 0;
        e->noteOn (40 + (int) (r.uni() * 40), 0.9f);
        Take t = render (*e, 2.5, 64 + (int) (r.uni() * 400));
        if (! finite (t) || peak (t) > 1.5f) ++bad_;
        worstPeak = std::max (worstPeak, peak (t));
        delete e;
    }
    check (bad_ == 0, "120 random machines (random valid parameters, drone + a note, random block size) are finite and inside the ceiling", fmt ("worst peak %.3f", worstPeak).c_str());
    // soak: the flagship, dense, 90 s
    Engine* e = fresh (48000.0, 15); e->p[P_drone] = 1; e->p[P_h_on] = 1; e->p[P_h_mode] = 1; e->p[P_h_dur] = xunmap (60.0f, 1.0f, 1800.0f); e->p[P_h_loop] = 1;
    e->p[P_mac_violence] = 0.7f; e->p[P_mac_contam] = 0.6f; e->p[P_mac_life] = 0.8f;
    e->noteOn (45, 0.8f); e->noteOn (52, 0.7f);
    Take t = render (*e, 90.0);
    const float a = rms (t, 48000 * 10, 48000 * 20), b = rms (t, 48000 * 80, 48000 * 90);
    check (finite (t) && peak (t) <= 1.5f && b < 4.0f * a && b > 0.01f, "the flagship with VIOLENCE, CONTAMINATION and LIFE up runs 90 s through a full HISTORY loop: finite, bounded, alive", fmt ("peak %.3f, rms 10-20 s %.3f, 80-90 s %.3f", peak (t), a, b).c_str());
    delete e;
    // every preset: loads, is bounded, makes sound with the drone on or a note played
    int silent = 0, over = 0;
    for (int i = 0; i < numPresets(); ++i)
    {
        Engine* x = fresh (48000.0, i); x->p[P_drone] = 1; x->noteOn (45, 0.8f); x->noteOn (52, 0.8f);
        Take tt = render (*x, 4.0);
        const float rr = std::max (rms (tt, 0, tt.n() / 2), rms (tt, tt.n() / 2)), pk = peak (tt);
        if (rr < 0.002f) { ++silent; std::printf ("    silent: %s (rms %.4f)\n", preset (i).name, rr); }
        if (! finite (tt) || pk > 1.5f) { ++over; std::printf ("    over: %s\n", preset (i).name); }
        delete x;
    }
    check (silent == 0 && over == 0, fmt ("all %.0f presets load, make sound and stay inside the ceiling", (double) numPresets()).c_str(), fmt ("%.0f silent, %.0f over", (double) silent, (double) over).c_str());
}

static float maxStep (const Take& t, int from, int to)
{
    float m = 0.0f;
    for (int i = std::max (1, from); i < to; ++i) m = std::max (m, std::max (std::abs (t.L[(size_t) i] - t.L[(size_t) i - 1]), std::abs (t.R[(size_t) i] - t.R[(size_t) i - 1])));
    return m;
}

static void testClicks()
{
    std::printf ("\n[10b] clicks: a parameter jump mid-note against the settled chain\n");
    struct J { const char* what; int id; float from, to; };
    J jumps[] = { { "CUTOFF 300 Hz -> 5 kHz", P_m_cut, 0.39f, 0.8f }, { "MASS LEVEL -6 -> 0 dB", P_m_gain, 0.5f, 0.7f },
                  { "SPACE AMOUNT 0 -> 60 %", P_e_rv_mix, 0.0f, 0.6f }, { "OSC 2 FINE +3 -> +40 c", P_m_o2fine, 0.53f, 0.7f },
                  { "HISTORY 0 -> 1 (two scenes, cutoff 0.3 -> 0.7)", P_h_pos, 0.0f, 1.0f } };
    for (const J& j : jumps)
    {
        Engine* e = fresh (48000.0, 16);   // COLD START: saws, no space
        e->p[P_m_a_atk] = 0.0f; e->p[j.id] = j.from;
        if (j.id == P_h_pos) { e->scene[0] = e->p; e->scene[0][P_m_cut] = 0.3f; e->sceneSet[0] = true; e->scene[3] = e->p; e->scene[3][P_m_cut] = 0.7f; e->sceneSet[3] = true; e->p[P_h_on] = 1; }
        e->noteOn (45, 0.8f); render (*e, 1.0);
        Take settled = render (*e, 1.0);
        e->p[j.id] = j.to;
        Take jumped = render (*e, 0.1);           // the transition
        Take settled2 = render (*e, 1.0);          // settled at the new value: a brighter signal steps more, legitimately
        const float base = std::max (maxStep (settled, 0, settled.n()), maxStep (settled2, settled2.n() / 2, settled2.n())), after = maxStep (jumped, 0, jumped.n());
        check (after <= base * 2.0f + 0.003f, j.what, fmt ("settled max step %.4f (before/after), across the jump %.4f", base, after).c_str());
        delete e;
    }
}

static void testCost()
{
    std::printf ("\n[11] cost at 48 kHz, one core (this machine)\n");
    struct Case { const char* name; int preset; int notes; float macros; };
    Case cases[] = { { "COLD START, one note", 16, 1, 0 }, { "flagship, drone + 2 notes", 15, 2, 0 }, { "flagship, 8 voices, every macro up", 15, 6, 0.8f }, { "THE SEA IS MADE OF IRON, drone", 6, 0, 0 } };
    for (const auto& c : cases)
    {
        Engine* e = fresh (48000.0, c.preset); e->p[P_drone] = c.preset != 16 ? 1.0f : 0.0f;
        for (int k = 0; k < c.notes; ++k) e->noteOn (40 + k * 5, 0.8f);
        for (int m = 0; m < NUM_MACROS; ++m) e->p[P_mac_mass + m] = c.macros;
        render (*e, 1.0);
        const auto t0 = std::chrono::high_resolution_clock::now();
        render (*e, 10.0, 512);
        const double s = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        std::printf ("  info  %-42s %5.1f %% of a core  (%d voices used)\n", c.name, 100.0 * s / 10.0, e->uiVoicesUsed);
        delete e;
    }
}

static void testBankLevels()
{
    std::printf ("\n[12] the bank against the output stage\n");
    float worstRed = 0.0f, loudest = 0.0f, quietest = 1e9f, hotPeak = 0.0f;
    const char* worstRedName = ""; const char* hotName = "";
    int clipped = 0;
    for (int i = 0; i < numPresets(); ++i)
    {
        /*  From note-on, because a struck gesture is over before a window that
            begins later; 8 s is past every attack in the bank. */
        Engine* e = fresh (48000.0, i); e->p[P_drone] = 1; e->noteOn (45, 0.9f); e->noteOn (52, 0.85f);
        Take t = render (*e, 8.0, 512);
        const int n = t.n(), W = 96000, H = 24000;
        float loud = 0.0f, pk = 0.0f;
        for (int k = 0; k < n; ++k) pk = std::max (pk, std::max (std::abs (t.L[(size_t) k]), std::abs (t.R[(size_t) k])));
        for (int s0 = 0; s0 + W <= n; s0 += H)
        {
            double e2 = 0; for (int k = s0; k < s0 + W; ++k) { const float l = t.L[(size_t) k], r = t.R[(size_t) k]; e2 += l * l + r * r; }
            loud = std::max (loud, (float) std::sqrt (e2 / (2 * W)));
        }
        if (e->limReduction > worstRed) { worstRed = e->limReduction; worstRedName = preset (i).name; }
        if (pk > hotPeak) { hotPeak = pk; hotName = preset (i).name; }
        if (pk >= 1.4999f) ++clipped;
        loudest = std::max (loudest, loud); quietest = std::min (quietest, loud);
        delete e;
    }
    check (worstRed < 0.05f, "the limiter is idle on every factory preset",
           (std::string ("worst ") + std::to_string (worstRed) + " on " + worstRedName).c_str());
    check (clipped == 0, "no preset reaches the output clamp", (std::to_string (clipped) + " did").c_str());
    check (hotPeak < 1.0f, "no preset asks the output stage for more than full scale",
           (std::string ("worst ") + std::to_string (hotPeak) + " on " + hotName).c_str());
    const float spread = 20.0f * std::log10 ((loudest + 1e-9f) / (quietest + 1e-9f));
    check (spread < 26.0f, "the bank's loudness spread is bounded",
           (std::to_string (spread) + " dB").c_str());
    std::printf ("  info  loudest 2 s across the bank: %.4f .. %.4f (%.1f dB)\n", quietest, loudest, spread);
}

static void listPresets()
{
    std::printf ("\n%-40s %-22s %8s %8s\n", "preset", "category", "rms", "peak");
    for (int i = 0; i < numPresets(); ++i)
    {
        Engine* x = fresh (48000.0, i); x->p[P_drone] = 1; x->noteOn (45, 0.8f); x->noteOn (52, 0.8f);
        Take tt = render (*x, 6.0);
        std::printf ("%-40s %-22s %8.4f %8.3f\n", preset (i).name, preset (i).cat, rms (tt, tt.n() / 2), peak (tt));
        delete x;
    }
}

static void dumpParams()
{
    // the page fixture: exactly what initialState carries, so the panel probe hard-codes nothing
    std::printf ("{\"product\":\"Thirty Thousand Years\",\"build\":\"dev\",\"latency\":96,\"memLatency\":2048,\"sr\":48000,\"params\":[");
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        const PSpec& sp = paramSpec (i);
        std::printf ("%s{\"id\":\"%s\",\"v\":%g,\"hi\":%g,\"n\":\"%s\",\"kind\":%d,\"def\":%g,\"lo\":%g,\"fhi\":%g,\"flags\":%d", i ? "," : "", sp.id, sp.def, paramMax (sp), sp.name, sp.kind, sp.def, sp.lo, sp.hi, sp.flags);
        int nn = 0; auto names = listNames (sp.id, nn);
        if (names && nn > 0) { std::printf (",\"names\":["); for (int j = 0; j < nn; ++j) std::printf ("%s\"%s\"", j ? "," : "", names[j]); std::printf ("]"); }
        std::printf ("}");
    }
    std::printf ("],\"presets\":[");
    for (int i = 0; i < numPresets(); ++i) std::printf ("%s{\"n\":%d,\"name\":\"%s\",\"cat\":\"%s\",\"note\":\"%s\"}", i ? "," : "", i, preset (i).name, preset (i).cat, preset (i).note);
    std::printf ("],\"sources\":[");
    for (int i = 0; i < NUM_MODSRC; ++i) std::printf ("%s\"%s\"", i ? "," : "", modSourceName (i));
    std::printf ("],\"macroNames\":[\"MASS\",\"DREAD\",\"VIOLENCE\",\"INSTABILITY\",\"CONTAMINATION\",\"DISTANCE\",\"LIFE\",\"HUMANITY\"]}\n");
}

int main (int argc, char** argv)
{
    if (argc > 1 && ! std::strcmp (argv[1], "--params")) { dumpParams(); return 0; }
    std::printf ("Thirty Thousand Years bench  (%d parameters, %d presets)\n", NUM_PARAMS, numPresets());
    if (argc > 1 && ! std::strcmp (argv[1], "--cost")) { testCost(); return 0; }
    if (argc > 1 && ! std::strcmp (argv[1], "--presets")) { listPresets(); return 0; }
    testTable(); testSilenceAndDrone(); testTuning(); testAdditive(); testStructure(); testMemory(); testEnvironment(); testLifeHistory(); testPanicDeterminismRates(); testRandomAndSoak(); testClicks(); testBankLevels(); testCost();

    /*  A PANIC LETS GO OF THE PEDAL.  Dropping the gates without releasing
        the sustain pedal silences the note that is ringing and leaves the
        next one held for ever by a pedal nobody is pressing -- and the
        panel's PANIC button routes here too, so there is no way out.
        Measured against a control engine that never touched the pedal, so
        this holds whatever the default patch does. */
    {
        double t[2] = { 0, 0 };
        for (int pass = 0; pass < 2; ++pass)
        {
            Engine* e = fresh();
            if (pass == 0) { e->setSustain (true); e->allNotesOff(); }
            e->noteOn (45, 0.8f); render (*e, 0.5);
            e->noteOff (45);      render (*e, 1.0);
            const Take tail = render (*e, 1.0);
            for (size_t i = 0; i < tail.L.size(); ++i) t[pass] = std::max (t[pass], (double) std::abs (tail.L[i]));
        }
        check (t[0] <= t[1] + 1.0e-4, "a panic lets go of the pedal: a note played afterwards still stops");
    }

    std::printf ("\n%d checks, %d failed  %s\n", passed + failed, failed, failed == 0 ? "ALL CLEAR" : "");
    return failed == 0 ? 0 : 1;
}
