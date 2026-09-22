#include "rop_rack.h"

#include <algorithm>
#include <cmath>

namespace rop
{

namespace
{
    //  §6 SPREAD: each slot leans a different way, so the two channels run the
    //  same score a few percent apart and the image opens BECAUSE the
    //  transition is happening.
    constexpr float kSlotLean[kSlots] = { 0.0f, 1.0f, -1.0f, 0.62f, -0.62f, 0.31f };
    constexpr float kMaxLean = 0.08f;          // at SPREAD 1, 8 % of the travel

    inline double beatsForGrid (int g) { return g == 0 ? 4.0 : (g == 1 ? 2.0 : 1.0); }

    inline float quantiseTo (float v, float step, float lo, float hi)
    {
        if (step <= 0.0f) return clampf (v, lo, hi);
        return clampf (lo + std::round ((v - lo) / step) * step, lo, hi);
    }

    //  a soft ceiling, so nothing this plugin does can leave the building
    //  above 0 dBFS. Not a limiter with a release — a shape, which cannot
    //  pump and cannot overshoot.
    inline float softClip (float x)
    {
        constexpr float t = 0.891f;    // -1 dBFS
        if (x >  t) return t + (1.0f - t) * std::tanh ((x - t) / (1.0f - t));
        if (x < -t) return -t - (1.0f - t) * std::tanh ((-x - t) / (1.0f - t));
        return x;
    }
}

Rack::Rack()
{
    for (auto& p : live) p.store (nullptr, std::memory_order_relaxed);
    for (auto& t : liveType) t = -1;
}
Rack::~Rack() = default;

// ---------------------------------------------------------------------------
void Rack::prepare (double sampleRate, int block)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    maxBlock = std::max (1, block);

    width.prepare (sr);
    bass.prepare (sr);
    bass.setCorner (bassMonoHz);
    widthSm.setTau (sr, 0.010); widthSm.snap (1.0f);
    mixSm.setTau (sr, 0.010);   mixSm.snap (mix);
    outSm.setTau (sr, 0.010);   outSm.snap (outputGain);
    meter.prepare (sr, 0.4);
    impRng.seed (0x13579BDFu);

    scratchL.assign ((size_t) kSubBlock, 0.0f);
    scratchR.assign ((size_t) kSubBlock, 0.0f);
    msKeep.assign ((size_t) kSubBlock, 0.0f);
    dryL.assign ((size_t) maxBlock, 0.0f);
    dryR.assign ((size_t) maxBlock, 0.0f);

    for (auto& e : owned) if (e) e->prepare (sr, kSubBlock);
    reset();
}

void Rack::reset()
{
    for (int i = 0; i < kSlots; ++i)
    {
        if (auto* e = live[(size_t) i].load (std::memory_order_relaxed)) e->reset();
        released[(size_t) i] = false;
        wasIn[(size_t) i] = false;
        for (int p = 0; p < kMaxParams; ++p) steppedValid[(size_t) i][(size_t) p] = false;
    }
    width.reset();
    bass.reset();
    meter.reset();
    didArrive = false;
    arrivalArmed.store (false, std::memory_order_relaxed);
    impEnv = 0.0; impPhase = 0.0;
    subPhase = 0;
    samplesSinceOrigin = 0;
    needResync = true;
}

/*  Is the rack doing anything at all? An empty one must be bit-transparent —
    the house contract — and the global field stage is part of "anything":
    summing the bass to mono is right when the plugin is working and wrong
    when it is asleep, because a plugin that quietly narrows a mix it was not
    asked to touch is a plugin nobody trusts on the master. */
bool Rack::fieldActive() const
{
    for (int i = 0; i < kSlots; ++i)
    {
        if (live[(size_t) i].load (std::memory_order_relaxed) == nullptr) continue;
        if (! st[(size_t) i].on) continue;
        if (released[(size_t) i] && st[(size_t) i].tail != Tail::Spill) continue;
        return true;
    }
    return impEnv > 1.0e-6 || std::abs (turn) > 1.0e-4 || monoGate > 1.0e-4;
}

