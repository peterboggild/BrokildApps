#pragma once

/*  DRUMEQ - THE KIT. Every DrumEQ instance in the session shares one small
    named block of memory ("Local\\BrokildDrumEQKit2", a Windows file mapping in
    the login session - the only sanctioned way for plug-in instances to see
    each other, and it works across processes too, so a bridged plug-in or a
    standalone joins the same kit). The mechanism is the Proxima Centauri B
    site's (BrokildWorldFX/adapter/proxima_site.h), cut down to what a kit is.

    WHAT IS IN IT
      * one slot per INSTANCE: who it is, which drum it is on, a heartbeat
        (a slot whose heartbeat is older than 2.5 s is a ghost, so a crashed
        or removed instance simply drops out);
      * one slot per DRUM: the current settings of whichever instance last
        changed that drum, and its preset name. This is what SAVE KIT writes.
      * MASTER (v2): one instance may claim the role; its panel edits any
        drum's slot with an EDIT write (editSeq), which every linked track on
        that drum takes. The track the master is aimed at shows it what it
        hears (the LIVE VIEW: log-binned spectra + meters), and answers a
        remote FIND. The block name carries the version, so a v1 instance
        still loaded in a session keeps its own block instead of misreading.
      * a KIT GENERATION: LOAD KIT writes every drum it carries into the drum
        slots, stamps each with the new generation, then bumps it. Every
        linked instance sees the bump on its next tick and takes the slot for
        ITS drum if (and only if) the load carried that drum.

    THREADS. Everything here runs on the message thread (a processor-side
    timer), never on the audio thread. A drum slot is a seqlock with a CAS on
    the sequence, so two instances on the same drum (two overhead mics on two
    tracks) cannot tear it.

    Header only, compiled into the plug-in (no runtime DLLs - the house rule).
    Elsewhere than Windows the kit is inert and every call is harmless. */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

namespace drumkit
{
constexpr uint32_t kMagic   = 0x5145444Bu;   // 'KDEQ'
constexpr uint32_t kVersion = 2;            // 2: MASTER, remote edits, the live view, remote FIND
constexpr int      kInst    = 64;
constexpr int      kSrc     = 11;
constexpr int      kVals    = 48;
constexpr int      kNameW   = 12;            // 48 characters, packed in words
constexpr uint64_t kStaleMs = 2500;
constexpr int      kSpec    = 192;           // the live view's spectrum: log bins, 20 Hz - 20 kHz

struct Inst
{
    std::atomic<uint64_t> id;
    std::atomic<uint64_t> beatMs;
    std::atomic<int32_t>  source;
    std::atomic<uint32_t> pad_;
};

struct Src
{
    std::atomic<uint32_t> seq;        // odd while being written
    std::atomic<uint32_t> kitGen;     // the kit load that last wrote it (0 = an instance did)
    std::atomic<uint64_t> writer;
    std::atomic<uint64_t> stampMs;    // 0 = never written
    std::atomic<uint32_t> count;      // values in use
    std::atomic<float>    v[kVals];
    std::atomic<uint32_t> name[kNameW];
    std::atomic<uint32_t> editSeq;    // bumped by every MASTER edit: "every linked track on this drum, take this"

    //  THE LIVE VIEW: a track on this drum shows the master what it hears
    //  (written only while the master is aimed at this drum; a torn spectrum
    //  is harmless, so no seqlock)
    std::atomic<uint64_t> viewMs;     // 0 = nobody has shown it
    std::atomic<uint64_t> viewWriter; // two tracks on one drum (two overheads): the louder one shows
    std::atomic<float>    viewRate;
    std::atomic<float>    pre[kSpec], post[kSpec];
    std::atomic<float>    meter[4];   // in, out (linear peaks), gain reduction, gate (dB)

