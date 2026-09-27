/*  1984 — the factory patches. Sparse: each names only what differs from
    the defaults, in real units (Hz, ms, cents, semitones, percent), which
    the helpers convert through the parameter table so a patch can never
    hold a value the table cannot. */
#include "Patches.h"
#include <cstring>
#include <functional>

namespace n84
{

namespace
{
    struct Ctx
    {
        Params& p;
        float& ref (const char* id) { const int i = paramIndex (id); return paramSpec (i < 0 ? 0 : i).ref (p); }
        const PSpec& sp (const char* id) { const int i = paramIndex (id); return paramSpec (i < 0 ? 0 : i); }
        void pct (const char* id, float v)   { ref (id) = clamp01 (v * 0.01f); }
        void hz  (const char* id, float f)   { const auto& s = sp (id); ref (id) = clamp01 (std::log (f / s.lo) / std::log (s.hi / s.lo)); }
        void ms  (const char* id, float m)   { const auto& s = sp (id); ref (id) = clamp01 (std::log (m / s.lo) / std::log (s.hi / s.lo)); }
        void sec (const char* id, float m)   { ms (id, m); }
        void glide (const char* id, float m) { ref (id) = m <= 0.0f ? 0.0f : clamp01 (std::log (m / sp (id).lo) / std::log (sp (id).hi / sp (id).lo)); }
        void semi (const char* id, float s)  { ref (id) = clamp01 (0.5f + s / 24.0f); }
        void cent (const char* id, float c)  { ref (id) = clamp01 (0.5f + c / sp (id).lo); }
        void centu (const char* id, float c) { ref (id) = clamp01 (c / sp (id).lo); }
        void bip (const char* id, float b)   { ref (id) = clamp01 (0.5f + b * 0.5f); }
        void list (const char* id, int i)    { ref (id) = (float) i; }
        void sw  (const char* id, bool on)   { ref (id) = on ? 1.0f : 0.0f; }
        void pw  (const char* id, float w)   { ref (id) = clamp01 ((w - 50.0f) / 45.0f); }   // width in percent
        void vol (const char* id, float v)   { ref (id) = clamp01 (v); }
        // a rank in one go: the CS-80 strip
        void rank (const char* P, float saw, float pulse, float pwPct, float tri, float sine, float noise, int feet,
                   float hpfHz, float hpq, float lpfHz, float lpq, int ladder,
                   float il, float al, float fa, float fd, float fr,
                   float va, float vd, float vsus, float vr, float lvl)
        {
            std::string s (P);
            pct ((s + "saw").c_str(), saw); pct ((s + "pulse").c_str(), pulse); pw ((s + "pw").c_str(), pwPct);
            pct ((s + "tri").c_str(), tri); pct ((s + "sine").c_str(), sine); pct ((s + "noise").c_str(), noise);
            list ((s + "oct").c_str(), feet);
            hz ((s + "hpf").c_str(), hpfHz); pct ((s + "hpq").c_str(), hpq); hz ((s + "lpf").c_str(), lpfHz); pct ((s + "lpq").c_str(), lpq);
            list ((s + "fmode").c_str(), ladder);
            bip ((s + "il").c_str(), il); bip ((s + "al").c_str(), al);
            ms ((s + "fa").c_str(), fa); ms ((s + "fd").c_str(), fd); ms ((s + "fr").c_str(), fr);
            ms ((s + "va").c_str(), va); ms ((s + "vd").c_str(), vd); pct ((s + "vs").c_str(), vsus); ms ((s + "vr").c_str(), vr);
            pct ((s + "lvl").c_str(), lvl);
        }
        void tape (int mode, float wow, float wowHz, float flut, float sat, float age, float drop, float hiss)
        {
            list ("tape_mode", mode); pct ("tape_wow", wow); hz ("tape_wowrate", wowHz); pct ("tape_flut", flut);
            pct ("tape_sat", sat); pct ("tape_age", age); pct ("tape_drop", drop); pct ("tape_hiss", hiss);
        }
        void hall (float mix, float preMs, float size, float rt, float dampHz, float mod, float shim)
        {
            pct ("hall_mix", mix); ms ("hall_pre", preMs); pct ("hall_size", size); sec ("hall_decay", rt);
            hz ("hall_damp", dampHz); pct ("hall_mod", mod); pct ("hall_shim", shim);
        }
        void ensemble (int mode, float rate, float depth, float mix) { list ("ens_mode", mode); pct ("ens_rate", rate); pct ("ens_depth", depth); pct ("ens_mix", mix); }
        void vibrato (float hz, float depthPct, float delayMs) { hz_ ("lfo_rate", hz); pct ("lfo_pitch", depthPct); glide ("lfo_delay", delayMs); }
        void hz_ (const char* id, float f) { hz (id, f); }
    };

    struct Patch { const char* name; const char* cat; void (*fn) (Ctx&); };

