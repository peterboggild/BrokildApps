#include "Engine.h"

namespace ps
{

static inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline double clampd (double x, double a, double b) { return x < a ? a : (x > b ? b : x); }

/* Band-limited step: removes the discontinuity of a naive saw/pulse. */
static inline double polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

//==============================================================================
// Biquad — WebAudio spec: Q is in dB for low/highpass, linear otherwise;
// peaking gain in dB.
void Biquad::set (Type t, double freq, double Q, double gainDb, double fs)
{
    type = t;
    const double f = clampd (freq, 0.0, fs * 0.5);
    const double w = juce::MathConstants<double>::twoPi * f / fs;
    const double cw = std::cos (w), sw = std::sin (w);
    double b0n = 1, b1n = 0, b2n = 0, a0 = 1, a1n = 0, a2n = 0;

    switch (t)
    {
        case lowpass:
        {
            const double q = std::pow (10.0, Q / 20.0);
            const double alpha = sw / (2.0 * juce::jmax (1e-9, q));
            b0n = (1 - cw) / 2; b1n = 1 - cw; b2n = b0n;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case highpass:
        {
            const double q = std::pow (10.0, Q / 20.0);
            const double alpha = sw / (2.0 * juce::jmax (1e-9, q));
            b0n = (1 + cw) / 2; b1n = -(1 + cw); b2n = b0n;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case bandpass:
        {
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = alpha; b1n = 0; b2n = -alpha;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case notch:
        {
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = 1; b1n = -2 * cw; b2n = 1;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case allpass:
        {
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = 1 - alpha; b1n = -2 * cw; b2n = 1 + alpha;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case peaking:
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = 1 + alpha * A; b1n = -2 * cw; b2n = 1 - alpha * A;
            a0 = 1 + alpha / A; a1n = -2 * cw; a2n = 1 - alpha / A;
            break;
        }
    }
    b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
    a1 = a1n / a0; a2 = a2n / a0;
}

//==============================================================================
float Ladder::process (float x, float g, float k, int mode, float dk, float dnorm)
{
    const double G = g / (1.0 + g);
    const double S = (((z[0] * G + z[1]) * G + z[2]) * G + z[3]) / (1.0 + g);
    const double G4 = G * G * G * G;
    const double u = (x - k * S) / (1.0 + k * G4);
    const double inp = std::tanh (u * dk) * dnorm;
    double v, y1, y2, y3, y4;
    v = (inp - z[0]) * G; y1 = v + z[0]; z[0] = y1 + v;
    v = (y1 - z[1]) * G;  y2 = v + z[1]; z[1] = y2 + v;
    v = (y2 - z[2]) * G;  y3 = v + z[2]; z[2] = y3 + v;
    v = (y3 - z[3]) * G;  y4 = v + z[3]; z[3] = y4 + v;
    if (mode == 0) return (float) y4;
    if (mode == 1) return (float) (4 * y2 - 8 * y3 + 4 * y4);
    if (mode == 3) return (float) (inp - 2 * y1 + 2 * y2);
    return (float) (inp - 4 * y1 + 6 * y2 - 4 * y3 + y4);
}

//==============================================================================
// Voice — exact port of the "photo-synth-voice" AudioWorklet processor.
static constexpr int OS = 2;
static constexpr double OSC_GAIN = 0.75;
static constexpr double OUT_GAIN = 0.52;

void Voice::prepare (double sampleRate, uint32_t seed)
{
    fsOut = sampleRate;
    fs = sampleRate * OS;
    dcR = 1.0 - 6.2831853 * 5.0 / sampleRate;      // 5 Hz DC blocker
    rng = seed * 2654435761u + 1u;
    cbL.assign (8192, 0.0);
    cbR.assign (8192, 0.0);
    resetState();
    eventCount = 0;
    gainCurrent = gainTarget = 0;
    panCurrent = panTarget = 0;
}

void Voice::resetState()
{
    env = 0; gate = 0; stage = 0; pg = 0;
    ladL.reset(); ladR.reset();
    phA = 0; phB = 0.37; triA = triB = 0;
    dcxL = dcyL = dcxR = dcyR = 0;
}

void Voice::setField (int field, float v)
{
    switch (field)
    {
        case vfBase:     p.base = v; break;
        case vfRatio:    p.ratio = v; break;
        case vfMorph:    p.morph = v; break;
        case vfPulse:    p.pulse = v; break;
        case vfDetune:   p.detune = v; break;
        case vfLevel:    p.level = v; break;
        case vfCut:      p.cut = v; break;
        case vfRes:      p.res = v; break;
        case vfMode:     p.mode = (int) v; break;
        case vfDrive:    p.drive = v; break;
        case vfAttack:   p.attack = v; break;
        case vfDecay:    p.decay = v; break;
        case vfSustain:  p.sustain = v; break;
        case vfRelease:  p.release = v; break;
        case vfGlide:    p.glide = v; break;
        case vfSpread:   p.spread = v; break;
        case vfEnvAmp:   p.envAmp = v; break;
        case vfEnvFlt:   p.envFlt = v; break;
        case vfSub:      p.sub = v; break;
        case vfSubOct:   p.subOct = v; break;
        case vfEnvPitch: p.envPitch = v; break;
        case vfStop:     p.stop = v; break;
        case vfGate:     gate = v > 0.5f ? 1 : 0; break;
        case vfPan:      panTarget = v; break;
        case vfGain:     gainTarget = v; gainCurrent = v; break;
        case vfWave:     p.wave = (int) v; break;
        default: break;
    }
}

void Voice::addEvents (const std::vector<VoiceEvent>& evs)
{
    for (const auto& e : evs)
    {
        if (eventCount >= kMaxEvents) break;
        events[(size_t) eventCount++] = e;
    }
    std::stable_sort (events.begin(), events.begin() + eventCount,
                      [] (const VoiceEvent& a, const VoiceEvent& b) { return a.frame < b.frame; });
}

void Voice::applyEvent (const VoiceEvent& e)
{
    int vi = 0;
    for (int f = 0; f < 32 && vi < 8; ++f)
        if (e.fieldMask & (1u << f))
            setField (f, e.values[vi++]);
    if (e.gate >= 0)
        gate = e.gate;
}

float Voice::shape (double& tri, double ph, double dt, double pw)
{
    const double sine = std::sin (6.2831853 * ph);
    const double sq = (ph < pw ? 1.0 : -1.0) + polyBlep (ph, dt) - polyBlep (std::fmod (ph + 1.0 - pw, 1.0), dt);
    // Integrate the square about its OWN mean. A pulse of width pw carries a
    // DC term of (2*pw - 1), and pw is swept by the 0.11 Hz width LFO, so
    // feeding the raw square to the integrator made it accumulate a slow
    // offset — the whole waveform drifting off zero with a nine-second period,
    // at up to 80 % of the signal's own peak-to-peak. The AC shape, and so the
    // sound, is unchanged: only the term that had no business being there goes.
    tri += 4.0 * dt * (sq - (2.0 * pw - 1.0));
    tri *= 0.9995;
    const double saw = 2.0 * ph - 1.0 - polyBlep (ph, dt);

    // Fixed waveforms (Waveform select); 0 = the original photo-driven morph.
    switch (p.wave)
    {
        case 1: return (float) sine;
        case 2: return (float) tri;
        case 3: return (float) saw;
        case 4: return (float) (sq * 0.6);
        default: break;
    }

    const double m = clampd (morph, 0, 1);
    const double pulsed = sq * 0.6;
    const double bright = saw * (1.0 - pulse) + pulsed * pulse;
    return (float) (m < 0.5 ? sine + (tri - sine) * (m * 2.0)
                            : tri + (bright - tri) * ((m - 0.5) * 2.0));
}

void Voice::render (float* mixL, float* mixR, int n, int64_t startFrame)
{
    // pan smoothing (StereoPanner setTargetAtTime ~25 ms)
    if (panTau > 0)
        panCurrent += (panTarget - panCurrent) * (1.0f - std::exp ((float) (-(double) n / (0.025 * fsOut))));
    else
        panCurrent = panTarget;

    // Idle short-circuit, as in the worklet.
    if (! gate && env < 1e-5 && gl < 1e-4
        && (eventCount == 0 || events[0].frame >= startFrame + n))
    {
        if (++quiet > 8)
        {
            env = 0; stage = 0; pg = 0; gl = 0;
            f0 = p.base * p.ratio; morph = p.morph; pulse = p.pulse;
            detune = p.detune; level = p.level; cut = p.cut; res = p.res;
            return;                                    // exact silence, nothing mixed
        }
    }
    else
        quiet = 0;

    if (gainCurrent < 1e-6f && gainTarget < 1e-6f && quiet > 8)
        return;

    const double aSmooth = 1.0 - std::exp (-1.0 / (0.006 * fsOut));
    const double aGate   = 1.0 - std::exp (-1.0 / (0.004 * fsOut));

    // StereoPanner (stereo input law)
    const float pcl = clampf (panCurrent + wmPanAdd, -1.0f, 1.0f);
    const float px = pcl <= 0 ? pcl + 1.0f : pcl;
    const float pgL = std::cos (juce::MathConstants<float>::halfPi * px);
    const float pgR = std::sin (juce::MathConstants<float>::halfPi * px);

    for (int i = 0; i < n; ++i)
    {
        // 128-sample cadence: drift walk, vibrato LFO, drive curve — the
        // worklet recomputed these once per 128-frame render quantum.
        if (blockCounter == 0)
        {
            driftA = clampd (driftA * 0.9985 + (rand01() - 0.5) * 0.02, -1, 1);
            driftB = clampd (driftB * 0.9985 + (rand01() - 0.5) * 0.02, -1, 1);
            lfo += 6.2831853 * 0.11 * 128.0 / fsOut;
            if (lfo > 6.2831853) lfo -= 6.2831853;
            pwm = 0.5 - 0.055 * std::sin (lfo) * morph;
            dk = 1.0 + clampd (p.drive, 0, 1) * 3.2;
            dnorm = 1.0 / std::tanh (dk);
            cAGlide = 1.0 - std::exp (-1.0 / (juce::jmax (0.0015, (double) p.glide) * fsOut));
            const double spread = clampd (p.spread, 0, 1);
            cLA = 0.5 + 0.25 * spread; cLB = 0.5 - 0.25 * spread;
            cEnvAmp = clampd (p.envAmp, 0, 1);
            cFltK = 3.4657 * clampd (p.envFlt, 0, 1);
            cEnvPitch = clampd (p.envPitch, 0, 1);
            cSubDiv = p.subOct >= 2 ? 4.0 : 2.0;
            cStopTarget = p.stop > 0.5f ? 0.0 : 1.0;
            cAStop = 1.0 - std::exp (-1.0 / ((p.stop > 0.5f ? 0.45 : 0.15) * fsOut));
            if (cw > (1 << 30)) cw -= (1 << 29);
            blockCounter = 128;
        }
        --blockCounter;

        // ---- scheduled events (sample-accurate) ----
        const int64_t frame = startFrame + i;
        while (eventCount > 0 && events[0].frame <= frame)
        {
            applyEvent (events[0]);
            for (int k = 1; k < eventCount; ++k) events[(size_t)(k - 1)] = events[(size_t) k];
            --eventCount;
        }

        const double aGlide = cAGlide;
        const double lA = cLA, lB = cLB;
        const double envAmp = cEnvAmp;
        const double fltK = cFltK;
        const double envPitch = cEnvPitch;
        const double subDiv = cSubDiv;
        const double stopTarget = cStopTarget;
        const double aStop = cAStop;

        // ---- parameter smoothing ----
        f0 += (p.base * p.ratio - f0) * aGlide;
        morph += (p.morph - morph) * aSmooth;
        pulse += (p.pulse - pulse) * aSmooth;
        detune += (p.detune - detune) * aSmooth;
        level += (p.level - level) * aSmooth;
        cut += (p.cut - cut) * aSmooth;
        res += (p.res - res) * aSmooth;

        // ---- envelope ----
        if (gate && ! pg) stage = 1;
        if (! gate && pg) stage = 3;
        pg = gate;
        double target, tauv;
        if (stage == 1)
        {
            target = 1; tauv = juce::jmax (0.001f, p.attack) / 3.0;
            if (env > 0.985) stage = 2;
        }
        else if (stage == 2)
        {
            target = clampd (p.sustain, 0, 1); tauv = juce::jmax (0.01f, p.decay) / 3.0;
        }
        else
        {
            target = 0; tauv = juce::jmax (0.004f, p.release) / 3.0;
        }
        env += (target - env) * (1.0 - std::exp (-1.0 / (tauv * fsOut)));
        gl += (gate - gl) * aGate;
        const double e = env * env * (3.0 - 2.0 * env);
        stopMul += (stopTarget - stopMul) * aStop;
        subL += (p.sub - subL) * aSmooth;

        // ---- oscillator + filter at 2x ----
        double pMul = stopMul;
        // Pitch sag, keyed to the GATE, not the amp envelope. A held note (or a
        // Dark Drone) sits at its sustain level, where env e < 1 — so keying the
        // sag off (1 - e) pulled every sustained note permanently flat (~a
        // semitone at typical sustain), which read as Dark Drone / Tape shifting
        // the base note. gl is ~1 for the whole held portion and only falls on
        // release, so the pitch now stays in tune and sags flat only as the note
        // dies — which is what "notes sag as they decay" was meant to mean.
        if (envPitch > 0.001) pMul *= std::exp (-0.6931 * envPitch * (1.0 - gl));
        // world-mod: fanned detune (wmMul) and the bus sag, keyed to the
        // SAME smoothed gate the native sag uses — in tune while held
        pMul *= wmMul;
        if (wmSagAmt > 0.0001f) pMul *= std::exp2 (-(double) wmSagAmt / 12.0 * (1.0 - gl));
        const double det = detune / 1200.0;
        const double fA = f0 * pMul * std::pow (2.0, (driftA * 3.5 - det * 600.0) / 1200.0);
        const double fB = f0 * pMul * std::pow (2.0, (driftB * 3.5 + det * 600.0) / 1200.0);
        const double dtA = clampd (fA / fs, 1e-7, 0.45);
        const double dtB = clampd (fB / fs, 1e-7, 0.45);
        const double cutEff = (fltK > 0 ? cut * std::exp (fltK * (e - 1.0)) : cut) * (double) wmFmul;
        const float g = (float) std::tan (juce::MathConstants<double>::pi * clampd (cutEff, 18.0, fs * 0.45) / fs);
        const float kres = (float) (3.85 * clampd (res, 0, 1));
        const int mode = p.mode;
        const double inGain = OSC_GAIN * (1.0 + 0.25 * kres);
        const double outGain = OUT_GAIN / (1.0 + 0.3 * kres);
        double sl = 0, sr = 0;

        for (int j = 0; j < OS; ++j)
        {
            phA += dtA; if (phA >= 1) phA -= 1;
            phB += dtB; if (phB >= 1) phB -= 1;

            const double a = shape (triA, phA, dtA, pwm);
            const double b = shape (triB, phB, dtB, pwm);

            double xl = (a * lA + b * lB) * level * inGain;
            double xr = (a * lB + b * lA) * level * inGain;
            if (subL > 0.003)
            {
                const double dtS = clampd (f0 * pMul / subDiv / fs, 1e-7, 0.45);
                phS += dtS; if (phS >= 1) phS -= 1;
                double sv = std::sin (6.2831853 * phS);
                sv = std::tanh (1.8 * sv) * 1.05;
                const double sAdd = sv * subL * level * inGain;
                xl += sAdd; xr += sAdd;
            }
            double yl, yr;
            if (mode == 4)
            {
                const double Dc = juce::jmin (8000.0, juce::jmax (4.0, fs / juce::jmin (8000.0, juce::jmax (40.0, cutEff))));
                const double gC = 0.55 + 0.43 * clampd (res, 0, 1);
                const double ri = (double) cw - Dc;
                const int i0 = (int) std::floor (ri);
                const double frc = ri - i0;
                const double vL = cbL[(size_t)(i0 & 8191)] * (1 - frc) + cbL[(size_t)((i0 + 1) & 8191)] * frc;
                const double vR = cbR[(size_t)(i0 & 8191)] * (1 - frc) + cbR[(size_t)((i0 + 1) & 8191)] * frc;
                yl = std::tanh (xl * dk) * dnorm + gC * std::tanh (vL);
                yr = std::tanh (xr * dk) * dnorm + gC * std::tanh (vR);
                cbL[(size_t)(cw & 8191)] = yl;
                cbR[(size_t)(cw & 8191)] = yr;
                ++cw;
                yl *= OUT_GAIN * (1.0 - 0.45 * gC);
                yr *= OUT_GAIN * (1.0 - 0.45 * gC);
            }
            else
            {
                yl = ladL.process ((float) xl, g, kres, mode, (float) dk, (float) dnorm) * outGain;
                yr = ladR.process ((float) xr, g, kres, mode, (float) dk, (float) dnorm) * outGain;
            }

            if (j == 0) { sl = 0.25 * dz0 + 0.5 * yl; sr = 0.25 * dz1 + 0.5 * yr; }
            else        { sl += 0.25 * yl; sr += 0.25 * yr; dz0 = yl; dz1 = yr; }
        }

        const double amp = (envAmp * e + (1.0 - envAmp) * gl) * stopMul;
        // Second line of defence: whatever offset the pulse width, the ladder
        // or the comb still leaves, a 5 Hz one-pole takes out here. It costs
        // a 20 Hz note 0.07 dB and everything above that rather less.
        double dl = sl * amp, dr = sr * amp;
        { const double y = dl - dcxL + dcR * dcyL; dcxL = dl; dcyL = y; dl = y; }
        { const double y = dr - dcxR + dcR * dcyR; dcxR = dr; dcyR = y; dr = y; }
        const float outL = clampf ((float) dl, -1.4f, 1.4f);
        const float outR = clampf ((float) dr, -1.4f, 1.4f);

        // StereoPanner stereo law, then vGain, into the mix
        float mL, mR;
        if (pcl <= 0) { mL = outL + outR * pgL; mR = outR * pgR; }
        else          { mL = outL * pgL;        mR = outR + outL * pgR; }
        mixL[i] += mL * gainCurrent * wmGain;
        mixR[i] += mR * gainCurrent * wmGain;
    }
    panTau = 0.025f;   // smoothing engaged after the first render
}

//==============================================================================
void Lofi::process (float* L, float* R, int n)
{
    const double aS = 1.0 - std::exp (-1.0 / (0.02 * fs));
    for (int i = 0; i < n; ++i)
    {
        crush += (pCrush - crush) * aS;
        noise += (pNoise - noise) * aS;
        dirt  += (pDirt - dirt) * aS;
        const double xl = L[i], xr = R[i];

        const double hold = 1.0 + crush * crush * 38.0;
        holdAcc += 1.0;
        if (holdAcc >= hold) { holdAcc -= hold; holdL = xl; holdR = xr; }
        const double wet = juce::jmin (1.0, crush * 3.0);
        double cl = xl + (holdL - xl) * wet;
        double cr = xr + (holdR - xr) * wet;

        if (crush > 0.01)
        {
            const double q = std::pow (2.0, 12.0 - crush * 8.0);
            cl = std::round (cl * q) / q;
            cr = std::round (cr * q) / q;
        }

        if (dirt > 0.01)
        {
            const double dkl = 1.0 + dirt * 5.0, dn = 1.0 / std::tanh (dkl);
            cl += (std::tanh (cl * dkl) * dn - cl) * dirt;
            cr += (std::tanh (cr * dkl) * dn - cr) * dirt;
        }

        if (noise > 0.002)
        {
            const double hiss = (rand01() * 2.0 - 1.0) * noise * 0.012;
            if (rand01() < noise * 0.0006) { crEnv = 1.0; crVal = rand01() * 2.0 - 1.0; }
            crEnv *= 0.994;
            const double bed = hiss + crVal * crEnv * noise * 0.35;
            cl += bed; cr += bed;
        }

        L[i] = (float) cl; R[i] = (float) cr;
    }
}

//==============================================================================
void SafetyCompressor::process (float* L, float* R, int n)
{
    // threshold -6 dB, knee 10 dB, ratio 3, attack 6 ms, release 250 ms
    const float aAtt = 1.0f - std::exp ((float) (-1.0 / (0.006 * fs)));
    const float aRel = 1.0f - std::exp ((float) (-1.0 / (0.25 * fs)));
    constexpr float thr = -6.0f, knee = 10.0f, ratio = 3.0f;
    for (int i = 0; i < n; ++i)
    {
        const float mag = juce::jmax (std::abs (L[i]), std::abs (R[i]));
        const float inDb = mag > 1e-6f ? 20.0f * std::log10 (mag) : -120.0f;
        float overDb = 0.0f;
        const float lo = thr - knee * 0.5f, hi = thr + knee * 0.5f;
        if (inDb > hi)      overDb = (inDb - thr) * (1.0f - 1.0f / ratio);
        else if (inDb > lo) { const float t = (inDb - lo) / knee; overDb = (1.0f - 1.0f / ratio) * knee * t * t * 0.5f; }
        envDb += ((overDb > envDb) ? aAtt : aRel) * (overDb - envDb);
        const float gain = std::pow (10.0f, -envDb / 20.0f) * makeup;
        L[i] *= gain; R[i] *= gain;
    }
}

//==============================================================================
Engine::Engine()
{
    cmdStorage.resize (65536);
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    fsHost = sampleRate;
    maxBlockSize = juce::jmax (32, maxBlock);
    currentFrame = 0;
    dcMR = 1.0 - 6.2831853 * 5.0 / sampleRate;      // 5 Hz master DC blocker
    dcMxL = dcMyL = dcMxR = dcMyR = 0;

    for (int i = 0; i < kNumVoices; ++i)
        voices[(size_t) i].prepare (sampleRate, (uint32_t) (i + 1));
    voices[0].gainTarget = voices[0].gainCurrent = 0;

    for (auto& pp : params) pp.init (0);
    params[pMaster].init (0.70f);
    params[pPkFreq].init (1000); params[pPkGain].init (0);
    params[pSatDry].init (1); params[pSatWet].init (0); params[pSatPre].init (1); params[pSatToneF].init (8400);
    params[pDelayTime0].init (0.26f); params[pDelayTime1].init (0.26f);
    params[pDelayFb].init (0.34f); params[pDampF].init (4200); params[pDhpF].init (25); params[pDfeed].init (1);
    params[pWowDepth].init (0); params[pDelayDry].init (1); params[pDelayWet].init (0);
    params[pRevDry].init (1); params[pRevWet].init (0); params[pRvG0].init (1); params[pRvG1].init (0);
    params[pPhDry].init (1); params[pPhWet].init (0); params[pPhRate].init (0.4f); params[pPhDepth].init (0);
    params[pChDry].init (1); params[pChWet].init (0); params[pChRate].init (0.45f); params[pChDepth].init (0);
    params[pStRate].init (8); params[pStBase].init (1); params[pStMod].init (0);
    params[pLpFreq].init (20000); params[pLpQ].init (0.7f); params[pLpGain].init (0);
    params[pEnvGain].init (0); params[pAddGain].init (0);
    for (int i = 0; i < kNumPartials; ++i)
    {
        params[pPartF0 + i].init (220.0f * (float) (i + 1));
        params[pPartG0 + i].init (0);
    }
    params[pLimit].init (0.4f);
    limEnv = 0;

    lpBiquad.reset(); pkBiquad.reset(); satDC.reset(); satToneBq.reset();
    dampBq.reset(); dhpBq.reset(); chLP.reset();
    for (auto& a : phAP) a.reset();

    satDC.set (Biquad::highpass, 18, 0.5, 0, fsHost);
    dhpBq.set (Biquad::highpass, 25, 0.5, 0, fsHost);
    dampBq.set (Biquad::lowpass, 4200, 1.0, 0, fsHost);
    satToneBq.set (Biquad::lowpass, 8400, 1.0, 0, fsHost);
    pkBiquad.set (Biquad::peaking, 1000, 1.2, 0, fsHost);
    lpBiquad.set (Biquad::lowpass, 20000, 0.7, 0, fsHost);
    const double phBase[4] = { 300, 650, 1050, 1500 };
    for (int i = 0; i < 4; ++i)
        phAP[(size_t) i].set (Biquad::allpass, phBase[i], 0.6, 0, fsHost);

    satOs = std::make_unique<juce::dsp::Oversampling<float>> (2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    satOs->initProcessing ((size_t) maxBlockSize);

    const int dLen = juce::nextPowerOfTwo ((int) std::ceil (1.6 * fsHost));
    dBufL.assign ((size_t) dLen, 0.0f); dBufR.assign ((size_t) dLen, 0.0f);
    dMask = dLen - 1; dWrite = 0;

    const int chLen = juce::nextPowerOfTwo ((int) std::ceil (0.08 * fsHost));
    chBufL.assign ((size_t) chLen, 0.0f); chBufR.assign ((size_t) chLen, 0.0f);
    chMask = chLen - 1; chWrite = 0;

    phFbBufL.fill (0); phFbBufR.fill (0); phFbPos = 0;

    lofi.prepare (fsHost);
    comp.prepare (fsHost);

    {
        // A render starting in the host lands here while the editor may be
        // pushing a new impulse; preparing a Convolution under a load in
        // flight is what takes the process down.
        const juce::ScopedLock sl (irLock);
        juce::dsp::ProcessSpec spec { fsHost, (juce::uint32) maxBlockSize, 2 };
        for (auto& c : conv) c.prepare (spec);
        irLoaded[0] = irLoaded[1] = false;
        revActive = 0;
    }
    loadReverbImpulse (ReverbType::room, 1.8f);

    scratchL.allocate ((size_t) maxBlockSize, true);
    scratchR.allocate ((size_t) maxBlockSize, true);
    wetL.allocate ((size_t) maxBlockSize, true);
    wetR.allocate ((size_t) maxBlockSize, true);
    satBufL.allocate ((size_t) maxBlockSize, true);
    satBufR.allocate ((size_t) maxBlockSize, true);
    rvInL.allocate ((size_t) maxBlockSize, true);
    rvInR.allocate ((size_t) maxBlockSize, true);

    heldMono.reserve (64);
    wowPhase = phLfoPhase = chLfoPhase = stLfoPhase = 0;

    if (! everConfigured.load())
    {
        // First prepare: seed the shadow mirrors from the defaults so a later
        // prepare (sample-rate change) can restore the running state.
        fxOrderPacked = defaultFxPacked();
        fxEnabledBits = (1 << fxSaturation) | (1 << fxDelay) | (1 << fxReverb);
        shadowFxPacked = fxOrderPacked;
        shadowFxEnabled = fxEnabledBits;
        for (int i = 0; i < numParams; ++i)
            shadowParams[(size_t) i] = params[(size_t) i].current;
        for (int v = 0; v < kNumVoices; ++v)
        {
            const auto& pv = voices[(size_t) v].p;
            auto& sh = shadowVoice[(size_t) v];
            sh[vfBase] = pv.base; sh[vfRatio] = pv.ratio; sh[vfMorph] = pv.morph; sh[vfPulse] = pv.pulse;
            sh[vfDetune] = pv.detune; sh[vfLevel] = pv.level; sh[vfCut] = pv.cut; sh[vfRes] = pv.res;
            sh[vfMode] = (float) pv.mode; sh[vfDrive] = pv.drive; sh[vfAttack] = pv.attack; sh[vfDecay] = pv.decay;
            sh[vfSustain] = pv.sustain; sh[vfRelease] = pv.release; sh[vfGlide] = pv.glide; sh[vfSpread] = pv.spread;
            sh[vfEnvAmp] = pv.envAmp; sh[vfEnvFlt] = pv.envFlt; sh[vfSub] = pv.sub; sh[vfSubOct] = pv.subOct;
            sh[vfEnvPitch] = pv.envPitch; sh[vfStop] = pv.stop; sh[vfGate] = 0; sh[vfPan] = 0; sh[vfGain] = 0;
        }
        everConfigured = true;
    }
    else
    {
        // Re-prepare with existing state: restore everything from the mirrors
        // (single-threaded here, before audio starts flowing again).
        for (int i = 0; i < numParams; ++i)
            params[(size_t) i].init (shadowParams[(size_t) i].load());
        for (int v = 0; v < kNumVoices; ++v)
            for (int k = 0; k < numVoiceFields; ++k)
                if (k != vfGate)
                    voices[(size_t) v].setField (k, shadowVoice[(size_t) v][(size_t) k].load());
        fxOrderPacked = shadowFxPacked.load();
        fxEnabledBits = shadowFxEnabled.load();
        dLimit = shadowDLimit.load() != 0;
        lpTypeIndex = shadowLpType.load();
        loadReverbImpulse ((ReverbType) shadowRevType.load(), shadowRevLen.load());
    }
}

bool Engine::pushCommand (const Command& c)
{
    // Message thread only. If a burst (preset apply) outruns the audio
    // thread, wait briefly for space rather than dropping state.
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        int s1, n1, s2, n2;
        cmdFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 + n2 >= 1)
        {
            cmdStorage[(size_t) s1] = c;
            cmdFifo.finishedWrite (1);
            return true;
        }
        juce::Thread::sleep (1);
    }
    return false;
}

void Engine::drainCommands()
{
    int s1, n1, s2, n2;
    cmdFifo.prepareToRead (cmdFifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) applyCommand (cmdStorage[(size_t)(s1 + i)]);
    for (int i = 0; i < n2; ++i) applyCommand (cmdStorage[(size_t)(s2 + i)]);
    cmdFifo.finishedRead (n1 + n2);
}

void Engine::applyCommand (const Command& c)
{
    const double now = (double) currentFrame.load() / fsHost;
    switch (c.type)
    {
        case Command::setParamValue:
            if (c.d1 >= 0 && c.d1 > now)
                params[(size_t) c.param].push ({ c.d1, c.f1, 0, false });
            else
            {
                params[(size_t) c.param].current = params[(size_t) c.param].target = c.f1;
                params[(size_t) c.param].tau = 0;
            }
            break;
        case Command::setParamTarget:
            if (c.d1 >= 0 && c.d1 > now)
                params[(size_t) c.param].push ({ c.d1, c.f1, c.f2, true });
            else
            {
                params[(size_t) c.param].target = c.f1;
                params[(size_t) c.param].tau = c.f2;
            }
            break;
        case Command::cancelParam:
            params[(size_t) c.param].cancel();
            break;
        case Command::setVoiceField:
            if (c.i1 >= 0 && c.i1 < kNumVoices)
                voices[(size_t) c.i1].setField (c.i2, c.f1);
            break;
        case Command::voiceEvents:
            if (c.ptr != nullptr)
            {
                if (c.ptr->voice >= 0 && c.ptr->voice < kNumVoices)
                    voices[(size_t) c.ptr->voice].addEvents (c.ptr->events);
                c.ptr->consumed = true;
            }
            break;
        case Command::voiceClear:
            if (c.i1 < 0) for (auto& v : voices) v.clearEvents();
            else if (c.i1 < kNumVoices) voices[(size_t) c.i1].clearEvents();
            break;
        case Command::voiceReset:
            if (c.i1 >= 0 && c.i1 < kNumVoices) voices[(size_t) c.i1].resetState();
            break;
        case Command::setFxOrder:
            fxOrderPacked = c.i1; fxEnabledBits = c.i2;
            break;
        case Command::setDelayLimit:
            dLimit = c.i1 != 0;
            break;
        case Command::setLpType:
            lpTypeIndex = c.i1;
            break;
        case Command::setRevXfade:
            break;
        case Command::setMeta:
            metaMode = c.i1;
            metaVoices = c.i2 & 0xff;
            metaNotes = (c.i2 >> 8) & 0xff;
            metaOsc = (c.i2 >> 16) & 0xff;
            break;
        case Command::setHostTune:
            hostTune = c.f1;
            repitchHeld();
            break;
    }
}

//==============================================================================
// Note -> frequency, in fractional semitones so the global Tune / Fine offset
// can sit between notes.
static double mtofd (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

void Engine::hostNoteOn (int note, int sampleOffset)
{
    const int64_t frame = currentFrame.load() + sampleOffset;
    if (metaMode == 0)   // mono / unison
    {
        heldMono.push_back (note);
        VoiceEvent e; e.frame = frame; e.fieldMask = 1u << vfBase; e.values[0] = (float) mtofd (note + hostTune.load()); e.gate = 1;
        std::vector<VoiceEvent> one { e };
        const int inUse = juce::jlimit (1, kNumVoices, metaVoices);
        for (int v = 0; v < inUse; ++v) voices[(size_t) v].addEvents (one);
    }
    else                 // polyphonic
    {
        const int notesN = juce::jlimit (1, 4, metaNotes);
        const int oscN = juce::jlimit (1, 4, metaOsc);
        int pick = -1;
        for (int s = 0; s < notesN; ++s) if (slots[(size_t) s].on && slots[(size_t) s].note == note) { pick = s; break; }
        if (pick < 0) for (int s = 0; s < notesN; ++s) if (! slots[(size_t) s].on) { pick = s; break; }
        if (pick < 0)
        {
            pick = 0;
            for (int s = 1; s < notesN; ++s) if (slots[(size_t) s].seq < slots[(size_t) pick].seq) pick = s;
        }
        slots[(size_t) pick] = { note, ++slotSeq, true };
        VoiceEvent e; e.frame = frame; e.fieldMask = 1u << vfBase; e.values[0] = (float) mtofd (note + hostTune.load()); e.gate = 1;
        std::vector<VoiceEvent> one { e };
        for (int o = 0; o < oscN; ++o)
        {
            const int vi = pick * oscN + o;
            if (vi < kNumVoices) voices[(size_t) vi].addEvents (one);
        }
    }
}

void Engine::hostNoteOff (int note, int sampleOffset)
{
    const int64_t frame = currentFrame.load() + sampleOffset;
    if (metaMode == 0)
    {
        for (int i = (int) heldMono.size() - 1; i >= 0; --i)
            if (heldMono[(size_t) i] == note) { heldMono.erase (heldMono.begin() + i); break; }
        const int inUse = juce::jlimit (1, kNumVoices, metaVoices);
        VoiceEvent e; e.frame = frame;
        if (! heldMono.empty())
        {
            e.fieldMask = 1u << vfBase; e.values[0] = (float) mtofd (heldMono.back() + hostTune.load());
        }
        else
            e.gate = 0;
        std::vector<VoiceEvent> one { e };
        for (int v = 0; v < inUse; ++v) voices[(size_t) v].addEvents (one);
    }
    else
    {
        const int notesN = juce::jlimit (1, 4, metaNotes);
        const int oscN = juce::jlimit (1, 4, metaOsc);
        for (int s = 0; s < notesN; ++s)
        {
            if (slots[(size_t) s].on && slots[(size_t) s].note == note)
            {
                slots[(size_t) s].on = false;
                VoiceEvent e; e.frame = frame; e.gate = 0;
                std::vector<VoiceEvent> one { e };
                for (int o = 0; o < oscN; ++o)
                {
                    const int vi = s * oscN + o;
                    if (vi < kNumVoices) voices[(size_t) vi].addEvents (one);
                }
                break;
            }
        }
    }
}

void Engine::hostAllNotesOff()
{
    heldMono.clear();
    for (auto& s : slots) s.on = false;
    VoiceEvent e; e.frame = currentFrame.load(); e.gate = 0;
    std::vector<VoiceEvent> one { e };
    for (auto& v : voices) v.addEvents (one);
}

/* Tune / Fine moved while host notes are held and the editor is closed: slide
 * the sounding notes to the new pitch (base only, so the glide time applies
 * and nothing retriggers) instead of waiting for the next note-on. With the
 * editor open the page pushes its own base values and this is a no-op repeat
 * of the same frequency. */
void Engine::repitchHeld()
{
    const int64_t frame = currentFrame.load();
    const double tune = hostTune.load();
    if (metaMode == 0)
    {
        if (heldMono.empty()) return;
        VoiceEvent e; e.frame = frame; e.fieldMask = 1u << vfBase;
        e.values[0] = (float) mtofd (heldMono.back() + tune);
        std::vector<VoiceEvent> one { e };
        const int inUse = juce::jlimit (1, kNumVoices, metaVoices);
        for (int v = 0; v < inUse; ++v) voices[(size_t) v].addEvents (one);
    }
    else
    {
        const int notesN = juce::jlimit (1, 4, metaNotes);
        const int oscN = juce::jlimit (1, 4, metaOsc);
        for (int s = 0; s < notesN; ++s)
        {
            if (! slots[(size_t) s].on) continue;
            VoiceEvent e; e.frame = frame; e.fieldMask = 1u << vfBase;
            e.values[0] = (float) mtofd (slots[(size_t) s].note + tune);
            std::vector<VoiceEvent> one { e };
            for (int o = 0; o < oscN; ++o)
            {
                const int vi = s * oscN + o;
                if (vi < kNumVoices) voices[(size_t) vi].addEvents (one);
            }
        }
    }
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    drainCommands();
    int done = 0;
    while (done < n)
    {
        const int m = juce::jmin (32, n - done);
        processSub (L + done, R + done, m);
        done += m;
        currentFrame += m;
    }
}

void Engine::renderAdditive (float* L, float* R, int n)
{
    const float addG = params[pAddGain].current;
    const float envG = params[pEnvGain].current;
    if (addG < 1e-5f && params[pAddGain].target < 1e-5f)
        return;

    lpBiquad.set ((Biquad::Type) lpTypeIndex,
                  params[pLpFreq].current,
                  params[pLpQ].current,
                  params[pLpGain].current, fsHost);

    double freqs[kNumPartials], steps[kNumPartials];
    float gains[kNumPartials];
    for (int k = 0; k < kNumPartials; ++k)
    {
        freqs[k] = params[pPartF0 + k].current;
        steps[k] = 6.2831853 * freqs[k] / fsHost;
        gains[k] = params[pPartG0 + k].current;
    }

    for (int i = 0; i < n; ++i)
    {
        double s = 0;
        for (int k = 0; k < kNumPartials; ++k)
        {
            partPhase[(size_t) k] += steps[k];
            if (partPhase[(size_t) k] > 6.2831853) partPhase[(size_t) k] -= 6.2831853;
            s += std::sin (partPhase[(size_t) k]) * gains[k];
        }
        s *= 0.9 * envG;
        const float y = lpBiquad.processL ((float) s) * addG;
        L[i] += y; R[i] += y;
    }
}

void Engine::processSaturation (float* L, float* R, int n)
{
    const float dry = params[pSatDry].current;
    const float wet = params[pSatWet].current;
    const float pre = params[pSatPre].current;
    satToneBq.set (Biquad::lowpass, params[pSatToneF].current, 1.0, 0, fsHost);

    if (wet < 1e-6f && params[pSatWet].target < 1e-6f)
        return;                                      // dry stays ~1: pass through

    for (int i = 0; i < n; ++i) { satBufL[i] = L[i] * pre; satBufR[i] = R[i] * pre; }

    // triode curve, 4x oversampled
    float* chans[2] = { satBufL.get(), satBufR.get() };
    juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
    auto up = satOs->processSamplesUp (blk);
    constexpr double SAT_K = 4.0, SAT_BIAS = 0.11;
    const double zero = std::tanh (SAT_K * SAT_BIAS);
    const double hi = std::tanh (SAT_K * (1 + SAT_BIAS)) - zero;
    const double lo = std::tanh (SAT_K * (-1 + SAT_BIAS)) - zero;
    const double norm = juce::jmax (std::abs (hi), std::abs (lo), 0.001);
    for (size_t ch = 0; ch < 2; ++ch)
    {
        auto* d = up.getChannelPointer (ch);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            d[i] = (float) ((std::tanh (SAT_K * (clampd (d[i], -1, 1) + SAT_BIAS)) - zero) / norm);
    }
    satOs->processSamplesDown (blk);

    for (int i = 0; i < n; ++i)
    {
        float wl = satDC.processL (satBufL[i]);
        float wr = satDC.processR (satBufR[i]);
        wl = satToneBq.processL (wl);
        wr = satToneBq.processR (wr);
        L[i] = L[i] * dry + wl * wet;
        R[i] = R[i] * dry + wr * wet;
    }
}

void Engine::processDelay (float* L, float* R, int n)
{
    const float t0 = params[pDelayTime0].current, t1 = params[pDelayTime1].current;
    const float fb = params[pDelayFb].current;
    const float feed = params[pDfeed].current;
    const float wowDepth = params[pWowDepth].current;
    const float dry = params[pDelayDry].current, wet = params[pDelayWet].current;
    dampBq.set (Biquad::lowpass, params[pDampF].current, 1.0, 0, fsHost);
    dhpBq.set (Biquad::highpass, params[pDhpF].current, 0.5, 0, fsHost);
    const double wowStep = 6.2831853 * 0.55 / fsHost;
    const double tanhNorm = std::tanh (1.6);

    for (int i = 0; i < n; ++i)
    {
        const float wow = wowDepth * (float) std::sin (wowPhase);
        wowPhase += wowStep; if (wowPhase > 6.2831853) wowPhase -= 6.2831853;

        // 4-point Catmull-Rom: the tap moves (wow, drift, tempo changes), and
        // a moving tap read with linear interpolation smears the top octave and
        // adds zipper. Cubic is ~8 dB cleaner on bright material.
        auto readTap = [this] (const std::vector<float>& buf, double delaySec) -> float
        {
            const double ds = clampd (delaySec, 0.0, 1.5) * fsHost;
            const double ri = (double) dWrite - ds;
            const int i0 = (int) std::floor (ri);
            const double t = ri - i0;
            const double ym1 = buf[(size_t)((i0 - 1) & dMask)], y0 = buf[(size_t)(i0 & dMask)];
            const double y1 = buf[(size_t)((i0 + 1) & dMask)], y2 = buf[(size_t)((i0 + 2) & dMask)];
            const double c1 = 0.5 * (y1 - ym1);
            const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
            const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
            return (float) (((c3 * t + c2) * t + c1) * t + y0);
        };
        const float outL = readTap (dBufL, (double) t0 + wow);
        const float outR = readTap (dBufR, (double) t1 + wow);

        float lpL = dampBq.processL (outL), lpR = dampBq.processR (outR);
        lpL = dhpBq.processL (lpL); lpR = dhpBq.processR (lpR);
        if (dLimit)
        {
            lpL = (float) (std::tanh (1.6 * lpL) / tanhNorm);
            lpR = (float) (std::tanh (1.6 * lpR) / tanhNorm);
        }
        dBufL[(size_t)(dWrite & dMask)] = L[i] * feed + lpL * fb;
        dBufR[(size_t)(dWrite & dMask)] = R[i] * feed + lpR * fb;
        ++dWrite;

        L[i] = L[i] * dry + outL * wet;
        R[i] = R[i] * dry + outR * wet;
    }
    if (dWrite > (1 << 30)) dWrite -= (1 << 29);
}

void Engine::processPhaser (float* L, float* R, int n)
{
    const float dry = params[pPhDry].current, wet = params[pPhWet].current;
    const float rate = params[pPhRate].current, depth = params[pPhDepth].current;
    if (wet < 1e-6f && params[pPhWet].target < 1e-6f)
    {
        phLfoPhase += 6.2831853 * rate * n / fsHost;
        if (phLfoPhase > 6.2831853) phLfoPhase -= 6.2831853;
        return;
    }
    static const double base[4] = { 300, 650, 1050, 1500 };
    // The sweep is recomputed every 32 samples rather than once per host
    // block: at a 512-sample buffer the old cadence stepped the notches ~10 ms
    // apart, which is audible as zipper on fast, deep sweeps. Frequencies are
    // kept away from 0 Hz — at f = 0 the all-pass degenerates (double pole on
    // the unit circle) and the sound drops out at the bottom of the sweep.
    int done = 0;
    while (done < n)
    {
        const int m = juce::jmin (32, n - done);
        const double lfoV = std::sin (phLfoPhase);
        for (int s = 0; s < 4; ++s)
            phAP[(size_t) s].set (Biquad::allpass, clampd (base[s] + depth * lfoV, 20.0, fsHost * 0.49), 0.6, 0, fsHost);
        phLfoPhase += 6.2831853 * rate * m / fsHost;
        if (phLfoPhase > 6.2831853) phLfoPhase -= 6.2831853;

        for (int i = done; i < done + m; ++i)
        {
            const int rp = (phFbPos - 128) & 255;
            float xl = L[i] + 0.3f * phFbBufL[(size_t) rp];
            float xr = R[i] + 0.3f * phFbBufR[(size_t) rp];
            for (int s = 0; s < 4; ++s) { xl = phAP[(size_t) s].processL (xl); xr = phAP[(size_t) s].processR (xr); }
            phFbBufL[(size_t)(phFbPos & 255)] = xl;
            phFbBufR[(size_t)(phFbPos & 255)] = xr;
            phFbPos = (phFbPos + 1) & 0x3fffffff;
            L[i] = L[i] * dry + xl * wet;
            R[i] = R[i] * dry + xr * wet;
        }
        done += m;
    }
}

void Engine::processChorus (float* L, float* R, int n)
{
    const float dry = params[pChDry].current, wet = params[pChWet].current;
    const float rate = params[pChRate].current, depth = params[pChDepth].current;
    chLP.set (Biquad::lowpass, 5500, 1.0, 0, fsHost);
    const double step = 6.2831853 * rate / fsHost;
    if (wet < 1e-6f && params[pChWet].target < 1e-6f)
    {
        chLfoPhase += step * n;
        while (chLfoPhase > 6.2831853) chLfoPhase -= 6.2831853;
        // keep buffers written so switch-on has history
        for (int i = 0; i < n; ++i)
        {
            chBufL[(size_t)(chWrite & chMask)] = L[i];
            chBufR[(size_t)(chWrite & chMask)] = R[i];
            ++chWrite;
        }
        if (chWrite > (1 << 30)) chWrite -= (1 << 29);
        return;
    }
    for (int i = 0; i < n; ++i)
    {
        const double lfoS = std::sin (chLfoPhase) * depth;
        chLfoPhase += step; if (chLfoPhase > 6.2831853) chLfoPhase -= 6.2831853;

        chBufL[(size_t)(chWrite & chMask)] = L[i];
        chBufR[(size_t)(chWrite & chMask)] = R[i];

        auto readTap = [this] (const std::vector<float>& buf, double delaySec) -> float
        {
            const double ds = clampd (delaySec, 0.0, 0.06) * fsHost;
            const double ri = (double) chWrite - ds;
            const int i0 = (int) std::floor (ri);
            const double t = ri - i0;
            const double ym1 = buf[(size_t)((i0 - 1) & chMask)], y0 = buf[(size_t)(i0 & chMask)];
            const double y1 = buf[(size_t)((i0 + 1) & chMask)], y2 = buf[(size_t)((i0 + 2) & chMask)];
            const double c1 = 0.5 * (y1 - ym1);
            const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
            const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
            return (float) (((c3 * t + c2) * t + c1) * t + y0);
        };
        float wl = readTap (chBufL, 0.016 + lfoS);
        float wr = readTap (chBufR, 0.021 - lfoS);
        ++chWrite;
        wl = chLP.processL (wl);
        wr = chLP.processR (wr);
        L[i] = L[i] * dry + wl * wet;
        R[i] = R[i] * dry + wr * wet;
    }
    if (chWrite > (1 << 30)) chWrite -= (1 << 29);
}

void Engine::processStutter (float* L, float* R, int n)
{
    const float base = params[pStBase].current, mod = params[pStMod].current;
    const float rate = params[pStRate].current;
    const double step = 6.2831853 * rate / fsHost;
    for (int i = 0; i < n; ++i)
    {
        const double x = std::sin (stLfoPhase);
        stLfoPhase += step; if (stLfoPhase > 6.2831853) stLfoPhase -= 6.2831853;
        const double u = clampd ((x + 0.4) / 0.5, 0, 1);
        const float gate = base + mod * (float) (u * u * (3 - 2 * u));
        L[i] *= gate; R[i] *= gate;
    }
}

void Engine::processReverb (float* L, float* R, int n)
{
    const float dry = params[pRevDry].current, wet = params[pRevWet].current;
    const float g0 = params[pRvG0].current, g1 = params[pRvG1].current;

    std::memcpy (rvInL.get(), L, (size_t) n * sizeof (float));
    std::memcpy (rvInR.get(), R, (size_t) n * sizeof (float));

    for (int i = 0; i < n; ++i) { L[i] *= dry; R[i] *= dry; }

    for (int c = 0; c < 2; ++c)
    {
        const float g = c == 0 ? g0 : g1;
        if (! irLoaded[(size_t) c].load() || (g < 1e-6f && params[c == 0 ? pRvG0 : pRvG1].target < 1e-6f))
            continue;
        std::memcpy (wetL.get(), rvInL.get(), (size_t) n * sizeof (float));
        std::memcpy (wetR.get(), rvInR.get(), (size_t) n * sizeof (float));
        float* chans[2] = { wetL.get(), wetR.get() };
        juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctxb (blk);
        conv[(size_t) c].process (ctxb);
        const float gg = g * wet;
        for (int i = 0; i < n; ++i) { L[i] += wetL[i] * gg; R[i] += wetR[i] * gg; }
    }
}

void Engine::processSub (float* L, float* R, int n)
{
    const double now = (double) currentFrame.load() / fsHost;
    for (auto& pp : params) pp.tick (now, fsHost, n);

    for (int i = 0; i < n; ++i) { L[i] = 0; R[i] = 0; }

    renderAdditive (L, R, n);

    // Brokild World FX bus, fanned across the pool once per block.
    // Neutral writes exact identities, so the render stays bit-identical.
    {
        const float det  = wmIn[0].load (std::memory_order_relaxed);
        const float pan  = wmIn[1].load (std::memory_order_relaxed);
        const float trD  = wmIn[2].load (std::memory_order_relaxed);
        const float trR  = wmIn[3].load (std::memory_order_relaxed);
        const float sag  = wmIn[4].load (std::memory_order_relaxed);
        const float fmul = wmIn[5].load (std::memory_order_relaxed);
        const bool act = det != 0.0f || pan != 0.0f || trD != 0.0f || sag != 0.0f || fmul != 1.0f;
        if (act) wmT += (double) n / fsHost; else wmT = 0.0;
        int vi = 0;
        for (auto& v : voices)
        {
            if (! act)
            {
                v.wmMul = 1.0; v.wmFmul = 1.0f; v.wmGain = 1.0f; v.wmPanAdd = 0.0f; v.wmSagAmt = 0.0f;
            }
            else
            {
                const float fan  = std::fmod ((float) vi * 0.6180339887f + 0.5f, 1.0f) * 2.0f - 1.0f;
                const float fan2 = std::fmod ((float) vi * 0.6180339887f + 0.21f, 1.0f) * 2.0f - 1.0f;
                const float u    = std::fmod ((float) vi * 0.6180339887f + 0.71f, 1.0f);
                v.wmMul = std::exp2 ((double) (det * fan) / 1200.0);
                v.wmSagAmt = sag;
                v.wmFmul = fmul;
                v.wmPanAdd = pan * fan2;
                v.wmGain = (trD > 0.0f && trR > 0.0f)
                    ? 1.0f - trD * (0.5f - 0.5f * (float) std::sin (6.2831853 * (double) (trR * (0.75f + 0.5f * u)) * wmT + (double) vi * 2.39996))
                    : 1.0f;
            }
            ++vi;
        }
    }

    const int64_t frame = currentFrame.load();
    for (auto& v : voices)
        v.render (L, R, n, frame);

    pkBiquad.set (Biquad::peaking, params[pPkFreq].current, 1.2, params[pPkGain].current, fsHost);
    for (int i = 0; i < n; ++i) { L[i] = pkBiquad.processL (L[i]); R[i] = pkBiquad.processR (R[i]); }

    const int order = fxOrderPacked;
    const int enabled = fxEnabledBits;
    for (int s = 0; s < numFxModules; ++s)
    {
        const int mod = (order >> (s * 3)) & 7;
        if (mod >= numFxModules) continue;
        if ((enabled & (1 << mod)) == 0) continue;
        switch (mod)
        {
            case fxSaturation: processSaturation (L, R, n); break;
            case fxPhaser:     processPhaser (L, R, n); break;
            case fxChorus:     processChorus (L, R, n); break;
            case fxStutter:    processStutter (L, R, n); break;
            case fxLofi:
                lofi.pCrush = params[pLofiCrush].current;
                lofi.pNoise = params[pLofiNoise].current;
                lofi.pDirt  = params[pLofiDirt].current;
                lofi.process (L, R, n);
                break;
            case fxDelay:      processDelay (L, R, n); break;
            case fxReverb:     processReverb (L, R, n); break;
        }
    }

    comp.process (L, R, n);

    // Master DC blocker, ahead of the master gain and the limiter. The tube
    // stage saturates asymmetrically and the delay/reverb loops can integrate
    // an offset of their own, so even with the oscillators clean there has to
    // be one last one here — an offset otherwise eats headroom and makes the
    // limiter duck on one side of the waveform only.
    for (int i = 0; i < n; ++i)
    {
        const double yl = L[i] - dcMxL + dcMR * dcMyL; dcMxL = L[i]; dcMyL = yl; L[i] = (float) yl;
        const double yr = R[i] - dcMxR + dcMR * dcMyR; dcMxR = R[i]; dcMyR = yr; R[i] = (float) yr;
    }

    const float master = params[pMaster].current;
    for (int i = 0; i < n; ++i) { L[i] *= master; R[i] *= master; }

    // Transparent output limiter: pure downward gain riding above a
    // threshold set by the amount — no makeup, no shaping, so it stays
    // inaudible until a peak would otherwise poke out.
    const float amt = params[pLimit].current;
    if (amt > 0.001f)
    {
        const float thr = std::pow (10.0f, (-9.0f * amt) / 20.0f);
        const float aAtt = 1.0f - std::exp ((float) (-1.0 / (0.0015 * fsHost)));
        const float aRel = 1.0f - std::exp ((float) (-1.0 / (0.15 * fsHost)));
        for (int i = 0; i < n; ++i)
        {
            const float peak = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            limEnv += ((peak > limEnv) ? aAtt : aRel) * (peak - limEnv);
            if (limEnv > thr)
            {
                const float gv = thr / limEnv;
                L[i] *= gv; R[i] *= gv;
            }
        }
    }
}

//==============================================================================
// Deterministic stereo impulse — exact port of reverbImpulse(), including the
// WebAudio ConvolverNode normalization (normalize = true by default there).
juce::AudioBuffer<float> Engine::makeReverbImpulse (ReverbType type, float length, double fs)
{
    const int frames = juce::jmax (1, (int) std::round (fs * length));
    juce::AudioBuffer<float> b (2, frames);

    if (type == ReverbType::spring)
    {
        for (int sc = 0; sc < 2; ++sc)
        {
            auto* sd = b.getWritePointer (sc);
            std::fill (sd, sd + frames, 0.0f);
            double en = 0;
            const int period = (int) std::round (fs * (0.045 + 0.004 * sc));
            const int chirpLen = (int) std::round (fs * 0.035);
            const int nPulse = juce::jmax (1, frames / juce::jmax (1, period));
            for (int k = 0; k < nPulse; ++k)
            {
                const int t0 = k * period;
                const double amp = std::pow (0.8, k) * std::pow (1.0 - (double) t0 / frames, 1.4);
                double ph = sc * 1.7 + k * 0.31;
                for (int si = 0; si < chirpLen && t0 + si < frames; ++si)
                {
                    const double u = (double) si / chirpLen;
                    const double f = 3800.0 * std::pow (250.0 / 3800.0, u);
                    ph += 6.2831853 * f / fs;
                    sd[t0 + si] += (float) (std::sin (ph) * amp * std::sin (juce::MathConstants<double>::pi * juce::jmin (1.0, u * 4)) * (1 - 0.6 * u));
                }
            }
            uint32_t ss = (uint32_t) (0x51ab + sc * 0x9e3779b9);
            for (int si = 0; si < frames; ++si)
            {
                ss = 1664525u * ss + 1013904223u;
                sd[si] += (float) ((ss / 4294967296.0 * 2 - 1) * 0.05 * std::pow (1.0 - (double) si / frames, 3.0));
                en += (double) sd[si] * sd[si];
            }
            const double sk = en > 0 ? 0.75 / std::sqrt (en) : 1.0;
            for (int si = 0; si < frames; ++si) sd[si] = (float) (sd[si] * sk);
        }
    }
    else
    {
        const uint32_t seed = type == ReverbType::hall ? 0x51f15e
                            : type == ReverbType::plate ? 0x71a7e
                            : type == ReverbType::reverse ? 0x9e111 : 0x22334;
        const bool rev = type == ReverbType::reverse;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = b.getWritePointer (ch);
            double energy = 0;
            uint32_t s = (uint32_t) (seed + (uint32_t) ch * 0x9e3779b9u);
            const double decayPower = type == ReverbType::hall ? 2.2 : type == ReverbType::plate ? 1.35 : 3.2;
            for (int i = 0; i < frames; ++i)
            {
                s = 1664525u * s + 1013904223u;
                const double noise = s / 4294967296.0 * 2 - 1;
                const double envv = rev
                    ? std::pow ((double) i / frames, 2.4) * juce::jmin (1.0, (double) (frames - i) / (fs * 0.03))
                    : std::pow (1.0 - (double) i / frames, decayPower);
                double v = noise * envv;
                if (type == ReverbType::room && i < (int) (fs * 0.08) && i % 997 == ch * 137) v += 2.2 * envv;
                d[i] = (float) v;
                energy += v * v;
            }
            const double scale = energy > 0 ? 0.75 / std::sqrt (energy) : 1.0;
            for (int i = 0; i < frames; ++i) d[i] = (float) (d[i] * scale);
        }
    }

    // ConvolverNode normalization (normalize = true): scale by calibrated
    // 1/power so the perceived level is independent of the IR length.
    double power = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = b.getReadPointer (ch);
        for (int i = 0; i < frames; ++i) power += (double) d[i] * d[i];
    }
    power = std::sqrt (power / (2.0 * frames));
    power = juce::jmax (power, 0.000125);
    double scale = 0.00125 / power;
    scale *= 44100.0 / fs;
    for (int ch = 0; ch < 2; ++ch)
        b.applyGain (ch, 0, frames, (float) scale);
    return b;
}

void Engine::loadReverbImpulse (ReverbType type, float lengthSeconds)
{
    const juce::ScopedLock sl (irLock);
    const int next = 1 - revActive.load();
    auto ir = makeReverbImpulse (type, juce::jlimit (0.2f, 6.0f, lengthSeconds), fsHost);
    conv[(size_t) next].loadImpulseResponse (std::move (ir), fsHost,
        juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no,
        juce::dsp::Convolution::Normalise::no);
    irLoaded[(size_t) next] = true;

    Command c1; c1.type = Command::setParamTarget; c1.param = next == 0 ? pRvG0 : pRvG1; c1.f1 = 1; c1.f2 = 0.05f; c1.d1 = -1;
    Command c2; c2.type = Command::setParamTarget; c2.param = next == 0 ? pRvG1 : pRvG0; c2.f1 = 0; c2.f2 = 0.05f; c2.d1 = -1;
    pushCommand (c1);
    pushCommand (c2);
    revActive = next;
}

//==============================================================================
juce::var Engine::snapshotState()
{
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> pv;
    for (int i = 0; i < numParams; ++i) pv.add (shadowParams[(size_t) i].load());
    obj->setProperty ("params", pv);
    obj->setProperty ("lpType", shadowLpType.load());
    obj->setProperty ("dLimit", shadowDLimit.load());
    obj->setProperty ("fxOrder", shadowFxPacked.load());
    obj->setProperty ("fxEnabled", shadowFxEnabled.load());
    obj->setProperty ("revType", shadowRevType.load());
    obj->setProperty ("revLen", shadowRevLen.load());
    juce::Array<juce::var> vs;
    for (int v = 0; v < kNumVoices; ++v)
    {
        juce::Array<juce::var> f;
        for (int k = 0; k < numVoiceFields; ++k) f.add (shadowVoice[(size_t) v][(size_t) k].load());
        vs.add (f);
    }
    obj->setProperty ("voices", vs);
    return juce::var (obj);
}

void Engine::applyStateSnapshot (const juce::var& v)
{
    everConfigured = true;      // shadows now hold the authoritative state
    if (auto* pv = v.getProperty ("params", juce::var()).getArray())
    {
        for (int i = 0; i < juce::jmin ((int) numParams, pv->size()); ++i)
        {
            const float val = (float) (double) (*pv)[i];
            shadowParams[(size_t) i] = val;
            Command c; c.type = Command::setParamValue; c.param = (int16_t) i; c.f1 = val; c.d1 = -1;
            pushCommand (c);
        }
    }
    { Command c; c.type = Command::setLpType; c.i1 = (int) v.getProperty ("lpType", 0); shadowLpType = c.i1; pushCommand (c); }
    { Command c; c.type = Command::setDelayLimit; c.i1 = (int) v.getProperty ("dLimit", 0); shadowDLimit = c.i1; pushCommand (c); }
    {
        Command c; c.type = Command::setFxOrder;
        c.i1 = (int) v.getProperty ("fxOrder", defaultFxPacked());
        c.i2 = (int) v.getProperty ("fxEnabled", 0x7f & ~((1 << fxPhaser) | (1 << fxChorus) | (1 << fxStutter) | (1 << fxLofi)));
        shadowFxPacked = c.i1; shadowFxEnabled = c.i2;
        pushCommand (c);
    }
    shadowRevType = (int) v.getProperty ("revType", 0);
    shadowRevLen = (float) (double) v.getProperty ("revLen", 1.8);
    loadReverbImpulse ((ReverbType) shadowRevType.load(), shadowRevLen.load());
    if (auto* vs = v.getProperty ("voices", juce::var()).getArray())
    {
        for (int vi = 0; vi < juce::jmin ((int) kNumVoices, vs->size()); ++vi)
        {
            if (auto* f = (*vs)[vi].getArray())
            {
                for (int k = 0; k < juce::jmin ((int) numVoiceFields, f->size()); ++k)
                {
                    if (k == vfGate) continue;                     // never restore held gates
                    const float val = (float) (double) (*f)[k];
                    shadowVoice[(size_t) vi][(size_t) k] = val;
                    Command c; c.type = Command::setVoiceField; c.i1 = vi; c.i2 = k; c.f1 = val;
                    pushCommand (c);
                }
            }
        }
    }
}

int Engine::defaultFxPacked()
{
    // S.fxOrder default: saturation, phaser, chorus, stutter, lofi, delay, reverb
    int packed = 0;
    const int order[7] = { fxSaturation, fxPhaser, fxChorus, fxStutter, fxLofi, fxDelay, fxReverb };
    for (int i = 0; i < 7; ++i) packed |= order[i] << (i * 3);
    return packed;
}

} // namespace ps
