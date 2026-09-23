#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

using namespace tty;

namespace
{
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));
    const char* NOTEN[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String fmtValue (const PSpec& s, float v)
    {
        switch (s.kind)
        {
            case KP_SW:    return v >= 0.5f ? "ON" : "OFF";
            case KP_HZ:
            {
                const float f = lawHz (s, v);
                return f < 1.0f ? juce::String (f, 3) + " Hz" : f < 100.0f ? juce::String (f, 2) + " Hz"
                     : f < 1000.0f ? juce::String (f, 0) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
            }
            case KP_MS:
            {
                const float ms = lawHz (s, v);
                return ms < 100.0f ? juce::String (ms, 1) + " ms"
                     : ms < 1000.0f ? juce::String (ms, 0) + " ms" : juce::String (ms / 1000.0f, 2) + " s";
            }
            case KP_SEC:
            {
                const float sec = lawHz (s, v);
                if (sec >= 60.0f) return juce::String ((int) (sec / 60.0f)) + " m " + juce::String ((int) std::fmod (sec, 60.0f)) + " s";
                return juce::String (sec, 2) + " s";
            }
            case KP_GLIDE:
            {
                const float ms = v < 0.01f ? 0.0f : lawHz (s, v);
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
                if (juce::String (s.id) == "patch") return juce::String (preset (i).name);
                return juce::String (i);
            }
            case KP_NOTE:  { const int n = (int) std::round (v); return juce::String (NOTEN[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1); }
            case KP_SEMI:  { const float st = lawSemi (s, v); return (st > 0 ? "+" : "") + juce::String (st, 1) + " st"; }
            case KP_CENT:  { const float c = lawSemi (s, v); return (c >= 0 ? "+" : "") + juce::String (c, 0) + " c"; }
            case KP_CENTU: return juce::String (v * s.lo, 0) + " c";
            case KP_BIPOL: { const float c = (v - 0.5f) * 200.0f; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " %"; }
            case KP_VOL:   { const float g = lawVol (v); return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB"; }
            case KP_PW:    return juce::String (50.0f + v * 45.0f, 0) + " %";
            case KP_SHZ:   { const float h = lawShz (s, v); return (h >= 0 ? "+" : "") + juce::String (h, 1) + " Hz"; }
            case KP_DB:    return juce::String (lawDb (s, v), 1) + " dB";
            default:       return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }

    bool musical (const PSpec& s) { return paramInScene (s); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TTYAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        const float hi = paramMax (s);
        const bool stepped = paramStepped (s);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (s.kind == KP_INT || s.kind == KP_NOTE ? s.lo : 0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (! (s.flags & F_NA))
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (paramSpec (i), v); })));
    }
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
TTYAudioProcessor::TTYAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Aux In", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TTY", createParameterLayout())
{
    for (int i = 0; i < numParams(); ++i) ids.add (paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);
    for (auto& h : midiHeld) h = false;
    formats.registerBasicFormats();

    applyPatchIndex (0);              // a fresh instance opens on THE MACHINES KEPT WORKING, drone OFF
    startTimerHz (15);                // bwfxRack.service() and the capture snapshot, editor open or not
    bwfxRack.setWorldModConsumed (true);
}

TTYAudioProcessor::~TTYAudioProcessor() = default;

bool TTYAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (! (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void TTYAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < numParams(); ++i) engine.p.v[i] = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    auxCopy.setSize (2, juce::jmax (64, samplesPerBlock));
    setLatencySamples (engine.latency());
}

void TTYAudioProcessor::pushEvent (const UiEvent& e)
{
    const int w = evWrite.load();
    const int next = (w + 1) % EVQ;
    if (next == evRead.load()) return;
    evq[(size_t) w] = e;
    evWrite.store (next);
}

//==============================================================================
void TTYAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() <= 0) return;

    // the AUX input shares the buffer with the output: copy it out before clearing
    const float* aL = nullptr; const float* aR = nullptr;
    if (getBusCount (true) > 0 && getBus (true, 0)->isEnabled())
    {
        auto aux = getBusBuffer (buffer, true, 0);
        if (aux.getNumChannels() > 0 && auxCopy.getNumSamples() >= n)
        {
            auxCopy.copyFrom (0, 0, aux, 0, 0, n);
            auxCopy.copyFrom (1, 0, aux, aux.getNumChannels() > 1 ? 1 : 0, 0, n);
            aL = auxCopy.getReadPointer (0); aR = auxCopy.getReadPointer (1);
        }
    }
    auto outBus = getBusBuffer (buffer, false, 0);
    outBus.clear();
    const int nch = outBus.getNumChannels();
    if (nch <= 0) return;

    if (wantPanic.exchange (false)) { engine.panic(); for (auto& h : midiHeld) h = false; }

    for (int i = 0; i < numParams(); ++i) engine.p.v[i] = raw[(size_t) i]->load();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            const double bpm = pos->getBpm().hasValue() ? *pos->getBpm() : 0.0;
            const double ppq = pos->getPpqPosition().hasValue() ? *pos->getPpqPosition() : -1.0;
            engine.setTransport (bpm, ppq, pos->getIsPlaying());
            bwfxRack.setTransport (bpm, ppq, pos->getIsPlaying());
        }

    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth, wm.tremRate, wm.pitchSag, wm.filterMul);
    }

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
            case 6: engine.setAftertouch (e.v); hostAt = e.v; break;
            case 7: engine.strike (e.v); break;
            default: break;
        }
    }

    auto* L = outBus.getWritePointer (0);
    auto* R = nch >= 2 ? outBus.getWritePointer (1) : nullptr;
    std::vector<float> monoR;
    if (R == nullptr) { monoR.assign ((size_t) n, 0.0f); R = monoR.data(); }

    int last = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (pos > last) { engine.process (L + last, R + last, pos - last, aL ? aL + last : nullptr, aR ? aR + last : nullptr); last = pos; }
        const auto m = meta.getMessage();
        const int ch = m.getChannel();
        if      (m.isNoteOn())        { engine.noteOn (m.getNoteNumber(), m.getFloatVelocity(), ch); midiHeld[(size_t) m.getNoteNumber()] = true; }
        else if (m.isNoteOff())       { engine.noteOff (m.getNoteNumber(), ch); midiHeld[(size_t) m.getNoteNumber()] = false; }
        else if (m.isPitchWheel())    { const float b = (m.getPitchWheelValue() - 8192) / 8192.0f; engine.setBend (b, ch); if (! engine.p.sw (P_mpe) || ch < 2) hostBend = b; }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc == 1)       { engine.setWheel (v / 127.0f); hostWheel = v / 127.0f; }
            else if (cc == 64) engine.setSustain (v >= 64);
            else if (cc == 74 && engine.p.sw (P_mpe)) engine.setAftertouch (v / 127.0f, ch);
            else if (cc == 120 || cc == 123) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
        }
        else if (m.isChannelPressure()) { engine.setAftertouch (m.getChannelPressureValue() / 127.0f, ch); hostAt = m.getChannelPressureValue() / 127.0f; }
        else if (m.isAftertouch())      { engine.setPolyAftertouch (m.getNoteNumber(), m.getAfterTouchValue() / 127.0f); }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
    }
    if (n > last) engine.process (L + last, R + last, n - last, aL ? aL + last : nullptr, aR ? aR + last : nullptr);

    bwfx_juce::pushMacros (bwfxRack, apvts);
    bwfxRack.process (L, R, n);

    if (nch == 1) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
    if (engine.uiFracture) fractureSeen = true;
}

