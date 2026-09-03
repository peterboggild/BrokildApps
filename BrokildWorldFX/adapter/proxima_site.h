#pragma once

/*  THE SITE — what the Proxima findings share.

    Four artefacts were recovered from one site, and on the bench they keep
    behaving as if they still were: they share the CLIMATE, and cooled and
    close together they fall into step. This header is that, for any plugin
    that includes it. It is the only thing the findings have in common at the
    code level, so it lives beside the other shared adapter headers.

    MECHANISM. One small named shared-memory block ("Local\\BrokildProximaSite",
    a Windows file mapping in the login session), which is the only sanctioned
    way for four DIFFERENT plugin DLLs to see each other — and it works across
    processes too, so DAW instances and standalones share one site. Header-
    only, compiled into each plugin (the house rule: no runtime DLLs). Every
    field is a lock-free atomic; there is no lock anywhere, so a crashed
    instance cannot wedge the others — its slot's heartbeat simply goes stale.

    TIMING IS A NEGOTIATION, NOT A CLOCK. No master pulse is imposed. Each
    instance advances its own site phase at its own natural rate and pulls
    toward the circular MEAN of the others' phases — Kuramoto's model, the
    textbook account of how coupled oscillators synchronise spontaneously,
    and a cousin of the Mirollo-Strogatz mechanism B2311.1 is built on. The
    pull is  K = coupling x (1 - distance) x (1 - warmth):  cold and close,
    the site locks; hot or far, every object runs its own time. That single
    line is the whole thesis, and it is thermodynamics: low temperature is
    low entropy.

    WHAT IT PROMISES.
      * Coupling OFF (the default): every read returns the caller's own
        values and every pull is exactly zero, so an uncoupled render is
        bit-identical to a build that never heard of this file. Bench that.
      * Offline render: the caller checks isNonRealtime() and simply does
        not call sync(); the last-known values hold.
      * Settings persist in %APPDATA%\Brokild\ProximaSite.json; the block is
        the runtime authority and any instance's SITE panel may change it.
      * Versioned. A fifth finding joins by including this header. */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shlobj.h>
#endif

namespace proxima
{

constexpr uint32_t kMagic   = 0x53425850u;   // 'PXBS'
constexpr uint32_t kVersion = 1;
constexpr int      kSlots   = 16;
constexpr uint64_t kStaleMs = 2500;          // a heartbeat older than this is a ghost

struct Slot
{
    std::atomic<uint64_t> id;          // 0 = free
    std::atomic<uint64_t> beatMs;      // GetTickCount64 of the last publish
    std::atomic<uint32_t> kind;        // 1, 22, 67, 104 ...
    std::atomic<float>    activity;    // 0..1, how much it is carrying
    std::atomic<float>    localKelvin; // its own ambient, for the record
    std::atomic<float>    phase;       // its own site phase, 0..1
    std::atomic<float>    natHz;       // its natural rate
    std::atomic<uint32_t> pad_;
};

struct Block
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> version;
    std::atomic<uint32_t> size;
    std::atomic<uint32_t> settingsSeq;

    //  the settings (the JSON's runtime mirror)
    std::atomic<uint32_t> climateShared;
    std::atomic<uint32_t> timingShared;
    std::atomic<float>    distance;      // 0 same bench .. 1 different rooms
    std::atomic<float>    pulseHz;       // the site's natural breath, ~0.5

    //  the climate
    std::atomic<float>    siteKelvin;
    std::atomic<uint32_t> climateSeq;
    std::atomic<uint64_t> climateOwner;

    Slot slots[kSlots];
};

static_assert (std::atomic<uint64_t>::is_always_lock_free, "site block needs lock-free atomics");
static_assert (std::atomic<double>::is_always_lock_free,   "site block needs lock-free atomics");

//==============================================================================
struct Settings
{
    bool  climate  = false;
    bool  timing   = false;
    float distance = 0.5f;
    float pulseHz  = 0.5f;
};

