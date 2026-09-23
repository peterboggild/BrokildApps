#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    juce::String fmtValue (const ab::PSpec& s, float v)
    {
        switch (s.kind)
        {
            case ab::KP_SW:  return v >= 0.5f ? "ON" : "OFF";
            case ab::KP_INT: return juce::String ((int) std::lround (v));
            case ab::KP_KELVIN:
                return juce::String ((int) std::lround (77.0f + 723.0f * v)) + " K";
            case ab::KP_VOL:
            {
                const float g = v < 0.005f ? 0.0f : 2.0f * v * v;
                return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB";
            }
            default: return juce::String ((int) std::lround (v * 100.0f)) + " %";
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ArtefactAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < ab::numParams(); ++i)
    {
        const auto& s = ab::paramSpec (i);
        const bool stepped = s.kind == ab::KP_SW || s.kind == ab::KP_INT;
        const float hi = s.kind == ab::KP_INT ? s.hi : 1.0f;
        //  SPECIMEN rewrites the whole being — the mood-organ precedent:
        //  saved, dialable, never a lane fighting forty other lanes
        //  SPECIMEN and OTHER each rebuild a whole being on the message
        //  thread; neither belongs on an automation lane (the mood-organ
        //  precedent from Blade Ruiner)
        const juce::String sid (s.id);
        const bool automatable = sid != "specimen" && sid != "other";
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (ab::paramSpec (i), v); })));
    }
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

ArtefactAudioProcessor::ArtefactAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ARTEFACTB2311", createParameterLayout())
{
    for (int i = 0; i < ab::numParams(); ++i) ids.add (ab::paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);

    startTimerHz (15);                       // bwfxRack.service + the survey
    bwfxRack.setWorldModConsumed (true);     // the characters possess the artefact
    tempIdx = ab::paramIndex ("temp");
    site.open (22);                          // join the bench (harmless if it fails)
}

ArtefactAudioProcessor::~ArtefactAudioProcessor() { site.close(); }

//==============================================================================
//  TEMPERATURE here is linear, 77 K at 0 and 800 K at 1
static float kelvinOfTemp (float v)  { return 77.0f + 723.0f * juce::jlimit (0.0f, 1.0f, v); }
static float tempOfKelvin (float k)  { return juce::jlimit (0.0f, 1.0f, (k - 77.0f) / 723.0f); }

void ArtefactAudioProcessor::siteStep()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    double dt = siteLastMs > 0.0 ? (nowMs - siteLastMs) * 0.001 : 1.0 / 15.0;
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
    const bool settled = siteTick > 14;

    const float activity = juce::jlimit (0.0f, 1.0f,
        (float) engine.debugActiveVoices() / (float) ab::kVoices * 2.0f);
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
            setParamById ("temp", tempOfKelvin (v.siteKelvin));
        }
    }
    if (std::abs (myK - siteLastLocalK) > 0.5f) siteLastLocalK = myK;

    //  the timing: Kuramoto, cold and close
    const float pull = proxima::Client::pullStrength (v, warmth);
    sitePhase = proxima::Client::stepPhase ((float) sitePhase, site.naturalHz(), dt, v, pull);
    engine.setSite ((float) sitePhase, v.pulseHz, pull);

    siteCoh = v.coherence; siteOthers = v.others; sitePull = pull;
    siteK = (v.climateShared && v.siteKelvin > 0.0f) ? v.siteKelvin : myK;
    if (siteTick % 4 == 0) emitSite();
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
    for (int i = 0; i < ab::numParams(); ++i)
        ab::pvalue (engine.p, ab::paramSpec (i)) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

void ArtefactAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) engine.allNotesOff();

    for (int i = 0; i < ab::numParams(); ++i)
        ab::pvalue (engine.p, ab::paramSpec (i)) = raw[(size_t) i]->load();

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())        engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())  engine.noteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel())
            engine.setBend (((float) msg.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    midi.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
            {
                bwfxRack.setBpm (*bpm);
                const auto ppq = pos->getPpqPosition();
                bwfxRack.setTransport (*bpm, ppq ? *ppq : 0.0, pos->getIsPlaying());
            }

    bwfx_juce::pushMacros (bwfxRack, apvts);

    //  SPECTRA possess the artefact through the bus (neutral = bit-identical)
    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth,
                            wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    auto* L = buffer.getWritePointer (0);
    if (nch >= 2)
    {
        engine.process (L, buffer.getWritePointer (1), n);
        bwfxRack.process (L, buffer.getWritePointer (1), n);
    }
    else
    {
        static thread_local std::vector<float> tmp;
        tmp.assign ((size_t) n, 0.0f);
        engine.process (L, tmp.data(), n);
        bwfxRack.process (L, tmp.data(), n);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + tmp[(size_t) i]);
    }
}

//==============================================================================
void ArtefactAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        /*  the accent: a body marked by a conversation must arrive marked.
            Edge multipliers, three decimals, and nothing at all when the
            bodies are still exactly their catalog selves. */
        {
            const auto a = accentToString (engine.accentSelf(), ab::kMaxEdges);
            const auto b = accentToString (engine.accentOther(), ab::kMaxEdges);
            if (a.isNotEmpty()) xml->setAttribute ("accA", a);
            if (b.isNotEmpty()) xml->setAttribute ("accB", b);
        }
        copyXmlToBinary (*xml, dest);
    }
}

void ArtefactAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            xml->removeAttribute ("bwfx");
            accentFromState (*xml);
            xml->removeAttribute ("accA");
            xml->removeAttribute ("accB");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            engine.clearMemory();
            uiHasState = false;
            otherLoadedUi = -1;
            accentSeen = -1;
            lastSent.assign ((size_t) ids.size(), -999.0f);
            specLoadedUi = -1;
        }
}

//==============================================================================
void ArtefactAudioProcessor::setParamById (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float hi = ab::paramSpec (ab::paramIndex (id.toRawUTF8())).kind == ab::KP_INT
                       ? (float) (ab::kCatalog - 1) : 1.0f;
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v / hi));
        p->endChangeGesture();
    }
}

void ArtefactAudioProcessor::handleUiMessage (const juce::var& payload)
{
    //  the page batches into { b: [...] } (one bridge crossing per microtask)
    const auto b = payload.getProperty ("b", juce::var());
    if (auto* arr = b.getArray())
        for (const auto& m : *arr) handleOne (m);
    else
        handleOne (payload);
}

void ArtefactAudioProcessor::handleOne (const juce::var& m)
{
    const juce::String k = m.getProperty ("k", juce::var()).toString();

    if (k == "p")
    {
        const juce::String id = m.getProperty ("id", juce::var()).toString();
        const float v = (float) (double) m.getProperty ("v", 0.0);
        const int idx = ids.indexOf (id);
        if (idx >= 0)
        {
            lastSent[(size_t) idx] = v;      // suppress the echo
            setParamById (id, v);
        }
    }
    else if (k == "hello")   { uiHasState = false; uiReady = true; emitSite(); }
    else if (k == "ready")   { uiReady = true; }
    else if (k == "site")
    {
        //  the SITE panel: settings are global — every finding sees the change
        proxima::Settings s = site.settings();
        const bool wasClimate = s.climate;
        if (m.hasProperty ("climate"))  s.climate  = (bool) m.getProperty ("climate", false);
        if (m.hasProperty ("timing"))   s.timing   = (bool) m.getProperty ("timing", false);
        if (m.hasProperty ("distance")) s.distance = juce::jlimit (0.0f, 1.0f, (float) (double) m.getProperty ("distance", 0.5));
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
    else if (k == "panic")   { wantPanic = true; }
    else if (k == "note")
    {
        //  the harness test keys (human equipment, allowed to exist)
        const int note = (int) m.getProperty ("n", 60);
        if ((int) m.getProperty ("on", 0) != 0)
            engine.noteOn (note, (float) (double) m.getProperty ("v", 0.8));
        else
            engine.noteOff (note);
    }
    else if (k == "touch")
    {
        if ((int) m.getProperty ("on", 0) != 0)
            engine.touchOn ((int) m.getProperty ("n", 0),
                            (float) (double) m.getProperty ("v", 0.8));
        else
            engine.touchOff();
    }
    else if (k == "bwfx")    { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "presetSave") { presetSaveAs(); }
    else if (k == "presetOpen") { presetOpenDialog(); }
    else if (k == "survey")  { surveySent = false; }   // field view asks again
}

//==============================================================================
void ArtefactAudioProcessor::notice (const juce::String& msg)
{
    if (emitToUi)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("text", msg);
        emitToUi ("notice", juce::var (o));
    }
}

