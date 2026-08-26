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
#include <cstring>
#include <memory>

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

    // A character's private pedal: one of the rack's own module classes owned
    // INSIDE the unit (Pink's wash, Black's grind chain, Glass's hall) — the
    // Phase B rule that a character owning FX carries that DSP privately,
    // without duplicating a line of it.
    struct PrivateFx
    {
        std::unique_ptr<Module> m;
        void create (const char* id)
        {
            for (int t = 0; t < numModuleTypes(); ++t)
                if (std::strcmp (moduleDescriptor (t).id, id) == 0)
                {
                    m.reset (createModule (t));
                    return;
                }
        }
        void set (const char* pid, float v)
        {
            if (! m) return;
            const Descriptor& d = m->desc();
            for (int p = 0; p < d.numParams; ++p)
                if (std::strcmp (d.params[p].id, pid) == 0)
                {
                    m->setParam (p, v);
                    return;
                }
        }
        void prepare (double fs, int mb) { if (m) m->prepare (fs, mb); }
        void reset()                     { if (m) m->reset(); }
        void service()                   { if (m) m->service(); }
        void run (float* L, float* R, int n, float duck)
        {
            if (! m) return;
            m->inputDuck (duck);
            m->process (L, R, n);
        }
    };
}

//==============================================================================
// DARK DRONE — PS2's drone engine possessed: the microtonal cluster instead
// of tidy unison, the pitch sag as notes die (hosts key it to the gate), and
// the minutes-scale random drift on cut-off and detune. Faithful numbers:
// tau = 300*0.1^(v/100) s floored at 10, OU walk clamped ±1, cutMul
// 2^(c*drift*1.2), det d*drift*10 cents. The sub-oscillator does NOT port —
// it needs the note, and an audio-domain octaver on polyphonic material is
// mush, not a sub. That half stays Photo-Synth-native (documented).
class DarkDrone : public Character
{
public:
    const Descriptor& desc() const override;

    void reset() override { wc.reset (0xD44Cu); wd.reset (0x0DD1u); }

    void tick (double dt, WorldMod& add) override
    {
        const float cluster = clampf (getParam (0), 0.0f, 60.0f);           // cents
        const float sag     = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float drift   = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);
        const float dtime   = clampf (getParam (3) / 100.0f, 0.0f, 1.0f);

        // PS2 noise term: uniform ±1.3*sqrt(dt/tau); Walk's is r∈±0.5 times
        // amp*sqrt(dt*60), so amp = 2*1.3/sqrt(60*tau).
        const double tau = std::max (10.0, 300.0 * std::pow (0.1, (double) dtime));
        const float amp = (float) (2.6 / std::sqrt (60.0 * tau));
        const float c = wc.step (dt, tau, amp);
        const float d = wd.step (dt, tau, amp);

        add.detuneCents += cluster + d * drift * 10.0f;
        add.pitchSag    += sag;                     // hosts key this to the gate
        add.filterMul   *= (float) std::pow (2.0, (double) (c * drift * 1.2f));
    }

private:
    Walk wc { 0xD44Cu }, wd { 0x0DD1u };
};

//==============================================================================
// PSYCHEDELIC PINK — the swirl: three incommensurate LFOs (rate, x0.618,
// x0.29) breathing the stereo width, smearing the tuning and blooming the
// filter, over a private phaser -> chorus -> reverse-reverb wash. PS2 swung
// the whole image L/R (panAdd ±0.7*swirl); the bus carries a WIDTH, so the
// swirl breathes the spread 0..0.7*swirl instead — same motion, per voice.
class PsychedelicPink : public Character
{
public:
    PsychedelicPink()
    {
        fxPhaser.create ("phaser");
        fxChorus.create ("chorus");
        fxVerb.create ("reverb");
    }

    const Descriptor& desc() const override;
    bool hasAudio() const override { return true; }

    void reset() override { ph = ph2 = ph3 = 0; }

    void prepare (double fs, int mb) override
    {
        fxPhaser.prepare (fs, mb);
        fxChorus.prepare (fs, mb);
        fxVerb.prepare (fs, mb);
        push();
    }
    void resetAudio() override { fxPhaser.reset(); fxChorus.reset(); fxVerb.reset(); }
    void service() override    { push(); fxPhaser.service(); fxChorus.service(); fxVerb.service(); }

    void processAudio (float* L, float* R, int n, float duck) override
    {
        fxPhaser.run (L, R, n, duck);
        fxChorus.run (L, R, n, duck);
        fxVerb.run   (L, R, n, duck);
    }

