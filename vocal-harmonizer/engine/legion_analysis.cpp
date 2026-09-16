#include "legion_analysis.h"

#include <algorithm>
#include <cmath>

namespace legion
{

namespace
{
    constexpr float kEps       = 1.0e-9f;
    constexpr int   kEnvIters  = 6;      // true-envelope passes (converges well
                                         // before this on speech; the loop
                                         // breaks early when it has)
    constexpr float kEnvDoneDb = 1.0f;   // stop when no bin pokes >1 dB out

    inline float wrapPi (float x)
    {
        //  no fmod: the argument is always within a few turns of zero here
        while (x >  (float) M_PI) x -= (float) (2.0 * M_PI);
        while (x < -(float) M_PI) x += (float) (2.0 * M_PI);
        return x;
    }
}

void Analyser::prepare (int maxN_, const Fft* f)
{
    fft  = f;
    maxN = maxN_;
    const size_t h1 = (size_t) (maxN / 2 + 1);

    spec.assign ((size_t) (2 * maxN), 0.0f);
    cep .assign ((size_t) (2 * maxN), 0.0f);

    magB.assign (h1, 0.0f);  phaseB.assign (h1, 0.0f);  omegaB.assign (h1, 0.0f);
    envB.assign (h1, 1.0f);  cReB.assign (h1, 0.0f);  cImB.assign (h1, 0.0f);
    logMag.assign (h1, 0.0f); cur.assign (h1, 0.0f); smoothed.assign (h1, 0.0f);
    prevPhase.assign (h1, 0.0f); prevMag.assign (h1, 0.0f);
    peakBinB.assign (h1, 0);
    peakLoB.assign (h1, 0);
    peakHiB.assign (h1, 0);
    peakOmegaB.assign (h1, 0.0f);

    reset();
}

void Analyser::reset()
{
    std::fill (prevPhase.begin(), prevPhase.end(), 0.0f);
    std::fill (prevMag.begin(),   prevMag.end(),   0.0f);
    havePrev = false;
    fr = Frame();
}

// ---------------------------------------------------------------------------
void Analyser::analyse (const float* windowed, int N, int hop, double fs)
{
    const int half = N / 2;

    for (int i = 0; i < N; ++i) { spec[(size_t) (2 * i)] = windowed[i]; spec[(size_t) (2 * i + 1)] = 0.0f; }
    fft->forward (spec.data(), N);

    const float expected = (float) (2.0 * M_PI * (double) hop / (double) N);
    float maxMag = 0.0f;
    double energy = 0.0;
    float flux = 0.0f, fluxRef = 0.0f;

    for (int k = 0; k <= half; ++k)
    {
        const float re = spec[(size_t) (2 * k)];
        const float im = spec[(size_t) (2 * k + 1)];
        const float m  = std::sqrt (re * re + im * im);
        const float p  = std::atan2 (im, re);

        magB[(size_t) k]   = m;
        phaseB[(size_t) k] = p;
        maxMag = std::max (maxMag, m);
        energy += (double) m * m;

        //  true frequency: bin centre plus the phase advance the last hop did
        //  not account for
        const float dev = havePrev ? wrapPi (p - prevPhase[(size_t) k] - expected * (float) k)
                                   : 0.0f;
        omegaB[(size_t) k] = (float) (2.0 * M_PI * (double) k / (double) N) + dev / (float) hop;

        flux    += std::max (0.0f, m - prevMag[(size_t) k]);
        fluxRef += prevMag[(size_t) k];

        prevPhase[(size_t) k] = p;
    }

    fr.N = N; fr.half = half;
    fr.mag = magB.data(); fr.env = envB.data();
    fr.cRe = cReB.data(); fr.cIm = cImB.data();
    fr.peakBin = peakBinB.data();
    fr.peakLo = peakLoB.data(); fr.peakHi = peakHiB.data();
    fr.peakOmega = peakOmegaB.data();

    //  A frame with nothing in it: hand back a defined, silent frame rather
    //  than dividing by an envelope estimated from the noise floor.
    if (energy < 1.0e-20)
    {
        std::fill (envB.begin(),  envB.begin()  + half + 1, 1.0f);
        std::fill (cReB.begin(), cReB.begin() + half + 1, 0.0f);
        std::fill (cImB.begin(), cImB.begin() + half + 1, 0.0f);
        std::fill (prevMag.begin(), prevMag.begin() + half + 1, 0.0f);
        peakBinB[0] = 0; peakOmegaB[0] = 0.0f;
        peakLoB[0] = 0; peakHiB[0] = half;
        fr.peakCount = 1; fr.transient = false; fr.f0 = 0.0f; fr.voiced = 0.0f;
        havePrev = true;
        return;
    }

    //  TRANSIENT. Flux relative to the previous frame's own size, so it fires
    //  on an attack at any level and not merely on a loud passage.
    fr.transient = havePrev && (flux > 0.55f * (fluxRef + 1.0e-6f));

    for (int k = 0; k <= half; ++k)
    {
        prevMag[(size_t) k] = magB[(size_t) k];
        logMag[(size_t) k]  = std::log (std::max (magB[(size_t) k], maxMag * 1.0e-7f + kEps));
    }

    trueEnvelope (N, 0, fs);

    for (int k = 0; k <= half; ++k)
    {
        const float g = (k & 1) ? -1.0f : 1.0f;
        cReB[(size_t) k] = spec[(size_t) (2 * k)]     * g;
        cImB[(size_t) k] = spec[(size_t) (2 * k + 1)] * g;
    }

    findPeaks (N);
    havePrev = true;
}

// ---------------------------------------------------------------------------
// The true envelope, plus the f0 that sizes it. One cepstrum is computed for
// both: the pitch lives at quefrency fs/f0, and the lifter must cut BELOW
// that or the "envelope" starts following individual harmonics and formant
// control turns back into plain resampling.
void Analyser::trueEnvelope (int N, int /*orderIn*/, double fs)
{
    const int half = N / 2;

    auto toCepstrum = [&] (const float* logSpec)
    {
        for (int k = 0; k <= half; ++k)
        {
            cep[(size_t) (2 * k)] = logSpec[(size_t) k];
            cep[(size_t) (2 * k + 1)] = 0.0f;
        }
        for (int k = 1; k < half; ++k)
        {
            cep[(size_t) (2 * (N - k))] = logSpec[(size_t) k];
            cep[(size_t) (2 * (N - k) + 1)] = 0.0f;
        }
        fft->inverse (cep.data(), N);
    };

    auto lifterToSmoothed = [&] (int order)
    {
        for (int q = order + 1; q < N - order; ++q)
        {
            cep[(size_t) (2 * q)] = 0.0f;
            cep[(size_t) (2 * q + 1)] = 0.0f;
        }
        fft->forward (cep.data(), N);
        for (int k = 0; k <= half; ++k) smoothed[(size_t) k] = cep[(size_t) (2 * k)];
    };

    toCepstrum (logMag.data());

    //  f0 from the cepstral peak, 60 Hz .. 1000 Hz
    const int qmin = std::max (2, (int) (fs / 1000.0));
    const int qmax = std::min (half - 2, (int) (fs / 60.0));
    int   qBest = 0;
    float vBest = 0.0f;
    double qMean = 0.0;
    for (int q = qmin; q <= qmax; ++q)
    {
        const float v = cep[(size_t) (2 * q)];
        qMean += std::abs (v);
        if (v > vBest) { vBest = v; qBest = q; }
    }
    qMean /= (double) std::max (1, qmax - qmin + 1);

    fr.f0 = 0.0f; fr.voiced = 0.0f;
    if (qBest > qmin && qBest < qmax)
    {
        //  parabolic refinement on the cepstral peak
        const float ym = cep[(size_t) (2 * (qBest - 1))];
        const float y0 = cep[(size_t) (2 * qBest)];
        const float yp = cep[(size_t) (2 * (qBest + 1))];
        const float den = ym - 2.0f * y0 + yp;
        const float d = (std::abs (den) > 1.0e-12f) ? 0.5f * (ym - yp) / den : 0.0f;
        fr.f0 = (float) (fs / ((double) qBest + (double) std::max (-0.5f, std::min (0.5f, d))));
        fr.voiced = (float) std::min (1.0, (double) vBest / (4.0 * qMean + 1.0e-9));
    }

    /*  ORDER. Cut at 0.4 of the pitch period so the lifter cannot resolve
        the harmonic comb; clamped so a wild f0 estimate degrades to a sane
        envelope rather than to nonsense. Unvoiced frames (screams, breath,
        consonants) get the mid value — there is no comb to avoid, and the
        true-envelope max-iteration is stable without one. */
    int order = (fr.f0 > 0.0f && fr.voiced > 0.15f)
              ? (int) (0.40 * fs / (double) fr.f0)
              : (int) (0.40 * fs / 220.0);
    order = std::max (10, std::min (order, N / 6));

    lifterToSmoothed (order);

    for (int it = 1; it < kEnvIters; ++it)
    {
        float worst = 0.0f;
        for (int k = 0; k <= half; ++k)
        {
            const float a = std::max (logMag[(size_t) k], smoothed[(size_t) k]);
            worst = std::max (worst, logMag[(size_t) k] - smoothed[(size_t) k]);
            cur[(size_t) k] = a;
        }
        if (worst < kEnvDoneDb * 0.11513f) break;   // 1 dB in natural log
        toCepstrum (cur.data());
        lifterToSmoothed (order);
    }

    for (int k = 0; k <= half; ++k)
        envB[(size_t) k] = std::exp (smoothed[(size_t) k]);
}

// ---------------------------------------------------------------------------
// Sinusoidal peaks and their regions of influence. The regions are what the
// shifter locks phase across: every bin around a peak keeps its ORIGINAL
// phase relationship to that peak, which is the difference between a
// harmonizer and a flanged whisper.
void Analyser::findPeaks (int N)
{
    const int half = N / 2;
    int n = 0;

    for (int k = 2; k <= half - 2; ++k)
    {
        const float m = magB[(size_t) k];
        if (m > magB[(size_t) (k - 1)] && m >= magB[(size_t) (k + 1)]
            && m > magB[(size_t) (k - 2)] && m >= magB[(size_t) (k + 2)])
        {
            peakBinB[(size_t) n] = k;
            peakOmegaB[(size_t) n] = omegaB[(size_t) k];
            ++n;
        }
    }

    if (n == 0)   // no structure at all — one region covering everything
    {
        int kMax = 0;
        for (int k = 0; k <= half; ++k) if (magB[(size_t) k] > magB[(size_t) kMax]) kMax = k;
        peakBinB[0] = kMax;
        peakOmegaB[0] = omegaB[(size_t) kMax];
        n = 1;
    }

    fr.peakCount = n;

    //  boundary between two peaks = the quietest bin between them
    int k = 0;
    for (int i = 0; i < n; ++i)
    {
        int end;
        if (i == n - 1) end = half;
        else
        {
            const int lo = peakBinB[(size_t) i], hi = peakBinB[(size_t) (i + 1)];
            int argmin = lo;
            for (int j = lo + 1; j <= hi; ++j)
                if (magB[(size_t) j] < magB[(size_t) argmin]) argmin = j;
            end = argmin;
        }
        peakLoB[(size_t) i] = k;
        peakHiB[(size_t) i] = end;
        k = end + 1;
    }
    peakHiB[(size_t) (n - 1)] = half;
}

} // namespace legion
