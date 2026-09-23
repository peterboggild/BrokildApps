#include "Engine.h"

namespace bo
{

static constexpr double PI = 3.14159265358979323846;
static constexpr int    CTRL = 32;          // control-rate block, in samples
// The level the choke's makeup is referenced to: a typical post-drive output.
static constexpr float  CHOKE_NOMINAL = 0.22f;

//==============================================================================
// RBJ cookbook, normalised by a0.
void Biquad::setLP (float f, double sr, float q)
{
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) (((1.0 - c) * 0.5) / a0);
    b1 = (float) ((1.0 - c) / a0);
    b2 = b0;
    a1 = (float) ((-2.0 * c) / a0);
    a2 = (float) ((1.0 - al) / a0);
}

void Biquad::setHP (float f, double sr, float q)
{
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) (((1.0 + c) * 0.5) / a0);
    b1 = (float) ((-(1.0 + c)) / a0);
    b2 = b0;
    a1 = (float) ((-2.0 * c) / a0);
    a2 = (float) ((1.0 - al) / a0);
}

void Biquad::setPeak (float f, double sr, float q, float dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al / A;
    b0 = (float) ((1.0 + al * A) / a0);
    b1 = (float) ((-2.0 * c) / a0);
    b2 = (float) ((1.0 - al * A) / a0);
    a1 = b1;
    a2 = (float) ((1.0 - al / A) / a0);
}

void Biquad::setLowShelf (float f, double sr, float q, float dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w);
    const double al = s / (2.0 * q);
    const double sq = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1.0) + (A - 1.0) * c + sq;
    b0 = (float) ((A * ((A + 1.0) - (A - 1.0) * c + sq)) / a0);
    b1 = (float) ((2.0 * A * ((A - 1.0) - (A + 1.0) * c)) / a0);
    b2 = (float) ((A * ((A + 1.0) - (A - 1.0) * c - sq)) / a0);
    a1 = (float) ((-2.0 * ((A - 1.0) + (A + 1.0) * c)) / a0);
    a2 = (float) (((A + 1.0) + (A - 1.0) * c - sq) / a0);
}

void Biquad::setHighShelf (float f, double sr, float q, float dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w);
    const double al = s / (2.0 * q);
    const double sq = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1.0) - (A - 1.0) * c + sq;
    b0 = (float) ((A * ((A + 1.0) + (A - 1.0) * c + sq)) / a0);
    b1 = (float) ((-2.0 * A * ((A - 1.0) + (A + 1.0) * c)) / a0);
    b2 = (float) ((A * ((A + 1.0) + (A - 1.0) * c - sq)) / a0);
    a1 = (float) ((2.0 * ((A - 1.0) - (A + 1.0) * c)) / a0);
    a2 = (float) (((A + 1.0) - (A - 1.0) * c - sq) / a0);
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
        const double s = (t == 0.0) ? 0.5 : std::sin (PI * 0.5 * t) / (PI * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * PI * n / (TAPS - 1))
                              + 0.08 * std::cos (4.0 * PI * n / (TAPS - 1));
        g[(size_t) n] = s * w;
        sum += g[(size_t) n];
    }
    for (auto& v : g) v /= sum;                 // unity at DC

    for (int k = 0; k < PH; ++k)
    {
        ge[(size_t) k] = (2 * k     < TAPS) ? (float) g[(size_t) (2 * k)]     : 0.0f;
        go[(size_t) k] = (2 * k + 1 < TAPS) ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
    }
}

//==============================================================================
namespace
{
    struct EngineDef
    {
        const char* name;
        const char* blurb;
        float driveMax;     // how much gain the THRUST knob reaches on this engine
        float character;    // this engine's fixed character, baked in
        float rabid;        // 0..1, how hard it burns fuel and how wild it sounds
        float trim;         // output match; MEASURED by the bench, not modelled
    };

    // Ported from Martian Gain's shaper bank, retuned for a single-knob pedal.
    // Ordered so the knob travels from polite to unhinged, which is what the
    // metal promises by putting them on one pot.
    const EngineDef ENGINES[NUM_ENGINES] =
    {
        { "IDLE BURN",   "soft asymmetric tube",      10.0f, 0.55f, 0.10f, 1.00f },
        { "ION DRIVE",   "the classic soft knee",     20.0f, 0.35f, 0.25f, 1.00f },
        { "PLASMA COIL", "tape saturation, memory",   14.0f, 0.45f, 0.35f, 1.00f },
        { "AFTERBURNER", "asymmetric fuzz, squashed", 26.0f, 0.60f, 0.55f, 1.00f },
        { "RAZOR WING",  "tanh into hard clip",       24.0f, 0.70f, 0.65f, 1.00f },
        { "WARP FOLD",   "wavefolder",                11.0f, 0.40f, 0.80f, 1.00f },
        { "HYPERDRIVE",  "harmonic injection",         8.0f, 0.45f, 0.90f, 1.00f },
        { "SUPERNOVA",   "fold, crush and chaos",     14.0f, 0.50f, 1.00f, 1.00f }
    };
}

/*  Whole note = 4 quarter notes, so 1/4 is 1.0 here. The triplet and dotted
    variants are the usual 2/3 and 3/2 of their straight value. */
const float SYNC_BEATS[NUM_SYNC] = { 0.0f, 4.0f, 2.0f, 1.0f, 2.0f/3.0f, 0.75f, 0.5f, 0.25f };
const char* SYNC_NAMES = "FREE|1/1|1/2|1/4|1/4T|1/8D|1/8|1/16";

const char* engineName  (int e) { return ENGINES[(size_t) clampf ((float) e, 0.0f, NUM_ENGINES - 1.0f)].name; }
const char* engineBlurb (int e) { return ENGINES[(size_t) clampf ((float) e, 0.0f, NUM_ENGINES - 1.0f)].blurb; }
float engineRabid (int e)       { return ENGINES[(size_t) clampf ((float) e, 0.0f, NUM_ENGINES - 1.0f)].rabid; }