    void tick (double dt, WorldMod& add) override
    {
        const float sw = clampf (getParam (0) / 100.0f, 0.0f, 1.0f);
        const float sm = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float bl = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);

        const double rate = 0.02 + 0.24 * sw;
        ph  += kTwoPi * rate * dt;          if (ph  > kTwoPi) ph  -= kTwoPi;
        ph2 += kTwoPi * rate * 0.618 * dt;  if (ph2 > kTwoPi) ph2 -= kTwoPi;
        ph3 += kTwoPi * rate * 0.29 * dt;   if (ph3 > kTwoPi) ph3 -= kTwoPi;

        add.panSpread   += 0.35f * sw * (1.0f + (float) std::sin (ph));
        add.detuneCents += (float) std::sin (ph2) * 9.0f * sm;
        add.filterMul   *= (float) std::pow (2.0, std::sin (ph3) * 0.55 * (double) bl);
    }

private:
    void push()
    {
        const float sm = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float bl = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);
        fxChorus.set ("mix", 40.0f + 35.0f * sm);
        fxChorus.set ("rate", 22.0f);
        fxChorus.set ("depth", 45.0f + 45.0f * sm);
        fxPhaser.set ("mix", 30.0f + 30.0f * sm);
        fxPhaser.set ("rate", 11.0f);
        fxPhaser.set ("depth", 55.0f + 35.0f * sm);
        fxVerb.set ("character", 4.0f);                  // REVERSE
        fxVerb.set ("mix", 22.0f + 45.0f * bl);
        fxVerb.set ("length", 240.0f + 330.0f * bl);
    }

    double ph = 0, ph2 = 0, ph3 = 0;
    PrivateFx fxPhaser, fxChorus, fxVerb;
};

//==============================================================================
// INDUSTRIAL BLACK — grind, chop, clang: a private crusher -> valve drive ->
// stutter -> short saturating tape echo, the PS2 gain-discipline numbers kept
// (satGain 2+6g on the 10^(v/32) law = 0.625*v dB into TUBE's dB knob). The
// square-wave/envelope macro half stays PS2-native, and the comb filter is
// deliberately NOT carried — it was PS2's self-oscillation trouble spot.
class IndustrialBlack : public Character
{
public:
    IndustrialBlack()
    {
        fxLofi.create ("lofi");
        fxTube.create ("saturation");
        fxGate.create ("stutter");
        fxEcho.create ("delay");
    }

    const Descriptor& desc() const override;
    bool hasAudio() const override { return true; }

    void reset() override {}

    void prepare (double fs, int mb) override
    {
        fxLofi.prepare (fs, mb);
        fxTube.prepare (fs, mb);
        fxGate.prepare (fs, mb);
        fxEcho.prepare (fs, mb);
        push();
    }
    void resetAudio() override { fxLofi.reset(); fxTube.reset(); fxGate.reset(); fxEcho.reset(); }
    void service() override    { push(); fxLofi.service(); fxTube.service(); fxGate.service(); fxEcho.service(); }

    void processAudio (float* L, float* R, int n, float duck) override
    {
        fxLofi.run (L, R, n, duck);
        fxTube.run (L, R, n, duck);
        fxGate.run (L, R, n, duck);
        fxEcho.run (L, R, n, duck);
    }

    void tick (double, WorldMod&) override {}   // pure FX character, no bus

private:
    void push()
    {
        const float g  = clampf (getParam (0) / 100.0f, 0.0f, 1.0f);
        const float ch = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float cl = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);
        fxLofi.set ("crush", 22.0f + 55.0f * g);
        fxLofi.set ("noise", 0.0f);              // silence in, silence out
        fxLofi.set ("dirt", 20.0f + 45.0f * g);
        fxTube.set ("drive", 0.625f * (2.0f + 6.0f * g));
        fxTube.set ("tone", 34.0f);
        fxGate.set ("amount", ch > 0.02f ? 35.0f + 60.0f * ch : 0.0f);
        fxGate.set ("rate", 60.0f + 80.0f * ch);
        fxGate.set ("sync", 0.0f);
        fxEcho.set ("mix", cl > 0.02f ? 16.0f + 24.0f * cl : 0.0f);
        fxEcho.set ("time", 150.0f - 70.0f * cl);   // never under 80 ms: decays, no howl
        fxEcho.set ("feedback", 26.0f + 22.0f * cl);
        fxEcho.set ("character", 1.0f);             // TAPE
        fxEcho.set ("offset", 0.0f);
        fxEcho.set ("sync", 0.0f);
    }

    PrivateFx fxLofi, fxTube, fxGate, fxEcho;
};

