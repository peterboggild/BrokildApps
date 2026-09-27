/*  GRAVITY WELL - the engine.

    No JUCE here: the offline bench compiles this file directly, so what the
    bench measures is what the plug-in runs.

    Signal path, per voice, at 4x:

      osc A + osc B (+ ring, + PM) + sub + noise          the accretion disk
        -> HORIZON A / HORIZON B   (series | parallel | split)
        -> drive (power neutral, trim MEASURED at prepare)
        -> + SINGULARITY (the protected core, routed round all of it)
        -> + RINGDOWN (quasi-normal modes, deeper and longer with MASS)
        -> formant bank, REDSHIFTED by sqrt(1 - rs/r)
      then decimate, tilt, ceiling, out.
*/
#include "Engine.h"
#include <cstring>
#include <algorithm>

namespace gw {

// ------------------------------------------------------------ the parameters
static const PSpec kSpecs[] = {
  #define X(id,label,def,lo,hi,kind,ch) { #id, label, (float)(def), (float)(lo), (float)(hi), kind, ch },
    GW_PARAMS(X)
  #undef X
};
const PSpec& spec (int i) { return kSpecs[i]; }

int          numParams()  { return NUM_PARAMS; }

Params::Params() { for (int i = 0; i < NUM_PARAMS; ++i) v[i] = kSpecs[i].def; }

/*  Schwarzschild quasi-normal modes: l=2 n=0,1,2 and l=3,4 n=0, as ratios of
    the fundamental (M*omega = 0.3737 - 0.0890i).  Published numbers, not a
    designer's guess - which is exactly why the body is inharmonic in a way
    that does not sound like a bell anyone has heard.                        */
const float QNM_F[NQNM] = { 1.0000f, 0.9278f, 0.8057f, 1.6039f, 2.1653f };
const float QNM_D[NQNM] = { 0.2381f, 0.7330f, 1.2799f, 0.2480f, 0.2521f };

// ------------------------------------------------------------------ helpers
static inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf  (float a, float b, float t) { return a + (b - a) * t; }
static inline float fastTanh (float x) {
    //  a rational tanh: monotonic, exact-ish to 1e-4, and no branch
    const float x2 = x * x;
    const float n = x * (27.0f + x2);
    const float d = 27.0f + 9.0f * x2;
    return clampf (n / d, -1.0f, 1.0f);
}
static inline float pitchToHz (float midi) { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }

//  0..1 -> Hz, logarithmic, the range a bass instrument actually needs
//  The exponential laws, named once.  expLaw() below hands the SAME numbers to
//  the host text and the panel, so no display can show a value the engine
//  does not use - both used to show 20*1200^v for every "HZ" parameter.
constexpr float CUT_BASE = 18.0f,    CUT_SPAN = 1400.0f;
constexpr float LFO_BASE = 0.02f,    LFO_SPAN = 2000.0f;
constexpr float SEC_BASE = 0.0005f,  SEC_SPAN = 12000.0f;
static inline float cutoffHz (float x) { return CUT_BASE * std::pow (CUT_SPAN, clampf (x, 0.f, 1.f)); }
static inline float lfoHz    (float x) { return LFO_BASE * std::pow (LFO_SPAN, clampf (x, 0.f, 1.f)); }
static inline float timeSec  (float x) { return SEC_BASE * std::pow (SEC_SPAN, clampf (x, 0.f, 1.f)); }

const float LFO_DIV_CPB[N_LFO_DIV] = {
    1.0f / 16.0f, 1.0f / 8.0f, 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f,
    4.0f / 3.0f,  1.5f,        2.0f,  8.0f / 3.0f, 3.0f, 4.0f,        6.0f,  8.0f };
const char* const LFO_DIV_NAME[N_LFO_DIV] = {
    "4/1", "2/1", "1/1", "1/2D", "1/2", "1/4D", "1/2T", "1/4",
    "1/8D", "1/4T", "1/8", "1/16D", "1/8T", "1/16", "1/16T", "1/32" };

bool expLaw (int pid, float& base, float& span)
{
    switch (pid)
    {
        case P_fa_cut: case P_fb_cut: case P_split:
            base = CUT_BASE;          span = CUT_SPAN; return true;
        case P_sing_freq:             //  the core sits two octaves under the cutoff law
            base = CUT_BASE * 0.25f;  span = CUT_SPAN; return true;
        case P_l1_rate: case P_l2_rate: case P_l3_rate: case P_rnd_rate:
            base = LFO_BASE;          span = LFO_SPAN; return true;
        default: break;
    }
    if (spec (pid).kind == GW_KIND_SEC) { base = SEC_BASE; span = SEC_SPAN; return true; }
    return false;
}

struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline float uni()  { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.0f / 16777216.0f); }
    inline float bip()  { return uni() * 2.0f - 1.0f; }
};

