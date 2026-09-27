#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "brokild_paths.h"

namespace
{
    enum Kind { K_PCT = 0, K_DB, K_HZ, K_CEIL, K_TILT };

    struct PDef { const char* id; const char* name; float def; int kind; };

    juce::String bandId (int b, const char* suffix) { return "b" + juce::String (b + 1) + suffix; }

    /*  Non-ASCII has to be spliced in as an explicit UTF-8 String. A bare
        "\xc2\xb7" inside a const char* is at the mercy of the narrow-string
        encoding, and arrived on the panel as mojibake. Every literal in this
        file is plain ASCII; this is the one exception, made explicit. */
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    juce::String dbText (float v)                    // level law: v*v*4  (0.5 = unity)
    {
        const float g = v * v * 4.0f;
        if (g < 1.0e-4f) return "-inf dB";
        return juce::String (20.0f * std::log10 (g), 1) + " dB";
    }

    juce::String fmtValue (int kind, float v)
    {
        switch (kind)
        {
            case K_DB:   return dbText (v);
            case K_HZ:   { const float f = mw::xmap (v, 20.0f, 20000.0f);
                           return f < 1000.0f ? juce::String (f, 0) + " Hz"
                                              : juce::String (f / 1000.0f, 2) + " kHz"; }
            case K_CEIL: return juce::String (20.0f * std::log10 (mw::xmap (v, 0.02f, 1.0f)), 1) + " dB";
            case K_TILT: { const float d = (v - 0.5f) * 24.0f;
                           return (d >= 0 ? "+" : "") + juce::String (d, 1) + " dB"; }
            default:     return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }

    const char* BAND_SUFFIX[] = { "drive", "bias", "char", "tone", "mix", "lvl", "ceil" };
    const char* BAND_LABEL[]  = { "DRIVE", "BIAS", "CHARACTER", "TONE", "MIX", "LEVEL", "CEILING" };
    const int   BAND_KIND[]   = { K_PCT, K_PCT, K_PCT, K_TILT, K_PCT, K_DB, K_CEIL };
    const float BAND_DEF[]    = { 0.35f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f, 0.9f };
    constexpr int BAND_KNOBS  = 7;
}

//==============================================================================
juce::StringArray MarsWarsAudioProcessor::paramIds()
{
    juce::StringArray a;
    for (int b = 0; b < mw::MAX_BANDS; ++b)
    {
        a.add (bandId (b, "on"));
        a.add (bandId (b, "solo"));
        a.add (bandId (b, "algo"));
        for (int k = 0; k < BAND_KNOBS; ++k) a.add (bandId (b, BAND_SUFFIX[k]));
    }
    for (int x = 0; x < mw::MAX_BANDS - 1; ++x) a.add ("x" + juce::String (x + 1));
    a.add ("bands");
    a.add ("os");
    a.add ("ingain");
    a.add ("outgain");
    a.add ("drywet");
    a.add ("autogain");
    a.add ("masterlim");
    for (int c = 0; c < mw::MAX_CABLES; ++c)
    {
        const auto n = juce::String (c + 1);
        a.add ("c" + n + "src");
        a.add ("c" + n + "dst");
        a.add ("c" + n + "amt");
    }
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout MarsWarsAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray algoNames;
    for (int i = 0; i < mw::NUM_ALGOS; ++i) algoNames.add (mw::algoName (i));

    for (int b = 0; b < mw::MAX_BANDS; ++b)
    {
        const auto pre = "BAND " + juce::String (b + 1) + " " + DOT + " ";

        layout.add (std::make_unique<juce::AudioParameterBool> (
            /*  Every band on. How many bands EXIST is the "bands" control;
                this switch is for A/Bing one, and a band that arrives muted
                just punches a silent hole in the spectrum.  */
            juce::ParameterID { bandId (b, "on"), 1 }, pre + "ON", true,
            juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                [] (bool v, int) { return juce::String (v ? "ON" : "OFF"); })));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { bandId (b, "solo"), 1 }, pre + "SOLO", false,
            juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                [] (bool v, int) { return juce::String (v ? "SOLO" : "-"); })));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { bandId (b, "algo"), 1 }, pre + "ALGORITHM",
            algoNames, b == 0 ? mw::A_VALVE : (b == 1 ? mw::A_OVERDRIVE : mw::A_WARM)));

        for (int k = 0; k < BAND_KNOBS; ++k)
        {
            const int kind = BAND_KIND[k];
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { bandId (b, BAND_SUFFIX[k]), 1 }, pre + BAND_LABEL[k],
                juce::NormalisableRange<float> (0.0f, 1.0f), BAND_DEF[k],
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [kind] (float v, int) { return fmtValue (kind, v); })));
        }
    }

    const float xdef[] = { 0.47f, 0.62f, 0.74f, 0.85f };
    for (int x = 0; x < mw::MAX_BANDS - 1; ++x)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "x" + juce::String (x + 1), 1 },
            "CROSSOVER " + juce::String (x + 1),
            juce::NormalisableRange<float> (0.0f, 1.0f), xdef[x],
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return fmtValue (K_HZ, v); })));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "bands", 1 }, "BANDS", 1, mw::MAX_BANDS, 3));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "os", 1 }, "OVERSAMPLING",
        juce::StringArray { "1x", "2x", "4x" }, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ingain", 1 }, "INPUT",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return dbText (v); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outgain", 1 }, "OUTPUT",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return dbText (v); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drywet", 1 }, "DRY / WET",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " % wet"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "autogain", 1 }, "AUTO GAIN", true,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool v, int) { return juce::String (v ? "MATCHED" : "OFF"); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "masterlim", 1 }, "OUTPUT LIMITER", true,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool v, int) { return juce::String (v ? "ON" : "OFF"); })));

    // ---- the patch bay ---------------------------------------------------
    for (int c = 0; c < mw::MAX_CABLES; ++c)
    {
        const auto n = juce::String (c + 1);
        const auto pre = "CABLE " + n + " " + DOT + " ";

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "c" + n + "src", 1 }, pre + "FROM",
            0, mw::NUM_SOURCES - 1, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (
                [] (int v, int) { return juce::String (mw::sourceName (v)); })));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "c" + n + "dst", 1 }, pre + "TO",
            0, mw::NUM_DESTS - 1, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (
                [] (int v, int) { return juce::String (mw::destName (v)); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "c" + n + "amt", 1 }, pre + "AMOUNT",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { const float a = (v - 0.5f) * 2.0f;
                                    return (a >= 0 ? "+" : "") + juce::String (a * 100.0f, 0) + " %"; })));
    }

    return layout;
}

