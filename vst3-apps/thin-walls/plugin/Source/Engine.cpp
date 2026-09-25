#include "Engine.h"
#include "Geometry.h"
#include "HrtfData.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE__)
 #include <xmmintrin.h>
 #define TW_SSE 1
#else
 #define TW_SSE 0
#endif

namespace tw
{

//==============================================================================
// the apartment
const Room ROOMS[NUM_ROOMS] =
{
    { "LARGE", 0.0f,  6.0f, 0.0f, 5.0f, 2.8f },
    { "SMALL", 3.0f,  6.0f, 5.0f, 8.5f, 2.5f },
    { "GIANT", 6.0f, 18.0f, 0.0f, 9.0f, 5.0f },
};

const Door DOORS[NUM_DOORS] =
{
    { "SMALL-LARGE", 1, 0, 1, 5.0f, 4.05f, 4.95f, true,  2.05f },
    { "LARGE-GIANT", 0, 2, 0, 6.0f, 2.05f, 2.95f, true,  2.05f },
    { "SMALL-GIANT", 1, 2, 0, 6.0f, 6.05f, 6.95f, false, 2.05f },
};

const char* MATERIAL_NAMES[NUM_MATERIALS] = { "ABSORBING", "FURNISHED", "PLASTER", "TILED", "STUDIO" };

/*  Octave-band absorption coefficients, 125 .. 8000 Hz, one profile for every
    surface of the room. Drawn from the standard tables (Kuttruff, Vorlander):
    heavy drapes + thick carpet + soft furniture; a furnished living room
    (plaster, carpet, furniture, curtains); bare plaster on brick with a wooden
    floor; glazed tiles with a little glass and wood. */
const float MATERIAL_ALPHA[NUM_MATERIALS][NBAND] =
{
    { 0.20f, 0.40f, 0.60f, 0.75f, 0.80f, 0.80f, 0.75f },
    { 0.10f, 0.12f, 0.16f, 0.20f, 0.24f, 0.28f, 0.32f },
    { 0.03f, 0.04f, 0.05f, 0.06f, 0.07f, 0.08f, 0.10f },
    { 0.02f, 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.05f },
    /*  STUDIO. Not guessed: solved from Eyring for a decay that does not move
        across the band, -S ln(1-alpha) + 4mV held constant at 0.161 V / 0.70 s
        for the hall. It comes out as CONSTANT absorption near a quarter, with a
        little relief at the very top to pay for air absorption - which is what
        broadband absorbers and bass traps are. 0.70 s in the hall, 0.39 in the
        living room, 0.27 in the box room: treated, and still live. */
    { 0.252f, 0.252f, 0.251f, 0.249f, 0.245f, 0.233f, 0.188f },
};

/*  How much of a reflection leaves the specular direction (ISO 17497). The four
    original materials are zero on purpose: every room that sounds a particular
    way today still does, sample for sample. A studio's diffusers take hold above
    their design frequency, which is why this rises with it. */
const float MATERIAL_SCATTER[NUM_MATERIALS][NBAND] =
{
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0.10f, 0.22f, 0.38f, 0.52f, 0.60f, 0.65f, 0.68f },
};

// splayed walls, as metres of deterministic perturbation per reflection order
const float MATERIAL_SPLAY[NUM_MATERIALS] = { 0, 0, 0, 0, 0.11f };

const float BAND_HZ[NBAND] = { 125, 250, 500, 1000, 2000, 4000, 8000 };

/*  The furniture catalogue. Absorption is the EQUIVALENT ABSORPTION AREA of the
    whole object in m^2 per octave band, the form the tables give for objects
    (Kuttruff, Room Acoustics, and the usual per-seat and per-person figures):
    an upholstered three-seater is about three upholstered seats; a person
    standing is Kuttruff's standing adult; the rug is a cut-pile carpet's alpha
    times its area (the floor under it is subtracted where it is used); the
    curtain is a heavy pleated drape. The hard pieces absorb little and matter
    by what they block and reflect. Scattering areas are an equivalent
    rough-surface area: books and upholstery high, a flat-fronted wardrobe low. */
const float PANEL_ALPHA[NBAND] = { 0.25f, 0.60f, 0.95f, 0.99f, 0.99f, 0.99f, 0.99f };

const FurnSpec FURN[NUM_FURN_TYPES] =
{
    //  id          name            w     d     h     zb    zt    occl   top    topA   floor  absorption 125 .. 8k                               scatter
    { "sofa",     "SOFA",         2.10f, 0.90f, 0.85f, 0.00f, 0.85f, true,  false, 0.00f, false, { 0.60f, 1.10f, 1.50f, 1.70f, 1.75f, 1.70f, 1.60f }, 3.0f },
    { "armchair", "ARMCHAIR",     0.85f, 0.85f, 0.90f, 0.00f, 0.90f, true,  false, 0.00f, false, { 0.22f, 0.40f, 0.52f, 0.58f, 0.60f, 0.58f, 0.55f }, 1.5f },
    { "bed",      "BED",          2.05f, 1.60f, 0.55f, 0.00f, 0.55f, true,  false, 0.00f, false, { 0.35f, 0.70f, 1.30f, 1.70f, 1.85f, 1.85f, 1.75f }, 3.0f },
    { "rug",      "RUG",          2.40f, 1.70f, 0.02f, 0.00f, 0.02f, false, false, 0.00f, true,  { 0.08f, 0.24f, 0.57f, 1.51f, 2.45f, 2.65f, 2.65f }, 0.0f },
    { "curtain",  "CURTAIN",      2.40f, 0.15f, 2.50f, 0.00f, 2.50f, false, false, 0.00f, false, { 0.84f, 2.10f, 3.30f, 4.32f, 4.20f, 3.90f, 3.60f }, 1.0f },
    { "bookcase", "BOOKCASE",     1.00f, 0.35f, 2.00f, 0.00f, 2.00f, true,  false, 0.00f, false, { 0.20f, 0.30f, 0.50f, 0.60f, 0.60f, 0.60f, 0.60f }, 4.0f },
    { "table",    "TABLE",        1.60f, 0.90f, 0.75f, 0.71f, 0.75f, true,  true,  0.07f, false, { 0.10f, 0.08f, 0.06f, 0.05f, 0.05f, 0.05f, 0.05f }, 1.5f },
    { "piano",    "GRAND PIANO",  2.10f, 1.50f, 1.00f, 0.35f, 1.00f, true,  true,  0.05f, false, { 0.30f, 0.25f, 0.15f, 0.10f, 0.08f, 0.08f, 0.08f }, 2.5f },
    { "wardrobe", "WARDROBE",     1.20f, 0.60f, 2.10f, 0.00f, 2.10f, true,  false, 0.00f, false, { 0.10f, 0.08f, 0.06f, 0.05f, 0.05f, 0.05f, 0.05f }, 2.0f },
    { "person",   "PERSON",       0.50f, 0.30f, 1.75f, 0.00f, 1.75f, true,  false, 0.00f, false, { 0.15f, 0.33f, 0.44f, 0.46f, 0.50f, 0.50f, 0.50f }, 1.0f },
};

namespace
{
    constexpr float PI = 3.14159265358979f;

    // ISO 9613-1, 20 C, 50 % RH, 1 atm: attenuation in dB per km, octave bands
    const float AIR_DB_PER_KM[NBAND] = { 0.38f, 1.13f, 2.4f, 4.5f, 8.7f, 22.4f, 71.0f };

    // transmission loss of a light interior door leaf (mass law softened by the
    // leakage round its edges) and of a party wall between the rooms
    const float LEAF_TL_DB[NBAND] = { 14, 17, 20, 23, 26, 29, 32 };
    const float WALL_TL_DB[NBAND] = { 30, 33, 37, 41, 45, 49, 53 };

    // the loudspeaker: a two-way box
    constexpr float WOOFER_RADIUS = 0.08f;      // 6.5 inch cone
    constexpr float TWEETER_RADIUS = 0.0125f;   // 1 inch dome
    constexpr float CROSSOVER_HZ = 2200.0f;
    constexpr float BOX_REAR_DB = 22.0f;        // what a finite baffle costs straight behind at high frequency
    constexpr float BOX_CORNER_HZ = 900.0f;     // and the frequency around which that sets in

    inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float linToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
    inline float deg (float rad)    { return rad * 180.0f / PI; }
    inline float rad (float d)      { return d * PI / 180.0f; }

    int nextPrime (int n)
    {
        auto isPrime = [] (int v) { if (v < 2) return false; for (int i = 2; i * i <= v; ++i) if (v % i == 0) return false; return true; };
        while (! isPrime (n)) ++n;
        return n;
    }

    // Bessel J1: series below 3, Hankel's asymptotic form above
    float besselJ1 (float x)
    {
        if (x < 3.0f)
        {
            const float x2 = x * x;
            return x * (0.5f - x2 * (1.0f / 16.0f - x2 * (1.0f / 384.0f - x2 * (1.0f / 18432.0f - x2 / 1474560.0f))));
        }
        const float chi = x - 0.75f * PI;
        const float p = 1.0f - 15.0f / (128.0f * x * x);
        const float q = 3.0f / (8.0f * x);
        return std::sqrt (2.0f / (PI * x)) * (p * std::cos (chi) - q * std::sin (chi));
    }

    // |2 J1(x) / x|, the circular piston's pattern, x = k a sin(theta)
    inline float piston (float x)
    {
        if (x < 1e-4f) return 1.0f;
        return std::abs (2.0f * besselJ1 (x) / x);
    }

    // which wall (0 x0, 1 x1, 2 y0, 3 y1) of the room the door sits in, -1 if none
    int doorWall (int room, int door)
    {
        const Room& r = ROOMS[room]; const Door& d = DOORS[door];
        if (d.roomA != room && d.roomB != room) return -1;
        if (d.axis == 0) return (std::abs (d.pos - r.x1) < 1e-4f) ? 1 : 0;
        return (std::abs (d.pos - r.y1) < 1e-4f) ? 3 : 2;
    }

    // the open strip of a door in span coordinates
    void openStrip (int door, float a, float& lo, float& hi)
    {
        const Door& d = DOORS[door];
        const float w = d.width() * std::max (0.0f, std::min (1.0f, a));
        if (d.hingeAtS0) { lo = d.s1 - w; hi = d.s1; }
        else             { lo = d.s0;     hi = d.s0 + w; }
    }

    inline float spanCoord (const Door& d, const Vec3& p) { return d.axis == 0 ? p.y : p.x; }
    inline float planeCoord (const Door& d, const Vec3& p) { return d.axis == 0 ? p.x : p.y; }
    inline Vec3 fromSpan (const Door& d, float s, float z)
    {
        return d.axis == 0 ? Vec3 (d.pos, s, z) : Vec3 (s, d.pos, z);
    }

    // the point where segment P->Q crosses the door's plane (P and Q on opposite sides)
    bool crossPlane (const Door& d, const Vec3& P, const Vec3& Q, Vec3& X)
    {
        const float a = planeCoord (d, P) - d.pos, b = planeCoord (d, Q) - d.pos;
        if ((a > 0) == (b > 0) && std::abs (a) > 1e-6f && std::abs (b) > 1e-6f) return false;
        const float t = (std::abs (a - b) < 1e-9f) ? 0.5f : a / (a - b);
        X = P + (Q - P) * t;
        return true;
    }

    // the party wall shared by two rooms: axis/pos/extent, or none
    bool sharedWall (int ra, int rb, int& axis, float& pos, float& s0, float& s1, float& h)
    {
        const Room& A = ROOMS[ra]; const Room& B = ROOMS[rb];
        if (std::abs (A.x1 - B.x0) < 1e-4f || std::abs (B.x1 - A.x0) < 1e-4f)
        {
            axis = 0; pos = (std::abs (A.x1 - B.x0) < 1e-4f) ? A.x1 : B.x1;
            s0 = std::max (A.y0, B.y0); s1 = std::min (A.y1, B.y1); h = std::min (A.h, B.h);
            return s1 > s0;
        }
        if (std::abs (A.y1 - B.y0) < 1e-4f || std::abs (B.y1 - A.y0) < 1e-4f)
        {
            axis = 1; pos = (std::abs (A.y1 - B.y0) < 1e-4f) ? A.y1 : B.y1;
            s0 = std::max (A.x0, B.x0); s1 = std::min (A.x1, B.x1); h = std::min (A.h, B.h);
            return s1 > s0;
        }
        return false;
    }

    int pairIndex (int ra, int rb)
    {
        const int a = std::min (ra, rb), b = std::max (ra, rb);
        if (a == 0 && b == 1) return 0;
        if (a == 0 && b == 2) return 1;
        return 2;
    }

    // ---- the dot product every path is made of -------------------------------
    // taps are stored REVERSED and zero padded to a multiple of 16, so a
    // convolution output is a contiguous dot product against the history window
    inline float dot (const float* hrev, const float* win, int n)
    {
       #if TW_SSE
        __m128 a0 = _mm_setzero_ps(), a1 = a0, a2 = a0, a3 = a0;
        for (int k = 0; k < n; k += 16)
        {
            a0 = _mm_add_ps (a0, _mm_mul_ps (_mm_loadu_ps (hrev + k),      _mm_loadu_ps (win + k)));
            a1 = _mm_add_ps (a1, _mm_mul_ps (_mm_loadu_ps (hrev + k + 4),  _mm_loadu_ps (win + k + 4)));
            a2 = _mm_add_ps (a2, _mm_mul_ps (_mm_loadu_ps (hrev + k + 8),  _mm_loadu_ps (win + k + 8)));
            a3 = _mm_add_ps (a3, _mm_mul_ps (_mm_loadu_ps (hrev + k + 12), _mm_loadu_ps (win + k + 12)));
        }
        __m128 s = _mm_add_ps (_mm_add_ps (a0, a1), _mm_add_ps (a2, a3));
        float out[4]; _mm_storeu_ps (out, s);
        return out[0] + out[1] + out[2] + out[3];
       #else
        float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
        for (int k = 0; k < n; k += 4)
        {
            a0 += hrev[k] * win[k]; a1 += hrev[k + 1] * win[k + 1];
            a2 += hrev[k + 2] * win[k + 2]; a3 += hrev[k + 3] * win[k + 3];
        }
        return (a0 + a1) + (a2 + a3);
       #endif
    }

    // dest[k] = hdelayed[padLen - 1 - k], so that sum_k dest[k] * win[j + k] over a
    // window ending at the current sample IS the convolution output at j
    void storeReversed (const float* h, int n, int preDelay, float* dest, int padLen)
    {
        std::fill (dest, dest + MAX_TAPS, 0.0f);
        for (int k = 0; k < padLen; ++k)
        {
            const int idx = padLen - 1 - k - preDelay;   // index into h
            dest[k] = (idx >= 0 && idx < n) ? h[idx] : 0.0f;
        }
    }

    // sign patterns for the room networks
    const float SGN_SRC[3][16] = {
        { 1,-1, 1, 1,-1, 1,-1,-1, 1,-1,-1, 1, 1,-1, 1,-1 },
        { 1, 1,-1, 1, 1,-1,-1, 1,-1, 1,-1,-1, 1,-1, 1, 1 },
        { -1,1, 1,-1, 1, 1,-1, 1,-1,-1, 1, 1,-1,-1, 1,-1 } };
    /*  A plain Hadamard is an involution (H/4 squared is the identity), so with
        equal-ish delays the state alternates between a spread pattern and a
        concentrated one and never mixes; the per-line sign flip breaks that.
        The observer has its own signs so it is not blind to the pattern the
        injection puts in. */
    const float SGN_MIX[16] = { 1, 1,-1, 1,-1,-1, 1, 1,-1, 1, 1,-1, 1,-1,-1,-1 };
    const float SGN_OUT[16] = { 1,-1,-1, 1, 1, 1,-1, 1,-1,-1, 1,-1, 1, 1,-1, 1 };
}

//==============================================================================
int roomOf (float x, float y)
{
    for (int r = 0; r < NUM_ROOMS; ++r) if (ROOMS[r].contains (x, y)) return r;
    return -1;
}

Vec3 clampIntoRooms (Vec3 p, float margin)
{
    int r = roomOf (p.x, p.y);
    if (r < 0)
    {
        float best = 1e9f;
        for (int i = 0; i < NUM_ROOMS; ++i)
        {
            const Room& R = ROOMS[i];
            const float dx = std::max ({ R.x0 - p.x, 0.0f, p.x - R.x1 });
            const float dy = std::max ({ R.y0 - p.y, 0.0f, p.y - R.y1 });
            const float d = dx * dx + dy * dy;
            if (d < best) { best = d; r = i; }
        }
    }
    const Room& R = ROOMS[r];
    p.x = std::min (std::max (p.x, R.x0 + margin), R.x1 - margin);
    p.y = std::min (std::max (p.y, R.y0 + margin), R.y1 - margin);
    p.z = std::min (std::max (p.z, 0.1f), R.h - 0.1f);
    return p;
}