struct Smooth {
    float y = 0.f, a = 0.f;
    void  setTau (double sr, float tau) { a = 1.0f - std::exp (-1.0f / (float) (sr * std::max (tau, 1e-5f))); }
    inline float tick (float target) { y += a * (target - y); return y; }
    void  snap (float v) { y = v; }
};

/*  DC has to be blocked AFTER the ceiling, and the hard bound has to be LAST:
    an odd saturator on an asymmetric wave has non-zero mean, and a blocker's
    own transient overshoot escapes a ceiling placed before it.  (B2311.104)  */
struct DCBlock {
    float x1 = 0.f, y1 = 0.f, R = 0.9995f;
    void setCorner (double sr, float hz) { R = 1.0f - 6.2831853f * hz / (float) sr; }
    inline float tick (float x) { float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.f; }
};

/*  A 2x half-band decimator.  The oscillators are generated at the oversampled
    rate, so nothing is ever UPsampled - only this, twice, on the way out.    */
struct Decim2 {
    static const int N = 32;
    float h[N] {}, z[N] {};
    int   pos = 0;
    void init() {
        //  windowed sinc at fs/4, Blackman window, normalised to unity DC
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            double n = i - (N - 1) / 2.0;
            double s = (std::abs (n) < 1e-9) ? 0.5 : std::sin (M_PI * 0.5 * n) / (M_PI * n);
            double w = 0.42 - 0.5 * std::cos (2.0 * M_PI * i / (N - 1))
                            + 0.08 * std::cos (4.0 * M_PI * i / (N - 1));
            h[i] = (float) (s * w);
            sum += h[i];
        }
        for (int i = 0; i < N; ++i) h[i] = (float) (h[i] / sum);
        reset();
    }
    void reset() { std::memset (z, 0, sizeof (z)); pos = 0; }
    inline void push (float x) { z[pos] = x; pos = (pos + 1) & (N - 1); }
    inline float read() const {
        float acc = 0.f; int p = pos;
        for (int i = N - 1; i >= 0; --i) { acc += h[i] * z[p]; p = (p + 1) & (N - 1); }
        return acc;
    }
    //  two in, one out
    inline float tick (float a, float b) { push (a); push (b); return read(); }
};

// ---------------------------------------------------------------- the filters
struct OnePoleTPT {
    float G = 0.f, s = 0.f;
    void setG (float g) { G = g; }
    inline float lp (float x) { float v = (x - s) * G; float y = v + s; s = y + v; return y; }
    void reset() { s = 0.f; }
};

/*  Moog ladder, zero-delay, with a tanh at the summing node and a gentle
    cubic per stage - so a screaming resonance stays a NOTE rather than
    collapsing into a square.                                                */
struct Ladder {
    OnePoleTPT p[4];
    float G = 0.f, k = 0.f, sat = 1.f;
    void set (double sr, float hz, float res, float drive) {
        float wd = 6.2831853f * clampf (hz, 10.f, (float) sr * 0.45f);
        float T  = 1.0f / (float) sr;
        float wa = (2.0f / T) * std::tan (clampf (wd * T * 0.5f, 1e-5f, 1.55f));
        G = wa * T * 0.5f; G = G / (1.0f + G);
        for (auto& s : p) s.setG (G);
        k   = 4.0f * clampf (res, 0.f, 1.02f);
        sat = 1.0f + 6.0f * drive * drive;
    }
    inline float tick (float x) {
        float G4 = G * G * G * G;
        float S  = p[3].s / (1.0f + G) + p[2].s * G / (1.0f + G)
                 + p[1].s * G * G / (1.0f + G) + p[0].s * G * G * G / (1.0f + G);
        float u  = (x * sat - k * S) / (1.0f + k * G4);
        u = fastTanh (u);
        float y = p[3].lp (p[2].lp (p[1].lp (p[0].lp (u))));
        return y * (1.0f / (1.0f + 0.25f * k));      // resonance must not eat the level
    }
    void reset() { for (auto& s : p) s.reset(); }
};