// ---------------------------------------------------------------------------
void Rack::setSlotEffect (int slot, int type)
{
    if (slot < 0 || slot >= kSlots) return;

    Effect* old = live[(size_t) slot].load (std::memory_order_relaxed);

    if (type < 0 || type >= numEffects())
    {
        live[(size_t) slot].store (nullptr, std::memory_order_release);
        liveType[(size_t) slot] = -1;
    }
    else
    {
        std::unique_ptr<Effect> e (createEffect (type));
        if (! e) return;
        e->prepare (sr, kSubBlock);
        e->reset();
        Effect* raw = e.get();
        owned.push_back (std::move (e));
        live[(size_t) slot].store (raw, std::memory_order_release);
        liveType[(size_t) slot] = type;

        //  a fresh slot starts at the effect's own defaults on both sides, so
        //  assigning something never changes the sound until it is edited
        const auto& d = effectDescriptor (type);
        for (int p = 0; p < d.numParams; ++p)
            st[(size_t) slot].A[p] = st[(size_t) slot].B[p] = d.params[p].def;
    }

    //  retire the old instance for service() rather than freeing it under the
    //  audio thread's feet
    if (old != nullptr)
        for (auto it = owned.begin(); it != owned.end(); ++it)
            if (it->get() == old) { retired.push_back (std::move (*it)); owned.erase (it); break; }

    for (int p = 0; p < kMaxParams; ++p) steppedValid[(size_t) slot][(size_t) p] = false;
    released[(size_t) slot] = false;
    wasIn[(size_t) slot] = false;
}

int Rack::slotEffect (int slot) const
{
    return (slot >= 0 && slot < kSlots) ? liveType[(size_t) slot] : -1;
}

/*  MESSAGE THREAD, ~15 Hz, from the processor's timer — which already ran
    for the BWFX rack, so this pump costs no thread and no timer.

    The live slots are serviced BEFORE the retired ones are freed, and the
    retired ones are not serviced at all: they are about to stop existing
    and have no audio left to be ready for. */
void Rack::service()
{
    for (auto& a : live)
        if (auto* e = a.load (std::memory_order_acquire)) e->service();
    retired.clear();
}

// ---------------------------------------------------------------------------
/*  THE CLOCK IS DERIVED, NEVER ACCUMULATED.

    ppq = origin + samples * increment, where samples is an exact integer
    count. Two wrong versions came first and the bench caught both. Adding
    the increment block by block rounds differently at 64 samples than at
    256, and the plugin rendered differently in the two hosts. Taking the
    host's value fresh every block was better — 0.024 down to 0.0004 — but
    still not identical, because the host's arithmetic and ours disagree in
    the last bit and a beat boundary that lands within one bit of a sample
    edge falls on either side of it.

    So the host is asked WHERE IT IS only to seat the origin, and afterwards
    a multiply by an integer gives the same answer at every buffer size. The
    origin is re-seated when the host genuinely disagrees, which is a
    playhead jump (§9), or when the tempo changes. */
void Rack::setTransport (double b, double p, bool play)
{
    const double nb = (b > 1.0 && b < 999.0) ? b : 120.0;
    if (nb != bpm) { bpm = nb; needResync = true; }
    ppqPerSample = bpm / (60.0 * sr);
    playing = play;
    haveClock = (p >= 0.0);

    if (haveClock)
    {
        const double derived = ppqOrigin + (double) samplesSinceOrigin * ppqPerSample;
        const bool jumped = std::abs (p - derived) > 0.5;
        if (jumped && ! needResync) reset();          // a jump is not a transition
        if (jumped || needResync)
        {
            ppqOrigin = p;
            samplesSinceOrigin = 0;
            needResync = false;
        }
    }
    ppq = ppqOrigin + (double) samplesSinceOrigin * ppqPerSample;
}