//==============================================================================
void BandFilter::setCoeffs (float fs)
{
    fsHz = fs;
    auto coef = [fs] (float fc) { return 1.0f - std::exp (-2.0f * PI * fc / fs); };
    a1 = coef (500.0f); a2 = coef (2000.0f); a3 = coef (8000.0f);
}

/*  The exact response of one digital shelf y = x + k (x - lp x), lp the one-pole
    s += a (x - s): H(z) = 1 + k (1 - a / (1 - (1 - a) z^-1)), in dB at f. */
static float shelfDb (float d, float a, float f, float fs)
{
    const float k = std::pow (10.0f, d * 0.05f) - 1.0f;
    const float w = 2.0f * PI * f / fs;
    const float cw = std::cos (w), sw = std::sin (w);
    const float b = 1.0f - a;
    const float dr = 1.0f - b * cw, di = b * sw;           // denominator
    const float den = dr * dr + di * di;
    const float lr = a * dr / den, li = -a * di / den;     // lp as a complex number
    const float hr = 1.0f + k * (1.0f - lr), hi = -k * li;
    return 10.0f * std::log10 (std::max (1e-12f, hr * hr + hi * hi));
}

void BandFilter::setBandsDb (const float* db)
{
    // the four anchors: 250 Hz sets the gain, then 1 k, 4 k and 8 k are hit
    // EXACTLY by the three shelves (a plain first-order shelf only realises
    // ~80 % of its gain one octave above its corner, so the fit is iterated)
    const float t0 = db[1], t1 = db[3], t2 = db[5], t3 = db[6];
    const float F1 = shelfDb (1.0f, a1, 1000.0f, fsHz), F2 = shelfDb (1.0f, a2, 4000.0f, fsHz), F3 = shelfDb (1.0f, a3, 8000.0f, fsHz);
    float gd = t0, d1 = t1 - t0, d2 = t2 - t1, d3 = (t3 - t2) / F3;
    auto clampD = [] (float v) { return std::max (-48.0f, std::min (24.0f, v)); };
    for (int it = 0; it < 5; ++it)
    {
        auto total = [&] (float f) { return gd + shelfDb (d1, a1, f, fsHz) + shelfDb (d2, a2, f, fsHz) + shelfDb (d3, a3, f, fsHz); };
        gd = t0 - (total (250.0f) - gd);
        d1 = clampD (d1 + (t1 - total (1000.0f)) / F1);
        d2 = clampD (d2 + (t2 - total (4000.0f)) / F2);
        d3 = clampD (d3 + (t3 - total (8000.0f)) / F3);
    }
    g  = dbToLin (gd);
    k1 = dbToLin (d1) - 1.0f;
    k2 = dbToLin (d2) - 1.0f;
    k3 = dbToLin (d3) - 1.0f;
}

//==============================================================================
void Hrtf::prepare (double sampleRate)
{
    fs = sampleRate;
    const int src = hrtfdata::NTAP;
    const double ratio = hrtfdata::FS / fs;                // source samples per output sample
    ntap = (int) std::ceil (src / ratio);
    ntap = std::max (src, std::min (MAX_TAPS - 96, ntap));
    taps.assign ((size_t) hrtfdata::NDIR * 2 * (size_t) ntap, 0.0f);
    itd.assign ((size_t) hrtfdata::NDIR, 0.0f);

    // Lanczos resampling of each minimum-phase response
    const int A = 6;
    std::vector<float> in ((size_t) src);
    for (int d = 0; d < hrtfdata::NDIR; ++d)
    {
        itd[(size_t) d] = hrtfdata::ITD[d] * (float) fs;
        for (int e = 0; e < 2; ++e)
        {
            const short* s = hrtfdata::TAPS + ((size_t) d * 2 + (size_t) e) * (size_t) src;
            for (int i = 0; i < src; ++i) in[(size_t) i] = s[i] * hrtfdata::SCALE;
            float* o = &taps[((size_t) d * 2 + (size_t) e) * (size_t) ntap];
            if (std::abs (ratio - 1.0) < 1e-9)
            {
                for (int i = 0; i < src; ++i) o[i] = in[(size_t) i];
                continue;
            }
            for (int n = 0; n < ntap; ++n)
            {
                const double t = n * ratio;
                const int m0 = (int) std::floor (t) - A + 1, m1 = (int) std::floor (t) + A;
                double acc = 0;
                for (int m = std::max (0, m0); m <= std::min (src - 1, m1); ++m)
                {
                    const double u = t - m;
                    double w = 1.0;
                    if (std::abs (u) > 1e-9)
                    {
                        const double pu = PI * u;
                        w = (std::sin (pu) / pu) * (std::sin (pu / A) / (pu / A));
                    }
                    acc += in[(size_t) m] * w;
                }
                o[n] = (float) acc;
            }
        }
    }
}

void Hrtf::blendDir (int dir, float w, float* l, float* r, float& it) const
{
    if (w <= 0) return;
    const float* tl = &taps[((size_t) dir * 2) * (size_t) ntap];
    const float* tr = tl + ntap;
    for (int i = 0; i < ntap; ++i) { l[i] += w * tl[i]; r[i] += w * tr[i]; }
    it += w * itd[(size_t) dir];
}

void Hrtf::lookup (float azDeg, float elDeg, float* left, float* right, float& itdSamples) const
{
    float az = std::fmod (azDeg, 360.0f); if (az < 0) az += 360.0f;
    bool mirror = false;
    if (az > 180.0f) { az = 360.0f - az; mirror = true; }
    const float el = std::max ((float) hrtfdata::RING_ELEV[0], std::min ((float) hrtfdata::RING_ELEV[hrtfdata::NRING - 1], elDeg));

    // elevation rings are 10 degrees apart
    float rf = (el - hrtfdata::RING_ELEV[0]) / 10.0f;
    int r0 = (int) std::floor (rf); if (r0 >= hrtfdata::NRING - 1) r0 = hrtfdata::NRING - 2;
    const float wr = rf - r0;

    float* L = mirror ? right : left;
    float* R = mirror ? left : right;
    std::fill (L, L + ntap, 0.0f); std::fill (R, R + ntap, 0.0f);
    float it = 0;

    for (int k = 0; k < 2; ++k)
    {
        const int ring = r0 + k;
        const float wring = k == 0 ? 1.0f - wr : wr;
        if (wring <= 0) continue;
        const int n = hrtfdata::RING_N[ring], first = hrtfdata::RING_FIRST[ring];
        if (n == 1) { blendDir (first, wring, L, R, it); continue; }
        const float step = 180.0f / (float) (n - 1);
        float af = az / step;
        int a0 = (int) std::floor (af); if (a0 >= n - 1) a0 = n - 2;
        const float wa = af - a0;
        blendDir (first + a0,     wring * (1.0f - wa), L, R, it);
        blendDir (first + a0 + 1, wring * wa,          L, R, it);
    }
    itdSamples = mirror ? -it : it;
}

//==============================================================================
static double besselI0 (double x)
{
    double s = 1.0, t = 1.0;
    for (int k = 1; k < 60; ++k)
    {
        const double q = x / (2.0 * k);
        t *= q * q; s += t;
        if (t < 1e-18 * s) break;
    }
    return s;
}

FracDelay::FracDelay()
{
    const double beta = 8.6;
    const double i0b = besselI0 (beta);
    for (int p = 0; p < PHASES; ++p)
    {
        const double f = (double) p / (double) PHASES;
        double sum = 0;
        for (int k = 0; k < TAPS; ++k)
        {
            const int j = k - (HALF - 1);              // -7 .. 8
            const double x = f - (double) j;           // 0 at the tap we are landing on
            const double s = (std::abs (x) < 1e-12) ? 1.0 : std::sin (PI * x) / (PI * x);
            const double u = x / (double) HALF;        // |u| <= 1 over the span
            const double wnd = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - u * u))) / i0b;
            h[p][k] = (float) (s * wnd);
            sum += s * wnd;
        }
        // unity at DC: a source that moves must not change its own level
        for (int k = 0; k < TAPS; ++k) h[p][k] = (float) (h[p][k] / sum);
    }
}

const FracDelay& FracDelay::table()
{
    static const FracDelay t;
    return t;
}

//==============================================================================
void DelayLine::prepare (int maxSamples)
{
    int n = 64; while (n < maxSamples + 4) n <<= 1;
    buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
}
void DelayLine::clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

//==============================================================================
float Engine::airDbPerMetre (int band) { return AIR_DB_PER_KM[band] * 0.001f; }

/*  Maekawa's barrier formula (1968; ISO 9613-2 uses the same curve): attenuation
    10 log10 (3 + 20 N) for a Fresnel number N = 2 delta / lambda, delta being how
    much longer the bent path is than the straight one. Below the shadow boundary
    (N < 0, line of sight) the curve runs smoothly to zero by N = -0.3, as on
    Maekawa's own chart, so an edge never switches on with a click. */
float Engine::diffractionDb (float N)
{
    if (N >= 0) return std::min (24.0f, 10.0f * std::log10 (3.0f + 20.0f * N));
    const float t = 1.0f + N / 0.3f;
    return t <= 0 ? 0.0f : 4.77f * t * t;
}

/*  Directivity, dB relative to on axis.

    PURE: a point source with a cardioid tendency, (1 - d) + d (1 + cos theta) / 2,
    the same at every frequency - a singer, an instrument's rough radiation.

    LOUDSPEAKER: a two-way box. In front, the piston formula |2 J1(ka sin t) / (ka sin t)|
    for the woofer below the crossover and the tweeter above (the cone beams as it
    grows large against the wavelength). Round the box the finite baffle turns
    the radiation cardioid-like with rising frequency - omni-ish at 125 Hz,
    twenty-odd dB down straight behind at 4 kHz and up - which is what the edge
    diffraction of a box the size of a studio monitor measures. Behind the box the
    piston factor is held at its 90 degree value; the total is floored at -30 dB. */
float Engine::directivityDb (const SourceParams& s, float cosTheta, int band)
{
    cosTheta = std::max (-1.0f, std::min (1.0f, cosTheta));
    if (s.type == SRC_PURE)
    {
        const float d = std::max (0.0f, std::min (1.0f, s.directivity));
        const float p = (1.0f - d) + d * 0.5f * (1.0f + cosTheta);
        return linToDb (std::max (p, 0.0316f));
    }
    const float f = BAND_HZ[band];
    const float a = f < CROSSOVER_HZ ? WOOFER_RADIUS : TWEETER_RADIUS;
    const float ka = 2.0f * PI * f * a / SPEED_OF_SOUND;
    const float sinT = std::sqrt (std::max (0.0f, 1.0f - cosTheta * cosTheta));
    const float pist = cosTheta >= 0 ? piston (ka * sinT) : piston (ka);
    const float B = BOX_REAR_DB * (1.0f - std::exp (-f / BOX_CORNER_HZ));
    const float boxDb = -B * 0.5f * (1.0f - cosTheta);
    return std::max (-30.0f, linToDb (std::max (pist, 1e-3f)) + boxDb);
}

// -10 log10 Q, Q = 1 / (1/2 integral of the power pattern over cos theta)
float Engine::radiatedPowerDb (const SourceParams& s, int band)
{
    double acc = 0;
    const int N = 48;
    for (int i = 0; i < N; ++i)
    {
        const float u = -1.0f + (i + 0.5f) * 2.0f / N;
        acc += std::pow (10.0, directivityDb (s, u, band) / 10.0);
    }
    acc /= N;                            // mean over the sphere of the power pattern
    return (float) (10.0 * std::log10 (std::max (acc, 1e-6)));
}

/*  Eyring's reverberation time per band: T = 0.161 V / (-S ln (1 - a) + 4 m V),
    every surface carrying the room's material, each door counted as a surface of
    absorption tau (fully open = 1, closed = its transmission), and the party walls
    likewise, with ISO 9613-1 air absorption m. */
static void roomAbsorption (int room, const RoomSurfaces& surf, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7,
                            float planArea = 0, float perimeter = 0,
                            const FurnItem* furn = nullptr, int nfurn = 0, float panelArea = 0)
{
    const Room& R = ROOMS[room];
    // a broken wall really does change the room's size: take the polygon's own
    // area and perimeter when one has been built
    const float aFloor = planArea > 0 ? planArea : (R.x1 - R.x0) * (R.y1 - R.y0);
    const float aCeil  = aFloor;
    const float aWall  = perimeter > 0 ? perimeter * R.h : std::max (0.0f, R.surface() - 2.0f * aFloor);
    const float S = aFloor + aCeil + aWall;
    const float V = aFloor * R.h;
    for (int b = 0; b < NBAND; ++b)
    {
        float A = aWall  * MATERIAL_ALPHA[surf.wall][b]
                + aFloor * MATERIAL_ALPHA[surf.floorMat()][b]
                + aCeil  * MATERIAL_ALPHA[surf.ceilMat()][b];
        for (int d = 0; d < NUM_DOORS; ++d)
        {
            if (doorWall (room, d) < 0) continue;
            const float a = doorAperture[d];
            const float tau = a + (1.0f - a) * std::pow (10.0f, -LEAF_TL_DB[b] * 0.1f);
            A += DOORS[d].area() * (tau - MATERIAL_ALPHA[surf.wall][b]);   // a door is in a wall
        }
        for (int q = 0; q < NUM_ROOMS; ++q)
        {
            if (q == room) continue;
            int axis; float pos, s0, s1, h;
            if (! sharedWall (room, q, axis, pos, s0, s1, h)) continue;
            float area = (s1 - s0) * h;
            for (int d = 0; d < NUM_DOORS; ++d)
                if ((DOORS[d].roomA == room && DOORS[d].roomB == q) || (DOORS[d].roomB == room && DOORS[d].roomA == q))
                    area -= DOORS[d].area();
            A += std::max (0.0f, area) * std::pow (10.0f, -WALL_TL_DB[b] * 0.1f);
        }
        // the furniture standing in this room (a rug replaces the floor it covers)
        for (int i = 0; i < nfurn; ++i)
        {
            const FurnItem& it = furn[i];
            if (it.type < 0 || it.type >= NUM_FURN_TYPES || roomOf (it.x, it.y) != room) continue;
            const FurnSpec& F = FURN[it.type];
            float a = F.absorb[b];
            if (F.coversFloor) a -= F.w * F.d * MATERIAL_ALPHA[surf.floorMat()][b];
            A += std::max (0.0f, a);
        }
        // acoustic art panels: their absorber in place of the wall behind them
        if (panelArea > 0) A += panelArea * std::max (0.0f, PANEL_ALPHA[b] - MATERIAL_ALPHA[surf.wall][b]);
        const float abar = std::min (0.98f, A / S);
        const float m = AIR_DB_PER_KM[b] / (1000.0f * 4.343f);
        alphaBar7[b] = abar;
        sabineArea7[b] = A;
        rt7[b] = 0.161f * V / (-S * std::log (1.0f - abar) + 4.0f * m * V);
    }
}

void Engine::eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, surf, doorAperture, a, A, out7);
}

void Engine::absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7)
{
    float a[NBAND], rt[NBAND];
    roomAbsorption (room, surf, doorAperture, a, out7, rt);
}

void Engine::eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, const FurnItem* furn, int nfurn, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, surf, doorAperture, a, A, out7, 0, 0, furn, nfurn);
}

void Engine::absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, const FurnItem* furn, int nfurn, float* out7)
{
    float a[NBAND], rt[NBAND];
    roomAbsorption (room, surf, doorAperture, a, out7, rt, 0, 0, furn, nfurn);
}

// the old shape, every surface the same: what the bench asks for
void Engine::eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7)
{
    RoomSurfaces s; s.wall = materials[room];
    eyringRt60 (room, s, doorAperture, out7);
}