//==============================================================================
// ---- blobs -----------------------------------------------------------------
juce::var TTYAudioProcessor::macrosVar()
{
    juce::Array<juce::var> maps;
    for (int m = 0; m < NUM_MACROS; ++m)
    {
        juce::Array<juce::var> dests;
        for (int d = 0; d < MACRO_DESTS; ++d)
        {
            const auto& md = engine.macro[m][d];
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", md.dst >= 0 && md.dst < NUM_PARAMS ? juce::String (paramSpec (md.dst).id) : juce::String());
            o->setProperty ("depth", (double) md.depth);
            dests.add (juce::var (o));
        }
        maps.add (dests);
    }
    auto* o = new juce::DynamicObject(); o->setProperty ("maps", maps); return juce::var (o);
}
void TTYAudioProcessor::macrosFromVar (const juce::var& v)
{
    auto* maps = v.getProperty ("maps", juce::var()).getArray();
    if (maps == nullptr) return;
    for (int m = 0; m < NUM_MACROS && m < maps->size(); ++m)
    {
        auto* dests = (*maps)[m].getArray(); if (dests == nullptr) continue;
        for (int d = 0; d < MACRO_DESTS; ++d)
        {
            MacroDest md;
            if (d < dests->size()) { const auto id = (*dests)[d].getProperty ("id", "").toString(); md.dst = id.isEmpty() ? -1 : paramIndex (id.toRawUTF8()); md.depth = (float) (double) (*dests)[d].getProperty ("depth", 0.0); }
            engine.macro[m][d] = md;
        }
    }
}
juce::var TTYAudioProcessor::slotsVar()
{
    juce::Array<juce::var> arr;
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        const Slot& s = engine.life.slots[i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("i", i); o->setProperty ("src", s.src); o->setProperty ("via", s.via);
        o->setProperty ("dst", s.dst >= 0 && s.dst < NUM_PARAMS ? juce::String (paramSpec (s.dst).id) : juce::String());
        o->setProperty ("depth", (double) s.depth); o->setProperty ("offset", (double) s.offset); o->setProperty ("curve", s.curve);
        o->setProperty ("slew", (double) s.slew); o->setProperty ("lo", (double) s.lo); o->setProperty ("hi", (double) s.hi); o->setProperty ("on", s.on);
        arr.add (juce::var (o));
    }
    auto* o = new juce::DynamicObject(); o->setProperty ("slots", arr); return juce::var (o);
}
void TTYAudioProcessor::slotsFromVar (const juce::var& v)
{
    auto* arr = v.getProperty ("slots", juce::var()).getArray();
    if (arr == nullptr) return;
    for (int k = 0; k < NUM_SLOTS; ++k) engine.life.slots[k].on = false;
    for (const auto& e : *arr)
    {
        const int i = (int) e.getProperty ("i", -1); if (i < 0 || i >= NUM_SLOTS) continue;
        Slot s; s.src = (int) e.getProperty ("src", 0); s.via = (int) e.getProperty ("via", 0);
        const auto id = e.getProperty ("dst", "").toString(); s.dst = id.isEmpty() ? -1 : paramIndex (id.toRawUTF8());
        s.depth = (float) (double) e.getProperty ("depth", 0.0); s.offset = (float) (double) e.getProperty ("offset", 0.0);
        s.curve = (int) e.getProperty ("curve", 0); s.slew = (float) (double) e.getProperty ("slew", 0.0);
        s.lo = (float) (double) e.getProperty ("lo", -1.0); s.hi = (float) (double) e.getProperty ("hi", 1.0);
        s.on = (bool) e.getProperty ("on", false) && s.dst >= 0 && s.src > 0;
        Slot& t = engine.life.slots[i]; t.on = false; t.src = s.src; t.via = s.via; t.dst = s.dst; t.depth = s.depth; t.offset = s.offset; t.curve = s.curve; t.slew = s.slew; t.lo = s.lo; t.hi = s.hi; t.on = s.on;
    }
}
juce::var TTYAudioProcessor::msegVar()
{
    juce::Array<juce::var> shapes;
    for (int i = 0; i < 4; ++i)
    {
        const Mseg& m = engine.life.mseg[i];
        auto* o = new juce::DynamicObject(); o->setProperty ("trig", m.trig); o->setProperty ("loop", m.loop);
        juce::Array<juce::var> pts;
        for (int k = 0; k < m.n; ++k) { auto* q = new juce::DynamicObject(); q->setProperty ("t", (double) m.pts[k].t); q->setProperty ("l", (double) m.pts[k].level); q->setProperty ("c", (double) m.pts[k].curve); pts.add (juce::var (q)); }
        o->setProperty ("pts", pts); shapes.add (juce::var (o));
    }
    auto* o = new juce::DynamicObject(); o->setProperty ("shapes", shapes); return juce::var (o);
}
void TTYAudioProcessor::msegFromVar (const juce::var& v)
{
    auto* shapes = v.getProperty ("shapes", juce::var()).getArray();
    if (shapes == nullptr) return;
    for (int i = 0; i < 4 && i < shapes->size(); ++i)
    {
        const auto& sh = (*shapes)[i];
        Mseg m; m.trig = (int) sh.getProperty ("trig", 0); m.loop = (bool) sh.getProperty ("loop", false); m.n = 0;
        if (auto* pts = sh.getProperty ("pts", juce::var()).getArray())
            for (const auto& q : *pts) { if (m.n >= MSEG_MAX) break; m.pts[m.n++] = { (float) (double) q.getProperty ("t", 0.0), (float) (double) q.getProperty ("l", 0.0), (float) (double) q.getProperty ("c", 0.0) }; }
        if (m.n < 2) m.defaults();
        engine.life.mseg[i] = m;
    }
}
juce::var TTYAudioProcessor::scenesVar()
{
    juce::Array<juce::var> set, names, vals;
    for (int i = 0; i < NUM_SCENES; ++i)
    {
        set.add (engine.sceneSet[i]); names.add (sceneNames[i]);
        auto* o = new juce::DynamicObject();
        if (engine.sceneSet[i]) for (int k = 0; k < NUM_PARAMS; ++k) if (musical (paramSpec (k))) o->setProperty (ids[k], (double) engine.scene[i].v[k]);
        vals.add (juce::var (o));
    }
    auto* o = new juce::DynamicObject(); o->setProperty ("set", set); o->setProperty ("names", names); o->setProperty ("values", vals); return juce::var (o);
}
void TTYAudioProcessor::scenesFromVar (const juce::var& v)
{
    auto* set = v.getProperty ("set", juce::var()).getArray(); auto* names = v.getProperty ("names", juce::var()).getArray(); auto* vals = v.getProperty ("values", juce::var()).getArray();
    for (int i = 0; i < NUM_SCENES; ++i)
    {
        engine.sceneSet[i] = set != nullptr && i < set->size() && (bool) (*set)[i];
        if (names != nullptr && i < names->size()) sceneNames[i] = (*names)[i].toString();
        engine.scene[i] = Params();
        for (int k = 0; k < NUM_PARAMS; ++k) engine.scene[i].v[k] = raw[(size_t) k]->load();
        if (vals != nullptr && i < vals->size())
            if (auto* o = (*vals)[i].getDynamicObject())
                for (const auto& kv : o->getProperties()) { const int k = ids.indexOf (kv.name.toString()); if (k >= 0) engine.scene[i].v[k] = juce::jlimit (0.0f, paramMax (paramSpec (k)), (float) (double) kv.value); }
    }
}
juce::var TTYAudioProcessor::scalaVar()
{
    auto* o = new juce::DynamicObject(); juce::Array<juce::var> c; for (float v : scalaCents) c.add ((double) v);
    o->setProperty ("cents", c); o->setProperty ("period", (double) scalaPeriod); o->setProperty ("name", scalaName); return juce::var (o);
}
void TTYAudioProcessor::scalaFromVar (const juce::var& v)
{
    scalaCents.clear();
    if (auto* c = v.getProperty ("cents", juce::var()).getArray()) for (const auto& x : *c) scalaCents.push_back ((float) (double) x);
    scalaPeriod = (float) (double) v.getProperty ("period", 1200.0); scalaName = v.getProperty ("name", "").toString();
    engine.setScala (scalaCents.data(), (int) scalaCents.size(), scalaPeriod);
}
juce::String TTYAudioProcessor::captureBase64()
{
    const Clip* c = engine.mem.captured.load();
    if (c == nullptr || c->data.empty() || ! rememberCapture) return {};
    juce::MemoryBlock mb; mb.setSize (c->data.size() * 2);
    auto* d = static_cast<int16_t*> (mb.getData());
    for (size_t i = 0; i < c->data.size(); ++i) d[i] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lround (juce::jlimit (-1.0f, 1.0f, c->data[i]) * 32767.0f));
    return juce::String ((int) c->rate) + ":" + mb.toBase64Encoding();
}
void TTYAudioProcessor::captureFromBase64 (const juce::String& s)
{
    if (s.isEmpty()) return;
    const int colon = s.indexOfChar (':'); if (colon < 0) return;
    const double rate = s.substring (0, colon).getDoubleValue();
    juce::MemoryBlock mb; if (! mb.fromBase64Encoding (s.substring (colon + 1))) return;
    auto clip = std::make_unique<Clip>(); clip->rate = rate > 1000 ? rate : engine.sr; clip->name = "remembered";
    const auto* d = static_cast<const int16_t*> (mb.getData()); const size_t n = mb.getSize() / 2;
    clip->data.resize (n); for (size_t i = 0; i < n; ++i) clip->data[i] = d[i] / 32767.0f;
    engine.mem.captured.store (clip.get());
    clips.push_back (std::move (clip)); rememberCapture = true;
}

