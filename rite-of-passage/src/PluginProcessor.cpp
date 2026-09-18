#include "PluginProcessor.h"
#include "PluginEditor.h"

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
