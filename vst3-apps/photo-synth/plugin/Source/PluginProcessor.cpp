#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

using namespace ps;

//==============================================================================
// Automatable parameters — one per Edit slider, in the slider's native units.
struct ControlDef { const char* id; float min, max, def; };
static const ControlDef kControls[] =
{
    { "vol",           0, 100, 70 },
    { "limiter",       0, 100, 40 },
    { "drive",         0, 100, 25 },
    { "glide",         0, 100, 22 },
    { "attack",        0, 100, 14 },
    { "release",       0, 100, 30 },
    { "legato",        0, 100, 24 },
    { "envAmp",        0, 100, 100 },
    { "envFlt",        0, 100, 0 },
    { "width",         0, 100, 65 },
    { "spread",        5, 60, 14 },
    { "octaves",       1, 4, 3 },
    { "filterQ",       0, 100, 50 },     // Q multiplier on the colour-driven resonance
    { "tune",        -12, 12, 0 },      // global transpose, semitones (continuous)
    { "fine",       -100, 100, 0 },      // fine tune, cents
    { "voices",        1, 12, 1 },
    { "polyNotes",     1, 4, 4 },
    { "polyOsc",       1, 4, 2 },
    { "satGain",       0, 24, 0 },
    { "satTone",       0, 100, 72 },
    { "delayMix",      0, 100, 13 },
    { "delayTime",     40, 900, 260 },
    { "delayFeedback", 0, 112, 34 },
    { "delayOffset",   -250, 250, 0 },
    { "reverbMix",     0, 100, 15 },
    { "reverbLength",  20, 600, 180 },
    { "phaserMix",     0, 100, 35 },
    { "phaserRate",    5, 200, 40 },
    { "phaserDepth",   0, 100, 55 },
    { "chorusMix",     0, 100, 40 },
    { "chorusRate",    5, 300, 45 },
    { "chorusDepth",   0, 100, 50 },
    { "stutterAmt",    0, 100, 60 },
    { "stutterRate",   10, 160, 80 },
    { "lofiCrush",     0, 100, 35 },
    { "lofiNoise",     0, 100, 25 },
    { "lofiDirt",      0, 100, 30 },
    { "subLevel",      0, 100, 50 },
    { "subOct",        1, 2, 1 },
    { "darkCluster",   0, 60, 24 },
    { "envPitch",      0, 100, 25 },
    { "drift",         0, 100, 40 },
    { "driftSpeed",    0, 100, 40 },
};

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& c : kControls)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { c.id, 1 }, c.id,
            juce::NormalisableRange<float> (c.min, c.max, 0.0f), c.def));
    //  the rack's five automatable macros, declared by shared code so
    //  every synth carries the identical five (see bwfx_juce.h)
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
PhotoSynthAudioProcessor::PhotoSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (const auto& c : kControls)
        apvts.addParameterListener (c.id, this);
    workPool = std::make_unique<juce::ThreadPool> (1);
    syncHostTune();

    startTimerHz (15);        // bwfxRack.service() - editor open or not
    bwfxRack.setWorldModConsumed (true);   // this engine maps the SPECTRA bus
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor()
{
    for (const auto& c : kControls)
        apvts.removeParameterListener (c.id, this);
    workPool->removeAllJobs (true, 4000);
    for (auto* b : ownedBatches) delete b;
    ownedBatches.clear();
}

void PhotoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    recSampleRate = sampleRate;
}

void PhotoSynthAudioProcessor::releaseResources() {}

bool PhotoSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // Host tempo, for the tempo-synced delay and stutter.
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                {
                    hostBpm = (float) *bpm;
                    bwfxRack.setBpm (*bpm);   // the world rack syncs too
                }

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        const int off = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (msg.isNoteOn())
        {
            engine.hostNoteOn (msg.getNoteNumber(), off);
            int s1, n1, s2, n2;
            midiUiFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) { midiUiStorage[(size_t) s1] = { 1, msg.getNoteNumber() }; midiUiFifo.finishedWrite (1); }
        }
        else if (msg.isNoteOff())
        {
            engine.hostNoteOff (msg.getNoteNumber(), off);
            int s1, n1, s2, n2;
            midiUiFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) { midiUiStorage[(size_t) s1] = { 0, msg.getNoteNumber() }; midiUiFifo.finishedWrite (1); }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            engine.hostAllNotesOff();
        }
    }
    midiMessages.clear();

    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // SPECTRA world-mod bus: the rack characters possess the voice pool.
    // One block of modulation latency; a neutral bus is bit-identical.
    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth,
                            wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    engine.process (L, R, n);

    // the world rack: one extra stage after the engine (empty = untouched);
    // ahead of the recorder so recordings carry it
    bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros
    bwfxRack.process (L, R, n);

    if (recOn.load())
    {
        const int64_t w = recWrite.load();
        const int64_t room = recCapacity - w;
        const int toCopy = (int) juce::jmin<int64_t> (room, (int64_t) n);
        if (toCopy > 0)
        {
            std::memcpy (recL.data() + w, L, (size_t) toCopy * sizeof (float));
            std::memcpy (recR.data() + w, R, (size_t) toCopy * sizeof (float));
            recWrite = w + toCopy;
        }
        if (toCopy < n)
            recOn = false;                       // hit the 180 s cap
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
// Bridge
void PhotoSynthAudioProcessor::emitBwfx()
{
    if (emitToUi)
        emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

void PhotoSynthAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* batch = payload.getProperty ("b", juce::var()).getArray())
    {
        for (const auto& m : *batch)
            handleOne (m);
        return;
    }
    handleOne (payload);
}