//==============================================================================
void TTYAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        xml->setAttribute ("loadedName", loadedName);
        xml->setAttribute ("macros", juce::JSON::toString (macrosVar(), true));
        xml->setAttribute ("slots", juce::JSON::toString (slotsVar(), true));
        xml->setAttribute ("mseg", juce::JSON::toString (msegVar(), true));
        xml->setAttribute ("scenes", juce::JSON::toString (scenesVar(), true));
        xml->setAttribute ("scala", juce::JSON::toString (scalaVar(), true));
        xml->setAttribute ("importPath", importPath);
        const auto cap = captureBase64(); if (cap.isNotEmpty()) xml->setAttribute ("capture", cap);
       #ifdef TTY_BUILD_ID
        xml->setAttribute ("build", TTY_BUILD_ID);
       #endif
        copyXmlToBinary (*xml, dest);
    }
}

void TTYAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            loadedName = xml->getStringAttribute ("loadedName");
            juce::var v;
            if (juce::JSON::parse (xml->getStringAttribute ("macros"), v).wasOk()) macrosFromVar (v);
            if (juce::JSON::parse (xml->getStringAttribute ("slots"), v).wasOk()) slotsFromVar (v);
            if (juce::JSON::parse (xml->getStringAttribute ("mseg"), v).wasOk()) msegFromVar (v);
            if (juce::JSON::parse (xml->getStringAttribute ("scala"), v).wasOk()) scalaFromVar (v);
            importPath = xml->getStringAttribute ("importPath");
            const auto cap = xml->getStringAttribute ("capture");
            for (const char* a : { "bwfx", "loadedName", "macros", "slots", "mseg", "scenes", "scala", "importPath", "capture", "build" }) xml->removeAttribute (a);
            // scenes are restored AFTER the values, since an unset scene defaults to them
            juce::String scenesText;
            {
                // (the attribute was removed above; read it again from the original block)
                if (auto xml2 = getXmlFromBinary (data, size)) scenesText = xml2->getStringAttribute ("scenes");
            }
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (juce::JSON::parse (scenesText, v).wasOk()) scenesFromVar (v);
            captureFromBase64 (cap);
            if (importPath.isNotEmpty()) { const juce::File f (importPath); if (f.existsAsFile()) importClip (f); else notice ("IMPORTED FILE NOT FOUND: " + f.getFileName().toUpperCase()); }
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
            lastPatchSent = -2;
            emitBwfx();
        }
}

