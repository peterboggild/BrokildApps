/*  HIGH TIDE — the factory patches.

    Two groups. STARTERS are the safe ground: familiar wavetable sounds —
    basses, leads, keys, pads — every one of them exactly in tune whatever
    the strike, with no rock and a tide that keeps the pins predictable.
    TERRAINS are the instrument showing what it is: thresholds, cascades,
    stranded balls, parametric pumps.

    THE CENTRE-LINE DISCOVERY, which is what makes the starters possible.
    Landau's result says the period of a bowl depends only on its WIDTH at
    each height. Write a bowl as

        x±(h) = c(h) ± sqrt(2h)

    and the width is 2*sqrt(2h) whatever c does — so THE BOWL'S CENTRE LINE
    MAY WANDER WITH HEIGHT HOWEVER IT LIKES AND THE PITCH DOES NOT MOVE. The
    centre line is the timbre; the pitch is the reference parabola's, exactly.
    A straight c gives the sheared bowl (even harmonics); a wavy c gives a
    rich, in-tune wave that is nothing like a sine. The only constraint is
    that the walls stay monotone, |c'(h)| < 1/sqrt(2h).
*/
#include "Engine.h"
#include <functional>
#include <algorithm>

namespace ht
{

namespace
{
    constexpr float DXF = (XMAX - XMIN) / (float) (NX - 1);
    inline float xOf (int i) { return XMIN + DXF * (float) i; }
    inline float zOf (int j) { return (float) j / (float) (NZ - 1); }
    inline float sstep (float e0, float e1, float x)
    {
        float t = (x - e0) / (e1 - e0); t = t < 0 ? 0 : (t > 1 ? 1 : t);
        return t * t * (3 - 2 * t);
    }

    //==========================================================================
    //  the bowl vocabulary. Every shape has its floor at 0.
    float bSine (float x) { return 0.5f * x * x; }

    /*  A box with a ROUNDED knee. A hard knee is a stiffness jump the ball
        crosses at an arbitrary phase of its step, and the O(dt^2) error of
        that crossing jitters the period — a hard box measured +37 dB of
        non-harmonic energy however finely the step was subdivided. The knee is
        softened over the brush floor (0.04), which is the rule the brush obeys. */
    float bBox (float x, float w = 0.55f, float k = 6.0f)
    {
        const float ws = 0.04f;
        const float a  = (std::abs (x) - w) / ws;
        const float s  = ws * (a > 20.0f ? a : std::log1p (std::exp (a)));   // softplus
        return k * s * s;
    }
    float bPit (float x, float d = 0.16f, float pw = 0.08f)
    {
        return 0.5f * x * x + d * (1.0f - std::exp (-(x * x) / (pw * pw)));
    }
    float bDouble (float x, float w = 0.55f, float h = 0.09f)
    {
        const float a = h / (w * w * w * w);
        const float q = (x * x - w * w);
        float u = a * q * q;
        const float ax = std::abs (x);
        if (ax > 1.0f) u += 3.0f * (ax - 1.0f) * (ax - 1.0f);
        return u;
    }
    float bShelf (float x, float at = 0.55f, float len = 0.22f)
    {
        if (x < at) return 0.5f * x * x;
        if (x < at + len) return 0.5f * at * at;
        const float xx = x - len;
        return 0.5f * xx * xx;
    }

    //==========================================================================
    //  THE ISOCHRONOUS FAMILY (see the head of this file)
    template <typename C>
    float bIso (float x, C c)
    {
        float lo = 0, hi = 4.0f;
        for (int it = 0; it < 44; ++it)
        {
            const float h = 0.5f * (lo + hi), r = std::sqrt (2.0f * h), m = c (h);
            if (x >= m - r && x <= m + r) hi = h; else lo = h;
        }
        return hi;
    }
    //  a straight centre line: the sheared bowl. Even harmonics, in tune.
    float bShear (float x, float s) { return bIso (x, [s] (float h) { return s * h; }); }
    //  a wavy centre line: rich in both odd and even harmonics, still in tune.
    //  Keep |s| + a*k below about 0.9 or the walls stop being monotone.
    float bWave (float x, float s, float a, float k)
    { return bIso (x, [s, a, k] (float h) { return s * h + a * std::sin (k * h); }); }
    //  kept for the terrains written before the centre-line construction
    float bAsym (float x, float s = 0.35f) { return bShear (x, s); }

