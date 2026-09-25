#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "brokild_paths.h"

#ifndef TW_BUILD_ID
 #define TW_BUILD_ID "0.0.0"
#endif

using namespace tw;

//==============================================================================
/*  The whole parameter surface. Eight rows per source, then the listener, the
    doors, the materials and the levels. Order matters: processBlock reads in
    table order, and the page keys by id. */
const std::vector<PSpec>& twSpecs()
{
    static std::vector<PSpec> specs = []
    {
        std::vector<PSpec> v;
        const float sx[4]   = { 3.0f / 18, 1.5f / 18, 4.5f / 18, 12.0f / 18 };
        const float sy[4]   = { 2.5f / 9,  1.0f / 9,  7.0f / 9,  4.5f / 9 };
        const float sz[4]   = { (1.2f - 0.2f) / 2.2f, (1.2f - 0.2f) / 2.2f, (1.2f - 0.2f) / 2.2f, (1.6f - 0.2f) / 2.2f };
        const float syaw[4] = { 0.0f, 45.0f / 360, 270.0f / 360, 180.0f / 360 };
        const float stype[4] = { 1, 1, 0, 0 };
        const float sdir[4]  = { 0, 0, 0.5f, 0 };
        const float sin_[4]  = { 3.0f / 6, 0, 0, 0 };
        for (int n = 0; n < MAX_SOURCES; ++n)
        {
            const std::string p = "s" + std::to_string (n + 1);
            const std::string N = "SOURCE " + std::to_string (n + 1) + " ";
            v.push_back ({ p + "x",    N + "X",           sx[n],   false, 0, "" });
            v.push_back ({ p + "y",    N + "Y",           sy[n],   false, 0, "" });
            v.push_back ({ p + "z",    N + "HEIGHT",      sz[n],   false, 0, "" });
            v.push_back ({ p + "yaw",  N + "FACING",      syaw[n], false, 0, "" });
            v.push_back ({ p + "type", N + "TYPE",        stype[n], true, 2, "PURE|LOUDSPEAKER" });
            v.push_back ({ p + "dir",  N + "DIRECTIVITY", sdir[n], false, 0, "" });
            v.push_back ({ p + "in",   N + "INPUT",       sin_[n], true,  7, "OFF|MAIN L|MAIN R|MAIN L+R|AUX L|AUX R|AUX L+R" });
            v.push_back ({ p + "lvl",  N + "LEVEL",       0.8f,    false, 0, "" });
        }
        v.push_back ({ "lisx",    "LISTENER X",        0.25f,        false, 0, "" });
        v.push_back ({ "lisy",    "LISTENER Y",        2.5f / 9,     false, 0, "" });
        v.push_back ({ "lisyaw",  "LISTENER FACING",   0.5f,         false, 0, "" });
        v.push_back ({ "door1",   "DOOR SMALL-LARGE",  1.0f,         false, 0, "" });
        v.push_back ({ "door2",   "DOOR LARGE-GIANT",  1.0f,         false, 0, "" });
        v.push_back ({ "door3",   "DOOR SMALL-GIANT",  0.0f,         false, 0, "" });
        const std::string mats = "ABSORBING|FURNISHED|PLASTER|TILED|STUDIO";
        const std::string surfMats = "AS WALLS|" + mats;             // the default is to follow the walls
        const float mn = (float) (NUM_MATERIALS - 1);
        const char* roomName[NUM_ROOMS] = { "LARGE", "SMALL", "GIANT" };
        const float wallDef[NUM_ROOMS] = { 1.0f / mn, 1.0f / mn, 2.0f / mn };
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            const std::string n = std::to_string (r + 1);
            const std::string R = std::string (roomName[r]) + " ";
            v.push_back ({ "mat" + n, R + "ROOM WALLS",   wallDef[r], true, NUM_MATERIALS,     mats });
            v.push_back ({ "flr" + n, R + "ROOM FLOOR",   0.0f,       true, NUM_MATERIALS + 1, surfMats });
            v.push_back ({ "cel" + n, R + "ROOM CEILING", 0.0f,       true, NUM_MATERIALS + 1, surfMats });
            // the two walls of this room that carry no doorway can be broken in two
            for (int k = 0; k < 2; ++k)
            {
                const std::string kk = k == 0 ? "A" : "B";
                v.push_back ({ "fold" + n + (k == 0 ? "a" : "b"),    R + "FOLD " + kk,          0.5f, false, 0, "" });
                v.push_back ({ "foldp" + n + (k == 0 ? "a" : "b"),   R + "FOLD " + kk + " POS", 0.5f, false, 0, "" });
            }
        }
        v.push_back ({ "direct",  "DIRECT",            0.8f,         false, 0, "" });
        v.push_back ({ "early",   "EARLY REFLECTIONS", 0.8f,         false, 0, "" });
        v.push_back ({ "reverb",  "REVERB",            0.8f,         false, 0, "" });
        v.push_back ({ "mix",     "MIX",               1.0f,         false, 0, "" });
        v.push_back ({ "output",  "OUTPUT",            0.8f,         false, 0, "" });
        v.push_back ({ "earspan", "EAR SPACING",       0.0294f,      false, 0, "" });
        return v;
    }();
    return specs;
}