inline std::string settingsPath()
{
   #if defined(_WIN32)
    char buf[MAX_PATH] = {};
    if (SUCCEEDED (SHGetFolderPathA (nullptr, CSIDL_APPDATA, nullptr, 0, buf)))
    {
        std::string p (buf);
        p += "\\Brokild";
        CreateDirectoryA (p.c_str(), nullptr);
        return p + "\\ProximaSite.json";
    }
   #endif
    return "ProximaSite.json";
}

//  a deliberately tiny reader: the file is ours, five keys, flat
inline Settings loadSettings()
{
    Settings s;
    std::ifstream f (settingsPath());
    if (! f) return s;
    std::stringstream ss; ss << f.rdbuf();
    const std::string t = ss.str();
    auto num = [&] (const char* key, float def) -> float
    {
        const auto at = t.find (std::string ("\"") + key + "\"");
        if (at == std::string::npos) return def;
        const auto colon = t.find (':', at);
        if (colon == std::string::npos) return def;
        return (float) std::atof (t.c_str() + colon + 1);
    };
    s.climate  = num ("climate", 0) >= 0.5f;
    s.timing   = num ("timing", 0) >= 0.5f;
    s.distance = std::fmin (1.0f, std::fmax (0.0f, num ("distance", 0.5f)));
    s.pulseHz  = std::fmin (4.0f, std::fmax (0.05f, num ("pulseHz", 0.5f)));
    return s;
}

inline void saveSettings (const Settings& s)
{
    std::ofstream f (settingsPath(), std::ios::trunc);
    if (! f) return;
    f << "{\n  \"version\": 1,\n"
      << "  \"climate\": " << (s.climate ? 1 : 0) << ",\n"
      << "  \"timing\": "  << (s.timing  ? 1 : 0) << ",\n"
      << "  \"distance\": " << s.distance << ",\n"
      << "  \"pulseHz\": "  << s.pulseHz  << "\n}\n";
}

//==============================================================================
/*  One per plugin instance. open() at construction, close() at destruction,
    publish()/sync() from the message-thread timer (never the audio thread —
    the audio thread reads the plain floats the caller copies out of here). */
class Client
{
public:
    Client() = default;
    ~Client() { close(); }
    Client (const Client&) = delete;
    Client& operator= (const Client&) = delete;

    bool open (uint32_t kindTag)
    {
       #if defined(_WIN32)
        kind = kindTag;
        map = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, (DWORD) sizeof (Block), L"Local\\BrokildProximaSite");
        if (map == nullptr) return false;
        const bool creator = GetLastError() != ERROR_ALREADY_EXISTS;
        blk = (Block*) MapViewOfFile (map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (Block));
        if (blk == nullptr) { CloseHandle (map); map = nullptr; return false; }

        if (creator)
        {
            //  a fresh mapping is zero-filled; write the settings, then the
            //  magic LAST with release semantics so a joiner never sees a
            //  half-written block
            const Settings s = loadSettings();
            blk->version.store (kVersion);
            blk->size.store ((uint32_t) sizeof (Block));
            blk->climateShared.store (s.climate ? 1u : 0u);
            blk->timingShared.store (s.timing ? 1u : 0u);
            blk->distance.store (s.distance);
            blk->pulseHz.store (s.pulseHz);
            blk->siteKelvin.store (0.0f);          // 0 = nobody has spoken yet
            blk->settingsSeq.store (1);
            blk->magic.store (kMagic, std::memory_order_release);
        }
        else
        {
            //  wait, briefly, for the creator to finish
            for (int i = 0; i < 200 && blk->magic.load (std::memory_order_acquire) != kMagic; ++i)
                Sleep (1);
            if (blk->magic.load (std::memory_order_acquire) != kMagic
                || blk->version.load() != kVersion)
            { detach(); return false; }
        }

