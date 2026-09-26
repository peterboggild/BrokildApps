#include "BeetProcessor.h"
#include "BeetEditor.h"

// The three drums, from their own source folders.
#include "../../../kickstart/plugin/src/PluginProcessor.h"
#include "../../../snare-tactics/plugin/src/PluginProcessor.h"
#include "../../../hats-off/plugin/src/PluginProcessor.h"

#ifndef BEET_BUILD_ID
 #define BEET_BUILD_ID "0.0.0"
#endif

namespace
{
    juce::String pid (int s, const char* what) { return "s" + juce::String (s + 1) + "_" + what; }

    constexpr float kMinDb = -60.0f;
    float dbToGain (float db) { return db <= kMinDb + 0.01f ? 0.0f : juce::Decibels::decibelsToGain (db); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BeetProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        const juce::String n = "Slot " + juce::String (s + 1) + " ";
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid (s, "level"), 1 }, n + "Level",
                   juce::NormalisableRange<float> (kMinDb, 6.0f, 0.1f, 2.5f), 0.0f,
                   juce::AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return v <= kMinDb + 0.01f ? juce::String ("-inf") : juce::String (v, 1); })));
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid (s, "pan"), 1 }, n + "Pan",
                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
                   juce::AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) {
                           const int p = juce::roundToInt (v * 100.0f);
                           return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p)); })));
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid (s, "mute"), 1 }, n + "Mute", false));
    }
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master",
               juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
               juce::AudioParameterFloatAttributes().withLabel ("dB")));
    bwfx_juce::addMacroParameters (l);        // BWFX MACRO 1..5, the rack's whole host surface
    return l;
}

BeetProcessor::BeetProcessor()
    : juce::AudioProcessor ([] {
          //  the main mix, plus one stereo pair per slot. The pairs start
          //  DISABLED: stereo is the default, and a host brings a pair up only
          //  when something is routed to it.
          BusesProperties b;
          b = b.withOutput ("Mix", juce::AudioChannelSet::stereo(), true);
          for (int s = 0; s < beet::NUM_SLOTS; ++s)
              b = b.withOutput ("Slot " + juce::String (s + 1), juce::AudioChannelSet::stereo(), false);
          return b; }()),
      apvts (*this, nullptr, "BEETMACHINE", layout())
{
    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        pLevel[(size_t) s] = apvts.getRawParameterValue (pid (s, "level"));
        pPan[(size_t) s]   = apvts.getRawParameterValue (pid (s, "pan"));
        pMute[(size_t) s]  = apvts.getRawParameterValue (pid (s, "mute"));
        slots[(size_t) s].note = beet::C3_ROW[s];
    }
    pMaster = apvts.getRawParameterValue ("master");

    loadKit (0);          // a fresh instance is a whole kit, not eight empty bays
    startTimerHz (15);    // the rack's service() (IR builds) must run with the editor closed
}

BeetProcessor::~BeetProcessor()
{
    stopTimer();
    for (auto& sl : slots) sl.proc.reset();
    graveyard.clear();
}

//==============================================================================
std::unique_ptr<juce::AudioProcessor> BeetProcessor::makeDrum (int type) const
{
    switch (type)
    {
        case beet::KICK:  return std::make_unique<KickstartProcessor>();
        case beet::SNARE: return std::make_unique<SnareTacticsProcessor>();
        case beet::HATS:  return std::make_unique<HatsOffProcessor>();
        default:          return nullptr;
    }
}

void BeetProcessor::prepareDrum (juce::AudioProcessor& p) const
{
    p.setPlayConfigDetails (0, 2, sr, blockSize);
    p.prepareToPlay (sr, blockSize);
}

void BeetProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    blockSize = juce::jmax (16, samplesPerBlock);
    bwfxRack.prepare (sampleRate, blockSize);

    //  Every drum type's latency, measured on a throwaway instance, so the
    //  alignment does not change when a slot changes type (a latency that
    //  moves makes the host re-align the whole track).
    maxLatency = 0;
    for (int t = beet::KICK; t < beet::NUM_TYPES; ++t)
    {
        auto tmp = makeDrum (t);
        prepareDrum (*tmp);
        latency[(size_t) t] = tmp->getLatencySamples();
        maxLatency = juce::jmax (maxLatency, latency[(size_t) t]);
    }
    setLatencySamples (maxLatency);

    const juce::SpinLock::ScopedLockType lock (swapLock);
    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        auto& sl = slots[(size_t) s];
        if (sl.proc != nullptr) prepareDrum (*sl.proc);
        resetSlotAudio (s);
    }
    clock = 0;
    prepared = true;
}