/*  Sallen-Key (K35), the MS-20 shape.  GROWL only screams at the top of the
    knob; SCREAM crosses K = 2 at noon and self-oscillates from there.       */
struct K35 {
    OnePoleTPT lp1, lp2;
    float G = 0.f, K = 0.f;
    bool  hp = false;
    void set (double sr, float hz, float res, bool scream, bool highpass) {
        float wd = 6.2831853f * clampf (hz, 10.f, (float) sr * 0.45f);
        float T  = 1.0f / (float) sr;
        //  the clipper takes its own share of the loop, so the prewarp is not
        //  the textbook one - Black Rider measured these
        float lift = scream ? 1.0099f : 1.0035f;
        float wa = (2.0f / T) * std::tan (clampf (wd * T * 0.5f * lift, 1e-5f, 1.55f));
        G = wa * T * 0.5f; G = G / (1.0f + G);
        lp1.setG (G); lp2.setG (G);
        K  = scream ? (0.35f + 2.55f * res) : (0.20f + 1.80f * res);
        hp = highpass;
    }
    inline float tick (float x) {
        float y1 = lp1.lp (x);
        float fb = lp2.s;
        float u  = y1 + K * fb;
        u = fastTanh (u * 0.8f) * 1.25f;             // the diode clipper
        float y2 = lp2.lp (u);
        float out = hp ? (x - y2) : y2;
        return out * (1.0f / (1.0f + 0.30f * K));
    }
    void reset() { lp1.reset(); lp2.reset(); }
};

struct SVF {
    float g = 0.f, k = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f, ic1 = 0.f, ic2 = 0.f;
    int   mode = 0;                                   // 0 LP 1 BP 2 HP 3 NOTCH
    void set (double sr, float hz, float res, int m) {
        g = std::tan (clampf (3.1415927f * clampf (hz, 10.f, (float) sr * 0.45f) / (float) sr, 1e-5f, 1.53f));
        k = 2.0f - 1.96f * clampf (res, 0.f, 1.f);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        mode = m;
    }
    inline float tick (float x) {
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        switch (mode) {
            case 1:  return v1;
            case 2:  return x - k * v1 - v2;
            case 3:  return x - k * v1;
            default: return v2;
        }
    }
    void reset() { ic1 = ic2 = 0.f; }
};

/*  The 303's ladder: 18 dB, and the feedback path is what gives it its
    particular squelch rather than the pole count.                           */
/*  THE 303 FILTER, and the three things that make one.

    ONE: three poles, not four.  A TB-303 diode ladder measures about
    18 dB/oct where a Moog ladder is 24, so it lets more of the buzz
    through above the cutoff.

    TWO, and this is the one that matters: A MOOG LADDER SUBTRACTS THE
    FEEDBACK FROM THE INPUT, so its low end falls as 1/(1 + k*W) and a
    resonant patch goes THIN.  Measured on the old code here: the
    fundamental lost 20.5 dB between resonance 0.05 and 0.90 - the exact
    opposite of a 303, which stays fat while it screams, and that is most
    of what people mean by acid.  The linear loop gain at DC is k*W with
    W the sum of the state weights, so the passband is restored by (1 +
    k*W) and the resonant peak then rises ABOVE a level low end instead of
    the low end sinking below the peak.

    THREE: the diodes.  A symmetric tanh makes odd harmonics only.  Real
    diodes clip ASYMMETRICALLY and the even harmonics are the growl.
    tanh(x + b) - tanh(b) is asymmetric, bounded, monotonic, and exactly
    zero at zero - so it colours the sound without pushing DC into a loop
    full of lowpasses at rest.                                            */