void Engine::absorptionArea (int room, const int* materials, const float* doorAperture, float* out7)
{
    RoomSurfaces s; s.wall = materials[room];
    absorptionArea (room, s, doorAperture, out7);
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate; maxBlock = std::max (16, maxBlockSize);
    hrtf.prepare (fs);
    ntap = hrtf.numTaps();

    const int maxItd = (int) (0.0045 * fs) + 2;      // up to a 1 m pair
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        source[s].prepare ((int) (0.6 * fs));
        srcPre[s].prepare ((int) (0.1 * fs));
        srcInFilt[s].setCoeffs ((float) fs);
        monoIn[s].assign ((size_t) SUB_BLOCK, 0.0f);
        srcLevel[s] = 1.0f;
    }

    for (auto& s : slots)
    {
        s = PathSlot();
        int n = 64; while (n < ntap + maxItd + SUB_BLOCK + 4 * FracDelay::TAPS) n <<= 1;
        s.hist.assign ((size_t) n, 0.0f); s.hmask = n - 1; s.hw = 0;
        s.zL.assign ((size_t) (SUB_BLOCK + maxItd + ntap + 4 * FracDelay::TAPS + 32), 0.0f);
        s.zR.assign (s.zL.size(), 0.0f);
        s.filt.setCoeffs ((float) fs); s.filtTarget.setCoeffs ((float) fs);
    }

    wetL.assign ((size_t) SUB_BLOCK, 0.0f); wetR.assign ((size_t) SUB_BLOCK, 0.0f);
    tmpA.assign ((size_t) (SUB_BLOCK + maxItd + MAX_TAPS + 64), 0.0f);
    tmpB.assign (tmpA.size(), 0.0f);

    // ---- the late fields ---------------------------------------------------
    uint32_t seed = 0x9e3779b9u;
    auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (seed >> 8) * (1.0f / 16777216.0f); };

    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        RoomField& F = rooms[(size_t) r];
        const Room& R = ROOMS[r];
        const float mfp = 4.0f * R.volume() / R.surface();          // mean free path, m
        const float tau = mfp / SPEED_OF_SOUND;
        for (int i = 0; i < RoomField::N; ++i)
        {
            const float spread = 0.55f + 0.95f * (float) i / (RoomField::N - 1);
            int len = (int) std::lround (tau * fs * spread * (1.0f + 0.04f * (rnd() - 0.5f)));
            len = nextPrime (std::max (len, 32));
            F.len[(size_t) i] = len;
            F.line[(size_t) i].prepare (len + 4 * FracDelay::TAPS);
            F.loss[(size_t) i].setCoeffs ((float) fs);
            F.modRate[i] = 0.17f + 0.31f * rnd();
            F.modPhase[i] = 2.0f * PI * rnd();

            // the three allpasses inside this line: allocated for the cap here,
            // their working lengths chosen per material by sizeDiffusers
            for (int q = 0; q < RoomField::NAP; ++q)
            {
                const size_t idx = (size_t) (i * RoomField::NAP + q);
                F.ap[idx].assign ((size_t) RoomField::AP_CAP, 0.0f);
                F.apLen[idx] = 0;
                F.apW[idx] = 0;
            }
            F.apTotal[(size_t) i] = 0;
        }
        // the way in: four allpasses, spread over the mean free path
        {
            const float inFrac[RoomField::NIN] = { 0.21f, 0.37f, 0.58f, 0.83f };
            for (int q = 0; q < RoomField::NIN; ++q)
            {
                const int al = nextPrime (std::max (11, (int) (inFrac[q] * tau * fs)));
                F.inApLen[(size_t) q] = al;
                F.inApW[(size_t) q] = 0;
                F.inAp[(size_t) q].assign ((size_t) al, 0.0f);
            }
        }
        F.tEarly = 2.0f * tau;
        F.preDelay = std::max (1, std::min ((int) (0.09 * fs), (int) (F.tEarly * fs)));
        F.feed.prepare ((int) (0.15 * fs));
        int n = 64; while (n < MAX_TAPS + SUB_BLOCK + 16) n <<= 1;
        F.dmask = n - 1;
        for (int j = 0; j < 8; ++j) { F.dhist[(size_t) j].assign ((size_t) n, 0.0f); F.dw[(size_t) j] = 0; }
        F.weight = F.weightTarget = 0;
    }

    // the eight fixed diffuse directions, ITD baked into the taps
    {
        std::vector<float> l ((size_t) ntap), rr ((size_t) ntap);
        for (int j = 0; j < 8; ++j)
        {
            const float az = 22.5f + 45.0f * j;
            const float el = (j & 1) ? 20.0f : -10.0f;
            float it = 0;
            hrtf.lookup (az, el, l.data(), rr.data(), it);
            const int dl = it > 0 ? 0 : (int) std::lround (-it);
            const int dr = it > 0 ? (int) std::lround (it) : 0;
            for (int r = 0; r < NUM_ROOMS; ++r)
            {
                storeReversed (l.data(), ntap, dl, rooms[(size_t) r].dL[(size_t) j].data(), MAX_TAPS);
                storeReversed (rr.data(), ntap, dr, rooms[(size_t) r].dR[(size_t) j].data(), MAX_TAPS);
            }
        }
    }

    geomStore.assign (sizeof (RoomGeom) * NUM_ROOMS, 0);
    {
        RoomGeom* g = reinterpret_cast<RoomGeom*> (geomStore.data());
        RoomBreaks flat;
        for (int r = 0; r < NUM_ROOMS; ++r) g[r] = buildRoomGeom (r, flat);
    }

    for (int r = 0; r < NUM_ROOMS; ++r) measureEfficiency (r);

    reset();
    paramsFresh = true;
    for (int s = 0; s < MAX_SOURCES; ++s) { lastSrcRoomAc[s] = -1; lastTypeAc[s] = -1; lastDirAc[s] = -1; lastActiveAc[s] = false; lastLevelAc[s] = -1; }
    updateRoomAcoustics (true);
}

void Engine::reset()
{
    for (int s = 0; s < MAX_SOURCES; ++s) { source[s].clear(); srcPre[s].clear(); srcInFilt[s].reset(); }
    for (auto& s : slots)
    {
        s.active = false; s.gain = 0; s.gainTarget = 0; s.fresh = true; s.crossing = false; s.env = s.envTarget = 0;
        std::fill (s.hist.begin(), s.hist.end(), 0.0f); s.hw = 0; s.filt.reset();
    }
    for (auto& F : rooms)
    {
        for (int i = 0; i < RoomField::N; ++i) { F.line[(size_t) i].clear(); F.loss[(size_t) i].reset(); F.out[(size_t) i] = 0; }
        F.feed.clear(); F.y = 0;
        for (size_t q = 0; q < F.ap.size(); ++q) { std::fill (F.ap[q].begin(), F.ap[q].end(), 0.0f); F.apW[q] = 0; }
        for (size_t q = 0; q < F.inAp.size(); ++q) { std::fill (F.inAp[q].begin(), F.inAp[q].end(), 0.0f); F.inApW[q] = 0; }
        for (int j = 0; j < 8; ++j) { std::fill (F.dhist[(size_t) j].begin(), F.dhist[(size_t) j].end(), 0.0f); F.dw[(size_t) j] = 0; }
    }
    for (auto& c : couplings) c.filt.reset();
    inSq = outSq = directSq = revSq = 0;
    activePaths = 0;
    for (int r = 0; r < NUM_ROOMS; ++r) { lightFast[r] = lightSlow[r] = lightOut[r] = 0; fieldAcc[r] = 0; }
    snapFades = true;
}

void Engine::setParams (const Params& p)
{
    target = p;
    if (paramsFresh)
    {
        cur = p;
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            srcPos[s] = clampIntoRooms ({ p.src[s].x, p.src[s].y, p.src[s].z }, 0.15f);
            srcYaw[s] = p.src[s].yaw;
            srcLevel[s] = p.src[s].active() ? dbToLin (p.src[s].levelDb) : 0.0f;
        }
        lisPos = clampIntoRooms ({ p.lisX, p.lisY, EAR_HEIGHT }, 0.15f);
        lisYaw = p.lisYaw;
        for (int d = 0; d < NUM_DOORS; ++d) doorNow[d] = p.door[d];
        for (int r = 0; r < NUM_ROOMS; ++r)
            for (int k = 0; k < 2; ++k) { cur.breakAlong[r][k] = p.breakAlong[r][k]; cur.breakPush[r][k] = p.breakPush[r][k]; }
        mixNow = p.mix; trimOut = dbToLin (p.outputDb);
        const int rl = std::max (0, roomOf (lisPos.x, lisPos.y));
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            rooms[(size_t) r].weight = rooms[(size_t) r].weightTarget = (r == rl) ? 1.0f : 0.0f;
            for (int s = 0; s < MAX_SOURCES; ++s)
                srcW[s][r] = (p.src[s].active() && roomOf (srcPos[s].x, srcPos[s].y) == r) ? 1.0f : 0.0f;
        }
        paramsFresh = false;
    }
}

//==============================================================================
// room acoustics: per-band RT60 -> line losses, calibration, couplings
void Engine::updateRoomAcoustics (bool force)
{
    bool changed = force, sourcesChanged = force;
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        RoomSurfaces s; s.wall = cur.material[r]; s.floor = cur.floorMat[r]; s.ceil = cur.ceilMat[r];
        if (s.wall != surfNow[r].wall || s.floor != surfNow[r].floor || s.ceil != surfNow[r].ceil)
        { surfNow[r] = s; matNow[r] = s.wall; changed = true; }
    }
    for (int d = 0; d < NUM_DOORS; ++d) if (std::abs (lastDoorAc[d] - doorNow[d]) > 0.002f) { lastDoorAc[d] = doorNow[d]; changed = true; }
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        const SourceParams& sp = cur.src[s];
        const int rs = sp.active() ? std::max (0, roomOf (srcPos[s].x, srcPos[s].y)) : -1;
        if (lastSrcRoomAc[s] != rs) { lastSrcRoomAc[s] = rs; sourcesChanged = true; }
        if (lastTypeAc[s] != sp.type) { lastTypeAc[s] = sp.type; sourcesChanged = true; }
        if (std::abs (lastDirAc[s] - sp.directivity) > 0.005f) { lastDirAc[s] = sp.directivity; sourcesChanged = true; }
        if (lastActiveAc[s] != sp.active()) { lastActiveAc[s] = sp.active(); sourcesChanged = true; }
        if (std::abs (lastLevelAc[s] - sp.levelDb) > 0.05f) { lastLevelAc[s] = sp.levelDb; sourcesChanged = true; }
    }
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        const float want[4] = { cur.breakAlong[r][0], cur.breakPush[r][0], cur.breakAlong[r][1], cur.breakPush[r][1] };
        for (int k = 0; k < 4; ++k)
            if (std::abs (breakNow[r][k] - want[k]) > 1.0e-4f) { breakNow[r][k] = want[k]; changed = true; }
    }
    /*  Furniture. Any move re-poses the pieces (occlusion and reflections read the
        poses every block); only a change of WHICH pieces stand in WHICH room
        touches Eyring, so dragging a sofa round its room does not re-run the
        room acoustics on every mouse event. */
    {
        const int nf = std::min (std::max (cur.nfurn, 0), MAX_FURN);
        bool moved = nf != lastNfurnAc;
        for (int i = 0; i < nf && ! moved; ++i)
            moved = cur.furn[i].type != lastFurnAc[i].type || cur.furn[i].x != lastFurnAc[i].x
                 || cur.furn[i].y != lastFurnAc[i].y || cur.furn[i].yaw != lastFurnAc[i].yaw;
        if (moved)
        {
            lastNfurnAc = nf;
            for (int i = 0; i < nf; ++i) lastFurnAc[i] = cur.furn[i];
            updateFurniture();
            bool acoustic = nf != furnKeyN;
            for (int i = 0; i < nf; ++i)
            {
                const int key = (furnPose[i].type + 1) * 8 + (furnPose[i].room + 1);
                if (key != furnKeyAc[i]) { furnKeyAc[i] = key; acoustic = true; }
            }
            furnKeyN = nf;
            if (acoustic) changed = true;
        }
    }
    for (int r = 0; r < NUM_ROOMS; ++r)
        if (cur.panelArea[r] != panelAreaAc[r]) { panelAreaAc[r] = cur.panelArea[r]; changed = true; }
    if (! changed && ! sourcesChanged) return;

    // rebuild the plan of every room whose walls have moved
    {
        RoomGeom* g = reinterpret_cast<RoomGeom*> (geomStore.data());
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            RoomBreaks br;
            br.w[0].along = breakNow[r][0]; br.w[0].push = breakNow[r][1];
            br.w[1].along = breakNow[r][2]; br.w[1].push = breakNow[r][3];
            g[r] = buildRoomGeom (r, br);
        }
    }

    if (changed)
    {
        for (int d = 0; d < NUM_DOORS; ++d)
            for (int b = 0; b < NBAND; ++b)
                doorTau[d][b] = doorNow[d] + (1.0f - doorNow[d]) * std::pow (10.0f, -LEAF_TL_DB[b] * 0.1f);
        for (int p = 0; p < 3; ++p)
            for (int b = 0; b < NBAND; ++b)
                wallTau[p][b] = std::pow (10.0f, -WALL_TL_DB[b] * 0.1f);

        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            RoomField& F = rooms[(size_t) r];
            const RoomGeom* gg = reinterpret_cast<const RoomGeom*> (geomStore.data());
            float abar[NBAND], A[NBAND];
            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter, cur.furn, nfurnNow, cur.panelArea[r]);
            // what the furniture adds, the engine's own way (the panel shows this, not a sum of its own)
            furnAbs1k[r] = 0.0f;
            if (nfurnNow > 0 || cur.panelArea[r] > 0)
            {
                float a0[NBAND], A0[NBAND], rt0[NBAND];
                roomAbsorption (r, surfNow[r], doorNow, a0, A0, rt0, gg[r].area, gg[r].perimeter);
                furnAbs1k[r] = A[3] - A0[3];
            }
            // the furniture's scattering: 1 - exp (-2 sum / S), capped; exactly 0 dB with none
            {
                float sum = 0;
                for (int i = 0; i < nfurnNow; ++i)
                    if (furnPose[i].type >= 0 && furnPose[i].room == r) sum += FURN[furnPose[i].type].scatter;
                const float Sr = gg[r].area > 0 ? 2.0f * gg[r].area + gg[r].perimeter * ROOMS[r].h : ROOMS[r].surface();
                furnScatter[r]   = sum > 0 ? std::min (0.6f, 1.0f - std::exp (-2.0f * sum / Sr)) : 0.0f;
                furnScatterDb[r] = sum > 0 ? 10.0f * std::log10 (1.0f - furnScatter[r]) : 0.0f;
            }
            for (int b = 0; b < NBAND; ++b) F.absorptionArea[b] = A[b];
            // the diffuser chain has to be sized before the losses are charged over it
            F.sizeDiffusers (fs, F.rt60[3]);

            // line losses: -60 dB per RT60 of travel
            float lossDb[NBAND];
            for (int i = 0; i < RoomField::N; ++i)
            {
                // the loop is the line plus what its allpasses really hold, and
                // the design is trimmed by what this network was measured to do
                const float loopLen = (float) F.len[(size_t) i] + F.apTotal[(size_t) i];
                for (int b = 0; b < NBAND; ++b)
                {
                    const float want = F.rt60[dbgFlatLoss ? 3 : b];
                    lossDb[b] = -60.0f * loopLen * F.calAt (F.calTrim, want) / ((float) fs * want);
                }
                F.loss[(size_t) i].setBandsDb (lossDb);
            }

            /*  Calibration, at 1 kHz. A stationary source of signal power p (our
                "acoustic power" is 4 pi p, since the direct pressure at r is p / r^2)
                builds a diffuse pressure of 16 pi p / A. The network's steady output
                for unit input power is G = fs RT60 / (13.8 L_total) times the
                efficiency measured at prepare. */
            float Ltot = 0;
            for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i] + F.apTotal[(size_t) i];
            const float G = (float) fs * F.rt60[3] / (13.8f * Ltot);
            F.efficiency = F.calAt (F.calEff, F.rt60[3]);
            F.inGain = std::sqrt (16.0f * PI / (A[3] * G * F.efficiency));
        }
    }

    // per source: the radiated-power filter (a directional source drives the
    // room less than its on-axis level suggests), and the room ranking
    for (int r = 0; r < NUM_ROOMS; ++r) rooms[(size_t) r].sourcePower = 0;
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        const SourceParams& sp = cur.src[s];
        float inDb[NBAND];
        for (int b = 0; b < NBAND; ++b) inDb[b] = radiatedPowerDb (sp, b);
        srcInFilt[s].setBandsDb (inDb);
        if (sp.active() && lastSrcRoomAc[s] >= 0)
            rooms[(size_t) lastSrcRoomAc[s]].sourcePower += dbToLin (sp.levelDb) * dbToLin (sp.levelDb);
    }
    rebuildCouplings();
}

/*  The steady-state formula G = fs RT / (13.8 L) assumes the rendered signal
    sees the stored energy uniformly. It does not quite: the sign-flipped
    Hadamard leaves some correlation between the lines a pair readout sums, and
    a first build measured 0.6 of the formula. So the efficiency is MEASURED
    here, once per room at prepare, with flat losses at a 1 s decay: an impulse
    in, the eight pair signals' energy out, against the formula's prediction. */
/*  Remembered per room and sample rate: the delay lengths are deterministic and
    the sizing rule is fixed, so the answer cannot differ between two Engines at
    the same rate. */
