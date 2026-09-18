#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <brokild_paths.h>

using namespace rop;

namespace
{
    juce::String pctText (float v, int) { return juce::String ((int) std::lround (v)) + " %"; }
    juce::String dbText  (float v, int) { return juce::String (v, 1) + " dB"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout RiteProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;

    l.add (std::make_unique<F> (juce::ParameterID { rop_ids::position, 1 }, "POSITION",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f,
        A().withStringFromValueFunction (pctText)));

    /*  ARRIVAL is a TRIGGER, not the end of the slider. The slider reaching
        1.0 must not fire a drop, or scrubbing while you edit fires one. */
    l.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { rop_ids::arrival, 1 }, "ARRIVAL", false));

    l.add (std::make_unique<F> (juce::ParameterID { rop_ids::mix, 1 }, "MIX",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f,
        A().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<F> (juce::ParameterID { rop_ids::output, 1 }, "OUTPUT",
        juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
        A().withStringFromValueFunction (dbText)));
    l.add (std::make_unique<F> (juce::ParameterID { rop_ids::spread, 1 }, "SPREAD",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f,
        A().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<F> (juce::ParameterID { rop_ids::turn, 1 }, "TURN",
        juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
        A().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<F> (juce::ParameterID { rop_ids::monogate, 1 }, "MONO GATE",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f,
        A().withStringFromValueFunction (pctText)));

    bwfx_juce::addMacroParameters (l);
    return l;
}

// ---------------------------------------------------------------------------
RiteProcessor::RiteProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RITE", layout())
{
    pPos     = apvts.getRawParameterValue (rop_ids::position);
    pArrival = apvts.getRawParameterValue (rop_ids::arrival);
    pMix     = apvts.getRawParameterValue (rop_ids::mix);
    pOut     = apvts.getRawParameterValue (rop_ids::output);
    pSpread  = apvts.getRawParameterValue (rop_ids::spread);
    pTurn    = apvts.getRawParameterValue (rop_ids::turn);
    pGate    = apvts.getRawParameterValue (rop_ids::monogate);

    startTimerHz (15);      // BWFX service, with the editor closed too
}

RiteProcessor::~RiteProcessor() { stopTimer(); }

void RiteProcessor::timerCallback()
{
    worldFx.service();
    engine.service();
}

bool RiteProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void RiteProcessor::prepareToPlay (double sampleRate, int block)
{
    engine.prepare (sampleRate, block);
    worldFx.prepare (sampleRate, block);
    setLatencySamples (0);      // every v1 effect is zero latency (§9)
}

// ---------------------------------------------------------------------------
void RiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0) return;

    double bpm = 0.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm().orFallback (0.0);
            ppq = pos->getPpqPosition().orFallback (-1.0);
            playing = pos->getIsPlaying();
        }

    engine.setTransport (bpm, ppq, playing);
    engine.setPosition (pPos->load() * 0.01f);
    engine.mix         = pMix->load() * 0.01f;
    engine.outputGain  = juce::Decibels::decibelsToGain (pOut->load(), -24.0f);
    engine.spread      = pSpread->load() * 0.01f;
    engine.turn        = pTurn->load() * 0.01f;
    engine.monoGate    = pGate->load() * 0.01f;

    //  ARRIVAL arms on the rising edge of its own parameter and the engine
    //  fires it at the next grid boundary, to the sample
    const bool arr = pArrival->load() > 0.5f;
    if (arr && ! lastArrival) engine.armArrival();
    lastArrival = arr;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (numOut > 1 ? 1 : 0);

    if (numOut > 1)
    {
        engine.process (L, R, n);
    }
    else
    {
        //  mono: the engine is stereo throughout, so give it a real right
        //  channel rather than the same pointer twice
        juce::AudioBuffer<float> tmp (1, n);
        tmp.copyFrom (0, 0, L, n);
        engine.process (L, tmp.getWritePointer (0), n);
    }

    worldFx.setTransport (bpm, ppq, playing);
    bwfx_juce::pushMacros (worldFx, apvts);
    if (numOut > 1) worldFx.process (L, R, n);
}