    //==========================================================================
    struct B
    {
        Terrain& t;
        explicit B (Terrain& tt) : t (tt) { std::fill (t.u.begin(), t.u.end(), 0.0f); }
        void fill (float z0, float z1, const std::function<float (float)>& shape)
        {
            for (int j = 0; j < NZ; ++j)
            {
                const float z = zOf (j);
                if (z < z0 - 1e-6f || z > z1 + 1e-6f) continue;
                for (int i = 0; i < NX; ++i) t.at (i, j) = shape (xOf (i));
            }
        }
        void morph (float z0, float z1, const std::function<float (float)>& a, const std::function<float (float)>& b)
        {
            for (int j = 0; j < NZ; ++j)
            {
                const float z = zOf (j);
                if (z < z0 - 1e-6f || z > z1 + 1e-6f) continue;
                const float m = sstep (z0, z1, z);
                for (int i = 0; i < NX; ++i) { const float x = xOf (i); t.at (i, j) = a (x) * (1 - m) + b (x) * m; }
            }
        }
        /*  A morph BETWEEN TWO ISOCHRONOUS BOWLS is not isochronous if it is
            done on the heights: the average of two bowls has neither width.
            Blend the CENTRE LINES instead and the whole sweep stays in tune —
            which is what lets a starter sweep its timbre and keep its pitch. */
        void morphIso (float z0, float z1,
                       const std::function<float (float)>& c0,
                       const std::function<float (float)>& c1)
        {
            for (int j = 0; j < NZ; ++j)
            {
                const float z = zOf (j);
                if (z < z0 - 1e-6f || z > z1 + 1e-6f) continue;
                const float m = sstep (z0, z1, z);
                for (int i = 0; i < NX; ++i)
                    t.at (i, j) = bIso (xOf (i), [&] (float h) { return c0 (h) * (1 - m) + c1 (h) * m; });
            }
        }
        void ridge (float zc, float w, float h)
        {
            for (int j = 0; j < NZ; ++j)
            {
                const float d = (zOf (j) - zc) / w;
                const float bump = h * std::exp (-d * d * 2.0f);
                for (int i = 0; i < NX; ++i) t.at (i, j) += bump;
            }
        }
        void tilt (float h0, float h1)
        {
            for (int j = 0; j < NZ; ++j) { const float add = h0 + (h1 - h0) * zOf (j); for (int i = 0; i < NX; ++i) t.at (i, j) += add; }
        }
        void done() { t.recomputeRelief(); }
    };

    void pin (Lanes& l, float t, float v, int e = 2) { l.pin.on.push_back ({ t, v, e }); }
    void pinOff (Lanes& l, float t, float v, int e = 2) { l.pin.off.push_back ({ t, v, e }); }
    void set (Params& p, const char* id, float v) { paramSpec (paramIndex (id)).get (p) = v; }

    /*  Every STARTER begins here: a plain poly voice, no rock, no drive, the
        pin lane empty (so POSITION rules), and the ball's own dynamics doing
        the work. A starter then changes only what it needs. */
    void safeGround (Params& p)
    {
        set (p, "rock", 0.0f);
        set (p, "hold", 0.6f);
        set (p, "tide", 0.7f);
        set (p, "position", 0.0f);
        set (p, "servo", 0.0f);
        set (p, "tune", 0.5f);
        set (p, "quality", 1.0f);
        set (p, "ampA", 0.10f); set (p, "ampD", 0.55f); set (p, "ampS", 0.85f); set (p, "ampR", 0.55f);
        set (p, "modAmt", 0.5f); set (p, "lfo1Amt", 0.5f); set (p, "lfo2Amt", 0.5f);
    }