static void shadowParamStore (ps::Engine& e, int idx, float v)
{
    if (idx >= 0 && idx < ps::numParams)
        e.shadowParams[(size_t) idx] = v;
}

void PhotoSynthAudioProcessor::handleOne (const juce::var& m)
{
    const juce::String k = m.getProperty ("k", juce::var()).toString();

    if (k == "p")
    {
        Command c;
        c.param = (int16_t) (int) m.getProperty ("i", 0);
        if (c.param < 0 || c.param >= numParams) return;
        const int mode = (int) m.getProperty ("m", 1);
        c.f1 = (float) (double) m.getProperty ("v", 0.0);
        c.f2 = (float) (double) m.getProperty ("tc", 0.0);
        c.d1 = (double) m.getProperty ("t", -1.0);
        if (mode == 2) c.type = Command::cancelParam;
        else if (mode == 0) c.type = Command::setParamValue;
        else c.type = Command::setParamTarget;
        engine.pushCommand (c);
        if (mode != 2) shadowParamStore (engine, c.param, c.f1);
    }
    else if (k == "v")
    {
        handleVoiceParams ((int) m.getProperty ("v", 0), m.getProperty ("f", juce::var()));
    }
    else if (k == "ve")
    {
        handleVoiceEvents ((int) m.getProperty ("v", 0), m.getProperty ("ev", juce::var()));
    }
    else if (k == "clear")
    {
        Command c; c.type = Command::voiceClear; c.i1 = (int) m.getProperty ("v", -1);
        engine.pushCommand (c);
    }
    else if (k == "reset")
    {
        Command c; c.type = Command::voiceReset; c.i1 = (int) m.getProperty ("v", 0);
        engine.pushCommand (c);
    }
    else if (k == "order")
    {
        int packed = 0;
        if (auto* arr = m.getProperty ("o", juce::var()).getArray())
            for (int i = 0; i < juce::jmin (7, arr->size()); ++i)
                packed |= ((int) (*arr)[i] & 7) << (i * 3);
        Command c; c.type = Command::setFxOrder; c.i1 = packed;
        c.i2 = (int) m.getProperty ("e", 0x7f);
        engine.shadowFxPacked = c.i1;
        engine.shadowFxEnabled = c.i2;
        engine.pushCommand (c);
    }
    else if (k == "dlimit")
    {
        Command c; c.type = Command::setDelayLimit; c.i1 = (int) m.getProperty ("on", 0);
        engine.shadowDLimit = c.i1;
        engine.pushCommand (c);
    }
    else if (k == "lptype")
    {
        Command c; c.type = Command::setLpType; c.i1 = (int) m.getProperty ("t", 0);
        engine.shadowLpType = c.i1;
        engine.pushCommand (c);
    }
    else if (k == "reverb")
    {
        const int type = (int) m.getProperty ("type", 0);
        const float len = (float) (double) m.getProperty ("len", 1.8);
        engine.shadowRevType = type;
        engine.shadowRevLen = len;
        engine.loadReverbImpulse ((ReverbType) type, len);
    }
    else if (k == "meta")
    {
        Command c; c.type = Command::setMeta;
        c.i1 = (int) m.getProperty ("mode", 0);
        c.i2 = ((int) m.getProperty ("voices", 1) & 0xff)
             | (((int) m.getProperty ("notes", 4) & 0xff) << 8)
             | (((int) m.getProperty ("osc", 2) & 0xff) << 16);
        engine.pushCommand (c);
        mirror.darkOn = (bool) m.getProperty ("darkOn", false);
        mirror.freeze = (bool) m.getProperty ("freeze", false);
        mirror.tape = m.getProperty ("delayChar", "clean").toString() == "tape";
        mirror.delayTimeSec = (float) (double) m.getProperty ("delayTime", 0.26);
        mirror.delayOffsetSec = (float) (double) m.getProperty ("delayOffset", 0.0);
        mirror.delayFeedback = (float) (double) m.getProperty ("delayFeedback", 0.34);
    }
    else if (k == "control")
    {
        const juce::String id = m.getProperty ("id", juce::var()).toString();
        const float v = (float) (double) m.getProperty ("v", 0.0);
        if (auto* param = apvts.getParameter (id))
        {
            suppressEcho = true;
            param->setValueNotifyingHost (param->convertTo0to1 (v));
            suppressEcho = false;
        }
        if (id == "tune" || id == "fine") syncHostTune();
    }
    else if (k == "state")
    {
        const juce::ScopedLock sl (stateLock);
        uiStateJson = m.getProperty ("j", juce::var()).toString();
    }
    else if (k == "getstate")
    {
        emitInitialState();
    }
    else if (k == "stateack")
    {
        uiHasState = true;          // the page has it; stop re-sending
    }
    else if (k == "uiready")
    {
        uiReady = true;             // deck painted; the editor can show it
    }
    else if (k == "rec")
    {
        startRecording ((bool) m.getProperty ("on", false));
    }
    else if (k == "recsave")
    {
        saveRecording();
    }
    else if (k == "render")
    {
        startRender (m);
    }
    else if (k == "savetext")
    {
        saveTextFile (m.getProperty ("name", "export.json").toString(),
                      m.getProperty ("text", juce::var()).toString());
    }
    else if (k == "bwfx")
    {
        if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx();
    }
    else if (k == "presetFolder")
    {
        presetPickFolder();
    }
    else if (k == "presetScan")
    {
        presetScan();
    }
    else if (k == "presetLoad")
    {
        presetLoad (m.getProperty ("path", "").toString());
    }
    else if (k == "presetSave")
    {
        presetSave (m.getProperty ("sub", "").toString(),
                    m.getProperty ("name", "").toString(),
                    m.getProperty ("json", juce::var()).toString());
    }
    else if (k == "presetSaveAs")
    {
        presetSaveAs (m.getProperty ("name", "").toString(),
                      m.getProperty ("json", juce::var()).toString());
    }
    else if (k == "presetOpen")
    {
        presetOpenDialog();
    }
    else if (k == "panic")
    {
        engine.hostAllNotesOff();
    }
}