// ---------------------------------------------------------------------------
// The score, §3. This is also what the bench calls directly to prove that a
// slot with ENTER 0.7 does nothing before 0.7.
void Rack::resolve (int slot, float t, float* out) const
{
    const int type = liveType[(size_t) slot];
    if (type < 0) return;
    const auto& d = effectDescriptor (type);
    const auto& s = st[(size_t) slot];

    const float span = std::max (1.0e-4f, s.exit - s.enter);
    const float raw = clampf ((t - s.enter) / span, 0.0f, 1.0f);

    for (int p = 0; p < d.numParams; ++p)
    {
        const int cv = s.paramCurve[p] >= 0 ? s.paramCurve[p] : s.curve;
        const float u = applyCurve (raw, cv) * s.depth;
        const auto& pd = d.params[p];

        float v;
        if (pd.warp == Warp::Log && s.A[p] > 1.0e-6f && s.B[p] > 1.0e-6f)
            v = s.A[p] * std::pow (s.B[p] / s.A[p], u);
        else
            v = lerpf (s.A[p], s.B[p], u);

        out[p] = clampf (v, std::min (pd.lo, pd.hi), std::max (pd.lo, pd.hi));
    }
}

// ---------------------------------------------------------------------------
void Rack::releaseSlot (int i)
{
    if (released[(size_t) i]) return;
    released[(size_t) i] = true;
    if (auto* e = live[(size_t) i].load (std::memory_order_acquire)) e->release (st[(size_t) i].tail);
}

void Rack::rearmSlot (int i)
{
    released[(size_t) i] = false;
    if (auto* e = live[(size_t) i].load (std::memory_order_acquire)) e->arm();
}

void Rack::fireArrival()
{
    for (int i = 0; i < kSlots; ++i) releaseSlot (i);
    didArrive = true;
    ++nArrivals;
    arrivalArmed.store (false, std::memory_order_relaxed);

    if (arrival.fireImpact)
    {
        impFreq  = std::max (20.0f, arrival.impactTune);
        impDecay = std::exp (-1.0 / (std::max (10.0f, arrival.impactDecay) * 0.001 * sr));
        impLevel = std::pow (10.0, arrival.impactLevel / 20.0);
        impEnv = 1.0;
        impPhase = 0.0;
    }
    //  the MONO GATE is released by the arrival, not by the slider (§6)
    widthSm.target = 1.0f;
}

// ---------------------------------------------------------------------------
void Rack::process (float* L, float* R, int n)
{
    if (n <= 0) return;
    if ((int) dryL.size() < n) { dryL.assign ((size_t) n, 0.0f); dryR.assign ((size_t) n, 0.0f); }

    std::copy (L, L + n, dryL.begin());
    std::copy (R, R + n, dryR.begin());

    //  with no host clock the same derivation free-runs, so no effect needs a
    //  "what if there is no transport" branch
    ppq = ppqOrigin + (double) samplesSinceOrigin * ppqPerSample;

    mixSm.target = clampf (mix, 0.0f, 1.0f);
    outSm.target = outputGain;
    bass.setCorner (bassMonoHz);

    /*  Sub-block edges are ABSOLUTE, not relative to the host's buffer. With
        them relative, a host running 100-sample buffers resolved the score at
        different instants than one running 256, and the output was not the
        same rendering. */
    int done = 0;
    while (done < n)
    {
        const int m = std::min (kSubBlock - subPhase, n - done);
        runSubBlock (L + done, R + done, m, done);
        subPhase = (subPhase + m) % kSubBlock;
        done += m;
    }
    samplesSinceOrigin += n;

    //  dry/wet, the field, the bass, the ceiling and the meter
    const bool field = fieldActive();
    for (int i = 0; i < n; ++i)
    {
        const float mx = mixSm.step();
        float l = lerpf (dryL[(size_t) i], L[i], mx);
        float r = lerpf (dryR[(size_t) i], R[i], mx);

        if (field && std::abs (turn) > 1.0e-4f)
            rotate (l, r, turn * 0.25f * (float) M_PI);

        //  MONO GATE: toward mono over the last of the travel, released by
        //  ARRIVAL. The width stage is energy preserving (§8.1), so the
        //  collapse cannot be a +6 dB jump.
        float w = 1.0f;
        if (field && monoGate > 1.0e-4f && ! didArrive)
        {
            const float start = 1.0f - clampf (monoGateSpan, 0.01f, 1.0f);
            const float g = clampf ((pos - start) / std::max (1.0e-4f, 1.0f - start), 0.0f, 1.0f);
            w = 1.0f - monoGate * g;
        }
        widthSm.target = w;
        const float ws = widthSm.step();
        if (field && ws < 0.999f) width.process (l, r, ws);

        if (field) bass.process (l, r);

        const float g = outSm.step();
        l = softClip (l * g);
        r = softClip (r * g);

        L[i] = l; R[i] = r;
        meter.push (l, r);
    }
}