struct Diode {
    OnePoleTPT p[3];
    float G = 0.f, k = 0.f, comp = 1.f;
    static constexpr float W    = 1.85f;   // 1 + 0.55 + 0.30, the state weights
    static constexpr float BIAS = 0.35f;   // the diode is not symmetric
    void set (double sr, float hz, float res) {
        float wd = 6.2831853f * clampf (hz, 10.f, (float) sr * 0.45f);
        float T  = 1.0f / (float) sr;
        float wa = (2.0f / T) * std::tan (clampf (wd * T * 0.5f, 1e-5f, 1.55f));
        G = wa * T * 0.5f; G = G / (1.0f + G);
        for (auto& s : p) s.setG (G);
        k = 3.4f * clampf (res, 0.f, 1.0f);
        //  hold the passband instead of letting resonance eat it
        comp = 1.0f + W * k;
    }
    inline float tick (float x) {
        float S  = p[2].s + 0.55f * p[1].s + 0.30f * p[0].s;
        float fb = fastTanh (S * 0.7f + BIAS) - fastTanh (BIAS);
        float u  = x - k * fb;
        float y  = p[2].lp (p[1].lp (p[0].lp (u)));
        return y * comp;
    }
    void reset() { for (auto& s : p) s.reset(); }
};

/*  A tuned comb.  THE TRAP, and it is serious down here: any interpolation
    inside a recirculating delay is a lowpass applied hundreds of times a
    second, so a comb tuned to a low note comes out dull AND flat.  Integer
    delay in the loop; the fractional part is corrected by a one-pole allpass
    OUTSIDE it, which cannot accumulate.  (Thin Walls measured this.)        */
struct Comb {
    std::vector<float> buf;
    int   w = 0, len = 2;
    float fb = 0.f, damp = 0.f, dz = 0.f, apC = 0.f, apZ = 0.f;
    bool  negative = false;
    void prepare (double sr) { buf.assign ((size_t) (sr * 0.06) + 8, 0.f); reset(); }
    void reset() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; dz = apZ = 0.f; }
    void set (double sr, float hz, float res, bool neg) {
        float d = (float) sr / clampf (hz, 20.f, 8000.f);
        int   i = (int) d;
        float frac = d - (float) i;
        if (i < 2) i = 2;
        if (i > (int) buf.size() - 2) i = (int) buf.size() - 2;
        len = i;
        apC = (1.0f - frac) / (1.0f + frac);          // the fractional part, outside the loop
        fb  = clampf (res, 0.f, 0.985f);
        damp = 0.18f;
        negative = neg;
    }
    inline float tick (float x) {
        float y = buf[(size_t) ((w - len + (int) buf.size()) % (int) buf.size())];
        dz = y + damp * (dz - y);
        float into = x + (negative ? -fb : fb) * dz;
        buf[(size_t) w] = into;
        w = (w + 1) % (int) buf.size();
        float ap = apC * (y - apZ) + apZ; apZ = y;    // one allpass, once
        return ap;
    }
};

struct Formant {
    SVF b[3];
    float gain[3] { 1.f, 0.7f, 0.5f };
    //  five vowels, morphed - the growl and the talking bass live here.
    //  ORDERED u-o-a-e-i so the second formant only ever rises along the
    //  morph (870 840 1090 1840 2290).  The old a-e-i-o-u order dropped it
    //  1450 Hz between i and o, and a sweep lurched from vowel to vowel -
    //  Peter heard it on GROWL as stepping.
    void set (double sr, float morph, float res, float shift) {
        static const float F[5][3] = {
            {  300.f,  870.f, 2240.f },   // u
            {  570.f,  840.f, 2410.f },   // o
            {  730.f, 1090.f, 2440.f },   // a
            {  530.f, 1840.f, 2480.f },   // e
            {  270.f, 2290.f, 3010.f } }; // i
        float x = clampf (morph, 0.f, 0.9999f) * 4.0f;
        int   i = (int) x; float t = x - (float) i;
        for (int k = 0; k < 3; ++k) {
            float f = lerpf (F[i][k], F[i + 1 > 4 ? 4 : i + 1][k], t) * shift;
            b[k].set (sr, f, 0.55f + 0.44f * res, 1);
        }
    }
    inline float tick (float x) {
        return b[0].tick (x) * gain[0] + b[1].tick (x) * gain[1] + b[2].tick (x) * gain[2];
    }
    void reset() { for (auto& f : b) f.reset(); }
};

