#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

using namespace gw;

//==============================================================================
/*  The layout is walked out of the ONE table in Engine.h, so the host's
    parameter list, the per-block read and the page's id list cannot drift
    apart from each other.                                                  */
juce::AudioProcessorValueTreeState::ParameterLayout
GravityWellAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = spec (i);
        const juce::ParameterID pid { s.id, 1 };

        if (s.kind == GW_KIND_CHOICE)
        {
            juce::StringArray choices;
            choices.addTokens (juce::String (s.choices != nullptr ? s.choices : "OFF|ON"), "|", "");
            const int n = juce::jmax (2, choices.size());
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                pid, s.label, choices, (int) juce::jlimit (0.0f, (float) n - 1.0f, s.def)));
        }
        else if (s.kind == GW_KIND_INT)
        {
            layout.add (std::make_unique<juce::AudioParameterInt> (
                pid, s.label, (int) s.lo, (int) s.hi, (int) s.def));
        }
        else
        {
            //  Seconds are stored as a normalised position and mapped through
            //  an exponential law, which gives a knob uniform resolution in
            //  PERCEIVED time.  The display converts, so the host shows ms.
            auto range = juce::NormalisableRange<float> (s.lo, s.hi);
            //  The law comes from expLaw(), the same statement the engine and the
            //  panel read: this used to print 20*1200^v for every "HZ" parameter,
            //  when the filters are 18*1400^v and the LFO rates 0.02*2000^v.
            auto text = [k = s.kind, pid = i] (float v, int) -> juce::String
            {
                float base = 0.f, span = 1.f;
                if (gw::expLaw (pid, base, span))
                {
                    const float x = base * std::pow (span, juce::jlimit (0.0f, 1.0f, v));
                    if (k == GW_KIND_SEC)
                        return x < 1.0f ? juce::String ((int) std::lround (x * 1000.0f)) + " ms"
                                        : juce::String (x, 2) + " s";
                    if (x >= 1000.0f) return juce::String (x / 1000.0f, 2) + " kHz";
                    if (x < 10.0f)    return juce::String (x, 2) + " Hz";
                    return juce::String ((int) std::lround (x)) + " Hz";
                }
                switch (k)
                {
                    case GW_KIND_PCT:   return juce::String ((int) std::lround (v * 100.0f)) + " %";
                    case GW_KIND_DB:    return juce::String (v, 1) + " dB";
                    case GW_KIND_BIPOL: return (v > 0.0f ? "+" : "") + juce::String ((int) std::lround (v * 100.0f)) + " %";
                    case GW_KIND_INT:   return juce::String ((int) std::lround (v));
                    default:            return juce::String (v, 3);
                }
            };
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                pid, s.label, range, s.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (text)));
        }
    }

    //  BWFX MACRO 1..5 - the rack's entire host surface, frozen at five.
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
GravityWellAudioProcessor::GravityWellAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRAVITYWELL", createParameterLayout())
{
    for (int i = 0; i < numParams(); ++i)
    {
        ids.add (spec (i).id);
        raw.push_back (apvts.getRawParameterValue (spec (i).id));
    }
    lastSent.assign ((size_t) numParams(), -999.0f);

    applyFactoryPattern (0, pattern, MAX_STEPS);
    for (int i = 0; i < MAX_STEPS; ++i) engine.setStep (i, pattern[i]);

    startTimerHz (15);
}

GravityWellAudioProcessor::~GravityWellAudioProcessor() { stopTimer(); }

void GravityWellAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, samplesPerBlock);
}

bool GravityWellAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void GravityWellAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    //  the one read: every parameter, in table order, by the same macro that
    //  built the layout
    for (int i = 0; i < numParams(); ++i)
        engine.p.v[i] = raw[(size_t) i]->load();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) engine.setTempo (*bpm);
            //  the bar position, so a synced LFO lands on the grid
            engine.setPosition (pos->getPpqPosition().orFallback (0.0), pos->getIsPlaying());
        }

    if (wantPanic.exchange (false)) engine.allNotesOff();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())            engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
        else if (m.isSustainPedalOn())  { sustainOn = true;  engine.sustain (true); }
        else if (m.isSustainPedalOff()) { sustainOn = false; engine.sustain (false); }
        else if (m.isPitchWheel())
        {
            //  the range is applied in the engine, live - it used to be applied
            //  here AND there, which left the wheel moving a third of a semitone
            bend = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
            engine.pitchBend (bend);
        }
        else if (m.isChannelPressure()) pressure = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isController() && m.getControllerNumber() == 1)
            wheel = (float) m.getControllerValue() / 127.0f;
    }

    //  WHEEL and PRESSURE were read here and never handed on: two matrix
    //  sources that always read 0
    engine.setWheel (wheel);
    engine.setPressure (pressure);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);

    //  the shared rack, after the instrument
    bwfx_juce::pushMacros (bwfxRack, apvts);
    bwfxRack.setBpm ((float) 120.0);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) bwfxRack.setBpm ((float) *bpm);
    bwfxRack.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