namespace
{
    inline float dbLaw (float v) { return -24.0f + v * 30.0f; }        // the level knobs
    inline int   choiceIndex (float v, int steps) { return juce::jlimit (0, steps - 1, (int) std::lround (v * (float) (steps - 1))); }
    inline float choiceNorm (int idx, int steps) { return steps > 1 ? (float) idx / (float) (steps - 1) : 0.0f; }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ThinWallsAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : twSpecs())
    {
        if (s.stepped)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::StringArray::fromTokens (juce::String (s.choices), "|", ""), choiceIndex (s.def, s.steps)));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::NormalisableRange<float> (0.0f, 1.0f), s.def));
        }
    }
    return layout;
}

//==============================================================================
ThinWallsAudioProcessor::ThinWallsAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
          .withInput  ("Aux In", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "THINWALLS", createParameterLayout())
{
    const auto& specs = twSpecs();
    paramPtr.resize (specs.size());
    lastSent.assign (specs.size(), -999.0f);
    for (size_t i = 0; i < specs.size(); ++i)
        paramPtr[i] = apvts.getRawParameterValue (specs[i].id);
    rawNow.assign (specs.size(), 0.0f);
    formats.registerBasicFormats();
    startTimerHz (30);
}

ThinWallsAudioProcessor::~ThinWallsAudioProcessor()
{
    stopTimer();
    if (renderJob != nullptr) renderJob->stopThread (4000);
    if (mp4 != nullptr) mp4->abandon();
}

bool ThinWallsAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto aux = l.getChannelSet (true, 1);
        if (! aux.isDisabled() && aux != juce::AudioChannelSet::stereo() && aux != juce::AudioChannelSet::mono()) return false;
    }
    return true;
}

void ThinWallsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (0);
}

//==============================================================================
/*  Raw host values, in table order, to engine units. Shared by the live
    processBlock and the offline take render, so a take replays through
    exactly the arithmetic that played it. */
static void rawToParams (const float* raw, tw::Params& current)
{
    // in table order, real units
    size_t k = 0;
    auto next = [&] { return raw[k++]; };
    // a choice parameter's RAW value is its index, not a fraction of 0..1 (the
    // first live build read every room as TILED for exactly this reason)
    auto nextChoice = [&] (int steps) { return juce::jlimit (0, steps - 1, (int) std::lround (raw[k++])); };
    for (int n = 0; n < MAX_SOURCES; ++n)
    {
        SourceParams& s = current.src[n];
        s.x = next() * APARTMENT_W;
        s.y = next() * APARTMENT_D;
        s.z = 0.2f + next() * 2.2f;
        s.yaw = next() * 360.0f;
        s.type = nextChoice (2);
        s.directivity = next();
        s.input = nextChoice (7);
        s.levelDb = dbLaw (next());
    }
    current.lisX = next() * APARTMENT_W;
    current.lisY = next() * APARTMENT_D;
    current.lisYaw = next() * 360.0f;
    current.door[0] = next(); current.door[1] = next(); current.door[2] = next();
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        current.material[r] = nextChoice (NUM_MATERIALS);
        // 0 means "as walls", which the engine spells -1
        current.floorMat[r] = nextChoice (NUM_MATERIALS + 1) - 1;
        current.ceilMat[r]  = nextChoice (NUM_MATERIALS + 1) - 1;
        for (int k = 0; k < 2; ++k)
        {
            current.breakPush[r][k]  = (next() - 0.5f) * 1.2f;     // +-0.6 m
            current.breakAlong[r][k] = 0.15f + next() * 0.7f;      // 0.15 .. 0.85 along the wall
        }
    }
    current.directDb = dbLaw (next());
    current.earlyDb  = dbLaw (next());
    current.reverbDb = dbLaw (next());
    current.mix      = next();
    current.outputDb = dbLaw (next());
    current.earSpan  = 0.15f + next() * 0.85f;
}

namespace
{
    /*  Renders a take through a fresh engine, block by block on the take's own
        parameter grid, off the message thread. Nothing here touches the live
        engine, so the plug-in keeps playing while an export renders. */
    struct RenderJob : juce::Thread
    {
        std::shared_ptr<TakeData> take;
        juce::AudioBuffer<float> out;
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> ok { false };
        explicit RenderJob (std::shared_ptr<TakeData> t) : juce::Thread ("Thin Walls take render"), take (std::move (t)) {}
        void run() override
        {
            const TakeData& T = *take;
            const int n = T.length.load();
            out.setSize (2, std::max (1, n));
            out.clear();
            auto eng = std::make_unique<Engine>();
            eng->prepare (T.rate, TakeData::PBLOCK);
            const int nb = T.nblocks.load(), nf = T.nfurn.load();
            int fi = -1;
            Params P;
            for (int s = 0; s < n; s += TakeData::PBLOCK)
            {
                if (threadShouldExit()) return;
                const int m = std::min (TakeData::PBLOCK, n - s);
                const int b = std::min (s / TakeData::PBLOCK, nb - 1);
                if (b >= 0) rawToParams (&T.params[(size_t) b * (size_t) T.np], P);
                while (fi + 1 < nf && T.furn[(size_t) (fi + 1)].sample <= s) ++fi;
                if (fi >= 0)
                {
                    P.nfurn = T.furn[(size_t) fi].n;
                    for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = T.furn[(size_t) fi].items[i];
                }
                eng->setParams (P);
                eng->process (T.input.getReadPointer (0, s), T.input.getReadPointer (1, s),
                              T.aux ? T.input.getReadPointer (2, s) : nullptr, T.aux ? T.input.getReadPointer (3, s) : nullptr,
                              out.getWritePointer (0, s), out.getWritePointer (1, s), m);
                progress = (float) (s + m) / (float) std::max (1, n);
            }
            ok = true;
        }
    };
}