void PhotoSynthAudioProcessor::handleVoiceParams (int voice, const juce::var& fields)
{
    if (voice < 0 || voice >= kNumVoices) return;
    if (auto* arr = fields.getArray())
    {
        for (const auto& pair : *arr)
        {
            if (auto* p = pair.getArray())
            {
                if (p->size() >= 2)
                {
                    Command c; c.type = Command::setVoiceField;
                    c.i1 = voice;
                    c.i2 = (int) (*p)[0];
                    c.f1 = (float) (double) (*p)[1];
                    if (c.i2 >= 0 && c.i2 < numVoiceFields)
                    {
                        engine.shadowVoice[(size_t) voice][(size_t) c.i2] = c.f1;
                        engine.pushCommand (c);
                    }
                }
            }
        }
    }
}

static void parseEventList (const juce::var& evs, std::vector<VoiceEvent>& out)
{
    if (auto* arr = evs.getArray())
    {
        out.reserve ((size_t) arr->size());
        for (const auto& ev : *arr)
        {
            VoiceEvent e;
            e.frame = (int64_t) (double) ev.getProperty ("f", 0.0);
            e.gate = (int) ev.getProperty ("g", -1);
            int vi = 0;
            if (auto* pl = ev.getProperty ("p", juce::var()).getArray())
            {
                for (const auto& pair : *pl)
                {
                    if (auto* pv = pair.getArray())
                    {
                        if (pv->size() >= 2 && vi < 8)
                        {
                            const int fid = (int) (*pv)[0];
                            if (fid >= 0 && fid < 32)
                            {
                                e.fieldMask |= 1u << fid;
                                e.values[vi++] = (float) (double) (*pv)[1];
                            }
                        }
                    }
                }
            }
            out.push_back (e);
        }
        std::stable_sort (out.begin(), out.end(),
                          [] (const VoiceEvent& a, const VoiceEvent& b) { return a.frame < b.frame; });
    }
}

void PhotoSynthAudioProcessor::handleVoiceEvents (int voice, const juce::var& evs)
{
    if (voice < 0 || voice >= kNumVoices) return;
    auto* batch = new EventBatch();
    batch->voice = voice;
    parseEventList (evs, batch->events);
    if (batch->events.empty()) { delete batch; return; }
    Command c; c.type = Command::voiceEvents; c.ptr = batch;
    if (engine.pushCommand (c))
        ownedBatches.add (batch);
    else
        delete batch;
}

void PhotoSynthAudioProcessor::collectGarbage()
{
    for (int i = ownedBatches.size(); --i >= 0;)
    {
        if (ownedBatches.getUnchecked (i)->consumed.load())
        {
            delete ownedBatches.getUnchecked (i);
            ownedBatches.remove (i);
        }
    }
}

//==============================================================================
/* Hand the page the state this processor is holding. A freshly opened editor
 * cannot receive anything until its browser is on screen, and an emit made
 * before then is simply dropped — so rather than answering only when asked,
 * timerService repeats this until the page acknowledges it. That is what
 * turns "default photos for a few seconds, then the real ones" into the
 * patch appearing as the window opens. */
void PhotoSynthAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    juce::String j;
    {
        const juce::ScopedLock sl (stateLock);
        j = uiStateJson;
    }
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("j", j);
    obj->setProperty ("fs", engine.getSampleRate());
   #ifdef PS_BUILD_ID
    obj->setProperty ("build", juce::String (PS_BUILD_ID));
   #endif
    emitToUi ("initialState", juce::var (obj));
}

