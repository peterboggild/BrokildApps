#include "Modulation.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace ax
{

//==============================================================================
namespace
{
    const char* const NEVER[] = {
        "level", "sat", "habit", "temp",           // loudness, the specimen, itself
        "conform", "ref", "octave", "bendr",       // anything that retunes a held note
        "voices", "glide",                         // voice allocation
        "extent", "peaks"                          // stepped, and they resize the body
    };
    const char* const SCENE[] = { "travel", "cutang", "cutpos" };

    inline bool inList (const char* id, const char* const* list, int n)
    {
        for (int i = 0; i < n; ++i) if (std::strcmp (id, list[i]) == 0) return true;
        return false;
    }

    // a small deterministic generator; the salt keeps it clear of every other
    // per-habit draw in the instrument
    struct Rng
    {
        uint64_t s;
        explicit Rng (uint64_t seed) : s (seed * 6364136223846793005ull + 1442695040888963407ull) {}
        uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return (uint32_t) (s >> 33); }
        float uni() { return (float) (next() & 0xffffff) / 16777216.0f; }
        int   pick (int n) { return n > 0 ? (int) (next() % (uint32_t) n) : 0; }
        float range (float a, float b) { return a + (b - a) * uni(); }
    };
}

int modTier (const char* id)
{
    if (inList (id, NEVER, (int) (sizeof NEVER / sizeof NEVER[0]))) return 0;
    if (inList (id, SCENE, (int) (sizeof SCENE / sizeof SCENE[0]))) return 2;
    return 1;
}

//==============================================================================
ModMatrix modMatrixFor (int habit)
{
    ModMatrix m;
    habit = ((habit % 256) + 256) % 256;
    Rng r ((uint64_t) habit ^ 0x4D0D5A11ull);

    // who may be driven, and who may drive
    int dstPool[64], nDst = 0, srcPool[64], nSrc = 0;
    for (int i = 0; i < numParams() && nDst < 64; ++i)
    {
        const int t = modTier (paramSpec (i).id);
        if (t != 0) dstPool[nDst++] = i;
        /*  A source is only ever READ, so anything the player can set may drive
            something -- including the controls that must not themselves be
            driven. Turning CEILING up and hearing four other things start to
            move is a perfectly good instrument. */
        if (std::strcmp (paramSpec (i).id, "habit") != 0
            && std::strcmp (paramSpec (i).id, "temp") != 0 && nSrc < 64)
            srcPool[nSrc++] = i;
    }
    if (nDst < 4 || nSrc < 4) return m;

    /*  FOUR SOURCES, each fanning out to two or three destinations. Fanning is
        the point: one control moving three or four others is what was asked
        for, and a matrix of unrelated one-to-one wires would not feel like
        anything. A destination is used at most twice, so nothing is the target
        of the whole panel. */
    int used[64]; for (int i = 0; i < 64; ++i) used[i] = 0;

    for (int s = 0; s < 4 && m.n < 16; ++s)
    {
        const int src = srcPool[r.pick (nSrc)];
        const int fan = 2 + r.pick (2);              // two or three
        for (int f = 0; f < fan && m.n < 16; ++f)
        {
            int dst = -1;
            for (int attempt = 0; attempt < 24; ++attempt)
            {
                const int cand = r.pick (nDst);
                if (dstPool[cand] == src) continue;
                if (used[cand] >= 2) continue;
                dst = cand; break;
            }
            if (dst < 0) continue;
            ++used[dst];

            ModWire w;
            w.src   = src;
            w.dst   = dstPool[dst];
            w.kind  = (r.uni() < 0.62f) ? 0 : 1;      // mostly depth, sometimes rate
            w.depth = r.range (0.35f, 1.0f);
            /*  Slow. This is a body settling, not a tremolo -- the fastest wire
                here takes two seconds to come round, the slowest most of a
                minute. */
            w.rate  = r.range (0.02f, 0.45f);
            w.phase = r.uni();
            m.w[m.n++] = w;
        }
    }
    return m;
}

