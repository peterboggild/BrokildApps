#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"

using namespace ab104;

//==============================================================================
static juce::String fmtValue (const PSpec& s, float v)
{
    switch (s.kind)
    {
        case KP_KELVIN:
            return juce::String ((int) std::lround (ambientKelvin (v))) + " K";
        case KP_SEMI:
            return juce::String (s.lo + (s.hi - s.lo) * v, 1) + " st";
        case KP_BIPOL:
            return juce::String ((int) std::round ((v * 2.0f - 1.0f) * 100.0f)) + " %";
        case KP_VOL:
        {
            const float g = 2.4f * v * v;
            return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB";
        }
        default: return juce::String ((int) std::round (v * 100.0f)) + " %";
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Artefact104AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (paramSpec (i), v); })));
    }
    /*  SPECIMEN is deliberately not among them. Dialling one rebuilds the
        whole web, and a lane fighting that is unusable — the mood-organ
        precedent from Blade Ruiner and HABIT from B2311.67. */
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
Artefact104AudioProcessor::Artefact104AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ARTEFACT104", createParameterLayout())
{
    for (int i = 0; i < numParams(); ++i) ids.add (paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    startTimerHz (30);
    bwfxRack.setWorldModConsumed (true);   // this engine maps the SPECTRA bus
    ambientIdx = ids.indexOf ("ambient");
    site.open (104);                        // join the bench (harmless if it fails)
}

Artefact104AudioProcessor::~Artefact104AudioProcessor() { site.close(); }

bool Artefact104AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Artefact104AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

//==============================================================================
void Artefact104AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) engine.allNotesOff();

    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();

    //  the host transport rides in to the rack (tempo-synced modules) and the
    //  engine; the object leans on nothing, but SPECTRA's synced modes want it
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    engine.setTransport (bpm, ppq, playing);
    bwfxRack.setTransport (bpm, ppq, playing);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())            engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
        else if (m.isPitchWheel())   engine.setBend ((m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
        else if (m.isSustainPedalOn())  { sustainOn = true;  engine.setSustain (true); }
        else if (m.isSustainPedalOff()) { sustainOn = false; engine.setSustain (false); }
    }

    //  SPECTRA: the world-mod bus into the conduits, keyed to the gate inside
    const bwfx::WorldMod wm = bwfxRack.worldMod();
    engine.setWorldMod (wm.detuneCents, wm.pitchSag, wm.tremDepth,
                        wm.tremRate, wm.filterMul, wm.panSpread);

    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : L;
    if (nch > 1) engine.process (L, R, n);
    else { std::vector<float> tmp ((size_t) n); engine.process (L, tmp.data(), n); }

    bwfxRack.process (L, R, n);
    bwfx_juce::pushMacros (bwfxRack, apvts);       // the five host macros
    if (nch > 2) for (int c = 2; c < nch; ++c) buffer.clear (c, 0, n);
}

//==============================================================================
void Artefact104AudioProcessor::timerCallback()
{
    engine.service();
    bwfxRack.service();
    siteStep();
    emitParamEcho();
    emitWeb();
}

//==============================================================================
static float ambientPosFor (double kelvin)
{
    const double k = juce::jlimit (ab104::T_AMB_LO, ab104::T_AMB_HI, kelvin);
    return (float) (std::log (k / ab104::T_AMB_LO) / std::log (ab104::T_AMB_HI / ab104::T_AMB_LO));
}

