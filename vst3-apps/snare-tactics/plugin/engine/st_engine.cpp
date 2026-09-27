#include "st_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace st
{

namespace
{
    constexpr double PI  = 3.14159265358979323846;
    constexpr float  LN60 = 6.9077553f;         // ln(1000): -60 dB

    inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
    inline float lerpf  (float a, float b, float t)   { return a + (b - a) * t; }
    inline float db2lin (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float smooth01 (float x) { x = clampf (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }

    /*  sin(2 pi x), any x: folded to a quarter wave, summed to y^11, error
        under 4e-8 (Kickstart's). A snare at 4x runs a dozen of these per
        voice, and std::sin costs several times as much. */
    inline float sin2pi (double x)
    {
        double r = x - std::floor (x + 0.5);
        if (r > 0.25)       r = 0.5 - r;
        else if (r < -0.25) r = -0.5 - r;
        const double y = 2.0 * PI * r, y2 = y * y;
        return (float) (y * (1.0 + y2 * (-1.0 / 6.0 + y2 * (1.0 / 120.0 + y2 * (-1.0 / 5040.0
                         + y2 * (1.0 / 362880.0 + y2 * (-1.0 / 39916800.0)))))));
    }

    //  IEEE-exact identity below 0.9 of the limit, asymptotic above (the fleet's)
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

    //  one oscillator of the electronic pair: sine -> triangle -> rounded square
    inline float waveOf (double ph, float s, float wave)
    {
        if (wave <= 0.0f) return s;
        const double q = ph - std::floor (ph);
        const float tri = (float) (q < 0.25 ? 4.0 * q : (q < 0.75 ? 2.0 - 4.0 * q : 4.0 * q - 4.0));
        if (wave <= 0.5f) return lerpf (s, tri, wave * 2.0f);
        const float sq = std::tanh (4.0f * s) * 1.0006711504f;   // 1/tanh(4)
        return lerpf (tri, sq, (wave - 0.5f) * 2.0f);
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
        /*  Measured, never typed: each engine's own shaper over a tone at three
            levels, averaged in the log domain, so turning DRIVE up changes the
            character and not the loudness. A snare's energy sits higher than a
            kick's, so the reference tone is 200 Hz. */
        TrimTable()
        {
            const float REF[3] = { 0.15f, 0.35f, 0.70f };
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
                            const float x = a * sin2pi (200.0 * n / 48000.0);
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

    const TrimTable& trims()
    {
        static const TrimTable tt;
        return tt;
    }

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

    //  base weight of each head mode, before STRIKE and RING
    constexpr float kModeBase[kModes] = { 1.0f, 0.62f, 0.50f, 0.46f, 0.40f, 0.36f, 0.32f, 0.29f, 0.27f, 0.25f };

    //  the second oscillator of the 808/909 body sits near 1 : 1.83
    constexpr float kPairRatio = 1.83f;

    //  shell and hoop modes a rim shot rings (at TUNE 185 Hz), and their decays
    constexpr float kShellHz[kShell]  = { 520.0f, 1190.0f, 2310.0f };
    constexpr float kShellMs[kShell]  = { 38.0f, 22.0f, 12.0f };
    constexpr float kShellAmp[kShell] = { 0.55f, 0.42f, 0.30f };

    //  clap burst jitter: hands are never exactly evenly spaced
    constexpr float kBurstJit[kBursts] = { 0.0f, 1.0f, 2.15f, 3.05f };

    inline float xorshift (uint32_t& r)
    {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        return (float) (int32_t) r * (1.0f / 2147483648.0f);
    }
}

//  circular membrane modes relative to the (0,1) mode: Bessel zeros over j(0,1)
const float kModeRatio[kModes] = { 1.0f, 1.5933f, 2.1355f, 2.2954f, 2.6531f, 2.9173f, 3.1555f, 3.5001f, 3.5985f, 3.6475f };
//  (0,1) (1,1) (2,1) (0,2) (3,1) (1,2) (4,1) (2,2) (0,3) (5,1): the (0,n) are axisymmetric
const bool  kModeAxis[kModes]  = { true, false, false, true, false, false, false, false, true, false };

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

float driveMaxFor (int e) { return kDrive[std::clamp (e, 0, NUM_DRIVE_ENGINES - 1)].driveMax; }
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

const char* noteName (float hz, char* buf, int bufLen)
{
    static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double m = 69.0 + 12.0 * std::log2 (std::max (1.0, (double) hz) / 440.0);
    const int r = (int) std::lround (m);
    const int c = (int) std::lround ((m - r) * 100.0);
    const int oct = r / 12 - 1;
    if (c == 0) std::snprintf (buf, (size_t) bufLen, "%s%d", N[((r % 12) + 12) % 12], oct);
    else        std::snprintf (buf, (size_t) bufLen, "%s%d %+dc", N[((r % 12) + 12) % 12], oct, c);
    return buf;
}

int Engine::articFor (int keys, int note)
{
    if (keys != KEYS_GM) return ART_SNARE;
    switch (note)
    {
        case 37: return ART_XSTICK;
        case 39: return ART_CLAP;
        case 40: return ART_RIM;
        default: return ART_SNARE;
    }
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

    hb1.design(); hb2.design();
    (void) trims();

    const double ms = fo * 0.001;
    //  8 lines, mutually prime-ish, 13-42 ms: dense enough that a snare (which
    //  shows every flaw a reverb has) does not ring metallic
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
    aFA = co (0.1); aSA = co (8.0); aRelA = co (15.0);
    aFR = co (6.0); aSR = co (120.0);

    laN = std::max (1, (int) std::lround (0.0015 * fs));
    laBuf.assign ((size_t) laN, 0.0f);
    holdN = laN + std::max (1, (int) std::lround (0.010 * fs));
    int dq = 1; while (dq < holdN + 4) dq <<= 1;
    dqVal.assign ((size_t) dq, 0.0f);
    dqIdx.assign ((size_t) dq, 0);
    dqMask = dq - 1;

    //  the echo holds two beats at 40 BPM
    eLen = (int) std::ceil (3.2 * fs) + 8;
    for (auto& b : eBuf) b.assign ((size_t) eLen, 0.0f);

    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice();
    hitSeed = 0;
    gritPh = 1.0; gritHeld = 0.0f;
    tFA = tSA = tFR = tSR = 0.0f;
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
    pos.fill (0); apPos.fill (0); dampZ.fill (0.0f);
    roomLive = false;
    sinceHit = 1.0e12; gateEnv = 0.0f;
    driveSm = 0.0f; colourSm = 1.0f; roomSm = 0.0f;
    adaaPrev = 0.0;
    std::fill (laBuf.begin(), laBuf.end(), 0.0f); laPos = 0;
    colourLp.clear(); coefTick = 0;
    hb1.clear(); hb2.clear();
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
        pend[(size_t) numPend++] = { -1, note, clampf (velocity01, 0.0f, 1.0f) };
}

void Engine::noteOnArtic (int artic, float velocity01)
{
    if (numPend < (int) pend.size())
        pend[(size_t) numPend++] = { std::clamp (artic, 0, NUM_ARTICS - 1), 38, clampf (velocity01, 0.0f, 1.0f) };
}

void Engine::setTempo (double b)
{
    if (b > 1.0) bpm = std::clamp (b, 40.0, 300.0);
}

//==============================================================================
void Engine::startVoice (int artic, int note, float vel)
{
    //  at most four are heard; the oldest of those is faded over 3 ms rather
    //  than cut (a cut clicks), and its slot is one of the two spares
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
    if (slot == nullptr)            // a 1/128 roll: take the most faded
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.choke < slot->choke) slot = &v;
    }

    Voice& v = *slot;
    v = Voice();
    v.on = true;
    v.artic = artic;
    sinceHit = 0.0;

    const float V = clampf (P.velo, 0.0f, 1.0f);
    v.velGain  = (1.0f - V) + V * std::pow (vel, 1.3f);
    v.wireVel  = (1.0f - V) + V * std::pow (vel, 0.75f);   // a ghost note is mostly wires
    v.stickVel = (1.0f - V) + V * vel * vel;
    v.dropVel  = 1.0f - 0.35f * V + 0.35f * V * vel;
    v.keyMul = (int) std::lround (P.keys) == KEYS_CHROMATIC ? (float) std::pow (2.0, (note - 38) / 12.0) : 1.0f;

    // ---- what this articulation asks of each part -------------------------
    const float strike = clampf (P.strike, 0.0f, 1.0f);
    float edge = smooth01 (strike / 0.7f);
    float rim  = smooth01 ((strike - 0.7f) / 0.3f);
    v.bodyAmt = 1.0f; v.bodyDecayMul = 1.0f; v.wireAmt = clampf (P.wires, 0.0f, 1.0f);
    v.clapAmt = clampf (P.clap, 0.0f, 1.0f);
    v.stickAmt = clampf (P.stick, 0.0f, 1.0f);
    v.shellMul = 1.0f;
    switch (artic)
    {
        case ART_RIM:
            edge = 1.0f; rim = 1.0f;
            v.bodyAmt = 1.25f;
            v.stickAmt = std::min (1.0f, 0.35f + 1.2f * v.stickAmt);
            break;
        case ART_XSTICK:
            //  the stick across the hoop, its tip on the head: the head is
            //  choked by the hand, the wires hardly move, the shell speaks
            edge = 0.6f; rim = 1.0f;
            v.bodyAmt = 0.35f; v.bodyDecayMul = 0.22f;
            v.wireAmt *= 0.08f; v.clapAmt = 0.0f;
            v.stickAmt = 0.8f; v.shellMul = 0.82f;
            break;
        case ART_CLAP:
            v.bodyAmt = 0.0f; v.wireAmt = 0.0f; v.stickAmt = 0.0f; rim = 0.0f;
            v.clapAmt = 1.0f;
            break;
        default: break;
    }
    v.rimAmt = rim;

    // ---- the head: which modes the strike excites, how long each rings ----
    const float ring = clampf (P.ring, 0.0f, 1.0f);
    const double tauBody = std::max (10.0f, P.decay * v.bodyDecayMul) / 1000.0 / LN60;
    for (int k = 0; k < kModes; ++k)
    {
        float a = kModeBase[k];
        if (k == 0)            a *= 1.0f - 0.45f * edge;      // off-centre, the fundamental speaks less
        else if (kModeAxis[k]) a *= (1.0f - 0.3f * edge) * (0.45f + 0.9f * ring);
        else                   a *= 0.95f * edge * (0.45f + 0.9f * ring);
        v.mAmp[k] = a;
        v.me[k] = 1.0;
        if (k > 0)
        {
            const double tau = tauBody * (0.18 + 1.4 * ring) / std::pow ((double) kModeRatio[k], 0.8);
            v.meK[k] = std::exp (-1.0 / (std::max (0.002, tau) * fo));
        }
        else v.meK[k] = 1.0;
    }

    // ---- the wires, fixed at the hit ------------------------------------------
    const float T = clampf (P.tension, 0.0f, 1.0f), loose = 1.0f - T;
    const float fHP = 220.0f * std::pow (2.0f, 3.4f * T);             // 220 Hz .. 2.3 kHz
    const float air = clampf (P.air, 0.0f, 1.0f);
    //  AIR opens the top; loose wires are darker than tight ones at any AIR
    const float fLP = 2200.0f * std::pow (2.0f, 3.2f * air) * (0.5f + 0.5f * T);
    v.wHp.set (fHP, (float) fo, 0.707f);
    v.wLpOn = fLP < 0.42f * (float) fo;
    v.wLp.set (fLP, (float) fo, 0.707f);
    const float bw = std::max (600.0f, (v.wLpOn ? fLP : (float) (0.45 * fo)) - fHP);
    v.wGain = 0.32f / std::sqrt (bw / (float) (0.5 * fo) / 3.0f);
    v.wLag  = (float) ((0.25 + 1.6 * loose) * 0.001 * fo);             // wires follow the head
    v.wAtt  = (float) ((0.6 + 1.4 * loose) * 0.001 * fo);
    v.wBuzz = 0.85f * std::pow (loose, 1.5f);                           // slapping the head each cycle
    v.wSymp = 0.30f * std::pow (loose, 1.2f);                           // ringing along with it

    // ---- stick and shell ---------------------------------------------------------
    v.sHp.set (2500.0f, (float) fo, 0.707f);
    v.stK = std::exp (-1.0 / (0.00035 * fo));
    v.stBlipF = 3600.0f * (1.0f + 0.4f * strike);
    const float shellScale = std::pow (clampf (P.tune, 100.0f, 420.0f) / 185.0f, 0.3f) * v.shellMul;
    for (int k = 0; k < kShell; ++k)
    {
        v.shF[k] = kShellHz[k] * shellScale;
        v.shA[k] = kShellAmp[k];
        const double tau = kShellMs[k] * (artic == ART_XSTICK ? 1.3 : 1.0) / 1000.0 / 2.3;
        v.shK[k] = std::exp (-1.0 / (tau * fo));
        v.shE[k] = 1.0;
    }

    // ---- the clap ------------------------------------------------------------------
    const float cfc = 1150.0f * std::pow (2.0f, 0.8f * (air - 0.5f));
    v.cBp.set (cfc, (float) fo, 1.3f);
    v.cNorm = (float) std::min (60.0, 0.6 / (0.57735 * std::sqrt (PI * (cfc / 1.3) / fo)));
    const float sp = clampf (P.spread, 0.0f, 1.0f);
    v.cNumBursts = sp < 0.02f ? 1 : kBursts;
    for (int b = 0; b < kBursts; ++b) v.cBurstAt[b] = kBurstJit[b] * sp * 0.0115 * fo;
    v.cBurstK = std::exp (-1.0 / (0.0024 * fo));
    //  SIZZLE sets the clap's tail as well: one knob for "how long it hangs"
    const double tailMs = clampf (P.sizzle * 0.7f, 80.0f, 900.0f);
    v.cTailK = std::exp (-LN60 / (tailMs * 0.001 * fo));
    v.cNext = 0; v.cEnv = 0.0; v.cTail = 0.0;

    v.rng = 0x9E3779B9u * (++hitSeed) ^ 0x2545F491u;
    if (v.rng == 0) v.rng = 1;
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
        startVoice (artic, h.note, h.vel);
    }
    numPend = 0;

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

    // ---- echo constants ---------------------------------------------------------
    const int ti = std::clamp ((int) std::lround (P.time), 0, kNumTimes - 1);
    echoT = kTimeBeats[ti] * 60.0 / bpm;
    const float aEcho = (float) (1.0 - std::exp (-1.0 / (0.02 * fs)));
    const double aTime = 1.0 - std::exp (-1.0 / (0.06 * fs));
    const float eHpA = (float) std::exp (-2.0 * PI * 160.0 / fs);
    const float eLpA = (float) (1.0 - std::exp (-2.0 * PI * 2600.0 / fs));
    if (! echoLive && P.echo > 0.0f) echoTSm = echoT;      // a fresh echo starts on time, not gliding in

    float os[kOS];
    for (int i = 0; i < n; ++i)
    {
        renderOS (P, os);
        const float a = hb1.down (os[0], os[1]);
        const float b = hb1.down (os[2], os[3]);
        float y = hb2.down (a, b);

        // ---- COMP -----------------------------------------------------------
        const float ynow = y;
        y = laBuf[(size_t) laPos];
        laBuf[(size_t) laPos] = ynow;
        if (++laPos >= laN) laPos = 0;
        if (compOn)
        {
            const float ax = std::abs (ynow);
            while (dqTail > dqHead && dqVal[(size_t) ((dqTail - 1) & dqMask)] <= ax) --dqTail;
            dqVal[(size_t) (dqTail & dqMask)] = ax;
            dqIdx[(size_t) (dqTail & dqMask)] = cTick;
            ++dqTail;
            while (dqIdx[(size_t) (dqHead & dqMask)] <= cTick - holdN) ++dqHead;
            ++cTick;
            const float L = 20.0f * std::log10 (dqVal[(size_t) (dqHead & dqMask)] + 1.0e-9f);
            const float target = gc (L);
            compG += (target < compG ? aA : aR) * (target - compG);
            y *= db2lin (compG + makeup);
        }
        else if (compG != 0.0f) { compG = 0.0f; dqHead = dqTail = 0; }

        // ---- ECHO -------------------------------------------------------------
        float l = y, r = y;
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
            //  the loop: each pass through the tape comes back thinner and darker
            auto loop = [&] (float x, int c)
            {
                const float hp = x - eHpZ[c];
                eHpZ[c] = x - eHpA * hp;                      // one-pole high-pass, 160 Hz
                eLpZ[c] += eLpA * (hp - eLpZ[c]);             // one-pole low-pass, 2.6 kHz
                return std::tanh (1.3f * eLpZ[c]) * (1.0f / 1.3f);
            };
            const float fb = 0.28f + 0.47f * echoSm, send = 0.85f * echoSm;
            eBuf[0][(size_t) eW] = send * y + fb * loop (rR, 1);   // ping ...
            eBuf[1][(size_t) eW] = fb * loop (rL, 0);              // ... pong
            if (++eW >= eLen) eW = 0;
            l = y + rL; r = y + rR;
        }
        else if (echoLive)
        {
            for (auto& bb : eBuf) std::fill (bb.begin(), bb.end(), 0.0f);
            eHpZ[0] = eHpZ[1] = eLpZ[0] = eLpZ[1] = 0.0f;
            echoLive = false;
        }

        // ---- LEVEL + ceiling ---------------------------------------------------
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
void Engine::renderOS (const Params& p, float* o4)
{
    const double foMs = fo * 0.001;
    const double invW = 1.0 / (std::max (10.0f, p.sizzle) * foMs);
    const double peK  = std::exp (-1.0 / (std::max (0.5f, p.bend) * foMs));
    const float  skin = clampf (p.skin, 0.0f, 1.0f), pair = 1.0f - skin;
    const float  wave = clampf (p.wave, 0.0f, 1.0f);
    const float  bodyLvl = 1.25f * clampf (p.body, 0.0f, 1.0f);
    const float  nyq = (float) (0.45 * fo);

    const float g = clampf (p.grit, 0.0f, 1.0f);
    const double gritRate = 44100.0 * std::pow (2.0, -2.5 * g);
    const float  gritQ = std::pow (2.0f, (16.0f - 12.0f * g) - 1.0f);

    const float att = clampf (p.attack, -1.0f, 1.0f), sus = clampf (p.sustain, -1.0f, 1.0f);
    const int eng = std::clamp ((int) std::lround (p.engine), 0, NUM_DRIVE_ENGINES - 1);
    const float aSm = (float) (1.0 - std::exp (-1.0 / (0.02 * fo)));

    const float rm = clampf (p.room, 0.0f, 1.0f);
    float roomG[kLines];
    const float rtNow = 0.22f + 2.4f * std::pow (roomSm, 1.4f);
    for (int i = 0; i < kLines; ++i)
        roomG[i] = std::pow (10.0f, -3.0f * (float) len[(size_t) i] / (rtNow * (float) fo));
    const float dampA = 1.0f - std::exp (-2.0f * (float) PI * (8500.0f - 4500.0f * roomSm) / (float) fo);

    const bool gateOn = p.gate > 0.0f;
    const double gateHold = std::max (20.0f, p.gate) * foMs;
    const float gateRel = (float) (1.0 / (0.010 * fo)), gateAtt = (float) (1.0 - std::exp (-1.0 / (0.0003 * fo)));

    for (int j = 0; j < kOS; ++j)
    {
        // ================= VOICES ============================================
        float x = 0.0f, env = 0.0f;
        for (auto& v : voices)
        {
            if (! v.on) continue;

            // ---- HEAD ------------------------------------------------------
            const double invD = 1.0 / (std::max (10.0f, p.decay * v.bodyDecayMul) * foMs);
            const double xd = benchSteady ? 0.0 : v.t * invD;
            float ae = (float) std::exp (-LN60 * xd);
            float fade = 1.0f;
            if (xd > 1.3) fade = 1.0f - smooth01 ((float) ((xd - 1.3) / 0.2));
            ae *= fade;

            const float semis = p.drop * v.dropVel * (float) v.pe + (benchSteady ? 0.0f : skin * 1.2f * v.velGain * ae);
            float f = p.tune * v.keyMul * std::exp2 (semis / 12.0f);
            f = std::min (f, nyq);
            lastPitch = f;

            v.ph += f / fo;
            if (v.ph >= 1.0) v.ph -= std::floor (v.ph);
            const float s1 = sin2pi (v.ph);
            float body = 0.0f;
            if (v.bodyAmt > 0.0f)
            {
                if (pair > 0.0f)
                {
                    const float f2 = std::min (f * kPairRatio, nyq);
                    v.ph2 += f2 / fo;
                    if (v.ph2 >= 1.0) v.ph2 -= std::floor (v.ph2);
                    const float e2 = (float) std::exp (-LN60 * xd / 0.6) * fade;
                    body += pair * (waveOf (v.ph, s1, wave) * ae + 0.6f * waveOf (v.ph2, sin2pi (v.ph2), wave) * e2);
                }
                if (skin > 0.0f)
                {
                    float m = s1 * ae * v.mAmp[0];
                    for (int k = 1; k < kModes; ++k)
                    {
                        if (v.me[k] < 1.0e-6) continue;
                        const float fk = f * kModeRatio[k];
                        if (fk < nyq)
                        {
                            v.phM[k] += fk / fo;
                            if (v.phM[k] >= 1.0) v.phM[k] -= std::floor (v.phM[k]);
                            m += sin2pi (v.phM[k]) * (float) v.me[k] * v.mAmp[k];
                        }
                        v.me[k] *= v.meK[k];
                    }
                    body += skin * m * fade;
                }
                body *= 0.55f * v.velGain * v.bodyAmt * bodyLvl;
            }

            // ---- WIRES -----------------------------------------------------
            float wires = 0.0f, wEnv = 0.0f;
            const double tw = v.t - v.wLag;
            const double xw = tw > 0.0 ? tw * invW : 0.0;
            if (v.wireAmt > 0.0f && tw > 0.0 && xw < 1.5)
            {
                float we = (float) std::exp (-LN60 * xw);
                if (xw > 1.3) we *= 1.0f - smooth01 ((float) ((xw - 1.3) / 0.2));
                const float rise = smooth01 ((float) (tw / v.wAtt));
                wEnv = rise * (we + v.wSymp * ae);
                float nz = v.wHp.hp (xorshift (v.rng));
                if (v.wLpOn) nz = v.wLp.lpOnly (nz);
                //  loose wires slap the head once a cycle: a rattle at its pitch
                const float slap = (1.0f - v.wBuzz) + v.wBuzz * 3.14159265f * std::max (0.0f, s1);
                wires = nz * v.wGain * wEnv * slap * v.wireAmt * v.wireVel;
                wEnv *= v.wireAmt * v.wireVel;
            }
            else if (v.wireAmt > 0.0f && tw > 0.0 && v.wSymp > 0.0f && ae > 1.0e-6f)
            {
                //  the wires' own decay is over, but loose ones still sing with the head
                wEnv = v.wSymp * ae;
                float nz = v.wHp.hp (xorshift (v.rng));
                if (v.wLpOn) nz = v.wLp.lpOnly (nz);
                const float slap = (1.0f - v.wBuzz) + v.wBuzz * 3.14159265f * std::max (0.0f, s1);
                wires = nz * v.wGain * wEnv * slap * v.wireAmt * v.wireVel;
                wEnv *= v.wireAmt * v.wireVel;
            }

            // ---- STICK and SHELL ---------------------------------------------
            float stick = 0.0f, stEnv = 0.0f;
            if ((v.stickAmt > 0.0f || v.rimAmt > 0.0f) && (v.stE > 1.0e-7 || v.shE[0] > 1.0e-6))
            {
                const float nzs = v.sHp.hp (xorshift (v.rng));
                v.stPh += v.stBlipF / fo;
                const float click = (0.6f * nzs + 0.7f * sin2pi (v.stPh)) * (float) v.stE;
                stick = click * (0.8f * v.stickAmt + 0.5f * v.rimAmt) * v.stickVel;
                stEnv = (float) v.stE * 0.8f * v.stickAmt * v.stickVel;
                v.stE *= v.stK;
                if (v.rimAmt > 0.0f)
                {
                    float sh = 0.0f;
                    for (int k = 0; k < kShell; ++k)
                    {
                        v.shPh[k] += v.shF[k] / fo;
                        if (v.shPh[k] >= 1.0) v.shPh[k] -= 1.0;
                        sh += sin2pi (v.shPh[k]) * (float) v.shE[k] * v.shA[k];
                        v.shE[k] *= v.shK[k];
                    }
                    stick += sh * v.rimAmt * 0.8f * v.stickVel;
                    stEnv += (float) v.shE[0] * v.rimAmt * 0.4f * v.stickVel;
                }
                else for (auto& e : v.shE) e = 0.0;
            }

            // ---- CLAP ----------------------------------------------------------
            float clap = 0.0f, cEnvOut = 0.0f;
            if (v.clapAmt > 0.0f && (v.cNext < v.cNumBursts || v.cEnv > 1.0e-6 || v.cTail > 1.0e-6))
            {
                if (v.cNext < v.cNumBursts && v.t >= v.cBurstAt[v.cNext])
                {
                    v.cEnv = 1.0;
                    if (++v.cNext == v.cNumBursts) v.cTail = 0.5;
                }
                float lp, bp;
                v.cBp.tick (xorshift (v.rng), lp, bp);
                const float e = (float) (v.cEnv + v.cTail);
                clap = bp * v.cBp.k * v.cNorm * e * 0.9f * v.clapAmt * v.wireVel;
                cEnvOut = e * 0.9f * v.clapAmt * v.wireVel;
                v.cEnv *= v.cBurstK;
                v.cTail *= v.cTailK;
            }

            float vo = body + wires + stick + clap;
            float ve = ae * 0.55f * v.velGain * v.bodyAmt * bodyLvl + wEnv * 0.5f + stEnv + cEnvOut;

            if (v.chokeStep > 0.0f)
            {
                v.choke -= v.chokeStep;
                if (v.choke <= 0.0f) { v.on = false; continue; }
                vo *= v.choke; ve *= v.choke;
            }
            x += vo; env += ve;

            v.pe *= peK;
            v.t += 1.0;
            const bool bodyDone  = xd > 1.5 || v.bodyAmt <= 0.0f;
            const bool wiresDone = v.wireAmt <= 0.0f || (xw >= 1.5 && (v.wSymp <= 0.0f || bodyDone));
            const bool stickDone = (v.stickAmt <= 0.0f && v.rimAmt <= 0.0f) || (v.stE <= 1.0e-7 && v.shE[0] <= 1.0e-6);
            const bool clapDone  = v.clapAmt <= 0.0f || (v.cNext >= v.cNumBursts && v.cEnv <= 1.0e-6 && v.cTail <= 1.0e-6);
            if (bodyDone && wiresDone && stickDone && clapDone) v.on = false;
        }

        // ================= GRIT ==============================================
        if (g > 0.0f)
        {
            gritPh += gritRate / fo;
            if (gritPh >= 1.0)
            {
                gritPh -= std::floor (gritPh);
                gritHeld = std::round (x * gritQ) / gritQ;
            }
            x = gritHeld;
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
            x *= db2lin (gdb);
        }

        // ================= ROOM ==============================================
        roomSm += aSm * (rm - roomSm);
        if (rm > 0.0f || roomSm > 1.0e-5f)
        {
            roomLive = true;
            //  two diffusers first: a snare's crack into bare delay lines is a
            //  flutter; smeared by a few ms it is a room
            float d = x * 0.5f;
            for (int k = 0; k < 2; ++k)
            {
                auto& buf = ap[(size_t) k];
                const float z = buf[(size_t) apPos[(size_t) k]];
                const float w = d + 0.62f * z;
                buf[(size_t) apPos[(size_t) k]] = w;
                if (++apPos[(size_t) k] >= apLen[(size_t) k]) apPos[(size_t) k] = 0;
                d = z - 0.62f * w;
            }
            float o[kLines];
            for (int i = 0; i < kLines; ++i)
            {
                const float r = line[(size_t) i][(size_t) pos[(size_t) i]];
                dampZ[(size_t) i] += dampA * (r - dampZ[(size_t) i]);
                o[i] = dampZ[(size_t) i];
            }
            //  an 8-point fast Walsh-Hadamard transform, normalised
            float h[kLines];
            for (int i = 0; i < kLines; ++i) h[i] = o[i];
            for (int span = 1; span < kLines; span <<= 1)
                for (int i = 0; i < kLines; i += 2 * span)
                    for (int k = i; k < i + span; ++k)
                    {
                        const float u = h[k], w = h[k + span];
                        h[k] = u + w; h[k + span] = u - w;
                    }
            constexpr float inv = 0.35355339f;       // 1/sqrt(8)
            const float sgn[kLines] = { 1, -1, 1, 1, -1, 1, -1, -1 };
            float wet = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                line[(size_t) i][(size_t) pos[(size_t) i]] = d * sgn[i] + h[i] * inv * roomG[i];
                if (++pos[(size_t) i] >= len[(size_t) i]) pos[(size_t) i] = 0;
                wet += o[i] * ((i & 1) ? -1.0f : 1.0f);
            }
            x += wet * 0.42f * roomSm;
        }
        else if (roomLive)
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
            for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
            dampZ.fill (0.0f);
            roomLive = false;
        }

        // ================= GATE ==============================================
        //  keyed by the hit, so it opens exactly on the stick and closes
        //  exactly when asked, whatever the level: the 80s gated room, with
        //  no threshold to chatter
        if (gateOn)
        {
            if (sinceHit < gateHold) gateEnv += gateAtt * (1.0f - gateEnv);
            else                     gateEnv = std::max (0.0f, gateEnv - gateRel);
            x *= gateEnv;
        }
        else gateEnv = 1.0f;
        sinceHit += 1.0;

        // ================= DRIVE =============================================
        const float xIn = x;
        driveSm += aSm * (p.drive - driveSm);
        if (p.drive <= 0.0f && driveSm < 1.0e-6f) driveSm = 0.0f;
        if (driveSm > 0.0f)
        {
            const float wet = smooth01 (driveSm / 0.12f);
            const float k = 1.0f + driveSm * kDrive[eng].driveMax;
            const float yv = driveADAA (eng, x, adaaPrev, k) * driveTrimFor (eng, driveSm);
            x = x + (yv - x) * wet;
        }
        adaaPrev = xIn;

        // ================= COLOUR ============================================
        colourSm += aSm * (p.colour - colourSm);
        if (p.colour >= 0.999f && colourSm > 0.998f) colourSm = 1.0f;
        if (colourSm < 1.0f)
        {
            if (--coefTick <= 0)
            {
                coefTick = 16;
                colourLp.set (1200.0f * std::pow (2.0f, 4.0f * colourSm), (float) fo, 0.7071f);
            }
            float lp, bp;
            colourLp.tick (x, lp, bp);
            x = lp;
        }
        else { colourLp.clear(); coefTick = 0; }

        o4[j] = x;
    }
}

//==============================================================================
void Engine::renderHit (const Params& p, double sampleRate, int artic, float vel,
                        std::vector<float>& out, int n)
{
    auto e = std::make_unique<Engine>();
    e->prepare (sampleRate, 512);
    e->noteOnArtic (artic, vel);
    out.assign ((size_t) n, 0.0f);
    std::vector<float> r ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += 512)
        e->process (p, out.data() + i, r.data() + i, std::min (512, n - i));
}

} // namespace st