//==============================================================================
/*  fuelSag is 1.0 at a full tank. It does NOT simply turn the level down: it
    takes a little drive away and a lot of output, which is what a dying battery
    does to a fuzz - more squashed, thinner, further away. At sag == 1 both
    factors are exactly 1.0, an IEEE-exact multiply, so a full tank is the same
    arithmetic as no fuel model at all. */
ShapeCo engineCoeffs (int engine, float thrust, float fuelSag)
{
    const EngineDef& e = ENGINES[(size_t) clampf ((float) engine, 0.0f, NUM_ENGINES - 1.0f)];
    const float chr = e.character;
    const float drive = clampf (thrust, 0.0f, 1.0f);

    ShapeCo c;
    c.trim = e.trim;

    switch (engine)
    {
        case E_IDLE:
            c.k = 1.0f + drive * e.driveMax;
            c.a = chr;
            break;

        case E_ION:
            c.k = 1.0f + drive * e.driveMax;
            c.a = chr;
            break;

        case E_PLASMA:
            c.k = 1.0f + drive * e.driveMax;
            c.a = 0.10f + (1.0f - chr) * 0.85f;
            c.b = chr * 0.6f;
            break;

        case E_AFTERBURNER:
            c.k = 1.0f + drive * e.driveMax;
            c.a = 0.20f + chr * 0.55f;
            c.b = 0.90f;
            break;

        case E_RAZOR:
            c.k = 1.0f + drive * e.driveMax;
            c.a = chr;
            break;

        case E_WARPFOLD:
            c.k = 1.0f + drive * e.driveMax;
            c.a = chr * 0.9f;
            break;

        case E_HYPERDRIVE:
            c.k = 1.0f + drive * e.driveMax;
            c.a = chr;
            break;

        case E_SUPERNOVA:
            c.k = 1.0f + drive * e.driveMax;
            c.a = chr;
            break;

        default: break;
    }

    c.k    *= 0.75f + 0.25f * fuelSag;
    c.trim *= 0.35f + 0.65f * fuelSag;
    return c;
}

/*  Measure each engine's own output level over the drive range, by running its
    shaper on a reference tone. Done at prepare, so it can never drift from the
    shaper it is compensating - a hardcoded table goes stale the first time a
    curve is retuned, silently. */
void Engine::buildTrimTable()
{
    constexpr int N = 2048;
    // THREE reference amplitudes, averaged in the log domain. One cannot be right
    // for a saturator - that is the whole reason a single-point calibration
    // fails - and averaging two is what lets the signal-derived auto-gain be
    // deleted rather than merely slowed.
    const float REF[3] = { 0.12f, 0.28f, 0.50f };

    for (int e = 0; e < NUM_ENGINES; ++e)
    {
        for (int d = 0; d < TRIM_PTS; ++d)
        {
            const float drive = (float) d / (float) (TRIM_PTS - 1);
            ShapeCo c = engineCoeffs (e, drive, 1.0f);
            c.trim = 1.0f;

            double logSum = 0.0;
            for (int a = 0; a < 3; ++a)
            {
                ShaperState st;
                double acc = 0.0;
                int counted = 0;
                for (int n = 0; n < N; ++n)
                {
                    const float x = REF[a] * std::sin (2.0f * (float) PI * 220.0f * n / 48000.0f);
                    const float y = shapeSample (e, x, c, st);
                    if (n >= N / 2) { acc += (double) y * y; ++counted; }   // let the state settle
                }
                const double r = std::sqrt (acc / std::max (1, counted));
                const double want = REF[a] * 0.70710678 / std::max (1.0e-5, r);
                logSum += std::log (std::max (1.0e-4, want));
            }
            const float g = (float) std::exp (logSum / 3.0);
            trimTable[(size_t) e][(size_t) d] = clampf (g, 0.02f, 8.0f);
        }
    }
}

float Engine::trimFor (int engine, float thrust) const
{
    const int e = (int) clampf ((float) engine, 0.0f, NUM_ENGINES - 1.0f);
    const float t = clampf (thrust, 0.0f, 1.0f) * (TRIM_PTS - 1);
    const int i = std::min ((int) t, TRIM_PTS - 2);
    return lerpf (trimTable[(size_t) e][(size_t) i], trimTable[(size_t) e][(size_t) (i + 1)], t - (float) i);
}

float shapeSample (int engine, float x, const ShapeCo& co, ShaperState& st)
{
    const float u = x * co.k;

    switch (engine)
    {
        case E_IDLE:
        {
            // A triode conducts differently either side of zero. Even at zero
            // bias this is lopsided, which is where the second harmonic comes
            // from and why it flatters almost anything.
            const float t = (u >= 0.0f) ? std::tanh (u)
                                        : std::tanh (u * co.a) * (0.6f + 0.4f * co.a);
            const float c = clampf (u, -1.0f, 1.0f);
            const float cub = 1.5f * (c - c * c * c / 3.0f);
            return lerpf (cub, t, 0.7f);
        }

        case E_ION:
        {
            const float soft = u / (1.0f + std::abs (u));
            return lerpf (soft * 1.6f, clampf (u, -1.0f, 1.0f), co.a);
        }

        case E_PLASMA:
        {
            const float t = std::tanh (u);
            st.tapeMem = flushDenorm (st.tapeMem + (t - st.tapeMem) * co.a);
            return lerpf (t, st.tapeMem, co.b);
        }

        case E_AFTERBURNER:
            // Different clip levels either way up: the ugliest kind of honest.
            return clampf (u, -co.b, co.a) / co.a;

        case E_RAZOR:
            return lerpf (std::tanh (u), clampf (u, -1.0f, 1.0f), co.a);

        case E_WARPFOLD:
            return wavefold (u + co.a) - wavefold (co.a);

        case E_HYPERDRIVE:
        {
            // Straight harmonic injection: the character slides the emphasis
            // from the second partial up to the fifth.
            const float c  = clampf (u, -1.0f, 1.0f);
            const float c2 = c * c;
            // The EVEN Chebyshev polynomials are non-zero at the origin -
            // T2(0) = -1, T4(0) = +1 - so used raw they emit a DC step and the
            // engine is not silent on a silent input. Each is offset by its own
            // value at zero, which removes the DC and leaves the harmonics
            // exactly as they were.
            const float t2 = 2.0f * c2;
            const float t3 = c * (4.0f * c2 - 3.0f);
            const float t4 = 8.0f * c2 * c2 - 8.0f * c2;
            const float t5 = c * (16.0f * c2 * c2 - 20.0f * c2 + 5.0f);
            const float s  = co.a * 3.0f;
            const float w2 = std::max (0.0f, 1.0f - std::abs (s));
            const float w3 = std::max (0.0f, 1.0f - std::abs (s - 1.0f));
            const float w4 = std::max (0.0f, 1.0f - std::abs (s - 2.0f));
            const float w5 = std::max (0.0f, 1.0f - std::abs (s - 3.0f));
            const float n  = w2 + w3 + w4 + w5 + 1.0e-6f;
            return c * 0.35f + 0.9f * (w2 * t2 + w3 * t3 + w4 * t4 + w5 * t5) / n;
        }

        case E_SUPERNOVA:
        {
            float y = wavefold (u);
            y = std::floor (y * 8.0f + 0.5f) * 0.125f;
            y = clampf (y * 1.6f, -1.0f, 1.0f);
            return y * (1.0f - co.a * 0.5f) + co.a * 0.5f * std::sin (11.0f * y);
        }

        default: break;
    }
    return x;
}

