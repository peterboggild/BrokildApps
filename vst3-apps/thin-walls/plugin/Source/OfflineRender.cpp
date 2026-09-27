#include "OfflineRender.h"
#include "LateRays.h"
#include "Fdtd.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

namespace tw
{

RenderQuality engineQualityFor (SoundQuality q)
{
    RenderQuality r;                                   // LIVE: the defaults
    if (q == SoundQuality::High)  { r.order = 4; r.maxPaths = 800;  r.maxSlots = 900; }
    if (q == SoundQuality::Ultra || q == SoundQuality::UltraBass) { r.order = 6; r.maxPaths = 1800; r.maxSlots = 2000; }
    return r;
}

namespace
{
    inline float dbLin (float db) { return std::pow (10.0f, db * 0.05f); }

    void fillBlock (const TakeView& T, int b, Params& P)
    {
        P = Params();
        if (T.paramsAt) T.paramsAt (b, P);
        const int s = b * T.pblock;
        int fi = -1;
        while (fi + 1 < (int) T.layouts.size() && T.layouts[(size_t) (fi + 1)].sample <= s) ++fi;
        P.nfurn = 0;
        for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = FurnItem();
        for (int r = 0; r < NUM_ROOMS; ++r) P.panelArea[r] = 0;
        if (fi >= 0)
        {
            const auto& L = T.layouts[(size_t) fi];
            P.nfurn = std::min (L.n, MAX_FURN);
            for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = (L.items != nullptr && i < L.n) ? L.items[i] : FurnItem();
            for (int r = 0; r < NUM_ROOMS; ++r) P.panelArea[r] = L.panelArea != nullptr ? L.panelArea[r] : 0.0f;
        }
    }

    float angDiff (float a, float b) { float d = std::fmod (std::abs (a - b), 360.0f); return d > 180.0f ? 360.0f - d : d; }

    // has anything the late field of source s depends on moved enough to trace again?
    bool differs (const Params& a, const Params& b, int s)
    {
        auto far = [] (float x0, float y0, float z0, float x1, float y1, float z1, float lim)
        { return (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1) + (z0 - z1) * (z0 - z1) > lim * lim; };
        if (far (a.lisX, a.lisY, 0, b.lisX, b.lisY, 0, 0.25f) || angDiff (a.lisYaw, b.lisYaw) > 10.0f) return true;
        const SourceParams &p = a.src[s], &q = b.src[s];
        if (far (p.x, p.y, p.z, q.x, q.y, q.z, 0.25f) || angDiff (p.yaw, q.yaw) > 10.0f) return true;
        if (p.type != q.type || std::abs (p.directivity - q.directivity) > 0.05f) return true;
        if (std::abs (a.earSpan - b.earSpan) > 0.01f) return true;
        for (int d = 0; d < NUM_DOORS; ++d) if (std::abs (a.door[d] - b.door[d]) > 0.04f) return true;
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            if (a.material[r] != b.material[r] || a.floorMat[r] != b.floorMat[r] || a.ceilMat[r] != b.ceilMat[r]) return true;
            if (std::abs (a.panelArea[r] - b.panelArea[r]) > 0.05f) return true;
            for (int k = 0; k < 2; ++k)
                if (std::abs (a.breakAlong[r][k] - b.breakAlong[r][k]) > 0.03f || std::abs (a.breakPush[r][k] - b.breakPush[r][k]) > 0.05f) return true;
        }
        if (a.nfurn != b.nfurn) return true;
        for (int i = 0; i < std::min (a.nfurn, MAX_FURN); ++i)
        {
            const FurnItem &f = a.furn[i], &g = b.furn[i];
            if (f.type != g.type || far (f.x, f.y, 0, g.x, g.y, 0, 0.05f) || angDiff (f.yaw, g.yaw) > 5.0f) return true;
        }
        return false;
    }

    struct Job { int src; int k0, k1; int startSample, endSample; int block; };