//==============================================================================
void GravityWellAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* p = apvts.getParameter (id))
    {
        const auto norm = p->convertTo0to1 (value);
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
        if (fromUi)
        {
            const int i = ids.indexOf (id);
            //  record what the plug-in STORED, not what the page asked for.  A
            //  choice or an integer is rounded on the way in; remembering the
            //  request made the echo send the rounded value back on every drag
            //  step, and A ENGINE / B ENGINE flickered as the two fought.
            if (i >= 0) lastSent[(size_t) i] = raw[(size_t) i]->load();
        }
    }
}

void GravityWellAudioProcessor::applyFactory (int i)
{
    i = juce::jlimit (0, NUM_FACTORY - 1, i);
    Params p;
    gw::applyFactory (i, p);
    for (int k = 0; k < numParams(); ++k)
        setParamById (ids[k], p.v[k], false);

    applyFactoryPattern (i, pattern, MAX_STEPS);
    for (int s = 0; s < MAX_STEPS; ++s) engine.setStep (s, pattern[s]);

    //  a patch load CLEARS the rack - the fleet rule
    bwfxRack.fromJson ("");
    patchName = factoryName (i);
    patchSlot = -1;
    currentProgram = i;
    emitPattern();
    emitBwfx();
}

void GravityWellAudioProcessor::setCurrentProgram (int i) { applyFactory (i); }

const juce::String GravityWellAudioProcessor::getProgramName (int i)
{
    return (i >= 0 && i < NUM_FACTORY) ? juce::String (factoryName (i)) : juce::String();
}

//==============================================================================
juce::var GravityWellAudioProcessor::patternToVar() const
{
    juce::Array<juce::var> a;
    for (int i = 0; i < MAX_STEPS; ++i)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("on",   (int) pattern[i].on);
        o->setProperty ("note", (int) pattern[i].note);
        o->setProperty ("acc",  (int) pattern[i].accent);
        o->setProperty ("sld",  (int) pattern[i].slide);
        o->setProperty ("gate", (int) pattern[i].gate);
        o->setProperty ("rat",  (int) pattern[i].ratchet);
        a.add (juce::var (o));
    }
    return juce::var (a);
}

void GravityWellAudioProcessor::patternFromVar (const juce::var& v)
{
    if (auto* arr = v.getArray())
        for (int i = 0; i < juce::jmin (MAX_STEPS, arr->size()); ++i)
        {
            const auto& e = (*arr)[i];
            pattern[i].on      = (uint8_t) (int) e.getProperty ("on",   1);
            pattern[i].note    = (int8_t)  (int) e.getProperty ("note", 0);
            pattern[i].accent  = (uint8_t) (int) e.getProperty ("acc",  0);
            pattern[i].slide   = (uint8_t) (int) e.getProperty ("sld",  0);
            pattern[i].gate    = (uint8_t) (int) e.getProperty ("gate", 50);
            pattern[i].ratchet = (uint8_t) (int) e.getProperty ("rat",  1);
            engine.setStep (i, pattern[i]);
        }
}

//==============================================================================
void GravityWellAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        //  the pattern and the rack ride as attributes, not as parameters
        xml->setAttribute ("pattern", juce::JSON::toString (patternToVar(), true));
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        xml->setAttribute ("patch", patchName);
        xml->setAttribute ("patchSlot", patchSlot);
        xml->setAttribute ("build", GW_BUILD_ID);
        copyXmlToBinary (*xml, dest);
    }
}

void GravityWellAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    const juce::String pat  = xml->getStringAttribute ("pattern");
    const juce::String rack = xml->getStringAttribute ("bwfx");
    patchName = xml->getStringAttribute ("patch", patchName);
    patchSlot = xml->getIntAttribute ("patchSlot", -1);

    xml->removeAttribute ("pattern");
    xml->removeAttribute ("bwfx");
    xml->removeAttribute ("patch");
    xml->removeAttribute ("build");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (pat.isNotEmpty()) patternFromVar (juce::JSON::parse (pat));
    //  an empty blob is the default empty rack, so an old project is untouched
    bwfxRack.fromJson (rack.toStdString());

    uiHasState = false;
}

//==============================================================================
juce::AudioProcessorEditor* GravityWellAudioProcessor::createEditor()
{
    return new GravityWellAudioProcessorEditor (*this);
}

//==============================================================================
juce::PropertiesFile& GravityWellAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Gravity Well";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File GravityWellAudioProcessor::presetFolderOrDefault()
{
    //  the house folder, sorted by the "app" tag every Brokild preset writes
    return brokild::patchFolder ("Gravity Well", { "gravity-well" });
}

juce::String GravityWellAudioProcessor::patchJson (const juce::String& name)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "gravity-well");
    o->setProperty ("build", GW_BUILD_ID);
    o->setProperty ("name", name);
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < numParams(); ++i) pv->setProperty (ids[i], raw[(size_t) i]->load());
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("pattern", patternToVar());
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (o), false);
}

void GravityWellAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    auto v = juce::JSON::parse (json);
    if (! v.isObject()) return;
    const auto params = v.getProperty ("params", juce::var());
    if (auto* o = params.getDynamicObject())
        for (auto& kv : o->getProperties())
            setParamById (kv.name.toString(), (float) (double) kv.value, false);
    patternFromVar (v.getProperty ("pattern", juce::var()));
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    patchName = name;
    emitPattern();
    emitBwfx();
}

//  the slot a user patch file claims: "slot" inside it, else a leading
//  three-digit number in its file name, else none (-1)
static int slotOfFile (const juce::File& f)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (auto* o = v.getDynamicObject())
        if (o->hasProperty ("slot")) return (int) o->getProperty ("slot");
    const auto nm = f.getFileNameWithoutExtension();
    if (nm.length() > 3 && nm.substring (0, 3).containsOnly ("0123456789") && nm[3] == ' ')
        return nm.substring (0, 3).getIntValue();
    return -1;
}

void GravityWellAudioProcessor::presetSave (const juce::String& name, int slot)
{
    //  factory slots 00..31 are the instrument's and cannot be written
    if (slot < 32 || slot > 199) { notice ("choose a slot from 32 to 199"); return; }
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    //  one file per slot: whatever held this slot before is replaced
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.json"))
        if (slotOfFile (e.getFile()) == slot) e.getFile().deleteFile();
    auto f = dir.getChildFile (juce::String (slot).paddedLeft ('0', 3) + " "
                               + juce::File::createLegalFileName (name) + ".json");
    juce::var v = juce::JSON::parse (patchJson (name));
    if (auto* o = v.getDynamicObject()) o->setProperty ("slot", slot);
    if (f.replaceWithText (juce::JSON::toString (v, false))) { patchName = name; patchSlot = slot; notice ("saved " + name + " in slot " + juce::String (slot)); }
    else notice ("could not write " + f.getFullPathName());
    presetScan();
}

//  Clearing a slot never destroys a patch: its file goes into a "Deleted"
//  folder beside the others, which the scan does not look into.
void GravityWellAudioProcessor::presetDelete (int slot)
{
    if (slot < 32 || slot > 199) return;
    auto dir = presetFolderOrDefault();
    juce::Array<juce::File> hits;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.json"))
        if (slotOfFile (e.getFile()) == slot) hits.add (e.getFile());
    int moved = 0;
    if (! hits.isEmpty()) {
        auto bin = dir.getChildFile ("Deleted");
        bin.createDirectory();
        for (auto& f : hits)
            if (f.moveFileTo (bin.getNonexistentChildFile (f.getFileNameWithoutExtension(), ".json", false))) ++moved;
    }
    if (patchSlot == slot) patchSlot = -1;
    notice (moved ? "cleared slot " + juce::String (slot) + " - kept in the Deleted folder"
                  : "slot " + juce::String (slot) + " was already empty");
    presetScan();
}

