// RITE OF PASSAGE — the BWFX modules, wrapped as slot effects. See
// rop_bwfx.h for why, and RITE-OF-PASSAGE-DESIGN.md §12a for the doctrine.

#include "rop_bwfx.h"

#include <bwfx.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace rop::bw
{
namespace
{

/*  WHICH MODULES, and the two things the wrapper has to know about each that
    a BWFX descriptor does not say.

    TAIL: whether the module has memory worth SPILLing. §4's three tail modes
    only make sense for something that can ring; a TUBE asked to SPILL has
    nothing to spill, and a distortion still grinding after the drop is the
    opposite of what SPILL is for. Those pass the audio through untouched
    from the arrival instead, which is BYPASS, which is the truth.

    ZERO: the parameter ids forced to their minimum as the SLOT's default.
    rop_rack.cpp:129 promises that assigning an effect never changes the
    sound until it is edited, and BWFX defaults are voiced for a rack you
    switch on deliberately (ECHO at 25 % wet, TUBE at 8 dB). Zeroing the one
    amount knob keeps that promise AND lands the right workflow: A is dry,
    and the thing you reach for first is B's amount. TUBE's tone tilt and
    STRIP's EQ are unity at their defaults, so ten of ten start inert.

    NOT HERE, and both for cause:

      SPACE (reverb) — CHARACTER and LENGTH only take effect on a SETTLED
      message-thread service tick (bwfx_modules.cpp:342), and the settle
      guard means they never rebuild while a value is moving. A reverb whose
      two interesting knobs cannot travel would be a lane that lies about
      what it is doing. It stays in the post-slots rack, which is where a
      colour you set once belongs (§1).

      KIERANATOR — its pattern is opaque extra state drawn by the step grid
      that only the BWFX panel has (descriptor custom = "steps"). In a slot
      it would run a default pattern with no way to edit it. */
struct Spec
{
    const char* bwfxId;
    bool        tail;
    const char* zero;     // '|' separated
};

const Spec kSpecs[] = {
    { "saturation", false, "drive"       },   // TUBE
    { "phaser",     false, "mix"         },   // SWEEP
    { "chorus",     false, "mix"         },   // ENSEMBLE
    { "trem",       false, "depth"       },   // HARMONIC
    { "stutter",    false, "amount"      },   // GATE
    { "lofi",       false, "crush|dirt"  },   // GRIT
    { "strip",      false, "amount"      },   // STRIP
    { "delay",      true,  "mix"         },   // ECHO
    { "shimmer",    true,  "mix"         },   // SHIMMER
    { "rotary",     false, "mix"         },   // ROTARY
};
constexpr int kNumSpecs = (int) (sizeof (kSpecs) / sizeof (kSpecs[0]));

bool listed (const char* list, const char* id)
{
    if (list == nullptr || id == nullptr) return false;
    const size_t n = std::strlen (id);
    for (const char* p = list; *p != '\0'; )
    {
        const char* e = std::strchr (p, '|');
        const size_t len = (e != nullptr) ? (size_t) (e - p) : std::strlen (p);
        if (len == n && std::strncmp (p, id, n) == 0) return true;
        if (e == nullptr) break;
        p = e + 1;
    }
    return false;
}

struct Gen
{
    std::string id;                            // "bwfx.delay"
    std::string sub;                           // "BWFX - stereo tape echo"
    int         bwfxType = -1;
    const Spec* spec = nullptr;
    std::array<ParamDesc, kMaxParams> params {};
    EffectDesc  desc {};
};

/*  Built once, from bwfx::moduleDescriptor(), never typed — the same rule
    BwfxPanel.cpp works by. A module that gains a knob gains it here on the
    next rebuild, with its own range, unit and choices. */
std::vector<Gen>& table()
{
    static std::vector<Gen> t = []
    {
        std::vector<Gen> v;
        v.reserve ((size_t) kNumSpecs);

        for (const auto& sp : kSpecs)
        {
            int type = -1;
            for (int i = 0; i < bwfx::numModuleTypes(); ++i)
                if (std::strcmp (bwfx::moduleDescriptor (i).id, sp.bwfxId) == 0) { type = i; break; }
            if (type < 0) continue;            // a BWFX that dropped a module

            const bwfx::Descriptor& bd = bwfx::moduleDescriptor (type);

            Gen g;
            g.bwfxType = type;
            g.spec = &sp;
            g.id  = std::string ("bwfx.") + bd.id;
            g.sub = std::string ("BWFX - ") + (bd.sub != nullptr ? bd.sub : "");

            /*  kMaxParams is 16 to match BWFX exactly, and this clamp is the
                belt: a module with more knobs than a slot can carry loses
                the extras SILENTLY otherwise, which is the one way this
                wrapper could ship a lie. */
            const int np = bd.numParams < kMaxParams ? bd.numParams : kMaxParams;

            for (int p = 0; p < np; ++p)
            {
                const bwfx::ParamDesc& s = bd.params[p];
                ParamDesc& d = g.params[(size_t) p];
                d.id      = s.id;              // BWFX literals: static storage
                d.name    = s.name;
                d.lo      = s.lo;
                d.hi      = s.hi;
                d.def     = listed (sp.zero, s.id) ? s.lo : s.def;
                d.step    = s.step;
                d.unit    = s.unit;
                d.warp    = Warp::Linear;      // BWFX knobs are linear in their own units
                d.choices = s.choices;
                d.levelKnob = true;            // declared Intentional; see below
            }

            g.desc.name = bd.name;             // TUBE, ECHO, GATE...
            g.desc.numParams = np;

            /*  §8, DECLARED HONESTLY, AND THIS IS THE ONE PLACE THE WRAPPER
                MAKES A CLAIM RATHER THAN FORWARDING ONE.

                Intentional + movesPitch is NOT an assertion that TUBE bends
                pitch. It is the statement that RITE OF PASSAGE'S level and
                pitch contracts DO NOT GOVERN these twelve knobs — BWFX's own
                bench does, against BWFX's own declarations. The alternative
                was to guess a class per module and have this plugin's bench
                police another plugin's DSP, which would be two benches
                disagreeing about one algorithm and no way to tell which was
                right. §8.3's table is for the eighteen effects this plugin
                owns; the World rack is a guest, and a guest is announced.

                What still applies, and is what check 11 holds them to: every
                one of them must stay bounded and finite at both ends of
                every knob. */
            g.desc.level = Level::Intentional;
            g.desc.movesPitch = true;
            g.desc.generator = false;
            //  BWFX modules carry one parameter set, so SPREAD's two sides
            //  read the same score. PLACE, TURN and the MONO GATE still
            //  reach them: those are applied by the rack, outside the effect.
            g.desc.perChannelParams = false;
            g.desc.reordersTime = false;
            g.desc.latency = 0;                // no BWFX module reports any

            v.push_back (std::move (g));
        }
        return v;
    }();

    //  pointers taken AFTER the vector is in its final home: a moved
    //  std::string that fitted in its own small buffer moves its bytes, so
    //  a c_str() taken before the move would dangle
    static const bool linked = []
    {
        for (auto& g : t)
        {
            g.desc.id     = g.id.c_str();
            g.desc.sub    = g.sub.c_str();
            g.desc.params = g.params.data();
        }
        return true;
    }();
    (void) linked;

    return t;
}

// ---------------------------------------------------------------------------
class Wrapped : public Effect
{
public:
    explicit Wrapped (const Gen& g)
        : gen (g), m (bwfx::createModule (g.bwfxType))
    {
        /*  the SLOT's defaults, not the rack's, and before prepare() — the
            module's smoothers init from whatever its parameters say, so
            leaving BWFX's own defaults in place would glide a 25 % wet down
            to zero over the first 50 ms of every fresh assignment */
        if (m != nullptr)
            for (int p = 0; p < gen.desc.numParams; ++p)
                m->setParam (p, gen.params[(size_t) p].def);
    }

    const EffectDesc& desc() const override { return gen.desc; }

    void prepare (double fs, int maxBlock) override
    {
        if (m == nullptr) return;
        m->prepare (fs, maxBlock < bwfx::kSubBlock ? bwfx::kSubBlock : maxBlock);
        /*  TWICE, deliberately: a module that guards its build on a settled
            tick records on the first call and builds on the second, so one
            call would leave it un-built until the host's 15 Hz timer came
            round. Nothing in the set needs it today; the next one might. */
        m->service();
        m->service();
    }

    void reset() override
    {
        if (m != nullptr) m->reset();
        spilling = false; passThrough = false; needsClear = false;
    }

    void process (float* L, float* R, int n,
                  const float* pL, const float* pR, const Ctx& c) override
    {
        (void) pR;                       // one parameter set; perChannelParams is false
        if (m == nullptr || passThrough) return;

        for (int p = 0; p < gen.desc.numParams; ++p)
            m->setParam (p, pL[p]);      // relaxed atomic stores, audio-thread safe

        m->setTempo (c.bpm);
        m->setClock (c.ppq, c.playing);
        //  SPILL: stop FEEDING it, let it ring. BWFX has the hook already,
        //  and the modules smooth it, so the drop is not a splice.
        m->inputDuck (spilling ? 0.0f : 1.0f);
        m->process (L, R, n);
    }

    void arm() override
    {
        spilling = false;
        passThrough = false;
        //  §9: a rite scrubbed backwards must not sound like last night's
        //  build, so whatever was held is dropped on the way back in
        if (needsClear && m != nullptr) m->reset();
        needsClear = false;
    }

    void release (Tail t) override
    {
        if (t == Tail::Spill && gen.spec->tail) { spilling = true; needsClear = true; return; }

        spilling = false;
        //  nothing to spill: from here it is out of the way, which is BYPASS
        passThrough = (t == Tail::Spill);

        if (t == Tail::Clear) { if (m != nullptr) m->reset(); needsClear = false; }
        else                  { needsClear = gen.spec->tail; }
    }

    bool ringing() const override { return spilling; }

    void service() override { if (m != nullptr) m->service(); }

private:
    const Gen& gen;
    std::unique_ptr<bwfx::Module> m;
    bool spilling = false, passThrough = false, needsClear = false;
};

} // namespace

// ---------------------------------------------------------------------------
int numWrapped() { return (int) table().size(); }

const EffectDesc& wrappedDescriptor (int i)
{
    auto& t = table();
    const int n = (int) t.size();
    if (n == 0)
    {
        //  a BWFX with none of these modules left. Nothing can reach this
        //  through the registry (numWrapped() is 0 there), so it only has to
        //  be SAFE, not useful.
        static const EffectDesc none { "bwfx.none", "BWFX", "no module", nullptr, 0,
                                       Level::Intentional, true, false, false, false, 0 };
        return none;
    }
    return t[(size_t) (i < 0 ? 0 : (i >= n ? n - 1 : i))].desc;
}

Effect* createWrapped (int i)
{
    auto& t = table();
    if (i < 0 || i >= (int) t.size()) return nullptr;
    return new Wrapped (t[(size_t) i]);
}

} // namespace rop::bw