void ArtefactAudioProcessor::emitSpecimen()
{
    if (! emitToUi) return;
    const ab::Specimen& s = engine.specimen();

    auto* o = new juce::DynamicObject();
    o->setProperty ("cat", s.catalog);
    o->setProperty ("family", s.family);
    o->setProperty ("revivalSeconds", s.revivalSeconds);
    o->setProperty ("margin", s.harmonicMarginCents);
    o->setProperty ("nNodes", s.nNodes);

    juce::Array<juce::var> nodes;
    for (int i = 0; i < s.nNodes; ++i)
    {
        auto* nd = new juce::DynamicObject();
        nd->setProperty ("x", s.px[i]);
        nd->setProperty ("y", s.py[i]);
        nd->setProperty ("z", s.pz[i]);
        nd->setProperty ("w", s.pw[i]);
        nodes.add (juce::var (nd));
    }
    o->setProperty ("nodes", nodes);

    juce::Array<juce::var> edges;
    for (int e = 0; e < s.nEdges; ++e)
    {
        juce::Array<juce::var> pair;
        pair.add (s.edgeA[e]);
        pair.add (s.edgeB[e]);
        edges.add (juce::var (pair));
    }
    o->setProperty ("edges", edges);

    //  per-node param tags: which field organ this tissue answers to. A
    //  deterministic rule of the specimen's own statistics, so the same
    //  tissue always does the same thing (learnable), while different
    //  slices expose different sets (alive).
    {
        juce::Array<juce::var> tags;
        for (int i = 0; i < s.nNodes; ++i)
        {
            float deg = 0, hf = 0, loc = 0;
            for (int e = 0; e < s.nEdges; ++e)
                if (s.edgeA[e] == i || s.edgeB[e] == i) deg += s.edgeW[e];
            for (int k = s.nModes / 2; k < s.nModes; ++k)
                hf += s.vec[(size_t) k * s.nNodes + i] * s.vec[(size_t) k * s.nNodes + i];
            for (int k = 0; k < s.nModes / 6 + 1; ++k)
                loc += s.vec[(size_t) k * s.nNodes + i] * s.vec[(size_t) k * s.nNodes + i];
            //  tag: 0 aperture, 1 metabolism, 2 revival, 3 membrane, 4 gravity, 5 depth
            int tag;
            if (deg > 2.4f)            tag = 5;             // hubs = depth
            else if (hf > 0.6f)        tag = 4;             // bright tissue = gravity
            else if (loc > 0.28f)      tag = 2;             // slow tissue = revival
            else if (s.pw[i] > 0.35f)  tag = 1;             // deep-w = metabolism
            else if (s.pw[i] < -0.35f) tag = 3;             // far-w = membrane
            else                       tag = 0;             // skin = aperture
            tags.add (tag);
        }
        o->setProperty ("tags", tags);
    }

    emitToUi ("specimen", juce::var (o));
}

juce::String ArtefactAudioProcessor::accentToString (const float* a, int n) const
{
    bool any = false;
    for (int e = 0; e < n; ++e)
        if (a[e] > 0.0f && std::fabs (a[e] - 1.0f) > 1.0e-4f) { any = true; break; }
    if (! any) return {};
    juce::String s;
    for (int e = 0; e < n; ++e)
    {
        if (e) s << ',';
        s << juce::String ((int) std::lround ((a[e] <= 0.0f ? 1.0f : a[e]) * 1000.0f));
    }
    return s;
}