// ---------------------------------------------------------------------------
void Rack::runSubBlock (float* L, float* R, int n, int blockOffset)
{
    const double subPpq = ppq + blockOffset * ppqPerSample;
    // ---- ARRIVAL, sample accurate and grid aligned (§4) -------------------
    if (arrivalArmed.load (std::memory_order_relaxed))
    {
        const double qb = beatsForGrid (arrival.grid);
        const double now = subPpq;
        const double nextBoundary = std::ceil (now / qb) * qb;
        const double beatsAway = nextBoundary - now;
        if (ppqPerSample > 0.0)
        {
            const double samplesAway = beatsAway / ppqPerSample;
            /*  The NEAREST sample to the boundary, and only if that sample is
                inside this sub-block. Rounding UP to the sub-block's own end
                and then clamping fired one sample early whenever the boundary
                sat in the last half sample of a sub-block — let it fall
                through instead, and the next sub-block rounds it to 0, which
                is the same instant. The single timing claim this plugin makes
                is that the arrival lands on the bar. */
            const long long round = std::llround (samplesAway);
            if (round < (long long) n)
            {
                const int at = (int) std::max (0LL, round);
                if (at > 0) runSubBlockRange (L, R, 0, at, subPpq);
                fireArrival();
                runSubBlockRange (L, R, at, n, subPpq);
                return;
            }
        }
    }
    runSubBlockRange (L, R, 0, n, subPpq);
}

