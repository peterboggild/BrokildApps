#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    juce::String fmtValue (const fmr::PSpec& s, float v)
    {
        using namespace fmr;
        switch (s.kind)
        {
            case KP_SW:   return v >= 0.5f ? "ON" : "OFF";
            case KP_HZ:
            {
                const float f = xmap (v, s.lo, s.hi);
                return f < 1000.0f ? juce::String (f, 1) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
            }
            case KP_MS:
            {
                const float ms = xmap (v, s.lo, s.hi);
                return ms < 1000.0f ? juce::String (ms, 0) + " ms" : juce::String (ms / 1000.0f, 2) + " s";
            }
            case KP_LIST:
            {
                int n = 0; auto names = listNames (s.id, n);
                const int i = juce::jlimit (0, std::max (0, n - 1), (int) std::round (v));
                return names != nullptr && n > 0 ? juce::String (names[i]) : juce::String (i);
            }
            case KP_BIPOL: { const float c = (v - 0.5f) * 200.0f; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " %"; }
            case KP_SEMI:  { const float st = (v - 0.5f) * s.lo; return (st >= 0 ? "+" : "") + juce::String (st, 1) + " semi"; }
            case KP_VOL:   { const float g = v < 0.005f ? 0.0f : 2.0f * v * v;
                             return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB"; }
            default:       return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }
}

//==============================================================================
/*  Main stereo plus twelve mono aux. Beatmakers ask for separate outs first,
    and it decides the bus architecture, so it goes in now rather than later.
    Every aux is off by default: a host that wants them enables them. */
juce::AudioProcessor::BusesProperties FmrAudioProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int c = 0; c < fmr::NCH; ++c)
        b = b.withOutput (fmr::channelName (c), juce::AudioChannelSet::mono(), false);
    return b;
}

juce::AudioProcessorValueTreeState::ParameterLayout FmrAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < fmr::numParams(); ++i)
    {
        const auto& s = fmr::paramSpec (i);
        const float hi = fmr::paramMax (s);
        const bool stepped = s.kind == fmr::KP_LIST || s.kind == fmr::KP_SW;
        const juce::String sid (s.id);
        const bool automatable = (sid != "os");     // a selector, not a lane

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (fmr::paramSpec (i), v); })));
    }
    //  the rack's five automatable macros, declared by shared code so
    //  every synth carries the identical five (see bwfx_juce.h)
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
FmrAudioProcessor::FmrAudioProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "FULLMETALRACKET", createParameterLayout())
{
    for (int i = 0; i < fmr::numParams(); ++i) ids.add (fmr::paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);

    startTimerHz (15);                       // bwfxRack.service(), editor open or not
    bwfxRack.setWorldModConsumed (true);     // this engine maps the SPECTRA bus
}

FmrAudioProcessor::~FmrAudioProcessor() = default;

bool FmrAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono())
        return false;
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto set = layouts.outputBuses[b];
        if (! set.isDisabled() && set != juce::AudioChannelSet::mono()) return false;
    }
    return true;
}

void FmrAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < fmr::numParams(); ++i)
        fmr::pvalue (engine.p, fmr::paramSpec (i)) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

void FmrAudioProcessor::pushEvent (const UiEvent& e)
{
    const int w = evWrite.load();
    const int next = (w + 1) % EVQ;
    if (next == evRead.load()) return;       // full: drop, never block
    evq[(size_t) w] = e;
    evWrite.store (next);
}