void Artefact104AudioProcessor::siteStep()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    double dt = siteLastMs > 0.0 ? (nowMs - siteLastMs) * 0.001 : 1.0 / 30.0;
    siteLastMs = nowMs;
    dt = juce::jlimit (0.0, 0.25, dt);

    const float amb    = ambientIdx >= 0 ? raw[(size_t) ambientIdx]->load() : 0.4748f;
    const float warmth = juce::jlimit (0.0f, 1.0f, amb);
    const float myK    = (float) ab104::ambientKelvin (amb);
    siteLocalK = myK;

    //  an offline render must be deterministic: the bench is not consulted
    if (isNonRealtime() || ! site.isOpen())
    {
        sitePull = 0.0f;
        engine.setSite ((float) sitePhase, 0.5f, 0.0f);
        return;
    }
    if (siteLastLocalK < 0.0f) siteLastLocalK = myK;
    ++siteTick;
    /*  SETTLING. For the first second the host is still restoring state, and
        a parameter jumping from its default to the saved value is not a
        player's move — an instance that proposed it would rewrite the whole
        bench on every project load. So: no proposals until settled, and while
        settling the bench's climate, if it has one, overrides whatever the
        restore brings in. */
    const bool settled = siteTick > 25;

    const float activity = juce::jlimit (0.0f, 1.0f, engine.gridPower.load() * 1.5f);
    const auto v = site.sync (activity, myK, (float) sitePhase, warmth);

    /*  THE CLIMATE. Whoever moves their own ambient moves the site's; the
        others follow through their host parameter, so the move is visible,
        automatable and undoable in every DAW. The guard is the applied value:
        a follow is not a proposal, or two instances would chase each other
        round the rounding error forever. */
    if (v.climateShared)
    {
        const bool iMoved = std::abs (myK - siteLastLocalK) > 0.5f
                         && std::abs (myK - siteAppliedK) > 0.5f;
        const bool follow = v.climateMoved || ! settled;
        /*  SEED. A bench that nobody has spoken to has no temperature, and
            until someone moves a slider nothing converges — findings loaded
            into one project sit at their own values with CLIMATE plainly
            switched on, which reads as the sharing being broken. So the first
            settled finding to find an empty bench gives it its own. */
        if (v.siteKelvin <= 0.0f && settled)
        {
            site.proposeKelvin (myK);
            siteAppliedK = myK; siteLastLocalK = myK;
        }
        else if (iMoved && settled)
        {
            site.proposeKelvin (myK);
            siteAppliedK = myK;
        }
        else if (follow && v.siteKelvin > 0.0f && std::abs (v.siteKelvin - myK) > 0.5f)
        {
            siteAppliedK = v.siteKelvin;
            siteLastLocalK = v.siteKelvin;
            setParamById ("ambient", ambientPosFor (v.siteKelvin), false);
        }
    }
    if (std::abs (myK - siteLastLocalK) > 0.5f) siteLastLocalK = myK;

    //  THE TIMING: Kuramoto, cold and close
    const float pull = proxima::Client::pullStrength (v, warmth);
    sitePhase = proxima::Client::stepPhase ((float) sitePhase, site.naturalHz(), dt, v, pull);
    engine.setSite ((float) sitePhase, v.pulseHz, pull);

    siteCoh = v.coherence; siteOthers = v.others; sitePull = pull;
    siteK = (v.climateShared && v.siteKelvin > 0.0f) ? v.siteKelvin : myK;
    if (siteTick % 8 == 0) emitSite();
}

void Artefact104AudioProcessor::emitSite()
{
    if (! emitToUi) return;
    const auto s = site.settings();
    auto* o = new juce::DynamicObject();
    o->setProperty ("open",     site.isOpen());
    o->setProperty ("climate",  s.climate);
    o->setProperty ("timing",   s.timing);
    o->setProperty ("distance", s.distance);
    o->setProperty ("others",   siteOthers);
    o->setProperty ("coh",      siteCoh);
    o->setProperty ("pull",     sitePull);
    o->setProperty ("kelvin",   siteK);
    o->setProperty ("local",    siteLocalK);
    o->setProperty ("phase",    (float) sitePhase);
    emitToUi ("site", juce::var (o));
}

void Artefact104AudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        const float v = juce::jlimit (0.0f, 1.0f, value);
        //  the page's own move needs no echo (it already shows it)
        if (fromUi)
        {
            const int idx = ids.indexOf (id);
            if (idx >= 0 && idx < (int) lastSent.size()) lastSent[(size_t) idx] = v;
        }
        prm->setValueNotifyingHost (v);
    }
}

void Artefact104AudioProcessor::emitParamEcho()
{
    if (! emitToUi || ! uiHasState.load()) return;
    if (lastSent.size() != (size_t) ids.size())
    {
        //  the baseline is whatever the page was just handed in initialState
        lastSent.resize ((size_t) ids.size());
        for (int i = 0; i < ids.size(); ++i) lastSent[(size_t) i] = raw[(size_t) i]->load();
        return;
    }
    juce::Array<juce::var> b;
    for (int i = 0; i < ids.size(); ++i)
    {
        const float v = raw[(size_t) i]->load();
        if (std::abs (v - lastSent[(size_t) i]) > 1e-6f)
        {
            lastSent[(size_t) i] = v;
            auto* e = new juce::DynamicObject();
            e->setProperty ("id", ids[i]);
            e->setProperty ("v", v);
            b.add (juce::var (e));
        }
    }
    if (b.isEmpty()) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("b", b);
    emitToUi ("hostParam", juce::var (o));
}