//==============================================================================
// GLASS CATHEDRAL — the room the note prays in: a private hall and a
// barely-there breath on the tuning (±2.2 cents at 0.05 Hz — window light,
// not vibrato). PS2's slow-attack/glide macro half needs the host's envelope
// and stays native; HALO and SHINE carry the cathedral.
class GlassCathedral : public Character
{
public:
    GlassCathedral() { fxVerb.create ("reverb"); }

    const Descriptor& desc() const override;
    bool hasAudio() const override { return true; }

    void reset() override { ph = 0; }

    void prepare (double fs, int mb) override { fxVerb.prepare (fs, mb); push(); }
    void resetAudio() override                { fxVerb.reset(); }
    void service() override                   { push(); fxVerb.service(); }

    void processAudio (float* L, float* R, int n, float duck) override
    {
        fxVerb.run (L, R, n, duck);
    }

    void tick (double dt, WorldMod& add) override
    {
        const float sh = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        ph += kTwoPi * 0.05 * dt;
        if (ph > kTwoPi) ph -= kTwoPi;
        add.detuneCents += (float) std::sin (ph) * 2.2f * sh;
    }

private:
    void push()
    {
        const float ha = clampf (getParam (0) / 100.0f, 0.0f, 1.0f);
        fxVerb.set ("character", 1.0f);              // HALL
        fxVerb.set ("mix", 16.0f + 30.0f * ha);
        fxVerb.set ("length", 260.0f + 300.0f * ha);
    }

    double ph = 0;
    PrivateFx fxVerb;
};

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
    const ParamDesc DARK_PARAMS[] = {
        { "cluster", "CLUSTER",    24, 0, 60,  0, "\\u00a2", nullptr },
        { "sag",     "SAG",        25, 0, 100, 0, "%", nullptr },
        { "drift",   "DRIFT",      40, 0, 100, 0, "%", nullptr },
        { "dtime",   "DRIFT TIME", 40, 0, 100, 0, "%", nullptr },
    };
    const ParamDesc PINK_PARAMS[] = {
        { "swirl", "SWIRL", 50, 0, 100, 0, "%", nullptr },
        { "smear", "SMEAR", 55, 0, 100, 0, "%", nullptr },
        { "bloom", "BLOOM", 45, 0, 100, 0, "%", nullptr },
    };
    const ParamDesc BLACK_PARAMS[] = {
        { "grind", "GRIND", 60, 0, 100, 0, "%", nullptr },
        { "chop",  "CHOP",  55, 0, 100, 0, "%", nullptr },
        { "clang", "CLANG", 40, 0, 100, 0, "%", nullptr },
    };
    const ParamDesc GLASS_PARAMS[] = {
        { "halo",  "HALO",  55, 0, 100, 0, "%", nullptr },
        { "shine", "SHINE", 60, 0, 100, 0, "%", nullptr },
    };
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
        { "darkdrone", "DARK DRONE",       "a cluster gone dark, sagging as it dies", 1, DARK_PARAMS,   4 },
        { "pink",      "PSYCHEDELIC PINK", "a swirl of smeared bloom",                1, PINK_PARAMS,   3 },
        { "black",     "INDUSTRIAL BLACK", "grind, chop, clang",                      1, BLACK_PARAMS,  3 },
        { "glass",     "GLASS CATHEDRAL",  "the room the note prays in",              1, GLASS_PARAMS,  2 },
        { "tape",      "TAPE SEANCE",      "the wow of a dying machine",              1, TAPE_PARAMS,   3 },
        { "insect",    "INSECT SWARM",     "sixteen wings, none agreeing",            1, INSECT_PARAMS, 3 },
    };
    constexpr int kNumChars = (int) (sizeof (CHAR_DESCS) / sizeof (CHAR_DESCS[0]));

    // Which characters own audio DSP — those work in EVERY host; the pure
    // modulators need a bus-consuming host. Mirrors CHAR_DESCS order.
    const bool CHAR_AUDIO[] = { false, true, true, true, false, false };
}

const Descriptor& DarkDrone::desc() const        { return CHAR_DESCS[0]; }
const Descriptor& PsychedelicPink::desc() const  { return CHAR_DESCS[1]; }
const Descriptor& IndustrialBlack::desc() const  { return CHAR_DESCS[2]; }
const Descriptor& GlassCathedral::desc() const   { return CHAR_DESCS[3]; }
const Descriptor& TapeSeance::desc() const       { return CHAR_DESCS[4]; }
const Descriptor& InsectSwarm::desc() const      { return CHAR_DESCS[5]; }

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
        case 0: ch = new DarkDrone(); break;
        case 1: ch = new PsychedelicPink(); break;
        case 2: ch = new IndustrialBlack(); break;
        case 3: ch = new GlassCathedral(); break;
        case 4: ch = new TapeSeance(); break;
        case 5: ch = new InsectSwarm(); break;
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
           + ",\"audio\":" + (CHAR_AUDIO[c] ? "1" : "0")
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