    //==========================================================================
    //  STARTERS — basses
    void sSub (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bShear (x, 0.12f); }); b.done();
        (void) l; safeGround (p);
        set (p, "voiceMode", 0.5f);           // MONO
        set (p, "glide", 0.10f);
        set (p, "tapPos", 1.0f); set (p, "tapVel", 0.0f);
        set (p, "tone", 0.42f); set (p, "strike", 0.85f); set (p, "velSens", 0.45f);
        set (p, "friction", 0.10f); set (p, "release", 0.55f);
        set (p, "ampA", 0.05f); set (p, "ampD", 0.50f); set (p, "ampS", 0.9f); set (p, "ampR", 0.35f);
        set (p, "level", 0.48f);
    }
    void sPluckBass (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.30f, 0.040f, 9.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "voiceMode", 0.5f);
        set (p, "tapPos", 0.55f); set (p, "tapVel", 0.55f);
        set (p, "tone", 0.62f); set (p, "strike", 0.85f); set (p, "velSens", 0.8f);
        //  the pluck is the ball SINKING: brightness closes with the energy
        set (p, "friction", 0.42f); set (p, "release", 0.7f);
        set (p, "ampA", 0.0f); set (p, "ampD", 0.62f); set (p, "ampS", 0.55f); set (p, "ampR", 0.30f);
        set (p, "level", 0.93f);
    }
    void sWideBass (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.34f, 0.032f, 7.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "unison", 0.33f); set (p, "detune", 0.16f); set (p, "spread", 0.35f); set (p, "width", 0.55f);
        set (p, "voiceMode", 0.5f);
        set (p, "tapPos", 0.7f); set (p, "tapVel", 0.35f);
        set (p, "tone", 0.55f); set (p, "strike", 0.8f); set (p, "velSens", 0.5f);
        set (p, "friction", 0.14f); set (p, "release", 0.6f);
        set (p, "ampA", 0.06f); set (p, "ampS", 0.85f); set (p, "ampR", 0.38f);
        set (p, "level", 0.82f);
    }
    void sSquareBass (Terrain& t, Lanes& l, Params& p)
    {
        //  a box is not isochronous; SERVO measures its period and holds the note
        B b (t); b.fill (0, 1.0f, [] (float x) { return bBox (x, 0.55f, 8.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "servo", 0.75f);
        set (p, "voiceMode", 0.5f); set (p, "glide", 0.08f);
        set (p, "tapPos", 0.45f); set (p, "tapVel", 0.5f);
        set (p, "tone", 0.52f); set (p, "strike", 0.7f); set (p, "velSens", 0.5f);
        set (p, "friction", 0.12f); set (p, "release", 0.6f);
        set (p, "ampA", 0.04f); set (p, "ampS", 0.9f); set (p, "ampR", 0.32f);
        set (p, "level", 0.61f);
    }

    //  STARTERS — leads
    void sSawLead (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.36f, 0.048f, 11.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "unison", 0.33f); set (p, "detune", 0.22f); set (p, "spread", 0.3f); set (p, "width", 0.5f);
        set (p, "tapPos", 0.4f); set (p, "tapVel", 0.7f);
        set (p, "tone", 0.8f); set (p, "strike", 0.7f); set (p, "velSens", 0.7f);
        set (p, "friction", 0.10f); set (p, "release", 0.5f);
        set (p, "ampA", 0.12f); set (p, "ampS", 0.85f); set (p, "ampR", 0.42f);
        set (p, "level", 1.00f);
    }
    void sSquareLead (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bBox (x, 0.5f, 10.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "servo", 0.8f);
        set (p, "unison", 0.33f); set (p, "detune", 0.14f); set (p, "width", 0.45f);
        set (p, "tapPos", 0.3f); set (p, "tapVel", 0.75f);
        set (p, "tone", 0.74f); set (p, "strike", 0.62f); set (p, "velSens", 0.6f);
        set (p, "friction", 0.08f); set (p, "release", 0.5f);
        set (p, "ampA", 0.08f); set (p, "ampS", 0.9f); set (p, "ampR", 0.4f);
        set (p, "level", 0.79f);
    }
    void sSweepLead (Terrain& t, Lanes& l, Params& p)
    {
        /*  the filter sweep without a filter: the CENTRE LINE morphs from
            straight to wavy along the position axis, so the timbre opens and
            the pitch does not move a cent. */
        B b (t);
        b.morphIso (0, 1.0f,
                    [] (float h) { return 0.05f * h; },
                    [] (float h) { return 0.34f * h + 0.05f * std::sin (12.0f * h); });
        b.done();
        safeGround (p);
        pin (l, 0.0f, 0.0f, 2); pin (l, 1.1f, 1.0f, 2);
        pinOff (l, 0.5f, 0.15f, 2);
        set (p, "hold", 0.85f); set (p, "tide", 1.0f);
        set (p, "tapPos", 0.5f); set (p, "tapVel", 0.55f);
        set (p, "tone", 0.78f); set (p, "strike", 0.7f); set (p, "velSens", 0.6f);
        set (p, "friction", 0.06f); set (p, "release", 0.5f);
        set (p, "ampA", 0.10f); set (p, "ampS", 0.9f); set (p, "ampR", 0.5f);
        set (p, "level", 0.60f);
    }
    void sGlideLead (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.28f, 0.038f, 8.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "voiceMode", 1.0f);            // LEGATO
        set (p, "glide", 0.32f);
        set (p, "tapPos", 0.6f); set (p, "tapVel", 0.5f);
        set (p, "tone", 0.7f); set (p, "strike", 0.68f); set (p, "velSens", 0.55f);
        set (p, "friction", 0.09f); set (p, "release", 0.5f);
        //  TIDE is target index 1 of six, so 0.2 — 1.0 would be PITCH, and a
        //  starter that wobbles its own pitch is not safe ground
        set (p, "lfo1Target", 0.2f); set (p, "lfo1Amt", 0.54f); set (p, "lfo1Rate", 0.52f);
        set (p, "ampA", 0.14f); set (p, "ampS", 0.88f); set (p, "ampR", 0.45f);
        set (p, "level", 0.59f);
    }

    //  STARTERS — keys and organs
    void sOrgan (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.10f, 0.030f, 13.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "unison", 0.33f); set (p, "detune", 0.06f); set (p, "width", 0.5f); set (p, "spread", 0.15f);
        set (p, "tapPos", 0.85f); set (p, "tapVel", 0.2f);
        set (p, "tone", 0.72f); set (p, "strike", 0.6f); set (p, "velSens", 0.25f);
        set (p, "friction", 0.0f);              // frictionless: it holds while the key is down
        set (p, "release", 0.75f);
        set (p, "ampA", 0.03f); set (p, "ampD", 0.3f); set (p, "ampS", 1.0f); set (p, "ampR", 0.10f);
        set (p, "level", 0.80f);
    }
    void sElectricPiano (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.22f, 0.045f, 10.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "tapPos", 0.7f); set (p, "tapVel", 0.45f);
        set (p, "tone", 0.72f); set (p, "strike", 0.75f); set (p, "velSens", 0.85f);
        //  the bell-into-body decay is the ball sinking, not a filter envelope
        set (p, "friction", 0.26f); set (p, "release", 0.6f);
        set (p, "ampA", 0.0f); set (p, "ampD", 0.72f); set (p, "ampS", 0.45f); set (p, "ampR", 0.42f);
        set (p, "level", 0.78f);
    }
    void sBell (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bPit (x, 0.14f, 0.10f); }); b.done();
        (void) l; safeGround (p);
        set (p, "servo", 0.7f);
        set (p, "tapPos", 0.35f); set (p, "tapVel", 0.7f);
        set (p, "tone", 0.9f); set (p, "strike", 0.8f); set (p, "velSens", 0.8f);
        set (p, "friction", 0.30f); set (p, "release", 0.25f);
        set (p, "ampA", 0.0f); set (p, "ampD", 0.85f); set (p, "ampS", 0.12f); set (p, "ampR", 0.72f);
        set (p, "level", 0.96f);
    }
    void sClav (Terrain& t, Lanes& l, Params& p)
    {
        /*  A box would be the obvious clav and it is the wrong choice: heavy
            friction drops the ball onto the flat floor, where the period grows
            without bound and the servo runs out of range — measured 35 cents
            flat within a second. A wavy centre line gives the same edge on the
            FORCE tap and cannot go out of tune at all. */
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.30f, 0.050f, 13.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "tapPos", 0.6f); set (p, "tapVel", 0.5f); set (p, "tapFrc", 0.85f);
        set (p, "tone", 0.8f); set (p, "strike", 0.9f); set (p, "velSens", 0.85f);
        set (p, "friction", 0.26f); set (p, "release", 0.8f);
        set (p, "ampA", 0.0f); set (p, "ampD", 0.6f); set (p, "ampS", 0.35f); set (p, "ampR", 0.22f);
        set (p, "level", 1.00f);
    }

    //  STARTERS — ambient
    void sSlowPad (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.20f, 0.036f, 9.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "unison", 1.0f); set (p, "detune", 0.22f); set (p, "spread", 0.28f); set (p, "width", 0.85f);
        set (p, "tapPos", 0.8f); set (p, "tapVel", 0.25f);
        set (p, "tone", 0.7f); set (p, "strike", 0.55f); set (p, "velSens", 0.5f);
        set (p, "friction", 0.02f); set (p, "release", 0.3f);
        set (p, "lfo1Target", 0.2f); set (p, "lfo1Amt", 0.56f); set (p, "lfo1Rate", 0.30f);   // TIDE breathing
        set (p, "ampA", 0.55f); set (p, "ampD", 0.7f); set (p, "ampS", 0.9f); set (p, "ampR", 0.72f);
        set (p, "level", 0.76f);
    }
    void sMorphPad (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.morphIso (0, 0.5f, [] (float h) { return 0.02f * h; },
                             [] (float h) { return 0.30f * h + 0.045f * std::sin (10.0f * h); });
        b.morphIso (0.5f, 1.0f, [] (float h) { return 0.30f * h + 0.045f * std::sin (10.0f * h); },
                                [] (float h) { return -0.28f * h + 0.050f * std::sin (16.0f * h); });
        b.done();
        safeGround (p);
        pin (l, 0.0f, 0.0f, 2); pin (l, 8.0f, 1.0f, 1);
        pinOff (l, 3.0f, 0.35f, 2);
        set (p, "hold", 0.8f); set (p, "tide", 1.0f);
        set (p, "unison", 0.67f); set (p, "detune", 0.14f); set (p, "spread", 0.35f); set (p, "width", 0.8f);
        set (p, "tapPos", 0.75f); set (p, "tapVel", 0.3f);
        set (p, "tone", 0.72f); set (p, "strike", 0.55f); set (p, "velSens", 0.5f);
        set (p, "friction", 0.03f); set (p, "release", 0.28f);
        set (p, "ampA", 0.48f); set (p, "ampS", 0.9f); set (p, "ampR", 0.75f);
        set (p, "level", 0.70f);
    }
    void sGlass (Terrain& t, Lanes& l, Params& p)
    {
        B b (t); b.fill (0, 1.0f, [] (float x) { return bWave (x, 0.0f, 0.050f, 15.0f); }); b.done();
        (void) l; safeGround (p);
        set (p, "unison", 0.67f); set (p, "detune", 0.09f); set (p, "spread", 0.2f); set (p, "width", 0.9f);
        set (p, "tapPos", 0.55f); set (p, "tapVel", 0.45f);
        set (p, "tone", 1.0f); set (p, "strike", 0.5f); set (p, "velSens", 0.6f);
        set (p, "friction", 0.02f); set (p, "release", 0.20f);
        set (p, "ampA", 0.22f); set (p, "ampD", 0.8f); set (p, "ampS", 0.8f); set (p, "ampR", 0.8f);
        set (p, "level", 0.80f);
    }
    void sDrone (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.morphIso (0, 1.0f, [] (float h) { return 0.06f * h + 0.030f * std::sin (7.0f * h); },
                             [] (float h) { return 0.24f * h + 0.040f * std::sin (12.0f * h); });
        b.done();
        (void) l; safeGround (p);
        set (p, "unison", 1.0f); set (p, "detune", 0.30f); set (p, "spread", 0.28f); set (p, "width", 1.0f);
        set (p, "position", 0.35f); set (p, "hold", 0.35f);
        set (p, "tapPos", 0.7f); set (p, "tapVel", 0.35f);
        set (p, "tone", 0.66f); set (p, "strike", 0.55f); set (p, "velSens", 0.35f);
        set (p, "friction", 0.0f); set (p, "release", 0.22f);
        set (p, "lfo1Target", 0.0f); set (p, "lfo1Amt", 0.62f); set (p, "lfo1Rate", 0.18f);   // POSITION wandering
        set (p, "ampA", 0.62f); set (p, "ampS", 1.0f); set (p, "ampR", 0.8f);
        set (p, "level", 0.74f);
    }
    //  the one a fresh instance opens on: warm, poly, and it moves a little
    void sSoftKeys (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.morphIso (0, 1.0f, [] (float h) { return 0.08f * h + 0.020f * std::sin (8.0f * h); },
                             [] (float h) { return 0.30f * h + 0.042f * std::sin (11.0f * h); });
        b.done();
        safeGround (p);
        pin (l, 0.0f, 0.55f, 2); pin (l, 1.6f, 0.15f, 2);
        pinOff (l, 0.8f, 0.0f, 2);
        set (p, "hold", 0.7f); set (p, "tide", 1.0f);
        set (p, "unison", 0.33f); set (p, "detune", 0.10f); set (p, "spread", 0.25f); set (p, "width", 0.6f);
        set (p, "tapPos", 0.75f); set (p, "tapVel", 0.4f);
        set (p, "tone", 0.72f); set (p, "strike", 0.68f); set (p, "velSens", 0.7f);
        set (p, "friction", 0.16f); set (p, "release", 0.5f);
        set (p, "ampA", 0.06f); set (p, "ampD", 0.68f); set (p, "ampS", 0.7f); set (p, "ampR", 0.55f);
        set (p, "level", 1.00f);
    }

    //==========================================================================
    //  TERRAINS — the instrument showing what it is
    void fFirstLight (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 0.3f, [] (float x) { return bSine (x); });
        b.morph (0.3f, 0.6f, [] (float x) { return bSine (x); }, [] (float x) { return bAsym (x, 0.3f); });
        b.fill (0.6f, 1.0f, [] (float x) { return bAsym (x, 0.3f); });
        b.ridge (0.45f, 0.06f, 0.10f);
        b.done();
        pin (l, 0.0f, 0.10f); pin (l, 2.5f, 0.85f, 2);
        pinOff (l, 1.2f, 0.10f, 2);
        set (p, "tide", 0.55f); set (p, "hold", 0.5f); set (p, "ampA", 0.42f); set (p, "ampR", 0.72f);
        set (p, "friction", 0.06f); set (p, "release", 0.30f); set (p, "unison", 0.33f); set (p, "spread", 0.25f);
        set (p, "level", 1.0f);
    }

    void fBoxTide (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 0.35f, [] (float x) { return bBox (x, 0.5f, 5.0f); });
        b.morph (0.35f, 0.65f, [] (float x) { return bBox (x, 0.5f, 5.0f); }, [] (float x) { return bSine (x); });
        b.fill (0.65f, 1.0f, [] (float x) { return bSine (x); });
        b.ridge (0.5f, 0.05f, 0.25f);
        b.done();
        pin (l, 0.0f, 0.15f); pin (l, 0.6f, 0.15f, 0); pin (l, 2.2f, 0.9f, 2);
        l.tide.on.push_back ({ 0.0f, 0.10f, 2 }); l.tide.on.push_back ({ 1.5f, 0.95f, 2 });
        set (p, "tapPos", 0.6f); set (p, "tapVel", 0.5f); set (p, "hold", 0.7f); set (p, "tone", 0.7f);
        set (p, "release", 0.4f); set (p, "ampA", 0.25f);
    }

    void fPitBass (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 0.4f, [] (float x) { return bPit (x, 0.22f, 0.07f); });
        b.morph (0.4f, 0.7f, [] (float x) { return bPit (x, 0.22f, 0.07f); }, [] (float x) { return bDouble (x, 0.5f, 0.07f); });
        b.fill (0.7f, 1.0f, [] (float x) { return bDouble (x, 0.5f, 0.07f); });
        b.done();
        pin (l, 0.0f, 0.05f); pin (l, 0.8f, 0.05f, 0); pin (l, 3.0f, 0.5f, 2);
        set (p, "tapPos", 0.5f); set (p, "tapVel", 0.6f); set (p, "tapFrc", 0.2f); set (p, "level", 0.85f);
        set (p, "tone", 0.62f); set (p, "strike", 0.75f); set (p, "friction", 0.08f);
        set (p, "voiceMode", 0.5f); set (p, "glide", 0.25f); set (p, "ampR", 0.5f); set (p, "release", 0.55f);
        set (p, "rock", 0.35f); set (p, "rockRatio", 2.0f / 6.0f);
    }

    void fCascade (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bDouble (x, 0.55f, 0.08f); });
        for (int k = 1; k < 5; ++k) b.ridge (k * 0.2f, 0.025f, 0.06f + 0.03f * k);
        b.done();
        pin (l, 0.0f, 0.05f); pin (l, 4.0f, 0.95f, 1);
        l.rock.on.push_back ({ 0.0f, 0.05f, 1 }); l.rock.on.push_back ({ 4.0f, 0.85f, 1 });
        set (p, "tapVel", 0.7f); set (p, "tapPos", 0.4f); set (p, "hold", 0.85f); set (p, "tide", 0.9f);
        set (p, "tone", 0.75f); set (p, "friction", 0.10f); set (p, "ampA", 0.2f); set (p, "ampR", 0.6f);
        set (p, "level", 0.55f);
    }

    void fShelfOrgan (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bShelf (x, 0.5f, 0.25f); });
        b.tilt (0.0f, 0.08f);
        b.done();
        (void) l;
        set (p, "unison", 0.67f); set (p, "spread", 0.5f); set (p, "width", 0.8f);
        set (p, "tapPos", 0.8f); set (p, "tapFrc", 0.3f); set (p, "friction", 0.0f); set (p, "ampA", 0.35f);
        set (p, "ampS", 1.0f); set (p, "ampR", 0.55f); set (p, "hold", 0.9f); set (p, "position", 0.5f);
        set (p, "level", 0.58f);
    }

    void fStranded (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 0.22f, [] (float x) { return bPit (x, 0.2f, 0.09f); });
        b.fill (0.22f, 0.45f, [] (float x) { return bBox (x, 0.4f, 8.0f); });
        b.fill (0.45f, 0.7f, [] (float x) { return bPit (x, 0.2f, 0.06f); });
        b.fill (0.7f, 1.0f, [] (float x) { return bAsym (x, -0.4f); });
        b.ridge (0.22f, 0.02f, 0.5f); b.ridge (0.45f, 0.02f, 0.5f); b.ridge (0.7f, 0.02f, 0.5f);
        b.done();
        pin (l, 0.0f, 0.1f); pin (l, 1.0f, 0.95f, 1);
        set (p, "tide", 0.15f); set (p, "hold", 0.3f); set (p, "tapPos", 0.7f); set (p, "tapVel", 0.4f);
        set (p, "lfo1Target", 0.0f); set (p, "lfo1Amt", 0.75f); set (p, "lfo1Rate", 0.22f);
        set (p, "ampR", 0.7f); set (p, "release", 0.35f);
    }

    void fSeiche (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bBox (x, 0.40f, 6.0f); });
        b.done();
        (void) l;
        set (p, "rockMode", 1.0f); set (p, "rock", 0.55f); set (p, "rockRatio", 1.0f / 6.0f);
        set (p, "friction", 0.35f); set (p, "release", 0.25f); set (p, "ampA", 0.5f); set (p, "ampR", 0.75f);
        set (p, "tapPos", 1.0f); set (p, "tone", 0.6f); set (p, "unison", 0.33f); set (p, "spread", 0.4f);
    }

    void fSpringTide (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 0.5f, [] (float x) { return bSine (x); });
        b.fill (0.5f, 1.0f, [] (float x) { return bPit (x, 0.12f, 0.1f); });
        b.ridge (0.5f, 0.04f, 0.3f);
        b.done();
        pin (l, 0.0f, 0.2f); pin (l, 0.5f, 0.2f, 0); pin (l, 1.0f, 0.8f, 2); pin (l, 1.5f, 0.2f, 2);
        l.pin.hasLoop = true; l.pin.loopA = 0.5f; l.pin.loopB = 1.5f;
        set (p, "lfo2Target", 1.0f / 5.0f); set (p, "lfo2Amt", 0.85f); set (p, "lfo2Rate", 0.28f);
        set (p, "tide", 0.6f); set (p, "hold", 0.75f); set (p, "tapPos", 0.4f); set (p, "tapVel", 0.7f);
        set (p, "unison", 0.33f); set (p, "spread", 0.4f); set (p, "width", 0.7f);
    }

    void fUndertow (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bBox (x, 0.35f, 4.0f); });
        b.tilt (0.0f, 0.15f);
        b.done();
        (void) l;
        set (p, "voiceMode", 0.5f); set (p, "glide", 0.35f); set (p, "tapFrc", 0.6f); set (p, "tapPos", 0.5f);
        set (p, "tone", 0.55f); set (p, "strike", 0.8f); set (p, "friction", 0.05f); set (p, "ampR", 0.45f);
        set (p, "release", 0.6f); set (p, "hold", 0.4f); set (p, "level", 0.62f);
    }

    void fTidePool (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bSine (x); });
        uint32_t s = 7u;
        for (int j = 0; j < NZ; ++j)
            for (int i = 0; i < NX; ++i)
            {
                s = s * 1664525u + 1013904223u;
                const float x = xOf (i);
                const float n = ((float) ((s >> 8) & 0xffff) / 65535.0f - 0.5f) * 0.012f * std::min (1.0f, std::abs (x) * 2.0f);
                t.at (i, j) += std::abs (n);
            }
        for (int pass = 0; pass < 6; ++pass)
            for (int j = 0; j < NZ; ++j)
                for (int i = 1; i < NX - 1; ++i)
                    t.at (i, j) = 0.25f * t.at (i - 1, j) + 0.5f * t.at (i, j) + 0.25f * t.at (i + 1, j);
        b.done();
        (void) l;
        set (p, "servo", 0.6f); set (p, "tapVel", 0.5f); set (p, "tapPos", 0.6f); set (p, "tone", 0.7f);
    }

    void fNacre (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bSine (x); });
        b.done();
        (void) l;
        set (p, "unison", 1.0f); set (p, "spread", 0.6f); set (p, "width", 0.9f);
        set (p, "tapVel", 0.6f); set (p, "tapPos", 0.6f); set (p, "friction", 0.03f);
        set (p, "ampA", 0.45f); set (p, "ampR", 0.7f); set (p, "release", 0.28f); set (p, "tone", 0.8f);
        set (p, "level", 0.95f);
    }

    void fGroundswell (Terrain& t, Lanes& l, Params& p)
    {
        B b (t);
        b.fill (0, 1.0f, [] (float x) { return bDouble (x, 0.6f, 0.12f); });
        b.done();
        pin (l, 0.0f, 0.5f);
        set (p, "rock", 0.5f); set (p, "rockRatio", 3.0f / 6.0f); set (p, "tapVel", 0.8f); set (p, "tapPos", 0.3f);
        set (p, "tone", 0.5f); set (p, "friction", 0.2f); set (p, "release", 0.5f); set (p, "ampA", 0.3f);
        set (p, "voiceMode", 1.0f); set (p, "glide", 0.2f);
    }

    //==========================================================================
    const FactoryPatch FACTORY[] =
    {
        //  STARTERS — safe ground: in tune whatever the strike, no drive
        { "SOFT KEYS",     "STARTERS", sSoftKeys },
        { "SUB",           "STARTERS", sSub },
        { "PLUCK BASS",    "STARTERS", sPluckBass },
        { "WIDE BASS",     "STARTERS", sWideBass },
        { "SQUARE BASS",   "STARTERS", sSquareBass },
        { "SAW LEAD",      "STARTERS", sSawLead },
        { "SQUARE LEAD",   "STARTERS", sSquareLead },
        { "SWEEP LEAD",    "STARTERS", sSweepLead },
        { "GLIDE LEAD",    "STARTERS", sGlideLead },
        { "ORGAN",         "STARTERS", sOrgan },
        { "ELECTRIC PIANO","STARTERS", sElectricPiano },
        { "BELL",          "STARTERS", sBell },
        { "CLAV",          "STARTERS", sClav },
        { "SLOW PAD",      "STARTERS", sSlowPad },
        { "MORPH PAD",     "STARTERS", sMorphPad },
        { "GLASS",         "STARTERS", sGlass },
        { "DRONE",         "STARTERS", sDrone },

        //  TERRAINS — the instrument showing what it is
        { "FIRST LIGHT",   "TERRAINS", fFirstLight },
        { "BOX TIDE",      "TERRAINS", fBoxTide },
        { "PIT BASS",      "TERRAINS", fPitBass },
        { "CASCADE",       "TERRAINS", fCascade },
        { "SHELF ORGAN",   "TERRAINS", fShelfOrgan },
        { "STRANDED",      "TERRAINS", fStranded },
        { "SEICHE",        "TERRAINS", fSeiche },
        { "SPRING TIDE",   "TERRAINS", fSpringTide },
        { "UNDERTOW",      "TERRAINS", fUndertow },
        { "TIDE POOL",     "TERRAINS", fTidePool },
        { "NACRE",         "TERRAINS", fNacre },
        { "GROUNDSWELL",   "TERRAINS", fGroundswell },
    };
}

int numFactory() { return (int) (sizeof (FACTORY) / sizeof (FACTORY[0])); }
const FactoryPatch& factory (int i)
{
    const int n = numFactory();
    return FACTORY[i < 0 ? 0 : (i >= n ? n - 1 : i)];
}
int numStarters()
{
    int n = 0;
    for (int i = 0; i < numFactory(); ++i) if (std::strcmp (FACTORY[i].group, "STARTERS") == 0) ++n;
    return n;
}

} // namespace ht
