#include "Engine.h"

namespace er
{

//==============================================================================
/*  The loom. Six bits in, eight wires out, always the same eight. Half of the
    wires are forced to land on a cell parameter so that no sigil is completely
    inert; the other half may go anywhere, the door and the traps included. */
void buildWiring (int sigil, Slot* out)
{
    Rng r;
    r.s = ((uint32_t) sigil * 2654435761u) ^ 0xa136aaadu;
    for (int i = 0; i < 6; ++i) r.u32();               // let the state stir

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        Slot s;
        s.src   = (int) (r.u32() % (uint32_t) NUM_SRC);
        s.dst   = (i < 4) ? (int) (r.u32() % 10u)
                          : (int) (r.u32() % (uint32_t) NUM_DST);
        s.curve = (int) (r.u32() % 3u);
        s.depth = (0.30f + 0.70f * r.uni()) * ((r.u32() & 1u) ? 1.0f : -1.0f);
        out[i] = s;
    }
}

namespace
{
    inline float curved (float v, int c)
    {
        switch (c)
        {
            case 1:  return v * std::abs (v);                       // squared, sign kept
            case 2:  return std::floor (v * 3.0f + 0.5f) / 3.0f;    // stepped
            default: return v;
        }
    }

    inline float noteToHz (int n) { return 440.0f * std::pow (2.0f, ((float) n - 69.0f) / 12.0f); }

    const float GLASS_RATIO[5] = { 1.0f, 1.414f, 1.987f, 2.618f, 3.401f };
    const float TIDE_RATIO[3]  = { 1.0f, 1.87f, 3.31f };
    const int   FDN_BASE[4]    = { 1237, 1583, 1949, 2381 };
}

//==============================================================================
Engine::Engine()
{
    setSigil (23);
    rng.s = 0x1badd00du;
}

void Engine::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate > 8000.0 ? sampleRate : 48000.0;

    for (auto& g : glass)  g.setSize ((int) (sr / 30.0));       // down to 30 Hz
    shBuf.setSize  ((int) (sr * 2.6));
    shBufR.setSize ((int) (sr * 2.6));
    chL.setSize    ((int) (sr * 2.6));
    chR.setSize    ((int) (sr * 2.6));
    for (auto& f : fdn) f.setSize ((int) (2400 * 3.2 * (sr / 44100.0)) + 64);
    shimBuf.setSize (SHIMW * 4);

    limRel   = 1.0f - std::exp (-1.0f / (float) (sr * 0.15));
    scopeSkip = std::max (1, (int) (sr * 0.030 / (double) SCOPE_LEN));

    reset();
}

void Engine::reset()
{
    panic();
    scope.fill (0.0f); scopeFill.fill (0.0f);
    scopeW = scopeCount = 0;
    for (auto& e : env) e = 0.0f;
    for (auto& g : gate) g = 0.0f;
    outRms = doorRing = 0.0f;
    ctrlCount = 0;
    controlTick();
}

void Engine::panic()
{
    for (auto& g : grains) g.on = false;
    ph1 = ph2 = y1 = y2 = 0; ironLp.clear(); ironDc.clear();
    for (auto& t : tide) t.clear();
    pinkA = pinkB = pinkC = 0;
    for (auto& g : glass)   g.clear();
    for (auto& g : glassLp) g.clear();
    mawHold = 0; mawCount = 0; mawDc.clear(); roomTail = 0;

    droneL.clear(); droneR.clear(); droneTL.clear(); droneTR.clear();
    for (auto& v : voices) { v.stage = 0; v.env = 0; v.note = -1; v.mL.clear(); v.mR.clear(); v.tL.clear(); v.tR.clear(); }

    shBuf.clear(); shBufR.clear(); shOn = false; shClock = shT = 0;
    chL.clear(); chR.clear(); chLpL.clear(); chLpR.clear(); chHpL.clear(); chHpR.clear();
    for (auto& f : fdn)   f.clear();
    for (auto& f : fdnLp) f.clear();
    shimBuf.clear(); shimPhase = 0;
    outDcL.clear(); outDcR.clear(); limEnv = 0;
    voiceEnv.fill (0.0f); voiceNote.fill (0.0f);
}

