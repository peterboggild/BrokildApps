#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

using namespace ab1;

//==============================================================================
static juce::String fmtValue (const PSpec& s, float v)
{
    switch (s.kind)
    {
        case KP_LIST:
        {
            int n = 0; const char* const* names = listNames (s.id, n);
            const int i = juce::jlimit (0, juce::jmax (0, n - 1), (int) std::lround (v));
            return names != nullptr && n > 0 ? juce::String (names[i]) : juce::String (i);
        }
        case KP_INT:    return juce::String ((int) std::round (v));
        case KP_KELVIN: return juce::String ((int) std::lround (s.lo + (s.hi - s.lo) * v)) + " K";
        case KP_VOL:
        {
            const float g = 2.4f * v * v;
            return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB";
        }
        default: return juce::String ((int) std::round (v * 100.0f)) + " %";
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Artefact1AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        const float hi = paramMax (s);
        const bool stepped = (s.kind == KP_LIST || s.kind == KP_INT);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (paramSpec (i), v); })));
    }
    /*  SPECIMEN is deliberately not among them. Dialling one rewrites sixteen
        other parameters, and a lane fighting sixteen lanes is unusable — the
        same call the mood organ made in Blade Ruiner and HABIT made in B2311.67. */
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
Artefact1AudioProcessor::Artefact1AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ARTEFACT1", createParameterLayout())
{
    for (int i = 0; i < numParams(); ++i) ids.add (paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    pokeQueue.reserve (64);
    startTimerHz (30);
    bwfxRack.setWorldModConsumed (false);
    tempIdx = ids.indexOf ("temp");
    site.open (1);                          // join the bench (harmless if it fails)
}

Artefact1AudioProcessor::~Artefact1AudioProcessor() { site.close(); }

bool Artefact1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Artefact1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

//==============================================================================
void Artefact1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }

    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();

    /*  The imposed pulse. The object leans towards whatever the host is doing;
        with the transport stopped it keeps its own time instead. */
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

    //  pokes queued by the panel, applied where they cannot interrupt anything
    {
        const juce::ScopedLock sl (pokeLock);
        for (const auto& pk : pokeQueue) engine.poke (pk.x, pk.y, pk.amount, pk.radius);
        pokeQueue.clear();
    }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())       { engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
                                  midiHeld[m.getNoteNumber() & 127] = true; }
        else if (m.isNoteOff()) { engine.noteOff (m.getNoteNumber());
                                  midiHeld[m.getNoteNumber() & 127] = false; }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
                                { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
    }

    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : L;
    if (nch > 1) engine.process (L, R, n);
    else { std::vector<float> tmp ((size_t) n); engine.process (L, tmp.data(), n); }

    bwfxRack.process (L, R, n);
    if (nch == 1) {} else if (nch > 2)
        for (int c = 2; c < nch; ++c) buffer.clear (c, 0, n);
}

//==============================================================================
void Artefact1AudioProcessor::timerCallback()
{
    engine.service();
    bwfxRack.service();
    siteStep();
    emitParamEcho();
    emitBody();
}

//==============================================================================
//  TEMPERATURE here is linear, 77 K at 0 and 800 K at 1
static float kelvinOfTemp (float v)  { return 77.0f + 723.0f * juce::jlimit (0.0f, 1.0f, v); }
static float tempOfKelvin (float k)  { return juce::jlimit (0.0f, 1.0f, (k - 77.0f) / 723.0f); }