namespace
{
    struct FieldCal { float eff[RoomField::NCAL]; float trim[RoomField::NCAL]; };
    std::map<std::pair<int, int>, FieldCal> g_calCache;
    std::mutex g_calMutex;
}

void Engine::measureEfficiency (int r)
{
    RoomField& FF = rooms[(size_t) r];
    const std::pair<int, int> key { r, (int) std::lround (fs) };
    {
        std::lock_guard<std::mutex> lock (g_calMutex);
        const auto it = g_calCache.find (key);
        if (it != g_calCache.end())
        {
            for (int c = 0; c < RoomField::NCAL; ++c) { FF.calEff[c] = it->second.eff[c]; FF.calTrim[c] = it->second.trim[c]; }
            FF.efficiency = FF.calEff[1];
            FF.sizeDiffusers (fs, 1.0f);
            for (int i = 0; i < RoomField::N; ++i) { FF.line[(size_t) i].clear(); FF.loss[(size_t) i].reset(); }
            for (size_t q = 0; q < FF.ap.size(); ++q) { std::fill (FF.ap[q].begin(), FF.ap[q].end(), 0.0f); FF.apW[q] = 0; }
            return;
        }
    }
    // four design decays, each with the chain it would really have
    for (int c = 0; c < RoomField::NCAL; ++c)
    {
        const float RTc = FF.calRt[c];
        FF.sizeDiffusers (fs, RTc);
        for (int i = 0; i < RoomField::N; ++i)
        {
            FF.line[(size_t) i].clear(); FF.loss[(size_t) i].reset();
            const float loopLen = (float) FF.len[(size_t) i] + FF.apTotal[(size_t) i];
            float db[NBAND]; for (int b = 0; b < NBAND; ++b) db[b] = -60.0f * loopLen / ((float) fs * RTc);
            FF.loss[(size_t) i].setBandsDb (db);
        }
        for (size_t q = 0; q < FF.ap.size(); ++q) { std::fill (FF.ap[q].begin(), FF.ap[q].end(), 0.0f); FF.apW[q] = 0; }

        float Lt = 0; for (int i = 0; i < RoomField::N; ++i) Lt += (float) FF.len[(size_t) i] + FF.apTotal[(size_t) i];
        // -25 dB for the decay, and 0.75 of a decay already holds 99.997 % of
        // an exponential's energy, so there is nothing to gain past it
        const int total = (int) ((0.8 * RTc + 0.20) * fs);
        std::vector<float> trace ((size_t) total, 0.0f);
        double rendered = 0;
        for (int n = 0; n < total; ++n)
        {
            float out[16];
            for (int k = 0; k < 16; ++k)
            {
                float v = FF.loss[(size_t) k].process (FF.line[(size_t) k].readInt (FF.len[(size_t) k] - 1));
                for (int q = 0; q < RoomField::NAP; ++q)
                {
                    const size_t idx = (size_t) (k * RoomField::NAP + q);
                    if (FF.apLen[idx] <= 0) continue;
                    const float b = FF.ap[idx][(size_t) FF.apW[idx]];
                    const float y = -FF.apG * v + b;
                    FF.ap[idx][(size_t) FF.apW[idx]] = v + FF.apG * y;
                    if (++FF.apW[idx] >= FF.apLen[idx]) FF.apW[idx] = 0;
                    v = y;
                }
                out[k] = v;
            }
            float v16[16]; for (int k = 0; k < 16; ++k) v16[k] = out[k];
            for (int len = 1; len < 16; len <<= 1)
                for (int a = 0; a < 16; a += len << 1)
                    for (int b = a; b < a + len; ++b) { const float p = v16[b], q2 = v16[b + len]; v16[b] = p + q2; v16[b + len] = p - q2; }
            const float inj = n == 0 ? 1.0f : 0.0f;
            float w[16];
            for (int k = 0; k < 16; ++k) { w[k] = 0.25f * SGN_MIX[k] * v16[k] + 0.25f * SGN_SRC[r][k] * inj; FF.line[(size_t) k].write (w[k]); }
            float s = 0;
            for (int j = 0; j < 8; ++j) { const float pr = (w[2 * j] + w[2 * j + 1]) * 0.25f; rendered += (double) pr * pr; s += pr; }
            trace[(size_t) n] = s;
        }
        FF.calEff[c] = (float) std::max (0.05, std::min (4.0, rendered / ((double) fs * RTc / (13.8 * Lt))));

        // the decay the network really has, by backward integration
        std::vector<double> edc ((size_t) total, 0.0);
        double acc = 0;
        for (int i = total - 1; i >= 0; --i) { acc += (double) trace[(size_t) i] * trace[(size_t) i]; edc[(size_t) i] = acc; }
        const int from = (int) (0.02 * fs);
        float trim = 1.0f;
        if (from < total && edc[(size_t) from] > 0)
        {
            const double top = 10.0 * std::log10 (edc[(size_t) from]);
            int i5 = -1, i25 = -1;
            for (int i = from; i < total; ++i)
            {
                const double d = 10.0 * std::log10 (std::max (edc[(size_t) i], 1e-300)) - top;
                if (i5 < 0 && d <= -5.0) i5 = i;
                if (d <= -25.0) { i25 = i; break; }
            }
            if (i5 > 0 && i25 > i5)
            {
                const double measured = 3.0 * (i25 - i5) / fs;
                trim = (float) std::max (0.5, std::min (2.0, measured / RTc));
            }
        }
        FF.calTrim[c] = trim;
        for (int i = 0; i < RoomField::N; ++i) { FF.line[(size_t) i].clear(); FF.loss[(size_t) i].reset(); }
        for (size_t q = 0; q < FF.ap.size(); ++q) { std::fill (FF.ap[q].begin(), FF.ap[q].end(), 0.0f); FF.apW[q] = 0; }
    }
    {
        std::lock_guard<std::mutex> lock (g_calMutex);
        FieldCal fc;
        for (int c = 0; c < RoomField::NCAL; ++c) { fc.eff[c] = FF.calEff[c]; fc.trim[c] = FF.calTrim[c]; }
        g_calCache[key] = fc;
    }
    FF.efficiency = FF.calEff[1];
    return;
}

void Engine::measureEfficiencyOld (int r)
{
    RoomField& F = rooms[(size_t) r];
    const float RT = 1.0f;
    F.sizeDiffusers (fs, RT);
    for (int i = 0; i < RoomField::N; ++i)
    {
        F.line[(size_t) i].clear(); F.loss[(size_t) i].reset();
        const float loopLen = (float) F.len[(size_t) i] + F.apTotal[(size_t) i];
        float db[NBAND]; for (int b = 0; b < NBAND; ++b) db[b] = -60.0f * loopLen / ((float) fs * RT);
        F.loss[(size_t) i].setBandsDb (db);
    }
    for (size_t q = 0; q < F.ap.size(); ++q) { std::fill (F.ap[q].begin(), F.ap[q].end(), 0.0f); F.apW[q] = 0; }
    double rendered = 0;
    const int total = (int) (2.2 * RT * fs);
    float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i] + F.apTotal[(size_t) i];
    for (int n = 0; n < total; ++n)
    {
        float out[16];
        for (int k = 0; k < 16; ++k)
        {
            float v = F.loss[(size_t) k].process (F.line[(size_t) k].readInt (F.len[(size_t) k] - 1));
            for (int q = 0; q < RoomField::NAP; ++q)
            {
                const size_t idx = (size_t) (k * RoomField::NAP + q);
                if (F.apLen[idx] <= 0) continue;
                const float b = F.ap[idx][(size_t) F.apW[idx]];
                const float y = -F.apG * v + b;
                F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * y;
                if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                v = y;
            }
            out[k] = v;
        }
        float v[16]; for (int k = 0; k < 16; ++k) v[k] = out[k];
        for (int len = 1; len < 16; len <<= 1)
            for (int a = 0; a < 16; a += len << 1)
                for (int b = a; b < a + len; ++b) { const float p = v[b], q = v[b + len]; v[b] = p + q; v[b + len] = p - q; }
        const float inj = n == 0 ? 1.0f : 0.0f;
        float w[16];
        for (int k = 0; k < 16; ++k) { w[k] = 0.25f * SGN_MIX[k] * v[k] + 0.25f * SGN_SRC[r][k] * inj; F.line[(size_t) k].write (w[k]); }
        // what the diffuse render would sum: eight pairs at 1/4, i.e. sum of pair^2 / 16
        for (int j = 0; j < 8; ++j) { const float s = (w[2 * j] + w[2 * j + 1]) * 0.25f; rendered += (double) s * s; }
    }
    const double formula = (double) fs * RT / (13.8 * Ltot);
    F.efficiency = (float) std::max (0.05, std::min (4.0, rendered / formula));
    for (int i = 0; i < RoomField::N; ++i) { F.line[(size_t) i].clear(); F.loss[(size_t) i].reset(); }
}

/*  Two coupled feedback networks with long decays have modal peaks tens of dB
    above their average gain, and at a coincident peak the loop between them
    exceeds unity however small the energy-balance coefficient is - that is the
    measured failure of the first build (a PLASTER hall ran away at +40 dB/s).
    So the energy flows one way only, down a ranking of the rooms by the source
    power in them: from the loudest room into the others, from the second into
    the third, never back. The flow back into a louder room is a second-order
    term in real coupled rooms too. */
/*  Size the diffuser chain to the decay it has to serve.

    The target is a total loop delay of 1.3 x RT60/2.2 - the modal criterion with
    a little margin - capped at 0.65 s, because past that the mean loop gets long
    enough that echo density starts to suffer and the modulation is the better
    tool for what remains. Below the criterion the lines alone are enough and the
    chain is switched off entirely, which is what an absorbing room needs: its
    own decay is shorter than a diffuser's ringing.

    Every length is taken to a prime, and never to its own line's length, so
    nothing in the loop resonates with anything else in it. */
void RoomField::sizeDiffusers (double fs, float rt60At1k)
{
    auto isPrime = [] (int v) { if (v < 2) return false; for (int i = 2; i * i <= v; ++i) if (v % i == 0) return false; return true; };
    auto nextP = [&] (int n) { while (! isPrime (n)) ++n; return n; };

    float lineTotal = 0;
    for (int i = 0; i < N; ++i) lineTotal += (float) len[(size_t) i];

    const float wantTotal = std::min (0.65f, 1.3f * rt60At1k / 2.2f) * (float) fs;
    const float needRaw = std::max (0.0f, (wantTotal - lineTotal) / apEff());
    /*  No single allpass may ring for longer than a third of the room's own
        decay. Its ringing is -20 log10(g) dB per La, so its own RT60 is
        60 La / 4.15 = 14.5 La at g = 0.62, and this is the length at which that
        reaches RT60/3. Without it a dead room's tail is set by its diffusers
        rather than by the room, which measured 0.22 s where Eyring says 0.09. */
    const int apCeil = std::max (0, (int) (rt60At1k * (float) fs / 43.5f));

    // shares of the three, summing to one, and of each line in proportion to itself
    const float share[NAP] = { 0.425f, 0.325f, 0.25f };
    for (int i = 0; i < N; ++i)
    {
        const float mine = needRaw * (float) len[(size_t) i] / std::max (1.0f, lineTotal);
        float raw = 0;
        for (int q = 0; q < NAP; ++q)
        {
            const size_t idx = (size_t) (i * NAP + q);
            int al = (int) std::lround (mine * share[q]);
            if (al < 13 || apCeil < 13) { apLen[idx] = 0; continue; }   // off rather than tiny
            al = std::min (al, std::min (AP_CAP - 1, apCeil));
            al = nextP (al);
            while (al == len[(size_t) i]) al = nextP (al + 1);
            if (al >= AP_CAP) al = nextP (AP_CAP / 2);
            apLen[idx] = al;
            if (apW[idx] >= al) apW[idx] = 0;
            raw += (float) al;
        }
        apTotal[(size_t) i] = raw * apEff();                    // what the LOOP grew by
    }
}

bool Engine::roomIsBroken (int room) const
{
    return std::abs (breakNow[room][1]) > 1.0e-4f || std::abs (breakNow[room][3]) > 1.0e-4f;
}

void Engine::rebuildCouplings()
{
    int order[NUM_ROOMS] = { 0, 1, 2 };
    std::sort (order, order + NUM_ROOMS, [this] (int a, int b)
    {
        const float pa = rooms[(size_t) a].sourcePower, pb = rooms[(size_t) b].sourcePower;
        if (std::abs (pa - pb) > 1e-9f) return pa > pb;
        return a < b;
    });
    // keep the filter states where the edge already existed
    std::vector<Coupling> old = couplings;
    couplings.clear();
    for (int i = 0; i < NUM_ROOMS; ++i)
        for (int j = i + 1; j < NUM_ROOMS; ++j)
        {
            int axis; float pos, s0, s1, h;
            if (! sharedWall (order[i], order[j], axis, pos, s0, s1, h)) continue;
            Coupling c; c.from = order[i]; c.to = order[j]; c.filt.setCoeffs ((float) fs);
            for (const auto& o : old) if (o.from == c.from && o.to == c.to) { c.filt = o.filt; break; }
            couplings.push_back (c);
        }

    for (auto& c : couplings)
    {
        // energy through the door(s) and the party wall between the two rooms
        float db[NBAND];
        int axis; float pos, s0, s1, h;
        sharedWall (c.from, c.to, axis, pos, s0, s1, h);
        float wallArea = (s1 - s0) * h;
        for (int b = 0; b < NBAND; ++b)
        {
            float St = 0;
            for (int d = 0; d < NUM_DOORS; ++d)
                if ((DOORS[d].roomA == c.from && DOORS[d].roomB == c.to) || (DOORS[d].roomB == c.from && DOORS[d].roomA == c.to))
                { St += DOORS[d].area() * doorTau[d][b]; if (b == 0) wallArea -= DOORS[d].area(); }
            St += std::max (0.0f, wallArea) * wallTau[pairIndex (c.from, c.to)][b];
            db[b] = 10.0f * std::log10 (std::max (1e-9f, St / (16.0f * PI)));
        }
        c.filt.setBandsDb (db);
        c.gain = rooms[(size_t) c.to].inGain;
    }
}

//==============================================================================
// geometry
void Engine::headRelative (const Vec3& p, float& az, float& el) const
{
    const Vec3 d = p - lisPos;
    const float cy = std::cos (rad (lisYaw)), sy = std::sin (rad (lisYaw));
    const float front = d.x * cy + d.y * sy;
    const float right = d.x * sy - d.y * cy;
    az = deg (std::atan2 (right, front));
    el = deg (std::atan2 (d.z, std::sqrt (front * front + right * right)));
}

void Engine::applyDirectivity (PathSpec& s, const Vec3& departDir) const
{
    const float len = departDir.len();
    if (len < 1e-6f) return;
    const Vec3 f (std::cos (rad (srcYaw[curSrc])), std::sin (rad (srcYaw[curSrc])), 0.0f);
    const float cosT = departDir.dot (f) / len;
    for (int b = 0; b < NBAND; ++b) s.bandDb[b] += directivityDb (cur.src[curSrc], cosT, b);
}

// air absorption + spreading, arrival direction; everything else already in bandDb
void Engine::finishSpec (PathSpec& s, const Vec3& arriveFrom, const Vec3& departTo, float length)
{
    s.length = length;
    s.gain = 1.0f / std::max (length, 0.1f);
    for (int b = 0; b < NBAND; ++b) s.bandDb[b] -= airDbPerMetre (b) * length;
    headRelative (arriveFrom, s.az, s.el);
    applyDirectivity (s, departTo - srcPos[curSrc]);
    s.src = curSrc; s.feed = curSrc;
    s.key |= (uint32_t) curSrc << 24;
}

bool Engine::bounceHitsOpening (int room, int wall, const Vec3& p) const
{
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        if (doorWall (room, d) != wall) continue;
        float lo, hi; openStrip (d, doorNow[d], lo, hi);
        const float s = spanCoord (DOORS[d], p);
        if (s >= lo && s <= hi && p.z >= 0 && p.z <= DOORS[d].height) return true;
    }
    return false;
}

/*  The image-source method in a shoebox: the image of S under (nx, ny, nz)
    reflections per axis, the straight line from it to L, and its crossings with
    the unfolded wall planes folded back into the room - which gives the bounce
    points in order, and which wall each is on. */