void ThinWallsAudioProcessor::readParams()
{
    for (size_t i = 0; i < paramPtr.size(); ++i) rawNow[i] = paramPtr[i]->load();
    rawToParams (rawNow.data(), current);
}

void ThinWallsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // the furniture layout, if the page changed it (never wait for the lock here)
    if (furnVersion.load() != furnVersionAudio)
    {
        const juce::SpinLock::ScopedTryLockType tl (furnLock);
        if (tl.isLocked())
        {
            for (int i = 0; i < MAX_FURN; ++i) current.furn[i] = furnLayout[i];
            current.nfurn = furnCount;
            furnVersionAudio = furnVersion.load();
        }
    }
    readParams();
    engine.setParams (current);

    auto main = getBusBuffer (buffer, true, 0);
    float* mL = main.getWritePointer (0);
    float* mR = main.getNumChannels() > 1 ? main.getWritePointer (1) : mL;

    const float* aL = nullptr; const float* aR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto aux = getBusBuffer (buffer, true, 1);
        if (aux.getNumChannels() > 0) { aL = aux.getReadPointer (0); aR = aux.getNumChannels() > 1 ? aux.getReadPointer (1) : aL; }
    }

    // the test signal replaces the MAIN input while it plays
    if (wavPlaying.load())
    {
        const juce::ScopedTryLock tl (wavLock);
        if (tl.isLocked() && wav != nullptr && wav->data.getNumSamples() > 1)
        {
            const Wav& w = *wav;
            const double step = w.rate / hostRate;
            const double len = (double) w.data.getNumSamples();
            const float g = std::pow (10.0f, (-40.0f + wavGain.load() * 40.0f) * 0.05f);
            const float* c0 = w.data.getReadPointer (0);
            const float* c1 = w.data.getNumChannels() > 1 ? w.data.getReadPointer (1) : c0;
            for (int i = 0; i < n; ++i)
            {
                const int i0 = (int) wavPos; const int i1 = (i0 + 1) % (int) len;
                const float f = (float) (wavPos - i0);
                mL[i] = g * (c0[i0] + f * (c0[i1] - c0[i0]));
                mR[i] = g * (c1[i0] + f * (c1[i1] - c1[i0]));
                wavPos += step; if (wavPos >= len) wavPos -= len;
            }
        }
    }

    // the take: what the engine is about to hear, and everything that shapes it
    {
        const juce::SpinLock::ScopedTryLockType tl (recLock);
        if (tl.isLocked() && recTarget != nullptr)
        {
            TakeData& T = *recTarget;
            const int pos = T.length.load();
            const int m = std::min (n, T.capacity - pos);
            if (m > 0)
            {
                T.input.copyFrom (0, pos, mL, m);
                T.input.copyFrom (1, pos, mR, m);
                if (T.aux)
                {
                    if (aL != nullptr) { T.input.copyFrom (2, pos, aL, m); T.input.copyFrom (3, pos, aR, m); }
                    else { T.input.clear (2, pos, m); T.input.clear (3, pos, m); }
                }
                const int maxBlocks = (int) (T.params.size() / (size_t) T.np);
                for (int g = ((pos + TakeData::PBLOCK - 1) / TakeData::PBLOCK) * TakeData::PBLOCK; g < pos + m; g += TakeData::PBLOCK)
                {
                    const int b = g / TakeData::PBLOCK;
                    if (b >= maxBlocks) break;
                    std::copy (rawNow.begin(), rawNow.end(), T.params.begin() + (size_t) b * (size_t) T.np);
                    T.nblocks = b + 1;
                }
                if (furnVersionAudio != recFurnVersion)
                {
                    const int fi = T.nfurn.load();
                    if (fi < (int) T.furn.size())
                    {
                        auto& snap = T.furn[(size_t) fi];
                        snap.sample = pos; snap.n = current.nfurn;
                        for (int i = 0; i < MAX_FURN; ++i) snap.items[i] = current.furn[i];
                        T.nfurn = fi + 1;
                    }
                    recFurnVersion = furnVersionAudio;
                }
                T.length = pos + m;
            }
            if (pos + m >= T.capacity) { recTarget = nullptr; recCapped = true; }
        }
    }

    // the engine writes the main output over the main input; the aux bus is read only
    float* oL = buffer.getWritePointer (0);
    float* oR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : oL;
    engine.process (mL, mR, aL, aR, oL, oR, n);
}

//==============================================================================
void ThinWallsAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute ("showrays", showRays);
        xml->setAttribute ("selsrc", selectedSource);
        xml->setAttribute ("wavgain", (double) wavGain.load());
        xml->setAttribute ("furn", juce::JSON::toString (furnJson(), true));
        {
            const juce::ScopedLock sl (wavLock);
            if (wav != nullptr) xml->setAttribute ("wavpath", wav->path);
        }
        copyXmlToBinary (*xml, dest);
    }
}

void ThinWallsAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr) return;
    if (! xml->hasTagName (apvts.state.getType())) return;

    showRays = xml->getIntAttribute ("showrays", 1);
    selectedSource = juce::jlimit (0, MAX_SOURCES - 1, xml->getIntAttribute ("selsrc", 0));
    wavGain = (float) xml->getDoubleAttribute ("wavgain", 0.75);
    const juce::String wavPath = xml->getStringAttribute ("wavpath", {});
    // a project from before furniture has none: an empty apartment
    setFurnFromVar (juce::JSON::parse (xml->getStringAttribute ("furn", "[]")));
    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath", "furn" }) xml->removeAttribute (a);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (wavPath.isNotEmpty())
    {
        const juce::File f (wavPath);
        if (f.existsAsFile()) loadWav (f);
    }
    uiHasState = false;          // make the panel take the new values
}