void Artefact1AudioProcessor::siteStep()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    double dt = siteLastMs > 0.0 ? (nowMs - siteLastMs) * 0.001 : 1.0 / 30.0;
    siteLastMs = nowMs;
    dt = juce::jlimit (0.0, 0.25, dt);

    const float tv     = tempIdx >= 0 ? raw[(size_t) tempIdx]->load() : 0.3f;
    const float warmth = juce::jlimit (0.0f, 1.0f, tv);
    const float myK    = kelvinOfTemp (tv);
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
    //  SETTLING: no proposals for the first second (the host is still
    //  restoring state, and a restored value is not a player's move); while
    //  settling, the bench's climate overrides whatever the restore brings in
    const bool settled = siteTick > 25;

    const float activity = juce::jlimit (0.0f, 1.0f, engine.outLevel.load() * 2.0f);
    const auto v = site.sync (activity, myK, (float) sitePhase, warmth);

    //  the climate: my move is proposed; a move by another is followed
    //  through the host parameter (visible, automatable, undoable)
    if (v.climateShared)
    {
        const bool iMoved = std::abs (myK - siteLastLocalK) > 0.5f
                         && std::abs (myK - siteAppliedK) > 0.5f;
        const bool follow = v.climateMoved || ! settled;
        //  SEED: a bench nobody has spoken to has no temperature, and nothing
        //  would converge until someone moved a slider — which reads as the
        //  sharing being broken. The first settled finding gives it its own.
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
            if (auto* prm = apvts.getParameter ("temp"))
                prm->setValueNotifyingHost (tempOfKelvin (v.siteKelvin));
        }
    }
    if (std::abs (myK - siteLastLocalK) > 0.5f) siteLastLocalK = myK;

    //  the timing: Kuramoto, cold and close
    const float pull = proxima::Client::pullStrength (v, warmth);
    sitePhase = proxima::Client::stepPhase ((float) sitePhase, site.naturalHz(), dt, v, pull);
    engine.setSite ((float) sitePhase, v.pulseHz, pull);

    siteCoh = v.coherence; siteOthers = v.others; sitePull = pull;
    siteK = (v.climateShared && v.siteKelvin > 0.0f) ? v.siteKelvin : myK;
    if (siteTick % 8 == 0) emitSite();
}

void Artefact1AudioProcessor::emitSite()
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

void Artefact1AudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        const auto& r = prm->getNormalisableRange();
        const float v = juce::jlimit (r.start, r.end, value);
        //  the page's own move needs no echo (it already shows it)
        if (fromUi)
        {
            const int idx = ids.indexOf (id);
            if (idx >= 0 && idx < (int) lastSent.size()) lastSent[(size_t) idx] = v;
        }
        prm->setValueNotifyingHost (r.convertTo0to1 (v));
    }
}

void Artefact1AudioProcessor::emitParamEcho()
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
        const float v = raw[(size_t) i]->load();      // raw units, as the page keeps them
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

void Artefact1AudioProcessor::dialSpecimen (int index)
{
    specimen = ((index % specimenCount()) + specimenCount()) % specimenCount();
    Params q; applySpecimen (specimen, q);
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        const juce::String sid (s.id);
        //  the specimen is the object; how loud, how cold and how the crate
        //  behaves are the player's business and are left alone
        if (sid == "level" || sid == "temp" || sid == "sat" || sid == "space"
            || sid == "division" || sid == "freehz") continue;
        setParamById (sid, s.get (q), false);
    }
    notice ("SPECIMEN " + juce::String (specimen).paddedLeft ('0', 3) + " DIALLED");
}

//==============================================================================
void Artefact1AudioProcessor::handleUiMessage (const juce::var& payload)
{
    /*  The page batches into a microtask and sends {b:[...]}, because dragging
        across the face produces a poke per pointer move and one bridge call
        each would be absurd. Unpack a batch into single messages and let the
        rest of this function stay simple. */
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
    else if (k == "poke")
    {
        const juce::ScopedLock sl (pokeLock);
        if (pokeQueue.size() < 256)
            pokeQueue.push_back ({ (int) o->getProperty ("x"), (int) o->getProperty ("y"),
                                   (int) o->getProperty ("r"),
                                   (float) (double) o->getProperty ("a") });
    }
    else if (k == "specimen") dialSpecimen ((int) o->getProperty ("n"));
    else if (k == "random")   dialSpecimen (juce::Random::getSystemRandom().nextInt (specimenCount()));
    else if (k == "panic")    wantPanic.store (true);
    else if (k == "note")
    {
        const int nt = (int) o->getProperty ("n");
        if ((bool) o->getProperty ("on")) engine.noteOn (nt, 0.9f);
        else                              engine.noteOff (nt);
    }
    else if (k == "hello")    { uiHasState.store (false); lastRateHash = 0; emitInitialState(); emitSite(); }
    else if (k == "bwfx")     { bwfx_juce::handleMessage (bwfxRack, apvts, payload); emitBwfx(); }
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

void Artefact1AudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

void Artefact1AudioProcessor::emitInitialState()
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
        e->setProperty ("lo", s.lo);
        e->setProperty ("hi", s.hi);
        e->setProperty ("max", paramMax (s));
        e->setProperty ("v", raw[(size_t) i]->load());
        int n = 0; const char* const* names = listNames (s.id, n);
        if (names && n > 0)
        {
            juce::Array<juce::var> ls;
            for (int q = 0; q < n; ++q) ls.add (juce::String (names[q]));
            e->setProperty ("names", ls);
        }
        ps.add (juce::var (e));
    }
    o->setProperty ("params", ps);
    o->setProperty ("build", AB_BUILD_ID);
    o->setProperty ("specimen", specimen);
    o->setProperty ("nx", ab1::NX);
    o->setProperty ("ny", ab1::NY);
    emitToUi ("initialState", juce::var (o));
    uiHasState.store (true);
    emitBwfx();
}

