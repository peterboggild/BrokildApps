#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    /*  Non-ASCII has to be spliced in as an explicit UTF-8 String. A bare
        "\xc2\xb7" inside a const char* is at the mercy of the narrow-string
        encoding and arrives on the panel as mojibake. Every other literal in
        this file is plain ASCII; this is the one exception, made explicit. */
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    enum Unit { U_PCT, U_DB, U_HZ, U_SEC, U_CENT };

    struct FDef { const char* id; const char* name; float def; int unit; float lo, hi; };

    /*  Every continuous control, in the order the engine reads them. The
        list is the single source of truth for the ids, the defaults and the
        way the host prints them. */
    const FDef FLOATS[] = {
        { "master",   "OUTPUT",             0.64f, U_DB,   0, 0 },
        { "tune",     "TUNE",               0.50f, U_CENT, 0, 0 },

        { "lalvl",    "L.A. LEVEL",         0.50f, U_DB,   0, 0 },
        { "lasprawl", "L.A. SPRAWL",        0.45f, U_PCT,  0, 0 },
        { "lasmog",   "L.A. SMOG",          0.40f, U_PCT,  0, 0 },
        { "lakipple", "L.A. KIPPLE",        0.30f, U_PCT,  0, 0 },
        { "larain",   "L.A. RAIN",          0.35f, U_PCT,  0, 0 },
        { "laneon",   "L.A. NEON",          0.35f, U_PCT,  0, 0 },
        { "ladecay",  "L.A. DECAY",         0.70f, U_PCT,  0, 0 },
        { "ladrift",  "L.A. DRIFT",         0.40f, U_PCT,  0, 0 },
        { "lasub",    "L.A. SUB",           0.50f, U_PCT,  0, 0 },

        { "dklvl",    "DECKARD LEVEL",      0.55f, U_DB,   0, 0 },
        { "dkbright", "DECKARD BRILLIANCE", 0.50f, U_HZ,   70.0f, 13000.0f },
        { "dkres",    "DECKARD RESONANCE",  0.32f, U_PCT,  0, 0 },
        { "dkfenv",   "DECKARD FILTER ENV", 0.55f, U_PCT,  0, 0 },
        { "dkatk",    "DECKARD ATTACK",     0.28f, U_SEC,  0.002f, 6.0f },
        { "dkdec",    "DECKARD DECAY",      0.40f, U_SEC,  0.02f,  8.0f },
        { "dksus",    "DECKARD SUSTAIN",    0.75f, U_PCT,  0, 0 },
        { "dkrel",    "DECKARD RELEASE",    0.55f, U_SEC,  0.03f, 12.0f },
        { "dkring",   "DECKARD RING",       0.00f, U_PCT,  0, 0 },
        { "dkens",    "DECKARD ENSEMBLE",   0.70f, U_PCT,  0, 0 },
        { "dkglide",  "DECKARD GLIDE",      0.00f, U_SEC,  0.005f, 1.6f },
        { "dkvib",    "DECKARD VIBRATO",    0.25f, U_PCT,  0, 0 },
        { "dkspace",  "DECKARD SPACE",      0.55f, U_PCT,  0, 0 },
        { "dkdetune", "DECKARD DETUNE",     0.30f, U_PCT,  0, 0 },

        { "rplvl",    "REPLICANT LEVEL",    0.50f, U_DB,   0, 0 },
        { "rpmetal",  "REPLICANT METAL",    0.45f, U_PCT,  0, 0 },
        { "rpdecay",  "REPLICANT DECAY",    0.35f, U_SEC,  0.015f, 1.6f },
        { "rpglitch", "REPLICANT GLITCH",   0.20f, U_PCT,  0, 0 },
        { "rpmenace", "REPLICANT MENACE",   0.30f, U_PCT,  0, 0 },
        { "rpspread", "REPLICANT SPREAD",   0.60f, U_PCT,  0, 0 },
        { "rpspace",  "REPLICANT SPACE",    0.40f, U_PCT,  0, 0 }
    };

    const FDef* findFloat (const juce::String& id)
    {
        for (const auto& f : FLOATS) if (id == f.id) return &f;
        return nullptr;
    }

    juce::String dbText (float v)
    {
        const float g = v * v * 2.0f;
        if (g < 1.0e-4f) return "-inf dB";
        return juce::String (20.0f * std::log10 (g), 1) + " dB";
    }
    juce::String secText (float s)
    {
        return s < 1.0f ? juce::String (s * 1000.0f, 0) + " ms"
                        : juce::String (s, 2) + " s";
    }
    juce::String hzText (float f)
    {
        return f < 1000.0f ? juce::String (f, 0) + " Hz"
                           : juce::String (f / 1000.0f, 2) + " kHz";
    }
    juce::String floatText (const FDef& d, float v)
    {
        switch (d.unit)
        {
            case U_DB:   return dbText (v);
            case U_CENT: { const float c = (v - 0.5f) * 200.0f;
                           return (c >= 0 ? "+" : "") + juce::String (c, 0) + " cents"; }
            case U_HZ:   return hzText (br::xmap (v, d.lo, d.hi));
            case U_SEC:  return v < 0.001f && juce::String (d.id) == "dkglide"
                                ? juce::String ("OFF") : secText (br::xmap (v, d.lo, d.hi));
            default:     return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }

    struct IDef { const char* id; const char* name; int lo, hi, def; const char* kind; };
    const IDef INTS[] = {
        { "laroot",  "L.A. ROOT",         -12, 12,  0, "semi"  },
        { "lachord", "L.A. VOICING",        0,  5,  0, "chord" },
        { "dkwave",  "DECKARD OSCILLATOR",  0,  3,  0, "wave"  },
        { "dkoct",   "DECKARD OCTAVE",     -2,  2,  0, "oct"   },
        { "rprate",  "REPLICANT RATE",      0,  6,  3, "rate"  },
        { "rpnexus", "REPLICANT NEXUS",     0, 63,  6, "nexus" },
        { "rpsteps", "REPLICANT STEPS",     4, 16, 16, "steps" },
        { "rpoct",   "REPLICANT OCTAVE",   -2,  2,  0, "oct"   },
        { "mood",    "MOOD",               -1, 999,  -1, "mood"  }
    };

    struct BDef { const char* id; const char* name; bool def; const char* on; const char* off; };
    const BDef BOOLS[] = {
        { "limiter", "OUTPUT LIMITER", true,  "ON",    "OFF"   },
        { "laon",    "L.A. ON",        true,  "LIVE",  "DARK"  },
        { "lagate",  "L.A. TRIGGER",   false, "KEYED", "DRONE" },
        { "dkon",    "DECKARD ON",     true,  "LIVE",  "DARK"  },
        { "rpon",    "REPLICANT ON",   false, "LIVE",  "DARK"  },
        { "rpgate",  "REPLICANT HOLD", false, "KEYED", "FREE"  }
    };

    const IDef* findInt (const juce::String& id)
    {
        for (const auto& d : INTS) if (id == d.id) return &d;
        return nullptr;
    }
    const BDef* findBool (const juce::String& id)
    {
        for (const auto& d : BOOLS) if (id == d.id) return &d;
        return nullptr;
    }
}