void ArtefactAudioProcessor::accentFromState (const juce::XmlElement& xml)
{
    const auto sa = xml.getStringAttribute ("accA");
    const auto sb = xml.getStringAttribute ("accB");
    if (sa.isEmpty() && sb.isEmpty()) { engine.clearAccent(); return; }
    std::vector<float> A, B;
    auto parse = [] (const juce::String& s, std::vector<float>& out)
    {
        out.clear();
        if (s.isEmpty()) return;
        auto toks = juce::StringArray::fromTokens (s, ",", "");
        for (const auto& t : toks)
            out.push_back (juce::jlimit (0.05f, 6.0f, t.getIntValue() * 0.001f));
    };
    parse (sa, A);
    parse (sb, B);
    engine.setAccent (A.empty() ? nullptr : A.data(), (int) A.size(),
                      B.empty() ? nullptr : B.data(), (int) B.size());
}

void ArtefactAudioProcessor::emitOther()
{
    if (! emitToUi) return;
    const ab::Specimen& s = engine.otherSpecimen();
    auto* o = new juce::DynamicObject();
    o->setProperty ("cat", s.catalog);
    o->setProperty ("family", s.family);
    o->setProperty ("nNodes", s.nNodes);
    o->setProperty ("voiceHz", s.voiceHz);
    //  how much of each other the two bodies can physically hear
    o->setProperty ("kin", spectralKinship (s, engine.specimen()));

    juce::Array<juce::var> nodes;
    for (int i = 0; i < s.nNodes; ++i)
    {
        auto* nd = new juce::DynamicObject();
        nd->setProperty ("x", s.px[i]);
        nd->setProperty ("y", s.py[i]);
        nd->setProperty ("z", s.pz[i]);
        nd->setProperty ("w", s.pw[i]);
        nodes.add (juce::var (nd));
    }
    o->setProperty ("nodes", nodes);
    juce::Array<juce::var> edges;
    for (int e = 0; e < s.nEdges; ++e)
    {
        juce::Array<juce::var> pair;
        pair.add (s.edgeA[e]);
        pair.add (s.edgeB[e]);
        edges.add (juce::var (pair));
    }
    o->setProperty ("edges", edges);
    emitToUi ("other", juce::var (o));
}

void ArtefactAudioProcessor::emitBwfx()
{
    if (emitToUi) emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

void ArtefactAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> ps;
    for (int i = 0; i < ab::numParams(); ++i)
    {
        auto* e = new juce::DynamicObject();
        const auto& s = ab::paramSpec (i);
        e->setProperty ("id", juce::String (s.id));
        e->setProperty ("name", juce::String (s.name));
        e->setProperty ("v", raw[(size_t) i]->load());
        e->setProperty ("kind", (int) s.kind);
        e->setProperty ("hi", s.kind == ab::KP_INT ? s.hi : 1.0f);
        ps.add (juce::var (e));
    }
    o->setProperty ("params", ps);
   #ifdef AB_BUILD_ID
    o->setProperty ("build", juce::String (AB_BUILD_ID));
   #endif
    emitToUi ("initialState", juce::var (o));
    emitSpecimen();
    /*  the second body belongs to the initial state as much as the first:
        emitOther() from the timer alone is lost whenever the page is not
        yet listening, and then never repeats — the panel showed no
        interlocutor while the engine was busily conversing with one. */
    if (engine.otherLoadedCatalog() >= 0) emitOther();
    emitBwfx();
    surveySent = false;
}

//==============================================================================
void ArtefactAudioProcessor::surveyStep()
{
    //  a few specimens per tick; the field fills in as the survey completes
    if (! surveyDone)
    {
        static thread_local ab::Specimen sp;
        int done = 0;
        while (surveyAt < ab::kCatalog && done < 6)
        {
            ab::generateSpecimen (surveyAt, sp);
            float fx, fy;
            ab::fieldPosition (sp, fx, fy);
            fieldX[(size_t) surveyAt] = fx;
            fieldY[(size_t) surveyAt] = fy;
            fieldFam[(size_t) surveyAt] = (uint8_t) sp.family;
            ++surveyAt;
            ++done;
        }
        if (surveyAt >= ab::kCatalog) surveyDone = true;
    }
    if (surveyDone && ! surveySent && emitToUi)
    {
        auto* o = new juce::DynamicObject();
        juce::Array<juce::var> xs, ys, fam;
        for (int c = 0; c < ab::kCatalog; ++c)
        {
            xs.add (fieldX[(size_t) c]);
            ys.add (fieldY[(size_t) c]);
            fam.add ((int) fieldFam[(size_t) c]);
        }
        o->setProperty ("x", xs);
        o->setProperty ("y", ys);
        o->setProperty ("fam", fam);
        emitToUi ("field", juce::var (o));
        surveySent = true;
    }
}