//==============================================================================
/* Global tuning is the one control the engine has to know about at all times:
 * host MIDI notes turn into a frequency inside the engine, not in the page.
 * The page applies the identical offset to the notes it plays, so both paths
 * arrive at the same pitch whether the editor is open or shut. */
void PhotoSynthAudioProcessor::syncHostTune()
{
    const float semis = apvts.getRawParameterValue ("tune")->load()
                      + apvts.getRawParameterValue ("fine")->load() / 100.0f;
    engine.hostTune = semis;
    Command c; c.type = Command::setHostTune; c.f1 = semis;
    engine.pushCommand (c);
}

//==============================================================================
// Host automation with the editor closed: apply the slider's mapping natively.
void PhotoSynthAudioProcessor::applyControlNative (const juce::String& id, float v)
{
    auto setP = [this] (int param, float value, float tc)
    {
        Command c; c.type = Command::setParamTarget; c.param = (int16_t) param; c.f1 = value; c.f2 = tc; c.d1 = -1;
        shadowParamStore (engine, param, value);
        engine.pushCommand (c);
    };
    auto setVoiceAll = [this] (int field, float value)
    {
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            Command c; c.type = Command::setVoiceField; c.i1 = vi; c.i2 = field; c.f1 = value;
            engine.shadowVoice[(size_t) vi][(size_t) field] = value;
            engine.pushCommand (c);
        }
    };
    const float u = v / 100.0f;

    if (id == "vol") setP (pMaster, u, 0.03f);
    else if (id == "limiter") setP (pLimit, u, 0.03f);
    else if (id == "drive") setVoiceAll (vfDrive, u);
    else if (id == "glide") setVoiceAll (vfGlide, std::pow (u, 2.0f) * 0.55f + 0.004f);
    else if (id == "attack") setVoiceAll (vfAttack, 0.002f + std::pow (u, 2.0f) * 1.0f);
    else if (id == "release") setVoiceAll (vfRelease, 0.01f + std::pow (u, 2.0f) * 3.0f);
    else if (id == "envAmp") setVoiceAll (vfEnvAmp, u);
    else if (id == "envFlt") setVoiceAll (vfEnvFlt, u);
    else if (id == "width") setVoiceAll (vfSpread, u);
    else if (id == "satGain")
    {
        const float mix = v / 24.0f;
        const double SAT_K = 4.0, SAT_BIAS = 0.11;
        const double zero = std::tanh (SAT_K * SAT_BIAS);
        const double hi = std::tanh (SAT_K * (1 + SAT_BIAS)) - zero;
        const double lo = std::tanh (SAT_K * (-1 + SAT_BIAS)) - zero;
        const double norm = juce::jmax (std::abs (hi), std::abs (lo), 0.001);
        const double pre = std::pow (10.0, v / 32.0);
        double sIn = 0, sOut = 0;
        for (int i = 0; i < 128; ++i)
        {
            const double x = 0.3 * std::sin (6.2831853 * i / 128);
            const double y = (std::tanh (SAT_K * (juce::jlimit (-1.0, 1.0, pre * x) + SAT_BIAS)) - zero) / norm;
            sIn += x * x; sOut += y * y;
        }
        const float comp = (float) (sOut > 1e-9 ? std::sqrt (sIn / sOut) : 1.0);
        setP (pSatDry, std::cos (mix * juce::MathConstants<float>::halfPi), 0.03f);
        setP (pSatWet, std::sin (mix * juce::MathConstants<float>::halfPi) * comp, 0.03f);
        setP (pSatPre, std::pow (10.0f, v / 32.0f), 0.03f);
    }
    else if (id == "satTone") setP (pSatToneF, 800.0f * std::pow (18000.0f / 800.0f, u), 0.03f);
    else if (id == "delayMix")
    {
        setP (pDelayDry, std::cos (u * juce::MathConstants<float>::halfPi), 0.05f);
        setP (pDelayWet, std::sin (u * juce::MathConstants<float>::halfPi), 0.05f);
    }
    else if (id == "delayTime" || id == "delayOffset")
    {
        if (id == "delayTime") mirror.delayTimeSec = v / 1000.0f;
        else mirror.delayOffsetSec = v / 1000.0f;
        setP (pDelayTime0, mirror.delayTimeSec + juce::jmax (0.0f, -mirror.delayOffsetSec), 0.02f);
        setP (pDelayTime1, mirror.delayTimeSec + juce::jmax (0.0f, mirror.delayOffsetSec), 0.02f);
    }
    else if (id == "delayFeedback")
    {
        mirror.delayFeedback = u;
        setP (pDelayFb, mirror.freeze ? 1.0f : u, 0.03f);
        Command c; c.type = Command::setDelayLimit;
        c.i1 = (mirror.tape || mirror.freeze || mirror.delayFeedback > 0.92f) ? 1 : 0;
        engine.shadowDLimit = c.i1;
        engine.pushCommand (c);
    }
    else if (id == "reverbMix")
    {
        setP (pRevDry, std::cos (u * juce::MathConstants<float>::halfPi), 0.05f);
        setP (pRevWet, std::sin (u * juce::MathConstants<float>::halfPi), 0.05f);
    }
    else if (id == "reverbLength")
    {
        engine.shadowRevLen = v / 100.0f;
        engine.loadReverbImpulse ((ReverbType) engine.shadowRevType.load(), v / 100.0f);
    }
    else if (id == "phaserMix")
    {
        setP (pPhDry, std::cos (u * juce::MathConstants<float>::halfPi), 0.05f);
        setP (pPhWet, std::sin (u * juce::MathConstants<float>::halfPi), 0.05f);
    }
    else if (id == "phaserRate") setP (pPhRate, v / 100.0f, 0.05f);
    else if (id == "phaserDepth") setP (pPhDepth, 80.0f + 780.0f * u, 0.05f);
    else if (id == "chorusMix")
    {
        setP (pChDry, std::cos (u * juce::MathConstants<float>::halfPi), 0.05f);
        setP (pChWet, std::sin (u * juce::MathConstants<float>::halfPi), 0.05f);
    }
    else if (id == "chorusRate") setP (pChRate, v / 100.0f, 0.05f);
    else if (id == "chorusDepth") setP (pChDepth, 0.0008f + 0.0042f * u, 0.05f);
    else if (id == "stutterAmt") { setP (pStBase, 1.0f - u, 0.03f); setP (pStMod, u, 0.03f); }
    else if (id == "stutterRate") setP (pStRate, v / 10.0f, 0.05f);
    else if (id == "lofiCrush") setP (pLofiCrush, u, 0.0f);
    else if (id == "lofiNoise") setP (pLofiNoise, u, 0.0f);
    else if (id == "lofiDirt") setP (pLofiDirt, u, 0.0f);
    else if (id == "subLevel") { if (mirror.darkOn) setVoiceAll (vfSub, u); }
    else if (id == "subOct") setVoiceAll (vfSubOct, v);
    else if (id == "envPitch") { if (mirror.darkOn) setVoiceAll (vfEnvPitch, u); }
    // voices / polyNotes / polyOsc / octaves / filterQ / spread / legato / darkCluster /
    // drift / driftSpeed need the photo-sampling UI to recompute — they take
    // effect when the editor is open (documented in docs/feature-parity.md).
}

void PhotoSynthAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (suppressEcho.load()) return;
    const juce::ScopedLock sl (dirtyLock);
    dirtyParams.addIfNotAlreadyThere (parameterID);
}

//==============================================================================
void PhotoSynthAudioProcessor::timerService()
{
    // Offer the stored state to a page that has not confirmed it yet. The
    // timer runs at 30 Hz; every third tick is about 100 ms, which is quick
    // enough that the patch is simply there when the window appears.
    if (emitToUi && ! uiHasState.load() && ++statePushTick >= 3)
    {
        statePushTick = 0;
        emitInitialState();
    }

    if (emitToUi)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("t", engine.getCurrentTime());
        obj->setProperty ("fs", engine.getSampleRate());
        obj->setProperty ("bpm", hostBpm.load());
        emitToUi ("clock", juce::var (obj));
    }

    juce::StringArray dirty;
    {
        const juce::ScopedLock sl (dirtyLock);
        dirty.swapWith (dirtyParams);
    }
    for (const auto& id : dirty)
    {
        const float v = apvts.getRawParameterValue (id)->load();
        if (id == "tune" || id == "fine") syncHostTune();
        if (emitToUi)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("id", id);
            obj->setProperty ("v", v);
            emitToUi ("hostParam", juce::var (obj));
        }
        else
        {
            applyControlNative (id, v);
        }
    }

    {
        int s1, n1, s2, n2;
        midiUiFifo.prepareToRead (midiUiFifo.getNumReady(), s1, n1, s2, n2);
        auto emitOne = [this] (const MidiUiEvent& e)
        {
            if (! emitToUi) return;
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("on", e.on);
            obj->setProperty ("note", e.note);
            emitToUi ("midiIn", juce::var (obj));
        };
        for (int i = 0; i < n1; ++i) emitOne (midiUiStorage[(size_t)(s1 + i)]);
        for (int i = 0; i < n2; ++i) emitOne (midiUiStorage[(size_t)(s2 + i)]);
        midiUiFifo.finishedRead (n1 + n2);
    }

    if (emitToUi && recCapacity > 0)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("on", recOn.load());
        obj->setProperty ("frames", (double) recWrite.load());
        obj->setProperty ("sr", recSampleRate);
        emitToUi ("recState", juce::var (obj));
    }

    collectGarbage();
}

//==============================================================================
void PhotoSynthAudioProcessor::startRecording (bool on)
{
    if (on)
    {
        recSampleRate = engine.getSampleRate();
        recCapacity = (int64_t) (recSampleRate * 180.0);
        recL.assign ((size_t) recCapacity, 0.0f);
        recR.assign ((size_t) recCapacity, 0.0f);
        recWrite = 0;
        recOn = true;
    }
    else
    {
        recOn = false;
    }
}