// ---------------------------------------------------------------------------
// THE RITE — six assignments, twelve settings, the score, the arrival, as one
// string. Keyed by SLOT and effect id, and an unknown effect id leaves its
// slot empty rather than failing the load (§2).
juce::String RiteProcessor::riteToJson() const
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> slots;

    for (int i = 0; i < kSlots; ++i)
    {
        auto* o = new juce::DynamicObject();
        const int type = engine.slotEffect (i);
        const auto& s = engine.state (i);
        o->setProperty ("fx", type >= 0 ? juce::String (effectDescriptor (type).id) : juce::String());
        o->setProperty ("on", s.on);
        o->setProperty ("enter", s.enter);
        o->setProperty ("exit", s.exit);
        o->setProperty ("depth", s.depth);
        o->setProperty ("curve", s.curve);
        o->setProperty ("place", (int) s.place);
        o->setProperty ("tail", (int) s.tail);
        o->setProperty ("q", s.quantise);
        if (type >= 0)
        {
            const auto& d = effectDescriptor (type);
            juce::Array<juce::var> a, b, pc;
            for (int p = 0; p < d.numParams; ++p)
            {
                a.add (s.A[p]); b.add (s.B[p]); pc.add ((int) s.paramCurve[p]);
            }
            o->setProperty ("A", a); o->setProperty ("B", b); o->setProperty ("pc", pc);
        }
        slots.add (juce::var (o));
    }
    root->setProperty ("slots", slots);

    auto* ar = new juce::DynamicObject();
    ar->setProperty ("dry", engine.arrival.restoreDry);
    ar->setProperty ("imp", engine.arrival.fireImpact);
    ar->setProperty ("tune", engine.arrival.impactTune);
    ar->setProperty ("dec", engine.arrival.impactDecay);
    ar->setProperty ("lvl", engine.arrival.impactLevel);
    ar->setProperty ("grid", engine.arrival.grid);
    root->setProperty ("arrival", juce::var (ar));
    root->setProperty ("gateSpan", engine.monoGateSpan);
    root->setProperty ("bass", engine.bassMonoHz);

    return juce::JSON::toString (juce::var (root), true);
}

void RiteProcessor::riteFromJson (const juce::String& js)
{
    const juce::var root = juce::JSON::parse (js);
    if (! root.isObject()) return;

    if (auto* slots = root.getProperty ("slots", juce::var()).getArray())
    {
        for (int i = 0; i < juce::jmin (kSlots, slots->size()); ++i)
        {
            const juce::var& v = slots->getReference (i);
            const juce::String fx = v.getProperty ("fx", juce::var ("")).toString();
            const int type = fx.isEmpty() ? -1 : effectTypeByName (fx.toRawUTF8());
            engine.setSlotEffect (i, type);      // unknown id -> empty slot

            auto& s = engine.state (i);
            s.on    = (bool) v.getProperty ("on", true);
            s.enter = (float) (double) v.getProperty ("enter", 0.0);
            s.exit  = (float) (double) v.getProperty ("exit", 1.0);
            s.depth = (float) (double) v.getProperty ("depth", 1.0);
            s.curve = (int) v.getProperty ("curve", 0);
            s.place = (Place) (int) v.getProperty ("place", 0);
            s.tail  = (Tail)  (int) v.getProperty ("tail", 0);
            s.quantise = (int) v.getProperty ("q", 0);

            if (type >= 0)
            {
                const auto& d = effectDescriptor (type);
                auto* a = v.getProperty ("A", juce::var()).getArray();
                auto* b = v.getProperty ("B", juce::var()).getArray();
                auto* pc = v.getProperty ("pc", juce::var()).getArray();
                for (int p = 0; p < d.numParams; ++p)
                {
                    if (a && p < a->size()) s.A[p] = (float) (double) a->getReference (p);
                    if (b && p < b->size()) s.B[p] = (float) (double) b->getReference (p);
                    if (pc && p < pc->size()) s.paramCurve[p] = (int8_t) (int) pc->getReference (p);
                }
            }
        }
    }

    const juce::var ar = root.getProperty ("arrival", juce::var());
    if (ar.isObject())
    {
        engine.arrival.restoreDry  = (bool) ar.getProperty ("dry", true);
        engine.arrival.fireImpact  = (bool) ar.getProperty ("imp", true);
        engine.arrival.impactTune  = (float) (double) ar.getProperty ("tune", 48.0);
        engine.arrival.impactDecay = (float) (double) ar.getProperty ("dec", 700.0);
        engine.arrival.impactLevel = (float) (double) ar.getProperty ("lvl", -6.0);
        engine.arrival.grid        = (int) ar.getProperty ("grid", 0);
    }
    engine.monoGateSpan = (float) (double) root.getProperty ("gateSpan", 0.15);
    engine.bassMonoHz   = (float) (double) root.getProperty ("bass", 120.0);
}