void ArtefactAudioProcessor::timerService()
{
    //  keep the engine's specimen in step with the parameter
    const int want = (int) std::lround (raw[(size_t) 0]->load());
    if (want != specLoadedUi)
    {
        //  a body dialled in by hand is a fresh being; one restored from a
        //  session (specLoadedUi still -1) keeps what it earned
        if (specLoadedUi >= 0) engine.forgetAccent (true, false);
        engine.loadSpecimen (want);
        specLoadedUi = want;
        emitSpecimen();
    }

    /*  the second body follows its parameter, and comes into existence the
        moment CONVERSE is raised above zero (generating a specimen is a
        message-thread job — the audio thread may never do it) */
    {
        const int wantO = (int) std::lround (raw[(size_t) ab::paramIndex ("other")]->load());
        const bool wantsConv = raw[(size_t) ab::paramIndex ("converse")]->load() > 0.0f;
        if (wantsConv && (wantO != otherLoadedUi || engine.otherLoadedCatalog() < 0))
        {
            if (otherLoadedUi >= 0) engine.forgetAccent (false, true);
            engine.loadOther (wantO);
            otherLoadedUi = wantO;
            emitOther();
        }
    }

    /*  PLASTICITY: the audio thread has decided the bodies are at rest and
        the scars are deep enough. Re-solving eigenmodes is our work, not
        its. Both bodies may have changed, so both are re-sent. */
    if (engine.remodelPending())
    {
        engine.serviceRemodel();
        emitSpecimen();
        emitOther();
    }

    if (! emitToUi) return;
    if (! uiHasState.exchange (true)) emitInitialState();

    //  echo changed params (host automation and the harness both land here)
    juce::Array<juce::var> batch;
    for (int i = 0; i < ab::numParams(); ++i)
    {
        const float v = raw[(size_t) i]->load();
        if (std::fabs (v - lastSent[(size_t) i]) > 1e-5f)
        {
            lastSent[(size_t) i] = v;
            auto* e = new juce::DynamicObject();
            e->setProperty ("id", ids[i]);
            e->setProperty ("v", v);
            batch.add (juce::var (e));
        }
    }
    if (! batch.isEmpty()) emitToUi ("params", juce::var (batch));

    //  the section + the tissue's living glow, streamed to the body
    if (++secDiv >= 1)
    {
        secDiv = 0;
        auto* o = new juce::DynamicObject();
        o->setProperty ("w", engine.sectionW());
        o->setProperty ("v", engine.sectionVel());

        //  per-node luminance: where the sound ACTUALLY lives on the body.
        //  (The first build summed vec^2 over modes — orthonormal, so the
        //  same at every node: a uniform glow carrying zero information.
        //  This is the real thing, from the realised per-mode amplitudes.)
        const ab::Specimen& s = engine.specimen();
        juce::Array<juce::var> lum;
        {
            float nl[ab::kMaxNodes] = { 0 };
            engine.nodeLuminance (nl);
            float pk = 1e-6f;
            for (int i = 0; i < s.nNodes; ++i) pk = std::max (pk, nl[i]);
            //  slow adaptive headroom so both a strike and its quiet bed read
            lumPeak = std::max (pk, lumPeak * 0.985f);
            const float sc = 1.0f / (lumPeak * 1.1f + 1e-6f);
            for (int i = 0; i < s.nNodes; ++i)
                lum.add (juce::jlimit (0.0f, 1.0f, std::sqrt (nl[i] * sc)));
        }
        o->setProperty ("lum", lum);

        /*  the second body's own glow — lit by what it hears as much as by
            what it says — and the state of the conversation */
        if (engine.otherLoadedCatalog() >= 0
            && raw[(size_t) ab::paramIndex ("converse")]->load() > 0.0f)
        {
            const ab::Specimen& os = engine.otherSpecimen();
            float nl[ab::kMaxNodes] = { 0 };
            engine.nodeLuminanceOther (nl);
            float pk = 1e-6f;
            for (int i = 0; i < os.nNodes; ++i) pk = std::max (pk, nl[i]);
            lumPeak2 = std::max (pk, lumPeak2 * 0.985f);
            const float sc = 1.0f / (lumPeak2 * 1.1f + 1e-6f);
            juce::Array<juce::var> lum2;
            for (int i = 0; i < os.nNodes; ++i)
                lum2.add (juce::jlimit (0.0f, 1.0f, std::sqrt (nl[i] * sc)));
            o->setProperty ("lum2", lum2);
            o->setProperty ("kin", engine.debugKinship());
            o->setProperty ("say", engine.debugSpeaking() ? 1 : 0);
        }
        o->setProperty ("accent", engine.accentSteps());
        o->setProperty ("scar", engine.debugScarMax());
        o->setProperty ("rest", engine.debugAtRest() ? 1 : 0);
        emitToUi ("sec", juce::var (o));
    }
}