void PhotoSynthAudioProcessor::saveRecording()
{
    const int64_t frames = recWrite.load();
    if (frames <= 0 || recL.empty()) return;

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    activeChooser = std::make_unique<juce::FileChooser> (
        "Save recording",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory)
            .getChildFile ("photo-synth-" + stamp + ".wav"),
        "*.wav");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, frames] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File{}) return;
            file.deleteFile();
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
            if (os == nullptr) return;
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (os.get(), recSampleRate, 2, 24, {}, 0));
            if (w == nullptr) return;
            os.release();
            const float* chans[2] = { recL.data(), recR.data() };
            w->writeFromFloatArrays (chans, 2, (int) frames);
        });
}

void PhotoSynthAudioProcessor::saveTextFile (const juce::String& name, const juce::String& text)
{
    activeChooser = std::make_unique<juce::FileChooser> (
        "Save file",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name),
        "*.*");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [text] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File{}) return;
            file.replaceWithText (text);
        });
}

//==============================================================================
// Presets are plain files in a folder of the player’s choosing, so they can be
// backed up, shared and sorted in Explorer like any other document. The folder
// is remembered in the plugin’s own settings file rather than in the session,
// so it survives across projects, hosts and reinstalls of the DAW.
juce::PropertiesFile& PhotoSynthAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Photo-Synth2";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

namespace
{
    /*  hasWriteAccess() is not to be trusted on Windows — it answers from the
        read-only attribute rather than from the ACL, so Program Files reports
        writable and then refuses. Ask the file system instead. */
    bool canWriteInto (const juce::File& dir)
    {
        if (! dir.isDirectory()) return false;
        const auto probe = dir.getChildFile (".photosynth-write-test.tmp");
        if (! probe.replaceWithText ("x")) return false;
        probe.deleteFile();
        return true;
    }
}

/*  A folder called "User presets" sitting beside the installed plugin is the
    first place anyone looks, and it travels with the plugin when the whole VST3
    folder is copied to another machine. It is only used when it can genuinely
    be written to: under Program Files it cannot be, without elevation, and a
    preset folder that throws on every save is worse than one somewhere else. */
juce::File PhotoSynthAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/Photo Synth/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    //  the patch folder is PRODUCT_NAME verbatim; the old spelling is kept as
    //  a former name so existing patches migrate rather than disappear
    return brokild::patchFolder ("Photo Synth", { "\"photo-synth\"" },
                                 "*.json", { "Photo-Synth2", "Photo-Synth 2" });
}

juce::File PhotoSynthAudioProcessor::presetFolderOrDefault()
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
                                   .getChildFile ("Photo Synth Presets");
        }
    }
    return presetFolder;
}

void PhotoSynthAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

void PhotoSynthAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Choose a preset folder", start);
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

/*  Save straight from the header button: the page hands over the patch, this
    puts a real Save dialog in front of it, opened on the preset folder. */
void PhotoSynthAudioProcessor::presetSaveAs (const juce::String& name, const juce::String& json)
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const auto stem = juce::File::createLegalFileName (name.trim().isEmpty() ? "Patch" : name.trim());
    const auto suggested = dir.getChildFile (stem + ".json");

    activeChooser = std::make_unique<juce::FileChooser> ("Save this patch", suggested, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, json] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;                  // cancelled: say nothing
            if (! file.hasFileExtension ("json")) file = file.withFileExtension ("json");
            file.getParentDirectory().createDirectory();
            const bool ok = file.replaceWithText (json);

            /*  Saving somewhere else moves the preset folder there — but saving
                into one of its own subfolders must not, or the menu would lose
                everything above it. */
            if (ok)
            {
                const auto chosen = file.getParentDirectory();
                const auto root = presetFolderOrDefault();
                if (chosen != root && ! chosen.isAChildOf (root))
                    rememberPresetFolder (chosen);
            }

            if (emitToUi)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("ok", ok);
                o->setProperty ("name", file.getFileNameWithoutExtension());
                if (! ok) o->setProperty ("error", "Could not write " + file.getFullPathName());
                emitToUi ("presetSaved", juce::var (o));
            }
            presetScan();
        });
}

void PhotoSynthAudioProcessor::presetOpenDialog()
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

// One level of subfolders becomes one level of submenus. Deeper trees are still
// walked, but a preset menu five deep helps nobody, so the nesting stops at two.
juce::var PhotoSynthAudioProcessor::presetScanDir (const juce::File& dir, int depth)
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
            if (kids.size() == 0) continue;          // no presets inside: no submenu
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

    struct Sorter                                    // folders first, then names
    {
        static int compareElements (const juce::var& a, const juce::var& b)
        {
            const bool da = (bool) a.getProperty ("d", false);
            const bool db = (bool) b.getProperty ("d", false);
            if (da != db) return da ? -1 : 1;
            return a.getProperty ("n", "").toString()
                    .compareNatural (b.getProperty ("n", "").toString());
        }
    };
    Sorter sorter;
    items.sort (sorter);
    return juce::var (items);
}