//==============================================================================
void FmrAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) engine.allNotesOff();

    for (int i = 0; i < fmr::numParams(); ++i)
        fmr::pvalue (engine.p, fmr::paramSpec (i)) = raw[(size_t) i]->load();

    /*  MORPH is applied to the engine's COPY of the parameters, never written
        back to the APVTS. A performance fader that silently rewrote a hundred
        knobs would make both its own automation and theirs unusable — this way
        the knobs stay where the player left them and the sound follows the
        fader. */
    engine.applyMorph (engine.p);

    /*  A host with a transport must be recognised as such even when it
        reports no tempo — the old form only called setTransport inside
        the getBpm() test, so such a host was indistinguishable from the
        standalone and the machine free-ran through a stopped DAW. */
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            const auto bpm = pos->getBpm();
            const auto ppq = pos->getPpqPosition();
            const bool good = bpm && *bpm > 20.0 && *bpm < 999.0;
            if (good)
            {
                engine.p.bpm = *bpm;
                bwfxRack.setBpm (*bpm);
                bwfxRack.setTransport (*bpm, ppq ? *ppq : 0.0, pos->getIsPlaying());
            }
            engine.setTransport (good ? *bpm : 0.0,
                                 ppq ? *ppq : -1.0,
                                 pos->getIsPlaying() && ppq.hasValue(),
                                 true);
        }
    }

    /*  SPECTRA world-mod bus: the rack's characters possess the machine. One
        block of modulation latency; a neutral bus is bit-identical. Note that
        on a drum machine the house rule about pitch sag inverts — see the
        comment in Engine.cpp. */
    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth,
                            wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    // the panel's trigger pads
    while (evRead.load() != evWrite.load())
    {
        const auto e = evq[(size_t) evRead.load()];
        evRead.store ((evRead.load() + 1) % EVQ);
        if      (e.kind == 1) engine.trigger (e.chan, e.v);
        else if (e.kind == 2) engine.allNotesOff();
    }

    auto* mainBus = getBus (true, 0);
    auto main = mainBus != nullptr ? mainBus->getBusBuffer (buffer) : buffer;
    const int nch = main.getNumChannels();
    auto* L = main.getWritePointer (0);
    std::vector<float> monoR;
    float* R = nch >= 2 ? main.getWritePointer (1) : nullptr;
    if (R == nullptr) { monoR.assign ((size_t) n, 0.0f); R = monoR.data(); }

    // the aux outs, only when the host has actually enabled some
    float* aux[fmr::NCH] = { nullptr };
    bool anyAux = false;
    for (int c = 0; c < fmr::NCH; ++c)
        if (auto* bus = getBus (true, c + 1))
            if (bus->isEnabled())
            {
                auto ab = bus->getBusBuffer (buffer);
                if (ab.getNumChannels() > 0) { aux[c] = ab.getWritePointer (0); anyAux = true; }
            }

    // render between MIDI events so a hit lands where it was played
    int last = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (pos > last)
        {
            float* a2[fmr::NCH];
            for (int c = 0; c < fmr::NCH; ++c) a2[c] = aux[c] != nullptr ? aux[c] + last : nullptr;
            engine.process (L + last, R + last, pos - last, anyAux ? a2 : nullptr);
            last = pos;
        }

        const auto m = meta.getMessage();
        if (m.isNoteOn())                                  engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isAllNotesOff() || m.isAllSoundOff())   engine.allNotesOff();
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc == 120 || cc == 123) engine.allNotesOff();
        }
    }
    if (n > last)
    {
        float* a2[fmr::NCH];
        for (int c = 0; c < fmr::NCH; ++c) a2[c] = aux[c] != nullptr ? aux[c] + last : nullptr;
        engine.process (L + last, R + last, n - last, anyAux ? a2 : nullptr);
    }

    // the world rack, on the main bus only (an aux out is a dry channel feed)
    bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros
    bwfxRack.process (L, R, n);

    if (nch == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
void FmrAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        xml->setAttribute ("seq",  juce::JSON::toString (seqVar(), true));
        xml->setAttribute ("kits", juce::JSON::toString (kitsVar(), true));
        xml->setAttribute ("kit",  kitIndex);
        copyXmlToBinary (*xml, dest);
    }
}

void FmrAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            //  a project made before any of this simply has no attribute, and
            //  gets an empty sequencer and no captured kits — which is exactly
            //  what it sounded like
            seqApply  (juce::JSON::parse (xml->getStringAttribute ("seq")));
            kitsApply (juce::JSON::parse (xml->getStringAttribute ("kits")));
            kitIndex = xml->getIntAttribute ("kit", 0);
            xml->removeAttribute ("bwfx");
            xml->removeAttribute ("seq");
            xml->removeAttribute ("kits");
            xml->removeAttribute ("kit");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
        }
}

juce::AudioProcessorEditor* FmrAudioProcessor::createEditor()
{
    return new FmrAudioProcessorEditor (*this);
}

//==============================================================================
void FmrAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void FmrAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