    //  REMOTE FIND: the master asks, a track on this drum listens and answers
    std::atomic<uint32_t> findReq, findAck;
    std::atomic<int32_t>  findState;  // 0 idle, 1 listening, 2 done, 3 nothing heard
    std::atomic<float>    findSec, findHz, findInDelta;
    std::atomic<int32_t>  findHits;
    std::atomic<int32_t>  findKeep;   // the master asked to keep the track's output level
};

struct Block
{
    std::atomic<uint32_t> magic, version, size, kitGen;
    std::atomic<uint32_t> kitName[kNameW * 2];
    std::atomic<uint64_t> masterId;      // the instance whose panel edits the kit (0 = none)
    std::atomic<int32_t>  masterTarget;  // the drum it is editing (-1 = its own)
    std::atomic<int32_t>  pad_;
    Inst inst[kInst];
    Src  src[kSrc];
};

static_assert (std::atomic<uint64_t>::is_always_lock_free, "the kit needs lock-free atomics");

inline uint64_t nowMs()
{
   #if defined(_WIN32)
    return (uint64_t) GetTickCount64();
   #else
    return 0;
   #endif
}

inline void packName (std::atomic<uint32_t>* dst, int words, const std::string& s)
{
    char buf[kNameW * 2 * 4 + 1] = {};
    std::strncpy (buf, s.c_str(), (size_t) words * 4 - 1);
    for (int i = 0; i < words; ++i)
    {
        uint32_t w; std::memcpy (&w, buf + i * 4, 4);
        dst[i].store (w, std::memory_order_relaxed);
    }
}

inline std::string unpackName (const std::atomic<uint32_t>* src, int words)
{
    char buf[kNameW * 2 * 4 + 1] = {};
    for (int i = 0; i < words; ++i) { const uint32_t w = src[i].load (std::memory_order_relaxed); std::memcpy (buf + i * 4, &w, 4); }
    buf[words * 4 - 1] = 0;
    return std::string (buf);
}

//==============================================================================
class Link
{
public:
    Link() = default;
    ~Link() { close(); }
    Link (const Link&) = delete;
    Link& operator= (const Link&) = delete;