//==============================================================================
/*  The canonical order. processBlock reads the cache in exactly this
    sequence, so the two must be edited together. */
juce::StringArray BladeRuinerAudioProcessor::paramIds()
{
    return {
        "master", "tune", "limiter",

        "laon", "lalvl", "laroot", "lachord", "lagate",
        "lasprawl", "lasmog", "lakipple", "larain", "laneon", "ladecay", "ladrift", "lasub",

        "dkon", "dklvl", "dkwave", "dkbright", "dkres", "dkfenv",
        "dkatk", "dkdec", "dksus", "dkrel", "dkring", "dkens", "dkglide", "dkvib",
        "dkspace", "dkoct", "dkdetune",

        "rpon", "rplvl", "rprate", "rpnexus", "rpsteps", "rpmetal", "rpdecay",
        "rpglitch", "rpmenace", "rpspread", "rpspace", "rpgate", "rpoct",

        "mood"
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout BladeRuinerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : FLOATS)
    {
        const FDef def = d;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { d.id, 1 }, d.name,
            juce::NormalisableRange<float> (0.0f, 1.0f), d.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [def] (float v, int) { return floatText (def, v); })));
    }

    for (const auto& d : BOOLS)
    {
        const juce::String on = d.on, off = d.off;
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { d.id, 1 }, d.name, d.def,
            juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                [on, off] (bool v, int) { return v ? on : off; })));
    }

    for (const auto& d : INTS)
    {
        const juce::String kind = d.kind;
        const bool automatable = juce::String (d.id) != "mood";
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { d.id, 1 }, d.name, d.lo, d.hi, d.def,
            juce::AudioParameterIntAttributes().withAutomatable (automatable)
                                               .withStringFromValueFunction (
                [kind] (int v, int)
                {
                    if (kind == "chord") return juce::String (br::Engine::chordName (v));
                    if (kind == "wave")  return juce::String (br::Engine::waveName (v));
                    if (kind == "rate")  return juce::String (br::Engine::rateName (v));
                    if (kind == "steps") return juce::String (v) + " STEPS";
                    if (kind == "nexus") return "N-" + juce::String (v).paddedLeft ('0', 2);
                    if (kind == "mood")  return v < 0 ? juce::String ("NONE") : juce::String (v);
                    if (kind == "oct")   return (v > 0 ? "+" : "") + juce::String (v) + " OCT";
                    return (v > 0 ? "+" : "") + juce::String (v) + " st";
                })));
    }

    //  the rack's five automatable macros, declared by shared code so
    //  every synth carries the identical five (see bwfx_juce.h)
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
BladeRuinerAudioProcessor::BladeRuinerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BLADERUINER", createParameterLayout())
{
    ids = paramIds();
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids)
    {
        auto* a = apvts.getRawParameterValue (id);
        jassert (a != nullptr);
        raw.push_back (a);
    }
    lastSent.assign ((size_t) ids.size(), -999.0f);

    for (int i = 0; i < FFT_SIZE; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (br::TWO_PI_F * (float) i / (float) (FFT_SIZE - 1));

    startTimerHz (15);        // bwfxRack.service() - editor open or not
    bwfxRack.setWorldModConsumed (true);   // this engine maps the SPECTRA bus
}