juce::AudioProcessorEditor* TTYAudioProcessor::createEditor() { return new TTYAudioProcessorEditor (*this); }

//==============================================================================
void TTYAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void TTYAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

void TTYAudioProcessor::applyParamsStruct (const Params& q, bool musicalOnly)
{
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        const juce::String id (s.id);
        if (id == "volume" || id == "quality" || id == "bend" || id == "patch" || id == "drone" || id == "seed" || id == "determin" || id == "mpe") continue;   // the player's, not the patch's
        if (musicalOnly && ! musical (s)) continue;
        setParamById (s.id, q.v[i]);
    }
}

void TTYAudioProcessor::takeSnapshot (Snapshot& s)
{
    for (int i = 0; i < NUM_PARAMS; ++i) s.p.v[i] = raw[(size_t) i]->load();
    s.rack = bwfxRack.toJson();
    for (int i = 0; i < NUM_SCENES; ++i) { s.scene[i] = engine.scene[i]; s.sceneSet[i] = engine.sceneSet[i]; }
    for (int m = 0; m < NUM_MACROS; ++m) for (int d = 0; d < MACRO_DESTS; ++d) s.macro[m][d] = engine.macro[m][d];
    for (int i = 0; i < NUM_SLOTS; ++i) s.slots[i] = engine.life.slots[i];
    for (int i = 0; i < 4; ++i) s.mseg[i] = engine.life.mseg[i];
    s.valid = true;
}
void TTYAudioProcessor::applySnapshot (const Snapshot& s)
{
    if (! s.valid) return;
    applyParamsStruct (s.p, false);
    bwfxRack.fromJson (s.rack);
    for (int i = 0; i < NUM_SCENES; ++i) { engine.scene[i] = s.scene[i]; engine.sceneSet[i] = s.sceneSet[i]; }
    for (int m = 0; m < NUM_MACROS; ++m) for (int d = 0; d < MACRO_DESTS; ++d) engine.macro[m][d] = s.macro[m][d];
    for (int i = 0; i < NUM_SLOTS; ++i) { engine.life.slots[i].on = false; engine.life.slots[i] = s.slots[i]; }
    for (int i = 0; i < 4; ++i) engine.life.mseg[i] = s.mseg[i];
    emitBwfx(); emitMacros(); emitSlots(); emitMseg(); emitScenes();
}
void TTYAudioProcessor::snapshotForUndo() { takeSnapshot (undoSnap); }
void TTYAudioProcessor::undo()
{
    if (! undoSnap.valid) { notice ("NOTHING TO UNDO"); return; }
    Snapshot now; takeSnapshot (now);
    applySnapshot (undoSnap); undoSnap = now;
    loadedName = "*"; lastPatchSent = -2; emitPatchInfo(); notice ("UNDONE");
}
void TTYAudioProcessor::abStore (int slot) { takeSnapshot (ab[slot & 1]); notice (juce::String ("STORED ") + (slot ? "B" : "A")); }
void TTYAudioProcessor::abRecall (int slot)
{
    if (! ab[slot & 1].valid) { notice (juce::String (slot ? "B" : "A") + " IS EMPTY"); return; }
    snapshotForUndo(); applySnapshot (ab[slot & 1]); loadedName = "*"; lastPatchSent = -2; emitPatchInfo(); notice (juce::String ("RECALLED ") + (slot ? "B" : "A"));
}

void TTYAudioProcessor::applyPatchIndex (int i)
{
    i = juce::jlimit (0, numPresets() - 1, i);
    if (undoSnap.valid || ids.size() > 0) snapshotForUndo();
    Params q;
    applyPreset (i, q, engine.macro, engine.scene, engine.sceneSet, engine.life.slots, engine.life.mseg);
    applyParamsStruct (q, false);
    setParamById ("patch", (float) i);
    for (int k = 0; k < NUM_SCENES; ++k) sceneNames[k] = k == 0 ? "AWAKENING" : k == 1 ? "OCCUPATION" : k == 2 ? "COLLAPSE" : "AFTERMATH";
    bwfxRack.clearState();            // a patch stores its own rack; the factory ones carry none
    emitBwfx(); emitMacros(); emitSlots(); emitMseg(); emitScenes();
    loadedName = {};
    lastPatchSent = -2;
    emitPatchInfo();
    notice (juce::String ("PRESET ") + juce::String (i + 1).paddedLeft ('0', 2) + " " + DOT + " " + preset (i).name + " " + DOT + " " + preset (i).cat);
}

void TTYAudioProcessor::mutate (float amount, int lock)
{
    snapshotForUndo();
    Params q; for (int i = 0; i < NUM_PARAMS; ++i) q.v[i] = raw[(size_t) i]->load();
    mutatePatch ((uint32_t) juce::Time::getHighResolutionTicks(), juce::jlimit (0.05f, 1.0f, amount), lock, q);
    applyParamsStruct (q, true);
    loadedName = "*"; lastPatchSent = -2; emitPatchInfo();
    notice ("MUTATED " + juce::String ((int) std::round (amount * 100)) + " %");
}

void TTYAudioProcessor::sceneOp (const juce::String& op, int i, const juce::String& name)
{
    if (i < 0 || i >= NUM_SCENES) return;
    if (op == "store")
    {
        for (int k = 0; k < NUM_PARAMS; ++k) engine.scene[i].v[k] = raw[(size_t) k]->load();
        engine.sceneSet[i] = true;
        notice ("SCENE " + sceneNames[i] + " STORED");
    }
    else if (op == "recall")
    {
        if (! engine.sceneSet[i]) { notice ("SCENE " + sceneNames[i] + " IS EMPTY"); return; }
        snapshotForUndo();
        applyParamsStruct (engine.scene[i], true);
        const float pos = i == 0 ? 0.0f : i == 1 ? raw[(size_t) P_h_sc2pos]->load() : i == 2 ? raw[(size_t) P_h_sc3pos]->load() : 1.0f;
        setParamById ("h_pos", pos);
        notice ("SCENE " + sceneNames[i] + " RECALLED");
    }
    else if (op == "clear") { engine.sceneSet[i] = false; notice ("SCENE " + sceneNames[i] + " CLEARED"); }
    else if (op == "name") { if (name.isNotEmpty()) sceneNames[i] = name.substring (0, 24).toUpperCase(); }
    emitScenes();
}