//==============================================================================
//  patches — the house folder, app tag "artefact-b2311"
juce::String ArtefactAudioProcessor::patchJson (const juce::String& name)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("app", "artefact-b2311");
    root->setProperty ("kind", "patch");
    root->setProperty ("version", 1);
    root->setProperty ("name", name);
   #ifdef AB_BUILD_ID
    root->setProperty ("build", juce::String (AB_BUILD_ID));
   #endif
    auto* ps = new juce::DynamicObject();
    for (int i = 0; i < ab::numParams(); ++i)
        ps->setProperty (ab::paramSpec (i).id, (double) raw[(size_t) i]->load());
    root->setProperty ("params", juce::var (ps));
    root->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (root), false);
}

void ArtefactAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    const auto v = juce::JSON::parse (json);
    auto* o = v.getDynamicObject();
    if (o == nullptr) { notice ("NOT A SPECIMEN RECORD"); return; }
    const auto app = o->getProperty ("app").toString();
    if (app != "artefact-b2311") { notice ("A RECORD FROM ANOTHER EXPEDITION"); return; }

    if (auto* ps = o->getProperty ("params").getDynamicObject())
        for (int i = 0; i < ab::numParams(); ++i)
        {
            const auto& s = ab::paramSpec (i);
            if (ps->hasProperty (s.id))
                setParamById (s.id, (float) (double) ps->getProperty (s.id));
        }
    bwfxRack.fromJson (o->getProperty ("bwfx").toString().toStdString());
    engine.clearMemory();
    emitBwfx();
    notice ("RESTORED " + name.toUpperCase());
}

void ArtefactAudioProcessor::presetSaveAs()
{
    if (presetFolder == juce::File())
        presetFolder = brokild::patchFolder ("Artefact B2311.22", { "\"artefact-b2311\"" });
    activeChooser = std::make_unique<juce::FileChooser> (
        "Save observation", presetFolder.getChildFile ("Observation.json"), "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("json")) f = f.withFileExtension ("json");
            if (f.replaceWithText (patchJson (f.getFileNameWithoutExtension())))
                notice ("LOGGED " + f.getFileNameWithoutExtension().toUpperCase());
        });
}

void ArtefactAudioProcessor::presetOpenDialog()
{
    if (presetFolder == juce::File())
        presetFolder = brokild::patchFolder ("Artefact B2311.22", { "\"artefact-b2311\"" });
    activeChooser = std::make_unique<juce::FileChooser> ("Open observation", presetFolder, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile())
                applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
        });
}

//==============================================================================
juce::AudioProcessorEditor* ArtefactAudioProcessor::createEditor()
{
    return new ArtefactAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArtefactAudioProcessor();
}
