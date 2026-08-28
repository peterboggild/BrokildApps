// Brokild World FX — MACROS.
//
// Five host parameters, and the only part of the rack a DAW can automate.
// The count is frozen: these become parameters in every synth, and a
// parameter list that grows is the one thing the blob design exists to avoid.
//
// A macro ADDS to its destinations rather than setting them — the Mars Wars
// patch-bay model. The panel keeps showing the patch's own value, an
// automation lane never fights a knob, and a macro at rest is exactly the
// patch. That last property is the contract: neutral at zero, like an empty
// rack, a presence, or the world-mod bus.
//
// Destinations are addressed by NAME ("echo.time", "space.pr", "mix"), never
// by index, so an assignment survives the registry gaining or losing modules,
// and one this build does not recognise is kept verbatim and written back out.

#include "bwfx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace bwfx
{

namespace
{
    float clamp01f (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    /*  The one destination that ships assigned. Macro 5 holds the rack's
        dry/wet at FULL NEGATIVE depth, which reads backwards until you think
        about what neutral-at-zero means: at rest the rack is at the patch's
        own mix, and pushing the macro up pulls the effects out. Adding
        effects the patch never had would be the other contract. */
    constexpr int   kDefaultMacro  = kMacros - 1;
    const     char* kDefaultDest   = "mix";
    constexpr float kDefaultDepth  = -1.0f;
}

void Rack::setMacro (int i, float v)
{
    if (i >= 0 && i < kMacros) macroIn[(size_t) i].store (clamp01f (v), std::memory_order_relaxed);
}

float Rack::getMacro (int i) const
{
    return (i >= 0 && i < kMacros) ? macroIn[(size_t) i].load (std::memory_order_relaxed) : 0.0f;
}

/*  The default wiring is IMPLICIT — a blob with no "m" key means macro 5
    holds the dry/wet, which is what keeps pre-macro blobs byte-identical.
    The moment anything is edited the rack has to state its wiring in full,
    and that means writing the default down rather than discarding it.
    Discarding it is what the first version did, so assigning anything to
    any macro silently took the dry/wet off macro 5. */
void Rack::materialiseDefault()
{
    if (! macroDefaulted) return;
    macroDefaulted = false;
    for (auto& v : macroAssign) v.clear();
    macroAssign[(size_t) kDefaultMacro].push_back ({ kDefaultDest, kDefaultDepth });
}

void Rack::setMacroAssign (int macro, const std::string& dest, float depth)
{
    if (macro < 0 || macro >= kMacros || dest.empty()) return;
    materialiseDefault();
    auto& v = macroAssign[(size_t) macro];
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].dest == dest)
        {
            if (std::fabs (depth) < 1e-4f) v.erase (v.begin() + (long) i);
            else                           v[i].depth = depth < -1.0f ? -1.0f : (depth > 1.0f ? 1.0f : depth);
            republishMacros();
            return;
        }
    if (std::fabs (depth) < 1e-4f) return;  // removing something not there
    //  the cap is the whole table, not per macro — 64 assignments is far more
    //  than a patch will ever want, and a bounded table is what lets the audio
    //  thread read it without allocating
    int total = 0;
    for (const auto& a : macroAssign) total += (int) a.size();
    if (total >= kMaxMacroDest) return;
    v.push_back ({ dest, depth < -1.0f ? -1.0f : (depth > 1.0f ? 1.0f : depth) });
    republishMacros();
}

void Rack::clearMacroAssigns (int macro)
{
    if (macro < 0 || macro >= kMacros) return;
    materialiseDefault();          // ...then take this one away, default or not
    macroAssign[(size_t) macro].clear();
    republishMacros();
}

/*  Resolve names to indices and publish. Message thread: it allocates
    nothing the audio thread can see, fills the spare slot of the double
    buffer, and swaps — the same publication the KIERANATOR uses for its
    pattern, so the audio thread never reads a half-written table.

    Offsets left behind by a destination that has just been unassigned would
    otherwise stay applied forever, so every module offset is zeroed here
    first. Twelve modules of sixteen is 192 stores, once, on an edit. */