BladeRuinerAudioProcessor::~BladeRuinerAudioProcessor() = default;
// (the 15 Hz service timer starts in the constructor - see below)

bool BladeRuinerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BladeRuinerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    ring.fill (0.0f);
    ringWrite = 0;
}

//==============================================================================
void BladeRuinerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) engine.reset();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
            {
                engine.p.bpm = *bpm;
                bwfxRack.setBpm (*bpm);   // the world rack syncs too
            }
            engine.p.playing = pos->getIsPlaying();
        }

    auto& p = engine.p;
    auto g = [this] (int i) { return raw[(size_t) i]->load(); };
    int k = 0;

    p.master = g (k++); p.tune = g (k++); p.limiter = g (k++);

    p.laOn = g (k++); p.laLvl = g (k++); p.laRoot = g (k++); p.laChord = g (k++); p.laGate = g (k++);
    p.laSprawl = g (k++); p.laSmog = g (k++); p.laKipple = g (k++); p.laRain = g (k++);
    p.laNeon = g (k++); p.laDecay = g (k++); p.laDrift = g (k++); p.laSub = g (k++);

    p.dkOn = g (k++); p.dkLvl = g (k++); p.dkWave = g (k++); p.dkBright = g (k++);
    p.dkRes = g (k++); p.dkFenv = g (k++);
    p.dkAtk = g (k++); p.dkDec = g (k++); p.dkSus = g (k++); p.dkRel = g (k++);
    p.dkRing = g (k++); p.dkEns = g (k++); p.dkGlide = g (k++); p.dkVib = g (k++);
    p.dkSpace = g (k++); p.dkOct = g (k++); p.dkDetune = g (k++);

    p.rpOn = g (k++); p.rpLvl = g (k++); p.rpRate = g (k++); p.rpNexus = g (k++);
    p.rpSteps = g (k++); p.rpMetal = g (k++); p.rpDecay = g (k++); p.rpGlitch = g (k++);
    p.rpMenace = g (k++); p.rpSpread = g (k++); p.rpSpace = g (k++); p.rpGate = g (k++);
    p.rpOct = g (k++);
    p.mood = g (k++);

    /*  At least one layer is always live. Nothing on the panel can switch the
        last one off, but a host writing three automation lanes at once can —
        so the audio thread keeps the last live layer sounding and tells the
        message thread to put the parameter back. */
    if (p.laOn < 0.5f && p.dkOn < 0.5f && p.rpOn < 0.5f)
    {
        if      (lastLiveLayer == 1) p.dkOn = 1.0f;
        else if (lastLiveLayer == 2) p.rpOn = 1.0f;
        else                         p.laOn = 1.0f;
        forcedLayerBack = true;
    }
    else
    {
        lastLiveLayer = p.laOn > 0.5f ? 0 : (p.dkOn > 0.5f ? 1 : 2);
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = nch >= 2 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float> monoR;
    if (R == nullptr) { monoR.assign ((size_t) n, 0.0f); R = monoR.data(); }

    // SPECTRA world-mod bus: the rack's characters possess the engine.
    // One block of modulation latency; a neutral bus is bit-identical.
    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth,
                            wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    // render between MIDI events so a note lands where it was played
    int last = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (pos > last) { engine.process (L + last, R + last, pos - last); last = pos; }

        const auto m = meta.getMessage();
        if      (m.isNoteOn())                             engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                            engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())   engine.allNotesOff();
    }
    if (n > last) engine.process (L + last, R + last, n - last);

    // the world rack: one extra stage after the engine (empty = untouched)
    bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros
    bwfxRack.process (L, R, n);

    if (nch == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);

    // feed the analyser
    {
        const auto* l = buffer.getReadPointer (0);
        const auto* r = nch >= 2 ? buffer.getReadPointer (1) : l;
        int w = ringWrite.load();
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) w] = 0.5f * (l[i] + r[i]);
            w = (w + 1) & (FFT_SIZE * 2 - 1);
        }
        ringWrite.store (w);
    }
}