//==============================================================================
namespace
{
    // Mutually prime-ish line lengths at 48k, scaled to the running rate.
    const int HALL_LEN[Hall::N] = { 1237, 1381, 1607, 1811, 2053, 2273, 2521, 2803 };
    constexpr int SHIFT_LEN = 4096;     // the octave-up window, in samples
}

void Hall::prepare (double sr)
{
    const double k = sr / 48000.0;
    for (int i = 0; i < N; ++i)
    {
        len[(size_t) i] = (float) std::max (8.0, HALL_LEN[i] * k);
        line[(size_t) i].setMaxSamples ((int) len[(size_t) i] + 8);
        damp[(size_t) i].setHz (4200.0f, sr);
    }
    shiftBuf.setMaxSamples (SHIFT_LEN + 8);
    clear();
}

void Hall::clear()
{
    for (auto& l : line) l.clear();
    for (auto& d : damp) d.clear();
    shiftBuf.clear();
    shiftPhase = 0.0f;
    shiftWin = 0.0f;
    dcL.clear(); dcR.clear();
}

/*  FDN-8 with a Householder feedback matrix (orthogonal, so it cannot add
    energy however long the tail is set), damped in the loop, with an octave-up
    path folded back into the input - the arrangement already proven in Blade
    Ruiner and BWFX SHIMMER. */
void Hall::process (float inL, float inR, float rt60, float damping,
                    float shimmer, double sr, float& outL, float& outR)
{
    std::array<float, N> v {};
    for (int i = 0; i < N; ++i)
        v[(size_t) i] = line[(size_t) i].readFrac (len[(size_t) i]);

    // outputs: even lines left, odd lines right
    float oL = 0.0f, oR = 0.0f;
    for (int i = 0; i < N; i += 2) { oL += v[(size_t) i]; oR += v[(size_t) (i + 1)]; }
    oL *= 0.35f; oR *= 0.35f;

    // --- octave up, from the hall's own output ---------------------------
    float shim = 0.0f;
    if (shimmer > 0.0f)
    {
        shiftBuf.write ((oL + oR) * 0.5f);
        // Two taps whose delay falls at one sample per sample read the buffer
        // at double rate; they are half a window apart so one is at full
        // crossfade weight while the other jumps.
        const float L = (float) SHIFT_LEN;
        const float pA = shiftPhase;
        const float pB = std::fmod (shiftPhase + L * 0.5f, L);
        const float a = shiftBuf.readFrac (L - pA + 2.0f);
        const float b = shiftBuf.readFrac (L - pB + 2.0f);
        const float wA = (float) std::sin (PI * pA / L); const float wA2 = wA * wA;
        shim = a * wA2 + b * (1.0f - wA2);
        shiftPhase += 1.0f;
        if (shiftPhase >= L) shiftPhase -= L;
    }

    // --- feedback --------------------------------------------------------
    float sum = 0.0f;
    for (int i = 0; i < N; ++i) sum += v[(size_t) i];
    const float h = 2.0f * sum / (float) N;

    const float rt = std::max (0.05f, rt60);
    const float inject = (inL + inR) * 0.25f;

    /*  THE OCTAVE-UP GOES IN BEFORE THE DAMPING, NOT AFTER IT.

        Injected after the filter, as it was, the shifted signal was the one
        thing in the loop that nothing ever attenuated: every pass it climbed
        another octave at full gain, and with the hall already at a 6 s decay
        the total loop gain passed unity. Measured NOT DECAYING across
        SPACE 0.60-0.90 - exactly the band where the shimmer is up - and it
        passed every bounds check while doing it, because the output ceiling
        keeps a runaway neatly inside full scale. "Bounded" is not "stable".

        Injected before the filter, the climb is self-limiting and physical:
        each octave up is further into the damping's stopband, so after a few
        recirculations the shifted energy is gone. That is why a real shimmer
        does not run away either. The main feedback is untouched - blending
        into it instead costs (1 - sm) of loop gain EVERY pass, which over the
        hundreds of passes in a long tail removes the tail entirely (measured:
        -84 dB where it should be lush).  */
    /*  ...and its level is BUDGETED against what the decay time has left.

        The two paths recirculate independently, so their energies add: the
        loop is stable only while g^2 + L^2 < 1, where L is the shimmer path's
        own gain. At a 6 s decay g is about 0.953 per pass, which leaves
        sqrt(1 - 0.908) = 0.30 for everything else - and the fixed injection
        was using 0.59, about twice what was available. A constant that happens
        to be stable at one decay time is not a design; this one is derived
        from the decay time, so it holds across the whole SPACE sweep.

        The 1.4 is the shimmer path's own round trip: `shim` sums four lines
        and is fed back into all eight. 0.75 keeps a quarter in hand.  */
    const float shimAmt = clampf (shimmer, 0.0f, 1.0f);

    /*  ...and the hall shortens to pay for it.

        The two paths recirculate independently and in the worst phase their
        gains ADD, so the guaranteed bound is g * (1 + L) < 1 with L the
        shimmer path's own gain. At a 6 s decay g is 0.95 per pass and that
        leaves almost nothing - budgeting on ENERGY instead (g^2 + L^2 < 1)
        measured as marginally unstable: the tail fell to -45 dB and then grew
        back to -14 dB over the following seconds.

        So the decay time itself gives way when the shimmer comes up. That is
        not a dodge - it is what every shimmer unit does, because a six second
        hall simply has no loop gain left to lend. The octave is paid for in
        tail length, and both numbers are derived here rather than typed, so
        the relationship holds across the whole sweep.

        The 1.4 is the shimmer path's round trip: `shim` sums four lines and is
        fed back into all eight. 0.8 keeps a fifth in hand.  */
    const float rtEff = rt / (1.0f + 1.1f * shimAmt);

    float lenAvg = 0.0f;
    for (int i = 0; i < N; ++i) lenAvg += len[(size_t) i];
    lenAvg /= (float) N;
    const float gTyp = std::pow (10.0f, -3.0f * lenAvg / (rtEff * (float) sr));
    const float sMax = 0.8f * (1.0f / std::max (0.05f, gTyp) - 1.0f) / 1.4f;
    const float shimInject = shim * shimAmt * std::min (0.42f, sMax);

    for (int i = 0; i < N; ++i)
    {
        // per line: g so that the line decays by 60 dB in rt60 seconds
        const float g = std::pow (10.0f, -3.0f * len[(size_t) i] / (rtEff * (float) sr));
        // The shifted signal is in the same room, so it decays with everything
        // else (inside g) and is filtered with everything else (before damp).
        float x = (v[(size_t) i] - h + shimInject) * g;
        x = damp[(size_t) i].process (x);
        x += (i & 1) ? inject : -inject;
        // A belt-and-braces ceiling inside the loop, as BWFX SHIMMER carries:
        // whatever else goes wrong, a line cannot run away.
        line[(size_t) i].write (flushDenorm (ceilSoft (x, 4.0f)));
    }

    (void) damping;
    outL = dcL (oL);
    outR = dcR (oR);
}