//==============================================================================
MarsWarsAudioProcessor::MarsWarsAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MARSWARS", createParameterLayout())
{
    ids = paramIds();
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);

    for (int i = 0; i < FFT_SIZE; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (FFT_SIZE - 1));

}

MarsWarsAudioProcessor::~MarsWarsAudioProcessor() = default;

bool MarsWarsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void MarsWarsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    ring.fill (0.0f);
    ringWrite = 0;
}

//==============================================================================
void MarsWarsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;

    if (wantPanic.exchange (false)) engine.reset();

    auto& p = engine.p;
    auto g = [this] (int i) { return raw[(size_t) i]->load(); };
    int k = 0;
    for (int b = 0; b < mw::MAX_BANDS; ++b)
    {
        auto& bp = p.band[(size_t) b];
        bp.on   = g (k++);
        bp.solo = g (k++);
        bp.algo = (int) g (k++);
        bp.drive = g (k++); bp.bias = g (k++); bp.chr = g (k++);
        bp.tone = g (k++); bp.mix = g (k++); bp.level = g (k++); bp.ceil = g (k++);
    }
    for (int x = 0; x < mw::MAX_BANDS - 1; ++x) p.xover[(size_t) x] = g (k++);
    p.bands     = (int) g (k++);
    p.os        = (int) g (k++);
    p.inGain    = g (k++);
    p.outGain   = g (k++);
    p.dryWet    = g (k++);
    p.autoGain  = g (k++);
    p.masterLim = g (k++);
    for (int c = 0; c < mw::MAX_CABLES; ++c)
    {
        auto& cb = p.cable[(size_t) c];
        cb.src = (int) g (k++);
        cb.dst = (int) g (k++);
        cb.amt = (g (k++) - 0.5f) * 2.0f;
    }

    if (nch >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else
    {
        juce::HeapBlock<float> tmp ((size_t) n);
        auto* l = buffer.getWritePointer (0);
        std::memcpy (tmp.get(), l, sizeof (float) * (size_t) n);
        engine.process (l, tmp.get(), n);
        for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + tmp[i]);
    }

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
void MarsWarsAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
            copyXmlToBinary (*xml, dest);
    }
}

void MarsWarsAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
        }
}

juce::AudioProcessorEditor* MarsWarsAudioProcessor::createEditor()
{
    return new MarsWarsAudioProcessorEditor (*this);
}

//==============================================================================
void MarsWarsAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void MarsWarsAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

/*  Randomise the character, never the levels. Input, output, dry/wet and the
    band trims are mix decisions and are left where they were, so a roll of
    the dice changes what it sounds like and not how loud it is. */