void Engine::setSigil (int s)
{
    s = std::max (0, std::min (63, s));
    if (s == curSigil) return;
    curSigil = s;
    buildWiring (s, slots.data());
}

//==============================================================================
void Engine::noteOn (int note, float velocity)
{
    // an already-sounding note retriggers rather than eating a second voice
    for (auto& v : voices)
        if (v.stage != 0 && v.note == note) { v.stage = 1; v.vel = velocity; v.age = ++voiceClock; return; }

    Voice* pick = nullptr;
    for (auto& v : voices) if (v.stage == 0) { pick = &v; break; }
    if (pick == nullptr)                                       // steal the oldest
    {
        pick = &voices[0];
        for (auto& v : voices) if (v.age < pick->age) pick = &v;
    }

    pick->note  = note;
    pick->vel   = 0.25f + 0.75f * velocity;
    pick->stage = 1;
    pick->age   = ++voiceClock;
    const float hz = noteToHz (note);
    pick->target = hz;
    if (pick->env <= 0.0001f) pick->hz = hz;                   // no glide from silence
}

void Engine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.stage != 0 && v.stage != 3 && v.note == note) v.stage = 3;
}

void Engine::allNotesOff()
{
    for (auto& v : voices) if (v.stage != 0) v.stage = 3;
}

