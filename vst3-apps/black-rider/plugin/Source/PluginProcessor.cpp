#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    juce::String fmtValue (const bk::PSpec& s, float v)
    {
        using namespace bk;
        switch (s.kind)
        {
            case KP_SW:    return v >= 0.5f ? "ON" : "OFF";
            case KP_HZ:
            {
                const float f = xmap (v, s.lo, s.hi);
                return f < 100.0f ? juce::String (f, 2) + " Hz"
                     : f < 1000.0f ? juce::String (f, 0) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
            }
            case KP_MS:
            {
                const float ms = xmap (v, s.lo, s.hi);
                return ms < 100.0f ? juce::String (ms, 1) + " ms"
                     : ms < 1000.0f ? juce::String (ms, 0) + " ms" : juce::String (ms / 1000.0f, 2) + " s";
            }
            case KP_GLIDE:
            {
                const float ms = glideMs (v);
                return ms <= 0.0f ? "OFF" : (ms < 1000.0f ? juce::String (ms, 0) + " ms" : juce::String (ms / 1000.0f, 2) + " s");
            }
            case KP_LIST:
            {
                int n = 0; auto names = listNames (s.id, n);
                const int i = juce::jlimit (0, std::max (0, n - 1), (int) std::round (v));
                return names != nullptr && n > 0 ? juce::String (names[i]) : juce::String (i);
            }
            case KP_INT:
            {
                const int i = (int) std::round (v);
                if (juce::String (s.id).endsWith ("src")) return juce::String (sourceName (i));
                if (juce::String (s.id).endsWith ("dst")) return juce::String (destName (i));
                return juce::String (i) + (juce::String (s.id) == "bend" ? " semi" : "");
            }
            case KP_SEMI:  { const int st = (int) std::round ((v - 0.5f) * 24.0f); return (st > 0 ? "+" : "") + juce::String (st) + " semi"; }
            case KP_CENT:  { const float c = (v - 0.5f) * s.lo; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " cents"; }
            case KP_CENTU: return juce::String (v * s.lo, 0) + " cents";
            case KP_BIPOL: { const float c = (v - 0.5f) * 200.0f; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " %"; }
            case KP_VOL:   { const float g = v < 0.005f ? 0.0f : 2.0f * v * v; return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB"; }
            case KP_PW:    return juce::String (50.0f + v * 45.0f, 0) + " %";
            default:       return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BlackRiderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < bk::numParams(); ++i)
    {
        const auto& s = bk::paramSpec (i);
        const float hi = bk::paramMax (s);
        const bool stepped = s.kind == bk::KP_LIST || s.kind == bk::KP_INT || s.kind == bk::KP_SW;
        const juce::String sid (s.id);
        const bool automatable = ! (sid == "os" || sid == "seed");   // selectors, not lanes

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (bk::paramSpec (i), v); })));
    }
    //  the rack's five automatable macros, declared by shared code so
    //  every synth carries the identical five (see bwfx_juce.h)
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
BlackRiderAudioProcessor::BlackRiderAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BLACKRIDER", createParameterLayout())
{
    for (int i = 0; i < bk::numParams(); ++i) ids.add (bk::paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);
    for (auto& h : midiHeld) h = false;

    /*  A fresh instance opens on seed 0 so the LED and the sound agree from the
        first frame; a project load calls setStateInformation and replaces it. */
    applySeed (0);

    startTimerHz (15);        // bwfxRack.service() — editor open or not
    bwfxRack.setWorldModConsumed (true);   // this engine maps the SPECTRA bus
}

BlackRiderAudioProcessor::~BlackRiderAudioProcessor() = default;

bool BlackRiderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BlackRiderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < bk::numParams(); ++i)
        bk::paramSpec (i).get (engine.p) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

void BlackRiderAudioProcessor::pushEvent (const UiEvent& e)
{
    const int w = evWrite.load();
    const int next = (w + 1) % EVQ;
    if (next == evRead.load()) return;          // full: drop, never block
    evq[(size_t) w] = e;
    evWrite.store (next);
}