bool BeetProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (! l.inputBuses.isEmpty())
        for (auto& in : l.inputBuses) if (! in.isDisabled()) return false;
    for (int b = 1; b < l.outputBuses.size(); ++b)
    {
        const auto& set = l.outputBuses.getReference (b);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

bool BeetProcessor::slotOutputConnected (int s) const
{
    if (auto* bus = getBus (false, 1 + s)) return bus->isEnabled();
    return false;
}

//==============================================================================
//  message thread
void BeetProcessor::resetSlotAudio (int s)
{
    auto& sl = slots[(size_t) s];
    const int t = sl.type.load();
    sl.delay = (t == beet::EMPTY) ? 0 : maxLatency - latency[(size_t) t];
    sl.ring.setSize (2, juce::jmax (1, sl.delay + blockSize + 8), false, true, false);
    sl.ring.clear();
    sl.ringPos = 0;
    sl.buf.setSize (2, blockSize, false, true, false);
    sl.gain = sl.target = 1.0f;
    sl.numEvents = 0;
    sl.midi.clear();
}

void BeetProcessor::setSlotType (int s, int type)
{
    s = juce::jlimit (0, beet::NUM_SLOTS - 1, s);
    type = juce::jlimit (0, beet::NUM_TYPES - 1, type);
    auto& sl = slots[(size_t) s];
    if (sl.type.load() == type && (type == beet::EMPTY) == (sl.proc == nullptr)) return;

    auto fresh = makeDrum (type);
    if (fresh != nullptr)
    {
        if (fresh->getNumPrograms() > 1) fresh->setCurrentProgram (1);   // skip INIT
        if (prepared) prepareDrum (*fresh);
    }

    std::unique_ptr<juce::AudioProcessor> old;
    {
        const juce::SpinLock::ScopedLockType lock (swapLock);
        old = std::move (sl.proc);
        sl.proc = std::move (fresh);
        sl.type = type;
        sl.keys = nullptr;
        if (auto* a = dynamic_cast<SnareTacticsProcessor*> (sl.proc.get())) sl.keys = a->apvts.getRawParameterValue ("keys");
        if (auto* a = dynamic_cast<HatsOffProcessor*> (sl.proc.get()))      sl.keys = a->apvts.getRawParameterValue ("keys");
        resetSlotAudio (s);
    }
    if (old != nullptr)
    {
        const juce::ScopedLock g (graveLock);
        graveyard.push_back (std::move (old));
    }
    ++layoutVersion;
}

void BeetProcessor::setSlotPreset (int s, int program)
{
    if (auto* p = slotProcessor (s))
        p->setCurrentProgram (juce::jlimit (0, p->getNumPrograms() - 1, program));
}

int BeetProcessor::slotPreset (int s) const
{
    if (auto* p = slotProcessor (s)) return p->getCurrentProgram();
    return -1;
}

juce::String BeetProcessor::slotPresetName (int s) const
{
    //  the drum's own idea of its name, so a loaded user patch reads as itself
    if (auto* k = dynamic_cast<KickstartProcessor*> (slotProcessor (s)))    return k->currentName();
    if (auto* n = dynamic_cast<SnareTacticsProcessor*> (slotProcessor (s))) return n->currentName();
    if (auto* h = dynamic_cast<HatsOffProcessor*> (slotProcessor (s)))      return h->currentName();
    return {};
}

void BeetProcessor::setSlotNote (int s, int note)
{
    slots[(size_t) s].note = juce::jlimit (0, 127, note);
    noteMapMode = MAP_CUSTOM;
    for (int i = 0; i < beet::NUM_SLOTS; ++i)
    {
        //  still exactly one of the rows? then say so
        bool c3 = true, gm = true;
        for (int j = 0; j < beet::NUM_SLOTS; ++j)
        {
            c3 = c3 && slots[(size_t) j].note.load() == beet::C3_ROW[j];
            gm = gm && slots[(size_t) j].note.load() == beet::GM_ROW[j];
        }
        if (c3) noteMapMode = MAP_C3; else if (gm) noteMapMode = MAP_GM;
        break;
    }
}

void BeetProcessor::setNoteMap (int map)
{
    if (map == MAP_CUSTOM) return;
    for (int s = 0; s < beet::NUM_SLOTS; ++s)
        slots[(size_t) s].note = (map == MAP_GM ? beet::GM_ROW[s] : beet::C3_ROW[s]);
    noteMapMode = map;
}

void BeetProcessor::loadKit (int index)
{
    currentKit = juce::jlimit (0, beet::numKits() - 1, index);
    const auto& k = beet::kit (currentKit);
    missingPresets.clear();

    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        const auto& ks = k.slots[s];
        setSlotType (s, ks.type);
        if (auto* p = slotProcessor (s))
        {
            int found = -1;
            for (int i = 0; i < p->getNumPrograms(); ++i)
                if (p->getProgramName (i) == ks.preset) { found = i; break; }
            if (found < 0) { missingPresets.add (juce::String (beet::typeShort (ks.type)) + ": " + ks.preset); found = 1; }
            p->setCurrentProgram (found);

            //  the kit's own adjustments to that drum, by parameter ID in real units
            for (auto& t : juce::StringArray::fromTokens (ks.tweaks != nullptr ? ks.tweaks : "", " ", ""))
            {
                const auto id = t.upToFirstOccurrenceOf ("=", false, false);
                const float v = t.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
                juce::RangedAudioParameter* hit = nullptr;
                for (auto* prm : p->getParameters())
                    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                        if (r->getParameterID() == id) { hit = r; break; }
                if (hit == nullptr || ! t.containsChar ('=')
                    || v < hit->getNormalisableRange().start || v > hit->getNormalisableRange().end)
                    missingPresets.add (juce::String (beet::typeShort (ks.type)) + " tweak: " + t);
                else
                    hit->setValueNotifyingHost (hit->convertTo0to1 (v));
            }
        }
        auto set = [this] (const juce::String& id, float v) {
            if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        set (pid (s, "level"), ks.levelDb);
        set (pid (s, "pan"),   ks.pan);
        set (pid (s, "mute"),  0.0f);
        setSlotChokedBy (s, ks.chokedBy);
        setSlotSolo (s, false);
        //  notes and outputs are NOT part of a kit: changing the sound must not
        //  change the wiring someone has set up in their session
    }
    //  A kit SETS the rack, as a patch does on every Brokild synth: a kit with
    //  a rack brings it, a kit without one loads the empty rack.
    bwfxRack.fromJson (k.rack != nullptr ? k.rack : "");
    ++layoutVersion;
}

void BeetProcessor::collectGarbage()
{
    const juce::ScopedLock g (graveLock);
    graveyard.clear();
}

void BeetProcessor::timerCallback()
{
    bwfxRack.service();
    //  with no editor open nothing can still be showing a replaced drum
    if (getActiveEditor() == nullptr) collectGarbage();
}

//==============================================================================
void BeetProcessor::pushEvent (Slot& sl, juce::int64 at, bool restore)
{
    if (sl.numEvents >= (int) sl.events.size()) return;   // 64 pending in one slot is a flood; drop
    //  kept in time order; a later event for the same moment wins
    int i = sl.numEvents++;
    while (i > 0 && sl.events[(size_t) (i - 1)].at > at) { sl.events[(size_t) i] = sl.events[(size_t) (i - 1)]; --i; }
    sl.events[(size_t) i] = { at, restore };
}

void BeetProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    const juce::SpinLock::ScopedLockType lock (swapLock);

    //  --- hits -------------------------------------------------------------
    struct Hit { int slot, pos; float vel; };
    std::array<Hit, 256> hits;
    int numHits = 0;
    auto addHit = [&] (int s, int pos, float vel) { if (numHits < (int) hits.size()) hits[(size_t) numHits++] = { s, pos, vel }; };

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        const int note = msg.getNoteNumber();
        const int pos = juce::jlimit (0, n - 1, meta.samplePosition);

        if (const int ls = learnSlot.load(); ls >= 0)
        {
            slots[(size_t) ls].note = note;
            noteMapMode = MAP_CUSTOM;
            learnSlot = -1;
        }
        for (int s = 0; s < beet::NUM_SLOTS; ++s)
            if (slots[(size_t) s].type.load() != beet::EMPTY && slots[(size_t) s].note.load() == note)
                addHit (s, pos, msg.getFloatVelocity());      // same note on two slots layers them
    }
    for (int s = 0; s < beet::NUM_SLOTS; ++s)
        if (const int v = slots[(size_t) s].audition.exchange (0); v > 0 && slots[(size_t) s].type.load() != beet::EMPTY)
            addHit (s, 0, v / 127.0f);

    if (panicRequest.exchange (false))
        for (auto& sl : slots) pushEvent (sl, clock, false);

    for (int h = 0; h < numHits; ++h)
    {
        const auto& hit = hits[(size_t) h];
        auto& sl = slots[(size_t) hit.slot];
        const int keys = sl.keys != nullptr ? juce::roundToInt (sl.keys->load()) : 2;
        sl.midi.addEvent (juce::MidiMessage::noteOn (1, beet::neutralNote (sl.type.load(), keys), hit.vel), hit.pos);
        ++sl.hits;

        //  Every slot's audio is aligned to maxLatency, so the moment this hit
        //  SOUNDS in the output is pos + maxLatency. Chokes act there.
        const juce::int64 at = clock + hit.pos + maxLatency;
        pushEvent (sl, at, true);
        for (int i = 0; i < beet::NUM_SLOTS; ++i)
            if (i != hit.slot && (slots[(size_t) i].chokedBy.load() & (1u << hit.slot)) != 0)
                pushEvent (slots[(size_t) i], at, false);
    }

    //  --- render -----------------------------------------------------------
    bool anySolo = false;
    for (auto& sl : slots) anySolo = anySolo || (sl.solo.load() && sl.proc != nullptr);

    const float master = juce::Decibels::decibelsToGain (pMaster->load());
    const float chokeK = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));   // 3 ms
    auto* mainL = buffer.getWritePointer (0);
    auto* mainR = buffer.getWritePointer (1);

    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        auto& sl = slots[(size_t) s];
        if (sl.proc == nullptr) { sl.meter = 0.0f; continue; }        // empty: costs nothing

        if (sl.buf.getNumSamples() < n) sl.buf.setSize (2, n, false, false, true);
        juce::AudioBuffer<float> view (sl.buf.getArrayOfWritePointers(), 2, n);
        view.clear();
        sl.proc->setPlayHead (getPlayHead());
        sl.proc->processBlock (view, sl.midi);
        sl.midi.clear();

        //  latency alignment: delay this drum by what the slowest drum needs
        if (sl.delay > 0)
        {
            const int len = sl.ring.getNumSamples();
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* r = sl.ring.getWritePointer (ch);
                auto* x = view.getWritePointer (ch);
                int w = sl.ringPos;
                for (int i = 0; i < n; ++i)
                {
                    r[w] = x[i];
                    int rd = w - sl.delay; if (rd < 0) rd += len;
                    x[i] = r[rd];
                    if (++w >= len) w = 0;
                }
            }
            sl.ringPos = (sl.ringPos + n) % sl.ring.getNumSamples();
        }

        //  choke envelope, sample-accurate against the event queue
        auto* L = view.getWritePointer (0);
        auto* R = view.getWritePointer (1);
        int e = 0;
        for (int i = 0; i < n; ++i)
        {
            const juce::int64 now = clock + i;
            while (e < sl.numEvents && sl.events[(size_t) e].at <= now)
            {
                if (sl.events[(size_t) e].restore) { sl.gain = sl.target = 1.0f; }
                else                               { sl.target = 0.0f; }
                ++e;
            }
            if (sl.gain != sl.target)
            {
                sl.gain += (sl.target - sl.gain) * chokeK;
                if (sl.gain < 1.0e-5f) sl.gain = 0.0f;
            }
            L[i] *= sl.gain; R[i] *= sl.gain;
        }
        if (e > 0)
        {
            for (int k = e; k < sl.numEvents; ++k) sl.events[(size_t) (k - e)] = sl.events[(size_t) k];
            sl.numEvents -= e;
        }

        //  level, pan (a balance law: centre is unity), mute, solo
        const bool silent = pMute[(size_t) s]->load() > 0.5f || (anySolo && ! sl.solo.load());
        const float g = silent ? 0.0f : dbToGain (pLevel[(size_t) s]->load()) * master;
        const float p = pPan[(size_t) s]->load();
        const float gl = g * (p > 0.0f ? std::cos (p * juce::MathConstants<float>::halfPi) : 1.0f);
        const float gr = g * (p < 0.0f ? std::cos (-p * juce::MathConstants<float>::halfPi) : 1.0f);
        view.applyGainRamp (0, 0, n, sl.lastL, gl);
        view.applyGainRamp (1, 0, n, sl.lastR, gr);
        sl.lastL = gl; sl.lastR = gr;

        sl.meter = juce::jmax (view.getMagnitude (0, 0, n), view.getMagnitude (1, 0, n));

        //  routing: MIX, OWN, BOTH. OWN with the pair not enabled in the host
        //  falls back to the mix - a slot routed to nowhere would just vanish.
        const int mode = sl.out.load();
        const bool own = mode != beet::OUT_MIX && slotOutputConnected (s);
        const bool toMix = mode == beet::OUT_MIX || mode == beet::OUT_BOTH || ! own;
        if (toMix)
        {
            juce::FloatVectorOperations::add (mainL, L, n);
            juce::FloatVectorOperations::add (mainR, R, n);
        }
        if (own)
        {
            auto aux = getBusBuffer (buffer, false, 1 + s);
            if (aux.getNumChannels() >= 2)
            {
                aux.copyFrom (0, 0, L, n);
                aux.copyFrom (1, 0, R, n);
            }
        }
    }

    //  --- BWFX on the main mix (an empty rack is bit-transparent) ---------
    {
        double bpm = 0.0, ppq = -1.0;
        bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                bpm = pos->getBpm().orFallback (0.0);
                ppq = pos->getPpqPosition().orFallback (-1.0);
                playing = pos->getIsPlaying();
            }
        bwfxRack.setTransport (bpm, ppq, playing);
        bwfx_juce::pushMacros (bwfxRack, apvts);
        bwfxRack.process (mainL, mainR, n);
    }

    clock += n;
    midi.clear();
}