// ---------------------------------------------------------------------------
// THE SIX QUICK PRESETS.
//
// The factory six are described as a TABLE rather than as JSON literals, and
// the JSON is generated from the descriptors: a table can only name a
// parameter that exists, because the id is looked up in the effect's own
// ParamDesc list and a miss is skipped rather than silently landing in the
// wrong slot. Hand-written JSON has no such guard.
namespace
{
    struct PV { const char* id; float a, b; };          // one parameter's journey
    struct FSlot
    {
        const char* fx;                                  // nullptr = empty slot
        float enter, exit, depth;
        int   curve;                                     // rop::Curve
        int   tail;                                      // 0 stop, 1 spill, 2 clear
        PV    pv[6];
    };
    struct FRite
    {
        const char* name;
        float mix, output, spread, turn, monogate;
        FSlot slot[rop::kSlots];
    };

    const FRite kFactory[6] = {
        //  1 — the classic: everything closes, then a hole, then the drop
        { "DISSOLVE", 100.0f, 0.0f, 25.0f, 0.0f, 60.0f, {
            { "climb", 0.00f, 1.00f, 1.0f, rop::CurveS,     0, { {"cutoff", 18000.0f, 320.0f}, {"reso", 15.0f, 62.0f} } },
            { "grain", 0.45f, 1.00f, 0.9f, rop::CurveAccel, 2, { {"size", 120.0f, 22.0f}, {"density", 12.0f, 60.0f}, {"mix", 0.0f, 90.0f} } },
            { "gap",   0.92f, 1.00f, 1.0f, rop::CurveLinear,2, { {"depth", 0.0f, 100.0f} } },
            { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} } } },