//==============================================================================
void BlackRiderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) { engine.reset(); for (auto& h : midiHeld) h = false; }

    for (int i = 0; i < bk::numParams(); ++i)
        bk::paramSpec (i).get (engine.p) = raw[(size_t) i]->load();

    // host tempo, for the LFO and delay sync (bpm is not a table param)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 999.0)
                {
                    engine.p.bpm = *bpm;
                    bwfxRack.setBpm (*bpm);   // the world rack syncs too
                }

    // SPECTRA world-mod bus: the rack's characters possess the engine.
    // One block of modulation latency; a neutral bus is bit-identical.
    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth,
                            wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    // the page's keyboard and wheels
    while (evRead.load() != evWrite.load())
    {
        const auto e = evq[(size_t) evRead.load()];
        evRead.store ((evRead.load() + 1) % EVQ);
        switch (e.kind)
        {
            case 1: engine.noteOn (e.note, e.v); midiHeld[(size_t) juce::jlimit (0, 127, e.note)] = true; break;
            case 2: engine.noteOff (e.note); midiHeld[(size_t) juce::jlimit (0, 127, e.note)] = false; break;
            case 3: engine.setBend (e.v); hostBend = e.v; break;
            case 4: engine.setWheel (e.v); hostWheel = e.v; break;
            case 5: engine.allNotesOff(); for (auto& h : midiHeld) h = false; break;
            default: break;
        }
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = nch >= 2 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float> monoR;
    if (R == nullptr) { monoR.assign ((size_t) n, 0.0f); R = monoR.data(); }

    // render between MIDI events so a note lands where it was played
    int last = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (pos > last) { engine.process (L + last, R + last, pos - last); last = pos; }

        const auto m = meta.getMessage();
        if      (m.isNoteOn())        { engine.noteOn (m.getNoteNumber(), m.getFloatVelocity()); midiHeld[(size_t) m.getNoteNumber()] = true; }
        else if (m.isNoteOff())       { engine.noteOff (m.getNoteNumber()); midiHeld[(size_t) m.getNoteNumber()] = false; }
        else if (m.isPitchWheel())    { const float b = (m.getPitchWheelValue() - 8192) / 8192.0f; engine.setBend (b); hostBend = b; }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc == 1)       { engine.setWheel (v / 127.0f); hostWheel = v / 127.0f; }
            else if (cc == 64) engine.setSustain (v >= 64);
            else if (cc == 120 || cc == 123) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
        }
        else if (m.isChannelPressure()) { engine.setAftertouch (m.getChannelPressureValue() / 127.0f); hostAt = m.getChannelPressureValue() / 127.0f; }
        else if (m.isAftertouch())      { engine.setAftertouch (m.getAfterTouchValue() / 127.0f); hostAt = m.getAfterTouchValue() / 127.0f; }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
    }
    if (n > last) engine.process (L + last, R + last, n - last);

    // The world rack: one extra stage after the engine. Empty = returns
    // without touching the buffers (bit-identical, the additive contract).
    bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros
    bwfxRack.process (L, R, n);

    if (nch == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
void BlackRiderAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        // the world rack rides as one attribute; old builds simply ignore it
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        copyXmlToBinary (*xml, dest);
    }
}

void BlackRiderAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            // pre-BWFX projects have no attribute -> the default empty rack
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            xml->removeAttribute ("bwfx");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
        }
}

juce::AudioProcessorEditor* BlackRiderAudioProcessor::createEditor()
{
    return new BlackRiderAudioProcessorEditor (*this);
}

//==============================================================================
void BlackRiderAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void BlackRiderAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

void BlackRiderAudioProcessor::applyParamsStruct (const bk::Params& q)
{
    bk::Params copy = q;
    for (int i = 0; i < bk::numParams(); ++i)
    {
        const auto& s = bk::paramSpec (i);
        const juce::String id (s.id);
        if (id == "os" || id == "volume" || id == "seed") continue;   // selectors / master, not a patch's character
        setParamById (s.id, s.get (copy));
    }
}

/*  Dial a seed: the whole two-hundred-patch library is deterministic, so this
    generates the patch, pushes every value through setParamById (the cables
    included), stamps the seed on the dial, and tells the panel its name. */
void BlackRiderAudioProcessor::applySeed (int seed)
{
    seed = juce::jlimit (0, bk::NUM_SEEDS - 1, seed);
    bk::Params q;
    bk::seedPatch (seed, q);
    applyParamsStruct (q);
    bwfxRack.clearState();                   // a patch stores its own rack; seeds carry none
    emitBwfx();
    setParamById ("seed", (float) seed);
    lastSeedSent = -1;                       // force a fresh seedinfo on the next tick
    emitSeedInfo (seed);
    notice (juce::String ("SEED ") + juce::String (seed).paddedLeft ('0', 3) + " "
            + DOT + " " + juce::String (bk::seedName (seed)).toUpperCase() + " " + DOT + " " + bk::seedCategoryName (seedCatOf (seed)));
}

