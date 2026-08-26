#include "bwfx.h"
#include "bwfx_dsp.h"
#include "bwfx_json.h"

#include <cstdio>
#include <cstring>

namespace bwfx
{

namespace
{
    // enable/presence ramp / structural dip: ~15 ms each way at 48 k
    inline float rampCoef (double fs) { return 1.0f - std::exp ((float) (-(double) kSubBlock / (0.005 * fs))); }

    // deterministic per-key stagger threshold in 0.08..0.92 for morph defection
    inline float staggerThreshold (const char* moduleId, const char* paramId)
    {
        uint32_t h = 2166136261u;
        for (const char* c = moduleId; *c; ++c) h = (h ^ (uint32_t) *c) * 16777619u;
        h = (h ^ (uint32_t) '/') * 16777619u;
        for (const char* c = paramId; *c; ++c) h = (h ^ (uint32_t) *c) * 16777619u;
        return 0.08f + 0.84f * (float) (h >> 8) / 16777216.0f;
    }
}

const char* Rack::version() { return "1.1.0"; }

// Parsed form of a state blob, defaults pre-filled.
struct Rack::BlobState
{
    float mix = 1.0f;
    int   order[kMaxModules] {};
    struct Mod
    {
        bool  on = false;
        float pr = 1.0f;
        float p[kMaxParams] {};
    } m[kMaxModules];
};

Rack::Rack()
{
    nTypes = numModuleTypes();
    for (int t = 0; t < nTypes; ++t)
        mods[(size_t) t].reset (createModule (t));

    int def[kMaxModules];
    for (int t = 0; t < nTypes; ++t) def[t] = t;
    orderPacked.store (packOrder (def, nTypes), std::memory_order_relaxed);
    orderApplied = orderPacked.load();
    for (auto& p : presenceIn) p.store (1.0f, std::memory_order_relaxed);
    env.fill (0.0f);
    wasOff.fill (true);
}

Rack::~Rack() = default;

uint64_t Rack::packOrder (const int* types, int count)
{
    uint64_t p = 0;
    for (int i = 0; i < count && i < kMaxModules; ++i)
        p |= (uint64_t) (types[i] & 0xF) << (4 * i);
    return p;
}

void Rack::unpackOrder (uint64_t packed, int* types) const
{
    for (int i = 0; i < nTypes; ++i)
        types[i] = (int) ((packed >> (4 * i)) & 0xF);
}

void Rack::prepare (double fsr, int maxBlockSize)
{
    fs = fsr;
    maxBlock = std::max (maxBlockSize, kSubBlock);
    for (int t = 0; t < nTypes; ++t)
        mods[(size_t) t]->prepare (fs, maxBlock);
    dryL.reset (new float[(size_t) maxBlock]);
    dryR.reset (new float[(size_t) maxBlock]);
    env.fill (0.0f);
    wasOff.fill (true);
    dip = 1.0f;
    mixSm = mixIn.load (std::memory_order_relaxed);
    orderApplied = orderPacked.load (std::memory_order_relaxed);
    prepared = true;
}

void Rack::service()
{
    for (int t = 0; t < nTypes; ++t)
        mods[(size_t) t]->service();
}

// --- edits -----------------------------------------------------------------
void Rack::setEnabled (int type, bool on)
{
    if (type < 0 || type >= nTypes) return;
    uint32_t bits = enabledBits.load (std::memory_order_relaxed);
    uint32_t nb;
    do
    {
        nb = on ? (bits | (1u << type)) : (bits & ~(1u << type));
    } while (! enabledBits.compare_exchange_weak (bits, nb, std::memory_order_relaxed));
}

void Rack::setParam (int type, int p, float v)
{
    if (type < 0 || type >= nTypes) return;
    const Descriptor& d = mods[(size_t) type]->desc();
    if (p < 0 || p >= d.numParams) return;
    const ParamDesc& pd = d.params[p];
    v = clampf (v, pd.lo, pd.hi);
    if (pd.step > 0) v = pd.lo + pd.step * std::round ((v - pd.lo) / pd.step);
    mods[(size_t) type]->setParam (p, v);
}

void Rack::setOrder (const int* types, int count)
{
    // accept only a true permutation of the registry
    if (count != nTypes) return;
    uint32_t seen = 0;
    for (int i = 0; i < count; ++i)
    {
        if (types[i] < 0 || types[i] >= nTypes) return;
        if (seen & (1u << types[i])) return;
        seen |= 1u << types[i];
    }
    orderPacked.store (packOrder (types, count), std::memory_order_relaxed);
}

void Rack::setMix (float v) { mixIn.store (clampf (v, 0.0f, 1.0f), std::memory_order_relaxed); }

void Rack::setPresence (int type, float v)
{
    if (type >= 0 && type < nTypes)
        presenceIn[(size_t) type].store (clampf (v, 0.0f, 1.0f), std::memory_order_relaxed);
}

// --- queries ---------------------------------------------------------------
bool Rack::anyEnabled() const  { return enabledBits.load (std::memory_order_relaxed) != 0; }
bool Rack::getEnabled (int type) const
{
    return type >= 0 && type < nTypes
        && (enabledBits.load (std::memory_order_relaxed) & (1u << type)) != 0;
}
float Rack::getParam (int type, int p) const
{
    return (type >= 0 && type < nTypes) ? mods[(size_t) type]->getParam (p) : 0.0f;
}
float Rack::getPresence (int type) const
{
    return (type >= 0 && type < nTypes) ? presenceIn[(size_t) type].load (std::memory_order_relaxed) : 1.0f;
}
void Rack::getOrder (int* types) const { unpackOrder (orderPacked.load (std::memory_order_relaxed), types); }
float Rack::getMix() const { return mixIn.load (std::memory_order_relaxed); }

// --- audio -----------------------------------------------------------------
void Rack::process (float* L, float* R, int n)
{
    if (! prepared || n <= 0) return;

    const uint32_t en = enabledBits.load (std::memory_order_relaxed);

    // The additive contract: an empty rack touches nothing. Structural
    // changes are also adopted silently here — nothing is sounding.
    bool anyEnv = false;
    for (int t = 0; t < nTypes; ++t) anyEnv |= env[(size_t) t] > 1e-4f;
    if (en == 0 && ! anyEnv)
    {
        orderApplied = orderPacked.load (std::memory_order_relaxed);
        dip = 1.0f;
        mixSm = mixIn.load (std::memory_order_relaxed);
        return;
    }

    n = std::min (n, maxBlock);
    std::memcpy (dryL.get(), L, sizeof (float) * (size_t) n);
    std::memcpy (dryR.get(), R, sizeof (float) * (size_t) n);

    const float coef = rampCoef (fs);
    int order[kMaxModules];

    int done = 0;
    while (done < n)
    {
        const int m = std::min (kSubBlock, n - done);
        float* bl = L + done;
        float* br = R + done;

        // structural change: dip the wet contribution, swap at the bottom
        const uint64_t want = orderPacked.load (std::memory_order_relaxed);
        float dipTarget = 1.0f;
        if (want != orderApplied)
        {
            dipTarget = 0.0f;
            if (dip < 0.02f)
            {
                orderApplied = want;
                dipTarget = 1.0f;
            }
        }
        const float dip0 = dip;
        dip += (dipTarget - dip) * coef;
        if (dipTarget >= 1.0f && dip > 0.9995f) dip = 1.0f;

        unpackOrder (orderApplied, order);
        for (int s = 0; s < nTypes; ++s)
        {
            const int t = order[s];
            const bool on = (en & (1u << t)) != 0;
            // the module's blend target is its PRESENCE while on, 0 while off
            const float target = on ? presenceIn[(size_t) t].load (std::memory_order_relaxed) : 0.0f;
            float e = env[(size_t) t];
            if (target <= 1e-4f && e <= 1e-4f) { env[(size_t) t] = 0.0f; wasOff[(size_t) t] = true; continue; }
            if (target > 1e-4f && wasOff[(size_t) t])
            {
                mods[(size_t) t]->reset();          // fresh start, no stale tails
                wasOff[(size_t) t] = false;
            }

            float e1 = e + (target - e) * coef;
            if (std::abs (e1 - target) < 5e-4f) e1 = target;
            env[(size_t) t] = e1;

            // duck buffer injection during TRANSITIONS only: how far the
            // module is along toward its own target, not the target itself —
            // a steady presence of 0.5 must not halve what enters the delay.
            const float ref = target > 0.05f ? e1 / target : e1 * 20.0f;
            mods[(size_t) t]->inputDuck (std::min (dip, clampf (ref, 0.0f, 1.0f)));

            if (e >= 1.0f && e1 >= 1.0f)
            {
                mods[(size_t) t]->process (bl, br, m);   // steady full: in place
            }
            else
            {
                float inL[kSubBlock], inR[kSubBlock];
                std::memcpy (inL, bl, sizeof (float) * (size_t) m);
                std::memcpy (inR, br, sizeof (float) * (size_t) m);
                mods[(size_t) t]->process (bl, br, m);
                for (int i = 0; i < m; ++i)              // fade the module in/out
                {
                    const float g = e + (e1 - e) * ((float) (i + 1) / (float) m);
                    bl[i] = inL[i] + (bl[i] - inL[i]) * g;
                    br[i] = inR[i] + (br[i] - inR[i]) * g;
                }
            }
        }

        // rack wet/dry and the structural dip
        const float mixT = mixIn.load (std::memory_order_relaxed);
        const float mix0 = mixSm;
        mixSm += (mixT - mixSm) * coef;
        if (std::abs (mixT - mixSm) < 5e-4f) mixSm = mixT;
        const float* dl = dryL.get() + done;
        const float* dr = dryR.get() + done;
        if (dip0 >= 1.0f && dip >= 1.0f && mix0 >= 1.0f && mixSm >= 1.0f)
        {
            // full wet, no transition: leave the chain output untouched
        }
        else
        {
            for (int i = 0; i < m; ++i)
            {
                const float u = (float) (i + 1) / (float) m;
                const float g = (dip0 + (dip - dip0) * u) * (mix0 + (mixSm - mix0) * u);
                bl[i] = dl[i] + (bl[i] - dl[i]) * g;
                br[i] = dr[i] + (br[i] - dr[i]) * g;
            }
        }

        done += m;
    }

    // Safety ceiling: the rack sits after the host's own limiter, so it must
    // not hand runaway feedback onward. Transparent below 0.98 (host-limited
    // material passes untouched), tanh knee above, asymptote ~1.25.
    for (int i = 0; i < n; ++i)
    {
        const float al = std::abs (L[i]);
        if (al > 0.98f) L[i] = (L[i] < 0 ? -1.0f : 1.0f) * (0.98f + 0.27f * std::tanh ((al - 0.98f) / 0.27f));
        const float ar = std::abs (R[i]);
        if (ar > 0.98f) R[i] = (R[i] < 0 ? -1.0f : 1.0f) * (0.98f + 0.27f * std::tanh ((ar - 0.98f) / 0.27f));
    }
}

// --- state -----------------------------------------------------------------
std::string Rack::toJson() const
{
    char buf[64];
    std::string s = "{\"v\":1,\"bwfx\":\"";
    s += version();
    s += "\",\"mix\":";
    std::snprintf (buf, sizeof (buf), "%g", (double) getMix());
    s += buf;
    s += ",\"order\":[";
    int order[kMaxModules];
    getOrder (order);
    for (int i = 0; i < nTypes; ++i)
    {
        if (i > 0) s += ",";
        s += "\"" + std::string (moduleDescriptor (order[i]).id) + "\"";
    }
    s += "],\"modules\":{";
    for (int t = 0; t < nTypes; ++t)
    {
        const Descriptor& d = mods[(size_t) t]->desc();
        if (t > 0) s += ",";
        s += "\"" + std::string (d.id) + "\":{\"ver\":" + std::to_string (d.version)
           + ",\"on\":" + (getEnabled (t) ? "1" : "0");
        std::snprintf (buf, sizeof (buf), ",\"pr\":%g", (double) getPresence (t));
        s += buf;
        s += ",\"p\":{";
        for (int p = 0; p < d.numParams; ++p)
        {
            if (p > 0) s += ",";
            std::snprintf (buf, sizeof (buf), "\"%s\":%g", d.params[p].id, (double) getParam (t, p));
            s += buf;
        }
        s += "}}";
    }
    s += "}}";
    return s;
}

void Rack::clearState()
{
    for (int t = 0; t < nTypes; ++t)
    {
        setEnabled (t, false);
        setPresence (t, 1.0f);
        const Descriptor& d = mods[(size_t) t]->desc();
        for (int p = 0; p < d.numParams; ++p)
            mods[(size_t) t]->setParam (p, d.params[p].def);
    }
    int def[kMaxModules];
    for (int t = 0; t < nTypes; ++t) def[t] = t;
    orderPacked.store (packOrder (def, nTypes), std::memory_order_relaxed);
    setMix (1.0f);
}

void Rack::parseBlob (const std::string& s, BlobState& bs) const
{
    // defaults first — a missing key is the default (the Kemper rule)
    bs.mix = 1.0f;
    for (int t = 0; t < nTypes; ++t)
    {
        bs.order[t] = t;
        bs.m[t].on = false;
        bs.m[t].pr = 1.0f;
        const Descriptor& d = moduleDescriptor (t);
        for (int p = 0; p < d.numParams; ++p) bs.m[t].p[p] = d.params[p].def;
    }
    if (s.empty()) return;
    json::Value v;
    if (! json::parse (s, v) || ! v.isObj()) return;

    if (const json::Value* mx = v.find ("mix"))
        bs.mix = clampf ((float) mx->asNum (1.0), 0.0f, 1.0f);

    if (const json::Value* modsV = v.find ("modules"))
        if (modsV->isObj())
            for (auto& kv : modsV->obj)
            {
                int type = -1;
                for (int t = 0; t < nTypes; ++t)
                    if (kv.first == moduleDescriptor (t).id) { type = t; break; }
                if (type < 0) continue;            // unknown module: ignored
                const Descriptor& d = moduleDescriptor (type);
                if (const json::Value* on = kv.second.find ("on"))
                    bs.m[type].on = on->asNum (0) > 0.5;
                if (const json::Value* pr = kv.second.find ("pr"))
                    bs.m[type].pr = clampf ((float) pr->asNum (1.0), 0.0f, 1.0f);
                if (const json::Value* pv = kv.second.find ("p"))
                    if (pv->isObj())
                        for (auto& pkv : pv->obj)
                            for (int p = 0; p < d.numParams; ++p)
                                if (pkv.first == d.params[p].id)
                                {
                                    const ParamDesc& pd = d.params[p];
                                    float val = clampf ((float) pkv.second.asNum (pd.def), pd.lo, pd.hi);
                                    if (pd.step > 0) val = pd.lo + pd.step * std::round ((val - pd.lo) / pd.step);
                                    bs.m[type].p[p] = val;
                                    break;
                                }
            }

    if (const json::Value* ord = v.find ("order"))
        if (ord->isArr())
        {
            int order[kMaxModules];
            uint32_t seen = 0;
            int count = 0;
            for (auto& e : ord->arr)
            {
                if (e.t != json::Value::Str) continue;
                for (int t = 0; t < nTypes; ++t)
                    if (e.str == moduleDescriptor (t).id && ! (seen & (1u << t)))
                    {
                        order[count++] = t;
                        seen |= 1u << t;
                        break;
                    }
            }
            for (int t = 0; t < nTypes; ++t)       // modules the blob predates
                if (! (seen & (1u << t)))
                    order[count++] = t;
            if (count == nTypes)
                for (int t = 0; t < nTypes; ++t) bs.order[t] = order[t];
        }
}

void Rack::applyBlobState (const BlobState& bs)
{
    for (int t = 0; t < nTypes; ++t)
    {
        setEnabled (t, bs.m[t].on);
        setPresence (t, bs.m[t].pr);
        const Descriptor& d = moduleDescriptor (t);
        for (int p = 0; p < d.numParams; ++p)
            mods[(size_t) t]->setParam (p, bs.m[t].p[p]);
    }
    setOrder (bs.order, nTypes);
    setMix (bs.mix);
}

void Rack::fromJson (const std::string& s)
{
    BlobState bs;
    parseBlob (s, bs);
    applyBlobState (bs);
}

void Rack::applyMorph (const std::string& a, const std::string& b, float t)
{
    t = clampf (t, 0.0f, 1.0f);
    BlobState A, B, out;
    parseBlob (a, A);
    parseBlob (b, B);
    out = A;

    for (int m = 0; m < nTypes; ++m)
    {
        const Descriptor& d = moduleDescriptor (m);
        const float prA = A.m[m].on ? A.m[m].pr : 0.0f;
        const float prB = B.m[m].on ? B.m[m].pr : 0.0f;
        const float pres = prA + (prB - prA) * t;
        out.m[m].on = pres > 1e-3f;
        out.m[m].pr = pres;

        for (int p = 0; p < d.numParams; ++p)
        {
            const float vA = A.m[m].p[p], vB = B.m[m].p[p];
            if (prA <= 1e-3f)      out.m[m].p[p] = vB;   // fading in: B's sound
            else if (prB <= 1e-3f) out.m[m].p[p] = vA;   // fading out: A's sound
            else if (d.params[p].step > 0)               // stepped: staggered defection
                out.m[m].p[p] = t < staggerThreshold (d.id, d.params[p].id) ? vA : vB;
            else                                         // continuous: glide
                out.m[m].p[p] = vA + (vB - vA) * t;
        }
    }

    // the chain order cannot lerp: it defects once, through the dip
    for (int i = 0; i < nTypes; ++i)
        out.order[i] = t < 0.5f ? A.order[i] : B.order[i];
    out.mix = A.mix + (B.mix - A.mix) * t;

    applyBlobState (out);
}

std::string Rack::uiStateJson() const
{
    // same shape as toJson plus nothing — the UI reads the state blob format
    return toJson();
}

} // namespace bwfx