        //  2 — it does not close, it catches fire
        { "IGNITE", 100.0f, 0.0f, 35.0f, 0.0f, 45.0f, {
            { "riser", 0.20f, 1.00f, 1.0f, rop::CurveAccel, 0, { {"level", -60.0f, -7.0f}, {"freq", 400.0f, 9000.0f} } },
            { "chop",  0.45f, 0.96f, 1.0f, rop::CurveAccel, 0, { {"div", 0.0f, 5.0f}, {"depth", 40.0f, 100.0f} } },
            { "climb", 0.30f, 1.00f, 1.0f, rop::CurveLinear,0, { {"cutoff", 900.0f, 12000.0f}, {"reso", 20.0f, 70.0f} } },
            { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} } } },

        //  3 — the air is pulled out of the room
        { "VACUUM", 100.0f, 0.0f, 20.0f, 0.0f, 85.0f, {
            { "freeze", 0.35f, 0.90f, 1.0f, rop::CurveDecel,  2, { {"hold", 0.0f, 100.0f}, {"blur", 30.0f, 85.0f} } },
            { "climb",  0.10f, 1.00f, 1.0f, rop::CurveDecel,  0, { {"mode", 2.0f, 2.0f}, {"cutoff", 30.0f, 2600.0f} } },
            { "gap",    0.88f, 1.00f, 1.0f, rop::CurveLinear, 2, { {"depth", 0.0f, 100.0f}, {"edge", 20.0f, 3.0f} } },
            { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} } } },

        //  4 — the room comes loose from its moorings
        { "SEASICK", 100.0f, 0.0f, 55.0f, 0.0f, 30.0f, {
            { "swirl", 0.10f, 1.00f, 1.0f, rop::CurveS,      1, { {"mix", 15.0f, 80.0f}, {"warp", 20.0f, 100.0f}, {"rate", 0.2f, 2.4f} } },
            { "swarm", 0.30f, 1.00f, 1.0f, rop::CurveLinear, 0, { {"voices", 2.0f, 8.0f}, {"detune", 4.0f, 42.0f}, {"mix", 30.0f, 90.0f} } },
            { "orbit", 0.00f, 1.00f, 1.0f, rop::CurveLinear, 0, { {"angle", 0.0f, 360.0f}, {"rear", 80.0f, 80.0f} } },
            { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} } } },

        //  5 — it is taken apart with tools
        { "DEMOLITION", 100.0f, 0.0f, 15.0f, 0.0f, 50.0f, {
            { "mangle", 0.20f, 1.00f, 1.0f, rop::CurveAccel, 0, { {"engine", 0.0f, 3.0f}, {"drive", 20.0f, 95.0f}, {"mix", 45.0f, 100.0f} } },
            { "dust",   0.40f, 1.00f, 1.0f, rop::CurveAccel, 0, { {"bits", 16.0f, 4.0f}, {"rate", 48000.0f, 2200.0f} } },
            { "brake",  0.90f, 1.00f, 1.0f, rop::CurveDecel, 2, { {"speed", 100.0f, 0.0f} } },
            { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} } } },

        //  6 — the track stops being music and becomes a machine
        { "THE MACHINE", 100.0f, 0.0f, 25.0f, 0.0f, 70.0f, {
            { "chant",   0.25f, 1.00f, 1.0f, rop::CurveS,      0, { {"pitch", -12.0f, 7.0f}, {"mix", 40.0f, 100.0f}, {"bands", 1.0f, 2.0f} } },
            { "stutter", 0.55f, 0.95f, 1.0f, rop::CurveAccel, 0, { {"div", 0.0f, 5.0f}, {"depth", 50.0f, 100.0f} } },
            { "reverse", 0.70f, 1.00f, 1.0f, rop::CurveLinear,2, { {"depth", 0.0f, 100.0f} } },
            { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} }, { nullptr, 0, 1, 1, 0, 0, {} } } },
    };

    //  the rite JSON for one factory entry, built from the DESCRIPTORS so a
    //  parameter that does not exist cannot be written into the wrong index
    juce::String factoryRiteJson (const FRite& f)
    {
        auto* rootObj = new juce::DynamicObject();
        juce::Array<juce::var> slots;
        for (int i = 0; i < rop::kSlots; ++i)
        {
            const FSlot& fs = f.slot[i];
            auto* o = new juce::DynamicObject();
            const int type = (fs.fx != nullptr) ? rop::effectTypeByName (fs.fx) : -1;
            o->setProperty ("fx", type >= 0 ? juce::String (fs.fx) : juce::String());
            o->setProperty ("on", true);
            o->setProperty ("enter", fs.enter);
            o->setProperty ("exit", fs.exit);
            o->setProperty ("depth", fs.depth);
            o->setProperty ("curve", fs.curve);
            o->setProperty ("place", 0);
            o->setProperty ("tail", fs.tail);
            o->setProperty ("q", 0);
            if (type >= 0)
            {
                const auto& d = rop::effectDescriptor (type);
                juce::Array<juce::var> A, B;
                for (int p = 0; p < d.numParams; ++p) { A.add (d.params[p].def); B.add (d.params[p].def); }
                for (const auto& pv : fs.pv)
                {
                    if (pv.id == nullptr) continue;
                    for (int p = 0; p < d.numParams; ++p)
                        if (juce::String (d.params[p].id) == pv.id) { A.set (p, pv.a); B.set (p, pv.b); break; }
                }
                o->setProperty ("A", A);
                o->setProperty ("B", B);
            }
            slots.add (juce::var (o));
        }
        rootObj->setProperty ("slots", slots);
        return juce::JSON::toString (juce::var (rootObj));
    }
}

