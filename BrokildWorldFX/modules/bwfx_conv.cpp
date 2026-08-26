#include "bwfx_conv.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace bwfx
{

// ---------------------------------------------------------------------------
// Iterative radix-2 complex FFT, interleaved re/im. Twiddles come from a
// double-precision per-stage recurrence: 10 sincos calls for N = 1024, and
// the rotation error stays below -100 dB — measured against direct
// convolution in the bench, which is the authority here.
void fftComplex (float* reim, int n, bool inverse)
{
    // bit-reversal permutation
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j)
        {
            std::swap (reim[2 * i],     reim[2 * j]);
            std::swap (reim[2 * i + 1], reim[2 * j + 1]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = (inverse ? 2.0 : -2.0) * 3.14159265358979323846 / len;
        const double wr = std::cos (ang), wi = std::sin (ang);
        for (int i = 0; i < n; i += len)
        {
            double cr = 1.0, ci = 0.0;
            for (int k = 0; k < len / 2; ++k)
            {
                float* a = reim + 2 * (i + k);
                float* b = reim + 2 * (i + k + len / 2);
                const double br = b[0] * cr - b[1] * ci;
                const double bi = b[0] * ci + b[1] * cr;
                const double ar = a[0], ai = a[1];
                a[0] = (float) (ar + br); a[1] = (float) (ai + bi);
                b[0] = (float) (ar - br); b[1] = (float) (ai - bi);
                const double ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }

    if (inverse)
    {
        const float s = 1.0f / (float) n;
        for (int i = 0; i < 2 * n; ++i) reim[i] *= s;
    }
}

// ---------------------------------------------------------------------------
namespace
{
    constexpr int kBins = PartConv::kHop + 1;          // half spectrum 0..N/2

    // FFT one real pair (a, b) of kFft-long blocks packed as a + ib, then
    // separate into two conjugate-symmetric half spectra (re/im per bin).
    void fftRealPair (const float* a, const float* b, int len,
                      float* specA, float* specB, float* work)
    {
        const int N = PartConv::kFft;
        for (int i = 0; i < N; ++i)
        {
            work[2 * i]     = i < len ? a[i] : 0.0f;
            work[2 * i + 1] = i < len ? (b != nullptr ? b[i] : 0.0f) : 0.0f;
        }
        fftComplex (work, N, false);
        for (int k = 0; k < kBins; ++k)
        {
            const int kc = (N - k) & (N - 1);
            const float xr = work[2 * k],  xi = work[2 * k + 1];
            const float yr = work[2 * kc], yi = work[2 * kc + 1];
            specA[2 * k]     = 0.5f * (xr + yr);
            specA[2 * k + 1] = 0.5f * (xi - yi);
            specB[2 * k]     = 0.5f * (xi + yi);
            specB[2 * k + 1] = 0.5f * (yr - xr);
        }
    }

    // Inverse: pack two half spectra as zL + i zR, IFFT, real -> a, imag -> b.
    void ifftRealPair (const float* specA, const float* specB,
                       float* a, float* b, float* work)
    {
        const int N = PartConv::kFft;
        for (int k = 0; k < kBins; ++k)
        {
            work[2 * k]     = specA[2 * k]     - specB[2 * k + 1];
            work[2 * k + 1] = specA[2 * k + 1] + specB[2 * k];
        }
        for (int k = kBins; k < N; ++k)
        {
            const int kc = N - k;
            work[2 * k]     = specA[2 * kc]      - (-specB[2 * kc + 1]);
            work[2 * k + 1] = -specA[2 * kc + 1] + specB[2 * kc];
        }
        fftComplex (work, N, true);
        for (int i = 0; i < N; ++i)
        {
            a[i] = work[2 * i];
            b[i] = work[2 * i + 1];
        }
    }
}

// ---------------------------------------------------------------------------
void PartConv::prepare (double fsr, float maxSeconds)
{
    nPartAlloc = (int) std::ceil (fsr * (double) maxSeconds / kHop) + 1;
    const size_t stride = (size_t) 2 * kBins;
    for (auto& s : sets) { s.data.assign ((size_t) nPartAlloc * stride * 2, 0.0f); s.nPart = 0; }
    histL.assign ((size_t) nPartAlloc * stride, 0.0f);
    histR.assign ((size_t) nPartAlloc * stride, 0.0f);
    workA.assign ((size_t) 2 * kFft, 0.0f);
    workB.assign ((size_t) 2 * kFft, 0.0f);
    accA.assign ((size_t) 2 * kBins, 0.0f);
    accB.assign ((size_t) 2 * kBins, 0.0f);
    live.store (-1, std::memory_order_release);
    applied = -1;
    reset();
}

void PartConv::reset()
{
    gathL.fill (0.0f); gathR.fill (0.0f);
    outAL.fill (0.0f); outAR.fill (0.0f);
    prevHopL.fill (0.0f); prevHopR.fill (0.0f);
    fill = 0;
    histPos = 0;
    histValid = 0;
    applied = live.load (std::memory_order_acquire);
    xfade = 1.0f;
    xfadeFrom = -1;
}

void PartConv::setImpulse (const float* irL, const float* irR, int frames)
{
    if (nPartAlloc == 0) return;
    const int cur = live.load (std::memory_order_acquire);
    const int next = cur == 0 ? 1 : 0;
    SpectraSet& s = sets[(size_t) next];
    const size_t stride = (size_t) 2 * kBins;

    int nPart = (frames + kHop - 1) / kHop;
    nPart = std::min (nPart, nPartAlloc);
    std::fill (s.data.begin(), s.data.end(), 0.0f);

    std::vector<float> chunkL (kHop, 0.0f), chunkR (kHop, 0.0f), work ((size_t) 2 * kFft);
    for (int k = 0; k < nPart; ++k)
    {
        const int base = k * kHop;
        const int m = std::min (kHop, frames - base);
        std::fill (chunkL.begin(), chunkL.end(), 0.0f);
        std::fill (chunkR.begin(), chunkR.end(), 0.0f);
        for (int i = 0; i < m; ++i) { chunkL[(size_t) i] = irL[base + i]; chunkR[(size_t) i] = irR[base + i]; }
        fftRealPair (chunkL.data(), chunkR.data(), kHop,
                     s.data.data() + (size_t) k * stride * 2,
                     s.data.data() + (size_t) k * stride * 2 + stride,
                     work.data());
    }
    s.nPart = nPart;
    live.store (next, std::memory_order_release);
}

void PartConv::macSet (const SpectraSet& s, float* accLp, float* accRp) const
{
    const size_t stride = (size_t) 2 * kBins;
    const int lim = std::min (s.nPart, histValid);
    for (int k = 0; k < lim; ++k)
    {
        const int h = histPos - k;
        const size_t hi = (size_t) (h < 0 ? h + nPartAlloc : h) * stride;
        const float* HL = s.data.data() + (size_t) k * stride * 2;
        const float* HR = HL + stride;
        const float* XL = histL.data() + hi;
        const float* XR = histR.data() + hi;
        for (int j = 0; j < kBins; ++j)
        {
            const float xlr = XL[2 * j], xli = XL[2 * j + 1];
            const float hlr = HL[2 * j], hli = HL[2 * j + 1];
            accLp[2 * j]     += xlr * hlr - xli * hli;
            accLp[2 * j + 1] += xlr * hli + xli * hlr;
            const float xrr = XR[2 * j], xri = XR[2 * j + 1];
            const float hrr = HR[2 * j], hri = HR[2 * j + 1];
            accRp[2 * j]     += xrr * hrr - xri * hri;
            accRp[2 * j + 1] += xrr * hri + xri * hrr;
        }
    }
}

void PartConv::renderHop()
{
    // FFT block = [previous hop, current hop] (overlap-save)
    float blockL[kFft], blockR[kFft];
    std::memcpy (blockL, prevHopL.data(), sizeof (float) * kHop);
    std::memcpy (blockR, prevHopR.data(), sizeof (float) * kHop);
    std::memcpy (blockL + kHop, gathL.data(), sizeof (float) * kHop);
    std::memcpy (blockR + kHop, gathR.data(), sizeof (float) * kHop);
    std::memcpy (prevHopL.data(), gathL.data(), sizeof (float) * kHop);
    std::memcpy (prevHopR.data(), gathR.data(), sizeof (float) * kHop);

    histPos = histPos + 1 == nPartAlloc ? 0 : histPos + 1;
    if (histValid < nPartAlloc) ++histValid;
    const size_t stride = (size_t) 2 * kBins;
    fftRealPair (blockL, blockR, kFft,
                 histL.data() + (size_t) histPos * stride,
                 histR.data() + (size_t) histPos * stride,
                 workA.data());

    // adopt a newly published IR set
    const int cur = live.load (std::memory_order_acquire);
    if (cur != applied)
    {
        if (applied >= 0) { xfadeFrom = applied; xfade = 0.0f; }
        applied = cur;
    }

    if (applied < 0)
    {
        outAL.fill (0.0f); outAR.fill (0.0f);
        return;
    }

    std::fill (accA.begin(), accA.end(), 0.0f);
    std::fill (accB.begin(), accB.end(), 0.0f);
    macSet (sets[(size_t) applied], accA.data(), accB.data());
    float newL[kFft], newR[kFft];
    ifftRealPair (accA.data(), accB.data(), newL, newR, workA.data());

    if (xfade < 1.0f && xfadeFrom >= 0)
    {
        std::fill (accA.begin(), accA.end(), 0.0f);
        std::fill (accB.begin(), accB.end(), 0.0f);
        macSet (sets[(size_t) xfadeFrom], accA.data(), accB.data());
        float oldL[kFft], oldR[kFft];
        ifftRealPair (accA.data(), accB.data(), oldL, oldR, workB.data());

        // ~85 ms at 48 k: 8 hops, ramped inside each hop so it never steps
        const float step = 1.0f / (8.0f * (float) kHop);
        float g = xfade;
        for (int i = 0; i < kHop; ++i)
        {
            const int j = kHop + i;                     // overlap-save: keep last half
            g = std::min (1.0f, g + step);
            newL[j] = oldL[j] + (newL[j] - oldL[j]) * g;
            newR[j] = oldR[j] + (newR[j] - oldR[j]) * g;
        }
        xfade = g;
        if (xfade >= 1.0f) xfadeFrom = -1;
    }

    for (int i = 0; i < kHop; ++i)
    {
        outAL[(size_t) i] = newL[kHop + i];
        outAR[(size_t) i] = newR[kHop + i];
    }
}

void PartConv::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (nPartAlloc == 0) return;
    for (int i = 0; i < n; ++i)
    {
        outL[i] += outAL[(size_t) fill];
        outR[i] += outAR[(size_t) fill];
        gathL[(size_t) fill] = inL[i];
        gathR[(size_t) fill] = inR[i];
        if (++fill == kHop)
        {
            renderHop();
            fill = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Deterministic stereo impulse — exact port of Photo-Synth 2's
// Engine::makeReverbImpulse (which itself ports the browser reverbImpulse()),
// including the WebAudio ConvolverNode normalisation (normalize = true).
int makeReverbImpulse (int type, float length, double fsr, std::vector<float>& out)
{
    const int frames = std::max (1, (int) std::round (fsr * length));
    out.assign ((size_t) frames * 2, 0.0f);
    float* ch[2] = { out.data(), out.data() + frames };

    if (type == 3)                                          // spring
    {
        for (int sc = 0; sc < 2; ++sc)
        {
            float* sd = ch[sc];
            double en = 0;
            const int period = (int) std::round (fsr * (0.045 + 0.004 * sc));
            const int chirpLen = (int) std::round (fsr * 0.035);
            const int nPulse = std::max (1, frames / std::max (1, period));
            for (int k = 0; k < nPulse; ++k)
            {
                const int t0 = k * period;
                const double amp = std::pow (0.8, k) * std::pow (1.0 - (double) t0 / frames, 1.4);
                double ph = sc * 1.7 + k * 0.31;
                for (int si = 0; si < chirpLen && t0 + si < frames; ++si)
                {
                    const double u = (double) si / chirpLen;
                    const double f = 3800.0 * std::pow (250.0 / 3800.0, u);
                    ph += 6.2831853 * f / fsr;
                    sd[t0 + si] += (float) (std::sin (ph) * amp
                                   * std::sin (3.14159265358979 * std::min (1.0, u * 4)) * (1 - 0.6 * u));
                }
            }
            uint32_t ss = (uint32_t) (0x51ab + sc * 0x9e3779b9);
            for (int si = 0; si < frames; ++si)
            {
                ss = 1664525u * ss + 1013904223u;
                sd[si] += (float) ((ss / 4294967296.0 * 2 - 1) * 0.05
                          * std::pow (1.0 - (double) si / frames, 3.0));
                en += (double) sd[si] * sd[si];
            }
            const double sk = en > 0 ? 0.75 / std::sqrt (en) : 1.0;
            for (int si = 0; si < frames; ++si) sd[si] = (float) (sd[si] * sk);
        }
    }
    else
    {
        const uint32_t seed = type == 1 ? 0x51f15e            // hall
                            : type == 2 ? 0x71a7e             // plate
                            : type == 4 ? 0x9e111 : 0x22334;  // reverse : room
        const bool rev = type == 4;
        for (int c = 0; c < 2; ++c)
        {
            float* d = ch[c];
            double energy = 0;
            uint32_t s = (uint32_t) (seed + (uint32_t) c * 0x9e3779b9u);
            const double decayPower = type == 1 ? 2.2 : type == 2 ? 1.35 : 3.2;
            for (int i = 0; i < frames; ++i)
            {
                s = 1664525u * s + 1013904223u;
                const double noise = s / 4294967296.0 * 2 - 1;
                const double envv = rev
                    ? std::pow ((double) i / frames, 2.4) * std::min (1.0, (double) (frames - i) / (fsr * 0.03))
                    : std::pow (1.0 - (double) i / frames, decayPower);
                double v = noise * envv;
                if (type == 0 && i < (int) (fsr * 0.08) && i % 997 == c * 137) v += 2.2 * envv;
                d[i] = (float) v;
                energy += v * v;
            }
            const double scale = energy > 0 ? 0.75 / std::sqrt (energy) : 1.0;
            for (int i = 0; i < frames; ++i) d[i] = (float) (d[i] * scale);
        }
    }

    double power = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < frames; ++i) power += (double) ch[c][i] * ch[c][i];
    power = std::sqrt (power / (2.0 * frames));
    power = std::max (power, 0.000125);
    double scale = 0.00125 / power;
    scale *= 44100.0 / fsr;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < frames; ++i) ch[c][i] = (float) (ch[c][i] * scale);
    return frames;
}

} // namespace bwfx
