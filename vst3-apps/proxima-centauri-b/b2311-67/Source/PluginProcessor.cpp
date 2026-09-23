#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    juce::String fmtValue (const ax::PSpec& s, float v)
    {
        using namespace ax;
        switch (s.kind)
        {
            case KP_SW:   return v >= 0.5f ? "PRESENT" : "ABSENT";
            case KP_LIST:
            {
                int n = 0; auto names = listNames (s.id, n);
                const int i = juce::jlimit (0, std::max (0, n - 1), (int) std::round (v));
                return names != nullptr && n > 0 ? juce::String (names[i]) : juce::String (i);
            }
            case KP_INT:  return juce::String ((int) std::round (v));
            case KP_BIPOL:
            {
                const float x = s.lo + (s.hi - s.lo) * v;
                return (x >= 0 ? "+" : "") + juce::String (x, 2);
            }
            case KP_CENT: { const float c = (v - 0.5f) * s.lo; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " cents"; }
            case KP_SEMI: { const int st = (int) std::round ((v - 0.5f) * 6.0f); return (st > 0 ? "+" : "") + juce::String (st) + " oct"; }
            case KP_VOL:  { const float g = 2.2f * v * v; return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB"; }
            case KP_KELVIN: return juce::String ((int) std::lround (s.lo + (s.hi - s.lo) * v)) + " K";
            default:      return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ArtefactAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < ax::numParams(); ++i)
    {
        const auto& s = ax::paramSpec (i);
        const float hi = ax::paramMax (s);
        const bool stepped = s.kind == ax::KP_LIST || s.kind == ax::KP_INT || s.kind == ax::KP_SW;
        const juce::String sid (s.id);
        /*  HABIT is not automatable. Dialling a specimen rewrites a dozen other
            parameters, and a lane fighting a dozen other lanes is unusable —
            the same call as the mood organ in Blade Ruiner. TRAVERSE, by
            contrast, is the most automatable thing here: an automation lane on
            it is the artefact being drawn through the cut over a whole piece. */
        const bool automatable = ! (sid == "habit");

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (ax::paramSpec (i), v); })));
    }
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
ArtefactAudioProcessor::ArtefactAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ARTEFACT67", createParameterLayout())
{
    for (int i = 0; i < ax::numParams(); ++i) ids.add (ax::paramSpec (i).id);
    for (int i = 0; i < ax::numParams(); ++i)
    {
        if (juce::String (ax::paramSpec (i).id) == "habit") habitIndex = i;
        if (juce::String (ax::paramSpec (i).id) == "temp")  tempIndex  = i;
    }
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);
    for (auto& h : midiHeld) h = false;

    applyHabitIndex (0);
    startTimerHz (30);
    bwfxRack.setWorldModConsumed (false);
    site.open (67);                          // join the bench (harmless if it fails)
}

ArtefactAudioProcessor::~ArtefactAudioProcessor() { site.close(); }

//==============================================================================
//  TEMPERATURE is linear, 77 K at 0 and 800 K at 1 (Modulation.h)
static float kelvinOfTemp (float v)  { return ax::TEMP_MIN + (ax::TEMP_MAX - ax::TEMP_MIN) * juce::jlimit (0.0f, 1.0f, v); }
static float tempOfKelvin (float k)  { return juce::jlimit (0.0f, 1.0f, (k - ax::TEMP_MIN) / (ax::TEMP_MAX - ax::TEMP_MIN)); }

void ArtefactAudioProcessor::siteStep()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    double dt = siteLastMs > 0.0 ? (nowMs - siteLastMs) * 0.001 : 1.0 / 30.0;
    siteLastMs = nowMs;
    dt = juce::jlimit (0.0, 0.25, dt);

    const float tv     = raw[(size_t) tempIndex]->load();
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
            setParamById ("temp", tempOfKelvin (v.siteKelvin));   // echoed to the page
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

