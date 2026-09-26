#include "ho_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#if defined(__SSE2__) || defined(_M_X64)
 #include <emmintrin.h>
 #define HO_SSE 1
#endif

namespace ho
{

namespace
{
    constexpr double PI  = 3.14159265358979323846;
    constexpr float  LN60 = 6.9077553f;

    inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
    inline float lerpf  (float a, float b, float t)   { return a + (b - a) * t; }
    inline float db2lin (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float smooth01 (float x) { x = clampf (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }

    inline float sin2pi (double x)
    {
        double r = x - std::floor (x + 0.5);
        if (r > 0.25)       r = 0.5 - r;
        else if (r < -0.25) r = -0.5 - r;
        const double y = 2.0 * PI * r, y2 = y * y;
        return (float) (y * (1.0 + y2 * (-1.0 / 6.0 + y2 * (1.0 / 120.0 + y2 * (-1.0 / 5040.0
                         + y2 * (1.0 / 362880.0 + y2 * (-1.0 / 39916800.0)))))));
    }

    inline float ceilSoft (float x, float lim = 1.0f, float knee = 0.90f)
    {
        const float t = knee * lim;
        const float a = std::abs (x);
        if (a <= t) return x;
        const float over = (a - t) / (lim - t);
        const float y = t + (lim - t) * std::tanh (over);
        return x < 0.0f ? -y : y;
    }

    inline float wavefold (float x)
    {
        float t = std::fmod (x + 1.0f, 4.0f);
        if (t < 0.0f) t += 4.0f;
        t -= 1.0f;
        return t > 1.0f ? 2.0f - t : t;
    }

    //  polyBLEP: the 808's squares at 4x, band-limited
    inline float blep (double t, double dt)
    {
        if (t < dt)        { const double x = t / dt;         return (float) (x + x - x * x - 1.0); }
        if (t > 1.0 - dt)  { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
        return 0.0f;
    }

    struct DriveDef { const char* name; float driveMax; float character; };
    constexpr DriveDef kDrive[NUM_DRIVE_ENGINES] =
    {
        { "IDLE BURN",  10.0f, 0.55f },
        { "HYPERDRIVE",  8.0f, 0.45f },
        { "RAZOR WING", 24.0f, 0.70f },
        { "SUPERNOVA",  14.0f, 0.50f },
    };

    inline double logcosh (double u) { u = std::abs (u); return u + std::log1p (std::exp (-2.0 * u)) - 0.6931471805599453; }

    constexpr int TRIM_PTS = 17;
    struct TrimTable
    {
        float t[NUM_DRIVE_ENGINES][TRIM_PTS];
        //  measured, never typed; a cymbal lives high, so the reference is 2 kHz
        TrimTable()
        {
            const float REF[3] = { 0.20f, 0.45f, 0.80f };
            for (int e = 0; e < NUM_DRIVE_ENGINES; ++e)
                for (int d = 0; d < TRIM_PTS; ++d)
                {
                    const float drive = (float) d / (TRIM_PTS - 1);
                    const float k = 1.0f + drive * kDrive[e].driveMax;
                    double logSum = 0.0;
                    for (float a : REF)
                    {
                        double acc = 0.0;
                        const int N = 4800;
                        for (int n = 0; n < N; ++n)
                        {
                            const float x = a * sin2pi (2000.0 * n / 48000.0);
                            const float y = driveShape (e, x, k);
                            acc += (double) y * y;
                        }
                        const double r = std::sqrt (acc / N);
                        logSum += std::log (std::max (1.0e-4, a * 0.70710678 / std::max (1.0e-6, r)));
                    }
                    t[e][d] = clampf ((float) std::exp (logSum / 3.0), 0.02f, 8.0f);
                }
        }
    };
    const TrimTable& trims() { static const TrimTable tt; return tt; }

    double antiderivative (int e, double u, double a)
    {
        switch (e)
        {
            case ENG_RAZOR:
                return (1.0 - a) * logcosh (u) + a * (std::abs (u) <= 1.0 ? 0.5 * u * u : std::abs (u) - 0.5);
            case ENG_IDLE:
            {
                const double s = 0.6 + 0.4 * a;
                const double ft = u >= 0.0 ? logcosh (u) : s / a * logcosh (a * u);
                const double fc = std::abs (u) <= 1.0 ? 1.5 * (0.5 * u * u - u * u * u * u / 12.0)
                                                      : 0.625 + std::abs (u) - 1.0;
                return 0.3 * fc + 0.7 * ft;
            }
            case ENG_HYPER:
            {
                const double s = a * 3.0;
                const double w2 = std::max (0.0, 1.0 - std::abs (s)), w3 = std::max (0.0, 1.0 - std::abs (s - 1.0));
                const double w4 = std::max (0.0, 1.0 - std::abs (s - 2.0)), w5 = std::max (0.0, 1.0 - std::abs (s - 3.0));
                const double g = 0.9 / (w2 + w3 + w4 + w5 + 1.0e-6);
                auto P = [&] (double c)
                {
                    const double c2 = c * c, c3 = c2 * c, c4 = c2 * c2;
                    return 0.175 * c2 + g * (w2 * (2.0 / 3.0) * c3 + w3 * (c4 - 1.5 * c2)
                                             + w4 * (1.6 * c4 * c - (8.0 / 3.0) * c3)
                                             + w5 * ((8.0 / 3.0) * c4 * c2 - 5.0 * c4 + 2.5 * c2));
                };
                auto p = [&] (double c)
                {
                    const double c2 = c * c;
                    return 0.35 * c + g * (w2 * 2.0 * c2 + w3 * c * (4.0 * c2 - 3.0) + w4 * (8.0 * c2 * c2 - 8.0 * c2)
                                           + w5 * c * (16.0 * c2 * c2 - 20.0 * c2 + 5.0));
                };
                if (u > 1.0)  return P (1.0) + p (1.0) * (u - 1.0);
                if (u < -1.0) return P (-1.0) + p (-1.0) * (u + 1.0);
                return P (u);
            }
            default: return 0.0;
        }
    }

    inline float xorshift (uint32_t& r)
    {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        return (float) (int32_t) r * (1.0f / 2147483648.0f);
    }
    inline float uni (uint32_t& r) { return 0.5f + 0.5f * xorshift (r); }

    //  the bell: a stiff dome, a few strong, nearly tonal partials
    constexpr int kDome = 8;
    constexpr float kDomeRatio[kDome] = { 1.0f, 1.99f, 2.71f, 3.92f, 5.08f, 6.35f, 7.6f, 9.1f };

    //  the cymbal is the same cymbal every hit: its modes come from one fixed seed
    constexpr uint32_t kCymbalSeed = 0x5EED1234u;

    struct Plate
    {
        float f[kMaxModes], amp[kMaxModes], pan[kMaxModes], sgn[kMaxModes];
        bool  dome[kMaxModes];
        int   n = 0, nPlate = 0;
        float fLo = 200.0f, top = 17000.0f;
    };

    int plateCount (float density) { return 10 + (int) std::lround (54.0f * clampf (density, 0.0f, 1.0f)); }

    float lowestOf (const Params& p, float keyMul)
    {
        return 2900.0f / clampf (p.size, 8.0f, 24.0f) * std::exp2 (p.pitch / 12.0f) * keyMul;
    }

    //  the plate's modes for these settings: sparse at the bottom, dense at the
    //  top, the upper ones in near-degenerate pairs that beat (the shimmer)
    void buildPlate (Plate& pl, const Params& p, float keyMul, float strike, float bright, float o, double fo)
    {
        const float pm = std::exp2 (p.pitch / 12.0f) * keyMul;
        pl.fLo = lowestOf (p, keyMul);
        pl.top = std::min (19000.0f, std::min (0.45f * (float) fo, 17000.0f * std::pow (pm, 0.3f)));
        pl.top = std::max (pl.top, pl.fLo * 4.0f);
        const int N = plateCount (p.density);
        const float trash = clampf (p.trash, 0.0f, 1.0f);
        uint32_t r = kCymbalSeed;
        const float span = std::log (pl.top / pl.fLo);
        for (int i = 0; i < N; ++i)
        {
            const float ru = uni (r), ra = uni (r), rp = uni (r), rt = uni (r), rs = uni (r);
            float f;
            if (i == 0) f = pl.fLo;
            else if (i >= 4 && (i & 1)) f = pl.f[i - 1] * (1.0f + 0.0007f + 0.0025f * ru);
            else
            {
                const float u = std::pow (((float) i + 0.3f + 0.6f * ru) / (float) N, 0.8f);
                f = pl.fLo * std::exp (u * span);
            }
            if (i > 0) f *= 1.0f + trash * 0.02f * (rt - 0.5f);
            pl.f[i] = std::min (f, pl.top);
            pl.amp[i] = 0.55f + 0.9f * ra;
            //  left and right alternate, so each beating pair is split across the
            //  stereo field (the shimmer moves), and the low modes stay near the middle
            pl.pan[i] = ((i & 1) ? -1.0f : 1.0f) * (0.6f + 0.4f * rp) * std::min (1.0f, (f / pl.fLo - 1.0f) / 0.3f);
            pl.dome[i] = false;
            //  a mode shape is + or - at the stick and at the ear: the modes start
            //  at rest either way, but with mixed signs they do not all rise together
            //  (all in phase, their first swing added coherently and pinned the
            //  output to the ceiling for the first milliseconds)
            pl.sgn[i] = rs < 0.5f ? -1.0f : 1.0f;
        }
        pl.nPlate = N;
        //  the bell leans low, the edge flat; closing a hi-hat clamps the big, low
        //  bending modes far harder than the small high ones, so a closed hat is top
        const float tilt = clampf (1.3f - 1.6f * strike - 0.8f * bright - 0.9f * (1.0f - o), -0.6f, 1.4f);
        const float bell = (1.0f - strike) * (1.0f - strike);
        for (int i = 0; i < N; ++i)
            pl.amp[i] *= std::pow (pl.f[i] / pl.fLo, -tilt) * (1.0f - 0.55f * bell);
        int n = N;
        for (int d = 0; d < kDome && bell > 0.0f && n < kMaxModes; ++d)
        {
            const float f = pl.fLo * 3.1f * kDomeRatio[d];
            if (f >= pl.top) break;
            pl.f[n] = f;
            pl.amp[n] = bell * 1.6f / std::sqrt (kDomeRatio[d]);
            pl.pan[n] = 0.0f;
            pl.dome[n] = true;
            pl.sgn[n] = (d & 1) ? -1.0f : 1.0f;
            ++n;
        }
        pl.n = n;
    }

    inline float openDamp (float o) { return 0.025f + 0.975f * o * o; }
    inline float rattleOf (float o) { return 4.0f * o * (1.0f - o) + 0.8f * o * o * o * o; }
}

const float k808Hz[kSquares] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

//==============================================================================
float driveADAA (int engine, float x, double xPrev, float k)
{
    if (engine == ENG_NOVA) return driveShape (engine, x, k);
    const double dx = (double) x - xPrev;
    if (std::abs (dx) < 1.0e-6)
        return driveShape (engine, (float) (0.5 * ((double) x + xPrev)), k);
    const double a = kDrive[engine].character;
    return (float) ((antiderivative (engine, (double) k * x, a) - antiderivative (engine, (double) k * xPrev, a)) / ((double) k * dx));
}

const char* driveEngineName (int e) { return kDrive[std::clamp (e, 0, NUM_DRIVE_ENGINES - 1)].name; }

float Engine::driveTrimFor (int engine, float drive)
{
    const auto& T = trims();
    const int e = std::clamp (engine, 0, NUM_DRIVE_ENGINES - 1);
    const float x = clampf (drive, 0.0f, 1.0f) * (TRIM_PTS - 1);
    const int i = std::min ((int) x, TRIM_PTS - 2);
    return lerpf (T.t[e][i], T.t[e][i + 1], x - (float) i);
}

float driveShape (int engine, float x, float k)
{
    const float u = x * k;
    switch (engine)
    {
        case ENG_IDLE:
        {
            const float a = kDrive[ENG_IDLE].character;
            const float t = u >= 0.0f ? std::tanh (u) : std::tanh (u * a) * (0.6f + 0.4f * a);
            const float c = clampf (u, -1.0f, 1.0f);
            const float cub = 1.5f * (c - c * c * c / 3.0f);
            return lerpf (cub, t, 0.7f);
        }
        case ENG_HYPER:
        {
            const float a = kDrive[ENG_HYPER].character;
            const float c = clampf (u, -1.0f, 1.0f), c2 = c * c;
            const float t2 = 2.0f * c2;
            const float t3 = c * (4.0f * c2 - 3.0f);
            const float t4 = 8.0f * c2 * c2 - 8.0f * c2;
            const float t5 = c * (16.0f * c2 * c2 - 20.0f * c2 + 5.0f);
            const float s  = a * 3.0f;
            const float w2 = std::max (0.0f, 1.0f - std::abs (s));
            const float w3 = std::max (0.0f, 1.0f - std::abs (s - 1.0f));
            const float w4 = std::max (0.0f, 1.0f - std::abs (s - 2.0f));
            const float w5 = std::max (0.0f, 1.0f - std::abs (s - 3.0f));
            const float nn = w2 + w3 + w4 + w5 + 1.0e-6f;
            return c * 0.35f + 0.9f * (w2 * t2 + w3 * t3 + w4 * t4 + w5 * t5) / nn;
        }
        case ENG_RAZOR:
            return lerpf (std::tanh (u), clampf (u, -1.0f, 1.0f), kDrive[ENG_RAZOR].character);
        case ENG_NOVA:
        {
            const float a = kDrive[ENG_NOVA].character;
            float y = wavefold (u);
            y = std::floor (y * 8.0f + 0.5f) * 0.125f;
            y = clampf (y * 1.6f, -1.0f, 1.0f);
            return y * (1.0f - a * 0.5f) + a * 0.5f * std::sin (11.0f * y);
        }
        default: break;
    }
    return x;
}

int Engine::articFor (int keys, int note)
{
    if (keys != KEYS_GM) return ART_DIALLED;
    switch (note)
    {
        case 42: return ART_CLOSED;
        case 44: return ART_PEDAL;
        case 46: return ART_OPEN;
        case 53: return ART_BELL;
        case 49: case 52: case 55: case 57: return ART_EDGE;
        default: return ART_DIALLED;
    }
}

float Engine::lowestModeHz (const Params& p) { return lowestOf (p, 1.0f); }
int   Engine::modeCount (const Params& p)    { return plateCount (p.density); }

int Engine::voicesOn() const
{
    int n = 0;
    for (auto& v : voices) if (v.on) ++n;
    return n;
}

//==============================================================================
void HalfBand::design()
{
    constexpr int C = TAPS / 2;
    std::array<double, TAPS> g {};
    double sum = 0.0;
    for (int n = 0; n < TAPS; ++n)
    {
        const double t = n - C;
        const double s = t == 0.0 ? 0.5 : std::sin (PI * 0.5 * t) / (PI * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * PI * n / (TAPS - 1))
                              + 0.08 * std::cos (4.0 * PI * n / (TAPS - 1));
        g[(size_t) n] = s * w;
        sum += g[(size_t) n];
    }
    for (auto& v : g) v /= sum;
    for (int k = 0; k < PH; ++k)
    {
        ge[(size_t) k] = 2 * k     < TAPS ? (float) g[(size_t) (2 * k)]     : 0.0f;
        go[(size_t) k] = 2 * k + 1 < TAPS ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
    }
}

void Svf::set (float fc, float fsr, float q)
{
    fc = clampf (fc, 5.0f, 0.49f * fsr);
    g = (float) std::tan (PI * fc / fsr);
    k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

//==============================================================================
void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
    fo = fs * kOS;
    hb1L.design(); hb2L.design(); hb1R.design(); hb2R.design(); dnL.design(); dnR.design();
    upL.design (dnL); upR.design (dnR);
    (void) trims();

    const double ms = fo * 0.001;
    const float roomMs[kLines] = { 13.1f, 17.3f, 19.7f, 23.9f, 29.3f, 31.7f, 37.1f, 41.9f };
    for (int i = 0; i < kLines; ++i)
    {
        len[(size_t) i] = std::max (8, (int) std::lround (roomMs[i] * ms));
        line[(size_t) i].assign ((size_t) len[(size_t) i], 0.0f);
    }
    const float apMs[2] = { 4.7f, 1.9f };
    for (int i = 0; i < 2; ++i)
    {
        apLen[(size_t) i] = std::max (4, (int) std::lround (apMs[i] * ms));
        ap[(size_t) i].assign ((size_t) apLen[(size_t) i], 0.0f);
    }

    auto co = [&] (double msT) { return (float) (1.0 - std::exp (-1.0 / (msT * ms))); };
    aFA = co (0.1); aSA = co (6.0); aRelA = co (12.0);
    aFR = co (5.0); aSR = co (150.0);

    laN = std::max (1, (int) std::lround (0.0015 * fs));
    laL.assign ((size_t) laN, 0.0f); laR.assign ((size_t) laN, 0.0f);
    holdN = laN + std::max (1, (int) std::lround (0.006 * fs));
    int dq = 1; while (dq < holdN + 4) dq <<= 1;
    dqVal.assign ((size_t) dq, 0.0f);
    dqIdx.assign ((size_t) dq, 0);
    dqMask = dq - 1;

    eLen = (int) std::ceil (3.2 * fs) + 8;
    for (auto& b : eBuf) b.assign ((size_t) eLen, 0.0f);

    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice();
    hitSeed = 0;
    cutL.clear(); cutR.clear(); airL.clear(); airR.clear();
    gritPh = 1.0; gritL = gritR = 0.0f;
    tFA = tSA = tFR = tSR = 0.0f;
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
    pos.fill (0); apPos.fill (0); dampZ.fill (0.0f);
    roomLive = false;
    driveSm = 0.0f; colourSm = 1.0f; roomSm = 0.0f;
    adaaL = adaaR = 0.0;
    colL.clear(); colR.clear(); coefTick = 0;
    hb1L.clear(); hb2L.clear(); hb1R.clear(); hb2R.clear(); dnL.clear(); dnR.clear();
    upL.clear(); upR.clear(); padL = padR = 0.0f;
    std::fill (laL.begin(), laL.end(), 0.0f); std::fill (laR.begin(), laR.end(), 0.0f); laPos = 0;
    dqHead = dqTail = cTick = 0;
    compG = 0.0f;
    for (auto& b : eBuf) std::fill (b.begin(), b.end(), 0.0f);
    eW = 0; wowPh = 0.0; echoSm = 0.0f; echoLive = false;
    eHpZ[0] = eHpZ[1] = eLpZ[0] = eLpZ[1] = 0.0f;
    fresh = true; levelSm = 1.0f;
    numPend = 0;
    mPeak.store (0.0f); mGr.store (0.0f);
}

void Engine::noteOn (int note, float velocity01)
{
    if (numPend < (int) pend.size())
        pend[(size_t) numPend++] = { -1, note, clampf (velocity01, 0.0f, 1.0f), -1.0f };
}

void Engine::noteOnArtic (int artic, float velocity01, float strike)
{
    if (numPend < (int) pend.size())
        pend[(size_t) numPend++] = { std::clamp (artic, 0, NUM_ARTICS - 1), 42, clampf (velocity01, 0.0f, 1.0f), strike };
}

void Engine::setTempo (double b)
{
    if (b > 1.0) bpm = std::clamp (b, 40.0, 300.0);
}

//==============================================================================
void Engine::startVoice (int artic, int note, float vel, float strikeOverride)
{
    // ---- what the articulation asks for ------------------------------------
    float o = clampf (P.open, 0.0f, 1.0f);
    float strike = strikeOverride >= 0.0f ? clampf (strikeOverride, 0.0f, 1.0f) : clampf (P.strike, 0.0f, 1.0f);
    float body = 1.0f, stickAmt = clampf (P.stick, 0.0f, 1.0f), chickAmt = 0.0f;
    switch (artic)
    {
        case ART_CLOSED: o = 0.0f; break;
        case ART_PEDAL:  o = 0.0f; body = 0.25f; stickAmt = 0.0f; chickAmt = clampf (P.chick, 0.0f, 1.0f); break;
        case ART_OPEN:   o = std::max (o, 0.8f); break;
        case ART_BELL:   strike = 0.0f; break;
        case ART_EDGE:   strike = 1.0f; vel = std::min (1.0f, vel * 1.1f); break;
        default: break;
    }
    const bool closedHit = o < 0.3f;

    // ---- a closed hat chokes what is ringing: the foot closes the plates ----
    if (closedHit && P.choke > 0.5f)
        for (auto& v : voices)
            if (v.on && v.chokeStep == 0.0f) v.chokeStep = (float) (1.0 / (0.012 * fo));

    // ---- a slot: six heard; the oldest of those fades over 3 ms -------------
    int audible = 0;
    Voice* oldest = nullptr;
    for (auto& v : voices)
        if (v.on && v.chokeStep == 0.0f)
        {
            ++audible;
            if (oldest == nullptr || v.t > oldest->t) oldest = &v;
        }
    if (audible >= kAudible && oldest != nullptr)
        oldest->chokeStep = (float) (1.0 / (0.003 * fo));
    Voice* slot = nullptr;
    for (auto& v : voices) if (! v.on) { slot = &v; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.choke < slot->choke) slot = &v;
    }

    Voice& v = *slot;
    v = Voice();
    v.on = true;
    v.artic = artic;
    v.isHatClosed = closedHit;
    v.rng = 0x9E3779B9u * (++hitSeed) ^ 0x2545F491u;
    if (v.rng == 0) v.rng = 1;

    const float V = clampf (P.velo, 0.0f, 1.0f);
    v.velGain = (1.0f - V) + V * std::pow (vel, 1.2f);
    const float bright = V * (vel - 0.7f);
    const float keyMul = (int) std::lround (P.keys) == KEYS_CHROMATIC ? (float) std::pow (2.0, (note - 42) / 12.0) : 1.0f;
    const float dm = openDamp (o);
    const float decayS = std::max (0.01f, P.decay * 0.001f) * dm;

    v.bronzeAmt = clampf (P.bronze, 0.0f, 1.0f);
    v.circuitAmt = 1.0f - v.bronzeAmt;
    v.noiseAmt = clampf (P.noise, 0.0f, 1.0f);
    v.trash = clampf (P.trash, 0.0f, 1.0f);
    v.bodyAmt = body;

    double longest = decayS;
    v.nWash = 0;

    // ---- the bronze plate -------------------------------------------------------
    if (v.bronzeAmt > 0.0f)
    {
        Plate pl;
        buildPlate (pl, P, keyMul, strike, bright, o, fo);
        //  energy normalised: the same loudness whatever DENSITY or STRIKE is
        double e = 0.0;
        for (int k = 0; k < pl.n; ++k) e += (double) pl.amp[k] * pl.amp[k];
        const float norm = (float) (0.9 / std::sqrt (std::max (1.0e-9, e)));
        const float bloom = clampf (P.bloom, 0.0f, 1.0f);
        const float w = clampf (P.width, 0.0f, 1.0f);
        const float trashDecay = 1.0f - 0.35f * v.trash;
        for (int k = 0; k < pl.n; ++k)
        {
            const float f = pl.f[k];
            //  higher modes die faster on a free cymbal; clamped, the lows go first
            float t60 = decayS * std::pow (1000.0f / f, 0.35f - 0.45f * (1.0f - o)) * trashDecay;
            if (pl.dome[k]) t60 *= 1.5f;
            t60 = clampf (t60, 0.006f, 30.0f);
            longest = std::max (longest, (double) t60);
            const double rad = std::exp (-(double) LN60 / (t60 * fo));
            const double th = 2.0 * PI * f / fo;
            v.cr[k] = (float) (rad * std::cos (th));
            v.ci[k] = (float) (rad * std::sin (th));
            //  every hit a little different, as a real cymbal is: where the stick
            //  lands moves each mode's amplitude. NOT its phase - a struck plate
            //  starts at rest, so every mode starts as a sine; a random phase is a
            //  step on the first sample, a click louder than the stick
            //  (measured: it hid STICK completely)
            float a = pl.amp[k] * norm;
            float ph = 0.0f;
            if (! benchSteady) a *= 1.0f + 0.7f * (uni (v.rng) - 0.5f);
            v.zr[k] = pl.sgn[k] * a * std::cos (ph);
            v.zi[k] = pl.sgn[k] * a * std::sin (ph);
            //  BLOOM: energy arrives in the high modes after the hit
            const float fr = pl.dome[k] ? 0.0f : f / pl.top;
            v.g[k] = 1.0f - bloom * 0.97f * std::pow (fr, 0.4f);
            const double ta = 0.003 + bloom * 0.06 * (double) fr;     // the top modes peak near 60 ms
            v.ga[k] = (float) (1.0 - std::exp (-1.0 / (ta * fo)));
            if (w <= 0.0f) { v.pl[k] = 1.0f; v.pr[k] = 1.0f; }
            else
            {
                const float pp = w * pl.pan[k];
                v.pl[k] = std::cos ((pp + 1.0f) * 0.25f * (float) PI) * 1.41421356f;
                v.pr[k] = std::sin ((pp + 1.0f) * 0.25f * (float) PI) * 1.41421356f;
            }
        }
        //  pad to a multiple of four with silent modes, for the SIMD loop
        int n = pl.n;
        while (n % 4) { v.zr[n] = v.zi[n] = v.cr[n] = v.ci[n] = 0.0f; v.g[n] = 0.0f; v.ga[n] = 0.0f; v.pl[n] = v.pr[n] = 0.0f; ++n; }
        v.nModes = n;
        v.low0 = std::max (1.0e-6f, pl.amp[0] * norm);

        //  THE WASH. Above a few kHz a real cymbal's modes are packed too densely,
        //  and coupled too strongly by the plate's nonlinearity, for anyone to hear
        //  them as lines: sixty sines draw a comb, a crash is a continuum. So the top
        //  is eight noise bands that obey the plate's own laws - the same decay by
        //  frequency, the same strike tilt, the same damping as the hat closes, the
        //  same bloom - and DENSITY brings them in.
        const float dens = clampf (P.density, 0.0f, 1.0f);
        const float washRms = 0.64f * std::pow (dens, 1.5f);
        if (washRms > 1.0e-4f)
        {
            const float tilt = clampf (1.3f - 1.6f * strike - 0.8f * bright - 0.9f * (1.0f - o), -0.6f, 1.4f);
            const float wLo = std::max (1500.0f, 5.0f * pl.fLo), wHi = std::min (17000.0f, 0.42f * (float) fo);
            const float ratio = std::pow (wHi / wLo, 1.0f / (kWash - 1));
            float e = 0.0f, amp[kWash];
            for (int b = 0; b < kWash; ++b) { const float fc = wLo * std::pow (ratio, (float) b); amp[b] = std::pow (fc / 1000.0f, -0.6f * tilt); e += amp[b] * amp[b]; }
            for (int b = 0; b < kWash; ++b)
            {
                const float fc = wLo * std::pow (ratio, (float) b), q = 1.6f;
                v.wbL[b].set (fc, (float) fo, q); v.wbR[b].set (fc, (float) fo, q);
                //  bpNorm of uniform white noise: its rms follows the bandwidth
                const float bwRms = std::sqrt ((1.0f / 3.0f) * (0.5f * (float) PI * fc / q) / (0.5f * (float) fo));
                v.wAmp[b] = washRms * amp[b] / std::sqrt (e) / bwRms;
                float t60 = decayS * std::pow (1000.0f / fc, 0.35f - 0.45f * (1.0f - o)) * trashDecay * 0.9f;
                t60 = clampf (t60, 0.006f, 30.0f);
                longest = std::max (longest, (double) t60);
                v.wK[b] = std::exp (-(double) LN60 / (t60 * fo));
                v.wE[b] = 1.0;
                const float fr = fc / pl.top;
                //  the continuum is the plate's nonlinearity passing energy upwards,
                //  and that takes a couple of milliseconds even without BLOOM - which
                //  is also what leaves room for the stick
                v.wG[b] = 0.3f * (1.0f - bloom * 0.97f * std::pow (fr, 0.4f));
                v.wGa[b] = (float) (1.0 - std::exp (-1.0 / ((0.0025 + bloom * 0.06 * (double) fr) * fo)));
            }
            v.nWash = kWash;
            const float th = w * 0.5f * (float) PI;
            v.wCos = w <= 0.0f ? 1.0f : std::cos (th);
            v.wSin = w <= 0.0f ? 0.0f : std::sin (th);
        }
    }

    // ---- the circuit --------------------------------------------------------------
    if (v.circuitAmt > 0.0f)
    {
        const float pm = std::exp2 (P.pitch / 12.0f) * keyMul;
        const float spread = 0.7f + 0.6f * clampf (P.density, 0.0f, 1.0f);
        for (int i = 0; i < kSquares; ++i)
        {
            const float ratio = k808Hz[i] / k808Hz[0];
            const float f = k808Hz[0] * (1.0f + (ratio - 1.0f) * spread) * pm;
            v.sqInc[i] = (float) (f / fo);
            v.sqPh[i] = 0.0;
        }
        const float fc = 7100.0f * std::pow (14.0f / clampf (P.size, 8.0f, 24.0f), 0.7f) * pm;
        v.cBp.set (clampf (fc, 1500.0f, (float) (0.42 * fo)), (float) fo, 1.3f);
    }

    v.ce = 1.0;
    v.ceK = std::exp (-(double) LN60 / (decayS * fo));

    // ---- rattle, stick, chick -------------------------------------------------------
    v.rattle = clampf (P.sizzle, 0.0f, 1.0f) * rattleOf (o) * 1.2f;
    v.rHp.set (7000.0f, (float) fo, 0.5f);      // a band-pass: the rattle, not the ultrasound
    v.stAmt = stickAmt;
    v.stE = stickAmt > 0.0f ? 1.0 : 0.0;
    v.stK = std::exp (-1.0 / (0.0008 * fo));
    v.sHp.set (6000.0f, (float) fo, 0.7f);       // a band-pass: white at 4x is mostly ultrasound
    v.chAmt = chickAmt;
    v.chE = chickAmt > 0.0f ? 1.0 : 0.0;
    v.chK = std::exp (-1.0 / (0.005 * fo));
    v.chAt2 = 0.0014 * fo;
    v.ch2 = false;
    v.chBp.set (2400.0f, (float) fo, 1.1f);

    v.tBase = longest * fo;
    hits.fetch_add (1, std::memory_order_relaxed);
}

//==============================================================================
void Engine::process (const Params& p, float* outL, float* outR, int n)
{
    P = p;
    if (fresh)
    {
        driveSm = p.drive; roomSm = p.room;
        colourSm = p.colour >= 0.999f ? 1.0f : p.colour;
        levelSm = p.level == 0.0f ? 1.0f : db2lin (p.level);
        echoSm = p.echo;
        echoT = echoTSm = kTimeBeats[std::clamp ((int) std::lround (p.time), 0, kNumTimes - 1)] * 60.0 / bpm;
        fresh = false;
    }

    for (int i = 0; i < numPend; ++i)
    {
        const auto& h = pend[(size_t) i];
        const int artic = h.artic >= 0 ? h.artic : articFor ((int) std::lround (P.keys), h.note);
        startVoice (artic, h.note, h.vel, h.strike);
    }
    numPend = 0;

    //  EQ: fixed for the block
    cutL.set (clampf (P.cut, 20.0f, 8000.0f), (float) fo, 0.707f);
    cutR.set (clampf (P.cut, 20.0f, 8000.0f), (float) fo, 0.707f);
    const float airF = 3000.0f * std::pow (2.0f, 2.9f * clampf (P.air, 0.0f, 1.0f));
    airL.set (airF, (float) fo, 0.707f);
    airR.set (airF, (float) fo, 0.707f);

    float peak = 0.0f;
    const bool compOn = P.comp > 0.0f;
    const float amt = clampf (P.comp, 0.0f, 1.0f);
    const float Tc = -30.0f * amt, R = 1.0f + 7.0f * amt, W = 6.0f;
    auto gc = [&] (float L)
    {
        const float over = L - Tc, s = 1.0f - 1.0f / R;
        if (over <= -0.5f * W) return 0.0f;
        if (over >=  0.5f * W) return -s * over;
        const float x = over + 0.5f * W;
        return -s * x * x / (2.0f * W);
    };
    const float makeup = compOn ? -gc (-6.0f) * 0.7f : 0.0f;
    const float sp = clampf (P.speed, 0.0f, 1.0f);
    const double attMs = 20.0 * std::pow (0.2 / 20.0, sp);
    const double relMs = 300.0 * std::pow (30.0 / 300.0, sp);
    const float aA = (float) (1.0 - std::exp (-1.0 / (attMs * 0.001 * fs)));
    const float aR = (float) (1.0 - std::exp (-1.0 / (relMs * 0.001 * fs)));
    const float levelT = P.level == 0.0f ? 1.0f : db2lin (P.level);
    const float aLev = (float) (1.0 - std::exp (-1.0 / (0.01 * fs)));

    const int ti = std::clamp ((int) std::lround (P.time), 0, kNumTimes - 1);
    echoT = kTimeBeats[ti] * 60.0 / bpm;
    const float aEcho = (float) (1.0 - std::exp (-1.0 / (0.02 * fs)));
    const double aTime = 1.0 - std::exp (-1.0 / (0.06 * fs));
    const float eHpA = (float) std::exp (-2.0 * PI * 160.0 / fs);
    const float eLpA = (float) (1.0 - std::exp (-2.0 * PI * 3200.0 / fs));
    if (! echoLive && P.echo > 0.0f) echoTSm = echoT;

    float osL[kOS], osR[kOS];
    for (int i = 0; i < n; ++i)
    {
        renderOS (P, osL, osR);
        //  SEQUENCED, never nested: C++ does not specify the order function
        //  arguments are evaluated in, and MSVC evaluates right to left - nested,
        //  the second pair went into the half-band first, and every harmonic above
        //  24 kHz came back folded (a clipped 4.7 kHz tone aliased at -30 dB)
        const float h0L = hb1L.down (osL[0], osL[1]);
        const float h1L = hb1L.down (osL[2], osL[3]);
        const float h0R = hb1R.down (osR[0], osR[1]);
        const float h1R = hb1R.down (osR[2], osR[3]);
        float yL = hb2L.down (h0L, h1L);
        float yR = hb2R.down (h0R, h1R);

        // ---- COMP (linked) -----------------------------------------------------
        const float nL = yL, nR = yR;
        yL = laL[(size_t) laPos]; yR = laR[(size_t) laPos];
        laL[(size_t) laPos] = nL; laR[(size_t) laPos] = nR;
        if (++laPos >= laN) laPos = 0;
        if (compOn)
        {
            const float ax = std::max (std::abs (nL), std::abs (nR));
            while (dqTail > dqHead && dqVal[(size_t) ((dqTail - 1) & dqMask)] <= ax) --dqTail;
            dqVal[(size_t) (dqTail & dqMask)] = ax;
            dqIdx[(size_t) (dqTail & dqMask)] = cTick;
            ++dqTail;
            while (dqIdx[(size_t) (dqHead & dqMask)] <= cTick - holdN) ++dqHead;
            ++cTick;
            const float L = 20.0f * std::log10 (dqVal[(size_t) (dqHead & dqMask)] + 1.0e-9f);
            const float target = gc (L);
            compG += (target < compG ? aA : aR) * (target - compG);
            const float gg = db2lin (compG + makeup);
            yL *= gg; yR *= gg;
        }
        else if (compG != 0.0f) { compG = 0.0f; dqHead = dqTail = 0; }

        // ---- ECHO -------------------------------------------------------------
        float l = yL, r = yR;
        echoSm += aEcho * (P.echo - echoSm);
        if (P.echo <= 0.0f && echoSm < 1.0e-5f) echoSm = 0.0f;
        if (echoSm > 0.0f)
        {
            echoLive = true;
            echoTSm += aTime * (echoT - echoTSm);
            wowPh += 0.43 / fs; if (wowPh >= 1.0) wowPh -= 1.0;
            const double D = std::min ((double) eLen - 4.0, echoTSm * fs * (1.0 + 0.0012 * sin2pi (wowPh)));
            auto readH = [&] (const std::vector<float>& buf)
            {
                double rp = (double) eW - D;
                while (rp < 0.0) rp += eLen;
                const int i1 = (int) rp;
                const float f = (float) (rp - i1);
                const int i0 = (i1 - 1 + eLen) % eLen, i2 = (i1 + 1) % eLen, i3 = (i1 + 2) % eLen;
                const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
                const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                return ((c3 * f + c2) * f + c1) * f + y1;
            };
            const float rL = readH (eBuf[0]), rR = readH (eBuf[1]);
            auto loop = [&] (float x, int c)
            {
                const float hp = x - eHpZ[c];
                eHpZ[c] = x - eHpA * hp;
                eLpZ[c] += eLpA * (hp - eLpZ[c]);
                return std::tanh (1.3f * eLpZ[c]) * (1.0f / 1.3f);
            };
            const float fb = 0.28f + 0.47f * echoSm, send = 0.85f * echoSm;
            eBuf[0][(size_t) eW] = send * 0.5f * (yL + yR) + fb * loop (rR, 1);
            eBuf[1][(size_t) eW] = fb * loop (rL, 0);
            if (++eW >= eLen) eW = 0;
            l = yL + rL; r = yR + rR;
        }
        else if (echoLive)
        {
            for (auto& bb : eBuf) std::fill (bb.begin(), bb.end(), 0.0f);
            eHpZ[0] = eHpZ[1] = eLpZ[0] = eLpZ[1] = 0.0f;
            echoLive = false;
        }

        if (levelSm != levelT)
        {
            levelSm += aLev * (levelT - levelSm);
            if (std::abs (levelSm - levelT) < 1.0e-6f) levelSm = levelT;
        }
        l = ceilSoft (l * levelSm);
        r = ceilSoft (r * levelSm);
        outL[i] = l; outR[i] = r;
        peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
    }

    mPeak.store (peak, std::memory_order_relaxed);
    mGr.store (compG, std::memory_order_relaxed);
}

//==============================================================================
void Engine::renderOS (const Params& p, float* oL, float* oR)
{
    const float g = clampf (p.grit, 0.0f, 1.0f);
    const double gritRate = 44100.0 * std::pow (2.0, -2.5 * g);
    const float  gritQ = std::pow (2.0f, (16.0f - 12.0f * g) - 1.0f);
    const float att = clampf (p.attack, -1.0f, 1.0f), sus = clampf (p.sustain, -1.0f, 1.0f);
    const int eng = std::clamp ((int) std::lround (p.engine), 0, NUM_DRIVE_ENGINES - 1);
    const float aSm = (float) (1.0 - std::exp (-1.0 / (0.02 * fo)));
    const float rm = clampf (p.room, 0.0f, 1.0f);
    const float width = clampf (p.width, 0.0f, 1.0f);
    const bool airOn = p.air < 0.985f;
    const bool cutOn = p.cut > 20.5f;     // at its floor, exactly bypassed
    float roomG[kLines];
    const float rtNow = 0.25f + 2.6f * std::pow (roomSm, 1.4f);
    for (int i = 0; i < kLines; ++i)
        roomG[i] = std::pow (10.0f, -3.0f * (float) len[(size_t) i] / (rtNow * (float) fo));
    const float dampA = 1.0f - std::exp (-2.0f * (float) PI * (9000.0f - 4500.0f * roomSm) / (float) fo);

    for (int j = 0; j < kOS; ++j)
    {
        float xL = 0.0f, xR = 0.0f, env = 0.0f;
        for (auto& v : voices)
        {
            if (! v.on) continue;

            float sL = 0.0f, sR = 0.0f;
            if (benchToneHz > 0.0f)
            {
                const float s = 0.5f * sin2pi (v.t * benchToneHz / fo);
                sL = sR = s;
                env = 0.5f;
            }
            else
            {
                // ---- the bronze plate ----------------------------------------------
                if (v.bronzeAmt > 0.0f && v.nModes > 0)
                {
                   #if HO_SSE
                    __m128 accL = _mm_setzero_ps(), accR = _mm_setzero_ps();
                    const __m128 one = _mm_set1_ps (1.0f);
                    for (int k = 0; k < v.nModes; k += 4)
                    {
                        const __m128 zr = _mm_load_ps (v.zr + k), zi = _mm_load_ps (v.zi + k);
                        const __m128 cr = _mm_load_ps (v.cr + k), ci = _mm_load_ps (v.ci + k);
                        const __m128 nr = _mm_sub_ps (_mm_mul_ps (zr, cr), _mm_mul_ps (zi, ci));
                        const __m128 ni = _mm_add_ps (_mm_mul_ps (zr, ci), _mm_mul_ps (zi, cr));
                        _mm_store_ps (v.zr + k, nr);
                        _mm_store_ps (v.zi + k, ni);
                        __m128 gg = _mm_load_ps (v.g + k);
                        gg = _mm_add_ps (gg, _mm_mul_ps (_mm_sub_ps (one, gg), _mm_load_ps (v.ga + k)));
                        _mm_store_ps (v.g + k, gg);
                        const __m128 val = _mm_mul_ps (ni, gg);
                        accL = _mm_add_ps (accL, _mm_mul_ps (val, _mm_load_ps (v.pl + k)));
                        accR = _mm_add_ps (accR, _mm_mul_ps (val, _mm_load_ps (v.pr + k)));
                    }
                    alignas (16) float la[4], ra[4];
                    _mm_store_ps (la, accL); _mm_store_ps (ra, accR);
                    sL = (la[0] + la[1]) + (la[2] + la[3]);
                    sR = (ra[0] + ra[1]) + (ra[2] + ra[3]);
                   #else
                    for (int k = 0; k < v.nModes; ++k)
                    {
                        const float nr = v.zr[k] * v.cr[k] - v.zi[k] * v.ci[k];
                        const float ni = v.zr[k] * v.ci[k] + v.zi[k] * v.cr[k];
                        v.zr[k] = nr; v.zi[k] = ni;
                        v.g[k] += (1.0f - v.g[k]) * v.ga[k];
                        const float val = ni * v.g[k];
                        sL += val * v.pl[k]; sR += val * v.pr[k];
                    }
                   #endif
                    // ---- the wash ------------------------------------------------------
                    if (v.nWash > 0 && v.bronzeAmt > 0.0f)
                    {
                        const float n1 = xorshift (v.rng);
                        float wl = 0.0f, wr = 0.0f;
                        if (v.wSin == 0.0f)
                        {
                            for (int b = 0; b < v.nWash; ++b)
                            {
                                v.wG[b] += (1.0f - v.wG[b]) * v.wGa[b];
                                wl += v.wbL[b].bpNorm (n1) * (float) v.wE[b] * v.wG[b] * v.wAmp[b];
                                v.wE[b] *= v.wK[b];
                            }
                            wr = wl;
                        }
                        else
                        {
                            //  decorrelated across WIDTH: the right side hears a second noise
                            const float n2 = v.wCos * n1 + v.wSin * xorshift (v.rng);
                            for (int b = 0; b < v.nWash; ++b)
                            {
                                v.wG[b] += (1.0f - v.wG[b]) * v.wGa[b];
                                const float gg = (float) v.wE[b] * v.wG[b] * v.wAmp[b];
                                wl += v.wbL[b].bpNorm (n1) * gg;
                                wr += v.wbR[b].bpNorm (n2) * gg;
                                v.wE[b] *= v.wK[b];
                            }
                        }
                        sL += wl; sR += wr;     // under the china modulation too
                    }

                    //  the china: the plate rings against its own lowest modes
                    if (v.trash > 0.0f)
                    {
                        const float low = (v.zi[0] + v.zi[1] + v.zi[2]) / (v.low0 * 1.7f);
                        const float m = 1.0f + v.trash * 1.6f * low;
                        sL *= m; sR *= m;
                    }
                    sL *= v.bronzeAmt; sR *= v.bronzeAmt;
                }

                // ---- the circuit ---------------------------------------------------
                float c = 0.0f, sq0 = 0.0f;
                if (v.circuitAmt > 0.0f)
                {
                    float sq[kSquares];
                    for (int i = 0; i < kSquares; ++i)
                    {
                        const double dt = v.sqInc[i];
                        double& ph = v.sqPh[i];
                        float y = ph < 0.5 ? 1.0f : -1.0f;
                        y += blep (ph, dt);
                        double h = ph + 0.5; if (h >= 1.0) h -= 1.0;
                        y -= blep (h, dt);
                        sq[i] = y;
                        ph += dt; if (ph >= 1.0) ph -= 1.0;
                    }
                    sq0 = sq[0];
                    float sum = (sq[0] + sq[1] + sq[2] + sq[3] + sq[4] + sq[5]) * (1.0f / 6.0f);
                    if (v.trash > 0.0f) sum += v.trash * 0.75f * (sq[0] * sq[3] + sq[1] * sq[4]);
                    c = v.cBp.bpNorm (sum) * 3.2f * (float) v.ce * v.circuitAmt;
                }

                // ---- noise, rattle -----------------------------------------------
                float nz = 0.0f;
                if (v.noiseAmt > 0.0f) nz = xorshift (v.rng) * 2.4f * (float) v.ce * v.noiseAmt;   // white at 4x: most of it is above hearing
                float rat = 0.0f;
                if (v.rattle > 0.0f)
                {
                    const float gate = (v.bronzeAmt > 0.0f ? v.zi[0] : sq0) > 0.0f ? 1.0f : 0.18f;
                    rat = v.rHp.bpNorm (xorshift (v.rng)) * gate * (float) v.ce * v.rattle * 3.0f;
                }
                const float centre = (c + nz + rat);
                sL = (sL + centre) * v.bodyAmt;
                sR = (sR + centre) * v.bodyAmt;

                // ---- stick and chick ---------------------------------------------
                float tick = 0.0f;
                if (v.stE > 1.0e-7)
                {
                    v.stPh += 5200.0 / fo;
                    tick = (1.5f * v.sHp.bpNorm (xorshift (v.rng)) + 0.5f * sin2pi (v.stPh)) * (float) v.stE * v.stAmt * 3.6f;
                    v.stE *= v.stK;
                }
                float ch = 0.0f;
                if (v.chAmt > 0.0f && (v.chE > 1.0e-7 || ! v.ch2))
                {
                    if (! v.ch2 && v.t >= v.chAt2) { v.chE = 0.8; v.ch2 = true; }
                    ch = v.chBp.bpNorm (xorshift (v.rng)) * (float) v.chE * v.chAmt * 3.0f;
                    v.chE *= v.chK;
                }
                sL += tick + ch; sR += tick + ch;
                env = (float) v.ce * v.bodyAmt + (float) v.stE * v.stAmt * 3.6f + (float) v.chE * v.chAmt;
                v.ce *= v.ceK;
            }

            //  a voice ends in exact zero rather than being cut at a small level
            const double xd = v.t / v.tBase;
            float fade = 1.0f;
            if (xd > 1.3) fade = 1.0f - smooth01 ((float) ((xd - 1.3) / 0.2));
            float gain = 0.9f * v.velGain * fade;
            if (v.chokeStep > 0.0f)
            {
                v.choke -= v.chokeStep;
                if (v.choke <= 0.0f) { v.on = false; continue; }
                gain *= v.choke;
            }
            xL += sL * gain; xR += sR * gain;
            env *= gain;
            v.t += 1.0;
            if (benchToneHz <= 0.0f && xd > 1.5 && v.stE <= 1.0e-7 && (v.chAmt <= 0.0f || (v.ch2 && v.chE <= 1.0e-7)))
                v.on = false;
        }

        // ================= EQ ================================================
        if (cutOn) { xL = cutL.hp (xL); xR = cutR.hp (xR); }
        if (airOn) { xL = airL.lpOnly (xL); xR = airR.lpOnly (xR); }

        // ================= GRIT ==============================================
        if (g > 0.0f)
        {
            gritPh += gritRate / fo;
            if (gritPh >= 1.0)
            {
                gritPh -= std::floor (gritPh);
                gritL = std::round (xL * gritQ) / gritQ;
                gritR = std::round (xR * gritQ) / gritQ;
            }
            xL = gritL; xR = gritR;
        }

        // ================= TRANSIENT =========================================
        tFA += (env > tFA ? aFA : aRelA) * (env - tFA);
        tSA += (env > tSA ? aSA : aRelA) * (env - tSA);
        tFR += (env > tFR ? 1.0f : aFR) * (env - tFR);
        tSR += (env > tSR ? 1.0f : aSR) * (env - tSR);
        if (att != 0.0f || sus != 0.0f)
        {
            const float dA = clampf (20.0f * std::log10 ((tFA + 1.0e-6f) / (tSA + 1.0e-6f)), 0.0f, 24.0f);
            const float dS = clampf (20.0f * std::log10 ((tSR + 1.0e-6f) / (tFR + 1.0e-6f)), 0.0f, 30.0f);
            const float gdb = clampf (att * 0.5f * dA + sus * (sus > 0.0f ? 0.4f : 0.8f) * dS, -30.0f, 12.0f);
            const float gg = db2lin (gdb);
            xL *= gg; xR *= gg;
        }

        // ================= ROOM ==============================================
        roomSm += aSm * (rm - roomSm);
        if (rm > 0.0f || roomSm > 1.0e-5f)
        {
            roomLive = true;
            float d = (xL + xR) * 0.25f;
            for (int k = 0; k < 2; ++k)
            {
                auto& buf = ap[(size_t) k];
                const float z = buf[(size_t) apPos[(size_t) k]];
                const float w = d + 0.62f * z;
                buf[(size_t) apPos[(size_t) k]] = w;
                if (++apPos[(size_t) k] >= apLen[(size_t) k]) apPos[(size_t) k] = 0;
                d = z - 0.62f * w;
            }
            float o[kLines], h[kLines];
            for (int i = 0; i < kLines; ++i)
            {
                const float rr = line[(size_t) i][(size_t) pos[(size_t) i]];
                dampZ[(size_t) i] += dampA * (rr - dampZ[(size_t) i]);
                o[i] = h[i] = dampZ[(size_t) i];
            }
            for (int span = 1; span < kLines; span <<= 1)
                for (int i = 0; i < kLines; i += 2 * span)
                    for (int k = i; k < i + span; ++k)
                    {
                        const float u = h[k], w = h[k + span];
                        h[k] = u + w; h[k + span] = u - w;
                    }
            constexpr float inv = 0.35355339f;
            const float sgn[kLines] = { 1, -1, 1, 1, -1, 1, -1, -1 };
            float wA = 0.0f, wB = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                line[(size_t) i][(size_t) pos[(size_t) i]] = d * sgn[i] + h[i] * inv * roomG[i];
                if (++pos[(size_t) i] >= len[(size_t) i]) pos[(size_t) i] = 0;
                wA += o[i] * ((i & 1) ? -1.0f : 1.0f);
                wB += o[i] * ((i & 2) ? -1.0f : 1.0f);
            }
            //  the room is as wide as WIDTH says; at 0 both sides are the same
            const float wetR = width <= 0.0f ? wA : wA + (wB - wA) * width;
            xL += wA * 0.42f * roomSm;
            xR += wetR * 0.42f * roomSm;
        }
        else if (roomLive)
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
            for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
            dampZ.fill (0.0f);
            roomLive = false;
        }

        // ================= DRIVE, at 8x =====================================
        //  A cymbal lives where a 4x clipper folds worst: a hard-clipped 4.7 kHz
        //  tone is a square whose 41st harmonic sits on the 4x rate. So the drive
        //  runs at twice the voice rate, ALWAYS (drive or not), with one sample
        //  of padding: 31 + 31 samples at 8x plus 1 at 4x is exactly 8 samples at
        //  the host rate, so the latency is an integer and never moves.
        driveSm += aSm * (p.drive - driveSm);
        if (p.drive <= 0.0f && driveSm < 1.0e-6f) driveSm = 0.0f;
        {
            float uL[2], uR[2];
            upL.up (xL, uL[0], uL[1]);
            upR.up (xR, uR[0], uR[1]);
            if (driveSm > 0.0f)
            {
                const float wet = smooth01 (driveSm / 0.12f);
                const float k = 1.0f + driveSm * kDrive[eng].driveMax;
                const float trim = driveTrimFor (eng, driveSm);
                for (int s = 0; s < 2; ++s)
                {
                    const float a = uL[s], b = uR[s];
                    uL[s] = a + (driveADAA (eng, a, adaaL, k) * trim - a) * wet;
                    uR[s] = b + (driveADAA (eng, b, adaaR, k) * trim - b) * wet;
                    adaaL = a; adaaR = b;
                }
            }
            else { adaaL = uL[1]; adaaR = uR[1]; }
            const float dL = dnL.down (uL[0], uL[1]), dR = dnR.down (uR[0], uR[1]);
            xL = padL; xR = padR;
            padL = dL; padR = dR;
        }

        // ================= COLOUR ============================================
        colourSm += aSm * (p.colour - colourSm);
        if (p.colour >= 0.999f && colourSm > 0.998f) colourSm = 1.0f;
        if (colourSm < 1.0f)
        {
            if (--coefTick <= 0)
            {
                coefTick = 16;
                const float fc = 1500.0f * std::pow (2.0f, 3.8f * colourSm);
                colL.set (fc, (float) fo, 0.7071f);
                colR.set (fc, (float) fo, 0.7071f);
            }
            xL = colL.lpOnly (xL); xR = colR.lpOnly (xR);
        }
        else { colL.clear(); colR.clear(); coefTick = 0; }

        oL[j] = xL; oR[j] = xR;
    }
}

//==============================================================================
void Engine::renderHit (const Params& p, double sampleRate, int artic, float vel,
                        std::vector<float>& left, std::vector<float>& right, int n)
{
    auto e = std::make_unique<Engine>();
    e->prepare (sampleRate, 512);
    e->noteOnArtic (artic, vel);
    left.assign ((size_t) n, 0.0f);
    right.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += 512)
        e->process (p, left.data() + i, right.data() + i, std::min (512, n - i));
}

} // namespace ho