void MarsWarsAudioProcessor::randomiseAll()
{
    juce::Random r (juce::Time::getHighResolutionTicks());

    const int nb = 2 + r.nextInt (4);
    setParamById ("bands", (float) nb);

    // crossovers: ordered, spread, and never bunched
    juce::Array<float> xs;
    for (int i = 0; i < mw::MAX_BANDS - 1; ++i) xs.add (0.18f + r.nextFloat() * 0.72f);
    std::sort (xs.begin(), xs.end());
    for (int i = 0; i < mw::MAX_BANDS - 1; ++i)
    {
        float v = xs[i];
        if (i > 0) v = std::max (v, xs[i - 1] + 0.08f);
        xs.set (i, juce::jlimit (0.05f, 0.97f, v));
        setParamById ("x" + juce::String (i + 1), xs[i]);
    }

    for (int b = 0; b < mw::MAX_BANDS; ++b)
    {
        /*  Always on. Muting a band at random does not sound adventurous,
            it sounds like a fault - a hole in the spectrum.  */
        setParamById (bandId (b, "on"),   1.0f);
        setParamById (bandId (b, "solo"), 0.0f);
        setParamById (bandId (b, "algo"), (float) r.nextInt (mw::NUM_ALGOS));
        setParamById (bandId (b, "drive"), 0.12f + r.nextFloat() * 0.78f);
        setParamById (bandId (b, "bias"),  0.30f + r.nextFloat() * 0.40f);
        setParamById (bandId (b, "char"),  r.nextFloat());
        setParamById (bandId (b, "tone"),  0.32f + r.nextFloat() * 0.36f);
        setParamById (bandId (b, "mix"),   0.55f + r.nextFloat() * 0.45f);
        setParamById (bandId (b, "lvl"),   0.44f + r.nextFloat() * 0.12f);
        setParamById (bandId (b, "ceil"),  0.62f + r.nextFloat() * 0.38f);
    }

    /*  A couple of cables, and never an audio one by accident — a random
        audio ring is a howl, not a preset. Control cables only. */
    const int cables = r.nextInt (3);
    for (int c = 0; c < mw::MAX_CABLES; ++c)
    {
        const auto n = juce::String (c + 1);
        if (c < cables)
        {
            setParamById ("c" + n + "src", (float) (1 + r.nextInt (mw::S_OUTENV)));
            setParamById ("c" + n + "dst", (float) (mw::D_DRIVE1 + r.nextInt (mw::D_XOVER4 - mw::D_DRIVE1 + 1)));
            setParamById ("c" + n + "amt", 0.5f + (r.nextFloat() - 0.5f) * 0.7f);
        }
        else setParamById ("c" + n + "src", 0.0f);
    }

    notice ("RANDOMISED " + DOT + " " + juce::String (nb) + " BANDS " + DOT + " "
            + juce::String (cables) + " CABLES " + DOT + " LEVELS UNTOUCHED");
}

//==============================================================================
void MarsWarsAudioProcessor::handleUiMessage (const juce::var& payload)
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

void MarsWarsAudioProcessor::handleOne (const juce::var& m)
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
    else if (k == "random") { randomiseAll(); }
    else if (k == "panic")  { wantPanic = true; notice ("RESET"); }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan")   { presetScan(); }
    else if (k == "presetFolder") { presetPickFolder(); }
    else if (k == "presetLoad")   { presetLoad (o->getProperty ("path").toString()); }

}