        //  an id, and a slot
        id = ((uint64_t) GetCurrentProcessId() << 40) ^ ((uint64_t) GetTickCount64() << 8)
           ^ (uint64_t) (uintptr_t) this;
        if (id == 0) id = 1;
        const uint64_t now = GetTickCount64();
        for (int i = 0; i < kSlots && slot < 0; ++i)
        {
            uint64_t cur = blk->slots[i].id.load();
            const uint64_t beat = blk->slots[i].beatMs.load();
            if (cur != 0 && now - beat < kStaleMs) continue;     // live, not ours
            if (blk->slots[i].id.compare_exchange_strong (cur, id))
            {
                slot = i;
                blk->slots[i].kind.store (kind);
                blk->slots[i].beatMs.store (now);
                blk->slots[i].activity.store (0.0f);
                blk->slots[i].phase.store (0.0f);
                blk->slots[i].natHz.store (blk->pulseHz.load());
            }
        }
        cached = readSettings();
        return true;
       #else
        (void) kindTag; return false;
       #endif
    }

    void close()
    {
       #if defined(_WIN32)
        if (blk != nullptr && slot >= 0)
        {
            blk->slots[slot].beatMs.store (0);
            blk->slots[slot].id.store (0);
        }
        detach();
       #endif
    }

    bool isOpen() const { return blk != nullptr; }

    //  ---- settings -------------------------------------------------------
    Settings settings() const { return cached; }

    //  from any instance's SITE panel: the block is updated for everyone now,
    //  the JSON for next time
    void setSettings (const Settings& s)
    {
        cached = s;
        saveSettings (s);
        if (blk == nullptr) return;
        blk->climateShared.store (s.climate ? 1u : 0u);
        blk->timingShared.store (s.timing ? 1u : 0u);
        blk->distance.store (s.distance);
        blk->pulseHz.store (s.pulseHz);
        blk->settingsSeq.fetch_add (1);
    }

    //  ---- the per-tick exchange -------------------------------------------
    struct View
    {
        bool  climateShared = false, timingShared = false;
        float distance = 1.0f, pulseHz = 0.5f;
        int   others = 0;              // live instances besides me
        float siteKelvin = 0.0f;       // 0 if nobody has spoken
        bool  climateMoved = false;    // someone ELSE moved it since my last sync
        float meanPhase = 0.0f;        // circular mean of the others' phases
        float coherence = 0.0f;        // Kuramoto order parameter R over all live
        float othersActivity = 0.0f;   // mean activity of the others
    };

    /*  Publish my state and read theirs. `warmth` is my 0..1 temperature
        position, `phase` my current site phase, `myKelvin` my ambient in K.
        Returns the view; the caller applies the pull it computes from it. */
    View sync (float activity, float myKelvin, float phase, float warmth)
    {
        View v;
       #if defined(_WIN32)
        if (blk == nullptr || slot < 0) return v;
        (void) warmth;
        //  settings may have been changed by another instance
        const uint32_t sseq = blk->settingsSeq.load();
        if (sseq != lastSettingsSeq) { cached = readSettings(); lastSettingsSeq = sseq; }
        v.climateShared = cached.climate; v.timingShared = cached.timing;
        v.distance = cached.distance;     v.pulseHz = cached.pulseHz;

        const uint64_t now = GetTickCount64();
        Slot& me = blk->slots[slot];
        me.beatMs.store (now);
        me.activity.store (activity);
        me.localKelvin.store (myKelvin);
        me.phase.store (phase);
        me.natHz.store (cached.pulseHz * natRateMul());

        //  the climate
        const uint32_t cseq = blk->climateSeq.load (std::memory_order_acquire);
        v.siteKelvin = blk->siteKelvin.load();
        if (cseq != lastClimateSeq)
        {
            v.climateMoved = blk->climateOwner.load() != id;
            lastClimateSeq = cseq;
        }

        //  the others: circular mean phase, coherence, activity
        double sx = std::cos (6.283185307 * phase), sy = std::sin (6.283185307 * phase);
        double ox = 0, oy = 0, act = 0; int n = 0;
        for (int i = 0; i < kSlots; ++i)
        {
            if (i == slot) continue;
            const Slot& o = blk->slots[i];
            if (o.id.load() == 0 || now - o.beatMs.load() > kStaleMs) continue;
            const double ph = o.phase.load();
            ox += std::cos (6.283185307 * ph); oy += std::sin (6.283185307 * ph);
            act += o.activity.load();
            ++n;
        }
        v.others = n;
        if (n > 0)
        {
            v.meanPhase = (float) (std::atan2 (oy, ox) / 6.283185307);
            if (v.meanPhase < 0) v.meanPhase += 1.0f;
            v.othersActivity = (float) (act / n);
            const double rx = (sx + ox) / (n + 1), ry = (sy + oy) / (n + 1);
            v.coherence = (float) std::sqrt (rx * rx + ry * ry);
        }
       #else
        (void) activity; (void) myKelvin; (void) phase; (void) warmth;
       #endif
        return v;
    }

    //  I moved my own ambient: tell the site
    void proposeKelvin (float k)
    {
       #if defined(_WIN32)
        if (blk == nullptr) return;
        blk->siteKelvin.store (k);
        blk->climateOwner.store (id);
        lastClimateSeq = blk->climateSeq.fetch_add (1, std::memory_order_acq_rel) + 1;
       #endif
    }

    /*  The Kuramoto step, offered here so every artefact leans the same way:
        advance my phase at my natural rate and pull toward the others' mean
        with strength K (0..1 per second-ish). Returns the new phase. */
    static float stepPhase (float phase, float natHz, double dtSeconds,
                            const View& v, float K)
    {
        double ph = phase + natHz * dtSeconds;
        if (v.others > 0 && K > 0.0f)
        {
            const double d = std::sin (6.283185307 * ((double) v.meanPhase - ph));
            ph += (double) K * v.coherence * d * dtSeconds / 6.283185307 * 6.283185307 * 0.5;
        }
        ph -= std::floor (ph);
        return (float) ph;
    }

    //  the whole law in one line: cold and close couples, hot or far does not
    static float pullStrength (const View& v, float warmth)
    {
        if (! v.timingShared || v.others <= 0) return 0.0f;
        const float k = (1.0f - v.distance) * (1.0f - warmth);
        return k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
    }

    uint64_t instanceId() const { return id; }

    //  my natural rate: the site's breath at this finding's own multiplier
    float naturalHz() const { return cached.pulseHz * natRateMul(); }

private:
    Settings readSettings() const
    {
        Settings s;
        if (blk == nullptr) return s;
        s.climate  = blk->climateShared.load() != 0;
        s.timing   = blk->timingShared.load() != 0;
        s.distance = blk->distance.load();
        s.pulseHz  = blk->pulseHz.load();
        return s;
    }
    //  each finding breathes at a slightly different natural rate, so the
    //  lock is a real negotiation and, hot, they audibly drift apart
    float natRateMul() const
    {
        switch (kind) { case 1: return 1.000f; case 22: return 0.985f;
                        case 67: return 1.012f; case 104: return 0.970f; default: return 1.0f; }
    }
    void detach()
    {
       #if defined(_WIN32)
        if (blk != nullptr) { UnmapViewOfFile (blk); blk = nullptr; }
        if (map != nullptr) { CloseHandle (map); map = nullptr; }
       #endif
        slot = -1;
    }

    Block*   blk = nullptr;
   #if defined(_WIN32)
    HANDLE   map = nullptr;
   #endif
    int      slot = -1;
    uint64_t id = 0;
    uint32_t kind = 0;
    uint32_t lastSettingsSeq = 0, lastClimateSeq = 0;
    Settings cached;
};

} // namespace proxima