void GravityWellAudioProcessor::presetLoad (const juce::String& path)
{
    juce::File f (path);
    if (! f.existsAsFile()) { notice ("no such patch"); return; }
    const auto text = f.loadFileAsString();
    const auto v = juce::JSON::parse (text);
    juce::String nm = f.getFileNameWithoutExtension();
    if (auto* o = v.getDynamicObject()) if (o->hasProperty ("name")) nm = o->getProperty ("name").toString();
    applyPatchJson (text, nm);
    patchSlot = slotOfFile (f);
    notice ("loaded " + patchName);
}

void GravityWellAudioProcessor::presetScan()
{
    if (! emitToUi) return;
    juce::Array<juce::var> a;
    auto dir = presetFolderOrDefault();
    if (dir.isDirectory())
        for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.json"))
        {
            auto* o = new juce::DynamicObject();
            const auto v = juce::JSON::parse (e.getFile().loadFileAsString());
            juce::String nm = e.getFile().getFileNameWithoutExtension();
            if (auto* po = v.getDynamicObject()) if (po->hasProperty ("name")) nm = po->getProperty ("name").toString();
            o->setProperty ("name", nm);
            o->setProperty ("slot", slotOfFile (e.getFile()));
            o->setProperty ("path", e.getFile().getFullPathName());
            a.add (juce::var (o));
        }
    emitToUi ("patchTree", juce::var (a));
}

//==============================================================================
void GravityWellAudioProcessor::notice (const juce::String& msg)
{
    if (emitToUi) emitToUi ("notice", msg);
}

void GravityWellAudioProcessor::emitBwfx()
{
    if (emitToUi) emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

void GravityWellAudioProcessor::emitPattern()
{
    if (emitToUi) emitToUi ("pattern", patternToVar());
}

void GravityWellAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    auto* o = new juce::DynamicObject();
    o->setProperty ("build", GW_BUILD_ID);
    o->setProperty ("patch", patchName);
    o->setProperty ("patchSlot", patchSlot);

    //  the table, so the page builds itself from the engine rather than from
    //  a second hand-written copy that can go stale
    juce::Array<juce::var> ps;
    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = spec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", s.id);
        e->setProperty ("name", s.label);
        e->setProperty ("def", s.def);
        e->setProperty ("lo", s.lo);
        e->setProperty ("hi", s.hi);
        e->setProperty ("kind", s.kind);
        if (s.choices != nullptr) e->setProperty ("choices", s.choices);
        {   float base = 0.f, span = 1.f;
            if (expLaw (i, base, span)) { e->setProperty ("lawBase", base); e->setProperty ("lawSpan", span); } }
        //  the sequencer is off the panel (Peter, 2026-09-27): the engine keeps
        //  it for the host, the page is told so it can leave it out on purpose
        if (juce::String (s.id).startsWith ("seq_")) e->setProperty ("hidden", true);
        e->setProperty ("v", raw[(size_t) i]->load());
        ps.add (juce::var (e));
    }
    o->setProperty ("params", juce::var (ps));

    juce::Array<juce::var> fac;
    for (int i = 0; i < NUM_FACTORY; ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("i", i);
        e->setProperty ("name", factoryName (i));
        e->setProperty ("group", factoryGroup (i));
        fac.add (juce::var (e));
    }
    o->setProperty ("factory", juce::var (fac));
    o->setProperty ("pattern", patternToVar());
    {   juce::Array<juce::var> dv;
        for (int i = 0; i < N_LFO_DIV; ++i) dv.add (juce::String (LFO_DIV_NAME[i]));
        o->setProperty ("lfoDivs", juce::var (dv)); }
    {   //  which MODE each circuit honours, so the panel can dim the rest
        juce::Array<juce::var> mm;
        for (unsigned m : MODE_MASK) mm.add ((int) m);
        o->setProperty ("modeMask", juce::var (mm)); }

    emitToUi ("initialState", juce::var (o));
    emitBwfx();
    presetScan();
}

/*  A value the page did not move itself must still reach the control - the
    host, a macro, a preset or the sequencer can all move one.  A control
    that is working and looks dead is the fleet's most-repeated bug.        */
void GravityWellAudioProcessor::emitParamEcho()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    int changed = 0;
    for (int i = 0; i < numParams(); ++i)
    {
        const float v = raw[(size_t) i]->load();
        if (std::abs (v - lastSent[(size_t) i]) > 1.0e-6f)
        {
            lastSent[(size_t) i] = v;
            o->setProperty (ids[i], v);
            ++changed;
        }
    }
    if (changed > 0) emitToUi ("hostParam", juce::var (o));
    else delete o;
}