//==============================================================================
// Built-in rack presets. Each blob is SPARSE — only what the preset sets;
// everything else is the Kemper default (off / factory value), so a preset
// can never carry hidden state. The bench applies every one of these through
// fromJson and proves it bounded, deterministic and silence-preserving.
namespace
{
    struct PresetDef { const char* name; const char* blob; };
    const PresetDef PRESETS[] = {
        { "VELVET STAGE",
          "{\"modules\":{\"chorus\":{\"on\":1,\"p\":{\"mix\":30,\"depth\":45}},"
          "\"reverb\":{\"on\":1,\"p\":{\"mix\":20,\"character\":1,\"length\":220}}}}" },
        { "DUB TELEGRAPH",
          "{\"modules\":{\"delay\":{\"on\":1,\"p\":{\"mix\":30,\"time\":340,\"feedback\":55,\"character\":1}},"
          "\"lofi\":{\"on\":1,\"p\":{\"crush\":20,\"dirt\":40}}}}" },
        { "MOTOR CITY",
          "{\"modules\":{\"saturation\":{\"on\":1,\"p\":{\"drive\":10,\"tone\":60}},"
          "\"rotary\":{\"on\":1,\"p\":{\"speed\":0,\"mix\":100,\"growl\":30}}}}" },
        { "CATHEDRAL BLOOM",
          "{\"modules\":{\"shimmer\":{\"on\":1,\"p\":{\"mix\":35,\"size\":70,\"decay\":70,\"shimmer\":55}}},"
          "\"spectra\":{\"glass\":{\"on\":1,\"p\":{\"halo\":70,\"shine\":50}}}}" },
        { "PINK HAZE",
          "{\"spectra\":{\"pink\":{\"on\":1,\"p\":{\"swirl\":60,\"smear\":60,\"bloom\":55}}}}" },
        { "IRON WORKS",
          "{\"modules\":{\"strip\":{\"on\":1,\"p\":{\"amount\":45}}},"
          "\"spectra\":{\"black\":{\"on\":1,\"p\":{\"grind\":70,\"chop\":60,\"clang\":45}}}}" },
        { "SEANCE",
          "{\"modules\":{\"reverb\":{\"on\":1,\"p\":{\"mix\":22,\"character\":3,\"length\":300}}},"
          "\"spectra\":{\"tape\":{\"on\":1,\"p\":{\"wobble\":60,\"sag\":45,\"dull\":55}}}}" },
        { "THE SWARM",
          "{\"spectra\":{\"insect\":{\"on\":1,\"p\":{\"swarm\":70,\"flutter\":65,\"skitter\":50}},"
          "\"darkdrone\":{\"on\":1,\"p\":{\"cluster\":30,\"sag\":20,\"drift\":50,\"dtime\":60}}}}" },
        { "BROKEN TRANSMISSION",
          "{\"modules\":{\"kieranator\":{\"on\":1,\"x\":\"1000300010002060\",\"p\":{\"mix\":100,\"crush\":55}},"
          "\"lofi\":{\"on\":1,\"p\":{\"crush\":45,\"dirt\":35}}}}" },
        { "POSSESSED CHOIR",
          "{\"modules\":{\"chorus\":{\"on\":1,\"p\":{\"mix\":35}}},"
          "\"spectra\":{\"darkdrone\":{\"on\":1,\"p\":{\"cluster\":28}},"
          "\"glass\":{\"on\":1,\"p\":{\"halo\":60,\"shine\":65}}}}" },
    };
    constexpr int kNumPresets = (int) (sizeof (PRESETS) / sizeof (PRESETS[0]));
}

std::string presetsJson()
{
    std::string s = "[";
    for (int i = 0; i < kNumPresets; ++i)
    {
        if (i > 0) s += ",";
        s += "{\"name\":\"" + json::escape (PRESETS[i].name) + "\",\"blob\":" + PRESETS[i].blob + "}";
    }
    s += "]";
    return s;
}

int numPresets() { return kNumPresets; }
const char* presetName (int i) { return i >= 0 && i < kNumPresets ? PRESETS[i].name : ""; }
const char* presetBlob (int i) { return i >= 0 && i < kNumPresets ? PRESETS[i].blob : "{}"; }

} // namespace bwfx