bool Engine::imagePath (int room, const Vec3& S, const Vec3& L, int nx, int ny, int nz,
                        Vec3* bounces, int& nb, int* wallIds, float& length)
{
    const Room& R = ROOMS[room];
    const float lo[3] = { R.x0, R.y0, 0.0f };
    const float hi[3] = { R.x1, R.y1, R.h };
    const int n[3] = { nx, ny, nz };
    const float sc[3] = { S.x, S.y, S.z }, lc[3] = { L.x, L.y, L.z };
    float ic[3];
    for (int a = 0; a < 3; ++a)
    {
        const float len = hi[a] - lo[a];
        const float u = sc[a] - lo[a];
        ic[a] = lo[a] + n[a] * len + ((n[a] & 1) ? (len - u) : u);
    }
    const Vec3 I (ic[0], ic[1], ic[2]);
    length = (I - L).len();

    // crossings, parametrised from L (t = 0) to I (t = 1)
    struct Cross { float t; int wall; };
    Cross cr[8]; int nc = 0;
    for (int a = 0; a < 3; ++a)
    {
        const float len = hi[a] - lo[a];
        const int cnt = std::abs (n[a]);
        for (int k = 0; k < cnt; ++k)
        {
            const float plane = n[a] > 0 ? hi[a] + k * len : lo[a] - k * len;
            const float denom = ic[a] - lc[a];
            if (std::abs (denom) < 1e-9f) return false;
            const float t = (plane - lc[a]) / denom;
            // the unfolded planes alternate walls: hi, lo, hi ... going up, lo, hi ... going down
            cr[nc].t = t; cr[nc].wall = a * 2 + (((n[a] > 0) == (k % 2 == 0)) ? 1 : 0); ++nc;
        }
    }
    std::sort (cr, cr + nc, [] (const Cross& p, const Cross& q) { return p.t < q.t; });
    nb = 0;
    for (int i = 0; i < nc; ++i)
    {
        Vec3 p = L + (I - L) * cr[i].t;
        float pc[3] = { p.x, p.y, p.z };
        for (int a = 0; a < 3; ++a)
        {
            const float len = hi[a] - lo[a];
            float u = std::fmod (pc[a] - lo[a], 2.0f * len); if (u < 0) u += 2.0f * len;
            if (u > len) u = 2.0f * len - u;
            pc[a] = lo[a] + u;
        }
        bounces[nb] = Vec3 (pc[0], pc[1], pc[2]);
        wallIds[nb] = cr[i].wall;
        ++nb;
        if (bounceHitsOpening (room, cr[i].wall, bounces[nb - 1])) return false;   // it left through a door
    }
    return true;
}

/*  Splayed walls. A reflection off a wall tilted by a degree or two arrives a
    little early or late and from a slightly different bearing, and - the point -
    the higher orders stop landing at exact multiples of the room dimension, so
    the comb never builds. Deterministic in the image indices: the same room is
    the same room on every render, and the bench can memcmp it. */
static void splayOf (int nx, int ny, int nz, int order, float amount, float& dLen, float& dAz)
{
    if (amount <= 0 || order == 0) { dLen = 0; dAz = 0; return; }
    uint32_t h = (uint32_t) ((nx + 8) * 73856093) ^ (uint32_t) ((ny + 8) * 19349663) ^ (uint32_t) ((nz + 8) * 83492791);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    const float a = ((h & 0xffff) / 32768.0f) - 1.0f;          // -1 .. 1
    const float b = (((h >> 16) & 0xffff) / 32768.0f) - 1.0f;
    dLen = a * amount * (float) order;
    dAz  = b * 2.2f * (float) order;                            // degrees: twice a ~1 degree tilt
}

/*  A room that is no longer a box. Every ordered sequence of surfaces up to the
    given order is a candidate; tracePath mirrors the source across them in turn
    and then walks back from the listener, and a sequence survives only if every
    reflection point really lands on the surface it is supposed to and nothing is
    in the way. Costs more than the shoebox arithmetic and runs only for a room
    whose walls have actually been moved. */
void Engine::addPlanImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    const RoomGeom& g = reinterpret_cast<const RoomGeom*> (geomStore.data())[room];

    auto emit = [&] (const int* seq, int nseq)
    {
        if (pathsFull (8)) return;
        Vec3 hits[3]; float len = 0;
        if (! tracePath (g, S, L, seq, nseq, hits, len)) return;
        // a reflection that lands in an open doorway has left the room
        for (int k = 0; k < nseq; ++k)
            if (bounceHitsOpening (room, g.s[seq[k]].wallId, hits[k])) return;

        PathSpec& sp = specs[(size_t) nspecs];
        sp = PathSpec();
        uint32_t id = 0;
        for (int k = 0; k < nseq; ++k) id = id * 16u + (uint32_t) (seq[k] + 1);
        sp.key = keyBase + 0x2000u + id;
        sp.kind = nseq == 0 ? PathKind::Direct : (nseq == 1 ? PathKind::Refl1 : PathKind::Refl2);
        for (int k = 0; k < nseq; ++k)
        {
            const int sm = surfNow[room].of (g.s[seq[k]].wallId);
            for (int band = 0; band < NBAND; ++band)
                sp.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                    (1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band]))) + furnScatterDb[room];
        }
        sp.npts = 0; sp.pts[sp.npts++] = S;
        for (int k = 0; k < nseq; ++k) sp.pts[sp.npts++] = hits[k];
        sp.pts[sp.npts++] = L;
        const Vec3 arriveFrom = nseq > 0 ? hits[nseq - 1] : S;
        const Vec3 departTo   = nseq > 0 ? hits[0] : L;
        finishSpec (sp, arriveFrom, departTo, len);
        if (nseq == 0)
        {
            const float sy = std::sin (rad (lisYaw)), cy = std::cos (rad (lisYaw));
            const Vec3 rightV (sy, -cy, 0.0f);
            const float half = 0.5f * target.earSpan;
            sp.gainL = len / std::max ((S - (L - rightV * half)).len(), 0.08f);
            sp.gainR = len / std::max ((S - (L + rightV * half)).len(), 0.08f);
        }
        ++nspecs;
    };

    int seq[3];
    emit (seq, 0);
    if (maxOrder >= 1)
        for (int i = 0; i < g.n; ++i) { seq[0] = i; emit (seq, 1); }
    if (maxOrder >= 2)
        for (int i = 0; i < g.n; ++i)
            for (int j = 0; j < g.n; ++j)
            {
                if (i == j) continue;
                seq[0] = i; seq[1] = j; emit (seq, 2);
            }
}

void Engine::addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    if (roomIsBroken (room)) { addPlanImagePaths (room, S, L, maxOrder, keyBase); return; }
    const int mat = matNow[room];
    for (int order = 0; order <= maxOrder; ++order)
        for (int nx = -order; nx <= order; ++nx)
            for (int ny = -order; ny <= order; ++ny)
                for (int nz = -order; nz <= order; ++nz)
                {
                    if (std::abs (nx) + std::abs (ny) + std::abs (nz) != order) continue;
                    if (pathsFull (8)) return;
                    Vec3 b[4]; int nb = 0, walls[4]; float len;
                    if (! imagePath (room, S, L, nx, ny, nz, b, nb, walls, len)) continue;
                    PathSpec& s = specs[(size_t) nspecs];
                    s = PathSpec();
                    s.key = keyBase + (uint32_t) ((nx + 2) * 25 + (ny + 2) * 5 + (nz + 2));
                    s.kind = order == 0 ? PathKind::Direct : (order == 1 ? PathKind::Refl1 : PathKind::Refl2);
                    /*  Per bounce: what THAT surface absorbs, and what it scatters
                        out of the specular direction. The scattered part is not
                        lost - the late field's calibration picks it up. The floor
                        and the ceiling carry their own material, which is the whole
                        point of a carpet or a cloud. */
                    for (int i = 0; i < nb; ++i)
                    {
                        const int sm = surfNow[room].of (walls[i]);
                        for (int band = 0; band < NBAND; ++band)
                            s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                                (1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band]))) + furnScatterDb[room];
                    }
                    // picture: source, bounces from the source side, listener
                    s.npts = 0; s.pts[s.npts++] = S;
                    for (int i = nb - 1; i >= 0; --i) s.pts[s.npts++] = b[i];
                    s.pts[s.npts++] = L;
                    const Vec3 arriveFrom = nb > 0 ? b[0] : S;
                    const Vec3 departTo = nb > 0 ? b[nb - 1] : L;
                    float dLen = 0, dAz = 0;
                    splayOf (nx, ny, nz, order, MATERIAL_SPLAY[mat], dLen, dAz);
                    // the floor guards the PERTURBATION only: a genuinely short path
                    // must keep its own length, or a source held close to the head
                    // quietly loses level
                    finishSpec (s, arriveFrom, departTo, dLen != 0.0f ? std::max (0.2f, len + dLen) : len);
                    s.az += dAz;
                    if (order == 0)
                    {
                        // near field: each ear at its own distance
                        const float sy = std::sin (rad (lisYaw)), cy = std::cos (rad (lisYaw));
                        const Vec3 rightV (sy, -cy, 0.0f);
                        const float half = 0.5f * target.earSpan;
                        const float rL = (S - (L - rightV * half)).len();
                        const float rR = (S - (L + rightV * half)).len();
                        s.gainL = len / std::max (rL, 0.08f);
                        s.gainR = len / std::max (rR, 0.08f);
                    }
                    ++nspecs;
                }
}

/*  Paths through a doorway. The straight line from P (a source or one of its
    images) to Q (the listener or one of its images) crosses the door plane at X;
    if X falls in the open strip the path is line of sight, otherwise it bends at
    the nearest point of the opening and loses Maekawa's diffraction attenuation
    per band. Either way the sound arrives from the doorway. */
static bool portalPoint (int door, float aperture, const Vec3& P, const Vec3& Q, Vec3& Xp, float& delta)
{
    const Door& d = DOORS[door];
    float lo, hi; openStrip (door, aperture, lo, hi);
    if (hi - lo < 0.02f) return false;
    Vec3 X;
    if (! crossPlane (d, P, Q, X)) return false;
    const float s = std::max (lo, std::min (hi, spanCoord (d, X)));
    const float z = std::max (0.05f, std::min (d.height - 0.05f, X.z));
    Xp = fromSpan (d, s, z);
    const float straight = (Q - P).len();
    const float bent = (Xp - P).len() + (Q - Xp).len();
    delta = bent - straight;                 // 0 when the line of sight is inside the opening
    if (delta < 1e-4f)
    {
        // line of sight: distance to the nearest edge of the opening, as a negative delta
        const float ds = std::min (spanCoord (d, X) - lo, hi - spanCoord (d, X));
        const float dz = std::min (X.z, d.height - X.z);
        const bool useS = ds < dz;
        Vec3 E = useS ? fromSpan (d, (spanCoord (d, X) - lo < hi - spanCoord (d, X)) ? lo : hi, X.z)
                      : fromSpan (d, spanCoord (d, X), (X.z < d.height - X.z) ? 0.0f : d.height);
        delta = -((E - P).len() + (Q - E).len() - straight);
    }
    return true;
}

static void addDiffraction (PathSpec& s, float delta)
{
    for (int b = 0; b < NBAND; ++b)
    {
        const float N = 2.0f * delta * BAND_HZ[b] / SPEED_OF_SOUND;
        s.bandDb[b] -= Engine::diffractionDb (N);
    }
}

static Vec3 imageOf (int room, const Vec3& P, int nx, int ny, int nz)
{
    const Room& R = ROOMS[room];
    const float lo[3] = { R.x0, R.y0, 0 }, hi[3] = { R.x1, R.y1, R.h };
    const int n[3] = { nx, ny, nz }; const float pc[3] = { P.x, P.y, P.z };
    float ic[3];
    for (int a = 0; a < 3; ++a) { const float len = hi[a] - lo[a]; const float u = pc[a] - lo[a]; ic[a] = lo[a] + n[a] * len + ((n[a] & 1) ? (len - u) : u); }
    return Vec3 (ic[0], ic[1], ic[2]);
}

/*  How far P stands from door d's opening: its distance from the plane (also
    returned as past), combined with how far it is beside the open strip. Very
    large for a door with no opening. */
float Engine::doorZone (int d, const Vec3& P, float& past) const
{
    const Door& D = DOORS[d];
    past = std::abs (planeCoord (D, P) - D.pos);
    float lo, hi; openStrip (d, doorNow[d], lo, hi);
    if (hi - lo < 0.02f) return 1e9f;
    const float s = spanCoord (D, P);
    const float ls = std::max ({ lo - s, 0.0f, s - hi });
    return std::sqrt (past * past + ls * ls);
}

// does the straight move a -> b pass through the opening of a door between their two rooms?
bool Engine::throughOpenDoor (const Vec3& a, const Vec3& b) const
{
    const int ra = roomOf (a.x, a.y), rb = roomOf (b.x, b.y);
    if (ra < 0 || rb < 0 || ra == rb) return false;
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (! ((D.roomA == ra && D.roomB == rb) || (D.roomA == rb && D.roomB == ra))) continue;
        float lo, hi; openStrip (d, doorNow[d], lo, hi);
        if (hi - lo < 0.02f) continue;
        Vec3 X; if (! crossPlane (D, a, b, X)) continue;
        const float s = spanCoord (D, X);
        if (s >= lo && s <= hi) return true;
    }
    return false;
}