//==============================================================================
void MarsWarsAudioProcessor::emitInitialState()
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
        e->setProperty ("hi", ids[i] == "bands" ? 5.0
                            : (ids[i] == "os" ? 2.0
                            : (ids[i].endsWith ("algo") ? (double) (mw::NUM_ALGOS - 1)
                            : (ids[i].endsWith ("src")  ? (double) (mw::NUM_SOURCES - 1)
                            : (ids[i].endsWith ("dst")  ? (double) (mw::NUM_DESTS - 1) : 1.0)))));
        e->setProperty ("n", prm->getName (64));
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    juce::Array<juce::var> algos, chars;
    for (int i = 0; i < mw::NUM_ALGOS; ++i)
    {
        algos.add (juce::String (mw::algoName (i)));
        chars.add (juce::String (mw::algoCharName (i)));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    juce::Array<juce::var> srcs, dsts;
    for (int i = 0; i < mw::NUM_SOURCES; ++i) srcs.add (juce::String (mw::sourceName (i)));
    for (int i = 0; i < mw::NUM_DESTS; ++i)   dsts.add (juce::String (mw::destName (i)));

    obj->setProperty ("algos", algos);
    obj->setProperty ("chars", chars);
    obj->setProperty ("srcs", srcs);
    obj->setProperty ("dsts", dsts);
    obj->setProperty ("cables", mw::MAX_CABLES);
   #ifdef MW_BUILD_ID
    obj->setProperty ("build", juce::String (MW_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
}


void MarsWarsAudioProcessor::timerService()
{

    if (! emitToUi) return;

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
        for (int i = 0; i < mw::SCOPE_BINS; ++i)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) i / mw::SCOPE_BINS);
            const double f1 = 20.0 * std::pow (1000.0, (double) (i + 1) / mw::SCOPE_BINS);
            int a = std::max (1, (int) (f0 / binHz));
            int b = std::min (FFT_SIZE / 2 - 1, std::max (a, (int) (f1 / binHz)));
            float peak = 0.0f;
            for (int j = a; j <= b; ++j) peak = std::max (peak, fftScratch[(size_t) j]);
            const float dB = 20.0f * std::log10 (std::max (1.0e-6f, peak / (FFT_SIZE * 0.25f)));
            const float norm = mw::clamp01 ((dB + 78.0f) / 78.0f);
            spectrum[(size_t) i] += (norm - spectrum[(size_t) i]) * 0.45f;
        }
    }

    // ---- meters -----------------------------------------------------------
    {
        juce::Array<juce::var> sp, rmsArr, grArr, agArr, hzArr;
        for (int i = 0; i < mw::SCOPE_BINS; ++i) sp.add ((double) spectrum[(size_t) i]);
        for (int b = 0; b < mw::MAX_BANDS; ++b)
        {
            rmsArr.add ((double) engine.bandRms[(size_t) b]);
            grArr.add ((double) engine.bandGr[(size_t) b]);
            agArr.add ((double) engine.bandAuto[(size_t) b]);
        }
        for (int x = 0; x < mw::MAX_BANDS - 1; ++x) hzArr.add ((double) engine.xoverHzNow[(size_t) x]);

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("sp", sp);
        obj->setProperty ("rms", rmsArr);
        obj->setProperty ("gr", grArr);
        obj->setProperty ("ag", agArr);
        obj->setProperty ("hz", hzArr);
        obj->setProperty ("in", (double) engine.inRms);
        obj->setProperty ("out", (double) engine.outRms);
        obj->setProperty ("mgr", (double) engine.masterGr);
        juce::Array<juce::var> mods;
        for (int i = 0; i < mw::NUM_DESTS; ++i) mods.add ((double) engine.modOut[(size_t) i]);
        obj->setProperty ("mod", mods);
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
// Patches on disk — the same shape as the other two Brokild plugins.
namespace
{
    bool canWriteInto (const juce::File& dir)
    {
        if (! dir.isDirectory()) return false;
        const auto probe = dir.getChildFile (".marswars-write-test.tmp");
        if (! probe.replaceWithText ("x")) return false;
        probe.deleteFile();
        return true;
    }
}

juce::PropertiesFile& MarsWarsAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "The Mars Wars";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File MarsWarsAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/The Mars Wars/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    return brokild::patchFolder ("Martian Gain", { "\"martian-gain\"", "\"mars-wars\"" },
                                   "*.json", { "The Mars Wars" });
}

juce::File MarsWarsAudioProcessor::presetFolderOrDefault()
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
                                   .getChildFile ("Mars Wars Presets");
        }
    }
    return presetFolder;
}

void MarsWarsAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var MarsWarsAudioProcessor::presetScanDir (const juce::File& dir, int depth)
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

void MarsWarsAudioProcessor::presetScan()
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

void MarsWarsAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Where the presets live", start);
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

juce::String MarsWarsAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i)
        pv->setProperty (ids[i], (double) raw[(size_t) i]->load());

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "martian-gain");
    o->setProperty ("kind", "preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef MW_BUILD_ID
    o->setProperty ("build", juce::String (MW_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    return juce::JSON::toString (juce::var (o), false);
}

void MarsWarsAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject())
    { notice ("THAT FILE IS NOT A PRESET"); return; }
    /*  Renamed from THE MARS WARS: everything saved under the old name
        must still open, so both tags are accepted forever. */
    const auto app = v.getProperty ("app", "").toString();
    if (app != "martian-gain" && app != "mars-wars")
    { notice ("THAT IS NOT A MARS WARS PRESET"); return; }

    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
            if (apvts.getParameter (kv.name.toString()) != nullptr)
            { setParamById (kv.name.toString(), (float) (double) kv.value); ++applied; }


    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("LOADED \"" + name.toUpperCase() + "\" " + DOT + " " + juce::String (applied) + " VALUES");
}

void MarsWarsAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const int nb = (int) raw[(size_t) ids.indexOf ("bands")]->load();
    const auto stem = juce::File::createLegalFileName (juce::String (nb) + "-band preset");
    const auto suggested = dir.getChildFile (stem + ".json");

    activeChooser = std::make_unique<juce::FileChooser> ("Save this preset", suggested, "*.json");
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

void MarsWarsAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Open a preset", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void MarsWarsAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PRESET IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MarsWarsAudioProcessor();
}
