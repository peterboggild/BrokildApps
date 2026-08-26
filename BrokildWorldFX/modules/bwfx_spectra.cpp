// The SPECTRA characters — ports of Photo-Synth 2's SPEC_MODULES `mods(dt,P)`
// behaviour onto the fixed world-modulation bus (design decision 7). Only the
// LIVE MODULATOR half of each character ports; the host-control macro half
// stays a Photo-Synth-native specialty, so a ported character sounds like the
// HOST synth possessed, not like Photo-Synth pasted in.
//
// Porting note that matters: PS2 ticked these at requestAnimationFrame rate
// (~60 Hz); the rack ticks at sub-block rate (~1.5 kHz at 48 k). The random
// walks are therefore converted from per-tick coefficients to time constants
// (decay exp(-dt/tau)) with the noise scaled by sqrt(dt*60), so the walk's
// character — not just its bounds — matches the original. And they use an
// owned LCG, never rand(): renders must be bit-repeatable.

#include "../src/bwfx.h"
#include "../src/bwfx_dsp.h"
#include "../src/bwfx_json.h"

#include <cstdio>

namespace bwfx
{

namespace
{
    struct Walk
    {
        float v = 0;
        uint32_t rng;
        explicit Walk (uint32_t seed) : rng (seed) {}
        float step (double dt, double tauSec, float noiseAmp)
        {
            rng = 1664525u * rng + 1013904223u;
            const float r = (float) (rng / 4294967296.0) - 0.5f;
            v = (float) (v * std::exp (-dt / tauSec))
              + r * noiseAmp * (float) std::sqrt (dt * 60.0);
            return v = clampf (v, -1.0f, 1.0f);
        }
        void reset (uint32_t seed) { v = 0; rng = seed; }
    };
}

//==============================================================================
// TAPE SEANCE — the wow of a dying machine: slow wow + fast flutter + a
// drifting random walk pulling the tuning, a sag the HOST keys to its gate,
// and a dulled top end. (PS2's dust/noise half was FX-macro side and cannot
// port; DULL carries the darkening.)
class TapeSeance : public Character
{
public:
    const Descriptor& desc() const override;

    void reset() override { ph = 0; ph2 = 0; jd.reset (0x7A9Eu); }

    void tick (double dt, WorldMod& add) override
    {
        const float wob  = clampf (getParam (0) / 100.0f, 0.0f, 1.0f);
        const float sag  = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float dull = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);

        ph  += kTwoPi * 0.5 * dt;  if (ph  > kTwoPi) ph  -= kTwoPi;   // wow
        ph2 += kTwoPi * 6.3 * dt;  if (ph2 > kTwoPi) ph2 -= kTwoPi;   // flutter
        // PS2: jd*0.985 per 60 Hz tick -> tau 1.10 s, noise 0.06*wob
        const float j = jd.step (dt, 1.10, 0.06f * wob);
        const float wow = (float) std::sin (ph) + 0.25f * (float) std::sin (ph2) + 0.6f * j;

        add.detuneCents += wow * 9.0f * wob;
        add.pitchSag    += 0.9f * sag;              // hosts key this to the gate
        add.filterMul   *= 1.0f - 0.35f * dull;
    }

private:
    double ph = 0, ph2 = 0;
    Walk jd { 0x7A9Eu };
};

//==============================================================================
// INSECT SWARM — sixteen wings that never quite agree: a fast skittering
// detune walk, per-voice tremolo at golden-angle phases (the host fans the
// depth+rate across its own voices), and the stereo field scattered wide.
class InsectSwarm : public Character
{
public:
    const Descriptor& desc() const override;

    void reset() override { jd.reset (0x1C5Ec7u); }

    void tick (double dt, WorldMod& add) override
    {
        const float swarm = clampf (getParam (0) / 100.0f, 0.0f, 1.0f);
        const float fl    = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float sk    = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);

        // PS2: jd*0.9 per 60 Hz tick -> tau 0.158 s, noise 1.4*sk
        const float j = jd.step (dt, 0.158, 1.4f * sk);

        add.detuneCents += j * 7.0f * sk * (1.0f + swarm);
        add.panSpread   += 0.25f + 0.55f * swarm;
        // PS2's per-voice trem: rate 3.1+0.83i, depth 0.5*fl — carried as
        // depth+rate; the host spreads rates and golden-angle phases itself
        const float d = 0.5f * fl;
        if (d > add.tremDepth) add.tremRate = 3.1f + 2.5f * fl;
        add.tremDepth = 1.0f - (1.0f - add.tremDepth) * (1.0f - d);
    }

private:
    Walk jd { 0x1C5Ec7u };
};

//==============================================================================
namespace
{
    const ParamDesc TAPE_PARAMS[] = {
        { "wobble", "WOBBLE", 45, 0, 100, 0, "%", nullptr },
        { "sag",    "SAG",    35, 0, 100, 0, "%", nullptr },
        { "dull",   "DULL",   40, 0, 100, 0, "%", nullptr },
    };
    const ParamDesc INSECT_PARAMS[] = {
        { "swarm",   "SWARM",   55, 0, 100, 0, "%", nullptr },
        { "flutter", "FLUTTER", 60, 0, 100, 0, "%", nullptr },
        { "skitter", "SKITTER", 40, 0, 100, 0, "%", nullptr },
    };

    const Descriptor CHAR_DESCS[] = {
        { "tape",   "TAPE SEANCE",  "the wow of a dying machine",   1, TAPE_PARAMS,   3 },
        { "insect", "INSECT SWARM", "sixteen wings, none agreeing", 1, INSECT_PARAMS, 3 },
    };
    constexpr int kNumChars = (int) (sizeof (CHAR_DESCS) / sizeof (CHAR_DESCS[0]));
}

const Descriptor& TapeSeance::desc() const  { return CHAR_DESCS[0]; }
const Descriptor& InsectSwarm::desc() const { return CHAR_DESCS[1]; }

int numCharacters() { return kNumChars; }

const Descriptor& characterDescriptor (int c)
{
    return CHAR_DESCS[c < 0 || c >= kNumChars ? 0 : c];
}

Character* createCharacter (int c)
{
    Character* ch = nullptr;
    switch (c)
    {
        case 0: ch = new TapeSeance(); break;
        case 1: ch = new InsectSwarm(); break;
        default: return nullptr;
    }
    const Descriptor& d = ch->desc();
    for (int p = 0; p < d.numParams; ++p)
        ch->setParam (p, d.params[p].def);
    ch->reset();
    return ch;
}

std::string characterJson()
{
    std::string s = "[";
    for (int c = 0; c < kNumChars; ++c)
    {
        const Descriptor& d = CHAR_DESCS[c];
        if (c > 0) s += ",";
        s += "{\"id\":\"" + json::escape (d.id) + "\",\"name\":\"" + json::escape (d.name)
           + "\",\"sub\":\"" + json::escape (d.sub) + "\",\"ver\":" + std::to_string (d.version)
           + ",\"params\":[";
        for (int p = 0; p < d.numParams; ++p)
        {
            const ParamDesc& pd = d.params[p];
            if (p > 0) s += ",";
            char buf[200];
            std::snprintf (buf, sizeof (buf),
                "{\"id\":\"%s\",\"name\":\"%s\",\"def\":%g,\"lo\":%g,\"hi\":%g,\"step\":%g,\"unit\":\"%s\"}",
                pd.id, pd.name, (double) pd.def, (double) pd.lo, (double) pd.hi, (double) pd.step, pd.unit);
            s += buf;
        }
        s += "]}";
    }
    s += "]";
    return s;
}

} // namespace bwfx