void GravityWellAudioProcessor::emitScope()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("mass", engine.massNow());
    o->setProperty ("rs",   engine.rsNow());
    o->setProperty ("peak", engine.outPeak());
    {   juce::Array<juce::var> lv;
        lv.add (engine.meterPeak (0)); lv.add (engine.meterPeak (1));
        lv.add (engine.meterRms (0));  lv.add (engine.meterRms (1));
        lv.add (engine.meterOver());
        o->setProperty ("lvl", juce::var (lv)); }
    o->setProperty ("step", engine.seqStep());
    juce::Array<juce::var> mods, froze;
    for (int i = 0; i < 9; ++i) { mods.add (engine.modValue (i)); froze.add (engine.modFrozen (i)); }
    o->setProperty ("mod", juce::var (mods));
    o->setProperty ("frozen", juce::var (froze));

    //  the oscilloscope: every 4th sample of the engine's own ring
    juce::Array<juce::var> wave;
    const int len = engine.scopeLen();
    const int w   = engine.scopeWrite();
    for (int i = 0; i < len; i += 4)
        wave.add (engine.scope ((w + i) % len));
    o->setProperty ("wave", juce::var (wave));

    emitToUi ("scope", juce::var (o));
}

void GravityWellAudioProcessor::timerCallback()
{
    //  IR builds and other slow rack work must run with the editor closed too
    bwfxRack.service();

    if (! uiHasState.load())
    {
        if (++tickCount % 2 == 0) emitInitialState();
        return;
    }
    emitParamEcho();
    emitScope();
}

//==============================================================================
void GravityWellAudioProcessor::handleUiMessage (const juce::var& payload)
{
    const juce::String k = payload.getProperty ("k", juce::var()).toString();

    if (k == "hello")   { uiHasState = false; tickCount = 0; return; }
    if (k == "ready")   { uiHasState = true;  return; }

    if (k == "p")
    {
        setParamById (payload.getProperty ("id", juce::var()).toString(),
                      (float) (double) payload.getProperty ("v", juce::var (0.0)), true);
        return;
    }
    if (k == "n")
    {
        const int note = (int) payload.getProperty ("n", juce::var (36));
        const bool on  = (bool) payload.getProperty ("on", juce::var (true));
        if (on) engine.noteOn (note, (float) (double) payload.getProperty ("v", juce::var (0.9)));
        else    engine.noteOff (note);
        return;
    }
    if (k == "panic")   { wantPanic = true; return; }
    if (k == "factory") { applyFactory ((int) payload.getProperty ("i", juce::var (0))); return; }
    if (k == "step")
    {
        const int i = juce::jlimit (0, MAX_STEPS - 1, (int) payload.getProperty ("i", juce::var (0)));
        pattern[i].on      = (uint8_t) (int) payload.getProperty ("on",   (int) pattern[i].on);
        pattern[i].note    = (int8_t)  (int) payload.getProperty ("note", (int) pattern[i].note);
        pattern[i].accent  = (uint8_t) (int) payload.getProperty ("acc",  (int) pattern[i].accent);
        pattern[i].slide   = (uint8_t) (int) payload.getProperty ("sld",  (int) pattern[i].slide);
        pattern[i].gate    = (uint8_t) (int) payload.getProperty ("gate", (int) pattern[i].gate);
        pattern[i].ratchet = (uint8_t) (int) payload.getProperty ("rat",  (int) pattern[i].ratchet);
        engine.setStep (i, pattern[i]);
        return;
    }
    if (k == "bwfx")
    {
        if (bwfx_juce::handleMessage (bwfxRack, apvts, payload)) emitBwfx();
        return;
    }
    if (k == "patchSave") { presetSave (payload.getProperty ("name", juce::var ("USER")).toString(),
                                        (int) payload.getProperty ("slot", juce::var (-1))); return; }
    if (k == "patchLoad") { presetLoad (payload.getProperty ("path", juce::var ("")).toString()); return; }
    if (k == "patchDelete") { presetDelete ((int) payload.getProperty ("slot", juce::var (-1))); return; }
    if (k == "patchScan") { presetScan(); return; }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GravityWellAudioProcessor();
}