//==============================================================================
juce::var ThinWallsAudioProcessor::patchJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("app", "thin-walls");
    obj->setProperty ("build", TW_BUILD_ID);
    auto* params = new juce::DynamicObject();
    for (const auto& s : twSpecs())
        if (auto* prm = apvts.getParameter (s.id)) params->setProperty (juce::Identifier (s.id), prm->getValue());
    obj->setProperty ("params", juce::var (params));
    obj->setProperty ("showrays", showRays);
    obj->setProperty ("furn", furnJson());
    return juce::var (obj);
}

void ThinWallsAudioProcessor::applyPatchJson (const juce::var& v)
{
    if (! v.isObject()) return;
    const juce::var params = v.getProperty ("params", {});
    if (! params.isObject()) return;
    for (const auto& s : twSpecs())
    {
        const juce::var pv = params.getProperty (juce::Identifier (s.id), {});
        if (pv.isVoid()) continue;
        if (auto* prm = apvts.getParameter (s.id))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) (double) pv));
    }
    if (v.hasProperty ("showrays")) showRays = (int) v.getProperty ("showrays", 1);
    // a placement saved before furniture existed leaves the furniture alone
    if (v.hasProperty ("furn")) setFurnFromVar (v.getProperty ("furn", {}));
    uiHasState = false;
}

//==============================================================================
juce::var ThinWallsAudioProcessor::furnJson() const
{
    juce::Array<juce::var> a;
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (furnLock));
    for (int i = 0; i < furnCount; ++i)
    {
        const FurnItem& it = furnLayout[i];
        if (it.type < 0 || it.type >= NUM_FURN_TYPES) continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("t", juce::String (FURN[it.type].id));
        o->setProperty ("x", it.x); o->setProperty ("y", it.y); o->setProperty ("yaw", it.yaw);
        a.add (juce::var (o));
    }
    return juce::var (a);
}

void ThinWallsAudioProcessor::setFurnFromVar (const juce::var& items)
{
    FurnItem next[MAX_FURN]; int n = 0;
    if (const auto* arr = items.getArray())
        for (const auto& v : *arr)
        {
            if (n >= MAX_FURN) break;
            const juce::String id = v.getProperty ("t", {}).toString();
            int type = -1;
            for (int t = 0; t < NUM_FURN_TYPES; ++t) if (id == FURN[t].id) { type = t; break; }
            if (type < 0) continue;
            next[n].type = type;
            next[n].x = juce::jlimit (0.0f, APARTMENT_W, (float) (double) v.getProperty ("x", 0.0));
            next[n].y = juce::jlimit (0.0f, APARTMENT_D, (float) (double) v.getProperty ("y", 0.0));
            next[n].yaw = (float) std::fmod ((double) v.getProperty ("yaw", 0.0) + 3600.0, 360.0);
            ++n;
        }
    {
        const juce::SpinLock::ScopedLockType sl (furnLock);
        for (int i = 0; i < MAX_FURN; ++i) furnLayout[i] = i < n ? next[i] : FurnItem();
        furnCount = n;
        ++furnVersion;
    }
}

void ThinWallsAudioProcessor::loadWav (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr)
    {
        if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", "could not read " + f.getFileName()); emitToUi ("notice", juce::var (o)); }
        return;
    }
    auto w = std::make_shared<Wav>();
    const int frames = (int) juce::jmin ((juce::int64) (reader->sampleRate * 600.0), reader->lengthInSamples);   // up to ten minutes
    w->data.setSize (2, juce::jmax (2, frames));
    w->data.clear();
    reader->read (&w->data, 0, frames, 0, true, true);
    if (reader->numChannels == 1) w->data.copyFrom (1, 0, w->data, 0, 0, frames);
    w->rate = reader->sampleRate;
    w->name = f.getFileName();
    w->path = f.getFullPathName();
    w->seconds = frames / reader->sampleRate;
    {
        const juce::ScopedLock sl (wavLock);
        wav = w;
        wavPos = 0;
    }
    wavDirty = true;
}