int BlackRiderAudioProcessor::seedCatOf (int seed)
{
    int c = 0, rank = 0; bk::seedInfo (seed, c, rank); return c;
}

void BlackRiderAudioProcessor::emitSeedInfo (int seed)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("n", seed);
    o->setProperty ("name", juce::String (bk::seedName (seed)));
    o->setProperty ("cat", juce::String (bk::seedCategoryName (seedCatOf (seed))));
    emitToUi ("seedinfo", juce::var (o));
    lastSeedSent = seed;
}

/*  Random makes a playable instrument, not a random number field: the
    envelopes, the filter and the oscillators are rolled inside musical
    ranges, the performance controls and the master are left alone, and at
    most two cables go in. */
void BlackRiderAudioProcessor::randomiseAll()
{
    juce::Random r (juce::Time::getHighResolutionTicks());
    auto set  = [this] (const char* id, float v) { setParamById (id, v); };
    auto roll = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto pick = [&r] (int n) { return (float) r.nextInt (n); };

    set ("mode", pick (3)); set ("udet", roll (0.1f, 0.6f)); set ("spread", roll (0.4f, 1.0f));
    set ("glide", r.nextFloat() < 0.35f ? roll (0.15f, 0.45f) : 0.0f);
    set ("o1wave", pick (4)); set ("o1oct", 1 + pick (2)); set ("o1semi", 0.5f); set ("o1fine", roll (0.47f, 0.53f));
    set ("o1pw", roll (0.0f, 0.8f)); set ("o1pwm", r.nextFloat() < 0.4f ? roll (0.2f, 0.7f) : 0.0f); set ("o1lvl", roll (0.5f, 0.9f));
    set ("o2wave", pick (4)); set ("o2oct", pick (4));
    static const float semis[] = { 0.5f, 0.5f, 0.5f, 0.5f + 7.0f / 24.0f, 0.5f + 5.0f / 24.0f, 0.5f + 12.0f / 24.0f, 0.5f - 12.0f / 24.0f, 0.5f + 3.0f / 24.0f };
    set ("o2semi", semis[r.nextInt (8)]); set ("o2fine", roll (0.4f, 0.6f));
    set ("o2pw", roll (0.0f, 0.8f)); set ("o2pwm", r.nextFloat() < 0.3f ? roll (0.2f, 0.7f) : 0.0f); set ("o2lvl", roll (0.0f, 0.9f));
    set ("o2sync", r.nextFloat() < 0.2f ? 1.0f : 0.0f); set ("o2fm", r.nextFloat() < 0.2f ? roll (0.1f, 0.6f) : 0.0f); set ("o2kbd", 1.0f);
    set ("suboct", pick (2)); set ("sublvl", r.nextFloat() < 0.5f ? roll (0.1f, 0.6f) : 0.0f);
    set ("nzcol", pick (2)); set ("nzlvl", r.nextFloat() < 0.3f ? roll (0.05f, 0.3f) : 0.0f);
    set ("ringlvl", r.nextFloat() < 0.15f ? roll (0.2f, 0.7f) : 0.0f); set ("fdrive", roll (0.0f, 0.6f));
    set ("fmodel", pick (3)); set ("hpf", r.nextFloat() < 0.3f ? roll (0.0f, 0.45f) : 0.0f); set ("hpeak", r.nextFloat() < 0.3f ? roll (0.0f, 0.6f) : 0.0f);
    set ("lpf", roll (0.25f, 0.8f)); set ("lpeak", roll (0.0f, 0.9f)); set ("fenv", roll (0.4f, 0.9f)); set ("fkey", roll (0.0f, 1.0f)); set ("flfo", r.nextFloat() < 0.3f ? roll (0.05f, 0.4f) : 0.0f);
    set ("e1a", roll (0.0f, 0.45f)); set ("e1d", roll (0.2f, 0.65f)); set ("e1s", roll (0.0f, 0.8f)); set ("e1r", roll (0.15f, 0.6f)); set ("e1vel", roll (0.0f, 0.8f));
    set ("e2a", roll (0.0f, 0.4f)); set ("e2d", roll (0.2f, 0.6f)); set ("e2s", roll (0.2f, 1.0f)); set ("e2r", roll (0.15f, 0.6f)); set ("e2vel", roll (0.2f, 0.8f));
    set ("lwave", pick (6)); set ("lrate", roll (0.25f, 0.6f)); set ("lpitch", r.nextFloat() < 0.25f ? roll (0.02f, 0.15f) : 0.0f);
    set ("lsync", 0.0f); set ("lfeel", 0.0f); set ("dlsync", 0.0f); set ("dlfeel", 0.0f);
    set ("vcamode", r.nextFloat() < 0.1f ? 2.0f : 0.0f);
    set ("drv", r.nextFloat() < 0.4f ? roll (0.05f, 0.5f) : 0.0f); set ("drvtone", roll (0.3f, 0.9f));
    set ("chmix", r.nextFloat() < 0.4f ? roll (0.2f, 0.6f) : 0.0f); set ("chdepth", roll (0.3f, 0.8f)); set ("chrate", roll (0.2f, 0.6f));
    set ("dlmix", r.nextFloat() < 0.4f ? roll (0.1f, 0.4f) : 0.0f); set ("dlfb", roll (0.2f, 0.6f)); set ("dltime", roll (0.4f, 0.75f)); set ("dlwow", roll (0.1f, 0.6f));
    set ("spmix", r.nextFloat() < 0.4f ? roll (0.1f, 0.45f) : 0.0f); set ("spdwell", roll (0.3f, 0.8f));
    for (int c = 1; c <= bk::NUM_CABLES; ++c)
    {
        const juce::String cs = "c" + juce::String (c);
        const bool patch = c <= 2 && r.nextFloat() < 0.45f;
        static const int srcs[] = { bk::S_EG1, bk::S_LFO, bk::S_SH, bk::S_VCO2, bk::S_KEY, bk::S_VEL, bk::S_NOISE };
        static const int dsts[] = { bk::D_P2, bk::D_PW1, bk::D_PW2, bk::D_LPF, bk::D_LPEAK, bk::D_PAN, bk::D_HPF, bk::D_FM2, bk::D_DRIVE, bk::D_DLYT };
        setParamById (cs + "src", patch ? (float) srcs[r.nextInt (7)] : 0.0f);
        setParamById (cs + "dst", patch ? (float) dsts[r.nextInt (10)] : 0.0f);
        setParamById (cs + "amt", patch ? roll (0.15f, 0.85f) : 0.5f);
    }
    notice ("A NEW MACHINE " + DOT + " RANDOM DIALLED");
}