/*  The cross-section, thirty times a second. Phase as one byte per cell and the
    heat left by a recent firing as another — 2048 bytes, which is what a body
    of nine thousand units costs to look at. */
void Artefact1AudioProcessor::emitBody()
{
    if (! emitToUi) return;
    static Slice sl;
    engine.visualState (sl);

    juce::MemoryBlock ph ((size_t) (Slice::W * Slice::H));
    juce::MemoryBlock ht ((size_t) (Slice::W * Slice::H));
    std::memcpy (ph.getData(), sl.phase, ph.getSize());
    std::memcpy (ht.getData(), sl.heat,  ht.getSize());

    auto* o = new juce::DynamicObject();
    o->setProperty ("ph", juce::Base64::toBase64 (ph.getData(), ph.getSize()));
    o->setProperty ("ht", juce::Base64::toBase64 (ht.getData(), ht.getSize()));
    /*  The marks, every frame. Unlike the rate field these move continuously
        while a trace fades, and a trace you cannot watch fade is not a trace. */
    {
        juce::MemoryBlock mk ((size_t) (Slice::W * Slice::H));
        std::memcpy (mk.getData(), sl.mark, mk.getSize());
        o->setProperty ("mk", juce::Base64::toBase64 (mk.getData(), mk.getSize()));
    }
    o->setProperty ("lvl", (double) engine.outLevel.load());
    o->setProperty ("beat", (double) engine.pulsePhase.load());
    o->setProperty ("casc", engine.lastCascade.load());
    //  the honest total. The page used to add lastCascade every frame, which
    //  counts the same event thirty times a second and looks like a busy object.
    o->setProperty ("fired", (double) engine.totalFired.load());
    o->setProperty ("steps", (double) engine.stepsRun.load());
    o->setProperty ("lean", (double) engine.concentration.load());
    o->setProperty ("specimen", specimen);

    /*  The rate field is static until the spread is changed, so it is sent when
        it MOVES rather than thirty times a second: a kilobyte a frame for a
        picture that is almost always the same picture would be waste. */
    {
        /*  Sent when it changes -- and ALSO every two seconds, because "send it
            once when it changes" loses the one send if the page was not
            listening yet, and then never sends it again. B2311.67 learned the
            same lesson about its initial state and answered it the same way. */
        uint32_t sum = 2166136261u;
        for (int i = 0; i < Slice::W * Slice::H; ++i) { sum ^= sl.rate[i]; sum *= 16777619u; }
        const bool due = (++rateTick >= 60);
        if (due) rateTick = 0;
        if (sum != lastRateHash || due)
        {
            lastRateHash = sum;
            juce::MemoryBlock rt ((size_t) (Slice::W * Slice::H));
            std::memcpy (rt.getData(), sl.rate, rt.getSize());
            o->setProperty ("rt", juce::Base64::toBase64 (rt.getData(), rt.getSize()));
        }
    }
    emitToUi ("body", juce::var (o));
}

void Artefact1AudioProcessor::emitBwfx()
{
    if (! emitToUi) return;
    emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void Artefact1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("specimen", specimen, nullptr);
    state.setProperty ("bwfx", juce::String (bwfxRack.toJson().c_str()), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void Artefact1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        if (tree.hasProperty ("specimen")) specimen = (int) tree.getProperty ("specimen");
        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();
        tree.removeProperty ("bwfx", nullptr);
        tree.removeProperty ("specimen", nullptr);
        apvts.replaceState (tree);
        bwfxRack.fromJson (rack.toRawUTF8());
        emitInitialState();
    }
}

juce::AudioProcessorEditor* Artefact1AudioProcessor::createEditor()
{
    return new Artefact1AudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Artefact1AudioProcessor();
}