//==============================================================================
void Engine::controlTick()
{
    const float ctrlRate = (float) sr / (float) CTRL;

    if ((int) p.sigil != curSigil) setSigil ((int) p.sigil);

    // ---- modulation sources -------------------------------------------------
    for (int i = 0; i < NUM_CELLS; ++i)
        srcVal[(size_t) i] = std::min (1.0f, env[(size_t) i] * 3.2f);

    {   // Rössler: three coupled outputs that never repeat but never wander off
        const float f  = xmap (p.rate, 0.03f, 5.0f);
        const float dt = std::min (0.03f, 6.0f * f / ctrlRate);
        const float dx = -chaosY - chaosZ;
        const float dy = chaosX + 0.2f * chaosY;
        const float dz = 0.2f + chaosZ * (chaosX - 5.7f);
        chaosX += dx * dt; chaosY += dy * dt; chaosZ += dz * dt;
        if (! (std::abs (chaosX) < 60.0f && std::abs (chaosY) < 60.0f && std::abs (chaosZ) < 90.0f))
        { chaosX = 0.1f; chaosY = 0.0f; chaosZ = 0.0f; }
        srcVal[5] = std::max (-1.0f, std::min (1.0f, chaosX * 0.09f));
        srcVal[6] = std::max (-1.0f, std::min (1.0f, chaosY * 0.09f));
        srcVal[7] = std::max (-1.0f, std::min (1.0f, (chaosZ - 2.5f) * 0.07f));
    }

    srcVal[8] = std::min (1.0f, doorEnvF * 2.0f);

    {   // two slow drunks, and one wandering goal per destination
        const float f = xmap (p.rate, 0.02f, 3.0f);
        if (--walkTick <= 0)
        {
            walkTick = std::max (2, (int) (ctrlRate / std::max (0.02f, f)));
            for (auto& g : driftGoal) g = rng.bi();
            lfoG[0] = rng.bi(); lfoG[1] = rng.bi();
        }
        const float k = std::min (0.35f, 4.0f * f / ctrlRate);
        for (size_t i = 0; i < driftW.size(); ++i) driftW[i] += (driftGoal[i] - driftW[i]) * k;
        lfoW[0] += (lfoG[0] - lfoW[0]) * k;
        lfoW[1] += (lfoG[1] - lfoW[1]) * k * 0.6f;
        srcVal[9]  = lfoW[0];
        srcVal[10] = lfoW[1];
    }

    srcVal[11] = std::min (1.0f, outEnvF * 2.5f);

    // ---- the loom -----------------------------------------------------------
    std::array<float, NUM_DST> acc {};
    for (const auto& s : slots)
        acc[(size_t) s.dst] += p.bind * s.depth * curved (srcVal[(size_t) s.src], s.curve) * 0.6f;

    for (size_t i = 0; i < acc.size(); ++i)
    {
        acc[i] += p.drift * driftW[i] * 0.45f;
        modOut[i] += (acc[i] - modOut[i]) * 0.05f;
    }

    auto M = [this] (int dst, float base) { return clamp01 (base + modOut[(size_t) dst]); };

    // ---- cells --------------------------------------------------------------
    for (int i = 0; i < NUM_CELLS; ++i)
    {
        gate[(size_t) i] += (p.cell[(size_t) i].on - gate[(size_t) i]) * 0.04f;
        const float lv = M (16 + i, p.cell[(size_t) i].lvl);
        d.lvl[(size_t) i] = lv * lv * 1.5f * gate[(size_t) i];
    }

    {   // I — dust
        const float a = M (0, p.cell[0].a), b = M (1, p.cell[0].b);
        d.dustDens  = xmap (a, 0.35f, 2600.0f);
        d.dustPitch = xmap (1.0f - b, 110.0f, 7500.0f);
        d.dustDec   = xmap (b, 0.0005f, 0.16f);
        d.dustGain  = 1.0f / std::sqrt (1.0f + d.dustDens / 26.0f);
    }
    {   // II — iron
        const float a = M (2, p.cell[1].a), b = M (3, p.cell[1].b);
        d.ironF1  = xmap (a, 17.0f, 2800.0f);
        d.ironF2  = d.ironF1 * (1.618f + a * 3.2f);
        d.ironIdx = b * b * 9.0f;
        ironLp.setCut (14000.0f, sr);
    }
    {   // III — tide
        const float a = M (4, p.cell[2].a), b = M (5, p.cell[2].b);
        const float base = xmap (a, 65.0f, 6000.0f);
        d.tideQ    = lerpf (2.0f, 46.0f, b);
        d.tideWalk = xmap (b, 0.05f, 9.0f);
        if (--tideTick <= 0)
        {
            tideTick = std::max (2, (int) (ctrlRate / std::max (0.05f, d.tideWalk)));
            for (auto& g : walkGoal3) g = rng.bi();
        }
        const float k = std::min (0.4f, 5.0f * d.tideWalk / ctrlRate);
        for (int i = 0; i < 3; ++i)
        {
            walk[(size_t) i] += (walkGoal3[(size_t) i] - walk[(size_t) i]) * k;
            d.tideHz[(size_t) i] = std::min ((float) sr * 0.42f,
                                             base * TIDE_RATIO[i] * std::pow (2.0f, walk[(size_t) i] * 1.3f));
            tide[(size_t) i].set (d.tideHz[(size_t) i], d.tideQ, sr);
        }
    }
    {   // IV — glass
        const float a = M (6, p.cell[3].a), b = M (7, p.cell[3].b);
        const float f0 = xmap (a, 42.0f, 1700.0f);
        for (int i = 0; i < 5; ++i)
        {
            d.glassLen[(size_t) i] = std::max (4.0f, (float) sr / (f0 * GLASS_RATIO[i]));
            glassLp[(size_t) i].setCut (xmap (b, 700.0f, 13000.0f), sr);
        }
        d.glassFb    = lerpf (0.82f, 0.9992f, b);
        d.glassSpark = xmap (1.0f - b, 0.6f, 14.0f) / (float) sr;   // sparks per sample
    }
    {   // V — maw
        const float a = M (8, p.cell[4].a), b = M (9, p.cell[4].b);
        d.mawSr   = 1 + (int) (a * a * 110.0f);
        d.mawQ    = std::pow (2.0f, lerpf (15.0f, 1.1f, a));
        d.mawFold = 1.0f + b * b * 7.5f;
        d.mawFeed = 0.15f + b * 1.75f;
    }

    // ---- Brokild World FX bus: copy once per control tick; neutral = inert --
    wmDet   = wmIn[0].load (std::memory_order_relaxed);
    wmPan   = wmIn[1].load (std::memory_order_relaxed);
    wmTremD = wmIn[2].load (std::memory_order_relaxed);
    wmTremR = wmIn[3].load (std::memory_order_relaxed);
    wmSag   = wmIn[4].load (std::memory_order_relaxed);
    wmFmul  = wmIn[5].load (std::memory_order_relaxed);
    wmActive = wmDet != 0.0f || wmPan != 0.0f || wmTremD != 0.0f
            || wmSag != 0.0f || wmFmul != 1.0f;
    if (wmActive) wmT += (double) CTRL / sr; else wmT = 0.0;

    // ---- the door -----------------------------------------------------------
    const float mCut = M (10, p.cut), mRes = M (11, p.res);
    d.doorFreeHz = xmap (mCut, 25.0f, 16000.0f);
    if (wmActive) d.doorFreeHz = std::max (15.0f, std::min ((float) sr * 0.44f, d.doorFreeHz * wmFmul));
    d.doorQ      = xmap (mRes, 0.55f, 58.0f);
    d.twinMul    = std::pow (2.0f, p.twin * 2.0f);
    d.twinAmt    = p.twinAmt;
    d.doorOct    = (p.envd * 2.0f - 1.0f) * 3.0f;
    d.atkC       = 1.0f - std::exp (-(float) CTRL / ((float) sr * xmap (p.atk, 0.0008f, 4.0f)));
    d.relC       = std::exp (-(float) CTRL * 6.908f / ((float) sr * xmap (p.rel, 0.004f, 8.0f)));
    d.glideC     = 1.0f - std::exp (-(float) CTRL / ((float) sr * xmap (p.glide, 0.0008f, 1.6f)));

    droneL.set  (d.doorFreeHz,          d.doorQ, sr);
    droneR.set  (d.doorFreeHz * 1.004f, d.doorQ, sr);
    droneTL.set (std::min ((float) sr * 0.44f, d.doorFreeHz * d.twinMul),          d.doorQ, sr);
    droneTR.set (std::min ((float) sr * 0.44f, d.doorFreeHz * d.twinMul * 0.996f), d.doorQ, sr);

    int active = 0;
    for (size_t vi = 0; vi < voices.size(); ++vi)
    {
        auto& v = voices[vi];
        if (v.stage == 0) { voiceEnv[vi] = 0.0f; voiceNote[vi] = 0.0f; continue; }
        ++active;

        // the loom moves freeHz, so at key < 1 it moves the played pitch too
        const float nHz = noteToHz (v.note);
        v.target = std::exp (lerpf (std::log (d.doorFreeHz), std::log (nHz), clamp01 (p.key)));

        float lg = std::log (std::max (10.0f, v.hz));
        lg += (std::log (v.target) - lg) * d.glideC;
        v.hz = std::exp (lg);

        if      (v.stage == 1) { v.env += (1.05f - v.env) * d.atkC; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 2; } }
        else if (v.stage == 3) { v.env *= d.relC; if (v.env < 0.0006f) { v.env = 0.0f; v.stage = 0; v.note = -1; } }

        float wmMul = 1.0f;
        v.wmGL = v.wmGR = 1.0f;
        if (wmActive)
        {
            // sag keys to the smoothed GATE (attack/hold = held): the door
            // stays in tune while held and sinks as the voice releases
            const float gk = 1.0f - std::exp (-(float) CTRL / (0.05f * (float) sr));
            v.wmGate += gk * ((v.stage == 1 || v.stage == 2 ? 1.0f : 0.0f) - v.wmGate);
            const float fan = std::fmod ((float) vi * 0.6180339887f + 0.5f, 1.0f) * 2.0f - 1.0f;
            const float centsV = wmDet * fan - wmSag * 100.0f * (1.0f - v.wmGate);
            wmMul = std::pow (2.0f, centsV / 1200.0f);
            float g = 1.0f;
            if (wmTremD > 0.0f && wmTremR > 0.0f)
            {
                const float u = std::fmod ((float) vi * 0.6180339887f + 0.71f, 1.0f);
                g = 1.0f - wmTremD * (0.5f - 0.5f * std::sin (6.2831853f * wmTremR * (0.75f + 0.5f * u) * (float) wmT + (float) vi * 2.39996f));
            }
            const float pan = wmPan * (std::fmod ((float) vi * 0.6180339887f + 0.21f, 1.0f) * 2.0f - 1.0f);
            v.wmGL = g * std::min (1.0f, 1.0f - pan);
            v.wmGR = g * std::min (1.0f, 1.0f + pan);
        }
        const float fc = std::max (15.0f, std::min ((float) sr * 0.44f,
                                                    v.hz * wmMul * std::pow (2.0f, d.doorOct * v.env)));
        v.mL.set (fc, d.doorQ, sr);
        v.mR.set (fc * 1.004f, d.doorQ, sr);
        v.tL.set (std::min ((float) sr * 0.44f, fc * d.twinMul), d.doorQ, sr);
        v.tR.set (std::min ((float) sr * 0.44f, fc * d.twinMul * 0.996f), d.doorQ, sr);

        voiceEnv[vi]  = v.env;
        voiceNote[vi] = v.hz;
    }
    doorNorm = 1.0f / (1.0f + 0.6f * (float) active + 0.5f * p.floorLvl);

    // ---- traps --------------------------------------------------------------
    d.shAmt  = M (12, p.shAmt);
    d.shSlice = xmap (p.shSize, 0.018f, 0.45f) * (float) sr;

    const float chT = M (13, p.chTime);
    d.chDelay = xmap (chT, 0.007f, 1.9f) * (float) sr;
    d.chFb    = std::min (1.02f, p.chFeed * 0.99f);
    d.chWet   = std::min (1.0f, p.chFeed * 2.2f);
    chLpL.setCut (xmap (p.chTone, 380.0f, 16000.0f), sr);
    chLpR.setCut (xmap (p.chTone, 380.0f, 16000.0f), sr);
    chHpL.setCut (xmap (1.0f - p.chTone, 18.0f, 900.0f), sr);
    chHpR.setCut (xmap (1.0f - p.chTone, 18.0f, 900.0f), sr);

    {
        const float scale = xmap (p.vaSize, 0.26f, 3.0f) * (float) (sr / 44100.0);
        const float rt    = xmap (p.vaSize, 0.35f, 16.0f);
        for (int i = 0; i < 4; ++i)
        {
            d.fdnLen[(size_t) i] = std::max (16, (int) ((float) FDN_BASE[i] * scale));
            const float g = std::pow (10.0f, -3.0f * (float) d.fdnLen[(size_t) i] / ((float) sr * rt));
            if (i == 0) d.fdnG = g;
            fdnLp[(size_t) i].setCut (xmap (1.0f - p.vaSize, 2200.0f, 12000.0f), sr);
        }
        d.vaMix  = M (14, p.vaMix);
        d.vaShim = p.vaShim;
    }

    // ---- the way out --------------------------------------------------------
    const float dr = M (15, p.drive);
    d.drive     = 1.0f + dr * 11.0f;
    d.driveComp = 1.0f / (0.38f + 0.62f * std::sqrt (d.drive));
    d.outGain   = p.gain * p.gain * 1.5f;

    // meters
    for (int i = 0; i < NUM_CELLS; ++i) cellRms[(size_t) i] = std::min (1.0f, env[(size_t) i] * 3.2f);
    outRms   = std::min (1.0f, outEnvF * 2.5f);
    doorRing = std::min (1.0f, doorEnvF * 2.5f);
}