//==============================================================================
void BlackRiderAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* o = payload.getDynamicObject())
    {
        const auto batch = o->getProperty ("b");
        if (auto* arr = batch.getArray())
        {
            for (const auto& m : *arr) handleOne (m);
            return;
        }
    }
    handleOne (payload);
}

void BlackRiderAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")
    {
        setParamById (o->getProperty ("id").toString(), (float) (double) o->getProperty ("v"), true);
    }
    else if (k == "ack")    { uiHasState = true; }
    else if (k == "ready")  { uiReady = true; }
    else if (k == "hello")  { uiHasState = false; }
    else if (k == "random") { juce::Random r (juce::Time::getHighResolutionTicks()); applySeed (r.nextInt (bk::NUM_SEEDS)); }
    else if (k == "seed")   { applySeed ((int) o->getProperty ("n")); }
    else if (k == "wildcard") { randomiseAll(); }   // the free, uncategorised roll, kept for the curious
    else if (k == "panic")  { wantPanic = true; notice ("ALL NOTES OFF " + DOT + " RESET"); }
    else if (k == "note")
    {
        UiEvent e; e.kind = (bool) o->getProperty ("on") ? 1 : 2; e.note = (int) o->getProperty ("n");
        e.v = o->hasProperty ("v") ? (float) (double) o->getProperty ("v") : 0.8f;
        pushEvent (e);
    }
    else if (k == "bend")   { UiEvent e; e.kind = 3; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "wheel")  { UiEvent e; e.kind = 4; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "alloff") { UiEvent e; e.kind = 5; pushEvent (e); }
    else if (k == "wide")   { const bool on = (bool) o->getProperty ("on"); uiWide = on; if (onWide) onWide (on); }
    else if (k == "recipe")
    {
        const int i = (int) o->getProperty ("i");
        bk::Params q;
        bk::applyRecipe (i, q);
        applyParamsStruct (q);
        notice (juce::String ("PATCH ") + bk::recipeName (i) + " " + DOT + " " + juce::String (bk::recipeBlurb (i)).toUpperCase());
    }
    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan")   { presetScan(); }
    else if (k == "presetFolder") { presetPickFolder(); }
    else if (k == "presetLoad")   { presetLoad (o->getProperty ("path").toString()); }
}

