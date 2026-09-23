#include "Engine.h"

namespace br
{

namespace
{
    /*  Nine voicings of the same root. TRITONE is the one that makes the city
        feel wrong; CLUSTER is the one that makes it feel crowded. */
    const int CHORD[NUM_CHORDS][LA_STACK] = {
        {  0, 12, 24, -12,  0, 12, 24, -24,  7 },   // OCTAVES
        {  0,  7, 12,  19, 24, -12, -5,   7, 12 },  // FIFTHS
        {  0,  3,  7,  12, 15,  19, 24, -12,  7 },  // MINOR
        {  0,  5,  7,  12, 17,  19, 24, -12,  5 },  // SUSPENDED
        {  0,  1,  2,   7, 12,  13, 14, -12,  0 },  // CLUSTER
        {  0,  6, 12,  18, 24, -12,  6,   7,  0 }   // TRITONE
    };
    const char* CHORD_NAME[NUM_CHORDS] = { "OCTAVES", "FIFTHS", "MINOR", "SUSPENDED", "CLUSTER", "TRITONE" };
    const char* WAVE_NAME [NUM_WAVES]  = { "SAW", "PULSE", "SAW+PULSE", "RING" };
    const char* RATE_NAME [NUM_RATES]  = { "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/32" };
    const float RATE_BEATS[NUM_RATES]  = { 2.0f, 1.0f, 0.666667f, 0.5f, 0.333333f, 0.25f, 0.125f };

    /*  Ratios a bell has and a piano does not. Walking METAL up the scale
        walks the voice from a plain octave into something with no fundamental
        you can name. */
    const float FM_RATIO[8] = { 1.0f, 2.0f, 1.414f, 3.0f, 3.162f, 4.236f, 5.196f, 7.071f };

    inline int   clampi (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float cents  (float c)               { return std::pow (2.0f, c / 1200.0f); }
}

const char* Engine::chordName (int i) { return CHORD_NAME[clampi (i, 0, NUM_CHORDS - 1)]; }
const char* Engine::waveName  (int i) { return WAVE_NAME [clampi (i, 0, NUM_WAVES  - 1)]; }
const char* Engine::rateName  (int i) { return RATE_NAME [clampi (i, 0, NUM_RATES  - 1)]; }
float       Engine::rateBeats (int i) { return RATE_BEATS[clampi (i, 0, NUM_RATES  - 1)]; }

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    const int cap = std::max (64, maxBlock);
    tmpL.assign ((size_t) cap, 0.0f);
    tmpR.assign ((size_t) cap, 0.0f);

    // ---- Los Angeles
    la.rng.seed (0x1F2E3D4Cu);
    for (int i = 0; i < LA_STACK; ++i)
    {
        la.osc[i].clear();
        la.osc[i].phase = la.rng.uni();
        la.driftPh[i]  = la.rng.uni();
        // 40 to 120 seconds a cycle: slow enough that you never catch it moving
        la.driftInc[i] = (0.0083f + la.rng.uni() * 0.017f) / (float) sr;
    }
    la.sub.clear();
    for (auto& f : la.smog) f.clear();
    la.rumble.setHz (28.0f, sr);
    la.verbHp.setHz (120.0f, sr);
    for (int i = 0; i < 3; ++i) { la.rainBp[i].clear(); la.rainPh[i] = la.rng.uni(); }
    la.rainLp.setHz (5200.0f, sr);
    for (auto& g : la.grain) { g.on = false; g.o.clear(); g.bp.clear(); }
    la.grainClock = 0.0f;
    la.env.set (2.6f, 1.0f, 1.0f, 4.5f, sr);
    la.env.clear();
    la.open = false;
    la.comb.init ((int) (sr * 0.05f));
    la.combPh = 0.0f;
    la.verb.prepare (sr, true);