    bool open()
    {
       #if defined(_WIN32)
        map = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) sizeof (Block),
                                  L"Local\\BrokildDrumEQKit2");   // 2: a v1 instance still loaded keeps its own block
        if (map == nullptr) return false;
        const bool creator = GetLastError() != ERROR_ALREADY_EXISTS;
        blk = (Block*) MapViewOfFile (map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (Block));
        if (blk == nullptr) { CloseHandle (map); map = nullptr; return false; }
        if (creator)
        {
            //  a fresh mapping is zero-filled; the magic goes LAST with release
            //  semantics, so a joiner never sees a half-written block
            blk->version.store (kVersion);
            blk->size.store ((uint32_t) sizeof (Block));
            blk->kitGen.store (0);
            blk->masterTarget.store (-1);
            blk->magic.store (kMagic, std::memory_order_release);
        }
        else
        {
            for (int i = 0; i < 200 && blk->magic.load (std::memory_order_acquire) != kMagic; ++i) Sleep (1);
            if (blk->magic.load (std::memory_order_acquire) != kMagic || blk->version.load() != kVersion
                || blk->size.load() != (uint32_t) sizeof (Block))
            { detach(); return false; }
        }
        id = ((uint64_t) GetCurrentProcessId() << 40) ^ ((uint64_t) GetTickCount64() << 8) ^ (uint64_t) (uintptr_t) this;
        if (id == 0) id = 1;
        seenGen = blk->kitGen.load();          // a new instance does not replay an old load
        return true;
       #else
        return false;
       #endif
    }

    void close()
    {
        if (blk != nullptr && slot >= 0) { blk->inst[slot].beatMs.store (0); blk->inst[slot].id.store (0); }
        slot = -1;
        detach();
    }

    bool isOpen() const { return blk != nullptr; }
    uint64_t myId() const { return id; }

    //  ---- presence ---------------------------------------------------------
    //  call every tick: keeps this instance alive and says which drum it is on
    void beat (int source)
    {
        if (blk == nullptr) return;
        const uint64_t now = nowMs();
        if (slot < 0)
            for (int i = 0; i < kInst && slot < 0; ++i)
            {
                uint64_t cur = blk->inst[i].id.load();
                const uint64_t b = blk->inst[i].beatMs.load();
                if (cur != 0 && now - b < kStaleMs) continue;
                if (blk->inst[i].id.compare_exchange_strong (cur, id)) slot = i;
            }
        if (slot < 0) return;
        blk->inst[slot].source.store (source);
        blk->inst[slot].beatMs.store (now);
    }

    //  live instances per drum (this one included)
    void presence (int counts[kSrc]) const
    {
        for (int s = 0; s < kSrc; ++s) counts[s] = 0;
        if (blk == nullptr) return;
        const uint64_t now = nowMs();
        for (int i = 0; i < kInst; ++i)
        {
            if (blk->inst[i].id.load() == 0 || now - blk->inst[i].beatMs.load() >= kStaleMs) continue;
            const int s = blk->inst[i].source.load();
            if (s >= 0 && s < kSrc) ++counts[s];
        }
    }

    //  ---- a drum's settings --------------------------------------------------
    void publish (int s, const float* vals, int n, const std::string& name) { write (s, vals, n, name, 0); }

    //  MASTER: write a drum for every linked track on it to take
    void edit (int s, const float* vals, int n, const std::string& name) { write (s, vals, n, name, 0, true); }

    //  what a reader learns about who wrote the slot, and when
    struct Meta { uint32_t seq = 0, gen = 0, editSeq = 0; uint64_t writer = 0; };

    //  false if the drum was never written
    bool read (int s, float* vals, int& n, std::string& name, uint32_t* gen = nullptr, Meta* meta = nullptr) const
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return false;
        const Src& d = blk->src[s];
        for (int tries = 0; tries < 200; ++tries)
        {
            const uint32_t a = d.seq.load (std::memory_order_acquire);
            if (a & 1u) continue;
            if (d.stampMs.load() == 0) return false;
            n = (int) d.count.load();
            if (n > kVals) n = kVals;
            for (int i = 0; i < n; ++i) vals[i] = d.v[i].load (std::memory_order_relaxed);
            name = unpackName (d.name, kNameW);
            const uint32_t g = d.kitGen.load();
            const uint32_t es = d.editSeq.load();
            const uint64_t wr = d.writer.load();
            std::atomic_thread_fence (std::memory_order_acquire);
            if (d.seq.load (std::memory_order_relaxed) == a)
            {
                if (gen) *gen = g;
                if (meta) { meta->seq = a; meta->gen = g; meta->editSeq = es; meta->writer = wr; }
                return true;
            }
        }
        return false;
    }

    //  ---- kits -----------------------------------------------------------
    //  LOAD KIT, step 1: write every drum the kit carries (gen = the new one)
    uint32_t beginKit() const { return blk == nullptr ? 0 : blk->kitGen.load() + 1; }
    void writeKitDrum (uint32_t gen, int s, const float* vals, int n, const std::string& name) { write (s, vals, n, name, gen); }
    //  step 2: publish the load to everyone
    void commitKit (uint32_t gen, const std::string& kitName)
    {
        if (blk == nullptr) return;
        packName (blk->kitName, kNameW * 2, kitName);
        blk->kitGen.store (gen, std::memory_order_release);
    }

    uint32_t kitGen() const { return blk == nullptr ? 0 : blk->kitGen.load (std::memory_order_acquire); }
    std::string kitName() const { return blk == nullptr ? std::string() : unpackName (blk->kitName, kNameW * 2); }

    //  a kit load this instance has not answered yet? (true once per load)
    bool newKit (uint32_t& gen)
    {
        const uint32_t g = kitGen();
        if (g == seenGen) return false;
        seenGen = g; gen = g;
        return true;
    }

    uint32_t editSeqOf (int s) const { return blk == nullptr || s < 0 || s >= kSrc ? 0 : blk->src[s].editSeq.load(); }

    //  ---- MASTER -------------------------------------------------------------
    //  one master per session: claiming takes it from whoever had it
    void claimMaster()   { if (blk != nullptr) { blk->masterTarget.store (-1); blk->masterId.store (id); } }
    void releaseMaster() { if (blk == nullptr) return; uint64_t m = id; if (blk->masterId.compare_exchange_strong (m, 0)) blk->masterTarget.store (-1); }
    bool isMaster() const { return blk != nullptr && blk->masterId.load() == id; }
    void setMasterTarget (int s) { if (isMaster()) blk->masterTarget.store (s); }

    //  the live master, the drum it edits (-1 = its own) and its own drum
    bool masterAlive (int* target = nullptr, int* masterSource = nullptr) const
    {
        if (blk == nullptr) return false;
        const uint64_t m = blk->masterId.load();
        if (m == 0) return false;
        const uint64_t now = nowMs();
        for (int i = 0; i < kInst; ++i)
            if (blk->inst[i].id.load() == m && now - blk->inst[i].beatMs.load() < kStaleMs)
            {
                if (target) *target = blk->masterTarget.load();
                if (masterSource) *masterSource = blk->inst[i].source.load();
                return true;
            }
        return false;
    }

    //  ---- the live view ------------------------------------------------------
    void showView (int s, const float* pre, const float* post, const float meters[4], float rate)
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return;
        Src& d = blk->src[s];
        //  another track on this drum is showing, recently, and is louder: let it
        const uint64_t now = nowMs();
        if (d.viewWriter.load() != id && now - d.viewMs.load() < 400 && meters[0] < d.meter[0].load (std::memory_order_relaxed)) return;
        d.viewWriter.store (id);
        for (int i = 0; i < kSpec; ++i) { d.pre[i].store (pre[i], std::memory_order_relaxed); d.post[i].store (post[i], std::memory_order_relaxed); }
        for (int i = 0; i < 4; ++i) d.meter[i].store (meters[i], std::memory_order_relaxed);
        d.viewRate.store (rate, std::memory_order_relaxed);
        d.viewMs.store (now, std::memory_order_release);
    }
    //  false when no track on that drum has shown it in the last second
    bool readView (int s, float* pre, float* post, float meters[4], float& rate) const
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return false;
        const Src& d = blk->src[s];
        const uint64_t t = d.viewMs.load (std::memory_order_acquire);
        if (t == 0 || nowMs() - t > 1000) return false;
        for (int i = 0; i < kSpec; ++i) { pre[i] = d.pre[i].load (std::memory_order_relaxed); post[i] = d.post[i].load (std::memory_order_relaxed); }
        for (int i = 0; i < 4; ++i) meters[i] = d.meter[i].load (std::memory_order_relaxed);
        rate = d.viewRate.load (std::memory_order_relaxed);
        return true;
    }

    //  ---- remote FIND --------------------------------------------------------
    void requestFind (int s, bool keepLevel)
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return;
        blk->src[s].findKeep.store (keepLevel ? 1 : 0);
        blk->src[s].findSec.store (0);
        blk->src[s].findState.store (1);
        blk->src[s].findReq.fetch_add (1);
    }
    //  a request this track has not taken yet? (takes it)
    bool findKeepLevel (int s) const { return blk != nullptr && s >= 0 && s < kSrc && blk->src[s].findKeep.load() != 0; }
    bool takeFind (int s)
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return false;
        Src& d = blk->src[s];
        const uint32_t q = d.findReq.load();
        if (q == d.findAck.load()) return false;
        d.findAck.store (q);
        return true;
    }
    void findProgress (int s, float sec) { if (blk != nullptr && s >= 0 && s < kSrc) blk->src[s].findSec.store (sec); }
    void findAnswer (int s, bool heard, float hz, int hits, float inDelta)
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return;
        Src& d = blk->src[s];
        d.findHz.store (hz); d.findHits.store (hits); d.findInDelta.store (inDelta);
        d.findState.store (heard ? 2 : 3);
    }
    int findState (int s, float& sec, float& hz, int& hits, float& inDelta) const
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return 0;
        const Src& d = blk->src[s];
        sec = d.findSec.load(); hz = d.findHz.load(); hits = d.findHits.load(); inDelta = d.findInDelta.load();
        return d.findState.load();
    }