//==============================================================================
void BlackRiderAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        const auto& s = bk::paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("hi", (double) bk::paramMax (s));
        e->setProperty ("n", juce::String (s.name));
        e->setProperty ("kind", s.kind);
        e->setProperty ("def", (double) s.def);
        e->setProperty ("lo", (double) s.lo);
        e->setProperty ("fhi", (double) s.hi);
        int nn = 0; auto names = bk::listNames (s.id, nn);
        if (names != nullptr && nn > 0)
        {
            juce::Array<juce::var> list;
            for (int j = 0; j < nn; ++j) list.add (juce::String (names[j]));
            e->setProperty ("names", list);
        }
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    juce::Array<juce::var> recipes, blurbs, srcs, dsts, cats, seeds;
    for (int i = 0; i < bk::NUM_RECIPES; ++i) { recipes.add (juce::String (bk::recipeName (i))); blurbs.add (juce::String (bk::recipeBlurb (i))); }
    for (int i = 0; i < bk::NUM_SOURCES; ++i) srcs.add (juce::String (bk::sourceName (i)));
    for (int i = 0; i < bk::NUM_DESTS; ++i)   dsts.add (juce::String (bk::destName (i)));
    for (int i = 0; i < bk::seedNumCategories(); ++i) cats.add (juce::String (bk::seedCategoryName (i)));
    for (int i = 0; i < bk::NUM_SEEDS; ++i)
    {
        int c = 0, rank = 0; bk::seedInfo (i, c, rank);
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", i);
        e->setProperty ("name", juce::String (bk::seedName (i)));
        e->setProperty ("c", c);
        seeds.add (juce::var (e));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    obj->setProperty ("recipes", recipes);
    obj->setProperty ("blurbs", blurbs);
    obj->setProperty ("srcs", srcs);
    obj->setProperty ("dsts", dsts);
    obj->setProperty ("cats", cats);
    obj->setProperty ("seeds", seeds);
    obj->setProperty ("cables", bk::NUM_CABLES);
   #ifdef BK_BUILD_ID
    obj->setProperty ("build", juce::String (BK_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
}

void BlackRiderAudioProcessor::emitBwfx()
{
    if (emitToUi)
        emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void BlackRiderAudioProcessor::timerService()
{
    if (! emitToUi) return;

    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    {
        juce::Array<juce::var> changed;
        for (int i = 0; i < ids.size(); ++i)
        {
            const float v = raw[(size_t) i]->load();
            if (std::abs (v - lastSent[(size_t) i]) > 1.0e-5f)
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

    // ---- meters -----------------------------------------------------------
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("out",  (double) std::sqrt (std::max (0.0f, engine.outRms)));
        obj->setProperty ("peak", (double) engine.outPeak);
        obj->setProperty ("e1",   (double) engine.uiEnv1);
        obj->setProperty ("e2",   (double) engine.uiEnv2);
        obj->setProperty ("lfo",  (double) engine.uiLfo);
        obj->setProperty ("cut",  (double) engine.uiCut);
        obj->setProperty ("lock", engine.locked);
        obj->setProperty ("bend", (double) hostBend.load());
        obj->setProperty ("wheel",(double) hostWheel.load());
        obj->setProperty ("at",   (double) hostAt.load());

        juce::Array<juce::var> notes, pans, held, scope;
        for (int i = 0; i < bk::MAX_VOICES; ++i) { notes.add (engine.uiNotes[(size_t) i]); pans.add ((double) engine.uiPan[(size_t) i]); }
        for (int n = 0; n < 128; ++n) if (midiHeld[(size_t) n]) held.add (n);
        // the last 1024 samples of the output, oldest first; the page triggers it
        const int w = engine.scopeWrite;
        for (int i = 0; i < 1024; ++i)
            scope.add ((double) engine.scope[(size_t) ((w - 1024 + i) & (bk::SCOPE_N - 1))]);
        obj->setProperty ("notes", notes);
        obj->setProperty ("pans", pans);
        obj->setProperty ("held", held);
        obj->setProperty ("scope", scope);
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
// Patches on disk — the same shape as the other Brokild plugins.
namespace
{
    bool canWriteInto (const juce::File& dir)
    {
        if (! dir.isDirectory()) return false;
        const auto probe = dir.getChildFile (".blackrider-write-test.tmp");
        if (! probe.replaceWithText ("x")) return false;
        probe.deleteFile();
        return true;
    }
}

juce::PropertiesFile& BlackRiderAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "BlackRider";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File BlackRiderAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/Black Rider/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    return brokild::patchFolder ("Black Rider", { "\"blackrider\"" });
}

juce::File BlackRiderAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder == juce::File{})
    {
        const auto saved = userSettings().getValue ("presetFolder", {});
        //  ...unless it is one the plugin wrote down for itself, in a
        //  place an installer replaces. See brokild_paths.h.
        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved)
            && ! brokild::isUnsafePatchFolder (juce::File (saved)))
            presetFolder = juce::File (saved);
        else
        {
            presetFolder = installedPresetFolder();
            if (presetFolder == juce::File{})
                presetFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile ("Black Rider Presets");
        }
    }
    return presetFolder;
}

void BlackRiderAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var BlackRiderAudioProcessor::presetScanDir (const juce::File& dir, int depth)
{
    juce::Array<juce::var> items;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto f = e.getFile();
        if (f.isHidden()) continue;
        if (f.isDirectory())
        {
            if (depth >= 2) continue;
            auto kids = presetScanDir (f, depth + 1);
            if (kids.size() == 0) continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileName());
            o->setProperty ("d", true);
            o->setProperty ("i", kids);
            items.add (juce::var (o));
        }
        else if (f.hasFileExtension ("json"))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileNameWithoutExtension());
            o->setProperty ("p", f.getFullPathName());
            items.add (juce::var (o));
        }
    }
    struct Sorter
    {
        static int compareElements (const juce::var& a, const juce::var& b)
        {
            const bool da = (bool) a.getProperty ("d", false);
            const bool db = (bool) b.getProperty ("d", false);
            if (da != db) return da ? -1 : 1;
            return a.getProperty ("n", "").toString().compareNatural (b.getProperty ("n", "").toString());
        }
    };
    Sorter sorter;
    items.sort (sorter);
    return juce::var (items);
}