void Engine::addPortalPaths (const Vec3& S, int rs, int rl)
{
    if (rs == rl || rs < 0 || rl < 0) return;

    // ---- one door straight between the two rooms
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (! ((D.roomA == rs && D.roomB == rl) || (D.roomB == rs && D.roomA == rl))) continue;
        if (doorNow[d] < 0.02f) continue;
        const int wallL = doorWall (rl, d);

        /*  The transition zone. u is how far the listener stands from the opening
            (past the plane, or beside the strip), in zone widths; w fades in what
            exists only on this side of the door. At the plane, inside the
            opening, w = 0 and litFade = 0. */
        float tPast = 0;
        const float zu = std::min (1.0f, doorZone (d, lisPos, tPast) / DOOR_ZONE_M);
        const float wZone = zu * zu * (3.0f - 2.0f * zu);
        const float litFade = std::min (1.0f, tPast / DOOR_ZONE_M);
        const int mat = matNow[rs];

        /*  (a) + (b): the source room's own image paths, direct and to second
            order, seen through the opening. Same keys, materials, splay and
            arrival direction as the in-room model (addImagePaths): with the
            listener IN the opening they are the same paths, so walking over the
            threshold carries every slot on instead of swapping one model for
            another - which is what the old first-order set did, in 2.7 ms, and it
            was heard as a bump. A path that is line of sight through the opening
            arrives from its own last bounce; a bent one arrives from the doorway.
            The lit side's edge loss fades in over the zone, because a listener
            standing in the opening hears the room as if there were no wall. */
        for (int order = 0; order <= 2; ++order)
            for (int nx = -order; nx <= order; ++nx)
                for (int ny = -order; ny <= order; ++ny)
                    for (int nz = -order; nz <= order; ++nz)
                    {
                        if (std::abs (nx) + std::abs (ny) + std::abs (nz) != order) continue;
                        // second order is what makes the two models meet at the plane;
                        // deep in the far room it is far down and was never modelled,
                        // so it fades out across the zone rather than costing paths
                        if (order == 2 && wZone >= 1.0f) continue;
                        if (pathsFull (4)) return;
                        const Vec3 I = imageOf (rs, S, nx, ny, nz);
                        Vec3 Xp; float delta;
                        if (! portalPoint (d, doorNow[d], I, lisPos, Xp, delta)) continue;
                        Vec3 b[4]; int nb = 0, walls[4]; float dummy;
                        if (order > 0 && ! imagePath (rs, S, Xp, nx, ny, nz, b, nb, walls, dummy)) continue;
                        const bool lit = delta <= 0.0f;
                        PathSpec& s = specs[(size_t) nspecs]; s = PathSpec();
                        s.key = (uint32_t) ((nx + 2) * 25 + (ny + 2) * 5 + (nz + 2));
                        s.kind = order == 0 ? (lit ? PathKind::Direct : PathKind::Portal)
                                            : (order == 1 ? PathKind::Refl1 : PathKind::Refl2);
                        for (int i = 0; i < nb; ++i)
                        {
                            const int sm = surfNow[rs].of (walls[i]);
                            for (int band = 0; band < NBAND; ++band)
                                s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                                    (1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band]))) + furnScatterDb[rs];
                        }
                        addDiffraction (s, lit ? delta * litFade : delta);
                        s.npts = 0; s.pts[s.npts++] = S;
                        for (int i = nb - 1; i >= 0; --i) s.pts[s.npts++] = b[i];
                        if (! lit) s.pts[s.npts++] = Xp;
                        s.pts[s.npts++] = lisPos;
                        const Vec3 arriveFrom = lit ? (nb > 0 ? b[0] : S) : Xp;
                        const Vec3 departTo = nb > 0 ? b[nb - 1] : (lit ? lisPos : Xp);
                        const float len = (Xp - I).len() + (lisPos - Xp).len();
                        float dLen = 0, dAz = 0;
                        splayOf (nx, ny, nz, order, MATERIAL_SPLAY[mat], dLen, dAz);
                        finishSpec (s, arriveFrom, departTo, dLen != 0.0f ? std::max (0.2f, len + dLen) : len);
                        s.az += dAz;
                        if (order == 2) s.gain *= 1.0f - wZone;
                        if (order == 0 && lit)
                        {
                            const float sy = std::sin (rad (lisYaw)), cy = std::cos (rad (lisYaw));
                            const Vec3 rightV (sy, -cy, 0.0f);
                            const float half = 0.5f * target.earSpan;
                            s.gainL = len / std::max ((S - (lisPos - rightV * half)).len(), 0.08f);
                            s.gainR = len / std::max ((S - (lisPos + rightV * half)).len(), 0.08f);
                        }
                        ++nspecs;
                    }

        // (c) the direct portal path reflected once in the LISTENER's room
        for (int w = 0; w < 6; ++w)
        {
            if (w == wallL) continue;
            if (pathsFull (4)) return;
            const int nx = (w == 0) ? -1 : (w == 1 ? 1 : 0);
            const int ny = (w == 2) ? -1 : (w == 3 ? 1 : 0);
            const int nz = (w == 4) ? -1 : (w == 5 ? 1 : 0);
            const Vec3 Q = imageOf (rl, lisPos, nx, ny, nz);
            Vec3 Xp; float delta;
            if (! portalPoint (d, doorNow[d], S, Q, Xp, delta)) continue;
            Vec3 b[4]; int nb = 0, walls[4]; float dummy;
            if (! imagePath (rl, Xp, lisPos, nx, ny, nz, b, nb, walls, dummy)) continue;
            PathSpec& s = specs[(size_t) nspecs]; s = PathSpec();
            s.key = 0x20000u + (uint32_t) d * 8u + (uint32_t) w;
            s.kind = PathKind::Portal;
            const float len = (Xp - S).len() + (Q - Xp).len();
            for (int band = 0; band < NBAND; ++band) s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f, (1.0f - MATERIAL_ALPHA[matNow[rl]][band]) * (1.0f - MATERIAL_SCATTER[matNow[rl]][band]))) + furnScatterDb[rl];
            addDiffraction (s, delta);
            s.npts = 0; s.pts[s.npts++] = S; s.pts[s.npts++] = Xp;
            if (nb > 0) s.pts[s.npts++] = b[0];
            s.pts[s.npts++] = lisPos;
            finishSpec (s, nb > 0 ? b[0] : Xp, Xp, len);
            s.gain *= wZone;                         // exists only once past the plane
            ++nspecs;
        }
    }

    // ---- two doors, through the third room
    const int mid = 3 - rs - rl;
    int d1 = -1, d2 = -1;
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if ((D.roomA == rs && D.roomB == mid) || (D.roomB == rs && D.roomA == mid)) d1 = d;
        if ((D.roomA == mid && D.roomB == rl) || (D.roomB == mid && D.roomA == rl)) d2 = d;
    }
    if (d1 >= 0 && d2 >= 0 && doorNow[d1] > 0.02f && doorNow[d2] > 0.02f && ! pathsFull (4))
    {
        Vec3 X1, X2; float del1 = 0, del2 = 0;
        if (portalPoint (d1, doorNow[d1], S, lisPos, X1, del1))
        {
            bool ok = true;
            for (int it = 0; it < 4 && ok; ++it)
                ok = portalPoint (d2, doorNow[d2], X1, lisPos, X2, del2)
                  && portalPoint (d1, doorNow[d1], S, X2, X1, del1);
            if (ok)
            {
                PathSpec& s = specs[(size_t) nspecs]; s = PathSpec();
                s.key = 0x30000u + (uint32_t) d1 * 4u + (uint32_t) d2;
                s.kind = PathKind::Portal;
                const float len = (X1 - S).len() + (X2 - X1).len() + (lisPos - X2).len();
                addDiffraction (s, del1); addDiffraction (s, del2);
                s.npts = 0; s.pts[s.npts++] = S; s.pts[s.npts++] = X1; s.pts[s.npts++] = X2; s.pts[s.npts++] = lisPos;
                finishSpec (s, X2, X1, len);
                ++nspecs;
            }
        }
    }
}

/*  Sound through the solid parts: a closed (or partly closed) leaf radiates
    from the point of it nearest the straight line, the party wall likewise.
    Both are localised - a door you cannot see through is still exactly where
    the muffled sound comes from. */
void Engine::addTransmissionPaths (const Vec3& S, int rs, int rl)
{
    if (rs == rl || rs < 0 || rl < 0) return;

    // near an open door between the two rooms these fade in with the zone, like
    // everything else that exists only on the far side of the plane
    float wz = 1.0f;
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (! ((D.roomA == rs && D.roomB == rl) || (D.roomB == rs && D.roomA == rl))) continue;
        float past; const float zu = std::min (1.0f, doorZone (d, lisPos, past) / DOOR_ZONE_M);
        wz = std::min (wz, zu * zu * (3.0f - 2.0f * zu));
    }

    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (! ((D.roomA == rs && D.roomB == rl) || (D.roomB == rs && D.roomA == rl))) continue;
        const float closed = 1.0f - doorNow[d];
        if (closed < 0.02f || pathsFull (2)) continue;
        Vec3 X; if (! crossPlane (D, S, lisPos, X)) continue;
        float lo, hi; openStrip (d, doorNow[d], lo, hi);
        float ls0, ls1;
        if (D.hingeAtS0) { ls0 = D.s0; ls1 = lo; } else { ls0 = hi; ls1 = D.s1; }
        const float s = std::max (ls0, std::min (ls1, spanCoord (D, X)));
        const float z = std::max (0.1f, std::min (D.height - 0.1f, X.z));
        const Vec3 C = fromSpan (D, s, z);
        PathSpec& p = specs[(size_t) nspecs]; p = PathSpec();
        p.key = 0x40000u + (uint32_t) d; p.kind = PathKind::Leaf;
        for (int b = 0; b < NBAND; ++b) p.bandDb[b] -= LEAF_TL_DB[b];
        const float len = (C - S).len() + (lisPos - C).len();
        p.npts = 0; p.pts[p.npts++] = S; p.pts[p.npts++] = C; p.pts[p.npts++] = lisPos;
        finishSpec (p, C, C, len);
        p.gain *= std::sqrt (closed) * wz;
        ++nspecs;
    }

    int axis; float pos, s0, s1, h;
    if (sharedWall (rs, rl, axis, pos, s0, s1, h) && ! pathsFull (2))
    {
        const float a = (axis == 0 ? S.x : S.y) - pos, b = (axis == 0 ? lisPos.x : lisPos.y) - pos;
        const float t = (std::abs (a - b) < 1e-9f) ? 0.5f : a / (a - b);
        Vec3 X = S + (lisPos - S) * t;
        const float s = std::max (s0 + 0.05f, std::min (s1 - 0.05f, axis == 0 ? X.y : X.x));
        const float z = std::max (0.1f, std::min (h - 0.1f, X.z));
        const Vec3 C = axis == 0 ? Vec3 (pos, s, z) : Vec3 (s, pos, z);
        PathSpec& p = specs[(size_t) nspecs]; p = PathSpec();
        p.key = 0x50000u + (uint32_t) pairIndex (rs, rl); p.kind = PathKind::Wall;
        for (int band = 0; band < NBAND; ++band) p.bandDb[band] -= WALL_TL_DB[band];
        const float len = (C - S).len() + (lisPos - C).len();
        p.npts = 0; p.pts[p.npts++] = S; p.pts[p.npts++] = C; p.pts[p.npts++] = lisPos;
        finishSpec (p, C, C, len);
        p.gain *= wz;
        ++nspecs;
    }
}

/*  A neighbouring room's late field heard through its door: the flux c E S tau / 4
    through the opening spreads over a hemisphere, so the pressure at distance R
    is E[y^2] S tau / (8 pi R^2), arriving from the doorway - rendered at its two
    edges so it is as wide as the door. */
void Engine::addDoorFieldPaths (int rl, float scale)
{
    if (rl < 0 || scale < 1e-4f) return;
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (D.roomA != rl && D.roomB != rl) continue;
        const int other = D.roomA == rl ? D.roomB : D.roomA;
        const float zc = std::min (EAR_HEIGHT, D.height - 0.2f);
        const Vec3 centre = fromSpan (D, 0.5f * (D.s0 + D.s1), zc);
        const float R = std::max (0.6f, (centre - lisPos).len());
        for (int e = 0; e < 2; ++e)
        {
            if (pathsFull (1)) return;
            const Vec3 edge = fromSpan (D, e == 0 ? D.s0 + 0.05f : D.s1 - 0.05f, zc);
            PathSpec& p = specs[(size_t) nspecs]; p = PathSpec();
            // keyed by the room whose field it carries: crossing a door used to
            // hand the same key a different feed, and the slot read on regardless
            p.key = 0xF0000000u + ((uint32_t) other << 8) + (uint32_t) d * 2u + (uint32_t) e;
            p.kind = PathKind::DoorField; p.src = -1; p.feed = MAX_SOURCES + other;
            p.length = R;
            p.gain = scale / (R * std::sqrt (2.0f));
            for (int b = 0; b < NBAND; ++b)
                p.bandDb[b] = 10.0f * std::log10 (std::max (1e-9f, D.area() * doorTau[d][b] / (8.0f * PI)));
            headRelative (edge, p.az, p.el);
            p.npts = 0;
            ++nspecs;
        }
    }
}

//==============================================================================
/*  THE LIGHT. How much sound is in each room right now, for the lamps to follow:
    what its sources play (at their level) plus the room's own late field, as an
    RMS per sub-block. A fast envelope (5 ms up, 90 ms down) against a slow one
    (0.45 s) gives the PUNCH - an onset reads as fast/slow above 1 - and the
    fast one on a -48..-12 dB scale gives the LEVEL. The lamps take half of
    each, so a sustained pad glows and a drum hit flashes. Silence is exactly 0. */
void Engine::updateLight (int n)
{
    const float dt = (float) n / (float) fs;
    const float att = 1.0f - std::exp (-dt / 0.005f), rel = 1.0f - std::exp (-dt / 0.09f);
    const float slow = 1.0f - std::exp (-dt / 0.45f);
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        double e = fieldAcc[r]; fieldAcc[r] = 0;
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            if (! cur.src[s].active() || roomOf (srcPos[s].x, srcPos[s].y) != r) continue;
            const float* mi = monoIn[s].data();
            double a = 0; for (int i = 0; i < n; ++i) a += (double) mi[i] * mi[i];
            e += a * (double) srcLevel[s] * srcLevel[s];
        }
        const float rms = (float) std::sqrt (e / std::max (1, n));
        lightFast[r] += (rms > lightFast[r] ? att : rel) * (rms - lightFast[r]);
        lightSlow[r] += slow * (lightFast[r] - lightSlow[r]);
        if (lightFast[r] < 1.0e-7f) { lightOut[r] = 0.0f; continue; }
        const float level = std::max (0.0f, std::min (1.0f, (20.0f * std::log10 (lightFast[r]) + 48.0f) / 36.0f));
        const float punch = std::max (0.0f, std::min (1.0f, (lightFast[r] / (lightSlow[r] + 1.0e-6f) - 1.0f) / 1.5f));
        lightOut[r] = std::max (0.0f, std::min (1.0f, 0.5f * level + 0.5f * punch * std::min (1.0f, 2.0f * level)));
    }
}

//==============================================================================
// furniture geometry
namespace
{
    inline float len3 (float x, float y, float z) { return std::sqrt (x * x + y * y + z * z); }

    // Liang-Barsky: the part of the 2D segment a->b inside |x| <= hw, |y| <= hd
    bool clipRect (float ax, float ay, float bx, float by, float hw, float hd, float& u0, float& u1)
    {
        u0 = 0; u1 = 1;
        const float dx = bx - ax, dy = by - ay;
        auto clip = [&] (float p, float q)
        {
            if (std::abs (p) < 1e-9f) return q >= 0;
            const float r = q / p;
            if (p < 0) { if (r > u1) return false; if (r > u0) u0 = r; }
            else       { if (r < u0) return false; if (r < u1) u1 = r; }
            return true;
        };
        return clip (-dx, ax + hw) && clip (dx, hw - ax) && clip (-dy, ay + hd) && clip (dy, hd - ay) && u1 > u0;
    }

    /*  The shortest way round the rectangle in the plane from p to q, both
        outside it: the convex hull of p, q and the four corners has two chains
        between p and q, one down each side; the shorter is the answer. */
    float aroundRect (float px, float py, float qx, float qy, float hw, float hd)
    {
        const float X[6] = { px, qx, -hw, hw, hw, -hw }, Y[6] = { py, qy, -hd, -hd, hd, hd };
        int idx[6] = { 0, 1, 2, 3, 4, 5 };
        std::sort (idx, idx + 6, [&] (int a, int b) { return X[a] < X[b] || (X[a] == X[b] && Y[a] < Y[b]); });
        auto cross = [&] (int o, int a, int b) { return (X[a] - X[o]) * (Y[b] - Y[o]) - (Y[a] - Y[o]) * (X[b] - X[o]); };
        int H[14]; int k = 0;
        for (int i = 0; i < 6; ++i) { while (k >= 2 && cross (H[k - 2], H[k - 1], idx[i]) <= 0) --k; H[k++] = idx[i]; }
        for (int i = 4, t = k + 1; i >= 0; --i) { while (k >= t && cross (H[k - 2], H[k - 1], idx[i]) <= 0) --k; H[k++] = idx[i]; }
        --k;
        int ip = -1, iq = -1;
        for (int i = 0; i < k; ++i) { if (H[i] == 0) ip = i; if (H[i] == 1) iq = i; }
        if (ip < 0 || iq < 0 || k < 3) return -1.0f;
        auto seg = [&] (int a, int b) { return std::hypot (X[H[b]] - X[H[a]], Y[H[b]] - Y[H[a]]); };
        float c1 = 0; for (int i = ip; i != iq; i = (i + 1) % k) c1 += seg (i, (i + 1) % k);
        float c2 = 0; for (int i = iq; i != ip; i = (i + 1) % k) c2 += seg (i, (i + 1) % k);
        return std::min (c1, c2);
    }
}

void Engine::updateFurniture()
{
    nfurnNow = 0;
    const int nf = std::min (std::max (cur.nfurn, 0), MAX_FURN);
    for (int i = 0; i < nf; ++i)
    {
        const FurnItem& it = cur.furn[i];
        FurnPose& p = furnPose[i];
        p = FurnPose();
        if (it.type >= 0 && it.type < NUM_FURN_TYPES)
        {
            const FurnSpec& F = FURN[it.type];
            const float a = rad (it.yaw);
            p.cx = it.x; p.cy = it.y; p.c = std::cos (a); p.s = std::sin (a);
            p.hw = 0.5f * F.w; p.hd = 0.5f * F.d; p.zb = F.zb; p.zt = F.zt;
            p.type = it.type; p.room = roomOf (it.x, it.y);
        }
        nfurnNow = i + 1;
    }
}

/*  The Maekawa path difference of the leg P->Q round piece i. Blocked: the
    shortest way over the top, under the bottom (a table, a piano on its legs)
    or round the sides, less the straight line. Clear: minus how much longer a
    path touching the piece at its point nearest the leg would be - the lit side
    of the same curve, so an edge comes and goes without a step. */