//==============================================================================
void BeetProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("build", BEET_BUILD_ID, nullptr);
    state.setProperty ("kit", currentKit, nullptr);
    state.setProperty ("noteMap", noteMapMode.load(), nullptr);
    state.setProperty ("bwfx", juce::String (bwfxRack.toJson()), nullptr);   // one opaque string

    juce::ValueTree slotsTree ("SLOTS");
    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        const auto& sl = slots[(size_t) s];
        juce::ValueTree t ("SLOT");
        t.setProperty ("type", sl.type.load(), nullptr);
        t.setProperty ("note", sl.note.load(), nullptr);
        t.setProperty ("out", sl.out.load(), nullptr);
        t.setProperty ("chokedBy", (int) sl.chokedBy.load(), nullptr);
        t.setProperty ("solo", sl.solo.load(), nullptr);
        if (sl.proc != nullptr)
        {
            juce::MemoryBlock mb;
            sl.proc->getStateInformation (mb);
            t.setProperty ("drum", mb.toBase64Encoding(), nullptr);
        }
        slotsTree.appendChild (t, nullptr);
    }
    state.appendChild (slotsTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BeetProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType())) return;

    auto slotsTree = tree.getChildWithName ("SLOTS");
    tree.removeChild (slotsTree, nullptr);
    currentKit = juce::jlimit (0, beet::numKits() - 1, (int) tree.getProperty ("kit", 0));
    const juce::String rackBlob = tree.getProperty ("bwfx", juce::String()).toString();
    tree.removeProperty ("bwfx", nullptr);
    apvts.replaceState (tree);
    bwfxRack.fromJson (rackBlob.toStdString());     // "" (a pre-BWFX project) = the empty rack

    for (int s = 0; s < beet::NUM_SLOTS && s < slotsTree.getNumChildren(); ++s)
    {
        auto t = slotsTree.getChild (s);
        setSlotType (s, (int) t.getProperty ("type", beet::EMPTY));
        slots[(size_t) s].note = juce::jlimit (0, 127, (int) t.getProperty ("note", beet::C3_ROW[s]));
        setSlotOut (s, (int) t.getProperty ("out", beet::OUT_MIX));
        setSlotChokedBy (s, (unsigned) (int) t.getProperty ("chokedBy", 0));
        setSlotSolo (s, (bool) t.getProperty ("solo", false));
        if (auto* p = slotProcessor (s))
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (t.getProperty ("drum").toString()) && mb.getSize() > 0)
                p->setStateInformation (mb.getData(), (int) mb.getSize());
        }
    }
    noteMapMode = juce::jlimit (0, 2, (int) tree.getProperty ("noteMap", MAP_C3));
    ++layoutVersion;
}

//==============================================================================
juce::AudioProcessorEditor* BeetProcessor::createEditor() { return new BeetEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BeetProcessor(); }