void TTYAudioProcessor::importClip (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) { notice ("COULD NOT READ " + f.getFileName().toUpperCase()); return; }
    const int frames = (int) juce::jmin ((juce::int64) (reader->sampleRate * 120.0), reader->lengthInSamples);   // up to two minutes
    juce::AudioBuffer<float> buf ((int) reader->numChannels, juce::jmax (1, frames));
    reader->read (&buf, 0, frames, 0, true, true);
    auto clip = std::make_unique<Clip>();
    clip->rate = engine.sr; clip->name = "import";
    // mono, resampled to the engine rate by linear interpolation
    const double ratio = reader->sampleRate / engine.sr;
    const int outN = (int) (frames / ratio);
    clip->data.resize ((size_t) std::max (1, outN));
    for (int i = 0; i < outN; ++i)
    {
        const double sp = i * ratio; const int i0 = (int) sp; const float fr = (float) (sp - i0);
        float a = 0.0f, b = 0.0f;
        for (int c = 0; c < (int) reader->numChannels; ++c) { a += buf.getSample (c, juce::jmin (frames - 1, i0)); b += buf.getSample (c, juce::jmin (frames - 1, i0 + 1)); }
        clip->data[(size_t) i] = (a + (b - a) * fr) / (float) reader->numChannels;
    }
    float pk = 1.0e-6f; for (float v : clip->data) pk = std::max (pk, std::abs (v));
    for (float& v : clip->data) v *= 0.8f / pk;
    engine.mem.imported.store (clip.get());
    clips.push_back (std::move (clip));
    while (clips.size() > 6) { /* keep old pointers alive: the engine may still read one for a block; trim only far back */ if (clips.size() > 12) clips.erase (clips.begin()); else break; }
    importPath = f.getFullPathName(); importName = f.getFileNameWithoutExtension();
    notice ("IMPORTED " + importName.toUpperCase() + " " + DOT + " " + juce::String (outN / engine.sr, 1) + " S");
}

void TTYAudioProcessor::importDialog()
{
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Import a sound into MEMORY", start, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) importClip (f); });
}

void TTYAudioProcessor::scalaFile (const juce::File& f)
{
    juce::StringArray lines; f.readLines (lines);
    std::vector<float> cents; int count = -1; bool haveDesc = false;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.startsWith ("!") || line.isEmpty()) continue;
        if (! haveDesc) { haveDesc = true; continue; }
        if (count < 0) { count = line.getIntValue(); continue; }
        auto tok = line.upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false);
        float c = 0.0f;
        if (tok.containsChar ('.')) c = tok.getFloatValue();
        else if (tok.containsChar ('/')) { const float a = tok.upToFirstOccurrenceOf ("/", false, false).getFloatValue(), b = tok.fromFirstOccurrenceOf ("/", false, false).getFloatValue(); if (a > 0 && b > 0) c = 1200.0f * std::log2 (a / b); }
        else c = 1200.0f * std::log2 (std::max (1.0f, tok.getFloatValue()));
        cents.push_back (c);
    }
    if (cents.empty()) { notice ("NOT A SCALA FILE: " + f.getFileName().toUpperCase()); return; }
    scalaPeriod = cents.back(); cents.pop_back();          // the last entry is the period (2/1 for an octave)
    scalaCents = cents; scalaName = f.getFileNameWithoutExtension();
    engine.setScala (scalaCents.data(), (int) scalaCents.size(), scalaPeriod);
    setParamById ("scale", 9.0f);
    notice ("SCALA " + scalaName.toUpperCase() + " " + DOT + " " + juce::String ((int) scalaCents.size() + 1) + " NOTES PER " + juce::String (scalaPeriod, 0) + " C");
}

void TTYAudioProcessor::scalaDialog()
{
    activeChooser = std::make_unique<juce::FileChooser> ("Import a Scala tuning", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.scl");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) scalaFile (f); });
}

/*  A stopped capture becomes a stable Clip, copied here on the message thread. */
void TTYAudioProcessor::snapshotCapture()
{
    const int len = (int) engine.mem.capLen;
    if (len < 1000) return;
    auto clip = std::make_unique<Clip>(); clip->rate = engine.sr; clip->name = "capture";
    clip->data.resize ((size_t) len);
    // the ring: capW is where the next write goes; the last `len` samples end there
    const int cap = (int) engine.mem.capRing.size();
    const int start = ((engine.mem.capW - len) % cap + cap) % cap;
    for (int i = 0; i < len; ++i) clip->data[(size_t) i] = engine.mem.capRing[(size_t) ((start + i) % cap)];
    engine.mem.captured.store (clip.get());
    clips.push_back (std::move (clip));
    rememberCapture = false;
    notice ("CAPTURED " + juce::String (len / engine.sr, 1) + " S " + DOT + " REMEMBER TO KEEP IT");
}

//==============================================================================
void TTYAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* o = payload.getDynamicObject())
    {
        const auto batch = o->getProperty ("b");
        if (auto* arr = batch.getArray()) { for (const auto& m : *arr) handleOne (m); return; }
    }
    handleOne (payload);
}

void TTYAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = m.getProperty ("k", juce::var()).toString();

    if (k == "p")
    {
        setParamById (m.getProperty ("id", juce::var()).toString(), (float) (double) m.getProperty ("v", juce::var()), true);
        if (loadedName.isEmpty()) { loadedName = "*"; lastPatchSent = -2; }   // edited
    }
    else if (k == "ack")    { uiHasState = true; }
    else if (k == "ready")  { uiReady = true; }
    else if (k == "hello")  { uiHasState = false; }
    else if (k == "patch")  { applyPatchIndex ((int) m.getProperty ("i", juce::var())); }
    else if (k == "mutate") { mutate ((float) (double) m.getProperty ("amount", 0.3), (int) m.getProperty ("lock", 0)); }
    else if (k == "undo")   { undo(); }
    else if (k == "ab")     { const bool b = m.getProperty ("slot", "A").toString() == "B"; if (m.getProperty ("op", "").toString() == "store") abStore (b); else abRecall (b); }
    else if (k == "panic")  { wantPanic = true; setParamById ("drone", 0.0f); notice ("PANIC " + DOT + " STOPPED " + DOT + " PLAY A NOTE OR SWITCH DRONE ON"); }
    else if (k == "strike") { UiEvent e; e.kind = 7; e.v = o->hasProperty ("v") ? (float) (double) m.getProperty ("v", juce::var()) : -1.0f; pushEvent (e); }
    else if (k == "note")
    {
        UiEvent e; e.kind = (bool) m.getProperty ("on", juce::var()) ? 1 : 2; e.note = (int) m.getProperty ("n", juce::var());
        e.v = o->hasProperty ("v") ? (float) (double) m.getProperty ("v", juce::var()) : 0.8f;
        pushEvent (e);
    }
    else if (k == "bend")   { UiEvent e; e.kind = 3; e.v = (float) (double) m.getProperty ("v", juce::var()); pushEvent (e); }
    else if (k == "wheel")  { UiEvent e; e.kind = 4; e.v = (float) (double) m.getProperty ("v", juce::var()); pushEvent (e); }
    else if (k == "at")     { UiEvent e; e.kind = 6; e.v = (float) (double) m.getProperty ("v", juce::var()); pushEvent (e); }
    else if (k == "alloff") { UiEvent e; e.kind = 5; pushEvent (e); }
    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan")   { presetScan(); }
    else if (k == "presetFolder") { presetPickFolder(); }
    else if (k == "presetLoad")   { presetLoad (m.getProperty ("path", juce::var()).toString()); }
    else if (k == "scene")  { sceneOp (m.getProperty ("op", "").toString(), (int) m.getProperty ("i", 0), m.getProperty ("name", "").toString()); }
    else if (k == "macro")
    {
        const int mi = juce::jlimit (0, NUM_MACROS - 1, (int) m.getProperty ("m", 0));
        if (m.getProperty ("op", "").toString() == "clear") { for (int d = 0; d < MACRO_DESTS; ++d) engine.macro[mi][d] = MacroDest(); }
        else
        {
            const int d = juce::jlimit (0, MACRO_DESTS - 1, (int) m.getProperty ("d", 0));
            const auto id = m.getProperty ("id", "").toString();
            MacroDest md; md.dst = id.isEmpty() ? -1 : paramIndex (id.toRawUTF8()); md.depth = juce::jlimit (-1.0f, 1.0f, (float) (double) m.getProperty ("depth", 0.0));
            engine.macro[mi][d] = md;
        }
        emitMacros();
    }
    else if (k == "slot")
    {
        const int i = (int) m.getProperty ("i", -1);
        if (i >= 0 && i < NUM_SLOTS)
        {
            Slot& s = engine.life.slots[i]; s.on = false;
            s.src = juce::jlimit (0, NUM_MODSRC - 1, (int) m.getProperty ("src", 0)); s.via = juce::jlimit (0, NUM_MODSRC - 1, (int) m.getProperty ("via", 0));
            const auto id = m.getProperty ("dst", "").toString(); s.dst = id.isEmpty() ? -1 : paramIndex (id.toRawUTF8());
            s.depth = juce::jlimit (-1.0f, 1.0f, (float) (double) m.getProperty ("depth", 0.0)); s.offset = juce::jlimit (-1.0f, 1.0f, (float) (double) m.getProperty ("offset", 0.0));
            s.curve = juce::jlimit (0, 3, (int) m.getProperty ("curve", 0)); s.slew = juce::jlimit (0.0f, 60.0f, (float) (double) m.getProperty ("slew", 0.0));
            s.lo = juce::jlimit (-1.0f, 1.0f, (float) (double) m.getProperty ("lo", -1.0)); s.hi = juce::jlimit (-1.0f, 1.0f, (float) (double) m.getProperty ("hi", 1.0));
            s.on = (bool) m.getProperty ("on", true) && s.dst >= 0 && s.src > 0 && (s.dst < NUM_PARAMS && paramModulatable (paramSpec (s.dst)));
            emitSlots();
        }
    }
    else if (k == "mseg")
    {
        const int i = (int) m.getProperty ("i", -1);
        if (i >= 0 && i < 4)
        {
            Mseg mm; mm.trig = juce::jlimit (0, 2, (int) m.getProperty ("trig", 0)); mm.loop = (bool) m.getProperty ("loop", false); mm.n = 0;
            if (auto* pts = m.getProperty ("pts", juce::var()).getArray())
                for (const auto& q : *pts) { if (mm.n >= MSEG_MAX) break; mm.pts[mm.n++] = { std::max (0.0f, (float) (double) q.getProperty ("t", 0.0)), clamp01 ((float) (double) q.getProperty ("l", 0.0)), juce::jlimit (-1.0f, 1.0f, (float) (double) q.getProperty ("c", 0.0)) }; }
            if (mm.n < 2) mm.defaults();
            for (int p2 = 1; p2 < mm.n; ++p2) mm.pts[p2].t = std::max (mm.pts[p2].t, mm.pts[p2 - 1].t + 0.001f);
            engine.life.mseg[i] = mm; emitMseg();
        }
    }
    else if (k == "capture")  { const bool on = (bool) m.getProperty ("on", false); engine.setCapturing (on); if (on) { captureWasOn = true; rememberCapture = false; notice ("CAPTURING " + DOT + " 30 S RING"); } }
    else if (k == "remember") { if (engine.mem.captured.load() != nullptr) { rememberCapture = true; notice ("REMEMBERED " + DOT + " THE CAPTURE IS SAVED WITH THE PATCH"); } else notice ("NOTHING CAPTURED YET"); }
    else if (k == "import")     { importDialog(); }
    else if (k == "importPath") { const juce::File f (m.getProperty ("path", "").toString()); if (f.existsAsFile()) importClip (f); else notice ("NO SUCH FILE"); }
    else if (k == "scala")      { scalaDialog(); }
    else if (k == "scalaPath")  { const juce::File f (m.getProperty ("path", "").toString()); if (f.existsAsFile()) scalaFile (f); else notice ("NO SUCH FILE"); }
}