float Engine::furnitureDelta (int i, const Vec3& P, const Vec3& Q) const
{
    if (i < 0 || i >= nfurnNow) return -1.0e9f;
    const FurnPose& f = furnPose[i];
    if (f.type < 0 || ! FURN[f.type].occludes) return -1.0e9f;
    auto loc = [&] (const Vec3& p, float& x, float& y)
    {
        const float dx = p.x - f.cx, dy = p.y - f.cy;
        x = f.c * dx + f.s * dy; y = -f.s * dx + f.c * dy;
    };
    float px, py, qx, qy; loc (P, px, py); loc (Q, qx, qy);
    const float pz = P.z, qz = Q.z;
    const float m = 1.0f;     // beyond this the lit side is below a quarter of a dB at 125 Hz
    if (std::max (px, qx) < -f.hw - m || std::min (px, qx) > f.hw + m
     || std::max (py, qy) < -f.hd - m || std::min (py, qy) > f.hd + m
     || std::max (pz, qz) < f.zb - m || std::min (pz, qz) > f.zt + m) return -1.0e9f;
    const float e = 1.0e-3f;
    auto inside = [&] (float x, float y, float z) { return std::abs (x) < f.hw - e && std::abs (y) < f.hd - e && z > f.zb + e && z < f.zt - e; };
    if (inside (px, py, pz) || inside (qx, qy, qz)) return -1.0e9f;     // nothing blocks itself
    const float straight = len3 (qx - px, qy - py, qz - pz);
    if (straight < 1.0e-4f) return -1.0e9f;

    float u0 = 0, u1 = 0;
    const bool xy = clipRect (px, py, qx, qy, f.hw, f.hd, u0, u1);
    const float z0 = pz + (qz - pz) * u0, z1 = pz + (qz - pz) * u1;
    const bool blocked = xy && std::max (z0, z1) > f.zb && std::min (z0, z1) < f.zt;
    if (blocked)
    {
        const float ax = px + (qx - px) * u0, ay = py + (qy - py) * u0;
        const float bx = px + (qx - px) * u1, by = py + (qy - py) * u1;
        float best = 1.0e9f;
        {   // over the top
            const float za = std::max (z0, f.zt), zb2 = std::max (z1, f.zt);
            const float L = len3 (ax - px, ay - py, za - pz) + len3 (bx - ax, by - ay, zb2 - za) + len3 (qx - bx, qy - by, qz - zb2);
            best = std::min (best, L - straight);
        }
        if (f.zb > 0.02f)
        {   // under it
            const float za = std::min (z0, f.zb), zb2 = std::min (z1, f.zb);
            const float L = len3 (ax - px, ay - py, za - pz) + len3 (bx - ax, by - ay, zb2 - za) + len3 (qx - bx, qy - by, qz - zb2);
            best = std::min (best, L - straight);
        }
        const bool pin = std::abs (px) <= f.hw && std::abs (py) <= f.hd;
        const bool qin = std::abs (qx) <= f.hw && std::abs (qy) <= f.hd;
        if (! pin && ! qin)
        {   // round the sides
            const float a2 = aroundRect (px, py, qx, qy, f.hw, f.hd);
            if (a2 > 0) best = std::min (best, len3 (a2, 0.0f, qz - pz) - straight);
        }
        return std::max (1.0e-5f, best);
    }
    // clear: the point of the box nearest the leg (distance is convex along it)
    auto nearest = [&] (float u, float& cx, float& cy, float& cz)
    {
        const float x = px + (qx - px) * u, y = py + (qy - py) * u, z = pz + (qz - pz) * u;
        cx = std::max (-f.hw, std::min (f.hw, x)); cy = std::max (-f.hd, std::min (f.hd, y)); cz = std::max (f.zb, std::min (f.zt, z));
        return len3 (x - cx, y - cy, z - cz);
    };
    float lo = 0, hi = 1, cx, cy, cz;
    for (int it = 0; it < 30; ++it)
    {
        const float m1 = lo + (hi - lo) / 3.0f, m2 = hi - (hi - lo) / 3.0f;
        if (nearest (m1, cx, cy, cz) < nearest (m2, cx, cy, cz)) hi = m2; else lo = m1;
    }
    nearest (0.5f * (lo + hi), cx, cy, cz);
    const float L = len3 (cx - px, cy - py, cz - pz) + len3 (qx - cx, qy - cy, qz - cz);
    return -(L - straight);
}

void Engine::applyOcclusion()
{
    if (nfurnNow == 0) return;
    for (int i = 0; i < nspecs; ++i)
    {
        PathSpec& s = specs[(size_t) i];
        if (s.kind == PathKind::DoorField || s.npts < 2) continue;
        for (int k = 0; k + 1 < s.npts; ++k)
            for (int f = 0; f < nfurnNow; ++f)
            {
                if (f == s.selfItem) continue;
                const float d = furnitureDelta (f, s.pts[k], s.pts[k + 1]);
                if (d > -0.5f) addDiffraction (s, d);
            }
    }
}

/*  A hard horizontal top - a table, a closed piano lid - mirrors the source like
    a floor would, but it is finite: while the mirror point is on the top the
    path is line of sight with the lit-side edge term of its nearest edge, and
    once the point leaves it the path bends at the edge with Maekawa's loss, so
    it fades out rather than vanishing when you step past the table. */
void Engine::addFurnitureReflections (const Vec3& S, int rs, int rl)
{
    if (rs < 0 || rs != rl || nfurnNow == 0) return;
    for (int i = 0; i < nfurnNow; ++i)
    {
        const FurnPose& f = furnPose[i];
        if (f.type < 0 || f.room != rs) continue;
        const FurnSpec& F = FURN[f.type];
        if (! F.reflectTop) continue;
        const float zt = f.zt;
        if (S.z <= zt + 0.02f || lisPos.z <= zt + 0.02f) continue;
        if (pathsFull (2)) return;
        const Vec3 I (S.x, S.y, 2.0f * zt - S.z);
        const float tt = (zt - I.z) / (lisPos.z - I.z);
        const Vec3 R = I + (lisPos - I) * tt;
        const float dx = R.x - f.cx, dy = R.y - f.cy;
        const float rx = f.c * dx + f.s * dy, ry = -f.s * dx + f.c * dy;
        const bool on = std::abs (rx) <= f.hw && std::abs (ry) <= f.hd;
        /*  The edge the path would graze: on the top, the SMALLEST detour over all
            four edges (each at its point nearest the mirror point) - taking the
            nearest edge in the plane instead switched edges with a 1.8 dB step,
            measured; the minimum of four continuous detours is continuous. Off
            the top, the nearest point of the rim. */
        const Vec3 R0 (f.cx, f.cy, zt);
        auto world = [&] (float ex, float ey) { return Vec3 (f.cx + f.c * ex - f.s * ey, f.cy + f.s * ex + f.c * ey, zt); };
        const float straight = (lisPos - I).len();
        Vec3 Ed = R0; float viaE = 1.0e9f;
        if (on)
        {
            const float cx = std::max (-f.hw, std::min (f.hw, rx)), cy = std::max (-f.hd, std::min (f.hd, ry));
            const Vec3 cand[4] = { world (f.hw, cy), world (-f.hw, cy), world (cx, f.hd), world (cx, -f.hd) };
            for (const Vec3& E : cand)
            {
                const float v = (E - I).len() + (lisPos - E).len();
                if (v < viaE) { viaE = v; Ed = E; }
            }
        }
        else
        {
            Ed = world (std::max (-f.hw, std::min (f.hw, rx)), std::max (-f.hd, std::min (f.hd, ry)));
            viaE = (Ed - I).len() + (lisPos - Ed).len();
        }
        (void) R0;
        const float delta = on ? -(viaE - straight) : (viaE - straight);
        if (delta > 1.5f) continue;
        const Vec3 X = on ? R : Ed;
        PathSpec& s = specs[(size_t) nspecs]; s = PathSpec();
        s.key = 0x60000u + (uint32_t) i * 8u;
        s.kind = PathKind::Refl1;
        s.selfItem = i;
        const float topDb = 10.0f * std::log10 (std::max (1.0e-4f, 1.0f - F.topAlpha));
        for (int b = 0; b < NBAND; ++b) s.bandDb[b] += topDb + furnScatterDb[rs];
        addDiffraction (s, delta);
        s.npts = 0; s.pts[s.npts++] = S; s.pts[s.npts++] = X; s.pts[s.npts++] = lisPos;
        const float len = on ? straight : (X - S).len() + (lisPos - X).len();
        finishSpec (s, X, X, len);
        ++nspecs;
    }
}

void Engine::buildPaths()
{
    nspecs = 0;
    pathsDropped = 0;
    const int rl = roomOf (lisPos.x, lisPos.y);
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        if (! cur.src[s].active()) continue;
        curSrc = s;
        const Vec3& S = srcPos[s];
        const int rs = roomOf (S.x, S.y);
        if (rs >= 0 && rs == rl) addImagePaths (rs, S, lisPos, 2, 0u);
        addPortalPaths (S, rs, rl);
        addTransmissionPaths (S, rs, rl);
        addFurnitureReflections (S, rs, rl);
    }
    applyOcclusion();
    /*  The late fields. Inside a doorway's transition zone the room on the other
        side is a second "listener's room": the two are blended on POWER (two
        diffuse fields are uncorrelated), half and half at the plane, so a walk
        through the door hands one field to the other without a step. Each
        assignment brings its own door fields, scaled the same way. */
    int zoneRoom = -1; float wHere = 1.0f;
    if (rl >= 0)
    {
        float best = DOOR_ZONE_M;
        for (int d = 0; d < NUM_DOORS; ++d)
        {
            const Door& D = DOORS[d];
            if (D.roomA != rl && D.roomB != rl) continue;
            float past; const float z = doorZone (d, lisPos, past);
            if (z < best) { best = z; zoneRoom = D.roomA == rl ? D.roomB : D.roomA; }
        }
        if (zoneRoom >= 0) { const float u = best / DOOR_ZONE_M; wHere = 0.5f + 0.5f * u * u * (3.0f - 2.0f * u); }
    }
    addDoorFieldPaths (rl, std::sqrt (wHere));
    if (zoneRoom >= 0) addDoorFieldPaths (zoneRoom, std::sqrt (1.0f - wHere));

    // the room weights for the diffuse render (amplitude, hence the square roots)
    for (int r = 0; r < NUM_ROOMS; ++r)
        rooms[(size_t) r].weightTarget = (r == rl) ? std::sqrt (wHere) : (r == zoneRoom ? std::sqrt (1.0f - wHere) : 0.0f);

    // each source's late field: 16 pi / A of reverberant energy in all, minus
    // what its rendered images already carry (at 1 kHz)
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        if (! cur.src[s].active()) { srcInGain[s] = 0; continue; }
        const int rs = std::max (0, roomOf (srcPos[s].x, srcPos[s].y));
        const RoomField& F = rooms[(size_t) rs];
        double early = 0;
        for (int i = 0; i < nspecs; ++i)
        {
            const PathSpec& p = specs[(size_t) i];
            if (p.src == s && (p.kind == PathKind::Refl1 || p.kind == PathKind::Refl2))
            { const double g = p.gain * std::pow (10.0, p.bandDb[3] / 20.0); early += g * g; }
        }
        const double all = 16.0 * PI / F.absorptionArea[3];
        const double fraction = std::max (0.10, std::min (1.0, 1.0 - early / all));
        if (s == 0) { dbgEarly = early; dbgAll = all; dbgFraction = fraction; }
        srcInGain[s] = F.inGain * (float) std::sqrt (fraction);
    }
}

//==============================================================================
void Engine::assignSlots()
{
    // retire slots whose path is gone
    for (auto& s : slots)
    {
        if (! s.active) continue;
        bool found = false;
        for (int i = 0; i < nspecs && ! found; ++i) found = specs[(size_t) i].key == s.key;
        if (! found) s.envTarget = 0;         // fade out on its last target, not in one block
    }
    for (int i = 0; i < nspecs; ++i)
    {
        const PathSpec& sp = specs[(size_t) i];
        PathSlot* slot = nullptr;
        for (auto& s : slots) if (s.active && s.key == sp.key) { slot = &s; break; }
        if (slot == nullptr)
        {
            for (auto& s : slots) if (! s.active) { slot = &s; break; }
            if (slot == nullptr) continue;             // out of slots: the quietest paths are last
            slot->active = true; slot->key = sp.key; slot->fresh = true; slot->gain = 0;
            slot->env = snapFades ? 1.0f : 0.0f;
            slot->filt.reset(); slot->crossing = false;
            std::fill (slot->hist.begin(), slot->hist.end(), 0.0f);
        }
        slot->kind = sp.kind; slot->feed = sp.feed; slot->envTarget = 1.0f;
        const float trim = (sp.kind == PathKind::Refl1 || sp.kind == PathKind::Refl2) ? trimEarly
                         : (sp.kind == PathKind::DoorField ? trimReverb : trimDirect);
        const float level = sp.kind == PathKind::DoorField ? 1.0f : srcLevel[sp.src];
        slot->gainTarget = sp.gain * trim * level;
        slot->gLTarget = sp.gainL; slot->gRTarget = sp.gainR;
        slot->delayTarget = std::max ((float) FracDelay::MIN_DELAY, sp.length / SPEED_OF_SOUND * (float) fs);
        slot->filtTarget.setBandsDb (sp.bandDb);
        float it;
        hrtf.lookup (sp.az, sp.el, tmpA.data(), tmpB.data(), it);
        slot->itdTarget = it * (target.earSpan / (2.0f * HEAD_RADIUS));   // a wider pair, a longer ITD
        const int ntapPad = (ntap + 15) & ~15;
        storeReversed (tmpA.data(), ntap, 0, slot->hLTarget.data(), ntapPad);
        storeReversed (tmpB.data(), ntap, 0, slot->hRTarget.data(), ntapPad);
        if (slot->fresh)
        {
            slot->delay = slot->delayTarget; slot->itd = slot->itdTarget;
            slot->gL = slot->gLTarget; slot->gR = slot->gRTarget;
            slot->filt.copyCoeffs (slot->filtTarget);
            slot->hL = slot->hLTarget; slot->hR = slot->hRTarget;
            slot->fresh = false;
        }
    }
    int n = 0; for (auto& s : slots) if (s.active) ++n;
    activePaths = n;
}

//==============================================================================
void Engine::publishScene()
{
    Scene& sc = sceneBuf[1 - sceneIdx.load()];
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        SceneSource& o = sc.sources[s];
        o.pos = srcPos[s]; o.yaw = srcYaw[s]; o.type = cur.src[s].type; o.directivity = cur.src[s].directivity;
        o.room = std::max (0, roomOf (srcPos[s].x, srcPos[s].y)); o.active = cur.src[s].active();
    }
    sc.lis = lisPos; sc.lisYaw = lisYaw;
    sc.lisRoom = std::max (0, roomOf (lisPos.x, lisPos.y));
    sc.pathsDropped = pathsDropped;
    for (int r = 0; r < NUM_ROOMS; ++r) sc.light[r] = lightOut[r];
    for (int r = 0; r < NUM_ROOMS; ++r) sc.furnA[r] = furnAbs1k[r];
    sc.npaths = 0;
    for (int i = 0; i < nspecs && sc.npaths < MAX_PATHS; ++i)
    {
        const PathSpec& p = specs[(size_t) i];
        if (p.npts < 2) continue;
        ScenePath& o = sc.paths[sc.npaths++];
        o.kind = p.kind; o.src = p.src; o.npts = p.npts;
        for (int k = 0; k < p.npts; ++k) o.pts[k] = p.pts[k];
        o.db = linToDb (p.gain) + p.bandDb[3];
        o.ms = p.length / SPEED_OF_SOUND * 1000.0f;
    }
    const RoomGeom* gg = geomStore.empty() ? nullptr : reinterpret_cast<const RoomGeom*> (geomStore.data());
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        sc.rt[r][0] = rooms[(size_t) r].rt60[1]; sc.rt[r][1] = rooms[(size_t) r].rt60[3];
        sc.rt[r][2] = rooms[(size_t) r].rt60[5]; sc.rt[r][3] = rooms[(size_t) r].rt60[6];
        sc.planN[r] = 0;
        if (gg != nullptr)
        {
            const int n = std::min (10, gg[r].np);
            for (int i = 0; i < n; ++i) { sc.planX[r][i] = gg[r].px[i]; sc.planY[r][i] = gg[r].py[i]; }
            sc.planN[r] = n;
        }
    }
    sc.inDb = 10.0f * std::log10 (std::max (inSq, 1e-12f));
    sc.outDb = 10.0f * std::log10 (std::max (outSq, 1e-12f));
    sc.drrDb = 10.0f * std::log10 (std::max (directSq, 1e-12f) / std::max (revSq, 1e-12f));
    sceneIdx.store (1 - sceneIdx.load());
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    process (L, R, nullptr, nullptr, L, R, n);
}

void Engine::process (const float* mainL, const float* mainR, const float* auxL, const float* auxR,
                      float* outL, float* outR, int n)
{
    int done = 0;
    while (done < n)
    {
        const int m = std::min (SUB_BLOCK, n - done);
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            float* mi = monoIn[s].data();
            const float* a = nullptr; const float* b = nullptr;
            switch (cur.src[s].input)
            {
                case IN_MAIN_L:  a = mainL; break;
                case IN_MAIN_R:  a = mainR; break;
                case IN_MAIN_LR: a = mainL; b = mainR; break;
                case IN_AUX_L:   a = auxL; break;
                case IN_AUX_R:   a = auxR; break;
                case IN_AUX_LR:  a = auxL; b = auxR; break;
                default: break;
            }
            if (a == nullptr) { std::fill (mi, mi + m, 0.0f); continue; }
            if (b == nullptr) for (int i = 0; i < m; ++i) mi[i] = a[done + i];
            else              for (int i = 0; i < m; ++i) mi[i] = 0.5f * (a[done + i] + b[done + i]);
        }
        renderSubBlock (m);
        for (int i = 0; i < m; ++i)
        {
            const float w = mixNow;
            const float dl = mainL[done + i], dr = mainR[done + i];
            outL[done + i] = trimOut * ((1.0f - w) * dl + w * wetL[(size_t) i]);
            outR[done + i] = trimOut * ((1.0f - w) * dr + w * wetR[(size_t) i]);
        }
        done += m;
    }
}