//==============================================================================
void Engine::dustS (float& l, float& r)
{
    dustClock += d.dustDens / (float) sr;
    while (dustClock >= 1.0f)
    {
        dustClock -= 1.0f;
        Grain* g = nullptr;
        for (auto& c : grains) if (! c.on) { g = &c; break; }
        if (g == nullptr)                                        // steal the quietest
        {
            g = &grains[0];
            for (auto& c : grains) if (c.env < g->env) g = &c;
        }
        g->on  = true;
        g->ph  = 0.0f;
        g->inc = d.dustPitch * (0.55f + rng.uni() * 1.1f) / (float) sr;
        g->dec = std::exp (-1.0f / (d.dustDec * (float) sr));
        g->env = 1.0f;
        g->amp = (0.35f + 0.65f * rng.uni()) * ((rng.u32() & 1u) ? 1.0f : -1.0f);
        g->pan = rng.uni();
    }

    float sl = 0.0f, sr_ = 0.0f;
    for (auto& g : grains)
    {
        if (! g.on) continue;
        g.env *= g.dec;
        if (g.env < 0.0004f) { g.on = false; continue; }
        g.ph += g.inc;
        if (g.ph >= 1.0f) g.ph -= std::floor (g.ph);
        const float v = (std::sin (6.2831853f * g.ph) * 0.75f + rng.bi() * 0.25f) * g.env * g.amp;
        sl += v * (1.0f - g.pan);
        sr_ += v * g.pan;
    }
    l = sl * d.dustGain * 1.4f;
    r = sr_ * d.dustGain * 1.4f;
}