void Rack::republishMacros()
{
    for (int t = 0; t < nTypes; ++t)
        for (int p = 0; p < kMaxParams; ++p)
            mods[(size_t) t]->setParamOffset (p, 0.0f);
    for (auto& p : presenceOff) p.store (0.0f, std::memory_order_relaxed);
    mixOff.store (0.0f, std::memory_order_relaxed);

    const int idx = destIdx.load (std::memory_order_relaxed);
    auto& out = destBuf[(size_t) (idx ^ 1)];
    int n = 0;

    auto add = [&] (int macro, const std::string& dest, float depth)
    {
        if (n >= kMaxMacroDest) return;
        RDest d;
        d.macro = (uint8_t) macro;
        d.depth = depth;

        if (dest == "mix") { d.kind = 1; d.lo = 0.0f; d.hi = 1.0f; out[(size_t) n++] = d; return; }

        const auto dot = dest.find ('.');
        if (dot == std::string::npos || dot == 0) return;
        const std::string modId = dest.substr (0, dot);
        const std::string parId = dest.substr (dot + 1);

        for (int t = 0; t < nTypes; ++t)
        {
            const Descriptor& md = mods[(size_t) t]->desc();
            if (modId != md.id) continue;
            d.type = (uint8_t) t;
            if (parId == "pr") { d.kind = 3; d.lo = 0.0f; d.hi = 1.0f; out[(size_t) n++] = d; return; }
            for (int p = 0; p < md.numParams; ++p)
                if (parId == md.params[p].id)
                {
                    d.kind = 2;
                    d.param = (uint8_t) p;
                    d.lo = md.params[p].lo;
                    d.hi = md.params[p].hi;
                    out[(size_t) n++] = d;
                    return;
                }
            return;                         // known module, unknown parameter
        }
        //  unknown module: kept in macroAssign (so it round-trips) but not
        //  resolved, so it is simply inert in this build
    };

    if (macroDefaulted) add (kDefaultMacro, kDefaultDest, kDefaultDepth);
    else
        for (int m = 0; m < kMacros; ++m)
            for (const auto& a : macroAssign[(size_t) m]) add (m, a.dest, a.depth);

    destCount[(size_t) (idx ^ 1)] = n;
    destIdx.store (idx ^ 1, std::memory_order_release);
}

/*  Audio thread, once per sub-block. The offset is computed so that the SUM
    lands inside the destination's own range, which is why Module::getParam
    can be a plain add rather than a clamp on every read. */