void ArtefactAudioProcessor::emitSite()
{
    if (! emitToUi || ! uiReady.load()) return;
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

bool ArtefactAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ArtefactAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    refreshParams (0.0);
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    /*  The brickwall's look-ahead is real delay. Reported, it is compensated
        by the host and costs nothing; unreported, every note would sit four
        milliseconds late against the rest of the session. */
    setLatencySamples (engine.latencySamples());
}

/*  The artefact is rebuilt here, off the audio thread, thirty times a second:
    the cut-and-project, the chain, the tuning anchor. It must run with the
    editor closed — TRAVERSE is a host parameter and dragging the body through
    the cut is the whole instrument. */
void ArtefactAudioProcessor::timerCallback()
{
    refreshParams (0.0);
    siteStep();                 // before service(): the rock lands in this tick's rebuild
    engine.service();
    bwfxRack.service();
}

void ArtefactAudioProcessor::pushEvent (const UiEvent& e)
{
    const int w = evWrite.load();
    const int next = (w + 1) % EVQ;
    if (next == evRead.load()) return;
    evq[(size_t) w] = e;
    evWrite.store (next);
}

//==============================================================================
void ArtefactAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) { engine.reset(); for (auto& h : midiHeld) h = false; }

    refreshParams ((double) n / juce::jmax (1.0, getSampleRate()));

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 999.0) bwfxRack.setBpm (*bpm);

    while (evRead.load() != evWrite.load())
    {
        const auto e = evq[(size_t) evRead.load()];
        evRead.store ((evRead.load() + 1) % EVQ);
        switch (e.kind)
        {
            case 1: engine.noteOn (e.note, e.v); midiHeld[(size_t) juce::jlimit (0, 127, e.note)] = true; break;
            case 2: engine.noteOff (e.note); midiHeld[(size_t) juce::jlimit (0, 127, e.note)] = false; break;
            case 3: engine.setBend (e.v); break;
            case 4: engine.setWheel (e.v); break;
            case 5: engine.allNotesOff(); for (auto& h : midiHeld) h = false; break;
            default: break;
        }
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = nch >= 2 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float> monoR;
    if (R == nullptr) { monoR.assign ((size_t) n, 0.0f); R = monoR.data(); }

    int last = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (pos > last) { engine.process (L + last, R + last, pos - last); last = pos; }

        const auto m = meta.getMessage();
        if      (m.isNoteOn())     { engine.noteOn (m.getNoteNumber(), m.getFloatVelocity()); midiHeld[(size_t) m.getNoteNumber()] = true; }
        else if (m.isNoteOff())    { engine.noteOff (m.getNoteNumber()); midiHeld[(size_t) m.getNoteNumber()] = false; }
        else if (m.isPitchWheel()) { engine.setBend ((m.getPitchWheelValue() - 8192) / 8192.0f); }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if      (cc == 1)  engine.setWheel (v / 127.0f);
            else if (cc == 64) engine.setSustainPedal (v >= 64);
            else if (cc == 120 || cc == 123) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
    }
    if (n > last) engine.process (L + last, R + last, n - last);

    bwfx_juce::pushMacros (bwfxRack, apvts);
    bwfxRack.process (L, R, n);

    if (nch == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
void ArtefactAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        // the arrests are the player's deformation of the four-dimensional
        // body; they are not parameters, and they are not derivable
        juce::String ar;
        for (const auto& a : engine.arrestList())
            ar += juce::String (a.first, 6) + "," + juce::String (a.second, 6) + ";";
        xml->setAttribute ("arrests", ar);
        xml->setAttribute ("tau", engine.travelDepth());
        copyXmlToBinary (*xml, dest);
    }
}

void ArtefactAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();

            engine.clearArrests();
            auto ar = juce::StringArray::fromTokens (xml->getStringAttribute ("arrests"), ";", "");
            for (const auto& t : ar)
            {
                auto xy = juce::StringArray::fromTokens (t, ",", "");
                if (xy.size() == 2) engine.arrest (xy[0].getDoubleValue(), xy[1].getDoubleValue(), true);
            }

            xml->removeAttribute ("bwfx"); xml->removeAttribute ("arrests"); xml->removeAttribute ("tau");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            engine.service();
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
            lastHabitSent = -1;
        }
}