//==============================================================================
void ThinWallsAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (! payload.isObject()) return;
    const juce::String k = payload.getProperty ("k", {}).toString();

    if (k == "hello")
    {
        uiHasState = false;
        uiReady = true;
        emitInitialState();
        return;
    }
    if (k == "stateack") { uiHasState = true; return; }
    if (k == "showrays") { showRays = (int) payload.getProperty ("v", 1); return; }
    if (k == "selsrc")   { selectedSource = juce::jlimit (0, MAX_SOURCES - 1, (int) payload.getProperty ("v", 0)); return; }

    if (k == "touch")
    {
        const juce::String id = payload.getProperty ("id", {}).toString();
        if (auto* prm = apvts.getParameter (id))
        {
            // wrapped in a gesture or the host's "learn from a touch" never sees the move
            if ((bool) payload.getProperty ("down", false)) prm->beginChangeGesture();
            else                                            prm->endChangeGesture();
        }
        return;
    }

    if (k == "p")
    {
        const juce::String id = payload.getProperty ("id", {}).toString();
        const float v = (float) (double) payload.getProperty ("v", 0.0);
        if (auto* prm = apvts.getParameter (id))
        {
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
            const auto& specs = twSpecs();
            for (size_t i = 0; i < specs.size(); ++i)
                if (id == juce::String (specs[i].id)) lastSent[i] = juce::jlimit (0.0f, 1.0f, v);   // do not echo it back
        }
        return;
    }

    if (k == "presetDefault")
    {
        for (const auto& s : twSpecs())
            if (auto* prm = apvts.getParameter (s.id)) prm->setValueNotifyingHost (prm->getDefaultValue());
        setFurnFromVar (juce::var (juce::Array<juce::var>()));
        uiHasState = false;
        return;
    }

    if (k == "furn") { setFurnFromVar (payload.getProperty ("items", {})); return; }

    if (k == "recStart") { startRecording(); return; }
    if (k == "recStop")  { stopRecording ("take held"); return; }
    if (k == "recCam")
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        if (recTarget != nullptr && take != nullptr && take->cam.size() < 200000)
            take->cam.push_back ({ (float) (double) payload.getProperty ("t", 0.0),
                                   (float) (double) payload.getProperty ("pitch", 0.0),
                                   (float) (double) payload.getProperty ("fov", 70.0) });
        return;
    }
    if (k == "vidBegin")
    {
        beginExport ((int) payload.getProperty ("w", 1920), (int) payload.getProperty ("h", 1080), (int) payload.getProperty ("fps", 30));
        return;
    }
    if (k == "vidFrame")
    {
        if (mp4 == nullptr || ! mp4->isOpen()) return;
        const int i = (int) payload.getProperty ("i", -1);
        if (i != vidNext) { failExport ("frame " + juce::String (i) + " arrived where " + juce::String (vidNext) + " was due"); return; }
        juce::MemoryOutputStream jpg;
        if (! juce::Base64::convertFromBase64 (jpg, payload.getProperty ("jpg", {}).toString())) { failExport ("frame " + juce::String (i) + " was not base64"); return; }
        const juce::Image img = juce::ImageFileFormat::loadFrom (jpg.getData(), jpg.getDataSize());
        if (! img.isValid()) { failExport ("frame " + juce::String (i) + " was not an image"); return; }
        juce::String err;
        if (! mp4->writeFrame (img, err)) { failExport (err); return; }
        // the sound up to the end of this frame, so the two streams interleave
        const juce::int64 total = (juce::int64) vidPcm.size() / 2;
        const juce::int64 upTo = std::min (total, (juce::int64) ((double) (i + 1) * vidPcmRate / vidFps));
        if (upTo > vidPcmWritten)
        {
            if (! mp4->writeAudio (vidPcm.data() + vidPcmWritten * 2, (int) (upTo - vidPcmWritten), err)) { failExport (err); return; }
            vidPcmWritten = upTo;
        }
        ++vidNext;
        recProgress = vidN > 0 ? (double) vidNext / vidN : 1.0;
        recText = "exporting frame " + juce::String (vidNext) + " of " + juce::String (vidN);
        if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("i", i); emitToUi ("vidAck", juce::var (o)); }
        return;
    }
    if (k == "vidEnd") { finishExport(); return; }
    if (k == "vidCancel")
    {
        if (renderJob != nullptr) { renderJob->stopThread (4000); renderJob.reset(); }
        if (mp4 != nullptr) mp4->abandon();
        mp4.reset();
        recState = take != nullptr && take->length.load() > 0 ? "ready" : "idle";
        recText = "export cancelled"; recProgress = 0; recDirty = true;
        return;
    }
    if (k == "reveal") { if (juce::File (lastFile).existsAsFile()) juce::File (lastFile).revealToUser(); return; }

    if (k == "presetSave" || k == "presetLoad")
    {
        const bool save = k == "presetSave";
        const juce::File folder = brokild::patchFolder ("Thin Walls", juce::StringArray { "thin-walls" });
        chooser = std::make_unique<juce::FileChooser> (save ? "Save a Thin Walls placement" : "Load a Thin Walls placement",
                                                       folder.getChildFile (save ? "Placement.json" : ""), "*.json");
        const int flags = save ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting)
                               : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);
        chooser->launchAsync (flags, [this, save] (const juce::FileChooser& fc)
        {
            const juce::File f = fc.getResult();
            if (f.getFullPathName().isEmpty()) return;
            juce::String text;
            if (save)
            {
                f.replaceWithText (juce::JSON::toString (patchJson()));
                text = "saved " + f.getFileName();
            }
            else
            {
                const juce::var v = juce::JSON::parse (f);
                if (v.isObject() && v.getProperty ("app", {}).toString() == "thin-walls") { applyPatchJson (v); furnDirtyUi = true; text = "loaded " + f.getFileName(); }
                else text = "not a Thin Walls placement: " + f.getFileName();
            }
            if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", text); emitToUi ("notice", juce::var (o)); }
        });
        return;
    }

    if (k == "wavOpen")
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a test signal",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const juce::File f = fc.getResult();
            if (f.getFullPathName().isEmpty()) return;
            loadWav (f);
            wavPlaying = true;
            wavDirty = true;
        });
        return;
    }
    if (k == "wavPath")
    {
        // a probe cannot drive a native file dialog: the same load, path handed in
        const juce::File f (payload.getProperty ("path", {}).toString());
        if (f.existsAsFile()) { loadWav (f); wavPlaying = true; wavDirty = true; }
        return;
    }
    if (k == "wavPlay") { wavPlaying = (bool) payload.getProperty ("v", false); wavDirty = true; return; }
    if (k == "wavGain") { wavGain = juce::jlimit (0.0f, 1.0f, (float) (double) payload.getProperty ("v", 0.75)); wavDirty = true; return; }
}