//==============================================================================
void BladeRuinerAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        copyXmlToBinary (*xml, dest);
    }
}

void BladeRuinerAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            xml->removeAttribute ("bwfx");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
        }
}

juce::AudioProcessorEditor* BladeRuinerAudioProcessor::createEditor()
{
    return new BladeRuinerAudioProcessorEditor (*this);
}

//==============================================================================
bool BladeRuinerAudioProcessor::wouldSilenceEverything (const juce::String& id, float value) const
{
    if (value > 0.5f) return false;
    if (id != "laon" && id != "dkon" && id != "rpon") return false;

    auto on = [this] (const char* i)
    {
        const int idx = ids.indexOf (i);
        return idx >= 0 && raw[(size_t) idx]->load() > 0.5f;
    };
    const bool la = id == "laon" ? false : on ("laon");
    const bool dk = id == "dkon" ? false : on ("dkon");
    const bool rp = id == "rpon" ? false : on ("rpon");
    return ! (la || dk || rp);
}

void BladeRuinerAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (wouldSilenceEverything (id, value))
    {
        notice ("SOMETHING HAS TO BE PLAYING");
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = -999.0f;   // make the panel snap back
        return;
    }
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void BladeRuinerAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

//==============================================================================
/*  RANDOM is a Penfield mood organ, and the mood organ is the patch.

    A seed between 0 and 999 generates every character parameter and its own
    line of text, deterministically — the same number is the same instrument
    tomorrow, on another machine, and in a project opened next year. Levels,
    tuning and the limiter are never touched, so dialling changes what the
    machine is thinking about and not how loud it is. */
void BladeRuinerAudioProcessor::applyMood (int seed)
{
    seed = juce::jlimit (0, br::NUM_MOODS - 1, seed);   // dialling always lands on a real one
    for (const auto& mv : br::moodPatch (seed))
        setParamById (mv.id, mv.v);
    bwfxRack.clearState();               // a patch stores its own rack; moods carry none
    emitBwfx();
    setParamById ("mood", (float) seed);
    emitMood();
}

void BladeRuinerAudioProcessor::emitMood()
{
    if (! emitToUi) return;
    const int idx = ids.indexOf ("mood");
    const int seed = idx >= 0 ? (int) raw[(size_t) idx]->load() : -1;

    auto* o = new juce::DynamicObject();
    o->setProperty ("code", seed);
    o->setProperty ("text", juce::String (br::moodLine (seed)));
    o->setProperty ("written", br::moodIsWritten (seed));
    emitToUi ("mood", juce::var (o));
}

//==============================================================================
void BladeRuinerAudioProcessor::handleUiMessage (const juce::var& payload)
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

void BladeRuinerAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if      (k == "p")     setParamById (o->getProperty ("id").toString(),
                                         (float) (double) o->getProperty ("v"), true);
    else if (k == "ack")   { uiHasState = true; emitMood(); }
    else if (k == "ready") uiReady = true;
    else if (k == "random") applyMood (juce::Random (juce::Time::getHighResolutionTicks())
                                          .nextInt (br::NUM_MOODS));
    else if (k == "mood")   applyMood ((int) o->getProperty ("n"));
    else if (k == "panic") { wantPanic = true; notice ("RESET"); }
    else if (k == "bwfx")  { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "save")  presetSaveAs();
    else if (k == "open")  presetOpenDialog();
    else if (k == "presetScan")   presetScan();
    else if (k == "presetFolder") presetPickFolder();
    else if (k == "presetLoad")   presetLoad (o->getProperty ("path").toString());
    else if (k == "note")
    {
        const int  n  = (int) o->getProperty ("n");
        const bool on = (bool) o->getProperty ("on");
        if (on) engine.noteOn (n, 0.8f); else engine.noteOff (n);
    }
}

//==============================================================================
void BladeRuinerAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        auto* prm = apvts.getParameter (ids[i]);
        if (prm == nullptr) continue;

        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("n", prm->getName (64));
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            e->setProperty ("d", (double) rp->convertFrom0to1 (rp->getDefaultValue()));

        if (const auto* f = findFloat (ids[i]))
        {
            e->setProperty ("lo", 0.0); e->setProperty ("hi", 1.0);
            const char* u[] = { "pct", "db", "hz", "sec", "cent" };
            e->setProperty ("u", juce::String (u[f->unit]));
            e->setProperty ("a", (double) f->lo);
            e->setProperty ("b", (double) f->hi);
        }
        else if (const auto* d = findInt (ids[i]))
        {
            e->setProperty ("lo", (double) d->lo);
            e->setProperty ("hi", (double) d->hi);
            e->setProperty ("u", juce::String (d->kind));
        }
        else if (const auto* b = findBool (ids[i]))
        {
            e->setProperty ("lo", 0.0); e->setProperty ("hi", 1.0);
            e->setProperty ("u", juce::String ("bool"));
            e->setProperty ("on", juce::String (b->on));
            e->setProperty ("off", juce::String (b->off));
        }
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    juce::Array<juce::var> chords, waves, rates;
    for (int i = 0; i < br::NUM_CHORDS; ++i) chords.add (juce::String (br::Engine::chordName (i)));
    for (int i = 0; i < br::NUM_WAVES;  ++i) waves.add  (juce::String (br::Engine::waveName (i)));
    for (int i = 0; i < br::NUM_RATES;  ++i) rates.add  (juce::String (br::Engine::rateName (i)));

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    obj->setProperty ("chords", chords);
    obj->setProperty ("waves", waves);
    obj->setProperty ("rates", rates);
    obj->setProperty ("steps", br::RP_STEPS);
    obj->setProperty ("moods", br::NUM_MOODS);
   #ifdef BR_BUILD_ID
    obj->setProperty ("build", juce::String (BR_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
}

void BladeRuinerAudioProcessor::emitBwfx()
{
    if (emitToUi)
        emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void BladeRuinerAudioProcessor::timerService()
{
    if (! emitToUi) return;

    if (forcedLayerBack.exchange (false))
    {
        const char* id = lastLiveLayer == 1 ? "dkon" : (lastLiveLayer == 2 ? "rpon" : "laon");
        setParamById (id, 1.0f);
        notice ("SOMETHING HAS TO BE PLAYING");
    }

    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // anything the host or another editor moved
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

            /*  A patch carries the mood it came from, so loading one moves
                that value like any other. The panel cannot work out what the
                number means on its own, so send the line with it. */
            for (const auto& e : changed)
                if (e.getProperty ("id", "").toString() == "mood") { emitMood(); break; }
        }
    }

    // ---- spectrum ---------------------------------------------------------
    {
        const int w = ringWrite.load();
        for (int i = 0; i < FFT_SIZE; ++i)
            fftScratch[(size_t) i] = ring[(size_t) ((w - FFT_SIZE + i) & (FFT_SIZE * 2 - 1))]
                                   * window[(size_t) i];
        std::fill (fftScratch.begin() + FFT_SIZE, fftScratch.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftScratch.data());

        const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
        const double binHz = sr / FFT_SIZE;
        for (int i = 0; i < br::SCOPE_BINS; ++i)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) i / br::SCOPE_BINS);
            const double f1 = 20.0 * std::pow (1000.0, (double) (i + 1) / br::SCOPE_BINS);
            const int a = std::max (1, (int) (f0 / binHz));
            const int b = std::min (FFT_SIZE / 2 - 1, std::max (a, (int) (f1 / binHz)));
            float peak = 0.0f;
            for (int j = a; j <= b; ++j) peak = std::max (peak, fftScratch[(size_t) j]);
            const float dB = 20.0f * std::log10 (std::max (1.0e-6f, peak / (FFT_SIZE * 0.25f)));
            const float norm = br::clamp01 ((dB + 78.0f) / 78.0f);
            spectrum[(size_t) i] += (norm - spectrum[(size_t) i]) * 0.4f;
        }
    }

    // ---- meters -----------------------------------------------------------
    {
        juce::Array<juce::var> sp, lv;
        for (int i = 0; i < br::SCOPE_BINS; ++i) sp.add ((double) spectrum[(size_t) i]);
        for (int i = 0; i < br::NUM_LAYERS; ++i) lv.add ((double) engine.layerRms[i]);

        juce::String pat;
        for (int i = 0; i < br::RP_STEPS; ++i) pat += juce::String (engine.patternStep (i));

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("pat", pat);
        obj->setProperty ("sp", sp);
        obj->setProperty ("lv", lv);
        obj->setProperty ("out", (double) engine.outRms);
        obj->setProperty ("gr", (double) engine.limitGr);
        obj->setProperty ("step", engine.rpStepNow);
        obj->setProperty ("hit", (double) engine.rpHit);
        obj->setProperty ("drift", (double) engine.laDriftNow);
        obj->setProperty ("voices", (double) engine.dkVoicesNow);
        obj->setProperty ("bpm", engine.p.bpm);
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
        const auto probe = dir.getChildFile (".bladeruiner-write-test.tmp");
        if (! probe.replaceWithText ("x")) return false;
        probe.deleteFile();
        return true;
    }
}