juce::AudioProcessorEditor* ArtefactAudioProcessor::createEditor()
{
    return new ArtefactAudioProcessorEditor (*this);
}

//==============================================================================
void ArtefactAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void ArtefactAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

/*  Dial a specimen. A habit is a field over the acceptance window, so most of
    what it decides is not a knob at all — but the scalars it does imply are
    written through so the panel and the body agree. */
void ArtefactAudioProcessor::applyHabitIndex (int index)
{
    index = juce::jlimit (0, ax::habitCount() - 1, index);
    ax::Params q = engine.p;
    ax::applyHabit (index, q);
    for (int i = 0; i < ax::numParams(); ++i)
    {
        const auto& s = ax::paramSpec (i);
        const juce::String id (s.id);
        if (id == "level" || id == "voices" || id == "conform" || id == "ref"
            || id == "octave" || id == "travel" || id == "bearing") continue;
        setParamById (s.id, s.get (q));
    }
    setParamById ("habit", (float) index);
    lastHabitSent = -1;
}

void ArtefactAudioProcessor::randomise()
{
    juce::Random r (juce::Time::getHighResolutionTicks());
    applyHabitIndex (r.nextInt (ax::habitCount()));
}

//==============================================================================
void ArtefactAudioProcessor::handleUiMessage (const juce::var& payload)
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

void ArtefactAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")        setParamById (o->getProperty ("id").toString(), (float) (double) o->getProperty ("v"), true);
    else if (k == "ack")    uiHasState = true;
    else if (k == "ready")  { uiReady = true; emitSite(); }
    else if (k == "hello")  uiHasState = false;
    else if (k == "site")
    {
        //  the SITE panel: settings are the bench's — every finding sees the change
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
    else if (k == "habit")  { applyHabitIndex ((int) o->getProperty ("n")); }
    else if (k == "random") randomise();
    else if (k == "panic")  { wantPanic = true; }
    else if (k == "note")
    {
        UiEvent e; e.kind = (bool) o->getProperty ("on") ? 1 : 2; e.note = (int) o->getProperty ("n");
        e.v = o->hasProperty ("v") ? (float) (double) o->getProperty ("v") : 0.8f;
        pushEvent (e);
    }
    else if (k == "bend")   { UiEvent e; e.kind = 3; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "wheel")  { UiEvent e; e.kind = 4; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "alloff") { UiEvent e; e.kind = 5; pushEvent (e); }
    else if (k == "arrest")
    {
        engine.arrest ((double) o->getProperty ("px"), (double) o->getProperty ("py"),
                       (bool) o->getProperty ("on"));
        engine.service();
    }
    else if (k == "clearArrests") { engine.clearArrests(); engine.service(); }
    /*  The wheel, past the ends of the slider. TRAVERSE stays a host parameter
        on nought to one; this is the free travel beyond it, which is not
        automatable and not saved, because it is a place you went and not a
        setting you chose. */
    else if (k == "twheel") { engine.setTravelDelta ((double) o->getProperty ("d")); }
    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "save")   patchSaveAs();
    else if (k == "open")   patchOpenDialog();
    else if (k == "patchScan") patchScan();
    else if (k == "patchLoad") patchLoad (o->getProperty ("path").toString());
}