void FmrAudioProcessor::applyKitIndex (int i)
{
    kitIndex = juce::jlimit (0, fmr::numSeeds() - 1, i);
    patchName = {};                      // a dialled seed is no longer that file
    /*  a morph belongs to ONE patch: a blend between the kit you have left
        and one you never chose is not a morph, it is a haunting */
    engine.haveA = engine.haveB = false;
    engine.restartClock();
    fmr::Params q;
    fmr::applySeed (kitIndex, q);
    for (int k = 0; k < fmr::numParams(); ++k)
    {
        const auto& s = fmr::paramSpec (k);
        const juce::String sid (s.id);
        //  the master, the oversampling and everything that belongs to the
        //  PERFORMANCE rather than to the kit is left alone
        if (sid == "os" || sid == "volume" || sid == "seq" || sid == "tempo"
            || sid == "morph" || sid == "swing" || sid == "feel" || sid == "grip") continue;
        setParamById (s.id, fmr::pvalue (q, s));
    }
    bwfxRack.fromJson ("");        // a kit brings its own rack; the seeds carry none
    emitBwfx();
    emitKit();
    notice (juce::String ("KIT ") + juce::String (kitIndex + 1) + " " +
            juce::String (fmr::seedCategoryName (fmr::seedCategory (kitIndex))) + " " +
            DOT + " " + fmr::kitName (kitIndex));
}

void FmrAudioProcessor::emitKit()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("i", kitIndex);
    o->setProperty ("name", patchName.isNotEmpty() ? patchName : juce::String (fmr::kitName (kitIndex)));
    o->setProperty ("cat", patchName.isNotEmpty() ? juce::String ("USER PATCH")
                                                  : juce::String (fmr::seedCategoryName (fmr::seedCategory (kitIndex))));
    o->setProperty ("user", patchName.isNotEmpty());
    o->setProperty ("rec", engine.recArm.load());
    o->setProperty ("a", engine.haveA);
    o->setProperty ("b", engine.haveB);
    /*  The captured kits go to the panel so it can show the blend as the
        fader moves. Sent on capture rather than per frame — it is a few
        hundred numbers, and they only change when something is captured. */
    auto pack = [] (const fmr::Params& q)
    {
        auto* k = new juce::DynamicObject();
        for (int i = 0; i < fmr::numParams(); ++i)
        {
            const auto& sp = fmr::paramSpec (i);
            if (fmr::morphable (sp))
                k->setProperty (sp.id, (double) fmr::pvalue (const_cast<fmr::Params&> (q), sp));
        }
        return juce::var (k);
    };
    if (engine.haveA) o->setProperty ("kitA", pack (engine.kitA));
    if (engine.haveB) o->setProperty ("kitB", pack (engine.kitB));
    emitToUi ("kit", juce::var (o));
}

//==============================================================================
/*  The sequencer, sparse. Only steps that are ON are written, and only lanes
    that have any — an empty machine costs about forty bytes of state. */
juce::var FmrAudioProcessor::seqVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("cur", engine.curPat);
    juce::Array<juce::var> pats;
    for (int pi = 0; pi < fmr::NPAT; ++pi)
    {
        juce::Array<juce::var> lanes;
        for (int c = 0; c < fmr::NCH; ++c)
        {
            const auto& L = engine.pat[pi].lane[c];
            juce::Array<juce::var> steps;
            for (int k = 0; k < fmr::NSTEP; ++k)
            {
                const auto& st = L.step[k];
                if (! st.on) continue;
                juce::Array<juce::var> e;
                e.add (k); e.add ((int) st.vel); e.add ((int) st.prob);
                e.add ((int) st.ratchet); e.add ((int) st.cond); e.add ((int) st.micro);
                steps.add (juce::var (e));
            }
            const bool plain = steps.isEmpty() && L.len == 16 && L.div == 1
                            && L.dir == 0 && L.swing == 0 && L.mute == 0;
            if (plain) continue;
            auto* lo = new juce::DynamicObject();
            lo->setProperty ("c", c);
            lo->setProperty ("len", (int) L.len);
            lo->setProperty ("div", (int) L.div);
            lo->setProperty ("dir", (int) L.dir);
            lo->setProperty ("sw", (int) L.swing);
            lo->setProperty ("m", (int) L.mute);
            lo->setProperty ("st", steps);
            lanes.add (juce::var (lo));
        }
        if (lanes.isEmpty()) continue;
        auto* po = new juce::DynamicObject();
        po->setProperty ("i", pi);
        po->setProperty ("lanes", lanes);
        pats.add (juce::var (po));
    }
    root->setProperty ("pats", pats);
    return juce::var (root);
}