void PhotoSynthAudioProcessor::presetScan()
{
    if (! emitToUi) return;
    const auto dir = presetFolderOrDefault();
    const bool there = dir.isDirectory();
    auto* o = new juce::DynamicObject();
    o->setProperty ("folder", dir.getFullPathName());
    o->setProperty ("exists", there);
    o->setProperty ("items", there ? presetScanDir (dir, 0)
                                   : juce::var (juce::Array<juce::var>()));
    emitToUi ("presetTree", juce::var (o));
}

void PhotoSynthAudioProcessor::presetLoad (const juce::String& path)
{
    if (! emitToUi) return;
    const juce::File f (path);
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", f.getFileNameWithoutExtension());
    if (f.existsAsFile())
    {
        // a patch stores its own rack: apply and strip it before the page sees the json
        auto txt = f.loadFileAsString();
        auto pv = juce::JSON::parse (txt);
        if (auto* po = pv.getDynamicObject())
        {
            bwfxRack.fromJson (pv.getProperty ("bwfx", juce::var ("")).toString().toStdString());
            emitBwfx();
            po->removeProperty ("bwfx");
            txt = juce::JSON::toString (pv, false);
        }
        o->setProperty ("json", txt);
    }
    else
        o->setProperty ("error", "That preset is no longer in the folder.");
    emitToUi ("presetFile", juce::var (o));
}

void PhotoSynthAudioProcessor::presetSave (const juce::String& sub, const juce::String& name,
                                           const juce::String& json)
{
    auto dir = presetFolderOrDefault();
    if (sub.isNotEmpty()) dir = dir.getChildFile (juce::File::createLegalFileName (sub));
    const auto made = dir.createDirectory();         // also covers the very first save

    const auto clean = juce::File::createLegalFileName (name.trim().isEmpty() ? "Untitled" : name);
    const auto file = dir.getChildFile (clean + ".json");
    juce::String jsonOut = json;                     // a patch stores its own rack
    {
        auto pv = juce::JSON::parse (json);
        if (auto* po = pv.getDynamicObject())
        {
            po->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
            jsonOut = juce::JSON::toString (pv, false);
        }
    }
    const bool ok = made.wasOk() && file.replaceWithText (jsonOut);

    if (emitToUi)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("ok", ok);
        o->setProperty ("name", clean);
        if (! ok) o->setProperty ("error", "Could not write to " + dir.getFullPathName());
        emitToUi ("presetSaved", juce::var (o));
    }
    presetScan();
}

//==============================================================================
// Offline MIDI -> WAV render: a private engine instance runs faster than
// real time with the exact parameter snapshot and event schedule the UI built.
void PhotoSynthAudioProcessor::applyRenderSetupTo (ps::Engine& e, const juce::var& payload)
{
    if (auto* arr = payload.getProperty ("params", juce::var()).getArray())
    {
        for (const auto& pair : *arr)
        {
            if (auto* p = pair.getArray())
            {
                if (p->size() >= 2)
                {
                    Command c; c.type = Command::setParamValue;
                    c.param = (int16_t) (int) (*p)[0];
                    c.f1 = (float) (double) (*p)[1];
                    c.d1 = -1;
                    if (c.param >= 0 && c.param < numParams) e.applyCommandNow (c);
                }
            }
        }
    }
    if (auto* arr = payload.getProperty ("sched", juce::var()).getArray())
    {
        for (const auto& it : *arr)
        {
            if (auto* p = it.getArray())
            {
                if (p->size() >= 5)
                {
                    Command c;
                    c.param = (int16_t) (int) (*p)[0];
                    const int mode = (int) (*p)[1];
                    c.f1 = (float) (double) (*p)[2];
                    c.f2 = (float) (double) (*p)[3];
                    c.d1 = (double) (*p)[4];
                    c.type = mode == 0 ? Command::setParamValue : Command::setParamTarget;
                    if (c.param >= 0 && c.param < numParams) e.applyCommandNow (c);
                }
            }
        }
    }
    { Command c; c.type = Command::setLpType; c.i1 = (int) payload.getProperty ("lptype", 0); e.applyCommandNow (c); }
    { Command c; c.type = Command::setDelayLimit; c.i1 = (int) payload.getProperty ("dlimit", 0); e.applyCommandNow (c); }
    {
        int packed = 0;
        if (auto* arr = payload.getProperty ("order", juce::var()).getArray())
            for (int i = 0; i < juce::jmin (7, arr->size()); ++i)
                packed |= ((int) (*arr)[i] & 7) << (i * 3);
        Command c; c.type = Command::setFxOrder; c.i1 = packed;
        c.i2 = (int) payload.getProperty ("en", 0x7f);
        e.applyCommandNow (c);
    }
    e.loadReverbImpulse ((ReverbType) (int) payload.getProperty ("revType", 0),
                         (float) (double) payload.getProperty ("revLen", 1.8));
    { Command c; c.type = Command::setParamValue; c.param = pRvG0; c.f1 = 1; c.d1 = -1; e.applyCommandNow (c); }
    { Command c; c.type = Command::setParamValue; c.param = pRvG1; c.f1 = 0; c.d1 = -1; e.applyCommandNow (c); }

    if (auto* vs = payload.getProperty ("voices", juce::var()).getArray())
    {
        for (int vi = 0; vi < juce::jmin ((int) kNumVoices, vs->size()); ++vi)
        {
            const auto& vv = (*vs)[vi];
            if (auto* farr = vv.getProperty ("f", juce::var()).getArray())
            {
                for (const auto& pair : *farr)
                {
                    if (auto* p = pair.getArray())
                    {
                        if (p->size() >= 2)
                        {
                            Command c; c.type = Command::setVoiceField;
                            c.i1 = vi; c.i2 = (int) (*p)[0]; c.f1 = (float) (double) (*p)[1];
                            if (c.i2 >= 0 && c.i2 < numVoiceFields) e.applyCommandNow (c);
                        }
                    }
                }
            }
            std::vector<VoiceEvent> evs;
            parseEventList (vv.getProperty ("ev", juce::var()), evs);
            if (! evs.empty())
                e.voiceAt (vi).addEvents (evs);
        }
    }
    { Command c; c.type = Command::setParamValue; c.param = pMaster;
      c.f1 = (float) (double) payload.getProperty ("master", 0.9); c.d1 = -1; e.applyCommandNow (c); }
}