    /*  Where the wave simulation hands over to the geometric model. Linkwitz-Riley,
        4th order: the low and high halves sum flat in level (an all-pass), so
        a sound that both halves render alike comes out as it went in. */
    struct Crossover
    {
        static constexpr double HZ = 220.0;
        double b0, b1, b2, a1, a2;
        Crossover (double fs, bool low)
        {
            const double w = 2.0 * 3.141592653589793 * HZ / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * 0.7071067811865476);
            const double a0 = 1.0 + al;
            if (low) { b0 = (1.0 - cw) * 0.5; b1 = 1.0 - cw; } else { b0 = (1.0 + cw) * 0.5; b1 = -(1.0 + cw); }
            b2 = b0; b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
        }
        void one (float* x, int n) const
        {
            for (int pass = 0; pass < 2; ++pass)          // two Butterworth sections = LR4
            {
                double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                for (int i = 0; i < n; ++i)
                {
                    const double v = x[i], y = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                    x2 = x1; x1 = v; y2 = y1; y1 = y; x[i] = (float) y;
                }
            }
        }
        void run (float* l, float* r, int n) const { one (l, n); one (r, n); }
    };
}

bool renderTake (const TakeView& T, const RenderOptions& opt, RenderOutput& out,
                 std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    const int n = T.length;
    out.L.assign ((size_t) std::max (1, n), 0.0f);
    out.R.assign ((size_t) std::max (1, n), 0.0f);
    out.light.clear();
    out.keyframes = 0; out.raySeconds = 0;

    const bool ultra = opt.quality == SoundQuality::Ultra || opt.quality == SoundQuality::UltraBass;
    const bool bass = opt.quality == SoundQuality::UltraBass;
    const float engineShare = bass ? 0.2f : (ultra ? 0.3f : 1.0f);
    const float rayEnd = bass ? 0.5f : 1.0f;

    auto eng = std::make_unique<Engine>();
    eng->prepare (T.rate, T.pblock, engineQualityFor (opt.quality));
    if (opt.head != nullptr) eng->setHrtf (opt.head);

    int fi = -1, nextFrame = 0;
    Params P;
    for (int s = 0; s < n; s += T.pblock)
    {
        if (cancel != nullptr && cancel->load()) return false;
        const int m = std::min (T.pblock, n - s);
        const int b = std::min (s / T.pblock, T.nblocks - 1);
        if (b >= 0 && T.paramsAt) T.paramsAt (b, P);
        while (fi + 1 < (int) T.layouts.size() && T.layouts[(size_t) (fi + 1)].sample <= s) ++fi;
        if (fi >= 0)
        {
            const auto& L = T.layouts[(size_t) fi];
            P.nfurn = std::min (L.n, MAX_FURN);
            for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = (L.items != nullptr && i < L.n) ? L.items[i] : FurnItem();
            for (int r = 0; r < NUM_ROOMS; ++r) P.panelArea[r] = L.panelArea != nullptr ? L.panelArea[r] : 0.0f;
        }
        // ULTRA: the statistical late field and the door fields give way to the traced one
        if (ultra) P.reverbDb = -200.0f;
        // ULTRA+BASS: this pass is the air only, and the mix and output trims are applied at the end
        if (bass) { P.transmitDb = -200.0f; P.mix = 1.0f; P.outputDb = 0.0f; }
        eng->setParams (P);
        eng->process (T.in[0] + s, T.in[1] + s, T.in[2] ? T.in[2] + s : nullptr, T.in[3] ? T.in[3] + s : nullptr,
                      out.L.data() + s, out.R.data() + s, m);
        if (progress != nullptr) progress->store (engineShare * (float) (s + m) / (float) std::max (1, n));
        // the lamps' light at every video frame that falls in this block
        while (opt.fps > 0 && (double) nextFrame / opt.fps * T.rate < (double) (s + m))
        {
            for (int r = 0; r < NUM_ROOMS; ++r) out.light.push_back (eng->lightLevel (r));
            ++nextFrame;
        }
    }
    static const char* names[] = { "LIVE", "HIGH (reflections to 4th order)", "ULTRA (6th order, traced tails)", "ULTRA + BASS" };
    out.note = names[(int) opt.quality];
    if (! ultra || n <= 0 || T.nblocks <= 0) return true;

    //--------------------------------------------------------------------------
    // ULTRA: the late field, traced through the rooms as the take had them
    const auto t0 = std::chrono::steady_clock::now();
    const int pb = T.pblock, nb = T.nblocks;
    std::vector<Params> blocks ((size_t) nb);
    for (int b = 0; b < nb; ++b) fillBlock (T, b, blocks[(size_t) b]);

    // what each source is fed, at its level, and the gain the wet field comes out at
    std::vector<float> mono[MAX_SOURCES];
    std::vector<float> wetGain ((size_t) n);
    for (int b = 0; b < nb; ++b)
    {
        const Params& Q = blocks[(size_t) b];
        const Params& Qn = blocks[(size_t) std::min (b + 1, nb - 1)];
        // ULTRA+BASS applies the mix and the output at the very end, over every part
        const float g0 = (bass ? 1.0f : Q.mix * dbLin (Q.outputDb)) * dbLin (Q.reverbDb), g1 = (bass ? 1.0f : Qn.mix * dbLin (Qn.outputDb)) * dbLin (Qn.reverbDb);
        for (int i = b * pb; i < std::min (n, (b + 1) * pb); ++i) wetGain[(size_t) i] = g0 + (g1 - g0) * (float) (i - b * pb) / (float) pb;
    }
    bool anySource = false;
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        bool used = false;
        for (int b = 0; b < nb && ! used; ++b) used = blocks[(size_t) b].src[s].active();
        if (! used) continue;
        mono[s].assign ((size_t) n, 0.0f);
        for (int b = 0; b < nb; ++b)
        {
            const SourceParams& q = blocks[(size_t) b].src[s];
            const SourceParams& qn = blocks[(size_t) std::min (b + 1, nb - 1)].src[s];
            const float l0 = q.active() ? dbLin (q.levelDb) : 0.0f, l1 = qn.active() ? dbLin (qn.levelDb) : 0.0f;
            const float* a = nullptr; const float* c = nullptr;
            switch (q.input)
            {
                case IN_MAIN_L:  a = T.in[0]; break;
                case IN_MAIN_R:  a = T.in[1]; break;
                case IN_MAIN_LR: a = T.in[0]; c = T.in[1]; break;
                case IN_AUX_L:   a = T.in[2]; break;
                case IN_AUX_R:   a = T.in[3]; break;
                case IN_AUX_LR:  a = T.in[2]; c = T.in[3]; break;
                default: break;
            }
            if (a == nullptr) continue;
            for (int i = b * pb; i < std::min (n, (b + 1) * pb); ++i)
            {
                const float g = l0 + (l1 - l0) * (float) (i - b * pb) / (float) pb;
                const float v = c != nullptr ? 0.5f * (a[i] + c[i]) : a[i];
                mono[s][(size_t) i] = g * v;
            }
        }
        anySource = true;
    }
    if (! anySource) return true;

    // keyframes: trace again when something the field depends on has moved, at most every 0.1 s
    const int minGap = std::max (1, (int) std::ceil (0.1 * T.rate / pb));
    std::vector<Job> jobs;
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        if (mono[s].empty()) continue;
        std::vector<int> kb { 0 };
        for (int b = 1; b < nb; ++b)
            if (b - kb.back() >= minGap && differs (blocks[(size_t) kb.back()], blocks[(size_t) b], s)) kb.push_back (b);
        for (size_t k = 0; k < kb.size(); ++k)
        {
            Job j; j.src = s; j.block = kb[k];
            j.startSample = kb[k] * pb;
            j.endSample = k + 1 < kb.size() ? kb[k + 1] * pb : n;
            j.k0 = (int) k; j.k1 = (int) kb.size();
            jobs.push_back (j);
        }
    }
    out.keyframes = (int) jobs.size();

    // one IR length and one transform size for the whole take
    float tail = 0.3f;
    for (const Job& j : jobs) tail = std::max (tail, lateSeconds (blocks[(size_t) j.block]));
    const int Ns = std::max (1, (int) std::lround (0.001 * T.rate));
    const int irLen = (int) std::ceil (tail / 0.001f + 2) * Ns + 1024;
    const int synthN = fftc::nextPow2 (irLen);
    const int convN = synthN * 2;
    std::vector<std::complex<float>> heads;
    makeHeadSpectra (eng->hrtf(), blocks[0].earSpan, synthN, heads);

    std::vector<float> wetL ((size_t) n, 0.0f), wetR ((size_t) n, 0.0f);
    std::mutex addLock;
    std::atomic<int> nextJob { 0 }, doneJobs { 0 };
    std::atomic<bool> stop { false };
    const int xfMax = (int) (0.06 * T.rate);

    auto work = [&]()
    {
        std::vector<std::complex<float>> H ((size_t) convN), X ((size_t) convN);
        std::vector<float> irL, irR, seg;
        for (;;)
        {
            const int ji = nextJob.fetch_add (1);
            if (ji >= (int) jobs.size() || stop.load()) return;
            if (cancel != nullptr && cancel->load()) { stop = true; return; }
            const Job& J = jobs[(size_t) ji];
            // the window this keyframe owns, with raised-cosine crossfades at its inner edges
            const Job* prev = (J.k0 > 0) ? &jobs[(size_t) ji - 1] : nullptr;
            const Job* next = (J.k0 + 1 < J.k1) ? &jobs[(size_t) ji + 1] : nullptr;
            const int xfA = prev ? std::min (xfMax, std::min (J.endSample - J.startSample, prev->endSample - prev->startSample)) : 0;
            const int xfB = next ? std::min (xfMax, std::min (J.endSample - J.startSample, next->endSample - next->startSample)) : 0;
            const int a = std::max (0, J.startSample - xfA / 2), b = std::min (n, J.endSample + xfB / 2);
            seg.assign ((size_t) std::max (0, b - a), 0.0f);
            double segE = 0;
            for (int i = a; i < b; ++i)
            {
                float w = 1.0f;
                if (xfA > 0 && i < J.startSample + xfA / 2) { const float u = (float) (i - (J.startSample - xfA / 2)) / (float) xfA; w *= std::sin (0.5f * 3.14159265f * std::max (0.0f, std::min (1.0f, u))); w *= w; }
                if (xfB > 0 && i > J.endSample - xfB / 2) { const float u = (float) (i - (J.endSample - xfB / 2)) / (float) xfB; const float c = std::cos (0.5f * 3.14159265f * std::max (0.0f, std::min (1.0f, u))); w *= c * c; }
                seg[(size_t) (i - a)] = w * mono[J.src][(size_t) i];
                segE += (double) seg[(size_t) (i - a)] * seg[(size_t) (i - a)];
            }
            if (segE > 0)
            {
                LateRayOptions lo;
                lo.imageOrder = engineQualityFor (opt.quality).order;
                lo.maxSeconds = tail;
                lo.seed = 1u + (uint32_t) J.src;              // the same rays every keyframe: neighbours crossfade smoothly
                LateEnergy E;
                if (! bass)
                {
                    traceLate (blocks[(size_t) J.block], J.src, lo, E);
                    synthLateIr (E, heads, synthN, T.rate, irL, irR);
                }
                else
                {
                    // the air above the crossover (the wave simulation has it below), and
                    // what the walls pass at every frequency (the simulation has no walls to pass through)
                    LateEnergy Et;
                    traceLate (blocks[(size_t) J.block], J.src, lo, E, &Et);
                    synthLateIr (E, heads, synthN, T.rate, irL, irR);
                    Crossover hp (T.rate, false);
                    hp.run (irL.data(), irR.data(), (int) irL.size());
                    std::vector<float> tL, tR;
                    synthLateIr (Et, heads, synthN, T.rate, tL, tR);
                    for (size_t i = 0; i < irL.size() && i < tL.size(); ++i) { irL[i] += tL[i]; irR[i] += tR[i]; }
                }
                // the response's spectrum, both ears in one: FFT (L + i R)
                std::fill (H.begin(), H.end(), std::complex<float> (0, 0));
                for (int i = 0; i < synthN; ++i) H[(size_t) i] = std::complex<float> (irL[(size_t) i], irR[(size_t) i]);
                fftc::fft (H.data(), convN, false);
                // overlap-add in chunks: a real input times (HL + i HR) comes back as yL + i yR
                const int C = convN - synthN;
                for (int c0 = 0; c0 < (int) seg.size(); c0 += C)
                {
                    const int m = std::min (C, (int) seg.size() - c0);
                    std::fill (X.begin(), X.end(), std::complex<float> (0, 0));
                    for (int i = 0; i < m; ++i) X[(size_t) i] = std::complex<float> (seg[(size_t) (c0 + i)], 0.0f);
                    fftc::fft (X.data(), convN, false);
                    for (int k = 0; k < convN; ++k) X[(size_t) k] *= H[(size_t) k];
                    fftc::fft (X.data(), convN, true);
                    std::lock_guard<std::mutex> g (addLock);
                    const int base = a + c0;
                    for (int i = 0; i < convN && base + i < n; ++i)
                    {
                        wetL[(size_t) (base + i)] += X[(size_t) i].real();
                        wetR[(size_t) (base + i)] += X[(size_t) i].imag();
                    }
                }
            }
            const int d = doneJobs.fetch_add (1) + 1;
            if (progress != nullptr) progress->store (engineShare + (rayEnd - engineShare) * (float) d / (float) std::max<size_t> (1, jobs.size()));
        }
    };
    const int nth = std::max (1, std::min ((int) jobs.size(), (int) std::thread::hardware_concurrency() - 1));
    std::vector<std::thread> pool;
    for (int i = 1; i < nth; ++i) pool.emplace_back (work);
    work();
    for (auto& t : pool) t.join();
    if (stop.load() || (cancel != nullptr && cancel->load())) return false;

    out.raySeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    if (! bass)
    {
        for (int i = 0; i < n; ++i)
        {
            out.L[(size_t) i] += wetGain[(size_t) i] * wetL[(size_t) i];
            out.R[(size_t) i] += wetGain[(size_t) i] * wetR[(size_t) i];
        }
        return true;
    }

    //--------------------------------------------------------------------------
    // ULTRA+BASS. What arrives through the solid parts, at every frequency: the
    // engine once more, the air silenced (LIVE order is enough - these paths do not reflect)
    std::vector<float> bL ((size_t) n, 0.0f), bR ((size_t) n, 0.0f);
    {
        auto e2 = std::make_unique<Engine>();
        e2->prepare (T.rate, T.pblock);
        if (opt.head != nullptr) e2->setHrtf (opt.head);
        for (int b = 0; b < nb; ++b)
        {
            if (cancel != nullptr && cancel->load()) return false;
            const int s0 = b * pb, m = std::min (pb, n - s0);
            if (m <= 0) break;
            Params Q = blocks[(size_t) b];
            Q.reverbDb = -200.0f; Q.airborneDb = -200.0f; Q.mix = 1.0f; Q.outputDb = 0.0f;
            e2->setParams (Q);
            e2->process (T.in[0] + s0, T.in[1] + s0, T.in[2] ? T.in[2] + s0 : nullptr, T.in[3] ? T.in[3] + s0 : nullptr,
                         bL.data() + s0, bR.data() + s0, m);
        }
    }
    // the air below the crossover, as a wave
    std::vector<float> fL, fR;
    FdtdStats fst;
    if (! fdtdLowBand (blocks, pb, mono, n, T.rate, fL, fR, &fst, progress, rayEnd, 1.0f, cancel)) return false;
    out.fdtdSeconds = fst.seconds; out.fdtdCells = fst.cells; out.fdtdDx = fst.dx;
    Crossover lp (T.rate, true), hpA (T.rate, false);
    lp.run (fL.data(), fR.data(), n);
    hpA.run (out.L.data(), out.R.data(), n);        // the engine's airborne paths, above the crossover
    // together: the mix and the output trim over the whole of it
    for (int b = 0; b < nb; ++b)
    {
        const Params& Q = blocks[(size_t) b];
        const Params& Qn = blocks[(size_t) std::min (b + 1, nb - 1)];
        for (int i = b * pb; i < std::min (n, (b + 1) * pb); ++i)
        {
            const float u = (float) (i - b * pb) / (float) pb;
            const float mix = Q.mix + (Qn.mix - Q.mix) * u;
            const float og = dbLin (Q.outputDb) + (dbLin (Qn.outputDb) - dbLin (Q.outputDb)) * u;
            const size_t k = (size_t) i;
            const float wl = out.L[k] + bL[k] + wetGain[k] * wetL[k] + fL[k];
            const float wr = out.R[k] + bR[k] + wetGain[k] * wetR[k] + fR[k];
            out.L[k] = og * ((1.0f - mix) * T.in[0][i] + mix * wl);
            out.R[k] = og * ((1.0f - mix) * T.in[1][i] + mix * wr);
        }
    }
    return true;
    return true;
}

} // namespace tw