void Engine::ironS (float& l, float& r)
{
    const float lim = (float) sr * 0.44f;
    float f1 = d.ironF1 * (1.0f + d.ironIdx * y2);
    float f2 = d.ironF2 * (1.0f + d.ironIdx * y1);
    f1 = std::max (-lim, std::min (lim, f1));
    f2 = std::max (-lim, std::min (lim, f2));

    ph1 += f1 / (float) sr; ph1 -= std::floor (ph1);
    ph2 += f2 / (float) sr; ph2 -= std::floor (ph2);
    y1 = std::sin (6.2831853f * ph1);
    y2 = std::sin (6.2831853f * ph2);

    const float m = ironDc (ironLp.lp (0.5f * (y1 + y2 * 0.85f)));
    l = m * 0.6f + y1 * 0.32f;
    r = m * 0.6f + y2 * 0.32f;
}

void Engine::tideS (float& l, float& r)
{
    const float w = rng.bi();
    pinkA = 0.99765f * pinkA + w * 0.0990460f;
    pinkB = 0.96300f * pinkB + w * 0.2965164f;
    pinkC = 0.57000f * pinkC + w * 1.0526913f;
    const float pink = (pinkA + pinkB + pinkC + w * 0.1848f) * 0.16f;

    tide[0].process (pink); tide[1].process (pink); tide[2].process (pink);
    const float b0 = tide[0].bp, b1 = tide[1].bp * 0.8f, b2 = tide[2].bp * 0.6f;
    l = (b0 * 0.9f + b1 * 0.6f + b2 * 0.25f) * 0.9f;
    r = (b0 * 0.25f + b1 * 0.6f + b2 * 0.9f) * 0.9f;
}