//==============================================================================
void ThinWallsAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("build", TW_BUILD_ID);
    obj->setProperty ("showrays", showRays);
    obj->setProperty ("selsrc", selectedSource);
    obj->setProperty ("auxConnected", (getBusCount (true) > 1 && getBus (true, 1)->isEnabled()) ? 1 : 0);
    {
        auto* w = new juce::DynamicObject();
        const juce::ScopedLock sl (wavLock);
        w->setProperty ("name", wav != nullptr ? wav->name : juce::String());
        w->setProperty ("playing", wavPlaying.load() ? 1 : 0);
        w->setProperty ("seconds", wav != nullptr ? wav->seconds : 0.0);
        w->setProperty ("gain", (double) wavGain.load());
        obj->setProperty ("wav", juce::var (w));
    }
    juce::Array<juce::var> params;
    const auto& specs = twSpecs();
    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("id", juce::String (specs[i].id));
        po->setProperty ("name", juce::String (specs[i].name));
        po->setProperty ("stepped", specs[i].stepped);
        po->setProperty ("steps", specs[i].steps);
        po->setProperty ("choices", specs[i].choices.empty() ? juce::var() : juce::var (juce::String (specs[i].choices)));
        if (auto* prm = apvts.getParameter (specs[i].id)) po->setProperty ("v", prm->getValue());
        params.add (juce::var (po));
        lastSent[i] = -999.0f;
    }
    obj->setProperty ("params", params);
    {
        juce::Array<juce::var> cat;
        for (int t = 0; t < NUM_FURN_TYPES; ++t)
        {
            const FurnSpec& F = FURN[t];
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", juce::String (F.id)); o->setProperty ("name", juce::String (F.name));
            o->setProperty ("w", F.w); o->setProperty ("d", F.d); o->setProperty ("h", F.h);
            o->setProperty ("zb", F.zb); o->setProperty ("zt", F.zt);
            o->setProperty ("occludes", F.occludes ? 1 : 0); o->setProperty ("reflectTop", F.reflectTop ? 1 : 0);
            o->setProperty ("absorb1k", F.absorb[3]);
            cat.add (juce::var (o));
        }
        obj->setProperty ("furniture", cat);
        obj->setProperty ("furn", furnJson());
    }
    furnDirtyUi = false;
    emitToUi ("initialState", juce::var (obj));
}

void ThinWallsAudioProcessor::emitWav()
{
    if (! emitToUi) return;
    auto* w = new juce::DynamicObject();
    {
        const juce::ScopedLock sl (wavLock);
        w->setProperty ("name", wav != nullptr ? wav->name : juce::String());
        w->setProperty ("seconds", wav != nullptr ? wav->seconds : 0.0);
    }
    w->setProperty ("playing", wavPlaying.load() ? 1 : 0);
    w->setProperty ("gain", (double) wavGain.load());
    emitToUi ("wav", juce::var (w));
}

static const char* kindName (PathKind k)
{
    switch (k)
    {
        case PathKind::Direct: return "direct";
        case PathKind::Refl1:  return "r1";
        case PathKind::Refl2:  return "r2";
        case PathKind::Portal: return "portal";
        case PathKind::Leaf:   return "leaf";
        case PathKind::Wall:   return "wall";
        default:               return "doorfield";
    }
}

void ThinWallsAudioProcessor::emitScene()
{
    const Scene& sc = engine.scene();
    auto* obj = new juce::DynamicObject();
    auto vec = [] (const Vec3& p) { juce::Array<juce::var> a; a.add (p.x); a.add (p.y); a.add (p.z); return juce::var (a); };

    juce::Array<juce::var> sources;
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("pos", vec (sc.sources[s].pos));
        o->setProperty ("yaw", sc.sources[s].yaw);
        o->setProperty ("type", sc.sources[s].type);
        o->setProperty ("room", sc.sources[s].room);
        o->setProperty ("active", sc.sources[s].active ? 1 : 0);
        o->setProperty ("dir", sc.sources[s].directivity);
        sources.add (juce::var (o));
    }
    obj->setProperty ("sources", sources);
    obj->setProperty ("lis", vec (sc.lis));
    obj->setProperty ("lisRoom", sc.lisRoom);
    obj->setProperty ("lisYaw", sc.lisYaw);
    obj->setProperty ("pathsDropped", sc.pathsDropped);
    {
        juce::Array<juce::var> fa;
        for (int r = 0; r < NUM_ROOMS; ++r) fa.add (sc.furnA[r]);
        obj->setProperty ("furnA", fa);
    }

    juce::Array<juce::var> paths;
    for (int i = 0; i < sc.npaths; ++i)
    {
        const ScenePath& p = sc.paths[i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("t", kindName (p.kind));
        o->setProperty ("s", p.src);
        juce::Array<juce::var> pts;
        for (int k = 0; k < p.npts; ++k) pts.add (vec (p.pts[k]));
        o->setProperty ("pts", pts);
        o->setProperty ("db", p.db);
        o->setProperty ("ms", p.ms);
        paths.add (juce::var (o));
    }
    obj->setProperty ("paths", paths);

    juce::Array<juce::var> rt;
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        juce::Array<juce::var> row; for (int b = 0; b < 4; ++b) row.add (sc.rt[r][b]);
        rt.add (juce::var (row));
    }
    obj->setProperty ("rt", rt);

    juce::Array<juce::var> plan;
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        juce::Array<juce::var> poly;
        for (int i = 0; i < sc.planN[r]; ++i)
        {
            juce::Array<juce::var> pt; pt.add (sc.planX[r][i]); pt.add (sc.planY[r][i]);
            poly.add (juce::var (pt));
        }
        plan.add (juce::var (poly));
    }
    obj->setProperty ("plan", plan);
    obj->setProperty ("in", sc.inDb);
    obj->setProperty ("out", sc.outDb);
    obj->setProperty ("drr", sc.drrDb);
    emitToUi ("scene", juce::var (obj));
}