juce::File RiteProcessor::quickPresetFile (int i) const
{
    const auto dir = brokild::patchFolder ("Rite of Passage", { "rite-of-passage" });
    if (dir == juce::File()) return {};
    return dir.getChildFile ("Quick " + juce::String (i + 1) + ".json");
}

juce::String RiteProcessor::quickPresetName (int i) const
{
    const auto f = quickPresetFile (i);
    if (f == juce::File() || ! f.existsAsFile()) return "EMPTY";
    const auto v = juce::JSON::parse (f.loadFileAsString());
    const auto n = v.getProperty ("name", juce::var()).toString();
    return n.isNotEmpty() ? n : ("QUICK " + juce::String (i + 1));
}

void RiteProcessor::storeQuickPreset (int i, const juce::String& name)
{
    const auto f = quickPresetFile (i);
    if (f == juce::File()) return;

    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name.isNotEmpty() ? name : ("QUICK " + juce::String (i + 1)));
    o->setProperty ("build", juce::String (ROP_BUILD_ID));
    o->setProperty ("rite", riteToJson());

    //  the globals, but never POSITION or ARRIVAL: those are the performance
    auto* g = new juce::DynamicObject();
    for (const char* id : { rop_ids::mix, rop_ids::output, rop_ids::spread,
                            rop_ids::turn, rop_ids::monogate })
        if (auto* p = apvts.getRawParameterValue (id)) g->setProperty (id, (double) p->load());
    o->setProperty ("globals", juce::var (g));

    f.replaceWithText (juce::JSON::toString (juce::var (o)));
}

bool RiteProcessor::loadQuickPreset (int i)
{
    const auto f = quickPresetFile (i);
    if (f == juce::File() || ! f.existsAsFile()) return false;
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject()) return false;

    riteFromJson (v.getProperty ("rite", juce::var()).toString());

    const juce::var g = v.getProperty ("globals", juce::var());
    if (g.isObject())
        for (const char* id : { rop_ids::mix, rop_ids::output, rop_ids::spread,
                                rop_ids::turn, rop_ids::monogate })
            if (g.hasProperty (id))
                if (auto* p = apvts.getParameter (id))
                {
                    const double raw = (double) g.getProperty (id, juce::var (0.0));
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) raw));
                }
    return true;
}

//  the six factories are written ONCE, and never over a slot the user has
//  stored into — a preset that came back from the dead is the worst kind
void RiteProcessor::ensureQuickPresets()
{
    for (int i = 0; i < kQuickPresets; ++i)
    {
        const auto f = quickPresetFile (i);
        if (f == juce::File() || f.existsAsFile()) continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", juce::String (kFactory[i].name));
        o->setProperty ("build", juce::String (ROP_BUILD_ID));
        o->setProperty ("factory", true);
        o->setProperty ("rite", factoryRiteJson (kFactory[i]));
        auto* g = new juce::DynamicObject();
        g->setProperty (rop_ids::mix,      kFactory[i].mix);
        g->setProperty (rop_ids::output,   kFactory[i].output);
        g->setProperty (rop_ids::spread,   kFactory[i].spread);
        g->setProperty (rop_ids::turn,     kFactory[i].turn);
        g->setProperty (rop_ids::monogate, kFactory[i].monogate);
        o->setProperty ("globals", juce::var (g));
        f.replaceWithText (juce::JSON::toString (juce::var (o)));
    }
}

void RiteProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("rite", riteToJson(), nullptr);
    state.setProperty ("bwfx", juce::String (worldFx.toJson()), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void RiteProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        const juce::String rite = tree.getProperty ("rite", juce::String()).toString();
        const juce::String blob = tree.getProperty ("bwfx", juce::String()).toString();
        apvts.replaceState (tree);
        riteFromJson (rite);
        worldFx.fromJson (blob.toStdString());
    }
}

juce::AudioProcessorEditor* RiteProcessor::createEditor() { return new RiteEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RiteProcessor(); }