private:
    void write (int s, const float* vals, int n, const std::string& name, uint32_t gen, bool edit = false)
    {
        if (blk == nullptr || s < 0 || s >= kSrc) return;
        Src& d = blk->src[s];
        //  take the slot: CAS the sequence from even to odd
        uint32_t a = 0;
        for (int tries = 0;; ++tries)
        {
            a = d.seq.load();
            if ((a & 1u) == 0 && d.seq.compare_exchange_weak (a, a + 1u, std::memory_order_acquire)) break;
           #if defined(_WIN32)
            if (tries > 1000) Sleep (0);
           #endif
            if (tries > 100000) return;      // a writer died mid-write; give up this tick
        }
        if (n > kVals) n = kVals;
        for (int i = 0; i < n; ++i) d.v[i].store (vals[i], std::memory_order_relaxed);
        d.count.store ((uint32_t) n, std::memory_order_relaxed);
        packName (d.name, kNameW, name);
        d.kitGen.store (gen, std::memory_order_relaxed);
        d.writer.store (id, std::memory_order_relaxed);
        d.stampMs.store (nowMs() | 1u, std::memory_order_relaxed);
        if (edit) d.editSeq.fetch_add (1u, std::memory_order_relaxed);
        d.seq.store (a + 2u, std::memory_order_release);
    }

    void detach()
    {
       #if defined(_WIN32)
        if (blk != nullptr) UnmapViewOfFile (blk);
        if (map != nullptr) CloseHandle (map);
        map = nullptr;
       #endif
        blk = nullptr;
    }

   #if defined(_WIN32)
    HANDLE map = nullptr;
   #endif
    Block* blk = nullptr;
    uint64_t id = 0;
    int slot = -1;
    uint32_t seenGen = 0;
};

} // namespace drumkit