/*  The ringdown.  Ratios from the published quasi-normal modes; the absolute
    decay is a knob, because a physically exact tau of a stellar-mass hole is
    microseconds and nobody wants that.  omega ~ 1/M and tau ~ M are honoured,
    so more MASS really does ring lower and longer.                          */
struct Ringdown {
    struct Mode { float a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f, g = 0.f; };
    Mode m[NQNM];
    void set (double sr, float baseHz, float decay, float massNorm) {
        float M = 1.0f + 3.0f * massNorm;             // heavier -> lower, longer
        for (int i = 0; i < NQNM; ++i) {
            float f   = clampf (baseHz * QNM_F[i] / M, 12.f, (float) sr * 0.45f);
            float tau = (0.02f + 2.6f * decay * decay) * M / std::max (QNM_D[i], 0.02f);
            float r   = std::exp (-1.0f / (float) (sr * tau));
            float w   = 6.2831853f * f / (float) sr;
            m[i].a1 = 2.0f * r * std::cos (w);
            m[i].a2 = -r * r;
            m[i].g  = (1.0f - r) * (i == 0 ? 1.0f : 0.55f / (1.0f + 0.5f * (float) i));
        }
    }
    inline float tick (float x) {
        float out = 0.f;
        for (int i = 0; i < NQNM; ++i) {
            float y = m[i].g * x + m[i].a1 * m[i].z1 + m[i].a2 * m[i].z2;
            m[i].z2 = m[i].z1; m[i].z1 = y;
            out += y;
        }
        return out * 0.45f;
    }
    void reset() { for (auto& q : m) { q.z1 = q.z2 = 0.f; } }
};

// ------------------------------------------------------------- the modulators
struct Env {
    enum Stage { Idle, Att, Dec, Sus, Rel } st = Idle;
    float y = 0.f, curve = 0.f;
    float ta = 0.01f, td = 0.2f, su = 0.7f, tr = 0.2f;
    double sr = 48000.0;
    void gate (bool on) { st = on ? Att : (st == Idle ? Idle : Rel); }
    void hardReset() { st = Idle; y = 0.f; }
    inline float tick (float dilate) {
        //  dilation slows an envelope exactly as it slows an LFO: it is a
        //  clock, and the well is what the clock runs in
        float d = std::max (dilate, 1e-4f);
        switch (st) {
            case Att: { float k = 1.0f - std::exp (-1.0f / (float) (sr * std::max (ta, 1e-4f) / d));
                        y += k * (1.05f - y); if (y >= 1.0f) { y = 1.0f; st = Dec; } } break;
            case Dec: { float k = 1.0f - std::exp (-1.0f / (float) (sr * std::max (td, 1e-4f) / d));
                        y += k * (su - 0.02f - y); if (y <= su + 0.001f) { y = su; st = Sus; } } break;
            case Sus:   y = su; break;
            case Rel: { float k = 1.0f - std::exp (-1.0f / (float) (sr * std::max (tr, 1e-4f) / d));
                        y += k * (-0.02f - y); if (y <= 0.0005f) { y = 0.f; st = Idle; } } break;
            default:    y = 0.f;
        }
        float lin = clampf (y, 0.f, 1.f);
        //  CURVE: exponential punches, linear swells.  The difference between
        //  a techno pluck and a doom swell, and it has to be reachable.
        return lerpf (lin * lin, lin, clampf (curve, 0.f, 1.f));
    }
    bool active() const { return st != Idle; }
};