void FmrAudioProcessor::seqApply (const juce::var& v)
{
    for (int pi = 0; pi < fmr::NPAT; ++pi) engine.pat[pi] = fmr::Pattern();
    engine.curPat = 0;
    auto* o = v.getDynamicObject();
    if (o == nullptr) return;
    engine.curPat = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("cur"));
    if (auto* pats = o->getProperty ("pats").getArray())
        for (const auto& pv : *pats)
        {
            auto* po = pv.getDynamicObject();
            if (po == nullptr) continue;
            const int pi = juce::jlimit (0, fmr::NPAT - 1, (int) po->getProperty ("i"));
            if (auto* lanes = po->getProperty ("lanes").getArray())
                for (const auto& lv : *lanes)
                {
                    auto* lo = lv.getDynamicObject();
                    if (lo == nullptr) continue;
                    const int c = juce::jlimit (0, fmr::NCH - 1, (int) lo->getProperty ("c"));
                    auto& L = engine.pat[pi].lane[c];
                    L.len   = (uint8_t) juce::jlimit (1, fmr::NSTEP, (int) lo->getProperty ("len"));
                    L.div   = (uint8_t) juce::jlimit (0, 3, (int) lo->getProperty ("div"));
                    L.dir   = (uint8_t) juce::jlimit (0, 3, (int) lo->getProperty ("dir"));
                    L.swing = (int8_t)  juce::jlimit (-50, 50, (int) lo->getProperty ("sw"));
                    L.mute  = (uint8_t) ((int) lo->getProperty ("m") != 0 ? 1 : 0);
                    if (auto* steps = lo->getProperty ("st").getArray())
                        for (const auto& sv : *steps)
                            if (auto* e = sv.getArray())
                                if (e->size() >= 6)
                                {
                                    const int k = juce::jlimit (0, fmr::NSTEP - 1, (int) (*e)[0]);
                                    auto& st = L.step[k];
                                    st.on      = 1;
                                    st.vel     = (uint8_t) juce::jlimit (1, 127, (int) (*e)[1]);
                                    st.prob    = (uint8_t) juce::jlimit (0, 100, (int) (*e)[2]);
                                    st.ratchet = (uint8_t) juce::jlimit (1, 8,   (int) (*e)[3]);
                                    st.cond    = (uint8_t) juce::jlimit (0, 5,   (int) (*e)[4]);
                                    st.micro   = (int8_t)  juce::jlimit (-50, 50, (int) (*e)[5]);
                                }
                }
        }
}

juce::var FmrAudioProcessor::kitsVar() const
{
    auto* root = new juce::DynamicObject();
    auto pack = [] (const fmr::Params& p)
    {
        auto* o = new juce::DynamicObject();
        for (int i = 0; i < fmr::numParams(); ++i)
        {
            const auto& s = fmr::paramSpec (i);
            //  morphable() is the single answer to "does this belong to the
            //  kit" — storing less than it meant a reloaded morph moved the
            //  twelve voices and left the machine's character behind
            if (! fmr::morphable (s)) continue;
            o->setProperty (s.id, (double) fmr::pvalue (const_cast<fmr::Params&> (p), s));
        }
        return juce::var (o);
    };
    root->setProperty ("haveA", engine.haveA);
    root->setProperty ("haveB", engine.haveB);
    if (engine.haveA) root->setProperty ("A", pack (engine.kitA));
    if (engine.haveB) root->setProperty ("B", pack (engine.kitB));
    return juce::var (root);
}

void FmrAudioProcessor::kitsApply (const juce::var& v)
{
    engine.haveA = engine.haveB = false;
    auto* o = v.getDynamicObject();
    if (o == nullptr) return;
    auto unpack = [] (const juce::var& src, fmr::Params& dst)
    {
        dst = fmr::Params();
        if (auto* so = src.getDynamicObject())
            for (int i = 0; i < fmr::numParams(); ++i)
            {
                const auto& s = fmr::paramSpec (i);
                if (! fmr::morphable (s)) continue;
                if (so->hasProperty (s.id))
                    fmr::pvalue (dst, s) = (float) (double) so->getProperty (s.id);
            }
    };
    if ((bool) o->getProperty ("haveA")) { unpack (o->getProperty ("A"), engine.kitA); engine.haveA = true; }
    if ((bool) o->getProperty ("haveB")) { unpack (o->getProperty ("B"), engine.kitB); engine.haveB = true; }
}