//==============================================================================
void Artefact104AudioProcessor::handleUiMessage (const juce::var& payload)
{
    /*  The page batches into a microtask and sends {b:[...]}: dragging heat
        into a conduit produces a message per pointer move, and one bridge
        call each would be absurd. */
    if (auto* outer = payload.getDynamicObject())
        if (outer->hasProperty ("b"))
        {
            if (auto* arr = outer->getProperty ("b").getArray())
                for (const auto& one : *arr) handleUiMessage (one);
            return;
        }

    auto* o = payload.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")        setParamById (o->getProperty ("id").toString(),
                                       (float) (double) o->getProperty ("v"), true);
    else if (k == "strike") engine.strike ((int) o->getProperty ("d"),
                                           (float) (double) o->getProperty ("a"));
    else if (k == "pour")   engine.pour ((int) o->getProperty ("d"),
                                         (float) (double) o->getProperty ("t"));
    else if (k == "spin")   engine.spin ((float) (double) o->getProperty ("a"),
                                         (float) (double) o->getProperty ("b"));
    else if (k == "specimen") { engine.requestSpecimen ((int) o->getProperty ("n"));
                                notice ("SPECIMEN " + juce::String ((int) o->getProperty ("n")).paddedLeft ('0', 3) + " PRESENT"); }
    else if (k == "random")   { const int s = juce::Random::getSystemRandom().nextInt (specimenCount());
                                engine.requestSpecimen (s);
                                notice ("SPECIMEN " + juce::String (s).paddedLeft ('0', 3) + " PRESENT"); }
    else if (k == "panic")    wantPanic.store (true);
    else if (k == "note")
    {
        const int nt = (int) o->getProperty ("n");
        if ((bool) o->getProperty ("on")) engine.noteOn (nt, 0.9f);
        else                              engine.noteOff (nt);
    }
    else if (k == "hello")    { uiHasState.store (false); lastGeomStamp = -1; emitInitialState(); emitSite(); }
    else if (k == "bwfx")     { if (bwfx_juce::handleMessage (bwfxRack, apvts, payload)) emitBwfx(); }
    else if (k == "site")
    {
        //  the SITE panel: settings are global — every finding sees the change
        proxima::Settings s = site.settings();
        const bool wasClimate = s.climate;
        if (o->hasProperty ("climate"))  s.climate  = (bool) o->getProperty ("climate");
        if (o->hasProperty ("timing"))   s.timing   = (bool) o->getProperty ("timing");
        if (o->hasProperty ("distance")) s.distance = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("distance"));
        site.setSettings (s);
        if (s.climate && ! wasClimate && site.isOpen())
        {
            //  CHOSEN HERE, GLOBAL NOW: the finding that turns the climate on
            //  gives the bench its temperature at once
            site.proposeKelvin (siteLocalK);
            siteAppliedK = siteLocalK; siteLastLocalK = siteLocalK;
        }
        emitSite();
    }
}

void Artefact104AudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

void Artefact104AudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> ps;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", s.id);
        e->setProperty ("name", s.name);
        e->setProperty ("gloss", s.gloss);
        e->setProperty ("kind", s.kind);
        e->setProperty ("v", raw[(size_t) i]->load());
        ps.add (juce::var (e));
    }
    o->setProperty ("params", ps);
    o->setProperty ("build", AB104_BUILD_ID);
    o->setProperty ("specimen", engine.currentSpecimen());
    emitToUi ("initialState", juce::var (o));
    uiHasState.store (true);
    lastGeomStamp = -1;
    emitBwfx();
}

/*  The web, thirty times a second. Geometry (nodes and conduits) rides along
    only when it changes — and every two seconds regardless, the .1 lesson: a
    single "send it once when it changes" is lost if the page was not listening
    yet, and then never sent again. The dynamic part (heat, carried power,
    presence, the rotation, the packets) goes every frame. */