struct Lfo {
    float ph = 0.f, hold = 0.f;
    int   shape = 0;
    Rng   rng;
    inline float tick (double sr, float hz, float dilate, bool frozen) {
        if (frozen) return hold;                       // the horizon: hold, do not reset
        ph += (float) (hz * dilate / sr);
        if (ph >= 1.f) ph -= std::floor (ph);
        float v;
        switch (shape) {
            case 1:  v = 4.0f * std::fabs (ph - 0.5f) - 1.0f; break;                 // tri
            case 2:  v = 2.0f * ph - 1.0f; break;                                    // saw
            case 3:  v = ph < 0.5f ? 1.0f : -1.0f; break;                            // square
            case 4: { //  the inspiral: f ~ (t_c - t)^(-3/8), the real chirp law
                     float u = clampf (1.0f - ph, 1e-3f, 1.f);
                     float acc = std::pow (u, -0.375f) - 1.0f;
                     v = std::sin (6.2831853f * clampf (acc * 0.35f, 0.f, 40.f)); } break;
            default: v = std::sin (6.2831853f * ph);
        }
        hold = v;
        return v;
    }
};

// ------------------------------------------------------------- oscillators
struct WaveTables {
    //  Eight tables, each a harmonic recipe.  Generated once; band-limiting
    //  is by harmonic count against the note, so nothing ever aliases.
    static const int LEN = 2048, NT = 8, NH = 64;
    float amp[NT][NH] {};
    void init() { fillAmps(); (void) bank(); }
    void fillAmps() {
        for (int t = 0; t < NT; ++t)
            for (int h = 1; h <= NH; ++h) {
                float n = (float) h, a = 0.f;
                switch (t) {
                    case 0: a = 1.0f / n; break;                                    // BASIC (saw)
                    case 1: a = (h % 2) ? 1.0f / n : 0.0f; break;                   // OCTAVE (square)
                    case 2: a = std::exp (-std::pow ((n - 5.f) / 3.2f, 2.f))        // FORMANT
                              + 0.6f * std::exp (-std::pow ((n - 13.f) / 4.f, 2.f)); break;
                    case 3: a = (h % 2) ? 1.2f / (n * n) : 0.9f / n; break;         // METAL
                    case 4: a = std::exp (-n * 0.09f) * (1.f + 0.5f * std::sin (n * 1.7f)); break; // GLASS
                    case 5: a = 1.0f / (n * (1.f + 0.35f * std::sin (n * 0.8f))); break;  // GROWL
                    case 6: a = (h <= 3 ? 1.0f / n : 0.8f / std::sqrt (n)) * ((h % 3) ? 1.f : 0.4f); break; // FOLD
                    default: a = 1.0f / n * (1.0f + 0.7f * std::sin (n * 2.399f));   // SWARM
                }
                amp[t][h - 1] = a;
            }
    }
    //  ---- the fast path: precomputed, band-limited, shared by every instance
    static const int NP = 17, NM = 7;           //  shape slices, bandwidths 1 2 4 .. 64
    static const std::vector<float>& bank() {
        static const std::vector<float> t = [] {
            WaveTables w; w.fillAmps();
            std::vector<float> s ((size_t) LEN);
            for (int i = 0; i < LEN; ++i) s[(size_t) i] = (float) std::sin (6.283185307179586 * i / LEN);
            std::vector<float> out ((size_t) NT * NP * NM * LEN, 0.f);
            std::vector<double> acc ((size_t) LEN);
            for (int tb = 0; tb < NT; ++tb)
                for (int p = 0; p < NP; ++p)
                    for (int m = 0; m < NM; ++m) {
                        const int H = 1 << m;
                        const double tilt = 0.5 + 2.5 * (double) p / (NP - 1);
                        std::fill (acc.begin(), acc.end(), 0.0);
                        for (int h = 1; h <= H; ++h) {
                            //  the same law as readExact, so the tables ARE it
                            const double a = w.amp[tb][h - 1] * std::exp (-(double) (h - 1) / (tilt * (double) H));
                            if (a == 0.0) continue;
                            for (int i = 0; i < LEN; ++i) acc[(size_t) i] += a * s[(size_t) ((h * i) & (LEN - 1))];
                        }
                        float* dst = &out[(((size_t) tb * NP + p) * NM + m) * LEN];
                        for (int i = 0; i < LEN; ++i) dst[i] = (float) (acc[(size_t) i] * 0.85);   //  readExact's own output scale
                    }
            return out;
        }();
        return t;
    }
    inline float read (int table, float phase, float pos, int maxH) const {
        //  band-limited by construction: the bandwidth is the largest power of
        //  two at or under the note's own limit, so nothing ever aliases.  A
        //  bass note has room for all 64 and reads exactly what it always did.
        const std::vector<float>& B = bank();
        int H = maxH < 1 ? 1 : (maxH > NH ? NH : maxH);
        int m = 0; while (m < NM - 1 && (2 << m) <= H) ++m;
        const float px = clampf (pos, 0.f, 1.f) * (float) (NP - 1);
        int p0 = (int) px; if (p0 > NP - 2) p0 = NP - 2;
        const float pf = px - (float) p0;
        float ph = phase - std::floor (phase);
        const float x = ph * (float) LEN;
        const int i0 = (int) x & (LEN - 1), i1 = (i0 + 1) & (LEN - 1);
        const float f = x - std::floor (x);
        const float* a = &B[(((size_t) table * NP + p0) * NM + m) * LEN];
        const float* b = a + (size_t) NM * LEN;          //  next shape slice
        const float va = a[i0] + (a[i1] - a[i0]) * f;
        const float vb = b[i0] + (b[i1] - b[i0]) * f;
        return va + (vb - va) * pf;
    }
    //  ---- the reference: additive at the note's own harmonic limit, kept so
    //  the bench can prove the tables match it
    inline float readExact (int table, float phase, float pos, int maxH) const {
        float out = 0.f, tw = 6.2831853f * phase;
        int H = maxH < 1 ? 1 : (maxH > NH ? NH : maxH);
        float tilt = 0.5f + 2.5f * clampf (pos, 0.f, 1.f);
        for (int h = 1; h <= H; ++h) {
            float a = amp[table][h - 1] * std::exp (-(float) (h - 1) / (tilt * (float) H));
            out += a * std::sin (tw * (float) h);
        }
        return out * 0.85f;
    }
};