//==============================================================================
Engine::Engine()
{
    rng.s = 0x9e3779b9u;
}

void Engine::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate > 8000.0 ? sampleRate : 48000.0;

    for (auto& ch : hb) for (auto& stage : ch) stage.design();

    const int maxTapeSamples = (int) (0.75 * sr) + 64;
    for (auto& d : tape) d.setMaxSamples (maxTapeSamples);
    {
        const int base[NAP] = { 47, 89, 131, 191, 257 };
        const float coef[NAP] = { 0.62f, 0.55f, 0.48f, 0.41f, 0.35f };
        for (int k = 0; k < NAP; ++k)
            widthAp[(size_t) k].setup ((int) (base[k] * sr / 48000.0), coef[k]);
    }
    widthComb.setMaxSamples ((int) (0.020 * sr) + 64);
    sideShelf.setHighShelf (2600.0f, sr, 0.70f, 5.0f);
    for (auto& d : vib)  d.setMaxSamples ((int) (0.012 * sr) + 64);
    for (auto& t : tapeLp) t.setHz (5200.0f, sr);
    for (auto& c : chokeLp) c.setHz (20000.0f, sr);
    for (auto& t : tremSplit) t.set (800.0f, sr);
    hall.prepare (sr);

    // --- measure the oversampler's own delay -----------------------------
    // The wet path is late by the half-band cascade's group delay, and the dry
    // path has to be delayed by the same amount or a mid MIX combs. Measured
    // with an impulse rather than derived, because a derivation that is wrong
    // is silent.
    {
        std::array<HalfBand, 2> probe;
        for (auto& s : probe) { s.design(); s.clear(); }
        const int N = 512;
        std::vector<float> imp ((size_t) N, 0.0f);
        int best = 0; float bestV = -1.0f;
        for (int n = 0; n < N; ++n)
        {
            const float x = (n == 0) ? 1.0f : 0.0f;
            float u0, u1, s0, s1, s2, s3;
            probe[0].up (x, u0, u1);
            probe[1].up (u0, s0, s1);
            probe[1].up (u1, s2, s3);
            const float d0 = probe[1].down (s0, s1);
            const float d1 = probe[1].down (s2, s3);
            const float y = probe[0].down (d0, d1);
            imp[(size_t) n] = y;
            if (std::abs (y) > bestV) { bestV = std::abs (y); best = n; }
        }
        latency = best;
    }

    for (auto& d : dry) d.setMaxSamples (latency + 64);

    buildTrimTable();

    reset();
}

void Engine::reset()
{
    for (auto& ch : hb) for (auto& s : ch) s.clear();
    for (auto& s : shaper) s.clear();
    for (auto& d : dcPre)  d.clear();
    for (auto& d : dcPost) d.clear();
    for (auto& a : widthAp) a.clear();
    widthComb.clear();
    sideShelf.clear();
    for (auto& d : tape) d.clear();
    for (auto& d : vib)  d.clear();
    for (auto& d : dry)  d.clear();
    for (auto& t : tapeLp) t.clear();
    for (auto& c : chokeLp) c.clear();
    for (auto& b : specMid)  b.clear();
    for (auto& b : specLo)   b.clear();
    for (auto& b : specHi)   b.clear();
    for (auto& b : specRoll) b.clear();
    for (auto& t : tremSplit) t.clear();
    hall.clear();

    tapeFbZ = { 0.0f, 0.0f };
    chokeEnv = 0.0f;
    widthDelaySm = 0.0f;
    meterPeak = 0.0f;
    wowPhase = wowVal = wowTarget = 0.0f;
    tremPhase = 0.0f;
    fuel = 1.0f;
    sagSm = 1.0f;
    misfireGate = 1.0f;
    misfireHold = 0.0f;
    rmsIn = 0.0f;
    fizzleT = 0.0f; emptyLatched = false; refilling = false;
    meterIn = meterOut = meterDrive = 0.0f;
    dlTimeMs = dlTimeSm = 30.0f;
    syncWasOn = false;
    first = true;
}