    // ---- Deckard
    dk.rng.seed (0x77A15C3Bu);
    for (auto& v : dk.v)
    {
        v = Voice{};
        v.a.phase = dk.rng.uni();
        v.b.phase = dk.rng.uni();
    }
    for (int i = 0; i < MAX_POLY; ++i)
        dk.v[(size_t) i].pan = ((float) i / (float) (MAX_POLY - 1) - 0.5f) * 0.7f;
    dk.ens.prepare (sr);
    dk.verb.prepare (sr, false);
    dk.vibPh = 0.0f;
    dk.vibRamp = 0.0f;

    // ---- Replicant
    rp.rng.seed (0x2B7E1516u);
    rp.car.clear(); rp.mod.clear(); rp.heart.clear();
    rp.env.clear(); rp.heartEnv.clear();
    rp.bp.clear();
    rp.hold.setHz (12000.0f, sr);
    rp.verb.prepare (sr, false);
    rp.stutter.init ((int) (sr * 0.75f));
    rp.builtFor = -1;
    rp.phase = 0.0f;
    rp.step = 0;

    lim.prepare (sr);
    dcL.setHz (8.0f, sr);
    dcR.setHz (8.0f, sr);

    reset();
}

void Engine::reset()
{
    la.verb.clear(); la.comb.clear();
    for (auto& f : la.smog) f.clear();
    la.env.clear(); la.open = false;
    for (auto& g : la.grain) g.on = false;

    for (auto& v : dk.v) { v.used = false; v.note = -1; v.amp.clear(); v.fenv.clear(); v.filt.clear(); }
    dk.ens.clear(); dk.verb.clear();

    rp.env.clear(); rp.heartEnv.clear(); rp.bp.clear();
    rp.verb.clear(); rp.stutter.clear();
    rp.phase = 0.0f; rp.step = 0; rp.curAmp = 0.0f;

    lim.clear(); dcL.clear(); dcR.clear();
    heldCount = 0; lowestHeld = -1;
    for (auto& h : held) h = -1;
    for (auto& v : layerRms) v = 0.0f;
    outRms = 0.0f;
}

//==============================================================================
void Engine::noteOn (int note, float velocity)
{
    if (heldCount < (int) held.size())
    {
        held[(size_t) heldCount++] = note;
        lowestHeld = note;
        for (int i = 0; i < heldCount; ++i) lowestHeld = std::min (lowestHeld, held[(size_t) i]);
    }

    // Deckard: steal the oldest voice if every one is busy
    int slot = -1;
    for (int i = 0; i < MAX_POLY; ++i)
        if (! dk.v[(size_t) i].amp.active() && ! dk.v[(size_t) i].used) { slot = i; break; }
    if (slot < 0)
    {
        uint32_t oldest = 0xFFFFFFFFu;
        for (int i = 0; i < MAX_POLY; ++i)
            if (dk.v[(size_t) i].age < oldest) { oldest = dk.v[(size_t) i].age; slot = i; }
    }

    auto& v = dk.v[(size_t) slot];
    const float target = midiHz ((float) note);
    if (! v.used || p.dkGlide < 0.001f) v.hz = target;
    v.hzTarget = target;
    v.note = note;
    v.vel = std::max (0.05f, velocity);
    v.used = true;
    v.age = ++voiceAge;
    v.amp.gate (true);
    v.fenv.gate (true);
}

void Engine::noteOff (int note)
{
    int w = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
    heldCount = w;
    lowestHeld = -1;
    for (int i = 0; i < heldCount; ++i)
        lowestHeld = (lowestHeld < 0) ? held[(size_t) i] : std::min (lowestHeld, held[(size_t) i]);

    for (auto& v : dk.v)
        if (v.used && v.note == note) { v.used = false; v.amp.gate (false); v.fenv.gate (false); }
}

void Engine::allNotesOff()
{
    heldCount = 0;
    lowestHeld = -1;
    for (auto& v : dk.v) { v.used = false; v.amp.gate (false); v.fenv.gate (false); }
}

//==============================================================================
void Engine::buildPattern (int seed)
{
    Rng r;
    r.seed ((uint32_t) seed * 2654435761u ^ 0x5F356495u);

    /*  Natural minor with a flat second: the second is what makes it sound
        like something is in the room with you. */
    static const int SCALE[7] = { 0, 1, 3, 5, 7, 8, 10 };

    for (int i = 0; i < RP_STEPS; ++i)
    {
        rp.gateOf[(size_t) i]   = r.uni() < 0.58f ? 1 : 0;
        const int deg = (int) (r.uni() * 7.0f);
        const int oct = r.uni() < 0.22f ? 12 : (r.uni() < 0.12f ? -12 : 0);
        rp.pitchOf[(size_t) i]  = (int8_t) (SCALE[clampi (deg, 0, 6)] + oct);
        rp.accentOf[(size_t) i] = r.uni() < 0.28f ? 1 : 0;
    }
    // a machine that might not start is not menacing
    rp.gateOf[0] = 1;
    rp.accentOf[0] = 1;
    rp.builtFor = seed;
}

int Engine::patternStep (int i) const
{
    if (i < 0 || i >= RP_STEPS) return 0;
    if (! rp.gateOf[(size_t) i]) return 0;
    return rp.accentOf[(size_t) i] ? 2 : 1;
}

int Engine::patternPitch (int i) const
{
    return (i < 0 || i >= RP_STEPS) ? 0 : (int) rp.pitchOf[(size_t) i];
}

//==============================================================================
void Engine::renderLA (float* L, float* R, int n)
{
    const int   chord = clampi ((int) std::lround (p.laChord), 0, NUM_CHORDS - 1);
    const bool  keyed = p.laGate > 0.5f;

    float rootNote = 33.0f + p.laRoot;                     // A1, transposed
    if (keyed && lowestHeld >= 0) rootNote = (float) lowestHeld - 12.0f + p.laRoot;

    const bool want = keyed ? (heldCount > 0) : true;
    if (want != la.open) { la.open = want; la.env.gate (want); }

    const float sprawl   = p.laSprawl * 34.0f;             // cents, half-spread
    const float driftAmt = p.laDrift;
    const float subLvl   = p.laSub * 0.8f;
    const float kip      = p.laKipple;
    const float rainLvl  = p.laRain;

    // the smog wanders with the drift, so the city breathes
    la.driftPh[0] += la.driftInc[0] * (float) n;
    if (la.driftPh[0] >= 1.0f) la.driftPh[0] -= 1.0f;
    laDriftNow = 0.5f + 0.5f * std::sin (TWO_PI_F * la.driftPh[0]);
    const float smogHz = xmap (1.0f - p.laSmog, 260.0f, 8500.0f)
                       * (1.0f + (laDriftNow - 0.5f) * 0.55f * driftAmt)
                       * (wmActive ? std::min (4.0f, std::max (0.05f, wmFmul)) : 1.0f);   // world-mod bus
    la.smog[0].set (smogHz, 0.85f + p.laKipple * 0.9f, sr);
    la.smog[1].set (smogHz * 1.04f, 0.85f + p.laKipple * 0.9f, sr);

    // where the rain sits, and how it wanders
    for (int i = 0; i < 3; ++i)
    {
        la.rainPh[i] += (0.031f + 0.019f * (float) i) / (float) sr * (float) n;
        if (la.rainPh[i] >= 1.0f) la.rainPh[i] -= 1.0f;
        const float c = std::sin (TWO_PI_F * la.rainPh[i]);
        const float hz = (i == 0 ? 700.0f : (i == 1 ? 2300.0f : 5600.0f)) * (1.0f + c * 0.35f);
        la.rainBp[i].set (hz, 1.4f + (float) i * 0.8f, sr);
    }

    // per-oscillator pitch and weight
    float hz[LA_STACK], wgt[LA_STACK];
    float wsum = 0.0f;
    for (int i = 0; i < LA_STACK; ++i)
    {
        la.driftPh[i] += la.driftInc[i] * (float) n;
        if (la.driftPh[i] >= 1.0f) la.driftPh[i] -= 1.0f;
        const float dr = std::sin (TWO_PI_F * la.driftPh[i]) * driftAmt * 11.0f;
        const float spread = ((float) i / (float) (LA_STACK - 1) - 0.5f) * 2.0f;
        const int   semis = CHORD[chord][i];
        // world-mod detune fans across the stack (golden angle), mean zero
        const float wmC = wmActive
            ? wmDet * (std::fmod ((float) i * 0.6180339887f + 0.5f, 1.0f) * 2.0f - 1.0f) : 0.0f;
        hz[i]  = midiHz (rootNote + (float) semis) * cents (spread * sprawl + dr + wmC) * tuneMul;
        wgt[i] = 1.0f / (1.0f + 0.42f * std::abs ((float) semis) / 12.0f);
        wsum  += wgt[i];
        la.osc[i].setHz (hz[i], sr);
    }
    for (auto& w : wgt) w /= wsum;
    la.sub.setHz (midiHz (rootNote - 12.0f) * tuneMul, sr);

    const float grainRate = (0.6f + kip * 26.0f) / (float) sr;
    const float combDelay = (float) sr * 0.0043f;

    la.verb.setSize (0.92f, p.laDecay, lerp (7200.0f, 2300.0f, p.laSmog), sr);

    for (int i = 0; i < n; ++i)
    {
        const float e = la.env.tick();

        float drone = 0.0f;
        for (int k = 0; k < LA_STACK; ++k) drone += la.osc[k].saw() * wgt[k];
        drone += la.sub.sine() * subLvl;

        // kipple: the junk that accumulates whether or not anyone adds to it
        if (kip > 0.001f)
        {
            la.combPh += 0.07f / (float) sr;
            if (la.combPh >= 1.0f) la.combPh -= 1.0f;
            const float d = combDelay * (1.0f + 0.5f * std::sin (TWO_PI_F * la.combPh));
            const float c = la.comb.read (d);
            la.comb.push (drone + c * 0.55f * kip);
            drone = lerp (drone, drone + c * 0.7f, kip * 0.45f);
            drone = grit (drone * (1.0f + kip * 1.4f), kip) / (1.0f + kip * 0.55f);
        }

        // grains of dust, each its own little decaying resonance
        la.grainClock += grainRate;
        while (la.grainClock >= 1.0f)
        {
            la.grainClock -= 1.0f;
            for (auto& g : la.grain)
                if (! g.on)
                {
                    g.on   = true;
                    g.amp  = 0.35f + la.rng.uni() * 0.65f;
                    g.dec  = 1.0f - std::exp (-1.0f / ((0.02f + la.rng.uni() * 0.34f) * (float) sr));
                    g.pan  = la.rng.bi() * 0.9f;
                    g.o.clear();
                    g.o.setHz (xmap (la.rng.uni(), 180.0f, 6500.0f), sr);
                    g.bp.set (xmap (la.rng.uni(), 300.0f, 7000.0f), 6.0f, sr);
                    break;
                }
        }
        float grainL = 0.0f, grainR = 0.0f;
        for (auto& g : la.grain)
            if (g.on)
            {
                const float s = g.bp.tick (g.o.tri() * 0.5f + la.rng.bi() * 0.5f).bp * g.amp;
                g.amp -= g.amp * g.dec;
                if (g.amp < 0.0008f) g.on = false;
                grainL += s * (0.5f - g.pan * 0.5f);
                grainR += s * (0.5f + g.pan * 0.5f);
            }

        // rain
        float rainL = 0.0f, rainR = 0.0f;
        if (rainLvl > 0.001f)
        {
            const float nz = la.rainLp.lp (la.rng.bi());
            for (int b = 0; b < 3; ++b)
            {
                const float s = la.rainBp[b].tick (nz).bp;
                const float pan = (float) (b - 1) * 0.6f;
                rainL += s * (0.5f - pan * 0.5f);
                rainR += s * (0.5f + pan * 0.5f);
            }
            rainL *= rainLvl * 0.5f;
            rainR *= rainLvl * 0.5f;
        }

        const float l = la.smog[0].tick (drone).lp * 0.9f + grainL * kip * 0.6f + rainL;
        const float r = la.smog[1].tick (drone).lp * 0.9f + grainR * kip * 0.6f + rainR;

        /*  The sub goes to the speakers but not into the reverb. A 40 Hz
            drone smeared over eight seconds is mud, not depth. */
        float wl = 0.0f, wr = 0.0f;
        la.verb.tick (la.verbHp.hp ((l + r) * 0.5f), p.laNeon * 0.55f, wl, wr);

        float wmG = 1.0f;
        if (wmActive && wmTremD > 0.0f && wmTremR > 0.0f)
            wmG = 1.0f - wmTremD * (0.5f - 0.5f * std::sin (TWO_PI_F * wmTremR * (float) (wmT + (double) i / sr)));
        L[i] = (l * 0.55f + wl * 0.95f) * e * wmG;
        R[i] = (r * 0.55f + wr * 0.95f) * e * wmG;
    }
}

//==============================================================================
void Engine::renderDK (float* L, float* R, int n)
{
    const int wave = clampi ((int) std::lround (p.dkWave), 0, NUM_WAVES - 1);
    const int oct  = clampi ((int) std::lround (p.dkOct), -2, 2);

    const float atk = xmap (p.dkAtk, 0.002f, 6.0f);
    const float dec = xmap (p.dkDec, 0.02f, 8.0f);
    const float rel = xmap (p.dkRel, 0.03f, 12.0f);
    const float sus = p.dkSus;
    for (auto& v : dk.v)
    {
        v.amp.set (atk, dec, sus, rel, sr);
        v.fenv.set (atk * 0.6f, dec * 1.3f, sus * 0.55f, rel * 0.9f, sr);
    }

    const float baseCut  = xmap (p.dkBright, 70.0f, 13000.0f);
    const float envDepth = p.dkFenv * 8500.0f;
    const float res      = p.dkRes * 0.94f;
    const float detune   = p.dkDetune * 16.0f;                       // cents
    const float ringAmt  = p.dkRing;
    const float glide    = p.dkGlide < 0.001f ? 1.0f
                         : 1.0f - std::exp (-1.0f / (xmap (p.dkGlide, 0.005f, 1.6f) * (float) sr));
    const float octMul   = std::pow (2.0f, (float) oct);

    /*  CS-80 vibrato arrives late. It is not an effect on the note, it is
        something the player does to a note that is already sounding. */
    const float vibTarget = heldCount > 0 ? 1.0f : 0.0f;
    const float vibK = 1.0f - std::exp (-1.0f / (0.9f * (float) sr));

    int active = 0;
    for (auto& v : dk.v) if (v.amp.active()) ++active;
    dkVoicesNow = (float) active;

    dk.verb.setSize (0.62f, 0.42f + p.dkSpace * 0.42f, 5400.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        dk.vibRamp += (vibTarget - dk.vibRamp) * vibK;
        dk.vibPh += 5.35f / (float) sr;
        if (dk.vibPh >= 1.0f) dk.vibPh -= 1.0f;
        const float vib = std::sin (TWO_PI_F * dk.vibPh) * p.dkVib * dk.vibRamp * 9.0f;   // cents

        // one slow PWM shared by the whole keyboard, as the hardware had
        const float pw = 0.5f + 0.32f * std::sin (TWO_PI_F * (float) (dk.counter & 0xFFFFF) / 1048576.0f * 3.0f);
        ++dk.counter;

        float sl = 0.0f, sr_ = 0.0f;
        for (auto& v : dk.v)
        {
            if (! v.amp.active()) continue;

            const int vi = (int) (&v - dk.v.data());
            float wmMul = 1.0f, wmG = 1.0f, wmPanAdd = 0.0f;
            if (wmActive)
            {
                // sag keys to the smoothed GATE: in tune while held, sags as
                // the note dies (the Photo-Synth lesson — never the amp env)
                v.wmGate += (1.0f - std::exp (-1.0f / (0.05f * (float) sr))) * ((v.used ? 1.0f : 0.0f) - v.wmGate);
                const float fan = std::fmod ((float) vi * 0.6180339887f + 0.5f, 1.0f) * 2.0f - 1.0f;
                wmMul = cents (wmDet * fan - wmSag * 100.0f * (1.0f - v.wmGate));
                if (wmTremD > 0.0f && wmTremR > 0.0f)
                {
                    const float u = std::fmod ((float) vi * 0.6180339887f + 0.71f, 1.0f);
                    wmG = 1.0f - wmTremD * (0.5f - 0.5f * std::sin (TWO_PI_F * wmTremR * (0.75f + 0.5f * u) * (float) (wmT + (double) i / sr) + (float) vi * 2.39996f));
                }
                wmPanAdd = wmPan * (std::fmod ((float) vi * 0.6180339887f + 0.21f, 1.0f) * 2.0f - 1.0f);
            }
            v.hz += (v.hzTarget - v.hz) * glide;
            const float f = v.hz * octMul * tuneMul * cents (vib) * wmMul;
            v.a.setHz (f * cents (-detune), sr);
            v.b.setHz (f * cents (detune), sr);
            v.sub.setHz (f * 0.5f, sr);

            float oa = 0.0f, ob = 0.0f;
            switch (wave)
            {
                case 0: oa = v.a.saw();      ob = v.b.saw();       break;
                case 1: oa = v.a.pulse (pw); ob = v.b.pulse (1.0f - pw); break;
                case 2: oa = v.a.saw();      ob = v.b.pulse (pw);  break;
                default:oa = v.a.saw();      ob = v.b.sine();      break;
            }

            float osc = (wave == 3) ? lerp (oa, oa * ob, 0.8f)
                                    : (oa + ob) * 0.5f;
            if (ringAmt > 0.001f) osc = lerp (osc, oa * ob, ringAmt);
            osc += v.sub.sine() * 0.34f;

            const float fe = v.fenv.tick();
            v.filt.set (std::min ((baseCut + envDepth * fe * v.vel + v.hz * 0.35f)
                                  * (wmActive ? std::min (4.0f, std::max (0.05f, wmFmul)) : 1.0f), (float) sr * 0.45f), res, sr);
            /*  2.4 because a single note was arriving 20 dB under the drone;
                a polysynth is allowed to get louder with more keys held, but
                it has to be audible with one. */
            const float y = v.filt.tick (osc * 0.42f) * v.amp.tick() * v.vel * 2.4f * wmG;

            const float vp = std::min (1.0f, std::max (-1.0f, v.pan + wmPanAdd));
            sl += y * (0.5f - vp * 0.5f);
            sr_ += y * (0.5f + vp * 0.5f);
        }

        float el = sl, er = sr_;
        dk.ens.tick (sl, sr_, p.dkEns, el, er);

        float wl = 0.0f, wr = 0.0f;
        dk.verb.tick ((el + er) * 0.5f, 0.0f, wl, wr);

        L[i] = el * (1.0f - p.dkSpace * 0.35f) + wl * p.dkSpace * 1.15f;
        R[i] = er * (1.0f - p.dkSpace * 0.35f) + wr * p.dkSpace * 1.15f;
    }
}

//==============================================================================
void Engine::renderRP (float* L, float* R, int n)
{
    const int seed  = clampi ((int) std::lround (p.rpNexus), 0, 63);
    if (seed != rp.builtFor) buildPattern (seed);

    const int   steps = clampi ((int) std::lround (p.rpSteps), 4, RP_STEPS);
    const int   rate  = clampi ((int) std::lround (p.rpRate), 0, NUM_RATES - 1);
    const int   oct   = clampi ((int) std::lround (p.rpOct), -2, 2);
    const bool  keyed = p.rpGate > 0.5f;
    const float bpm   = p.bpm > 1.0 ? (float) p.bpm : 120.0f;

    const float samplesPerStep = std::max (32.0f, (60.0f / bpm) * rateBeats (rate) * (float) sr);
    const float stepInc = 1.0f / samplesPerStep;

    const float metal   = p.rpMetal;
    const int   ratioIx = clampi ((int) (metal * 7.999f), 0, 7);
    const float fmIndex = metal * metal * 9.0f;
    const float decay   = xmap (p.rpDecay, 0.015f, 1.6f);
    const float glitch  = p.rpGlitch;
    const float menace  = p.rpMenace;
    const float spread  = p.rpSpread;

    rp.env.set (0.0008f, decay, 0.0f, decay * 0.7f, sr);
    rp.heartEnv.set (0.004f, 0.34f, 0.0f, 0.3f, sr);
    rp.verb.setSize (0.42f, p.rpSpace, 4200.0f, sr);

    const bool running = keyed ? (heldCount > 0) : true;
    const int  transpose = (keyed && lowestHeld >= 0) ? lowestHeld - 48 : 0;

    // crush: hold the sample for a while, and quantise what is held
    const float holdRate = lerp (1.0f, 0.035f, glitch);
    const float levels   = std::pow (2.0f, lerp (16.0f, 3.0f, glitch));

    if (! running) { rp.env.gate (false); rp.curAmp = 0.0f; }

    for (int i = 0; i < n; ++i)
    {
        if (running)
        {
            rp.phase += stepInc;
            while (rp.phase >= 1.0f)
            {
                rp.phase -= 1.0f;
                rp.step = (rp.step + 1) % steps;
                rpStepNow = rp.step;

                if (rp.gateOf[(size_t) rp.step])
                {
                    const float note = 48.0f + (float) rp.pitchOf[(size_t) rp.step]
                                     + (float) transpose + (float) (oct * 12);
                    rp.curHz = midiHz (note) * tuneMul;
                    rp.curAmp = rp.accentOf[(size_t) rp.step] ? 1.0f : 0.62f;
                    rp.pan = ((rp.step & 1) ? 1.0f : -1.0f) * spread * 0.85f;
                    /*  A one-shot: the sustain level is zero, so Att runs into
                        Dec and Dec decays to nothing on its own. No note-off
                        to schedule, and nothing to leave hanging. */
                    rp.env.st = ADSR::Att;
                    rp.env.v = 0.0f;
                    rp.bp.set (std::min (rp.curHz * (2.0f + metal * 5.0f)
                                         * (wmActive ? std::min (4.0f, std::max (0.05f, wmFmul)) : 1.0f), (float) sr * 0.44f),
                               2.0f + menace * 7.0f, sr);
                    rpHit = 1.0f;
                    rp.stutterOn = glitch > 0.45f && (rp.rng.uni() < (glitch - 0.45f) * 1.6f);
                }
                if (rp.step == 0) { rp.heartEnv.st = ADSR::Att; rp.heartEnv.v = 0.0f; }
            }
        }

        const float e = rp.env.tick();

        /*  Phase modulation, not frequency modulation: it is unconditionally
            stable however deep the index goes, and it does not change
            character with the sample rate. */
        rp.mod.setHz (rp.curHz * FM_RATIO[ratioIx], sr);
        rp.car.setHz (rp.curHz, sr);
        const float mo = rp.mod.sine();
        rp.car.advance();
        float s = std::sin (TWO_PI_F * (rp.car.phase + mo * fmIndex * e * 0.16f)) * e * rp.curAmp;
        s = rp.bp.tick (s).bp * (0.6f + metal * 0.9f);

        // menace: a tritone shadow and a little too much drive
        if (menace > 0.001f)
            s = std::tanh (s * (1.0f + menace * 4.5f)) / (1.0f + menace * 1.4f);

        // glitch: sample-and-hold then quantise
        if (glitch > 0.001f)
        {
            rp.crushPh += holdRate;
            if (rp.crushPh >= 1.0f) { rp.crushPh -= 1.0f; rp.holdVal = s; }
            const float q = std::round (rp.holdVal * levels) / levels;
            s = lerp (s, q, glitch);
        }

        // stutter: repeat the last fraction of a step, occasionally
        rp.stutter.push (s);
        if (rp.stutterOn && rp.phase > 0.5f)
            s = lerp (s, rp.stutter.read (samplesPerStep * 0.25f), 0.8f);

        // the heartbeat under it all
        const float he = rp.heartEnv.tick();
        rp.heart.setHz (46.0f * tuneMul, sr);
        const float heart = rp.heart.sine() * he * he * menace * 0.55f;

        float l = s * (0.5f - rp.pan * 0.5f) + heart;
        float r = s * (0.5f + rp.pan * 0.5f) + heart;

        float wl = 0.0f, wr = 0.0f;
        rp.verb.tick ((l + r) * 0.5f, 0.0f, wl, wr);

        float wmG = 1.0f;
        if (wmActive && wmTremD > 0.0f && wmTremR > 0.0f)
            wmG = 1.0f - wmTremD * (0.5f - 0.5f * std::sin (TWO_PI_F * wmTremR * 1.13f * (float) (wmT + (double) i / sr) + 2.1f));
        L[i] = (l * 0.85f + wl * p.rpSpace * 0.9f) * wmG;
        R[i] = (r * 0.85f + wr * p.rpSpace * 0.9f) * wmG;

        rpHit -= rpHit * 0.0004f;
    }
}

//==============================================================================
void Engine::process (float* left, float* right, int n)
{
    if (n <= 0) return;
    if ((int) tmpL.size() < n) { tmpL.assign ((size_t) n, 0.0f); tmpR.assign ((size_t) n, 0.0f); }

    tuneMul = cents ((p.tune - 0.5f) * 200.0f);

    // Brokild World FX bus: copy once per block; neutral = inert
    wmDet   = wmIn[0].load (std::memory_order_relaxed);
    wmPan   = wmIn[1].load (std::memory_order_relaxed);
    wmTremD = wmIn[2].load (std::memory_order_relaxed);
    wmTremR = wmIn[3].load (std::memory_order_relaxed);
    wmSag   = wmIn[4].load (std::memory_order_relaxed);
    wmFmul  = wmIn[5].load (std::memory_order_relaxed);
    wmActive = wmDet != 0.0f || wmPan != 0.0f || wmTremD != 0.0f
            || wmSag != 0.0f || wmFmul != 1.0f;
    if (wmActive) wmT += (double) n / sr; else wmT = 0.0;

    std::fill (left,  left  + n, 0.0f);
    std::fill (right, right + n, 0.0f);

    auto layer = [&] (int idx, bool on, float lvl, void (Engine::*fn) (float*, float*, int))
    {
        if (! on) { layerRms[idx] += (0.0f - layerRms[idx]) * 0.25f; return; }
        (this->*fn) (tmpL.data(), tmpR.data(), n);
        const float g = lvl * lvl * 2.2f;
        double acc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float l = tmpL[(size_t) i] * g, r = tmpR[(size_t) i] * g;
            left[i]  += l;
            right[i] += r;
            acc += (double) l * l + (double) r * r;
        }
        const float rms = (float) std::sqrt (acc / (2.0 * n));
        layerRms[idx] += (rms - layerRms[idx]) * 0.3f;
    };

    layer (0, p.laOn > 0.5f, p.laLvl, &Engine::renderLA);
    layer (1, p.dkOn > 0.5f, p.dkLvl, &Engine::renderDK);
    layer (2, p.rpOn > 0.5f, p.rpLvl, &Engine::renderRP);

    const float mg = p.master * p.master * 2.0f;
    double acc = 0.0;
    for (int i = 0; i < n; ++i)
    {
        float l = left[i] * mg, r = right[i] * mg;
        l -= dcL.lp (l);
        r -= dcR.lp (r);
        /*  The switch turns off the smooth, musical gain reduction. It does
            not turn off the ceiling — an instrument has no business emitting
            anything above full scale, and the manual says so. */
        if (p.limiter > 0.5f) lim.tick (l, r, 0.985f);
        l = ceilSoft (l, 0.985f);
        r = ceilSoft (r, 0.985f);
        left[i] = l;
        right[i] = r;
        acc += (double) l * l + (double) r * r;
    }
    const float rms = (float) std::sqrt (acc / (2.0 * n));
    outRms += (rms - outRms) * 0.3f;
    limitGr = lim.gr;
}

} // namespace br
