#include "legion_harmonizer.h"

#include <algorithm>
#include <cmath>

namespace legion
{

namespace
{
    inline int nextPow2 (int v)
    {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    inline float dbToGain (float db) { return db <= -59.9f ? 0.0f : std::pow (10.0f, db * 0.05f); }

    //  four-point Catmull-Rom, the house read (bwfx_dsp.h): linear smears
    //  the top octave on a moving tap, and a choir spread is a moving tap
    //  every time the delay knob is touched.
    inline float catmullRead (const std::vector<float>& buf, int mask, int writePos, float delaySamples)
    {
        const float ri = (float) writePos - delaySamples;
        const int   i0 = (int) std::floor (ri);
        const float t  = ri - (float) i0;
        const float ym1 = buf[(size_t) ((i0 - 1) & mask)], y0 = buf[(size_t) (i0 & mask)];
        const float y1  = buf[(size_t) ((i0 + 1) & mask)], y2 = buf[(size_t) ((i0 + 2) & mask)];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    //  a choir does not line up. These are the per-voice offsets HUMANISE
    //  adds on top of whatever the delay knobs say (ms, prime-ish so they
    //  never beat against each other).
    constexpr float kStagger[kVoices] = { 0.0f, 13.0f, 23.0f, 7.0f };
}

// ---------------------------------------------------------------------------
void Harmonizer::prepare (double sampleRate, int maxBlockIn)
{
    fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
    maxBlock = std::max (1, maxBlockIn);

    //  Window duration, not window samples, is what the ear cares about:
    //  NATURAL is ~43 ms everywhere, which holds three periods of an 80 Hz
    //  male fundamental — the floor for resolving a low voice's harmonics.
    baseN = fs > 128000.0 ? 8192 : (fs > 64000.0 ? 4096 : 2048);
    sizes[0] = baseN / 2;
    sizes[1] = baseN;
    sizes[2] = baseN * 2;
    maxN = sizes[2];

    fft.prepare (maxN);
    analyser.prepare (maxN, &fft);
    for (auto& s : shifter) s.prepare (maxN);

    for (int d = 0; d < kDetails; ++d)
    {
        const int n = sizes[d];
        window[d].resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            window[d][(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * M_PI * (double) i / (double) n);
        /*  87.5 % overlap (hop N/8), and the sum of Hann^2 at that hop is
            3. The cheaper 75 % was built first: it puts the worst shifted-
            tone spur at -32 dB where this puts it at -39, for about 7 % more
            of a core on four voices. Seven decibels of artefact is the
            difference between a second voice you can bury and one you can
            solo, so the CPU is spent. */
        olaScale[d] = 1.0f / 3.0f;
    }

    ringSize = nextPow2 (2 * maxN + maxBlock + 8);
    ringMask = ringSize - 1;
    inRing.assign ((size_t) ringSize, 0.0f);
    dryRingL.assign ((size_t) ringSize, 0.0f);
    dryRingR.assign ((size_t) ringSize, 0.0f);
    for (auto& o : olaRing) o.assign ((size_t) ringSize, 0.0f);

    dlySize = nextPow2 ((int) (kMaxDelayMs * 0.001 * fs) + 8 + maxBlock);
    dlyMask = dlySize - 1;
    for (auto& d : dlyRing) d.assign ((size_t) dlySize, 0.0f);

    frameBuf.assign ((size_t) maxN, 0.0f);
    specBuf.assign ((size_t) (2 * maxN), 0.0f);

    activeDetail = std::max (0, std::min (kDetails - 1, pendingDetail.load()));
    N = sizes[activeDetail];
    hop = N / 8;

    for (int v = 0; v < kVoices; ++v) rngState[v] = 0x9E3779B9u * (uint32_t) (v + 1) + 12345u;

    reset();
}

int Harmonizer::windowFor (int d) const
{
    return sizes[std::max (0, std::min (kDetails - 1, d))];
}

void Harmonizer::setDetail (int d)
{
    pendingDetail.store (std::max (0, std::min (kDetails - 1, d)), std::memory_order_relaxed);
}

void Harmonizer::reset()
{
    std::fill (inRing.begin(),   inRing.end(),   0.0f);
    std::fill (dryRingL.begin(), dryRingL.end(), 0.0f);
    std::fill (dryRingR.begin(), dryRingR.end(), 0.0f);
    for (auto& o : olaRing) std::fill (o.begin(), o.end(), 0.0f);
    for (auto& d : dlyRing) std::fill (d.begin(), d.end(), 0.0f);
    analyser.reset();
    for (auto& s : shifter) s.reset();
    pos = 0; hopCount = 0; dlyPos = 0;
    idle = true;
    glideInit = false;
    for (int v = 0; v < kVoices; ++v)
    {
        gL[v] = gR[v] = 0.0f;
        dlyRead[v] = 0.0f;
        drift[v] = shimmer[v] = tract[v] = 0.0f;
    }
}

// ---------------------------------------------------------------------------
void Harmonizer::runFrame (const Params& p, int writePos)
{
    const float* win = window[activeDetail].data();

    //  the N most recent input samples, windowed
    const int start = (writePos - N) & ringMask;
    for (int i = 0; i < N; ++i)
        frameBuf[(size_t) i] = inRing[(size_t) ((start + i) & ringMask)] * win[i];

    analyser.analyse (frameBuf.data(), N, hop, fs);
    const Frame& f = analyser.frame();
    f0Out.store (f.f0, std::memory_order_relaxed);

    const float scale = olaScale[activeDetail];

    for (int v = 0; v < kVoices; ++v)
    {
        if (! p.v[v].on) { shifter[v].reset(); continue; }

        //  HUMANISE: a slow random walk per voice, refreshed once a frame.
        //  One-pole on white noise gives roughly a 1 Hz wander — the rate a
        //  singer's own tuning drifts at, not an LFO.
        auto rnd = [&]() -> float
        {
            rngState[v] = 1664525u * rngState[v] + 1013904223u;
            return (float) (rngState[v] / 2147483648.0) - 1.0f;
        };
        drift[v]   += 0.08f * (rnd() - drift[v]);
        shimmer[v] += 0.06f * (rnd() - shimmer[v]);
        tract[v]   += 0.05f * (rnd() - tract[v]);

        const float cents = p.v[v].cents + p.humanize * 14.0f * drift[v];
        const float pitch = std::pow (2.0f, (p.v[v].semitones + cents * 0.01f) / 12.0f);

        //  the tract follows the pitch by FOLLOW, then the formant knob moves
        //  it on top. FOLLOW 0 keeps the body still; FOLLOW 1 is resampling.
        const float fSemis = p.v[v].formant + p.humanize * 0.35f * tract[v];
        const float formant = std::pow (pitch, p.v[v].follow) * std::pow (2.0f, fSemis / 12.0f);

        shifter[v].render (f, hop, pitch, formant, specBuf.data());
        fft.inverse (specBuf.data(), N);

        auto& ola = olaRing[v];
        for (int i = 0; i < N; ++i)
            ola[(size_t) ((writePos + i) & ringMask)] += specBuf[(size_t) (2 * i)] * win[i] * scale;
    }
}

// ---------------------------------------------------------------------------
void Harmonizer::process (const Params& p,
                          const float* inL, const float* inR, int n,
                          float* harmL, float* harmR, float* dryL, float* dryR)
{
    const int want = pendingDetail.load (std::memory_order_relaxed);
    if (want != activeDetail)
    {
        activeDetail = want;
        N = sizes[activeDetail];
        hop = N / 8;
        reset();
    }

    bool any = false;
    for (int v = 0; v < kVoices; ++v) any = any || p.v[v].on;

    //  Waking from silence with a stale phase history would spend one frame
    //  computing nonsense frequencies. Start clean instead.
    if (any && idle) { analyser.reset(); for (auto& s : shifter) s.reset(); }
    idle = ! any;

    //  target gains and delays, recomputed per block; smoothed per sample
    float tgL[kVoices], tgR[kVoices], tgDly[kVoices];
    for (int v = 0; v < kVoices; ++v)
    {
        const float lvl = p.v[v].on ? dbToGain (p.v[v].gainDb) : 0.0f;
        const float sh  = 1.0f + p.humanize * 0.16f * shimmer[v];
        const float th  = (p.v[v].pan + 1.0f) * 0.25f * (float) M_PI;   // equal power
        tgL[v] = lvl * sh * std::cos (th);
        tgR[v] = lvl * sh * std::sin (th);
        const float ms = std::min (kMaxDelayMs, p.v[v].delayMs + p.humanize * kStagger[v]);
        //  kTapPad keeps the cubic read away from the write head at zero
        //  delay; the dry path carries the same two samples, so a voice at
        //  DELAY 0 lands exactly on the singer.
        tgDly[v] = (float) kTapPad + ms * 0.001f * (float) fs;
    }

    if (! glideInit)
    {
        for (int v = 0; v < kVoices; ++v) dlyRead[v] = tgDly[v];
        glideInit = true;
    }

    //  ~10 ms one-pole on gain, ~120 ms on the delay tap (a delay that jumps
    //  is a click; a delay that glides is a singer leaning in)
    const float aG = 1.0f - std::exp (-1.0f / (0.010f * (float) fs));
    const float aD = 1.0f - std::exp (-1.0f / (0.120f * (float) fs));
    const int   D  = N + kTapPad;

    for (int i = 0; i < n; ++i)
    {
        const float l = inL[i], r = inR[i];
        const float mono = 0.5f * (l + r);

        inRing[(size_t) pos]   = mono;
        dryRingL[(size_t) pos] = l;
        dryRingR[(size_t) pos] = r;

        const int rd = (pos - D) & ringMask;
        dryL[i] = dryRingL[(size_t) rd];
        dryR[i] = dryRingR[(size_t) rd];

        float sumL = 0.0f, sumR = 0.0f;
        for (int v = 0; v < kVoices; ++v)
        {
            const float y = olaRing[v][(size_t) pos];
            olaRing[v][(size_t) pos] = 0.0f;

            dlyRing[v][(size_t) dlyPos] = y;
            dlyRead[v] += aD * (tgDly[v] - dlyRead[v]);
            const float d = catmullRead (dlyRing[v], dlyMask, dlyPos, dlyRead[v]);

            gL[v] += aG * (tgL[v] - gL[v]);
            gR[v] += aG * (tgR[v] - gR[v]);
            sumL += d * gL[v];
            sumR += d * gR[v];
        }
        harmL[i] = sumL;
        harmR[i] = sumR;

        pos = (pos + 1) & ringMask;
        dlyPos = (dlyPos + 1) & dlyMask;

        if (++hopCount >= hop)
        {
            hopCount = 0;
            if (any) runFrame (p, pos);
        }
    }
}

} // namespace legion