//==============================================================================
void Modulator::setHabit (int h)
{
    h = ((h % 256) + 256) % 256;
    if (h == habit) return;
    habit = h;
    m = modMatrixFor (h);
    for (int i = 0; i < 16; ++i) ph[i] = m.w[i].phase;
}

static inline float srcNorm (const float* stored, int idx)
{
    const PSpec& s = paramSpec (idx);
    const float hi = paramMax (s);
    return hi > 0.0f ? std::min (1.0f, std::max (0.0f, stored[idx] / hi)) : 0.0f;
}

void Modulator::advance (double dt, const float* stored)
{
    if (dt <= 0.0) return;
    for (int i = 0; i < m.n; ++i)
    {
        const ModWire& w = m.w[i];
        float rate = w.rate;
        /*  A RATE wire does not move its destination further, it moves it
            sooner: the source multiplies the speed between a quarter and four
            times. That is the "or change frequency" half of the brief. */
        if (w.kind == 1) rate *= 0.25f + 3.75f * srcNorm (stored, w.src);
        ph[i] += (float) (dt * (double) rate);
        if (ph[i] > 1.0e6f) ph[i] = std::fmod (ph[i], 1.0f);
    }
}

/*  The two allowances, and the whole of the temperature law. */
static inline void allowances (float kelvin, float& ordinary, float& scene)
{
    const float k = std::min (1.0f, std::max (0.0f,
                        (kelvin - TEMP_MIN) / (TEMP_MAX - TEMP_MIN)));
    ordinary = EXC_ORDINARY * k;
    const float h = std::min (1.0f, std::max (0.0f,
                        (kelvin - TEMP_SCENE_ONSET) / (TEMP_MAX - TEMP_SCENE_ONSET)));
    scene = EXC_SCENE * h;
}

void Modulator::apply (const float* stored, float* eff, float kelvin) const
{
    const int n = numParams();
    std::memcpy (eff, stored, sizeof (float) * (size_t) n);

    float ordinary = 0.0f, scene = 0.0f;
    allowances (kelvin, ordinary, scene);
    if (ordinary <= 0.0f && scene <= 0.0f) return;   // frozen: an exact copy

    float off[128] = {};
    for (int i = 0; i < m.n && i < 16; ++i)
    {
        const ModWire& w = m.w[i];
        if (w.dst >= 128) continue;
        const float allow = (modTier (paramSpec (w.dst).id) == 2) ? scene : ordinary;
        if (allow <= 0.0f) continue;
        float amp = w.depth;
        if (w.kind == 0) amp *= srcNorm (stored, w.src);   // the source sets the depth
        off[w.dst] += amp * std::sin (6.283185307179586f * ph[i]);
    }

    for (int i = 0; i < n && i < 128; ++i)
    {
        if (off[i] == 0.0f) continue;
        const PSpec& s = paramSpec (i);
        const float allow = (modTier (s.id) == 2) ? scene : ordinary;
        const float hi = paramMax (s);
        /*  Clamp the TOTAL displacement, not each wire's share, so a
            destination wired twice cannot go twice as far. */
        float d = off[i];
        if (d >  allow) d =  allow;
        if (d < -allow) d = -allow;
        eff[i] = std::min (hi, std::max (0.0f, stored[i] + d * hi));
    }
}

float Modulator::displacement (int paramIndex, const float* stored, float kelvin) const
{
    float ordinary = 0.0f, scene = 0.0f;
    allowances (kelvin, ordinary, scene);
    const float allow = (modTier (paramSpec (paramIndex).id) == 2) ? scene : ordinary;
    if (allow <= 0.0f) return 0.0f;
    float d = 0.0f;
    for (int i = 0; i < m.n && i < 16; ++i)
    {
        const ModWire& w = m.w[i];
        if (w.dst != paramIndex) continue;
        float amp = w.depth;
        if (w.kind == 0) amp *= srcNorm (stored, w.src);
        d += amp * std::sin (6.283185307179586f * ph[i]);
    }
    if (d >  allow) d =  allow;
    if (d < -allow) d = -allow;
    return allow > 0.0f ? d / allow : 0.0f;
}

} // namespace ax