    // ---------------------------------------------------------------- BRASS
    void bladeBrass (Ctx& c)
    {
        c.rank ("a_", 90, 30, 60, 0, 30, 0, 2, 10, 0, 2600, 20, 0, -0.4f, 0.45f, 130, 900, 400, 55, 400, 85, 500, 85);
        c.rank ("b_", 80, 0, 50, 0, 20, 0, 2, 10, 0, 2200, 15, 0, -0.3f, 0.4f, 180, 1100, 400, 70, 400, 85, 500, 70);
        c.cent ("b_fine", 9); c.pct ("a_velb", 50); c.pct ("b_velb", 50); c.pct ("a_vel", 45); c.pct ("b_vel", 45);
        c.vibrato (5.4f, 16, 700); c.pct ("at_brill", 55); c.pct ("at_lfo", 35);
        c.ensemble (3, 50, 50, 42); c.tape (1, 25, 0.5f, 20, 35, 30, 0, 15); c.hall (30, 22, 70, 2.4f, 5500, 30, 0);
        c.pct ("vintage", 40); c.pct ("spread", 55);
    }
    void chariotsBrass (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 3000, 35, 0, -0.6f, 0.5f, 220, 1400, 600, 120, 600, 80, 700, 85);
        c.rank ("b_", 100, 45, 55, 0, 0, 0, 1, 10, 0, 2400, 30, 1, -0.5f, 0.4f, 260, 1400, 600, 160, 600, 80, 700, 75);
        c.cent ("b_fine", -6); c.pct ("a_velb", 60); c.pct ("b_velb", 60);
        c.vibrato (5.0f, 14, 900); c.pct ("at_brill", 60);
        c.ensemble (3, 45, 45, 35); c.tape (1, 30, 0.4f, 25, 45, 35, 0, 20); c.hall (36, 35, 80, 3.2f, 4500, 35, 0);
        c.pct ("vintage", 45);
    }
    void tightBrass (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 40, 0, 1800, 30, 1, -0.4f, 0.6f, 70, 500, 300, 25, 300, 75, 350, 85);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 40, 0, 1800, 30, 1, -0.4f, 0.6f, 70, 500, 300, 25, 300, 75, 350, 85);
        c.cent ("b_fine", 7); c.pct ("a_velb", 70); c.pct ("b_velb", 70); c.pct ("a_vel", 60); c.pct ("b_vel", 60);
        c.ensemble (1, 50, 40, 30); c.tape (1, 15, 0.6f, 25, 40, 25, 0, 10); c.hall (22, 15, 55, 1.6f, 6000, 25, 0);
    }
    void frenchHorns (Ctx& c)
    {
        c.rank ("a_", 70, 40, 75, 0, 40, 0, 2, 10, 0, 1400, 10, 0, -0.5f, 0.3f, 300, 1500, 700, 220, 500, 90, 900, 85);
        c.rank ("b_", 70, 40, 72, 0, 40, 0, 1, 10, 0, 1100, 10, 0, -0.5f, 0.25f, 350, 1500, 700, 260, 500, 90, 900, 60);
        c.cent ("b_fine", 5); c.pct ("a_velb", 40); c.pct ("b_velb", 40);
        c.vibrato (4.6f, 10, 1200); c.ensemble (3, 40, 40, 30); c.tape (1, 35, 0.35f, 15, 30, 40, 0, 20); c.hall (45, 40, 90, 4.5f, 4000, 40, 0);
    }
    // -------------------------------------------------------------- STRINGS
    void cs80Strings (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 120, 0, 2200, 10, 0, 0.0f, 0.3f, 400, 2000, 900, 350, 800, 85, 900, 80);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 120, 0, 1800, 10, 0, 0.0f, 0.25f, 500, 2000, 900, 450, 800, 85, 900, 60);
        c.cent ("b_fine", 8); c.pct ("a_pwm", 0); c.vibrato (5.2f, 9, 1400);
        c.ensemble (3, 50, 60, 55); c.tape (1, 30, 0.4f, 25, 25, 35, 0, 20); c.hall (40, 30, 80, 3.5f, 5000, 40, 0);
        c.pct ("vintage", 45); c.pct ("spread", 70);
    }
    void vp330Strings (Ctx& c)
    {
        c.rank ("a_", 100, 40, 62, 0, 0, 0, 2, 200, 0, 3500, 0, 0, 0.0f, 0.0f, 300, 1000, 500, 250, 500, 100, 700, 75);
        c.rank ("b_", 100, 40, 58, 0, 0, 0, 3, 200, 0, 3000, 0, 0, 0.0f, 0.0f, 300, 1000, 500, 250, 500, 100, 700, 50);
        c.cent ("b_fine", -4);
        c.ensemble (3, 55, 75, 70); c.tape (1, 25, 0.5f, 30, 20, 40, 0, 25); c.hall (30, 25, 65, 2.6f, 5500, 30, 0);
        c.pct ("vintage", 50); c.pct ("spread", 80);
    }
    void cathedralStrings (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 150, 0, 1600, 10, 0, -0.2f, 0.2f, 900, 3000, 2500, 800, 800, 90, 2500, 75);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 3, 150, 0, 2400, 10, 0, -0.2f, 0.2f, 1200, 3000, 2500, 1100, 800, 90, 2500, 45);
        c.cent ("b_fine", 6);
        c.ensemble (4, 45, 70, 65); c.tape (1, 30, 0.3f, 20, 20, 45, 0, 25); c.hall (55, 60, 100, 7.0f, 3800, 45, 15);
        c.pct ("vintage", 45); c.pct ("spread", 85);
    }
    void solinaRoom (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 300, 0, 4500, 0, 0, 0.0f, 0.0f, 200, 800, 400, 180, 400, 100, 600, 80);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 300, 0, 3000, 0, 0, 0.0f, 0.0f, 200, 800, 400, 180, 400, 100, 600, 60);
        c.cent ("b_fine", 5);
        c.ensemble (3, 60, 80, 80); c.tape (1, 35, 0.6f, 35, 25, 45, 5, 30); c.hall (20, 12, 45, 1.2f, 6000, 20, 0);
        c.pct ("vintage", 55);
    }
    // ---------------------------------------------------------------- CHOIR
    void humanVoice (Ctx& c)
    {
        c.rank ("a_", 20, 100, 66, 0, 0, 0, 2, 100, 0, 5000, 0, 0, 0.0f, 0.0f, 250, 1000, 600, 350, 500, 100, 800, 80);
        c.rank ("b_", 20, 100, 60, 0, 0, 0, 3, 100, 0, 4000, 0, 0, 0.0f, 0.0f, 250, 1000, 600, 350, 500, 100, 800, 55);
        c.cent ("b_fine", 7);
        c.pct ("choir_mix", 85); c.pct ("choir_vowel", 55); c.bip ("choir_reg", 0.0f); c.pct ("choir_air", 30);
        c.ensemble (3, 50, 65, 65); c.tape (1, 25, 0.5f, 25, 20, 40, 0, 25); c.hall (35, 30, 75, 3.0f, 5000, 35, 0);
        c.pct ("vintage", 40); c.pct ("spread", 70);
    }
    void vangelisChoir (Ctx& c)
    {
        c.rank ("a_", 40, 100, 70, 0, 0, 0, 2, 120, 0, 6000, 0, 0, 0.0f, 0.0f, 700, 2000, 1500, 900, 800, 100, 2000, 80);
        c.rank ("b_", 40, 100, 64, 0, 0, 0, 1, 120, 0, 3500, 0, 0, 0.0f, 0.0f, 900, 2000, 1500, 1200, 800, 100, 2000, 50);
        c.cent ("b_fine", -5);
        c.pct ("choir_mix", 90); c.pct ("choir_vowel", 40); c.bip ("choir_reg", -0.15f); c.pct ("choir_air", 40);
        c.vibrato (4.4f, 8, 1800);
        c.ensemble (4, 40, 70, 70); c.tape (1, 30, 0.35f, 20, 20, 45, 0, 25); c.hall (55, 50, 100, 8.0f, 3500, 45, 25);
        c.pct ("vintage", 45); c.pct ("spread", 90);
    }
    void boysChoir (Ctx& c)
    {
        c.rank ("a_", 30, 100, 62, 0, 0, 0, 3, 200, 0, 7000, 0, 0, 0.0f, 0.0f, 400, 1500, 900, 500, 600, 100, 1200, 75);
        c.rank ("b_", 30, 100, 58, 0, 0, 0, 3, 200, 0, 7000, 0, 0, 0.0f, 0.0f, 400, 1500, 900, 500, 600, 100, 1200, 60);
        c.cent ("b_fine", 9);
        c.pct ("choir_mix", 90); c.pct ("choir_vowel", 78); c.bip ("choir_reg", 0.5f); c.pct ("choir_air", 50);
        c.ensemble (3, 55, 60, 60); c.tape (1, 20, 0.5f, 25, 15, 35, 0, 20); c.hall (50, 40, 90, 5.0f, 4500, 40, 20);
        c.pct ("spread", 80);
    }
    // ----------------------------------------------------------------- LEAD
    void bladeLead (Ctx& c)
    {
        c.list ("mode", 3); c.glide ("glide", 90); c.sw ("legato", true);
        c.rank ("a_", 100, 0, 50, 0, 30, 0, 2, 10, 0, 2600, 25, 0, -0.1f, 0.5f, 40, 700, 300, 15, 500, 80, 400, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 2600, 25, 0, -0.1f, 0.5f, 40, 700, 300, 15, 500, 80, 400, 70);
        c.cent ("b_fine", 11); c.pct ("a_velb", 60); c.pct ("b_velb", 60);
        c.vibrato (5.6f, 20, 500); c.pct ("at_lfo", 60); c.pct ("at_brill", 50); c.pct ("wheel_lfo", 60);
        c.ensemble (1, 50, 45, 35); c.tape (1, 20, 0.5f, 20, 45, 30, 0, 15); c.hall (30, 40, 70, 2.8f, 5000, 30, 0);
    }
    void syncLead (Ctx& c)
    {
        c.list ("mode", 3); c.glide ("glide", 60); c.sw ("legato", true);
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 6000, 15, 1, 0.0f, 0.2f, 20, 600, 300, 10, 400, 85, 300, 80);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 6000, 15, 1, 0.9f, 0.9f, 20, 1400, 500, 10, 400, 85, 300, 0);
        c.sw ("b_sync", true); c.semi ("a_semi", 7);       // rank I is the slave: the audible, swept one
        c.bip ("pm_envpitch", 0.7f); c.pct ("a_velb", 40);
        c.vibrato (5.5f, 12, 600); c.pct ("at_lfo", 50);
        c.list ("drv_mode", 1); c.pct ("drv_amt", 25);
        c.ensemble (1, 50, 30, 25); c.tape (1, 15, 0.5f, 20, 40, 25, 0, 10); c.hall (28, 30, 60, 2.0f, 6000, 30, 0);
    }
    void screamingLead (Ctx& c)
    {
        c.list ("mode", 2); c.centu ("unidet", 14); c.glide ("glide", 40);
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 60, 0, 900, 55, 1, -0.2f, 0.9f, 25, 900, 400, 10, 500, 85, 400, 85);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 60, 0, 900, 55, 1, -0.2f, 0.9f, 25, 900, 400, 10, 500, 85, 400, 70);
        c.cent ("b_fine", 4); c.pct ("a_velb", 60);
        c.list ("drv_mode", 3); c.pct ("drv_amt", 45); c.bip ("drv_tone", 0.2f);
        c.vibrato (6.0f, 18, 400); c.pct ("at_lfo", 60);
        c.tape (1, 20, 0.6f, 25, 50, 30, 0, 15); c.hall (25, 20, 60, 1.8f, 5500, 25, 0);
    }
    // ----------------------------------------------------------------- BASS
    void vintageBass (Ctx& c)
    {
        c.list ("mode", 3); c.sw ("legato", true);
        c.rank ("a_", 100, 30, 55, 0, 60, 0, 2, 10, 0, 500, 35, 1, 0.2f, 0.9f, 8, 350, 200, 4, 300, 70, 220, 90);
        c.rank ("b_", 100, 0, 50, 0, 60, 0, 1, 10, 0, 400, 30, 1, 0.2f, 0.9f, 8, 350, 200, 4, 300, 70, 220, 65);
        c.cent ("b_fine", 4); c.pct ("a_velb", 70); c.pct ("b_velb", 70); c.pct ("a_vel", 50); c.pct ("b_vel", 50);
        c.list ("drv_mode", 2); c.pct ("drv_amt", 30);
        c.tape (1, 10, 0.5f, 15, 45, 20, 0, 10); c.hall (8, 10, 40, 1.0f, 5000, 20, 0);
    }
    void moogishBass (Ctx& c)
    {
        c.list ("mode", 3); c.sw ("legato", true); c.glide ("glide", 30);
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 350, 45, 1, 0.4f, 1.0f, 5, 260, 150, 3, 250, 60, 200, 100);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 10, 0, 350, 45, 1, 0.4f, 1.0f, 5, 260, 150, 3, 250, 60, 200, 60);
        c.cent ("b_fine", -3); c.pct ("a_velb", 80); c.pct ("b_velb", 80);
        c.tape (1, 8, 0.5f, 10, 40, 15, 0, 5); c.hall (5, 10, 40, 0.8f, 5000, 20, 0);
    }
    void pulseBass (Ctx& c)
    {
        c.list ("mode", 3); c.sw ("legato", true);
        c.rank ("a_", 20, 100, 70, 0, 50, 0, 2, 10, 0, 700, 25, 0, -0.1f, 0.8f, 6, 300, 200, 3, 300, 65, 250, 90);
        c.rank ("b_", 0, 100, 60, 0, 0, 0, 1, 10, 0, 500, 20, 0, -0.1f, 0.6f, 6, 300, 200, 3, 300, 65, 250, 60);
        c.pct ("a_pwm", 25); c.hz_ ("lfo_rate", 0.9f); c.pct ("a_velb", 60);
        c.list ("drv_mode", 1); c.pct ("drv_amt", 20);
        c.tape (1, 10, 0.5f, 15, 40, 20, 0, 10); c.hall (10, 10, 40, 1.0f, 5000, 20, 0);
    }
    // ------------------------------------------------------------------ PAD
    void heavenAndHell (Ctx& c)
    {
        c.rank ("a_", 100, 40, 60, 0, 20, 0, 2, 10, 0, 900, 15, 0, -0.5f, 0.2f, 1800, 4000, 3000, 1400, 1000, 90, 3000, 80);
        c.rank ("b_", 100, 40, 66, 0, 20, 0, 1, 10, 0, 700, 15, 0, -0.5f, 0.2f, 2200, 4000, 3000, 1800, 1000, 90, 3000, 60);
        c.cent ("b_fine", 7); c.pct ("a_pwm", 20); c.pct ("b_pwm", 20); c.hz_ ("lfo_rate", 0.25f);
        c.ensemble (4, 40, 70, 60); c.tape (1, 35, 0.3f, 20, 25, 45, 0, 25); c.hall (55, 60, 100, 9.0f, 3500, 50, 20);
        c.pct ("vintage", 50); c.pct ("spread", 90);
    }
    void antarcticPad (Ctx& c)
    {
        c.rank ("a_", 80, 0, 50, 30, 0, 8, 2, 200, 0, 1400, 20, 0, -0.6f, 0.1f, 2500, 5000, 4000, 2000, 1000, 100, 4000, 75);
        c.rank ("b_", 80, 0, 50, 30, 0, 8, 3, 200, 0, 1800, 20, 0, -0.6f, 0.1f, 3000, 5000, 4000, 2600, 1000, 100, 4000, 50);
        c.cent ("b_fine", -8); c.pct ("lfo_vcf", 12); c.hz_ ("lfo_rate", 0.12f); c.list ("lfo_wave", 1);
        c.ensemble (4, 35, 80, 65); c.tape (1, 40, 0.25f, 15, 20, 55, 3, 30); c.hall (65, 90, 100, 14.0f, 3000, 55, 35);
        c.pct ("vintage", 50); c.pct ("spread", 100);
    }
    void glassPad (Ctx& c)
    {
        c.rank ("a_", 0, 0, 50, 100, 30, 0, 3, 300, 0, 5000, 10, 0, 0.0f, 0.0f, 1200, 3000, 2000, 900, 800, 100, 2500, 75);
        c.rank ("b_", 0, 0, 50, 100, 30, 0, 4, 300, 0, 5000, 10, 0, 0.0f, 0.0f, 1200, 3000, 2000, 900, 800, 100, 2500, 40);
        c.cent ("b_fine", 6); c.list ("ring_mode", 1); c.pct ("ring_depth", 35); c.sw ("ring_key", true); c.hz_ ("ring_speed", 3.0f); c.bip ("ring_mod", 0.0f);
        c.ensemble (3, 50, 55, 50); c.tape (1, 25, 0.4f, 20, 15, 30, 0, 20); c.hall (55, 40, 95, 7.0f, 6000, 45, 40);
        c.pct ("spread", 85);
    }
    void tapeWash (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 12, 2, 250, 0, 800, 10, 0, -0.4f, 0.0f, 3000, 6000, 5000, 2500, 1000, 100, 5000, 75);
        c.rank ("b_", 100, 0, 50, 0, 0, 12, 1, 250, 0, 600, 10, 0, -0.4f, 0.0f, 3500, 6000, 5000, 3000, 1000, 100, 5000, 55);
        c.cent ("b_fine", 10);
        c.ensemble (4, 30, 90, 70); c.tape (1, 70, 0.3f, 40, 45, 75, 15, 50); c.hall (60, 100, 100, 12.0f, 2800, 60, 30);
        c.pct ("vintage", 70); c.pct ("spread", 100);
    }
    // ----------------------------------------------------------------- KEYS
    void electricGrand (Ctx& c)
    {
        c.rank ("a_", 40, 60, 80, 0, 50, 0, 2, 10, 0, 1800, 20, 0, 0.6f, 0.6f, 2, 1200, 300, 2, 1800, 20, 350, 85);
        c.rank ("b_", 0, 0, 50, 100, 30, 0, 3, 10, 0, 3000, 10, 0, 0.9f, 0.9f, 2, 400, 200, 2, 900, 0, 300, 50);
        c.cent ("b_fine", 3); c.pct ("a_velb", 80); c.pct ("b_velb", 80); c.pct ("a_vel", 70); c.pct ("b_vel", 80);
        c.ensemble (1, 45, 35, 30); c.tape (1, 15, 0.5f, 15, 30, 25, 0, 10); c.hall (28, 25, 65, 2.2f, 5500, 30, 0);
    }
    void clavinet84 (Ctx& c)
    {
        c.rank ("a_", 20, 100, 88, 0, 0, 0, 2, 200, 10, 2500, 35, 1, 1.0f, 1.0f, 1, 500, 200, 1, 700, 10, 250, 90);
        c.rank ("b_", 20, 100, 92, 0, 0, 0, 3, 200, 10, 2500, 35, 1, 1.0f, 1.0f, 1, 500, 200, 1, 700, 10, 250, 40);
        c.pct ("a_velb", 90); c.pct ("a_vel", 70);
        c.list ("drv_mode", 1); c.pct ("drv_amt", 20);
        c.tape (1, 10, 0.5f, 15, 35, 20, 0, 10); c.hall (15, 12, 45, 1.1f, 6000, 20, 0);
    }
    void organ84 (Ctx& c)
    {
        c.rank ("a_", 0, 40, 50, 0, 100, 0, 2, 10, 0, 8000, 0, 0, 0.0f, 0.0f, 2, 300, 100, 2, 300, 100, 60, 85);
        c.rank ("b_", 0, 40, 50, 0, 100, 0, 3, 10, 0, 8000, 0, 0, 0.0f, 0.0f, 2, 300, 100, 2, 300, 100, 60, 60);
        c.pct ("lfo_vca", 18); c.hz_ ("lfo_rate", 6.5f); c.list ("lfo_mode", 2);
        c.ensemble (2, 50, 50, 45); c.tape (1, 20, 0.5f, 25, 40, 30, 0, 15); c.hall (30, 30, 80, 2.8f, 5000, 30, 0);
    }
    // ---------------------------------------------------------------- BELLS
    void ringBells (Ctx& c)
    {
        c.rank ("a_", 0, 0, 50, 100, 40, 0, 3, 10, 0, 7000, 10, 0, 0.9f, 0.9f, 1, 1500, 900, 1, 2500, 0, 1200, 85);
        c.rank ("b_", 0, 0, 50, 100, 0, 0, 4, 10, 0, 7000, 10, 0, 0.9f, 0.9f, 1, 1500, 900, 1, 2500, 0, 1200, 50);
        c.semi ("b_semi", 5); c.list ("ring_mode", 2); c.pct ("ring_depth", 80);
        c.pct ("a_velb", 70); c.pct ("a_vel", 60);
        c.ensemble (1, 40, 40, 30); c.tape (1, 15, 0.4f, 15, 20, 30, 0, 15); c.hall (50, 30, 90, 6.0f, 6000, 40, 30);
    }
    void tubularDream (Ctx& c)
    {
        c.rank ("a_", 0, 0, 50, 100, 30, 0, 2, 10, 0, 5000, 30, 0, 0.8f, 0.8f, 1, 2200, 1200, 1, 3500, 0, 1500, 85);
        c.rank ("b_", 0, 60, 50, 40, 0, 0, 4, 10, 0, 6000, 30, 0, 0.8f, 0.8f, 1, 1200, 800, 1, 2000, 0, 1000, 45);
        c.list ("ring_mode", 1); c.pct ("ring_depth", 60); c.sw ("ring_key", true); c.hz_ ("ring_speed", 5.6f); c.bip ("ring_mod", -0.3f); c.ms ("ring_d", 900);
        c.ensemble (3, 45, 40, 35); c.tape (1, 20, 0.4f, 15, 20, 35, 0, 15); c.hall (55, 40, 100, 8.0f, 5000, 45, 35);
    }
    // -------------------------------------------------------------- STRANGE
    void polyModSweep (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 3000, 30, 1, 0.9f, 0.9f, 5, 1200, 500, 5, 600, 70, 500, 85);
        c.rank ("b_", 0, 0, 50, 100, 0, 0, 2, 10, 0, 3000, 30, 1, 0.9f, 0.9f, 5, 1200, 500, 5, 600, 70, 500, 0);
        c.pct ("pm_o2pitch", 45); c.pct ("pm_o2filt", 30); c.bip ("pm_envpitch", 0.5f);
        c.semi ("b_semi", -12); c.cent ("b_fine", 0);
        c.ensemble (1, 50, 30, 25); c.tape (1, 15, 0.5f, 20, 35, 25, 0, 10); c.hall (30, 20, 70, 2.5f, 5500, 30, 0);
    }
    void strangerThings (Ctx& c)
    {
        c.list ("mode", 0);
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 1, 10, 0, 2000, 40, 1, 0.8f, 0.8f, 3, 400, 250, 2, 300, 70, 250, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 10, 0, 1800, 40, 1, 0.8f, 0.8f, 3, 400, 250, 2, 300, 70, 250, 70);
        c.cent ("b_fine", 12); c.pct ("a_pwm", 0);
        c.list ("lfo_wave", 4); c.hz_ ("lfo_rate", 5.9f); c.pct ("lfo_vca", 45); c.list ("lfo_mode", 2);   // the pulse
        c.list ("drv_mode", 1); c.pct ("drv_amt", 35);
        c.ensemble (1, 55, 35, 30); c.tape (2, 35, 0.5f, 40, 45, 45, 8, 35); c.hall (25, 20, 60, 1.8f, 5000, 25, 0);
        c.pct ("vintage", 55);
    }
    void vhsMemory (Ctx& c)
    {
        c.rank ("a_", 60, 60, 66, 0, 40, 0, 2, 10, 0, 1200, 15, 0, -0.3f, 0.3f, 900, 3000, 2500, 700, 800, 95, 2500, 80);
        c.rank ("b_", 60, 60, 60, 0, 40, 0, 1, 10, 0, 1000, 15, 0, -0.3f, 0.3f, 1100, 3000, 2500, 900, 800, 95, 2500, 55);
        c.cent ("b_fine", 8);
        c.ensemble (4, 40, 70, 60); c.tape (2, 60, 0.4f, 60, 50, 60, 25, 55); c.hall (45, 50, 90, 6.0f, 3200, 45, 15);
        c.pct ("vintage", 65); c.pct ("spread", 90);
    }
    void insectSwarm (Ctx& c)
    {
        c.list ("mode", 2); c.centu ("unidet", 40);
        c.rank ("a_", 0, 100, 94, 0, 0, 20, 3, 800, 40, 5000, 60, 0, 0.5f, 0.9f, 20, 800, 400, 10, 400, 70, 300, 80);
        c.rank ("b_", 0, 100, 90, 0, 0, 20, 4, 800, 40, 5000, 60, 0, 0.5f, 0.9f, 20, 800, 400, 10, 400, 70, 300, 60);
        c.list ("lfo_wave", 6); c.hz_ ("lfo_rate", 12.0f); c.pct ("lfo_pitch", 22); c.pct ("lfo_pw", 60);
        c.list ("ring_mode", 1); c.pct ("ring_depth", 50); c.hz_ ("ring_speed", 37.0f);
        c.tape (1, 20, 0.6f, 40, 30, 40, 5, 20); c.hall (40, 20, 70, 3.0f, 6000, 40, 20);
    }
    void machineHum (Ctx& c)
    {
        c.list ("mode", 3);
        c.rank ("a_", 100, 50, 52, 0, 80, 0, 1, 10, 0, 180, 20, 1, 0.0f, 0.3f, 3000, 8000, 5000, 2500, 1000, 100, 4000, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 300, 50, 1, 0.0f, 0.2f, 3000, 8000, 5000, 2500, 1000, 100, 4000, 55);
        c.cent ("b_fine", 3); c.pct ("lfo_vcf", 20); c.list ("lfo_wave", 1); c.hz_ ("lfo_rate", 0.08f);
        c.list ("drv_mode", 2); c.pct ("drv_amt", 40);
        c.tape (1, 45, 0.2f, 25, 50, 55, 6, 35); c.hall (35, 30, 100, 9.0f, 3000, 50, 10);
        c.pct ("vintage", 60);
    }
    void sequencerPluck (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 500, 50, 1, 1.0f, 1.0f, 1, 220, 150, 1, 250, 0, 200, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 500, 50, 1, 1.0f, 1.0f, 1, 220, 150, 1, 250, 0, 200, 70);
        c.cent ("b_fine", 6); c.semi ("b_semi", 0); c.pct ("a_velb", 80); c.pct ("b_velb", 80);
        c.ensemble (1, 50, 35, 25); c.tape (1, 10, 0.5f, 20, 40, 25, 0, 10); c.hall (35, 60, 80, 3.0f, 5000, 35, 0);
    }
    void nakedRankI (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 20000, 0, 0, 0.0f, 0.0f, 1, 200, 200, 1, 200, 100, 50, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 20000, 0, 0, 0.0f, 0.0f, 1, 200, 200, 1, 200, 100, 50, 0);
        c.pct ("hall_mix", 0); c.pct ("vintage", 0);
    }
    void wideUnisonPad (Ctx& c)
    {
        c.list ("mode", 2); c.centu ("unidet", 22); c.pct ("spread", 100);
        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 100, 0, 1500, 15, 0, -0.4f, 0.2f, 1500, 3500, 2500, 1200, 800, 100, 2500, 80);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 100, 0, 1200, 15, 0, -0.4f, 0.2f, 1800, 3500, 2500, 1500, 800, 100, 2500, 55);
        c.cent ("b_fine", 5);
        c.ensemble (3, 45, 60, 45); c.tape (1, 30, 0.35f, 20, 25, 40, 0, 20); c.hall (50, 40, 100, 6.0f, 4000, 45, 10);
    }
    void duoLead (Ctx& c)
    {
        c.list ("mode", 1); c.centu ("unidet", 12); c.glide ("glide", 70);
        c.rank ("a_", 100, 0, 50, 0, 20, 0, 2, 10, 0, 1800, 30, 1, 0.0f, 0.7f, 30, 800, 350, 12, 500, 80, 400, 85);
        c.rank ("b_", 100, 30, 62, 0, 0, 0, 1, 10, 0, 1500, 30, 1, 0.0f, 0.7f, 30, 800, 350, 12, 500, 80, 400, 65);
        c.pct ("a_velb", 55); c.vibrato (5.4f, 15, 700); c.pct ("at_lfo", 50);
        c.ensemble (1, 50, 40, 30); c.tape (1, 20, 0.5f, 20, 40, 30, 0, 15); c.hall (30, 30, 70, 2.5f, 5000, 30, 0);
    }
    void filterSweepDrone (Ctx& c)
    {
        c.rank ("a_", 100, 40, 62, 0, 0, 5, 1, 60, 30, 400, 70, 1, -0.5f, 1.0f, 4000, 9000, 6000, 3000, 1000, 100, 5000, 85);
        c.rank ("b_", 100, 40, 58, 0, 0, 5, 2, 60, 30, 400, 70, 1, -0.5f, 1.0f, 5000, 9000, 6000, 3500, 1000, 100, 5000, 60);
        c.cent ("b_fine", 7); c.pct ("a_pwm", 30); c.pct ("b_pwm", 30); c.hz_ ("lfo_rate", 0.2f);
        c.ensemble (3, 40, 60, 50); c.tape (1, 35, 0.3f, 25, 35, 45, 4, 30); c.hall (45, 50, 100, 10.0f, 3500, 50, 20);
        c.pct ("vintage", 55); c.pct ("spread", 95);
    }
    void brassSection (Ctx& c)
    {
        c.list ("mode", 1); c.centu ("unidet", 8);
        c.rank ("a_", 100, 20, 58, 0, 20, 0, 2, 10, 0, 2800, 25, 0, -0.5f, 0.45f, 150, 1000, 500, 80, 500, 85, 550, 85);
        c.rank ("b_", 100, 20, 55, 0, 20, 0, 1, 10, 0, 2200, 25, 0, -0.5f, 0.4f, 200, 1000, 500, 100, 500, 85, 550, 70);
        c.cent ("b_fine", 6); c.pct ("a_velb", 55); c.pct ("b_velb", 55);
        c.vibrato (5.2f, 12, 900); c.pct ("at_brill", 55);
        c.ensemble (3, 50, 50, 40); c.tape (1, 25, 0.45f, 20, 40, 30, 0, 15); c.hall (35, 30, 80, 3.0f, 5000, 35, 0);
        c.pct ("spread", 80);
    }
    void softKeys (Ctx& c)
    {
        c.rank ("a_", 60, 0, 50, 40, 50, 0, 2, 10, 0, 1400, 10, 0, 0.3f, 0.5f, 4, 900, 300, 3, 1200, 40, 400, 85);
        c.rank ("b_", 60, 0, 50, 40, 50, 0, 3, 10, 0, 1800, 10, 0, 0.3f, 0.5f, 4, 700, 300, 3, 900, 30, 400, 45);
        c.cent ("b_fine", 4); c.pct ("a_velb", 60); c.pct ("a_vel", 60); c.pct ("b_vel", 60);
        c.ensemble (1, 45, 40, 35); c.tape (1, 15, 0.5f, 15, 25, 25, 0, 10); c.hall (35, 25, 75, 2.8f, 5500, 35, 0);
    }
    void ghostHall (Ctx& c)
    {
        c.rank ("a_", 20, 20, 60, 60, 40, 15, 3, 400, 20, 2500, 40, 0, -0.3f, 0.3f, 2000, 5000, 4000, 1500, 1000, 100, 5000, 70);
        c.rank ("b_", 20, 20, 60, 60, 40, 15, 4, 400, 20, 2500, 40, 0, -0.3f, 0.3f, 2500, 5000, 4000, 2000, 1000, 100, 5000, 45);
        c.cent ("b_fine", -9);
        c.pct ("choir_mix", 50); c.pct ("choir_vowel", 20); c.bip ("choir_reg", -0.3f);
        c.ensemble (4, 30, 80, 70); c.tape (2, 50, 0.3f, 40, 30, 70, 15, 45); c.hall (70, 120, 100, 16.0f, 2800, 60, 45);
        c.pct ("vintage", 70); c.pct ("spread", 100);
    }
    void thePlanets (Ctx& c)
    {
        c.rank ("a_", 100, 0, 50, 0, 40, 0, 1, 10, 0, 600, 20, 0, -0.6f, 0.4f, 2500, 6000, 4000, 2000, 1000, 100, 4000, 85);
        c.rank ("b_", 100, 0, 50, 0, 40, 0, 2, 10, 0, 800, 20, 0, -0.6f, 0.4f, 3000, 6000, 4000, 2500, 1000, 100, 4000, 65);
        c.cent ("b_fine", 6); c.pct ("lfo_vcf", 10); c.list ("lfo_wave", 0); c.hz_ ("lfo_rate", 0.15f); c.list ("lfo_mode", 0);
        c.ensemble (4, 40, 75, 65); c.tape (1, 35, 0.3f, 20, 30, 45, 2, 25); c.hall (60, 80, 100, 12.0f, 3200, 55, 30);
        c.pct ("vintage", 50); c.pct ("spread", 100);
    }

    const Patch PATCHES[] =
    {
        { "BLADE BRASS",        "BRASS",   bladeBrass },
        { "CHARIOTS BRASS",     "BRASS",   chariotsBrass },
        { "TIGHT BRASS",        "BRASS",   tightBrass },
        { "FRENCH HORNS",       "BRASS",   frenchHorns },
        { "BRASS SECTION",      "BRASS",   brassSection },
        { "CS STRINGS",         "STRINGS", cs80Strings },
        { "VP STRINGS",         "STRINGS", vp330Strings },
        { "CATHEDRAL STRINGS",  "STRINGS", cathedralStrings },
        { "SOLINA ROOM",        "STRINGS", solinaRoom },
        { "HUMAN VOICE",        "CHOIR",   humanVoice },
        { "VANGELIS CHOIR",     "CHOIR",   vangelisChoir },
        { "BOYS CHOIR",         "CHOIR",   boysChoir },
        { "BLADE LEAD",         "LEAD",    bladeLead },
        { "SYNC LEAD",          "LEAD",    syncLead },
        { "SCREAMING LEAD",     "LEAD",    screamingLead },
        { "DUO LEAD",           "LEAD",    duoLead },
        { "VINTAGE BASS",       "BASS",    vintageBass },
        { "MOOGISH BASS",       "BASS",    moogishBass },
        { "PULSE BASS",         "BASS",    pulseBass },
        { "HEAVEN AND HELL",    "PAD",     heavenAndHell },
        { "ANTARCTIC PAD",      "PAD",     antarcticPad },
        { "GLASS PAD",          "PAD",     glassPad },
        { "TAPE WASH",          "PAD",     tapeWash },
        { "WIDE UNISON PAD",    "PAD",     wideUnisonPad },
        { "THE PLANETS",        "PAD",     thePlanets },
        { "ELECTRIC GRAND",     "KEYS",    electricGrand },
        { "CLAVINET 84",        "KEYS",    clavinet84 },
        { "ORGAN 84",           "KEYS",    organ84 },
        { "SOFT KEYS",          "KEYS",    softKeys },
        { "RING BELLS",         "BELLS",   ringBells },
        { "TUBULAR DREAM",      "BELLS",   tubularDream },
        { "SEQUENCER PLUCK",    "BELLS",   sequencerPluck },
        { "POLY-MOD SWEEP",     "STRANGE", polyModSweep },
        { "STRANGER THINGS",    "STRANGE", strangerThings },
        { "VHS MEMORY",         "STRANGE", vhsMemory },
        { "INSECT SWARM",       "STRANGE", insectSwarm },
        { "MACHINE HUM",        "STRANGE", machineHum },
        { "FILTER SWEEP DRONE", "STRANGE", filterSweepDrone },
        { "GHOST HALL",         "STRANGE", ghostHall },
        { "NAKED RANK I",       "STRANGE", nakedRankI },
    };
}