void FmrAudioProcessor::emitSeq()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("seq", seqVar());
    emitToUi ("seq", juce::var (o));
}

void FmrAudioProcessor::randomiseAll()
{
    juce::Random r (juce::Time::getHighResolutionTicks());
    for (int i = 0; i < fmr::numParams(); ++i)
    {
        const auto& s = fmr::paramSpec (i);
        const juce::String sid (s.id);
        /*  The transport and the performance controls are not part of a
            sound. RANDOM used to be able to start the sequencer, change the
            tempo and throw in a morph, which is not what anyone presses it
            for — the same list applyKitIndex leaves alone. */
        if (sid == "os" || sid == "volume" || sid == "seq" || sid == "tempo"
            || sid == "morph" || sid == "swing" || sid == "feel" || sid == "grip") continue;
        if (s.chan >= 0 && s.slot == fmr::CP_MUTE) continue;      // never mute a channel at random
        if (s.chan >= 0 && s.slot == fmr::CP_LEVEL)
        {
            //  the metal sits back a little, same trim the channel table gives
            //  it — a randomised kit should balance like a dialled one
            const bool metal = fmr::channelFamily (s.chan) == fmr::FAM_METAL
                               && juce::String (fmr::channelId (s.chan)) != "ch";
            setParamById (s.id, (0.5f + 0.4f * r.nextFloat()) * (metal ? 0.92f : 1.0f));
            continue;
        }
        if (s.chan >= 0 && s.slot == fmr::CP_PAN)   { setParamById (s.id, 0.35f + 0.3f * r.nextFloat()); continue; }
        if (s.chan >= 0 && s.slot == fmr::CP_DRIVE) { setParamById (s.id, 0.3f * r.nextFloat()); continue; }
        setParamById (s.id, s.kind == fmr::KP_LIST ? std::floor (r.nextFloat() * (fmr::paramMax (s) + 0.999f))
                          : (s.kind == fmr::KP_SW ? (r.nextBool() ? 1.0f : 0.0f) : r.nextFloat()));
    }
    notice ("RANDOM MACHINE");
}

//==============================================================================
void FmrAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* o = payload.getDynamicObject())
        if (auto* arr = o->getProperty ("b").getArray())
        {
            for (const auto& m : *arr) handleOne (m);
            return;
        }
    handleOne (payload);
}

void FmrAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")           setParamById (o->getProperty ("id").toString(), (float) (double) o->getProperty ("v"), true);
    else if (k == "ack")    uiHasState = true;
    else if (k == "ready")  uiReady = true;
    else if (k == "hello")  uiHasState = false;      // the page rebooted under us
    else if (k == "hit")
    {
        UiEvent e; e.kind = 1;
        e.chan = (int) o->getProperty ("c");
        e.v = o->hasProperty ("v") ? (float) (double) o->getProperty ("v") : 0.9f;
        pushEvent (e);
    }
    else if (k == "panic")  { wantPanic = true; notice ("ALL SOUND OFF"); }
    else if (k == "kit")    applyKitIndex ((int) o->getProperty ("i"));
    else if (k == "random") randomiseAll();
    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "capture")
    {
        //  capture the CURRENT panel into kit A or B, so a morph is always
        //  between two things the player actually made
        fmr::Params q;
        for (int i = 0; i < fmr::numParams(); ++i)
            fmr::pvalue (q, fmr::paramSpec (i)) = raw[(size_t) i]->load();
        const bool toB = o->getProperty ("slot").toString() == "b";
        const bool clear = (int) o->getProperty ("clear") != 0;
        if (clear)
        {
            //  emptying a slot must also stand the morph down, or the
            //  fader would keep blending toward a kit that is no longer there
            if (toB) engine.haveB = false; else engine.haveA = false;
            emitKit();
            notice (toB ? "B EMPTIED" : "A EMPTIED");
        }
        else
        {
            if (toB) { engine.kitB = q; engine.haveB = true; }
            else     { engine.kitA = q; engine.haveA = true; }
            emitKit();
            notice (toB ? "CAPTURED INTO B" : "CAPTURED INTO A");
        }
    }
    else if (k == "step")
    {
        const int pi = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("p"));
        const int c  = juce::jlimit (0, fmr::NCH - 1,  (int) o->getProperty ("c"));
        const int s  = juce::jlimit (0, fmr::NSTEP - 1, (int) o->getProperty ("s"));
        auto& st = engine.pat[pi].lane[c].step[s];
        if (o->hasProperty ("on"))   st.on      = (uint8_t) ((int) o->getProperty ("on") ? 1 : 0);
        if (o->hasProperty ("vel"))  st.vel     = (uint8_t) juce::jlimit (1, 127, (int) o->getProperty ("vel"));
        if (o->hasProperty ("prob")) st.prob    = (uint8_t) juce::jlimit (0, 100, (int) o->getProperty ("prob"));
        if (o->hasProperty ("rt"))   st.ratchet = (uint8_t) juce::jlimit (1, 8,   (int) o->getProperty ("rt"));
        if (o->hasProperty ("cond")) st.cond    = (uint8_t) juce::jlimit (0, 5,   (int) o->getProperty ("cond"));
        if (o->hasProperty ("mic"))  st.micro   = (int8_t)  juce::jlimit (-50, 50, (int) o->getProperty ("mic"));
    }
    else if (k == "lane")
    {
        const int pi = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("p"));
        const int c  = juce::jlimit (0, fmr::NCH - 1,  (int) o->getProperty ("c"));
        auto& L = engine.pat[pi].lane[c];
        if (o->hasProperty ("len")) L.len   = (uint8_t) juce::jlimit (1, fmr::NSTEP, (int) o->getProperty ("len"));
        if (o->hasProperty ("div")) L.div   = (uint8_t) juce::jlimit (0, 3, (int) o->getProperty ("div"));
        if (o->hasProperty ("dir")) L.dir   = (uint8_t) juce::jlimit (0, 3, (int) o->getProperty ("dir"));
        if (o->hasProperty ("sw"))  L.swing = (int8_t)  juce::jlimit (-50, 50, (int) o->getProperty ("sw"));
        if (o->hasProperty ("m"))   L.mute  = (uint8_t) ((int) o->getProperty ("m") ? 1 : 0);
    }
    else if (k == "pat")    { engine.curPat = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("i")); emitSeq(); }
    else if (k == "clearpat")
    {
        engine.pat[engine.curPat] = fmr::Pattern();
        emitSeq();
        notice ("PATTERN CLEARED");
    }
    else if (k == "copypat")
    {
        const int to = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("to"));
        engine.pat[to] = engine.pat[engine.curPat];
        engine.curPat = to;
        emitSeq();
        notice ("PATTERN COPIED");
    }
    else if (k == "seqget") { emitSeq(); }
    else if (k == "transport")
    {
        const juce::String what = o->getProperty ("what").toString();
        /*  ▶ and ⏵ arm, and additionally raise the free-run latch when no host
        transport is rolling — that is the "start it without the DAW" case.
        With a DAW rolling they simply arm and fall into step with it. */
    if (what == "run")        { engine.restartClock(); engine.setFreeRun (! engine.isPlayingHost());
                                setParamById ("seq", 1.0f); notice ("RUNNING"); }
        else if (what == "cont")  { engine.setFreeRun (! engine.isPlayingHost()); setParamById ("seq", 1.0f); notice ("CONTINUE"); }
        else if (what == "pause") { engine.setFreeRun (false); setParamById ("seq", 0.0f); notice ("PAUSED"); }
        else if (what == "stop")  { engine.setFreeRun (false); setParamById ("seq", 0.0f); engine.restartClock(); notice ("STOPPED"); }
        emitKit();
    }
    else if (k == "rec")
    {
        const bool on = (int) o->getProperty ("on") != 0;
        engine.recArm.store (on);
        notice (on ? "RECORD ARMED " + DOT + " PLAY TO WRITE STEPS" : "RECORD OFF");
        emitKit();
    }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan") { presetScan(); }
    else if (k == "presetLoad") { presetLoad (o->getProperty ("path").toString()); }
}