void PhotoSynthAudioProcessor::startRender (const juce::var& payload)
{
    const double dur = juce::jlimit (0.1, 1200.0, (double) payload.getProperty ("dur", 10.0));
    const double sr = 48000.0;
    const juce::String name = payload.getProperty ("name", "midi").toString();
    juce::var payloadCopy = payload;

    workPool->addJob ([this, payloadCopy, dur, sr, name]
    {
        auto e = std::make_unique<ps::Engine>();
        e->prepare (sr, 512);
        applyRenderSetupTo (*e, payloadCopy);

        const int64_t total = (int64_t) std::ceil (dur * sr);
        auto bufL = std::make_shared<std::vector<float>> ((size_t) total, 0.0f);
        auto bufR = std::make_shared<std::vector<float>> ((size_t) total, 0.0f);
        int64_t done = 0;
        while (done < total)
        {
            const int nn = (int) juce::jmin<int64_t> (512, total - done);
            e->process (bufL->data() + done, bufR->data() + done, nn);
            done += nn;
        }

        juce::MessageManager::callAsync ([this, bufL, bufR, total, sr, name]
        {
            const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
            const auto safe = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
            activeChooser = std::make_unique<juce::FileChooser> (
                "Save rendered WAV",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                    .getChildFile ("photo-synth-" + (safe.isEmpty() ? juce::String ("midi") : safe) + "-" + stamp + ".wav"),
                "*.wav");
            activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                [this, bufL, bufR, total, sr] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    bool ok = false;
                    const double seconds = (double) total / sr;
                    if (file != juce::File{})
                    {
                        file.deleteFile();
                        juce::WavAudioFormat fmt;
                        std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
                        if (os != nullptr)
                        {
                            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (os.get(), sr, 2, 24, {}, 0));
                            if (w != nullptr)
                            {
                                os.release();
                                const float* chans[2] = { bufL->data(), bufR->data() };
                                ok = w->writeFromFloatArrays (chans, 2, (int) total);
                            }
                        }
                    }
                    if (emitToUi)
                    {
                        auto* obj = new juce::DynamicObject();
                        obj->setProperty ("ok", ok);
                        obj->setProperty ("seconds", seconds);
                        emitToUi ("renderDone", juce::var (obj));
                    }
                });
        });
    });
}

//==============================================================================
void PhotoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("engine", engine.snapshotState());
    {
        const juce::ScopedLock sl (stateLock);
        root->setProperty ("ui", uiStateJson);
    }
    root->setProperty ("apvts", apvts.copyState().toXmlString());
    root->setProperty ("bwfx", juce::String (bwfxRack.toJson()));   // the world rack
    const auto json = juce::JSON::toString (juce::var (root), true);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void PhotoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto json = juce::String::fromUTF8 ((const char*) data, sizeInBytes);
    const auto v = juce::JSON::parse (json);
    if (! v.isObject()) return;

    if (auto xml = juce::XmlDocument::parse (v.getProperty ("apvts", juce::var()).toString()))
    {
        suppressEcho = true;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        suppressEcho = false;
    }
    {
        const juce::ScopedLock sl (stateLock);
        uiStateJson = v.getProperty ("ui", juce::var()).toString();
    }
    engine.applyStateSnapshot (v.getProperty ("engine", juce::var()));
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    emitBwfx();
    syncHostTune();

    if (emitToUi)
    {
        juce::String j;
        {
            const juce::ScopedLock sl (stateLock);
            j = uiStateJson;
        }
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("j", j);
        obj->setProperty ("fs", engine.getSampleRate());
        emitToUi ("initialState", juce::var (obj));
    }
}

juce::AudioProcessorEditor* PhotoSynthAudioProcessor::createEditor()
{
    return new PhotoSynthAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhotoSynthAudioProcessor();
}