//==============================================================================
void ArtefactAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        const auto& s = ax::paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("hi", (double) ax::paramMax (s));
        e->setProperty ("n", juce::String (s.name));
        e->setProperty ("g", juce::String (s.gloss));
        e->setProperty ("kind", s.kind);
        e->setProperty ("def", (double) s.def);
        e->setProperty ("lo", (double) s.lo);
        e->setProperty ("fhi", (double) s.hi);
        int nn = 0; auto names = ax::listNames (s.id, nn);
        if (names != nullptr && nn > 0)
        {
            juce::Array<juce::var> list;
            for (int j = 0; j < nn; ++j) list.add (juce::String (names[j]));
            e->setProperty ("names", list);
        }
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    /*  The whole catalogue, once. The page draws the tiling itself, so it
        needs the field that colours it — and a habit IS that field. Sent at
        init rather than per change so the catalogue can paint all 256
        swatches from the same arithmetic the engine uses. */
    juce::Array<juce::var> ht;
    for (int i = 0; i < ax::habitCount(); ++i)
    {
        const ab::Habit h = ab::habitOf (i);
        auto* e = new juce::DynamicObject();
        juce::Array<juce::var> km, kd, ka, kp;
        for (int j = 0; j < 4; ++j) { km.add (h.kmag[j]); kd.add (h.kdir[j]); ka.add (h.kamp[j]); kp.add (h.kphi[j]); }
        e->setProperty ("w", h.waves);
        e->setProperty ("km", km); e->setProperty ("kd", kd);
        e->setProperty ("ka", ka); e->setProperty ("kp", kp);
        e->setProperty ("r", h.radial); e->setProperty ("c", h.crisp); e->setProperty ("l", h.levels);
        e->setProperty ("fb", h.filmBase); e->setProperty ("fs", h.filmSpan); e->setProperty ("g", h.glint);
        int gA = 0, gB = 0; float hA = 0, hB = 0;
        ax::habitSignature (i, gA, gB, hA, hB);
        // the specimen is catalogued, not christened: two structural
        // invariants of its field, which is all an alien object gets
        e->setProperty ("sig", juce::String (gA) + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\xb6")) + juce::String (gB));
        ht.add (juce::var (e));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    obj->setProperty ("habitTable", ht);
    obj->setProperty ("habits", ax::habitCount());
    obj->setProperty ("grade", ax::GRADE);
   #ifdef AB_BUILD_ID
    obj->setProperty ("build", juce::String (AB_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
}

/*  What the panel needs every frame. The tiling itself is NOT sent: the page
    runs the same cut-and-project, which is a dozen lines of arithmetic, and
    shipping a thousand vertices thirty times a second over a JSON bridge would
    be absurd. What has to cross is the state the projection depends on — where
    the cut is in the fourth dimension, how the window has been deformed by the
    player's arrests — plus the one thing the page cannot know: how much the
    body is actually moving, site by site. */
/*  Host parameters in, engine parameters out, with the specimen's own wiring in
    between.

    All three places that used to copy the parameter table into the engine now
    come through here, so there is one description of what the engine is
    actually running on. The stored values are what the host and the panel hold;
    the effective values are what the instrument hears. Nothing reads the
    effective values back, which is why no wiring can form a loop.

    At 77 K `apply` is a straight memcpy and this costs a copy and nothing
    else -- a frozen instrument is the instrument as it was before any of this
    was written, to the bit. */
void ArtefactAudioProcessor::refreshParams (double dtSeconds)
{
    const int n = ax::numParams();
    if ((int) paramStored.size() != n) { paramStored.assign ((size_t) n, 0.0f);
                                         paramEffective.assign ((size_t) n, 0.0f); }

    for (int i = 0; i < n; ++i) paramStored[(size_t) i] = raw[(size_t) i]->load();

    const int hb = (int) std::lround (paramStored[(size_t) habitIndex]);
    modulator.setHabit (hb);
    modulator.advance (dtSeconds, paramStored.data());

    const float kelvin = ax::TEMP_MIN + (ax::TEMP_MAX - ax::TEMP_MIN)
                       * juce::jlimit (0.0f, 1.0f, paramStored[(size_t) tempIndex]);
    modulator.apply (paramStored.data(), paramEffective.data(), kelvin);

    for (int i = 0; i < n; ++i)
        ax::paramSpec (i).get (engine.p) = paramEffective[(size_t) i];
}

void ArtefactAudioProcessor::emitBody()
{
    if (! emitToUi) return;

    static float e[ax::MAXN];
    int nE = 0;
    engine.visualState (e, ax::MAXN, nE);

    float peak = 1.0e-6f;
    for (int i = 0; i < nE; ++i) peak = std::max (peak, e[i]);
    juce::MemoryBlock mb ((size_t) std::max (0, nE));
    auto* bytes = (juce::uint8*) mb.getData();
    for (int i = 0; i < nE; ++i)
        bytes[i] = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (255.0f * std::sqrt (e[i] / peak)));

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("tau", engine.travelDepth());
    obj->setProperty ("n", nE);
    obj->setProperty ("peak", (double) peak);
    obj->setProperty ("lvl", (double) engine.outLevel.load());
    obj->setProperty ("gr", (double) engine.limitGR.load());
    obj->setProperty ("w1", (double) engine.lowHz.load());
    obj->setProperty ("orders", engine.starN.load());
    obj->setProperty ("e", juce::Base64::toBase64 (mb.getData(), mb.getSize()));

    juce::Array<juce::var> ar;
    {
        int i = 0;
        for (const auto& a : engine.arrestList())
        {
            auto* p = new juce::DynamicObject();
            p->setProperty ("x", a.first); p->setProperty ("y", a.second);
            p->setProperty ("s", (double) engine.arrestStrainAt (i++));
            ar.add (juce::var (p));
        }
    }
    obj->setProperty ("arrests", ar);

    /*  THE STAR ITSELF.

        The panel used to compute its own diffraction plate in JavaScript, from
        the idealised module and three parameters -- and so it never moved for
        travel, the cut, the aperture, obliquity or even the specimen. Measured:
        the plate's signature was identical through all of them. It was a
        picture of an idea, not of the sound.

        The engine has the real thing: peaks found in the structure factor of
        the chain that is sounding. Send those. It is a few hundred numbers and
        it only changes when the star is rebuilt, so it rides on the generation
        counter rather than on the frame. */
    obj->setProperty ("sqf", (double) engine.squareFraction());

    /*  The EFFECTIVE parameter values -- what the instrument is actually
        running on once the specimen's own wiring has stirred them. The panel
        draws its rings from these, so a body that is moving is a body you can
        SEE moving. One byte each is plenty for a ring sweep. */
    {
        const int np = ax::numParams();
        juce::MemoryBlock em ((size_t) np);
        auto* eb = (juce::uint8*) em.getData();
        for (int i = 0; i < np && i < (int) paramEffective.size(); ++i)
        {
            const float hi = ax::paramMax (ax::paramSpec (i));
            eb[i] = (juce::uint8) juce::jlimit (0, 255,
                        (int) std::lround (255.0f * (hi > 0.0f ? paramEffective[(size_t) i] / hi : 0.0f)));
        }
        obj->setProperty ("eff", juce::Base64::toBase64 (em.getData(), em.getSize()));
    }
    if (engine.starGeneration() != lastStarGen)
    {
        lastStarGen = engine.starGeneration();
        const auto& pk = engine.star();
        double mx = 0.0;
        for (const auto& p : pk) mx = std::max (mx, p.amp);
        if (mx <= 0.0) mx = 1.0;
        juce::Array<juce::var> lam, amp;
        lam.ensureStorageAllocated ((int) pk.size());
        amp.ensureStorageAllocated ((int) pk.size());
        for (const auto& p : pk)
        {
            lam.add (juce::var (p.lam));
            amp.add (juce::var (p.amp / mx));
        }
        obj->setProperty ("starLam", lam);
        obj->setProperty ("starAmp", amp);
    }
    if (engine.takeTorn()) notice ("A PIN TORE LOOSE " + DOT + " THE BODY WOULD NOT CARRY THE STRAIN");
    /*  How much of the fragment the cut is passing through. The page draws the
        body by it, so the picture and the sound cannot disagree about whether
        there is anything here. */
    obj->setProperty ("pres", (double) engine.presencePub.load (std::memory_order_relaxed));
    emitToUi ("body", juce::var (obj));
}

void ArtefactAudioProcessor::emitBwfx()
{
    if (emitToUi) emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void ArtefactAudioProcessor::timerService()
{
    if (! emitToUi) return;

    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

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
        auto* o = new juce::DynamicObject();
        o->setProperty ("p", changed);
        emitToUi ("hostParam", juce::var (o));
    }

    emitBody();
}

//==============================================================================
juce::PropertiesFile& ArtefactAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "ArtefactB2311_67";
        o.filenameSuffix = "settings";
        o.folderName = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File ArtefactAudioProcessor::patchFolder()
{
    return brokild::patchFolder ("Artefact B2311.67", { "artefact-b2311-67" });
}

juce::String ArtefactAudioProcessor::patchJson (const juce::String& name)
{
    juce::DynamicObject::Ptr o (new juce::DynamicObject());
    o->setProperty ("app", "artefact-b2311-67");
    o->setProperty ("name", name);
    for (int i = 0; i < ids.size(); ++i)
        o->setProperty (ids[i], (double) raw[(size_t) i]->load());
    juce::String ar;
    for (const auto& a : engine.arrestList())
        ar += juce::String (a.first, 6) + "," + juce::String (a.second, 6) + ";";
    o->setProperty ("arrests", ar);
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (o.get()), false);
}

void ArtefactAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed()) { notice ("SPECIMEN UNREADABLE"); return; }
    auto* o = v.getDynamicObject();
    if (o == nullptr) return;
    for (int i = 0; i < ids.size(); ++i)
        if (o->hasProperty (ids[i]))
            setParamById (ids[i], (float) (double) o->getProperty (ids[i]));
    engine.clearArrests();
    auto ar = juce::StringArray::fromTokens (o->getProperty ("arrests").toString(), ";", "");
    for (const auto& t : ar)
    {
        auto xy = juce::StringArray::fromTokens (t, ",", "");
        if (xy.size() == 2) engine.arrest (xy[0].getDoubleValue(), xy[1].getDoubleValue(), true);
    }
    bwfxRack.fromJson (o->getProperty ("bwfx").toString().toStdString());
    emitBwfx();
    engine.service();
    lastHabitSent = -1;
    notice (name.toUpperCase());
}