void Artefact104AudioProcessor::emitWeb()
{
    if (! emitToUi) return;
    static Web w;
    engine.visualState (w);

    auto* o = new juce::DynamicObject();
    o->setProperty ("spec", w.specimen);
    o->setProperty ("rotA", (double) w.rotA);
    o->setProperty ("rotB", (double) w.rotB);
    o->setProperty ("secH", (double) w.sectionH);
    o->setProperty ("veil", (double) w.veilW);
    o->setProperty ("power", (double) engine.gridPower.load());
    o->setProperty ("sing",  engine.singing.load());
    o->setProperty ("lvl",   (double) engine.outLevel.load());

    //  geometry, when it changes (or every 2 s)
    const bool due = (++webTick >= 60);
    if (due) webTick = 0;
    if (w.geomStamp != lastGeomStamp || due)
    {
        lastGeomStamp = w.geomStamp;
        o->setProperty ("nNode", w.nNode);
        o->setProperty ("nDuct", w.nDuct);
        juce::MemoryBlock nb ((size_t) w.nNode * 4 * sizeof (float));
        std::memcpy (nb.getData(), w.node, nb.getSize());
        o->setProperty ("nodes", juce::Base64::toBase64 (nb.getData(), nb.getSize()));
        juce::MemoryBlock db ((size_t) w.nDuct * 3);   // a, b, closedEnd
        auto* p = (uint8_t*) db.getData();
        for (int i = 0; i < w.nDuct; ++i) { p[i*3] = w.ductA[i]; p[i*3+1] = w.ductB[i]; p[i*3+2] = w.closedEnd[i]; }
        o->setProperty ("ducts", juce::Base64::toBase64 (db.getData(), db.getSize()));
        juce::MemoryBlock fb ((size_t) w.nDuct * sizeof (float));
        std::memcpy (fb.getData(), w.fPassive, fb.getSize());
        o->setProperty ("fpas", juce::Base64::toBase64 (fb.getData(), fb.getSize()));
    }

    //  the dynamic per-conduit state: heat, carried amplitude, presence, order,
    //  and — the see-hear link — how far into chaos and how bright each conduit is
    const int nd = w.nDuct;
    juce::MemoryBlock hb ((size_t) nd), ab ((size_t) nd), sb ((size_t) nd), cb ((size_t) nd);
    juce::MemoryBlock xb ((size_t) nd), yb ((size_t) nd);
    std::memcpy (hb.getData(), w.heat, hb.getSize());
    std::memcpy (ab.getData(), w.amp,  ab.getSize());
    std::memcpy (sb.getData(), w.sig,  sb.getSize());
    std::memcpy (cb.getData(), w.cmd,  cb.getSize());
    std::memcpy (xb.getData(), w.chaos,  xb.getSize());
    std::memcpy (yb.getData(), w.bright, yb.getSize());
    o->setProperty ("heat",  juce::Base64::toBase64 (hb.getData(), hb.getSize()));
    o->setProperty ("amp",   juce::Base64::toBase64 (ab.getData(), ab.getSize()));
    o->setProperty ("sig",   juce::Base64::toBase64 (sb.getData(), sb.getSize()));
    o->setProperty ("cmd",   juce::Base64::toBase64 (cb.getData(), cb.getSize()));
    o->setProperty ("chaos", juce::Base64::toBase64 (xb.getData(), xb.getSize()));
    o->setProperty ("brite", juce::Base64::toBase64 (yb.getData(), yb.getSize()));

    //  the heat packets moving through the grid
    juce::Array<juce::var> pk;
    for (int i = 0; i < w.nPkt; ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("d", w.pktDuct[i]);
        e->setProperty ("t", w.pktT[i] / 255.0);
        pk.add (juce::var (e));
    }
    o->setProperty ("pkt", pk);

    emitToUi ("web", juce::var (o));
}

void Artefact104AudioProcessor::emitBwfx()
{
    if (! emitToUi) return;
    emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void Artefact104AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("specimen", engine.currentSpecimen(), nullptr);
    state.setProperty ("bwfx", juce::String (bwfxRack.toJson().c_str()), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void Artefact104AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        const int spec = tree.hasProperty ("specimen") ? (int) tree.getProperty ("specimen") : 0;
        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();
        tree.removeProperty ("bwfx", nullptr);
        tree.removeProperty ("specimen", nullptr);
        apvts.replaceState (tree);
        bwfxRack.fromJson (rack.toRawUTF8());
        engine.requestSpecimen (spec);
        emitInitialState();
    }
}

juce::AudioProcessorEditor* Artefact104AudioProcessor::createEditor()
{
    return new Artefact104AudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Artefact104AudioProcessor();
}