//==============================================================================
namespace
{
    inline float smoothstep (float t)
    {
        t = clampf (t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    /*  A stage's weight as the SPACE knob passes through it. */
    inline float ramp (float p, float a, float b)
    {
        return smoothstep ((p - a) / std::max (1.0e-6f, b - a));
    }
}

/*  SPECTRUM, the Orange Thunderverb Shape law: one knob sweeping the whole
    spectrum with the mids moving against the two ends.

      fully CCW   mid-focused and present, highs rolled off, bass tucked
      noon        close to flat, the mids just beginning to dip
      fully CW    mids scooped, bass and treble both boosted

    The gains are interlocked on one parameter on purpose - that is what makes
    it a Shape control rather than three tone knobs in a trench coat. */
float Engine::spectrumGainNorm (float s) const
{
    // Broadband compensation across the sweep, so the knob changes tone and not
    // loudness - an EQ that gets louder as you turn it always sounds "better"
    // for the wrong reason. Weights approximate where a guitar keeps its energy;
    // the bench measures the residual and it is held under a stated tolerance.
    const float midDb = lerpf (  7.0f, -12.0f, s);
    const float loDb  = lerpf ( -5.0f,   8.0f, s);
    const float hiDb  = lerpf ( -8.0f,   7.0f, s);
    const float wDb = 0.46f * loDb + 0.38f * midDb + 0.16f * hiDb;
    return std::pow (10.0f, -wDb / 20.0f);
}

void Engine::updateDerived()
{
    // --- SPECTRUM --------------------------------------------------------
    const float s = sSpec;
    const float midDb = lerpf (  7.0f, -12.0f, s);
    const float loDb  = lerpf ( -5.0f,   8.0f, s);
    const float hiDb  = lerpf ( -8.0f,   7.0f, s);
    // The treble rolloff only exists in the bottom third of the sweep; above
    // that it is parked out of the way rather than switched, so there is no
    // step in the response as the knob passes.
    const float rollHz = 4000.0f * std::pow (5.0f, clampf (s / 0.35f, 0.0f, 1.0f));

    for (int c = 0; c < 2; ++c)
    {
        specMid[(size_t) c].setPeak (650.0f, sr, 0.70f, midDb);
        specLo [(size_t) c].setLowShelf (150.0f, sr, 0.70f, loDb);
        specHi [(size_t) c].setHighShelf (3200.0f, sr, 0.70f, hiDb);
        specRoll[(size_t) c].setLP (rollHz, sr, 0.70f);
    }
}

//==============================================================================
void Engine::process (float* left, float* right, int numSamples)
{
    p = pending;

    if (first)
    {
        sMix = p.mix; sThrust = p.thrust; sAnti = p.antithrust;
        sSpace = p.space; sSpec = p.spectrum;
        updateDerived();
        first = false;
    }

    const float ctrlDt = (float) (CTRL / sr);
    const float smoothA = 1.0f - std::exp (- (float) (CTRL / (0.020 * sr)));   // 20 ms

    int ctrl = 0;
    ShapeCo localCo = engineCoeffs (p.engine, sThrust, sagSm);

    // Values recomputed each control tick.
    float widthAmt = 0.0f, widthDelayTarget = 0.0f, fxWet = 1.0f;
    float chokeDepth = 0.0f, chokeHz = 20000.0f;
    float tapeAmt = 0.0f, hallAmt = 0.0f, shimAmt = 0.0f, tremAmt = 0.0f, vibAmt = 0.0f;
    float dlFb = 0.0f, hallRt = 1.0f, specNorm = 1.0f;
    float fuelLpHz = 20000.0f;
    bool  spaceOn = false, antiOn = false;

    for (int n = 0; n < numSamples; ++n)
    {
        //--------------------------------------------------------------- control
        if (ctrl == 0)
        {
            sMix    += (p.mix        - sMix)    * smoothA;
            sThrust += (p.thrust     - sThrust) * smoothA;
            sAnti   += (p.antithrust - sAnti)   * smoothA;
            sSpace  += (p.space      - sSpace)  * smoothA;

            if (std::abs (p.spectrum - sSpec) > 1.0e-5f)
            {
                sSpec += (p.spectrum - sSpec) * smoothA;
                updateDerived();
            }
            specNorm = spectrumGainNorm (sSpec);

            antiOn  = sAnti  > 1.0e-4f;

            /*  Overall FX level, BIPOLAR about the centre: 0.5 is exactly
                1.0 - an IEEE-exact multiply, so a patch that never touches
                it is bit-identical - 0 turns every SPACE effect off, and 1
                goes half again beyond today. */
            fxWet = (p.spaceWet <= 0.5f) ? p.spaceWet * 2.0f
                                         : 1.0f + (p.spaceWet - 0.5f) * 1.6f;
            spaceOn = sSpace > 1.0e-4f;

            // --- ANTITHRUST ------------------------------------------------
            if (antiOn)
            {
                // WIDTH. The knob adds decorrelated side derived from the mid;
                // the delay shortens as it rises so the image opens from a
                // deep, slow room towards a tight, bright spread.
                /*  Capped so the level rise stays modest. Side energy adds
                    to total energy - that is unavoidable: the only way to
                    hold total level constant while widening is to pull the
                    MID down, and pulling the mid down is precisely what
                    moves the mono sum. Mono-exact and level-exact cannot
                    both hold, so this keeps mono exact and keeps the rise
                    small enough to read as bigger rather than louder. */
                widthAmt = 0.95f * sAnti;
                widthDelayTarget = lerpf (14.0f, 3.0f, sAnti) * 0.001f * (float) sr;
                chokeDepth = sAnti * sAnti;
                // A gentle top-end tilt, not a real loss. Closing hard would
                // need level compensation, and compensating a
                // frequency-dependent loss with a flat gain boosts the lows
                // and moves the mono sum - so it is easier not to lose it.
                chokeHz = lerpf (20000.0f, 11000.0f, sAnti * sAnti);
                for (auto& c : chokeLp) c.setHz (chokeHz, sr);
            }
            else
            {
                widthAmt = chokeDepth = 0.0f;
            }

            // --- SPACE -----------------------------------------------------
            if (spaceOn)
            {
                const float q = sSpace;
                tapeAmt = ramp (q, 0.00f, 0.10f) * (1.0f - ramp (q, 0.33f, 0.48f));
                hallAmt = ramp (q, 0.28f, 0.42f) * (1.0f - ramp (q, 0.80f, 0.94f));
                shimAmt = ramp (q, 0.50f, 0.68f) * (1.0f - ramp (q, 0.88f, 0.98f));
                tremAmt = ramp (q, 0.70f, 0.86f);
                vibAmt  = ramp (q, 0.90f, 1.00f);

                // 30 ms out to 600 and back again, so the knob passes THROUGH
                // the useful delay range instead of ending on it.
                const float u = clampf (q / 0.45f, 0.0f, 1.0f);
                dlTimeMs = 30.0f + 570.0f * std::sin ((float) PI * u);

                /*  SPACE SYNC: the delay locks to the host instead of sweeping.
                    FREE, or no clock, leaves the line above untouched - so the
                    default path is bit-identical to a build without any of
                    this. Engaging sync SNAPS the smoother rather than gliding
                    into the new time: a glide lands the first echo early, which
                    Black Rider shipped once and had to be fixed. */
                if (p.spaceSync > SYNC_FREE && p.bpm > 1.0)
                {
                    const float beats = SYNC_BEATS[(size_t) std::min (p.spaceSync, NUM_SYNC - 1)];
                    const float ms = (float) (beats * 60000.0 / p.bpm);
                    dlTimeMs = clampf (ms, 5.0f, 700.0f);
                    if (! syncWasOn || std::abs (dlTimeMs - dlTimeSm) > 40.0f)
                        dlTimeSm = dlTimeMs;
                    syncWasOn = true;
                }
                else syncWasOn = false;
                dlFb = 0.50f * ramp (q, 0.00f, 0.35f);
                hallRt = 0.8f + 8.2f * ramp (q, 0.28f, 0.72f);
            }
            else
            {
                tapeAmt = hallAmt = shimAmt = tremAmt = vibAmt = 0.0f;
                dlFb = 0.0f;
            }

            // --- fuel ------------------------------------------------------
            // ONE gauge step every five seconds OF PLAYING. Not load-dependent,
            // not clever: a deterministic countdown you can watch, which is
            // what makes the gag land. Silence costs nothing.
            {
                const bool playing = rmsIn > 0.0025f;          // about -52 dBFS

                if (refilling)
                {
                    fuel += ctrlDt / 0.6f;                     // visibly fills, ~0.6 s
                    if (fuel >= 1.0f) { fuel = 1.0f; refilling = false; }
                }
                else if (p.autorefill)
                {
                    // Topped up on reaching next-to-bottom, so with the button
                    // lit the tank never actually runs dry.
                    if (fuel <= REFILL_AT) refilling = true;
                    else if (playing) fuel -= ctrlDt / (SECS_PER_STEP * FUEL_STEPS);
                }
                else if (playing)
                {
                    fuel -= ctrlDt / (SECS_PER_STEP * FUEL_STEPS);
                }
                fuel = clampf (fuel, 0.0f, 1.0f);

                // Pressing AUTOREFILL is the way back from empty.
                if (p.autorefill && emptyLatched) { emptyLatched = false; fizzleT = 0.0f; refilling = true; }

                if (fuel <= 0.0f && ! p.autorefill)
                {
                    emptyLatched = true;
                    fizzleT = std::min (FIZZLE_SECS, fizzleT + ctrlDt);
                }
                else if (fuel > 0.0f)
                {
                    emptyLatched = false;
                    fizzleT = 0.0f;
                }
            }

            // How much engine is left. Deliberately EXACTLY 1.0 for most of the
            // tank - the sag is a warning in the last step, not a slow fade
            // across the whole seventy seconds - and then the five-second
            // fizzle takes it the rest of the way to nothing.
            //
            // The near-empty term bottoms out at 0.55 rather than 0, so the
            // fizzle has somewhere to fall FROM. Letting it reach zero at the
            // moment the tank runs dry made the five seconds unreachable: the
            // engine was already silent before the fizzle started, and the
            // whole gag measured as 2.4 dB of fade.
            {
                const float low = 0.55f + 0.45f * smoothstep (fuel / 0.11f);   // 1.0 above 11 %
                const float fizz = emptyLatched
                                 ? (1.0f - clampf (fizzleT / FIZZLE_SECS, 0.0f, 1.0f))
                                 : 1.0f;
                const float target = low * fizz;
                sagSm += (target - sagSm) * 0.08f;
                if (target >= 1.0f && sagSm > 0.99995f) sagSm = 1.0f;  // exact identity on a full tank
            }

            // The engine coughs on the way out, harder as it dies.
            {
                const float miss = 1.0f - sagSm;
                if (misfireHold > 0.0f)
                {
                    misfireHold -= ctrlDt;
                    misfireGate += (0.10f - misfireGate) * 0.35f;
                }
                else
                {
                    misfireGate += (1.0f - misfireGate) * 0.18f;
                    if (miss > 0.04f && rng.uni() < miss * miss * 0.22f)
                        misfireHold = 0.025f + rng.uni() * 0.055f;
                }
                if (misfireGate > 0.99999f) misfireGate = 1.0f;
            }

            fuelLpHz = 1800.0f + 18000.0f * sagSm;
            localCo = engineCoeffs (p.engine, sThrust, sagSm);
            localCo.trim *= trimFor (p.engine, sThrust);
            meterDrive = clampf ((localCo.k - 1.0f) / 26.0f, 0.0f, 1.0f);

            // --- wow -------------------------------------------------------
            // A wander must never step: the random component is slewed, not
            // held. (Black Rider shipped the stepped version once and it read
            // as noise in the delay's feedback.)
            wowPhase += 0.6f * ctrlDt;
            if (wowPhase >= 1.0f) { wowPhase -= 1.0f; wowTarget = rng.bi(); }
            wowVal += (wowTarget - wowVal) * 0.04f;
        }
        if (++ctrl >= CTRL) ctrl = 0;

        //---------------------------------------------------------------- audio
        const float inL = left[n], inR = right[n];

        // input level, for the meters and the fuel model
        const float absIn = 0.5f * (std::abs (inL) + std::abs (inR));
        rmsIn += (absIn - rmsIn) * 0.0008f;
        meterIn += (absIn - meterIn) * 0.02f;

        // the dry path is delayed by the oversampler's own measured latency
        dry[0].write (inL); dry[1].write (inR);

        std::array<float, 2> y {};
        const float in[2] = { inL, inR };
        float chokeCand = 0.0f;

        for (int c = 0; c < 2; ++c)
        {
            float x = dcPre[(size_t) c] (in[(size_t) c]);

            // ANTITHRUST no longer touches the signal before the drive. Width
            // has to come AFTER it: distortion is nonlinear, so two slightly
            // different signals through two shapers make wildly different
            // harmonics and the image smears instead of widening.

            // --- the engine, at 4x -----------------------------------------
            float u0, u1, s0, s1, s2, s3;
            hb[(size_t) c][0].up (x, u0, u1);
            hb[(size_t) c][1].up (u0, s0, s1);
            hb[(size_t) c][1].up (u1, s2, s3);

            s0 = shapeSample (p.engine, s0, localCo, shaper[(size_t) c]);
            s1 = shapeSample (p.engine, s1, localCo, shaper[(size_t) c]);
            s2 = shapeSample (p.engine, s2, localCo, shaper[(size_t) c]);
            s3 = shapeSample (p.engine, s3, localCo, shaper[(size_t) c]);

            const float d0 = hb[(size_t) c][1].down (s0, s1);
            const float d1 = hb[(size_t) c][1].down (s2, s3);
            float v = hb[(size_t) c][0].down (d0, d1);

            v *= localCo.trim;

            // --- ANTITHRUST: the choke, after the drive --------------------
            // ONE envelope drives both channels, so the choke can never pull
            // the stereo image sideways - but it is fed by the LOUDER of the
            // two, or a hard hit landing only on the right would not choke at
            // all. The envelope is updated once both channels are known, so
            // this reads last sample's value; at these time constants one
            // sample of lag is nothing.
            if (antiOn)
            {
                // Detected BEFORE the gain reduction. Reading the choked output
                // instead makes it a feedback compressor: smoother, but it
                // backs its own detector off and the braking loses its depth.
                chokeCand = std::max (chokeCand, std::abs (v));
                // WITH MAKEUP. Without it this is a pure attenuator: it was
                // most of ANTITHRUST's 6.8 dB level loss, and a compressor is
                // supposed to reduce dynamic RANGE, not average level. The
                // makeup is a constant against a nominal level, NOT a tracker -
                // a tracker is the auto-gain that was deleted for breathing.
                const float makeup = 1.0f + chokeDepth * 6.0f * CHOKE_NOMINAL;
                const float gr = makeup / (1.0f + chokeDepth * 6.0f * chokeEnv);
                v = chokeLp[(size_t) c].process (v * gr);
            }

            // --- fuel sag --------------------------------------------------
            // sagSm is EXACTLY 1.0 above a tenth of a tank, so this is an
            // identity multiply in normal use - and it is what lets the engine
            // reach true silence at the end of the fizzle, which the trim term
            // inside engineCoeffs cannot do (it bottoms out at 0.35).
            v *= misfireGate * sagSm;

            // --- SPECTRUM --------------------------------------------------
            v = specRoll[(size_t) c].process (v);
            v = specMid [(size_t) c].process (v);
            v = specLo  [(size_t) c].process (v);
            v = specHi  [(size_t) c].process (v);
            v *= specNorm;

            v = dcPost[(size_t) c] (v);
            y[(size_t) c] = v;
        }

        if (antiOn)
        {
            const float k = (chokeCand > chokeEnv) ? 0.02f : 0.0009f;   // fast attack
            chokeEnv += (chokeCand - chokeEnv) * k;

            /*  --- WIDTH, in mid/side -----------------------------------
                The knob ADDS decorrelated side derived from the mid and never
                touches the mid, so L+R is exactly what it always was: no
                cancellation at any setting, on any source. That is what makes
                it mono-safe by construction rather than by tuning, and it is
                also why an incoming stereo image survives - the existing side
                passes through untouched and is added to, not replaced.

                The decorrelation is allpasses, not a comb: an allpass has a
                flat magnitude response, so it widens without leaving notches
                across the tone. A little comb is blended in for body.  */
            const float M = 0.5f * (y[0] + y[1]);
            const float S = 0.5f * (y[0] - y[1]);

            widthDelaySm += (widthDelayTarget - widthDelaySm) * 0.002f;
            float h = M;
            for (int k2 = 0; k2 < NAP; ++k2) h = widthAp[(size_t) k2].process (h);
            widthComb.write (M);
            h = h * 0.68f + widthComb.readFrac (std::max (1.0f, widthDelaySm)) * 0.32f;
            h = sideShelf.process (h);          // shaped: more side up top reads as depth

            /*  The mid goes through UNSCALED. That is the whole guarantee:
                L+R comes out as L+R went in, so nothing cancels when the
                track is summed, and the centre - which is the sound - does
                not move as the knob opens. An earlier version applied a
                broadband trim here to hold the total energy constant, and
                it dragged the mid down with it: -28.9 dB at full, far worse
                than the level drop it was meant to cure.  */
            const float sd = S + widthAmt * h;
            y[0] = M + sd;
            y[1] = M - sd;
        }

        // --- SPACE ---------------------------------------------------------
        if (spaceOn)
        {
            float wetL = 0.0f, wetR = 0.0f;

            if (tapeAmt > 0.0f)
            {
                dlTimeSm += (dlTimeMs - dlTimeSm) * 0.0006f;
                const float wow = 1.0f + wowVal * 0.0016f;
                const float dS = clampf (dlTimeSm * wow * 0.001f * (float) sr,
                                         2.0f, (float) (0.72 * sr));
                for (int c = 0; c < 2; ++c)
                {
                    // a little stereo offset, so the two taps are not one mono echo
                    const float off = (c == 0) ? 1.0f : 1.018f;
                    const float t = tape[(size_t) c].readFrac (dS * off);
                    float fb = tapeLp[(size_t) c].process (t * dlFb);
                    fb = std::tanh (fb * 1.2f) * 0.85f;    // the loop saturates, never runs away
                    tape[(size_t) c].write (flushDenorm (y[(size_t) c] + fb));
                    ((c == 0) ? wetL : wetR) += t * tapeAmt;
                }
            }
            else
            {
                for (int c = 0; c < 2; ++c) tape[(size_t) c].write (y[(size_t) c]);
            }

            if (hallAmt > 0.0f || shimAmt > 0.0f)
            {
                float hL = 0.0f, hR = 0.0f;
                hall.process (y[0], y[1], hallRt, 0.5f, shimAmt, sr, hL, hR);
                const float amt = std::max (hallAmt, shimAmt * 0.9f);
                wetL += hL * amt;
                wetR += hR * amt;
            }

            // MIXED, not summed. Adding the wet on top made the knob a level
            // control as well as a space control, and was most of why the
            // output ceiling was being reached in 116 of 300 random settings.
            // The stage weights above already crossfade delay against hall, so
            // they are normalised here into one unit-level wet and blended.
            {
                const float raw = tapeAmt + std::max (hallAmt, shimAmt * 0.9f);
                if (raw > 1.0e-6f)
                {
                    const float inv = 1.0f / raw;
                    const float w = clampf (0.62f * std::min (1.0f, raw) * fxWet,
                                            0.0f, 0.92f);
                    y[0] = y[0] * (1.0f - w) + wetL * inv * w;
                    y[1] = y[1] * (1.0f - w) + wetR * inv * w;
                }
            }

            // --- harmonic tremolo: the halves move in OPPOSITE phase -------
            // Guarded on the EFFECTIVE depth, not the stage weight. At zero
            // depth this block is not an identity: the Linkwitz-Riley split
            // and sum is an ALLPASS, so running it still phase-shifts the
            // signal. It has to be skipped, not merely turned down.
            if (tremAmt * fxWet > 1.0e-6f)
            {
                const float rate = 2.0f + 5.0f * tremAmt;
                tremPhase += (float) (rate / sr);
                if (tremPhase >= 1.0f) tremPhase -= 1.0f;
                const float m = std::sin (2.0f * (float) PI * tremPhase);
                const float d = 0.85f * clampf (tremAmt * fxWet, 0.0f, 1.0f);
                // Normalised: without this the modulated band peaks at 1+d, i.e.
                // +5.3 dB, and a signal living in that band simply gets louder.
                const float nrm = 1.0f / (1.0f + d * 0.62f);
                const float gLo = (1.0f + d * m) * nrm;
                const float gHi = (1.0f - d * m) * nrm;
                for (int c = 0; c < 2; ++c)
                {
                    float lo, hi;
                    tremSplit[c].process (y[(size_t) c], lo, hi);
                    y[(size_t) c] = lo * gLo + hi * gHi;
                }
            }

            // --- true-pitch vibrato, at the very top ----------------------
            // Likewise: at zero depth the vibrato still reads its line at the
            // base delay, so it would delay the signal by 4 ms for nothing.
            if (vibAmt * fxWet > 1.0e-6f)
            {
                const float m = std::sin (2.0f * (float) PI * tremPhase * 1.7f);
                const float base = 0.004f * (float) sr;
                const float dep = clampf (vibAmt * fxWet, 0.0f, 1.0f) * 0.0022f * (float) sr;
                for (int c = 0; c < 2; ++c)
                {
                    vib[(size_t) c].write (y[(size_t) c]);
                    const float wet = vib[(size_t) c].readFrac (base + dep * m);
                    y[(size_t) c] = lerpf (y[(size_t) c], wet, vibAmt);
                }
            }
        }

        // --- MIX -----------------------------------------------------------
        // Linear, not equal power: dry and wet here are correlated, and a
        // cos/sin crossfade of correlated signals sums to +3 dB at mid mix.
        // readInt(d) returns the sample d-1 back, because write() advances the
        // cursor past the sample it just stored - so the dry path needs one more.
        const float dL = dry[0].readInt (latency + 1);
        const float dR = dry[1].readInt (latency + 1);
        float oL = lerpf (dL, y[0], sMix);
        float oR = lerpf (dR, y[1], sMix);

        // A SOFT ceiling, last: exactly transparent below 70 % of full scale,
        // asymptotic above it, so nothing downstream ever sees more than full
        // scale and there is no hard corner to hear on the way there.
        oL = ceilSoft (oL, 1.0f);
        oR = ceilSoft (oR, 1.0f);

        left[n] = oL; right[n] = oR;

        const float absO = 0.5f * (std::abs (oL) + std::abs (oR));
        meterOut += (absO - meterOut) * 0.02f;

        // Peak, fast attack and slow release: overload is a peak event and is
        // invisible in an average, so the panel needs its own follower.
        const float pk = std::max (std::abs (oL), std::abs (oR));
        if (pk > meterPeak) meterPeak = pk; else meterPeak *= 0.99985f;
    }
}

} // namespace bo