void Rack::runSubBlockRange (float* L, float* R, int from, int to, double subPpq)
{
    const int n = to - from;
    if (n <= 0) return;
    float* pl = L + from;
    float* pr = R + from;

    Ctx c;
    c.fs = sr;
    c.bpm = bpm;
    c.ppq = subPpq + from * ppqPerSample;
    c.ppqPerSample = ppqPerSample;
    c.playing = playing;
    c.t = pos;

    /*  Stepped parameters adopt only on the grid (§3): a STUTTER changing
        division off the beat sounds broken. Without a host clock there is no
        grid, so they adopt at once. */
    const double tBeat0 = subPpq + from * ppqPerSample;
    const double tBeat1 = subPpq + to   * ppqPerSample;

    float pL[kMaxParams], pR[kMaxParams];

    for (int i = 0; i < kSlots; ++i)
    {
        auto* e = live[(size_t) i].load (std::memory_order_acquire);
        if (e == nullptr) continue;
        const auto& s = st[(size_t) i];
        if (! s.on) continue;

        const auto& d = e->desc();

        //  §6 SPREAD — the two channels read the score a few percent apart
        const float lean = spread * kMaxLean * kSlotLean[i];
        const float tl = clampf (pos + lean, 0.0f, 1.0f);
        const float tr = clampf (pos - lean, 0.0f, 1.0f);

        /*  LANE EDGES, AND THIS MUST COME BEFORE THE RELEASED-SKIP BELOW.
            A slot is released on the way OUT of its lane, and the only
            thing that can bring it back is the arm on the way IN — so if
            that decision sits after the `continue`, it is unreachable and
            the lane is dead for the rest of the session after one pass.
            Any lane with ENTER above 0 hit it; a lane starting at 0 never
            leaves, which is why it went unnoticed and why a check written
            with ENTER 0 cannot see it. */
        const bool in = (tl >= s.enter);
        if (in && ! wasIn[(size_t) i]) rearmSlot (i);
        if (! in && wasIn[(size_t) i]) releaseSlot (i);
        wasIn[(size_t) i] = in;

        if (released[(size_t) i] && s.tail != Tail::Spill) continue;

        resolve (i, tl, pL);
        if (d.perChannelParams && std::abs (lean) > 1.0e-6f) resolve (i, tr, pR);
        else                                                 std::copy (pL, pL + kMaxParams, pR);

        //  stepped parameters hold their adopted value until the grid says
        //  otherwise — a division that changes off the beat sounds broken
        const double qb = beatsForGrid (s.quantise);
        const bool crossed = ! haveClock
                           || (std::floor (tBeat1 / qb) != std::floor (tBeat0 / qb));

        for (int p = 0; p < d.numParams; ++p)
        {
            const auto& pd = d.params[p];
            if (pd.step <= 0.0f) continue;
            auto& held  = stepped[(size_t) i][(size_t) p];
            auto& valid = steppedValid[(size_t) i][(size_t) p];
            if (! valid || crossed) { held = quantiseTo (pL[p], pd.step, pd.lo, pd.hi); valid = true; }
            //  SPREAD moves continuous parameters only: the two sides at
            //  different divisions would be a rhythmic mess, not a width
            pL[p] = pR[p] = held;
        }

        c.inLane = in;   //  decided above, before the released-skip

        const float span = std::max (1.0e-4f, s.exit - s.enter);
        c.u = applyCurve (clampf ((tl - s.enter) / span, 0.0f, 1.0f), s.curve) * s.depth;

        //  §6 PLACE — a slot can work on the middle, the sides or one channel
        switch (s.place)
        {
            case Place::Stereo:
                e->process (pl, pr, n, pL, pR, c);
                break;

            case Place::Mid:
            case Place::Side:
            {
                for (int k = 0; k < n; ++k)
                {
                    float m, sd; toMS (pl[k], pr[k], m, sd);
                    scratchL[(size_t) k] = (s.place == Place::Mid) ? m : sd;
                    scratchR[(size_t) k] = scratchL[(size_t) k];
                    msKeep[(size_t) k] = (s.place == Place::Mid) ? sd : m;
                }
                e->process (scratchL.data(), scratchR.data(), n, pL, pR, c);
                for (int k = 0; k < n; ++k)
                {
                    const float a = scratchL[(size_t) k], b = msKeep[(size_t) k];
                    if (s.place == Place::Mid) fromMS (a, b, pl[k], pr[k]);
                    else                       fromMS (b, a, pl[k], pr[k]);
                }
                break;
            }

            case Place::Left:
            case Place::Right:
            {
                float* target = (s.place == Place::Left) ? pl : pr;
                for (int k = 0; k < n; ++k)
                    scratchL[(size_t) k] = scratchR[(size_t) k] = target[k];
                e->process (scratchL.data(), scratchR.data(), n, pL, pR, c);
                for (int k = 0; k < n; ++k) target[k] = scratchL[(size_t) k];
                break;
            }
        }
    }

    //  IMPACT — §4, fired by ARRIVAL and not by a slot
    if (impEnv > 1.0e-6)
    {
        for (int k = 0; k < n; ++k)
        {
            impPhase += 2.0 * M_PI * impFreq * (1.0 + 3.0 * impEnv * impEnv) / sr;
            if (impPhase > 2.0 * M_PI) impPhase -= 2.0 * M_PI;
            const double click = impEnv * impEnv * impEnv * impEnv * impRng.bip() * 0.35;
            const float v = (float) ((std::sin (impPhase) + click) * impEnv * impLevel);
            pl[k] += v; pr[k] += v;
            impEnv *= impDecay;
        }
    }

    //  the slider going back releases everything (§9)
    if (pos < 0.02f && didArrive) { didArrive = false; for (int i = 0; i < kSlots; ++i) rearmSlot (i); }
}

} // namespace rop