void ThinWallsAudioProcessor::timerCallback()
{
    if (! emitToUi) return;

    if (! uiHasState)
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // echo any parameter the page did not move itself
    {
        juce::Array<juce::var> changed;
        const auto& specs = twSpecs();
        for (size_t i = 0; i < specs.size(); ++i)
        {
            auto* prm = apvts.getParameter (specs[i].id);
            if (prm == nullptr) continue;
            const float v = prm->getValue();
            if (std::abs (v - lastSent[i]) > 1.0e-5f)
            {
                lastSent[i] = v;
                auto* po = new juce::DynamicObject();
                po->setProperty ("id", juce::String (specs[i].id));
                po->setProperty ("v", v);
                changed.add (juce::var (po));
            }
        }
        if (! changed.isEmpty())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("params", changed);
            emitToUi ("hostParam", juce::var (obj));
        }
    }

    const bool auxNow = getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    if (auxNow != auxWasConnected) { auxWasConnected = auxNow; uiHasState = false; }

    if (wavDirty) { wavDirty = false; emitWav(); }
    emitScene();

    // a layout the page did not make (preset, project) goes to the page
    if (furnDirtyUi)
    {
        furnDirtyUi = false;
        auto* o = new juce::DynamicObject(); o->setProperty ("items", furnJson());
        emitToUi ("furn", juce::var (o));
    }

    // the take: the recorder hit its limit, the render finished, or just progress
    if (recState == "recording" && recCapped.exchange (false)) stopRecording ("stopped at the four-minute limit");
    if (recState == "rendering" && renderJob != nullptr)
    {
        if (auto* job = dynamic_cast<RenderJob*> (renderJob.get())) recProgress = job->progress.load();
        if (! renderJob->isThreadRunning()) sendPlan();
    }
    if (recDirty || ((recState == "recording" || recState == "rendering" || recState == "exporting") && ++recTick >= 3))
    {
        recTick = 0; recDirty = false;
        emitRec();
    }
}

//==============================================================================
// the take: record, render offline, export

void ThinWallsAudioProcessor::emitRec()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("state", recState);
    double sec = 0;
    { const juce::SpinLock::ScopedLockType sl (recLock); if (take != nullptr) sec = take->seconds(); }
    o->setProperty ("sec", sec);
    o->setProperty ("max", 240.0);
    o->setProperty ("progress", recProgress);
    o->setProperty ("file", lastFile);
    o->setProperty ("text", recText);
    emitToUi ("rec", juce::var (o));
}

void ThinWallsAudioProcessor::startRecording()
{
    if (renderJob != nullptr || (mp4 != nullptr && mp4->isOpen())) { recText = "an export is running"; recDirty = true; return; }
    auto t = std::make_shared<TakeData>();
    t->rate = hostRate > 0 ? hostRate : 48000.0;
    t->capacity = (int) (t->rate * 240.0);
    t->aux = getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    t->input.setSize (t->aux ? 4 : 2, t->capacity);
    t->input.clear();
    t->np = (int) twSpecs().size();
    t->params.assign ((size_t) (t->capacity / TakeData::PBLOCK + 2) * (size_t) t->np, 0.0f);
    t->furn.resize (1024);
    t->cam.reserve (8192);
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        take = t;
        recTarget = take.get();
        recFurnVersion = -1;
        recCapped = false;
    }
    recState = "recording"; recText = "recording"; recProgress = 0; recDirty = true;
}

void ThinWallsAudioProcessor::stopRecording (const juce::String& why)
{
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        recTarget = nullptr;
    }
    const bool have = take != nullptr && take->length.load() > (int) (0.2 * take->rate);
    recState = have ? "ready" : "idle";
    recText = have ? why + ": " + juce::String (take->seconds(), 1) + " s" : "nothing recorded";
    recDirty = true;
}

void ThinWallsAudioProcessor::failExport (const juce::String& why)
{
    if (renderJob != nullptr) { renderJob->stopThread (4000); renderJob.reset(); }
    if (mp4 != nullptr) mp4->abandon();
    mp4.reset();
    recState = "error"; recText = "export failed: " + why; recProgress = 0; recDirty = true;
    if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", recText); emitToUi ("notice", juce::var (o)); }
}

void ThinWallsAudioProcessor::beginExport (int w, int h, int fps)
{
    if (recState == "recording") stopRecording ("take held");
    if (take == nullptr || take->length.load() <= 0) { failExport ("there is no take - record one first"); return; }
    if (renderJob != nullptr) return;
    vidW = juce::jlimit (320, 3840, w) & ~1;
    vidH = juce::jlimit (240, 2160, h) & ~1;
    vidFps = juce::jlimit (12, 60, fps);
    renderJob = std::make_unique<RenderJob> (take);
    renderJob->startThread();
    recState = "rendering"; recText = "rendering the sound of the take"; recProgress = 0; recDirty = true;
}