void ArtefactAudioProcessor::patchScan()
{
    if (! emitToUi) return;
    auto dir = patchFolder();
    juce::Array<juce::var> list;
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            auto* e = new juce::DynamicObject();
            e->setProperty ("n", f.getFileNameWithoutExtension());
            e->setProperty ("p", f.getFullPathName());
            list.add (juce::var (e));
        }
    auto* o = new juce::DynamicObject();
    o->setProperty ("files", list);
    o->setProperty ("dir", dir.getFullPathName());
    emitToUi ("patchTree", juce::var (o));
}

void ArtefactAudioProcessor::patchSaveAs()
{
    auto dir = patchFolder();
    dir.createDirectory();
    activeChooser = std::make_unique<juce::FileChooser> ("Record specimen", dir.getChildFile ("specimen.json"), "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (! f.hasFileExtension ("json")) f = f.withFileExtension ("json");
            f.replaceWithText (patchJson (f.getFileNameWithoutExtension()));
            notice ("RECORDED " + DOT + " " + f.getFileNameWithoutExtension().toUpperCase());
            patchScan();
        });
}

void ArtefactAudioProcessor::patchOpenDialog()
{
    activeChooser = std::make_unique<juce::FileChooser> ("Read specimen", patchFolder(), "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
        });
}

void ArtefactAudioProcessor::patchLoad (const juce::String& path)
{
    juce::File f (path);
    if (f.existsAsFile()) applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArtefactAudioProcessor();
}