juce::PropertiesFile& BladeRuinerAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Blade Ruiner";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

/*  Walk up from the running binary to the folder whose name ends .vst3, and
    take its parent: that is where the plugin was installed, and a library of
    patches that lives beside it travels with it. juce::File::hasWriteAccess
    is useless here — on Windows it answers from the read-only attribute, so
    Program Files claims to be writable and then refuses. Probe by writing. */
juce::File BladeRuinerAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/Blade Ruiner/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    return brokild::patchFolder ("Blade Ruiner", { "\"blade-ruiner\"" });
}

juce::File BladeRuinerAudioProcessor::presetFolderOrDefault()
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
                                   .getChildFile ("Blade Ruiner Presets");
        }
    }
    return presetFolder;
}

void BladeRuinerAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var BladeRuinerAudioProcessor::presetScanDir (const juce::File& dir, int depth)
{
    juce::Array<juce::var> items;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*",
                                                        juce::File::findFilesAndDirectories))
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

void BladeRuinerAudioProcessor::presetScan()
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

void BladeRuinerAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Where the patches live", start);
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File{} || ! dir.isDirectory()) return;
            rememberPresetFolder (dir);
            presetScan();
        });
}

juce::String BladeRuinerAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i)
        pv->setProperty (ids[i], (double) raw[(size_t) i]->load());

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "blade-ruiner");
    o->setProperty ("kind", "patch");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef BR_BUILD_ID
    o->setProperty ("build", juce::String (BR_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));   // a patch stores its own rack
    return juce::JSON::toString (juce::var (o), false);
}

void BladeRuinerAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject())
    { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "blade-ruiner")
    { notice ("THAT IS NOT A BLADE RUINER PATCH"); return; }

    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
            if (apvts.getParameter (kv.name.toString()) != nullptr)
            { setParamById (kv.name.toString(), (float) (double) kv.value); ++applied; }

    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    emitBwfx();

    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("LOADED \"" + name.toUpperCase() + "\" " + DOT + " " + juce::String (applied) + " VALUES");
}

void BladeRuinerAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const int mi = ids.indexOf ("mood");
    const auto stem = juce::File::createLegalFileName (
        (mi >= 0 && raw[(size_t) mi]->load() >= 0)
            ? "Mood " + juce::String ((int) raw[(size_t) mi]->load())
            : juce::String ("Blade Ruiner patch"));
    const auto suggested = dir.getChildFile (stem + ".json");

    activeChooser = std::make_unique<juce::FileChooser> ("Save this patch", suggested, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
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
            notice (ok ? "SAVED \"" + name.toUpperCase() + "\""
                       : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void BladeRuinerAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Open a patch", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void BladeRuinerAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BladeRuinerAudioProcessor();
}