void Engine::renderSubBlock (int n)
{
    // ---- parameters: positions glide over ~25 ms, everything else per sub-block
    {
        const float k = 1.0f - std::exp (-(float) n / (0.025f * (float) fs));
        const float k1 = 1.0f - std::exp (-(float) n / (0.12f * (float) fs));   // walls move slowly
        auto slerpYaw = [k] (float a, float b) { float d = std::fmod (b - a + 540.0f, 360.0f) - 180.0f; return std::fmod (a + k * d + 720.0f, 360.0f); };
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            Vec3 sT = clampIntoRooms ({ target.src[s].x, target.src[s].y, target.src[s].z }, 0.15f);
            // a room change is a jump, not a glide through the wall
            if (roomOf (sT.x, sT.y) != roomOf (srcPos[s].x, srcPos[s].y) && ! throughOpenDoor (srcPos[s], sT)) srcPos[s] = sT; else srcPos[s] = srcPos[s] + (sT - srcPos[s]) * k;
            srcYaw[s] = slerpYaw (srcYaw[s], target.src[s].yaw);
            cur.src[s].type = target.src[s].type; cur.src[s].directivity = target.src[s].directivity;
            cur.src[s].input = target.src[s].input; cur.src[s].levelDb = target.src[s].levelDb;
            const float lt = target.src[s].active() ? dbToLin (target.src[s].levelDb) : 0.0f;
            srcLevel[s] += k * (lt - srcLevel[s]);
        }
        Vec3 lT = clampIntoRooms ({ target.lisX, target.lisY, EAR_HEIGHT }, 0.15f);
        if (roomOf (lT.x, lT.y) != roomOf (lisPos.x, lisPos.y) && ! throughOpenDoor (lisPos, lT)) lisPos = lT; else lisPos = lisPos + (lT - lisPos) * k;
        lisYaw = slerpYaw (lisYaw, target.lisYaw);
        for (int d = 0; d < NUM_DOORS; ++d) doorNow[d] += k * (target.door[d] - doorNow[d]);
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            cur.material[r] = target.material[r]; cur.floorMat[r] = target.floorMat[r]; cur.ceilMat[r] = target.ceilMat[r];
            if (r == 0) { cur.nfurn = target.nfurn; for (int i = 0; i < MAX_FURN; ++i) cur.furn[i] = target.furn[i]; }
            cur.panelArea[r] = target.panelArea[r];
            for (int k = 0; k < 2; ++k)
            {
                cur.breakAlong[r][k] += k1 * (target.breakAlong[r][k] - cur.breakAlong[r][k]);
                cur.breakPush[r][k]  += k1 * (target.breakPush[r][k]  - cur.breakPush[r][k]);
            }
        }
        trimDirect = dbToLin (target.directDb); trimEarly = dbToLin (target.earlyDb);
        trimReverb = dbToLin (target.reverbDb); trimOut += k * (dbToLin (target.outputDb) - trimOut);
        mixNow += k * (target.mix - mixNow);
    }
    updateRoomAcoustics (false);
    buildPaths();
    assignSlots();
    snapFades = false;

    // ---- the source lines
    float isq = 0;
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        const float* mi = monoIn[s].data();
        for (int i = 0; i < n; ++i) { source[s].write (mi[i]); isq += mi[i] * mi[i]; }
    }
    inSq += 0.2f * (isq / (float) n - inSq);

    std::fill (wetL.begin(), wetL.begin() + n, 0.0f);
    std::fill (wetR.begin(), wetR.begin() + n, 0.0f);

    // ---- late fields first (their feeds are read by the door paths)
    tickFields (n);
    updateLight (n);

    // ---- discrete paths
    float dsq = 0, rsq = 0;
    for (auto& s : slots)
    {
        if (! s.active) continue;
        float before = 0; for (int i = 0; i < n; ++i) before += wetL[(size_t) i] * wetL[(size_t) i] + wetR[(size_t) i] * wetR[(size_t) i];
        renderPath (s, n);
        float after = 0; for (int i = 0; i < n; ++i) after += wetL[(size_t) i] * wetL[(size_t) i] + wetR[(size_t) i] * wetR[(size_t) i];
        // the DRR meter: what arrives by the shortest route (line of sight, or through
        // the doorway / leaf / wall when there is none) against everything reflected
        const bool asDirect = s.kind == PathKind::Direct || s.kind == PathKind::Portal || s.kind == PathKind::Leaf || s.kind == PathKind::Wall;
        (asDirect ? dsq : rsq) += std::max (0.0f, after - before);
        if ((s.envTarget <= 0 && s.env <= 0) || (s.gainTarget <= 0 && s.gain < 1e-6f)) { s.active = false; s.env = 0; }
    }
    renderDiffuse (n);
    float osq = 0; for (int i = 0; i < n; ++i) osq += wetL[(size_t) i] * wetL[(size_t) i] + wetR[(size_t) i] * wetR[(size_t) i];
    const float rev = std::max (0.0f, osq - dsq);
    directSq += 0.1f * (dsq / (2.0f * n) - directSq);
    revSq += 0.1f * (rev / (2.0f * n) - revSq);
    outSq += 0.2f * (osq / (2.0f * n) - outSq);

    if (++sceneTick >= 3) { sceneTick = 0; publishScene(); }
}

//==============================================================================
void Engine::renderPath (PathSlot& p, int n)
{
    const DelayLine& feed = p.feed < MAX_SOURCES ? source[p.feed] : rooms[(size_t) (p.feed - MAX_SOURCES)].feed;
    const int maxItd = (int) (0.0045 * fs) + 2;

    // per-block smoothing of the slow things
    p.filt.approach (p.filtTarget, 0.5f);
    for (int k = 0; k < MAX_TAPS; ++k) { p.hL[(size_t) k] += 0.5f * (p.hLTarget[(size_t) k] - p.hL[(size_t) k]); p.hR[(size_t) k] += 0.5f * (p.hRTarget[(size_t) k] - p.hR[(size_t) k]); }
    p.gL += 0.5f * (p.gLTarget - p.gL); p.gR += 0.5f * (p.gRTarget - p.gR);

    // delay: slew at up to 1 % (a 3.4 m/s source), else crossfade the jump
    const float slewMax = 0.01f * (float) n;
    if (! p.crossing && std::abs (p.delayTarget - p.delay) > 4.0f * slewMax)
    {
        p.crossing = true; p.delayB = p.delayTarget; p.xfade = 0;
    }
    const float xstep = p.crossing ? 1.0f / (float) n : 0.0f;
    const float dstep = p.crossing ? 0.0f : std::max (-slewMax, std::min (slewMax, p.delayTarget - p.delay)) / (float) n;

    const float g0 = p.gain, g1 = p.gainTarget;
    const float fadeStep = (float) n / (PATH_FADE_S * (float) fs);
    const float e0 = p.env;
    const float e1 = p.envTarget > p.env ? std::min (p.envTarget, p.env + fadeStep) : std::max (p.envTarget, p.env - fadeStep);
    const float it0 = p.itd, it1 = p.itdTarget;
    const int maxDelay = feed.capacity() - FracDelay::TAPS - 4;

    // write the filtered mono into the history
    const int hw0 = p.hw;
    for (int i = 0; i < n; ++i)
    {
        p.delay += dstep;
        const float back = (float) (n - 1 - i);      // the whole block is already written
        float x = feed.read (std::min ((float) maxDelay, std::max ((float) FracDelay::MIN_DELAY, p.delay + back)));
        if (p.crossing)
        {
            const float xb = feed.read (std::min ((float) maxDelay, std::max ((float) FracDelay::MIN_DELAY, p.delayB + back)));
            p.xfade += xstep;
            x += (xb - x) * p.xfade;
        }
        const float fr = (float) (i + 1) / (float) n;
        const float g = (g0 + (g1 - g0) * fr) * (e0 + (e1 - e0) * fr);
        x = p.filt.process (x) * g;
        p.hist[(size_t) ((hw0 + i) & p.hmask)] = x;
    }
    p.hw = (hw0 + n) & p.hmask;
    p.gain = g1;
    p.env = e1;
    if (p.crossing && p.xfade >= 1.0f - 1e-6f) { p.crossing = false; p.delay = p.delayB; }

    // the convolution window: samples hw0 - off - ntapPad + 1 .. hw0 + n - 1, linear
    const int ntapPad = (ntap + 15) & ~15;
    // room for the sinc: HALF-1 newer taps at the largest ITD, HALF-1 older at
    // the smallest, and the constant pre-delay that keeps the newest tap behind
    // the write cursor
    const int off = maxItd + 2 * FracDelay::HALF;
    const int winLen = n + off + ntapPad;
    float* win = tmpA.data();
    int idx = (hw0 - off - ntapPad + 1) & p.hmask;
    for (int k = 0; k < winLen; ++k) { win[k] = p.hist[(size_t) idx]; idx = (idx + 1) & p.hmask; }

    // z[j] for j = -off .. n-1 -> zL[j + off]; z[j] = sum_k h[k] hist[j - k]
    for (int j = 0; j < n + off; ++j)
    {
        p.zL[(size_t) j] = dot (p.hL.data(), win + j, ntapPad);
        p.zR[(size_t) j] = dot (p.hR.data(), win + j, ntapPad);
    }

    /*  Ears: the later ear delayed by the ITD, ramped through the block. Both
        ears carry a constant HALF-sample pre-delay - the same on every path, so
        no cue moves - which is what lets the interpolation kernel look "forward"
        without reading past the newest sample written. */
    for (int i = 0; i < n; ++i)
    {
        const float it = it0 + (it1 - it0) * (float) (i + 1) / (float) n;
        const float dL = (float) FracDelay::HALF + (it > 0 ? 0.0f : -it);
        const float dR = (float) FracDelay::HALF + (it > 0 ? it : 0.0f);
        const int iL = (int) dL, iR = (int) dR;
        const float fL = dL - (float) iL, fR = dR - (float) iR;
        wetL[(size_t) i] += p.gL * DelayLine::interp (p.zL.data(), i - iL + off, fL);
        wetR[(size_t) i] += p.gR * DelayLine::interp (p.zR.data(), i - iR + off, fR);
    }
    p.itd = it1;
}

//==============================================================================
void Engine::tickFields (int n)
{
    const float phaseInc = 2.0f * PI / (float) fs;
    const float kw = 1.0f - std::exp (-1.0f / (0.03f * (float) fs));
    const float wk = 1.0f - std::exp (-(float) n / (0.05f * (float) fs));
    for (int r = 0; r < NUM_ROOMS; ++r) rooms[(size_t) r].weight += wk * (rooms[(size_t) r].weightTarget - rooms[(size_t) r].weight);
    int srcRoom[MAX_SOURCES];
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        srcRoom[s] = cur.src[s].active() ? std::max (0, roomOf (srcPos[s].x, srcPos[s].y)) : -1;
        srcInGainNow[s] += 0.5f * (srcInGain[s] - srcInGainNow[s]);
    }

    for (int i = 0; i < n; ++i)
    {
        // 1. every room's line outputs, losses, observed sum
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            RoomField& F = rooms[(size_t) r];
            float y = 0;
            for (int k = 0; k < RoomField::N; ++k)
            {
                /*  Modulated, and read with the sinc. The old LINEAR read here
                    cost 3-5 dB of the field's energy per pass, which is why the
                    modulation was taken out; with a lossless reader it is free,
                    and it is what keeps the network's own modes from standing
                    still and being heard as pitch. */
                F.modPhase[k] += phaseInc * F.modRate[k];
                if (F.modPhase[k] > 2.0f * PI) F.modPhase[k] -= 2.0f * PI;
                const float d = (float) (F.len[(size_t) k] - 1) + F.modDepth * std::sin (F.modPhase[k]);
                float v = F.line[(size_t) k].read (d);
                v = F.loss[(size_t) k].process (v);
                // the three allpasses inside this line
                for (int q = 0; q < RoomField::NAP; ++q)
                {
                    const size_t idx = (size_t) (k * RoomField::NAP + q);
                    if (F.apLen[idx] <= 0) continue;
                    const float b = F.ap[idx][(size_t) F.apW[idx]];
                    const float yy = -F.apG * v + b;
                    F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * yy;
                    if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                    v = yy;
                }
                F.out[(size_t) k] = v; y += SGN_OUT[k] * v;
            }
            F.y = y * 0.25f;
            fieldAcc[r] += (double) F.y * F.y;
            F.feed.write (F.y);
            dbgFieldEnergy[r] += (double) F.y * F.y;
        }
        // 2. injections: each source into its room (through the diffusers), neighbours through the doors
        float inj[NUM_ROOMS] = { 0, 0, 0 };
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            const float x = monoIn[s][(size_t) i];
            srcPre[s].write (x);
            bool any = false;
            for (int r = 0; r < NUM_ROOMS; ++r)
            {
                srcW[s][r] += kw * (((r == srcRoom[s]) ? 1.0f : 0.0f) - srcW[s][r]);
                if (srcW[s][r] > 1e-4f) any = true;
            }
            if (! any) continue;
            const int rd = srcRoom[s] >= 0 ? srcRoom[s] : 0;
            const float v = srcInFilt[s].process (srcPre[s].readInt (rooms[(size_t) rd].preDelay)) * srcInGainNow[s] * srcLevel[s];
            for (int r = 0; r < NUM_ROOMS; ++r) if (srcW[s][r] > 1e-4f) inj[r] += v * srcW[s][r];
        }
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            RoomField& F = rooms[(size_t) r];
            float sIn = inj[r];
            // four Schroeder allpasses smear the injection before it enters the
            // tank: the hall's first 50 ms measured at half the echo density it
            // needed, which is a sparse injection, not a sparse loop
            static const float inG[RoomField::NIN] = { 0.62f, 0.58f, 0.55f, 0.52f };
            for (int q = 0; q < RoomField::NIN; ++q)
            {
                const float b = F.inAp[(size_t) q][(size_t) F.inApW[(size_t) q]];
                const float y = -inG[q] * sIn + b;
                F.inAp[(size_t) q][(size_t) F.inApW[(size_t) q]] = sIn + inG[q] * y;
                if (++F.inApW[(size_t) q] >= F.inApLen[(size_t) q]) F.inApW[(size_t) q] = 0;
                sIn = y;
            }
            inj[r] = sIn;
        }
        for (auto& c : couplings)
            inj[c.to] += c.filt.process (rooms[(size_t) c.from].y) * c.gain;
        // 3. mix (sign-flipped Walsh-Hadamard, orthonormal) and write back with the injections
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            RoomField& F = rooms[(size_t) r];
            float v[16]; for (int k = 0; k < 16; ++k) v[k] = F.out[(size_t) k];
            for (int len = 1; len < 16; len <<= 1)
                for (int a = 0; a < 16; a += len << 1)
                    for (int b = a; b < a + len; ++b) { const float p = v[b], q = v[b + len]; v[b] = p + q; v[b + len] = p - q; }
            const float srcIn = inj[r];
            dbgInjEnergy[r] += (double) srcIn * srcIn;
            for (int k = 0; k < 16; ++k)
                F.line[(size_t) k].write (0.25f * SGN_MIX[k] * v[k] + 0.25f * SGN_SRC[r][k] * srcIn);
        }
    }
}

void Engine::renderDiffuse (int n)
{
    const int ntapPad = MAX_TAPS;
    const float norm = trimReverb / std::sqrt (16.0f);       // (l_a + l_b)/sqrt2 per direction, /sqrt8 over eight
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        RoomField& F = rooms[(size_t) r];
        if (F.weight < 1e-4f) continue;
        for (int j = 0; j < 8; ++j)
        {
            std::vector<float>& h = F.dhist[(size_t) j];
            int w = F.dw[(size_t) j];
            for (int i = 0; i < n; ++i)
            {
                // two of the sixteen lines, from the line buffers, this sub-block
                const float a = F.line[(size_t) (2 * j)].readInt (n - 1 - i);
                const float b = F.line[(size_t) (2 * j + 1)].readInt (n - 1 - i);
                h[(size_t) w] = (a + b) * norm * F.weight;
                w = (w + 1) & F.dmask;
            }
            const int winLen = n + ntapPad;
            float* win = tmpB.data();
            int idx = (w - n - ntapPad + 1) & F.dmask;
            for (int k = 0; k < winLen; ++k) { win[k] = h[(size_t) idx]; idx = (idx + 1) & F.dmask; }
            for (int i = 0; i < n; ++i)
            {
                wetL[(size_t) i] += dot (F.dL[(size_t) j].data(), win + i, ntapPad);
                wetR[(size_t) i] += dot (F.dR[(size_t) j].data(), win + i, ntapPad);
            }
            F.dw[(size_t) j] = w;
        }
    }
}

} // namespace tw