//==============================================================================
void TTYAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        const auto& s = paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("hi", (double) paramMax (s));
        e->setProperty ("n", juce::String (s.name));
        e->setProperty ("kind", s.kind);
        e->setProperty ("def", (double) s.def);
        e->setProperty ("lo", (double) s.lo);
        e->setProperty ("fhi", (double) s.hi);
        e->setProperty ("flags", s.flags);
        int nn = 0; auto names = listNames (s.id, nn);
        if (names != nullptr && nn > 0)
        {
            juce::Array<juce::var> list;
            for (int j = 0; j < nn; ++j) list.add (juce::String (names[j]));
            e->setProperty ("names", list);
        }
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }
    juce::Array<juce::var> presets;
    for (int i = 0; i < numPresets(); ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", i); e->setProperty ("name", juce::String (preset (i).name)); e->setProperty ("cat", juce::String (preset (i).cat)); e->setProperty ("note", juce::String (preset (i).note));
        presets.add (juce::var (e));
    }
    juce::Array<juce::var> sources; for (int i = 0; i < NUM_MODSRC; ++i) sources.add (juce::String (modSourceName (i)));
    juce::Array<juce::var> macroNames; for (const char* nm : { "MASS", "DREAD", "VIOLENCE", "INSTABILITY", "CONTAMINATION", "DISTANCE", "LIFE", "HUMANITY" }) macroNames.add (juce::String (nm));
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("product", "Thirty Thousand Years");
    obj->setProperty ("params", ps);
    obj->setProperty ("presets", presets);
    obj->setProperty ("sources", sources);
    obj->setProperty ("macroNames", macroNames);
    obj->setProperty ("latency", engine.latency());
    obj->setProperty ("memLatency", engine.mem.latencySamples);
    obj->setProperty ("sr", engine.sr);
   #ifdef TTY_BUILD_ID
    obj->setProperty ("build", juce::String (TTY_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx(); emitMacros(); emitSlots(); emitMseg(); emitScenes();
    lastPatchSent = -2;
    emitPatchInfo();
}

void TTYAudioProcessor::emitPatchInfo()
{
    if (! emitToUi) return;
    const int i = juce::jlimit (0, numPresets() - 1, (int) std::round (raw[(size_t) P_patch]->load()));
    auto* o = new juce::DynamicObject();
    if (loadedName.isEmpty())      { o->setProperty ("i", i); o->setProperty ("name", juce::String (preset (i).name)); o->setProperty ("cat", juce::String (preset (i).cat)); o->setProperty ("note", juce::String (preset (i).note)); }
    else if (loadedName == "*")    { o->setProperty ("i", i); o->setProperty ("name", juce::String (preset (i).name) + " *"); o->setProperty ("cat", juce::String (preset (i).cat)); o->setProperty ("note", juce::String (preset (i).note)); }
    else                           { o->setProperty ("i", -1); o->setProperty ("name", loadedName.toUpperCase()); o->setProperty ("cat", juce::String ("USER")); o->setProperty ("note", juce::String()); }
    emitToUi ("patchinfo", juce::var (o));
    lastPatchSent = i;
}

void TTYAudioProcessor::emitBwfx()   { if (emitToUi) emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack)); }
void TTYAudioProcessor::emitMacros() { if (emitToUi) emitToUi ("macros", macrosVar()); }
void TTYAudioProcessor::emitSlots()  { if (emitToUi) emitToUi ("slots", slotsVar()); }
void TTYAudioProcessor::emitMseg()   { if (emitToUi) emitToUi ("mseg", msegVar()); }
void TTYAudioProcessor::emitScenes()
{
    if (! emitToUi) return;
    juce::Array<juce::var> set, names; for (int i = 0; i < NUM_SCENES; ++i) { set.add (engine.sceneSet[i]); names.add (sceneNames[i]); }
    auto* o = new juce::DynamicObject(); o->setProperty ("set", set); o->setProperty ("names", names);
    emitToUi ("scenes", juce::var (o));
}

void TTYAudioProcessor::emitParamEcho()
{
    juce::Array<juce::var> changed;
    for (int i = 0; i < ids.size(); ++i)
    {
        const float v = raw[(size_t) i]->load();
        if (std::abs (v - lastSent[(size_t) i]) > 1.0e-5f)
        {
            lastSent[(size_t) i] = v;
            auto* e = new juce::DynamicObject(); e->setProperty ("id", ids[i]); e->setProperty ("v", (double) v);
            changed.add (juce::var (e));
        }
    }
    if (! changed.isEmpty()) { auto* obj = new juce::DynamicObject(); obj->setProperty ("p", changed); emitToUi ("hostParam", juce::var (obj)); }
}

void TTYAudioProcessor::emitEff()
{
    juce::Array<juce::var> v; v.ensureStorageAllocated (NUM_PARAMS);
    for (int i = 0; i < NUM_PARAMS; ++i) v.add ((double) engine.eff.v[i]);
    auto* o = new juce::DynamicObject(); o->setProperty ("v", v);
    emitToUi ("eff", juce::var (o));
}

void TTYAudioProcessor::emitMeter()
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("out",  (double) engine.outRms);
    obj->setProperty ("peak", (double) engine.outPeak);
    obj->setProperty ("lim",  (double) engine.limReduction);
    obj->setProperty ("loop", (double) engine.loopEnergy);
    obj->setProperty ("space", (double) engine.spaceEnergy);
    juce::Array<juce::var> strata, notes, levels, held, spectrum, scope;
    for (int b = 0; b < 4; ++b) strata.add ((double) engine.stratumAct[b]);
    for (int v = 0; v < MAX_VOICES; ++v) { notes.add (engine.uiNotes[(size_t) v]); levels.add ((double) engine.uiLevels[(size_t) v]); }
    for (int n = 0; n < 128; ++n) if (midiHeld[(size_t) n]) held.add (n);
    for (int b = 0; b < SPEC_BANDS; ++b) spectrum.add ((double) engine.uiSpectrum[b]);
    const int w = engine.scopeWrite;
    for (int i = 0; i < 256; ++i) scope.add ((double) engine.scope[(size_t) ((w - 256 + i) & (SCOPE_N - 1))]);
    obj->setProperty ("strata", strata); obj->setProperty ("notes", notes); obj->setProperty ("levels", levels); obj->setProperty ("held", held);
    obj->setProperty ("history", (double) engine.historyPos);
    obj->setProperty ("grains", engine.uiGrains); obj->setProperty ("memAct", (double) engine.uiMemAct); obj->setProperty ("erosion", (double) engine.uiErosion);
    obj->setProperty ("voices", engine.uiVoicesUsed);
    obj->setProperty ("spectrum", spectrum); obj->setProperty ("scope", scope);
    {
        auto* mods = new juce::DynamicObject();
        juce::Array<juce::var> lfo, env, shape, rnd, fol, evt, ne, nb, nt;
        for (int i = 0; i < NUM_LFO; ++i) lfo.add ((double) engine.life.lfoVal[i]);
        for (int i = 0; i < NUM_ENV; ++i) env.add ((double) engine.life.src[MS_ENV1 + i]);
        for (int i = 0; i < 4; ++i) shape.add ((double) engine.life.uiMsegVal[i]);
        for (int i = 0; i < NUM_STO; ++i) rnd.add ((double) engine.life.src[MS_RND1 + i]);
        for (int i = 0; i < NUM_FOL; ++i) fol.add ((double) engine.life.src[MS_FOL1 + i]);
        for (int i = 0; i < NUM_EVT; ++i) evt.add ((double) engine.life.src[MS_EVT1 + i]);
        for (int i = 0; i < 5; ++i) { ne.add ((double) engine.life.det[i].energy); nb.add ((double) engine.life.det[i].bright); nt.add ((double) engine.life.det[i].trans); }
        auto* net = new juce::DynamicObject(); net->setProperty ("e", ne); net->setProperty ("b", nb); net->setProperty ("t", nt);
        mods->setProperty ("lfo", lfo); mods->setProperty ("env", env); mods->setProperty ("shape", shape); mods->setProperty ("rnd", rnd); mods->setProperty ("fol", fol); mods->setProperty ("evt", evt); mods->setProperty ("net", juce::var (net));
        obj->setProperty ("mods", juce::var (mods));
    }
    obj->setProperty ("fracture", fractureSeen); fractureSeen = false; engine.uiFracture = false;
    obj->setProperty ("capturing", engine.mem.capturing);
    obj->setProperty ("capLen", (double) (engine.mem.capLen / engine.sr));
    obj->setProperty ("hasCapture", engine.mem.captured.load() != nullptr);
    obj->setProperty ("remembered", rememberCapture);
    obj->setProperty ("hasImport", engine.mem.imported.load() != nullptr);
    obj->setProperty ("importName", importName);
    obj->setProperty ("stopped", engine.isStopped());
    obj->setProperty ("bend", (double) hostBend.load()); obj->setProperty ("wheel", (double) hostWheel.load()); obj->setProperty ("at", (double) hostAt.load());
    emitToUi ("meter", juce::var (obj));
}