//==============================================================================
void FmrAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        const auto& s = fmr::paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("hi", (double) fmr::paramMax (s));
        e->setProperty ("n", juce::String (s.name));
        e->setProperty ("kind", s.kind);
        e->setProperty ("def", (double) s.def);
        e->setProperty ("lo", (double) s.lo);
        e->setProperty ("fhi", (double) s.hi);
        e->setProperty ("ch", s.chan);
        e->setProperty ("slot", s.slot);
        int nn = 0; auto names = fmr::listNames (s.id, nn);
        if (names != nullptr && nn > 0)
        {
            juce::Array<juce::var> list;
            for (int j = 0; j < nn; ++j) list.add (juce::String (names[j]));
            e->setProperty ("names", list);
        }
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    juce::Array<juce::var> chans, kits, cats;
    for (int c = 0; c < fmr::NCH; ++c)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("id",   juce::String (fmr::channelId (c)));
        e->setProperty ("name", juce::String (fmr::channelName (c)));
        e->setProperty ("fam",  juce::String (fmr::familyName (fmr::channelFamily (c))));
        e->setProperty ("note", fmr::defaultNote (c));
        chans.add (juce::var (e));
    }
    for (int i = 0; i < fmr::numKits(); ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", juce::String (fmr::kitName (i)));
        e->setProperty ("c", fmr::seedCategory (i));
        kits.add (juce::var (e));
    }
    for (int i = 0; i < fmr::numSeedCategories(); ++i) cats.add (juce::String (fmr::seedCategoryName (i)));

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    obj->setProperty ("chans", chans);
    obj->setProperty ("kits", kits);
    obj->setProperty ("cats", cats);
    obj->setProperty ("nstep", fmr::NSTEP);
    obj->setProperty ("npat", fmr::NPAT);
    obj->setProperty ("kit", kitIndex);
   #ifdef FM_BUILD_ID
    obj->setProperty ("build", juce::String (FM_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
    emitSeq();
    emitKit();
}

void FmrAudioProcessor::emitBwfx()
{
    if (emitToUi) emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void FmrAudioProcessor::timerService()
{
    if (! emitToUi) return;

    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // changed parameters
    {
        juce::Array<juce::var> changed;
        for (int i = 0; i < ids.size(); ++i)
        {
            const float v = raw[(size_t) i]->load();
            if (std::abs (v - lastSent[(size_t) i]) > 1.0e-6f)
            {
                lastSent[(size_t) i] = v;
                auto* e = new juce::DynamicObject();
                e->setProperty ("id", ids[i]);
                e->setProperty ("v", (double) v);
                changed.add (juce::var (e));
            }
        }
        if (! changed.isEmpty())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("p", changed);
            emitToUi ("hostParam", juce::var (obj));
        }
    }

    // the strip lamps
    if (++meterTick >= 2)
    {
        meterTick = 0;
        juce::Array<juce::var> lv;
        for (int c = 0; c < fmr::NCH; ++c) lv.add ((double) engine.channelLevel (c));
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("m", lv);
        juce::Array<juce::var> ls;
        for (int c = 0; c < fmr::NCH; ++c) ls.add (engine.seqStepFor (c));
        obj->setProperty ("step", engine.seqStep());
        obj->setProperty ("steps", ls);
        //  whether a host transport exists at all decides who owns SPACE
        obj->setProperty ("xport", engine.hostHasTransport() ? 1 : 0);
        obj->setProperty ("free", engine.isFreeRunning() ? 1 : 0);
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
//  PATCHES ON DISK
//
//  Default folder is "User patches" beside the installed .vst3, which is where
//  the rest of the Brokild plugins put theirs. NOTE: File::hasWriteAccess() is
//  useless on Windows — it answers from the read-only attribute, so Program
//  Files claims to be writable and then refuses. Probe by writing.
//==============================================================================
juce::File FmrAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/Full Metal Racket/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    return brokild::patchFolder ("Full Metal Racket", {}, "*.fmrkit");
}

juce::PropertiesFile& FmrAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "FullMetalRacket";
        o.filenameSuffix = "settings";
        o.folderName = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File FmrAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder.isDirectory()) return presetFolder;
    //  ...unless it is one the plugin wrote down for itself, in a place an
    //  installer replaces — which is how the first two kits were lost.
    const auto remembered = userSettings().getValue ("patchFolder");
    if (remembered.isNotEmpty() && juce::File (remembered).isDirectory()
        && ! brokild::isUnsafePatchFolder (juce::File (remembered)))
        return (presetFolder = juce::File (remembered));

    auto beside = installedPresetFolder();
    if (beside != juce::File())
    {
        beside.createDirectory();
        auto probe = beside.getChildFile (".fmr-write-probe");
        if (probe.replaceWithText ("ok")) { probe.deleteFile(); return (presetFolder = beside); }
    }
    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("Full Metal Racket");
    docs.createDirectory();
    return (presetFolder = docs);
}

void FmrAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    presetFolder = dir;
    userSettings().setValue ("patchFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

void FmrAudioProcessor::presetScan()
{
    if (! emitToUi) return;
    auto dir = presetFolderOrDefault();
    juce::Array<juce::var> files;
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.fmrkit"))
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", f.getFileNameWithoutExtension());
        e->setProperty ("p", f.getFullPathName());
        //  the subfolder, so the browser can group them the way the folder is
        const auto rel = f.getParentDirectory().getRelativePathFrom (dir);
        e->setProperty ("sub", rel == "." ? juce::String()
                                          : rel.replaceCharacter (juce::File::getSeparatorChar(), '/'));
        files.add (juce::var (e));
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("dir", dir.getFullPathName());
    o->setProperty ("files", files);
    emitToUi ("patches", juce::var (o));
}

juce::String FmrAudioProcessor::patchJson (const juce::String& name)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "fmrkit");
    root->setProperty ("version", 1);
    root->setProperty ("name", name);
   #ifdef FM_BUILD_ID
    root->setProperty ("build", juce::String (FM_BUILD_ID));
   #endif
    auto* ps = new juce::DynamicObject();
    for (int i = 0; i < fmr::numParams(); ++i)
        ps->setProperty (fmr::paramSpec (i).id, (double) raw[(size_t) i]->load());
    root->setProperty ("params", juce::var (ps));
    root->setProperty ("seq", seqVar());
    root->setProperty ("kits", kitsVar());
    root->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (root), false);
}

void FmrAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    const auto v = juce::JSON::parse (json);
    auto* o = v.getDynamicObject();
    if (o == nullptr) { notice ("NOT A PATCH FILE"); return; }

    if (auto* ps = o->getProperty ("params").getDynamicObject())
        for (int i = 0; i < fmr::numParams(); ++i)
        {
            const auto& s = fmr::paramSpec (i);
            //  a patch written by an older build simply has fewer keys, and
            //  anything it does not name keeps its current value
            if (ps->hasProperty (s.id))
                setParamById (s.id, (float) (double) ps->getProperty (s.id));
        }
    seqApply  (o->getProperty ("seq"));
    kitsApply (o->getProperty ("kits"));
    bwfxRack.fromJson (o->getProperty ("bwfx").toString().toStdString());
    patchName = name;
    /*  Put the free clock back to the top so a load always begins at step
        one. With a host rolling the bar governs and this changes nothing. */
    engine.restartClock();
    emitBwfx(); emitSeq(); emitKit();
    notice ("LOADED " + name.toUpperCase());
}

void FmrAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    activeChooser = std::make_unique<juce::FileChooser> (
        "Save kit", dir.getChildFile (juce::String (fmr::kitName (kitIndex)) + ".fmrkit"), "*.fmrkit");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("fmrkit")) f = f.withFileExtension ("fmrkit");
            if (f.replaceWithText (patchJson (f.getFileNameWithoutExtension())))
            {
                //  remember a NEW folder, but not a subfolder of the current one
                auto d = f.getParentDirectory();
                if (! d.isAChildOf (presetFolderOrDefault())) rememberPresetFolder (d);
                /*  What is on the machine IS that file from the moment it is
                    written; the display should not wait until you load it back
                    to agree with that. */
                patchName = f.getFileNameWithoutExtension();
                notice ("SAVED " + patchName.toUpperCase());
                emitKit();
                presetScan();
            }
            else notice ("COULD NOT WRITE THAT FILE");
        });
}

void FmrAudioProcessor::presetOpenDialog()
{
    activeChooser = std::make_unique<juce::FileChooser> ("Open kit", presetFolderOrDefault(), "*.fmrkit");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) presetLoad (f.getFullPathName());
        });
}

void FmrAudioProcessor::presetLoad (const juce::String& path)
{
    juce::File f (path);
    if (! f.existsAsFile()) { notice ("NO SUCH FILE"); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FmrAudioProcessor();
}