struct Osc {
    float ph = 0.f, inc = 0.f, last = 0.f;
    inline float polyBlep (float t, float dt) const {
        if (t < dt)            { float x = t / dt;        return x + x - x * x - 1.0f; }
        if (t > 1.0f - dt)     { float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
        return 0.0f;
    }
    //  shape morphs saw -> square -> triangle -> sine, all band-limited
    inline float analogue (float shape, float width) {
        float dt = inc;
        float saw = 2.0f * ph - 1.0f - polyBlep (ph, dt);
        float p2  = ph + (1.0f - width); if (p2 >= 1.0f) p2 -= 1.0f;
        float sq  = ((ph < width) ? 1.0f : -1.0f) + polyBlep (ph, dt) - polyBlep (p2, dt);
        float tri = 2.0f * std::fabs (2.0f * ph - 1.0f) - 1.0f;
        float sin_ = std::sin (6.2831853f * ph);
        float x = clampf (shape, 0.f, 0.9999f) * 3.0f;
        int   i = (int) x; float t = x - (float) i;
        const float tab[4] = { saw, sq, tri, sin_ };
        return lerpf (tab[i], tab[i + 1 > 3 ? 3 : i + 1], t);
    }
    inline bool advance() { ph += inc; if (ph >= 1.0f) { ph -= 1.0f; return true; } return false; }
};

double wavetableWorstError() {
    WaveTables w; w.init();
    double worst = 0.0;
    for (int t = 0; t < WaveTables::NT; ++t)
        for (float pos : { 0.f, 0.375f, 1.f })
            for (int H : { 64, 16 }) {
                double err = 0.0, pk = 1e-9;
                for (int i = 0; i < 4096; ++i) {
                    const float ph = (float) i / 4096.f;
                    const double ex = w.readExact (t, ph, pos, H), fa = w.read (t, ph, pos, H);
                    err = std::max (err, std::fabs (ex - fa)); pk = std::max (pk, std::fabs (ex));
                }
                worst = std::max (worst, err / pk);
            }
    return worst;
}

} // namespace gw

#include "EngineImpl.inl"