int numPatches() { return (int) (sizeof (PATCHES) / sizeof (PATCHES[0])); }
const char* patchName (int i)     { return PATCHES[i < 0 ? 0 : (i >= numPatches() ? numPatches() - 1 : i)].name; }
const char* patchCategory (int i) { return PATCHES[i < 0 ? 0 : (i >= numPatches() ? numPatches() - 1 : i)].cat; }

void applyPatch (int i, Params& p)
{
    i = i < 0 ? 0 : (i >= numPatches() ? numPatches() - 1 : i);
    const float keepOs = p.os, keepVol = p.volume, keepBend = p.bend, keepTune = p.tune, keepFine = p.fine;
    p = Params();
    // every table default, explicitly, so a patch starts from the table and not from the struct
    for (int k = 0; k < numParams(); ++k) paramSpec (k).ref (p) = paramSpec (k).def;
    Ctx c { p };
    PATCHES[i].fn (c);
    p.patch = (float) i;
    p.os = keepOs; p.volume = keepVol; p.bend = keepBend; p.tune = keepTune; p.fine = keepFine;
}

/*  A playable instrument, rolled inside musical ranges. The performance
    controls, the master and the quality are left alone. */
void randomPatch (uint32_t seed, Params& p)
{
    Rng r; r.seed (seed);
    auto roll = [&r] (float lo, float hi) { return lo + r.uni() * (hi - lo); };
    auto pick = [&r] (int n) { return (float) (r.u32() % (uint32_t) n); };
    auto chance = [&r] (float pr) { return r.uni() < pr; };
    const float keepOs = p.os, keepVol = p.volume, keepBend = p.bend, keepTune = p.tune, keepFine = p.fine;
    for (int k = 0; k < numParams(); ++k) paramSpec (k).ref (p) = paramSpec (k).def;

    p.mode = chance (0.7f) ? 0.0f : pick (4);
    p.glide = chance (0.3f) ? roll (0.15f, 0.5f) : 0.0f;
    p.spread = roll (0.3f, 1.0f); p.unidet = roll (0.1f, 0.6f); p.vintage = roll (0.1f, 0.7f);
    for (int k = 0; k < NUM_RANKS; ++k)
    {
        RankParams& q = p.rk[k];
        q.saw = chance (0.75f) ? roll (0.4f, 1.0f) : 0.0f;
        q.pulse = chance (0.5f) ? roll (0.3f, 1.0f) : 0.0f;
        if (q.saw == 0.0f && q.pulse == 0.0f) q.tri = roll (0.5f, 1.0f); else q.tri = chance (0.2f) ? roll (0.2f, 0.7f) : 0.0f;
        q.pw = roll (0.0f, 0.9f); q.pwm = chance (0.35f) ? roll (0.1f, 0.6f) : 0.0f;
        q.sine = chance (0.4f) ? roll (0.2f, 0.7f) : 0.0f;
        q.noise = chance (0.15f) ? roll (0.02f, 0.2f) : 0.0f;
        q.oct = 1.0f + pick (3);
        q.semi = chance (0.2f) ? 0.5f + (float) (int) (pick (3) - 1) * 7.0f / 24.0f : 0.5f;
        q.fine = 0.5f + roll (-0.12f, 0.12f) * (k == 1 ? 1.0f : 0.3f);
        q.hpf = chance (0.4f) ? roll (0.0f, 0.45f) : 0.0f; q.hpq = chance (0.3f) ? roll (0.0f, 0.5f) : 0.0f;
        q.lpf = roll (0.35f, 0.85f); q.lpq = roll (0.0f, 0.7f); q.fmode = pick (2);
        q.il = roll (0.2f, 0.8f); q.al = roll (0.4f, 1.0f);
        q.fa = roll (0.0f, 0.7f); q.fd = roll (0.3f, 0.8f); q.fr = roll (0.3f, 0.8f);
        q.va = roll (0.0f, 0.6f); q.vd = roll (0.3f, 0.8f); q.vs = roll (0.3f, 1.0f); q.vr = roll (0.3f, 0.8f);
        q.lvl = k == 0 ? roll (0.6f, 0.9f) : (chance (0.85f) ? roll (0.2f, 0.9f) : 0.0f);
        q.pan = 0.5f + roll (-0.2f, 0.2f); q.vel = roll (0.2f, 0.8f); q.velb = roll (0.1f, 0.8f);
        q.sync = 0.0f;
    }
    p.rk[1].sync = chance (0.15f) ? 1.0f : 0.0f;
    p.ring_mode = chance (0.2f) ? 1.0f + pick (2) : 0.0f; p.ring_depth = p.ring_mode > 0.0f ? roll (0.2f, 0.8f) : 0.0f;
    p.ring_speed = roll (0.2f, 0.8f); p.ring_key = chance (0.6f) ? 1.0f : 0.0f;
    p.pm_o2pitch = chance (0.15f) ? roll (0.1f, 0.5f) : 0.0f; p.pm_o2filt = chance (0.15f) ? roll (0.1f, 0.5f) : 0.0f;
    p.pm_envpitch = chance (0.2f) ? roll (0.35f, 0.8f) : 0.5f;
    p.lfo_wave = pick (7); p.lfo_rate = roll (0.3f, 0.8f);
    p.lfo_pitch = chance (0.5f) ? roll (0.05f, 0.25f) : 0.0f; p.lfo_vcf = chance (0.3f) ? roll (0.05f, 0.3f) : 0.0f;
    p.lfo_vca = chance (0.2f) ? roll (0.05f, 0.4f) : 0.0f; p.lfo_delay = chance (0.5f) ? roll (0.3f, 0.7f) : 0.0f;
    p.lfo_mode = pick (3);
    p.drv_mode = chance (0.35f) ? 1.0f + pick (4) : 0.0f; p.drv_amt = roll (0.1f, 0.6f); p.drv_tone = roll (0.3f, 0.7f);
    p.ens_mode = chance (0.7f) ? 1.0f + pick (4) : 0.0f; p.ens_rate = roll (0.3f, 0.7f); p.ens_depth = roll (0.3f, 0.9f); p.ens_mix = roll (0.2f, 0.7f);
    p.choir_mix = chance (0.25f) ? roll (0.3f, 0.9f) : 0.0f; p.choir_vowel = roll (0.0f, 1.0f); p.choir_reg = roll (0.3f, 0.7f); p.choir_air = roll (0.0f, 0.6f);
    p.tape_mode = chance (0.7f) ? 1.0f + pick (2) : 0.0f; p.tape_wow = roll (0.0f, 0.6f); p.tape_wowrate = roll (0.2f, 0.7f);
    p.tape_flut = roll (0.0f, 0.5f); p.tape_sat = roll (0.0f, 0.7f); p.tape_age = roll (0.0f, 0.7f);
    p.tape_drop = chance (0.3f) ? roll (0.0f, 0.3f) : 0.0f; p.tape_hiss = roll (0.0f, 0.5f);
    p.hall_mix = chance (0.85f) ? roll (0.1f, 0.6f) : 0.0f; p.hall_pre = roll (0.2f, 0.7f); p.hall_size = roll (0.3f, 1.0f);
    p.hall_decay = roll (0.2f, 0.8f); p.hall_damp = roll (0.3f, 0.8f); p.hall_mod = roll (0.0f, 0.7f); p.hall_shim = chance (0.3f) ? roll (0.1f, 0.6f) : 0.0f;
    p.os = keepOs; p.volume = keepVol; p.bend = keepBend; p.tune = keepTune; p.fine = keepFine;
}

} // namespace n84