void BlackRiderAudioProcessor::presetScan()
{
    if (! emitToUi) return;
    const auto dir = presetFolderOrDefault();
    const bool there = dir.isDirectory();
    auto* o = new juce::DynamicObject();
    o->setProperty ("folder", dir.getFullPathName());
    o->setProperty ("exists", there);
    o->setProperty ("items", there ? presetScanDir (dir, 0) : juce::var (juce::Array<juce::var>()));
    emitToUi ("presetTree", juce::var (o));
}

void BlackRiderAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Where the patches live", start);
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File{} || ! dir.isDirectory()) return;
            rememberPresetFolder (dir);
            presetScan();
        });
}

juce::String BlackRiderAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i)
        pv->setProperty (ids[i], (double) raw[(size_t) i]->load());

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "blackrider");
    o->setProperty ("kind", "preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef BK_BUILD_ID
    o->setProperty ("build", juce::String (BK_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));   // a patch stores its own rack
    return juce::JSON::toString (juce::var (o), false);
}

void BlackRiderAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject())
    { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "blackrider")
    { notice ("THAT IS NOT A BLACK RIDER PATCH"); return; }

    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
            if (apvts.getParameter (kv.name.toString()) != nullptr)
            { setParamById (kv.name.toString(), (float) (double) kv.value); ++applied; }

    // a patch stores its own rack; pre-BWFX patches come rack-empty
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    emitBwfx();

    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("LOADED \"" + name.toUpperCase() + "\" " + DOT + " " + juce::String (applied) + " VALUES");
}

void BlackRiderAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const auto suggested = dir.getChildFile ("Ride.json");

    activeChooser = std::make_unique<juce::FileChooser> ("Save this patch", suggested, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            if (! file.hasFileExtension ("json")) file = file.withFileExtension ("json");
            file.getParentDirectory().createDirectory();

            const auto name = file.getFileNameWithoutExtension();
            const bool ok = file.replaceWithText (patchJson (name));
            if (ok)
            {
                const auto chosen = file.getParentDirectory();
                const auto root = presetFolderOrDefault();
                if (chosen != root && ! chosen.isAChildOf (root)) rememberPresetFolder (chosen);
            }
            notice (ok ? "SAVED \"" + name.toUpperCase() + "\"" : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void BlackRiderAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Open a patch", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void BlackRiderAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlackRiderAudioProcessor();
}