void Engine::glassS (float& l, float& r, float excite)
{
    glassClock += d.glassSpark;
    float spark = 0.0f;
    if (glassClock >= 1.0f) { glassClock -= 1.0f; sparkLeft = (int) (0.0016f * sr); }
    if (sparkLeft > 0) { --sparkLeft; spark = rng.bi() * 0.8f; }

    const float in = excite * 0.55f + spark;
    float sl = 0.0f, sr_ = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float y = glassLp[(size_t) i].lp (glass[(size_t) i].readF (d.glassLen[(size_t) i]));
        glass[(size_t) i].write (softclip (in * 0.3f + y * d.glassFb));
        const float pan = (float) i / 4.0f;
        sl += y * (1.0f - pan * 0.85f);
        sr_ += y * (0.15f + pan * 0.85f);
    }
    l = sl * 0.32f;
    r = sr_ * 0.32f;
}

float Engine::mawS (float roomIn)
{
    const float in = roomIn * d.mawFeed;
    if (--mawCount <= 0) { mawCount = d.mawSr; mawHold = in; }
    const float q = d.mawQ;
    float v = std::floor (mawHold * q + 0.5f) / q;
    v = wavefold (v * d.mawFold);
    return mawDc (v) * 0.85f;
}

//==============================================================================
void Engine::process (float* left, float* right, int numSamples)
{
    const int mode = p.mode;

    for (int n = 0; n < numSamples; ++n)
    {
        if (ctrlCount == 0) controlTick();
        ctrlCount = (ctrlCount + 1) % CTRL;

        // ---- the cells ------------------------------------------------------
        float d1l, d1r, d2l, d2r, d3l, d3r, d4l, d4r;
        dustS (d1l, d1r);
        ironS (d2l, d2r);
        tideS (d3l, d3r);
        // CELL IV listens to CELL I whether or not CELL I is switched on
        glassS (d4l, d4r, (d1l + d1r) * 0.5f);
        const float d5 = mawS (roomTail);

        float roomL = d1l * d.lvl[0] + d2l * d.lvl[1] + d3l * d.lvl[2] + d4l * d.lvl[3] + d5 * d.lvl[4];
        float roomR = d1r * d.lvl[0] + d2r * d.lvl[1] + d3r * d.lvl[2] + d4r * d.lvl[3] + d5 * d.lvl[4];

        env[0] += (std::abs (d1l + d1r) * 0.5f * gate[0] - env[0]) * 0.0007f;
        env[1] += (std::abs (d2l + d2r) * 0.5f * gate[1] - env[1]) * 0.0007f;
        env[2] += (std::abs (d3l + d3r) * 0.5f * gate[2] - env[2]) * 0.0007f;
        env[3] += (std::abs (d4l + d4r) * 0.5f * gate[3] - env[3]) * 0.0007f;
        env[4] += (std::abs (d5) * gate[4] - env[4]) * 0.0007f;

        // ---- the door -------------------------------------------------------
        float dl = 0.0f, dr = 0.0f;
        {
            droneL.process (roomL); droneR.process (roomR);
            float fl = droneL.pick (mode), fr = droneR.pick (mode);
            if (d.twinAmt > 0.002f)
            {
                droneTL.process (roomL); droneTR.process (roomR);
                fl = fl * (1.0f - 0.55f * d.twinAmt) + droneTL.pick (mode) * 0.95f * d.twinAmt;
                fr = fr * (1.0f - 0.55f * d.twinAmt) + droneTR.pick (mode) * 0.95f * d.twinAmt;
            }
            dl += fl * p.floorLvl; dr += fr * p.floorLvl;

            for (auto& v : voices)
            {
                if (v.stage == 0) continue;
                v.mL.process (roomL); v.mR.process (roomR);
                float vl = v.mL.pick (mode), vr = v.mR.pick (mode);
                if (d.twinAmt > 0.002f)
                {
                    v.tL.process (roomL); v.tR.process (roomR);
                    vl = vl * (1.0f - 0.55f * d.twinAmt) + v.tL.pick (mode) * 0.95f * d.twinAmt;
                    vr = vr * (1.0f - 0.55f * d.twinAmt) + v.tR.pick (mode) * 0.95f * d.twinAmt;
                }
                const float a = v.env * v.vel;
                dl += vl * a * v.wmGL; dr += vr * a * v.wmGR;
            }
            dl *= doorNorm; dr *= doorNorm;
        }

        roomTail = (dl + dr) * 0.5f;
        doorEnvF += (std::abs (roomTail) - doorEnvF) * 0.0007f;

        // ---- trap 1: shatter ------------------------------------------------
        float sl = dl, sr2 = dr;
        shBuf.write (dl); shBufR.write (dr);
        shClock += 1.0f;
        if (shClock >= d.shSlice)
        {
            shClock = 0.0f;
            if (rng.uni() < d.shAmt)
            {
                shOn = true; shT = 0.0f;
                const float u = rng.uni();
                shSpeed = u < 0.45f ? 1.0f : (u < 0.70f ? -1.0f : (u < 0.88f ? 2.0f : 0.5f));
            }
            else shOn = false;
        }
        if (shOn)
        {
            const float dly = std::max (2.0f, d.shSlice + shT * (1.0f - shSpeed));
            sl  = lerpf (dl, shBuf.readF (dly),  d.shAmt);
            sr2 = lerpf (dr, shBufR.readF (dly), d.shAmt);
            shT += 1.0f;
            if (std::abs (shSpeed) * shT >= d.shSlice) shT = 0.0f;
        }

        // ---- trap 2: chasm --------------------------------------------------
        float cl = sl, cr = sr2;
        if (d.chWet > 0.0005f || d.chFb > 0.0005f)
        {
            chPhase += 6.2831853f * 0.27f / (float) sr;
            if (chPhase > 6.2831853f) chPhase -= 6.2831853f;
            const float wob = 1.0f + 0.0035f * std::sin (chPhase);
            const float wl = chL.readF (d.chDelay * wob);
            const float wr = chR.readF (d.chDelay * wob * 1.007f);
            const float fl = chHpL.hp (chLpL.lp (wl));
            const float fr = chHpR.hp (chLpR.lp (wr));
            chL.write (softclip (sl + fr * d.chFb * 0.80f + fl * d.chFb * 0.30f));
            chR.write (softclip (sr2 + fl * d.chFb * 0.80f + fr * d.chFb * 0.30f));
            cl = sl + wl * d.chWet;
            cr = sr2 + wr * d.chWet;
        }

        // ---- trap 3: vapour -------------------------------------------------
        float vl = cl, vr = cr;
        if (d.vaMix > 0.0015f)
        {
            const float s0 = fdnLp[0].lp (fdn[0].readI (d.fdnLen[0]));
            const float s1 = fdnLp[1].lp (fdn[1].readI (d.fdnLen[1]));
            const float s2 = fdnLp[2].lp (fdn[2].readI (d.fdnLen[2]));
            const float s3 = fdnLp[3].lp (fdn[3].readI (d.fdnLen[3]));
            const float h  = 0.5f * (s0 + s1 + s2 + s3);

            float shim = 0.0f;
            if (d.vaShim > 0.002f)
            {
                shimBuf.write (h * 0.5f);
                shimPhase += 1.0f / (float) SHIMW;
                if (shimPhase >= 1.0f) shimPhase -= 1.0f;
                const float pa = shimPhase;
                const float pb = (pa < 0.5f) ? pa + 0.5f : pa - 0.5f;
                const float wa = 0.5f - 0.5f * std::cos (6.2831853f * pa);
                const float wb = 0.5f - 0.5f * std::cos (6.2831853f * pb);
                shim = (shimBuf.readF ((float) SHIMW * (1.0f - pa) + 3.0f) * wa
                      + shimBuf.readF ((float) SHIMW * (1.0f - pb) + 3.0f) * wb) * d.vaShim * 0.85f;
            }

            const float inM = (cl + cr) * 0.35f;
            fdn[0].write (softclip (inM + cl * 0.18f + (s0 - h + shim) * d.fdnG));
            fdn[1].write (softclip (inM - cr * 0.18f + (s1 - h + shim) * d.fdnG));
            fdn[2].write (softclip (inM + cr * 0.18f + (s2 - h - shim) * d.fdnG));
            fdn[3].write (softclip (inM - cl * 0.18f + (s3 - h - shim) * d.fdnG));

            vl = lerpf (cl, (s0 + s2) * 0.62f, d.vaMix);
            vr = lerpf (cr, (s1 + s3) * 0.62f, d.vaMix);
        }

        // ---- the way out ----------------------------------------------------
        float ol = outDcL (softclip (vl * d.drive) * d.driveComp) * d.outGain;
        float orr = outDcR (softclip (vr * d.drive) * d.driveComp) * d.outGain;

        const float pk = std::max (std::abs (ol), std::abs (orr));
        if (pk > limEnv) limEnv = pk; else limEnv += (pk - limEnv) * limRel;
        if (limEnv > 0.90f) { const float g = 0.90f / limEnv; ol *= g; orr *= g; }
        ol  = std::max (-1.0f, std::min (1.0f, ol));
        orr = std::max (-1.0f, std::min (1.0f, orr));

        outEnvF += (pk - outEnvF) * 0.0007f;

        left[n]  = ol;
        right[n] = orr;

        if (++scopeCount >= scopeSkip)
        {
            scopeCount = 0;
            scopeFill[(size_t) scopeW] = (ol + orr) * 0.5f;
            if (++scopeW >= SCOPE_LEN) { scopeW = 0; scope = scopeFill; }
        }
    }
}

} // namespace er