void TTYAudioProcessor::timerService()
{
    if (! emitToUi) return;
    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }
    emitParamEcho();
    {
        const int i = (int) std::round (raw[(size_t) P_patch]->load());
        if (i != lastPatchSent) { if (loadedName == "*" || loadedName.isEmpty()) loadedName = {}; emitPatchInfo(); }
    }
    emitEff();
    emitMeter();
}

void TTYAudioProcessor::timerCallback()
{
    bwfxRack.service();
    // a capture that has just stopped becomes a stable source
    const bool on = engine.mem.capturing;
    if (captureWasOn && ! on) { captureWasOn = false; snapshotCapture(); }
    if (on) captureWasOn = true;
}

//==============================================================================
// Patches on disk — the same shape as the other Brokild plugins.
juce::PropertiesFile& TTYAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "ThirtyThousandYears";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File TTYAudioProcessor::installedPresetFolder()
{
    return brokild::patchFolder ("Thirty Thousand Years", { "\"app\":\"thirty-thousand-years\"" });
}

juce::File TTYAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder == juce::File{})
    {
        const auto saved = userSettings().getValue ("presetFolder", {});
        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved) && ! brokild::isUnsafePatchFolder (juce::File (saved)))
            presetFolder = juce::File (saved);
        else
        {
            presetFolder = installedPresetFolder();
            if (presetFolder == juce::File{})
                presetFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Thirty Thousand Years Presets");
        }
    }
    return presetFolder;
}

void TTYAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var TTYAudioProcessor::presetScanDir (const juce::File& dir, int depth)
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
            o->setProperty ("n", f.getFileName()); o->setProperty ("d", true); o->setProperty ("i", kids);
            items.add (juce::var (o));
        }
        else if (f.hasFileExtension ("json"))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileNameWithoutExtension()); o->setProperty ("p", f.getFullPathName());
            items.add (juce::var (o));
        }
    }
    struct Sorter
    {
        static int compareElements (const juce::var& a, const juce::var& b)
        {
            const bool da = (bool) a.getProperty ("d", false), db = (bool) b.getProperty ("d", false);
            if (da != db) return da ? -1 : 1;
            return a.getProperty ("n", "").toString().compareNatural (b.getProperty ("n", "").toString());
        }
    };
    Sorter sorter; items.sort (sorter);
    return juce::var (items);
}

void TTYAudioProcessor::presetScan()
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

void TTYAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory()) start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
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

juce::String TTYAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i) pv->setProperty (ids[i], (double) raw[(size_t) i]->load());
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "thirty-thousand-years");
    o->setProperty ("kind", "preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef TTY_BUILD_ID
    o->setProperty ("build", juce::String (TTY_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    o->setProperty ("macros", macrosVar()); o->setProperty ("slots", slotsVar()); o->setProperty ("mseg", msegVar()); o->setProperty ("scenes", scenesVar()); o->setProperty ("scala", scalaVar());
    o->setProperty ("importPath", importPath);
    const auto cap = captureBase64(); if (cap.isNotEmpty()) o->setProperty ("capture", cap);
    return juce::JSON::toString (juce::var (o), false);
}

void TTYAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject()) { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "thirty-thousand-years") { notice ("THAT IS NOT A THIRTY THOUSAND YEARS PATCH"); return; }
    snapshotForUndo();
    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
        {
            const auto id = kv.name.toString();
            if (id == "volume" || id == "quality" || id == "drone" || id == "patch") continue;
            if (apvts.getParameter (id) != nullptr) { setParamById (id, (float) (double) kv.value); ++applied; }
        }
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    if (v.hasProperty ("macros")) macrosFromVar (v.getProperty ("macros", juce::var())); else engine.defaultMacroMaps();
    if (v.hasProperty ("slots")) slotsFromVar (v.getProperty ("slots", juce::var())); else for (auto& s : engine.life.slots) s.on = false;
    if (v.hasProperty ("mseg")) msegFromVar (v.getProperty ("mseg", juce::var()));
    if (v.hasProperty ("scenes")) scenesFromVar (v.getProperty ("scenes", juce::var())); else for (int i = 0; i < NUM_SCENES; ++i) engine.sceneSet[i] = false;
    if (v.hasProperty ("scala")) scalaFromVar (v.getProperty ("scala", juce::var()));
    importPath = v.getProperty ("importPath", "").toString();
    if (importPath.isNotEmpty()) { const juce::File f (importPath); if (f.existsAsFile()) importClip (f); else notice ("IMPORTED FILE NOT FOUND: " + f.getFileName().toUpperCase() + " " + DOT + " IMPORT IT AGAIN"); }
    captureFromBase64 (v.getProperty ("capture", "").toString());
    emitBwfx(); emitMacros(); emitSlots(); emitMseg(); emitScenes();
    loadedName = name;
    lastSent.assign ((size_t) ids.size(), -999.0f);
    lastPatchSent = -2;
    emitPatchInfo();
    notice ("LOADED \"" + name.toUpperCase() + "\" " + DOT + " " + juce::String (applied) + " VALUES");
}

void TTYAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto suggested = dir.getChildFile ("Aftermath.json");
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
                loadedName = name; lastPatchSent = -2; emitPatchInfo();
            }
            notice (ok ? "SAVED \"" + name.toUpperCase() + "\"" : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void TTYAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Open a patch", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void TTYAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TTYAudioProcessor();
}