void ThinWallsAudioProcessor::sendPlan()
{
    auto* job = dynamic_cast<RenderJob*> (renderJob.get());
    if (job == nullptr) return;
    if (! job->ok.load()) { failExport ("the render stopped"); return; }
    const TakeData& T = *take;
    const int n = T.length.load();

    // the sound, 16-bit stereo at 44.1 or 48 kHz (AAC takes nothing else)
    vidPcmRate = (std::abs (T.rate - 44100.0) < 1.0) ? 44100 : 48000;
    const double ratio = T.rate / vidPcmRate;
    const int outN = (int) std::floor (n / ratio);
    std::vector<float> ch[2];
    for (int c = 0; c < 2; ++c)
    {
        ch[c].assign ((size_t) std::max (outN, 1), 0.0f);
        if (std::abs (ratio - 1.0) < 1e-9) std::copy (job->out.getReadPointer (c), job->out.getReadPointer (c) + outN, ch[c].begin());
        else { juce::LagrangeInterpolator li; li.process (ratio, job->out.getReadPointer (c), ch[c].data(), outN); }
    }
    vidPcm.assign ((size_t) outN * 2, 0);
    for (int i = 0; i < outN; ++i)
        for (int c = 0; c < 2; ++c)
            vidPcm[(size_t) i * 2 + (size_t) c] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lround (ch[c][(size_t) i] * 32767.0f));
    vidPcmWritten = 0;
    renderJob.reset();

    // the file
    mp4File = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("Thin Walls videos")
                  .getChildFile ("Thin Walls take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S") + ".mp4");
    mp4 = std::make_unique<Mp4Writer>();
    const int kbps = juce::jlimit (4000, 60000, (int) ((double) vidW * vidH * vidFps * 0.12 / 1000.0));
    juce::String err;
    if (! mp4->open (mp4File, vidW, vidH, vidFps, kbps, vidPcmRate, err)) { failExport (err); return; }

    // the plan: every host parameter at every frame, normalised as the page knows them
    const auto& specs = twSpecs();
    const double seconds = n / T.rate;
    vidN = std::max (1, (int) std::ceil (seconds * vidFps));
    vidNext = 0;
    auto* plan = new juce::DynamicObject();
    plan->setProperty ("fps", vidFps); plan->setProperty ("n", vidN); plan->setProperty ("seconds", seconds);
    plan->setProperty ("w", vidW); plan->setProperty ("h", vidH);
    juce::Array<juce::var> ids;
    for (const auto& s : specs) ids.add (juce::String (s.id));
    plan->setProperty ("ids", ids);
    juce::Array<juce::var> frames;
    const int nb = T.nblocks.load();
    for (int i = 0; i < vidN; ++i)
    {
        const int b = juce::jlimit (0, std::max (0, nb - 1), (int) ((double) i / vidFps * T.rate / TakeData::PBLOCK));
        juce::Array<juce::var> row;
        for (size_t j = 0; j < specs.size(); ++j)
        {
            const float raw = nb > 0 ? T.params[(size_t) b * (size_t) T.np + j] : 0.0f;
            const float v = specs[j].stepped ? raw / (float) std::max (1, specs[j].steps - 1) : raw;
            row.add (std::round (v * 100000.0f) / 100000.0f);
        }
        frames.add (juce::var (row));
    }
    plan->setProperty ("frames", frames);
    juce::Array<juce::var> fz;
    for (int f = 0; f < T.nfurn.load(); ++f)
    {
        const auto& snap = T.furn[(size_t) f];
        auto* o = new juce::DynamicObject();
        o->setProperty ("f", (int) std::ceil (snap.sample / T.rate * vidFps));
        juce::Array<juce::var> items;
        for (int i = 0; i < snap.n; ++i)
        {
            const FurnItem& it = snap.items[i];
            if (it.type < 0 || it.type >= NUM_FURN_TYPES) continue;
            auto* io = new juce::DynamicObject();
            io->setProperty ("t", juce::String (FURN[it.type].id));
            io->setProperty ("x", it.x); io->setProperty ("y", it.y); io->setProperty ("yaw", it.yaw);
            items.add (juce::var (io));
        }
        o->setProperty ("items", items);
        fz.add (juce::var (o));
    }
    plan->setProperty ("furn", fz);
    juce::Array<juce::var> cam;
    for (const auto& c : T.cam)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("t", c.t); o->setProperty ("pitch", c.pitch); o->setProperty ("fov", c.fov);
        cam.add (juce::var (o));
    }
    plan->setProperty ("cam", cam);

    recState = "exporting"; recText = "exporting " + juce::String (vidN) + " frames"; recProgress = 0; recDirty = true;
    if (emitToUi) emitToUi ("vidPlan", juce::var (plan));
    else { juce::var keep (plan); }
}

void ThinWallsAudioProcessor::finishExport()
{
    if (mp4 == nullptr || ! mp4->isOpen()) return;
    juce::String err;
    const juce::int64 total = (juce::int64) vidPcm.size() / 2;
    if (total > vidPcmWritten && ! mp4->writeAudio (vidPcm.data() + vidPcmWritten * 2, (int) (total - vidPcmWritten), err)) { failExport (err); return; }
    vidPcmWritten = total;
    if (! mp4->finish (err)) { failExport (err); mp4.reset(); return; }
    mp4.reset();
    lastFile = mp4File.getFullPathName();
    recState = "done"; recProgress = 1.0;
    recText = "saved " + mp4File.getFileName() + " (" + juce::String (mp4File.getSize() / 1048576.0, 1) + " MB)";
    recDirty = true;
    if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", recText); emitToUi ("notice", juce::var (o)); }
}

//==============================================================================
juce::AudioProcessorEditor* ThinWallsAudioProcessor::createEditor()
{
    return new ThinWallsAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThinWallsAudioProcessor();
}