void Rack::applyMacros()
{
    const int idx = destIdx.load (std::memory_order_acquire);
    const auto& tab = destBuf[(size_t) idx];
    const int n = destCount[(size_t) idx];
    if (n == 0) return;

    float mo = 0.0f;
    bool  moTouched = false;

    for (int i = 0; i < n; ++i)
    {
        const RDest& d = tab[(size_t) i];
        const float v = macroIn[(size_t) d.macro].load (std::memory_order_relaxed);
        const float delta = v * d.depth * (d.hi - d.lo);

        switch (d.kind)
        {
            case 1:                                          // rack mix
            {
                const float base = mixIn.load (std::memory_order_relaxed);
                const float want = base + delta;
                mo += (want < d.lo ? d.lo : (want > d.hi ? d.hi : want)) - base;
                moTouched = true;
                break;
            }
            case 2:                                          // module parameter
            {
                const float base = mods[(size_t) d.type]->getParamRaw (d.param);
                const float want = base + delta;
                mods[(size_t) d.type]->setParamOffset (
                    d.param, (want < d.lo ? d.lo : (want > d.hi ? d.hi : want)) - base);
                break;
            }
            case 3:                                          // module presence
            {
                const float base = presenceIn[(size_t) d.type].load (std::memory_order_relaxed);
                const float want = base + delta;
                presenceOff[(size_t) d.type].store (
                    (want < 0.0f ? 0.0f : (want > 1.0f ? 1.0f : want)) - base,
                    std::memory_order_relaxed);
                break;
            }
            default: break;
        }
    }
    if (moTouched) mixOff.store (mo, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// State. Assignments are STRUCTURE: they ride in the blob, travel with a
// patch, cross synths, and a morph leaves them alone. The macro VALUES do not
// live here at all — they are host parameters, and the host owns them.
//
// A rack that has never had an assignment edited emits NO "m" key, so every
// blob written before macros existed is byte-identical to one written after.

std::string Rack::macroAssignJson() const
{
    std::string s = "[";
    for (int m = 0; m < kMacros; ++m)
    {
        if (m) s += ",";
        s += "[";
        if (macroDefaulted)
        {
            if (m == kDefaultMacro)
            {
                char buf[64];
                std::snprintf (buf, sizeof (buf), "{\"d\":\"%s\",\"a\":%d}",
                               kDefaultDest, (int) std::lround (kDefaultDepth * 100.0f));
                s += buf;
            }
        }
        else
        {
            bool first = true;
            for (const auto& a : macroAssign[(size_t) m])
            {
                if (! first) s += ",";
                first = false;
                char buf[96];
                std::snprintf (buf, sizeof (buf), "{\"d\":\"%s\",\"a\":%d}",
                               a.dest.c_str(), (int) std::lround (a.depth * 100.0f));
                s += buf;
            }
        }
        s += "]";
    }
    return s + "]";
}

bool Rack::macroIsDefault() const { return macroDefaulted; }

void Rack::macroSetDefault()
{
    macroDefaulted = true;
    for (auto& v : macroAssign) v.clear();
    republishMacros();
}

/*  Parsed straight out of the blob text rather than through the blob struct:
    assignments are a list of variable length, and threading that through
    BlobState would buy nothing — parseBlob's job is the fixed shape. */
void Rack::macroFromBlob (const std::string& s)
{
    const auto key = s.find ("\"m\":");
    if (key == std::string::npos) { macroSetDefault(); return; }

    macroDefaulted = false;
    for (auto& v : macroAssign) v.clear();

    size_t i = s.find ('[', key);
    if (i == std::string::npos) { republishMacros(); return; }
    ++i;
    for (int m = 0; m < kMacros; ++m)
    {
        const auto open = s.find ('[', i);
        if (open == std::string::npos) break;
        const auto close = s.find (']', open);
        if (close == std::string::npos) break;
        const std::string body = s.substr (open + 1, close - open - 1);

        size_t p = 0;
        while (true)
        {
            const auto dq = body.find ("\"d\":\"", p);
            if (dq == std::string::npos) break;
            const auto de = body.find ('"', dq + 5);
            if (de == std::string::npos) break;
            const std::string dest = body.substr (dq + 5, de - dq - 5);
            const auto aq = body.find ("\"a\":", de);
            float depth = 1.0f;
            if (aq != std::string::npos) depth = (float) std::atof (body.c_str() + aq + 4) / 100.0f;
            if (! dest.empty() && std::fabs (depth) >= 1e-4f)
            {
                int total = 0;
                for (const auto& a : macroAssign) total += (int) a.size();
                if (total < kMaxMacroDest)
                    macroAssign[(size_t) m].push_back (
                        { dest, depth < -1.0f ? -1.0f : (depth > 1.0f ? 1.0f : depth) });
            }
            p = de + 1;
        }
        i = close + 1;
    }
    republishMacros();
}

std::string Rack::macroToBlob() const
{
    if (macroDefaulted) return {};          // byte-identical to a pre-macro blob
    return ",\"m\":" + macroAssignJson();
}

} // namespace bwfx
