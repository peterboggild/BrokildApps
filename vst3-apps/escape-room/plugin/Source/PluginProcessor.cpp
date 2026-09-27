#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    /*  Every knob in the room is a plain 0..1 float. `kind` only decides what
        the host is told the number means — the engine never sees these. */
    enum Kind { K_PCT = 0, K_HZ, K_Q, K_ATK, K_REL, K_GLI, K_TWIN, K_SHARD, K_CHASM, K_TAIL, K_RUNE };

    struct PDef { const char* id; const char* name; float def; int kind; };

    // order is the wire order; the page indexes nothing by position, but the
    // raw-pointer cache and the echo comparison both walk this table.
    const PDef DEFS[] =
    {
        { "c1on",  "CELL I \xc2\xb7 OPEN",        1.0f,  K_PCT },
        { "c1lvl", "CELL I \xc2\xb7 MASS",        0.55f, K_PCT },
        { "c1a",   "CELL I \xc2\xb7 ALEPH",       0.44f, K_RUNE },
        { "c1b",   "CELL I \xc2\xb7 BETH",        0.55f, K_RUNE },

        { "c2on",  "CELL II \xc2\xb7 OPEN",       0.0f,  K_PCT },
        { "c2lvl", "CELL II \xc2\xb7 MASS",       0.40f, K_PCT },
        { "c2a",   "CELL II \xc2\xb7 ALEPH",      0.28f, K_RUNE },
        { "c2b",   "CELL II \xc2\xb7 BETH",       0.35f, K_RUNE },

        { "c3on",  "CELL III \xc2\xb7 OPEN",      1.0f,  K_PCT },
        { "c3lvl", "CELL III \xc2\xb7 MASS",      0.50f, K_PCT },
        { "c3a",   "CELL III \xc2\xb7 ALEPH",     0.42f, K_RUNE },
        { "c3b",   "CELL III \xc2\xb7 BETH",      0.55f, K_RUNE },

        { "c4on",  "CELL IV \xc2\xb7 OPEN",       1.0f,  K_PCT },
        { "c4lvl", "CELL IV \xc2\xb7 MASS",       0.38f, K_PCT },
        { "c4a",   "CELL IV \xc2\xb7 ALEPH",      0.34f, K_RUNE },
        { "c4b",   "CELL IV \xc2\xb7 BETH",       0.72f, K_RUNE },

        { "c5on",  "CELL V \xc2\xb7 OPEN",        0.0f,  K_PCT },
        { "c5lvl", "CELL V \xc2\xb7 MASS",        0.30f, K_PCT },
        { "c5a",   "CELL V \xc2\xb7 ALEPH",       0.45f, K_RUNE },
        { "c5b",   "CELL V \xc2\xb7 BETH",        0.30f, K_RUNE },

        { "dcut",  "DOOR \xc2\xb7 WHEEL",         0.45f, K_HZ },
        { "dres",  "DOOR \xc2\xb7 TEETH",         0.68f, K_Q },
        { "dkey",  "DOOR \xc2\xb7 TRACK",         1.0f,  K_PCT },
        { "dgli",  "DOOR \xc2\xb7 CREEP",         0.06f, K_GLI },
        { "datk",  "DOOR \xc2\xb7 ONSET",         0.18f, K_ATK },
        { "drel",  "DOOR \xc2\xb7 LETGO",         0.42f, K_REL },
        { "dfloor","DOOR \xc2\xb7 RESIDUE",       0.45f, K_PCT },
        { "denv",  "DOOR \xc2\xb7 SWEEP",         0.5f,  K_PCT },
        { "dtwin", "DOOR \xc2\xb7 TWIN",          0.0f,  K_TWIN },
        { "dtwa",  "DOOR \xc2\xb7 TWIN MASS",     0.40f, K_PCT },

        { "bind",  "LOOM \xc2\xb7 BIND",          0.35f, K_PCT },
        { "drift", "LOOM \xc2\xb7 DRIFT",         0.12f, K_PCT },
        { "rate",  "LOOM \xc2\xb7 PULSE",         0.35f, K_PCT },

        { "shamt", "TRAP \xe2\xa7\x97 \xc2\xb7 CHANCE",  0.0f,  K_PCT },
        { "shsize","TRAP \xe2\xa7\x97 \xc2\xb7 SHARD",   0.35f, K_SHARD },
        { "chtime","TRAP \xe2\x88\x9e \xc2\xb7 DEPTH",   0.32f, K_CHASM },
        { "chfeed","TRAP \xe2\x88\x9e \xc2\xb7 RETURN",  0.0f,  K_PCT },
        { "chtone","TRAP \xe2\x88\x9e \xc2\xb7 GRAIN",   0.5f,  K_PCT },
        { "vasize","TRAP \xe2\x98\x81 \xc2\xb7 SPACE",   0.55f, K_TAIL },
        { "vamix", "TRAP \xe2\x98\x81 \xc2\xb7 VEIL",    0.22f, K_PCT },
        { "vashim","TRAP \xe2\x98\x81 \xc2\xb7 ASCENT",  0.0f,  K_PCT },

        { "drive", "MOUTH",                        0.20f, K_PCT },
        { "gain",  "EXIT",                         0.70f, K_PCT }
    };
    constexpr int NUM_DEFS = (int) (sizeof (DEFS) / sizeof (PDef));

    const char* RUNES[8] = { "\xe1\x9a\xa2", "\xe1\x9a\xb1", "\xe1\x9b\x9e", "\xe1\x9a\xbe",
                             "\xe1\x9b\x92", "\xe1\x9a\xb7", "\xe1\x9b\x9d", "\xe1\x9a\xa6" };

    juce::String secText (float s)
    {
        return s < 1.0f ? juce::String (s * 1000.0f, s < 0.1f ? 1 : 0) + " ms"
                        : juce::String (s, 2) + " s";
    }

    juce::String fmtValue (int kind, float v)
    {
        switch (kind)
        {
            case K_HZ:    { const float f = er::xmap (v, 25.0f, 16000.0f);
                            return f < 1000.0f ? juce::String (f, 0) + " Hz"
                                               : juce::String (f / 1000.0f, 2) + " kHz"; }
            case K_Q:     return "Q " + juce::String (er::xmap (v, 0.55f, 58.0f), 1);
            case K_ATK:   return secText (er::xmap (v, 0.0008f, 4.0f));
            case K_REL:   return secText (er::xmap (v, 0.004f, 8.0f));
            case K_GLI:   return secText (er::xmap (v, 0.0008f, 1.6f));
            case K_TWIN:  return v < 0.005f ? juce::String ("shut")
                                            : juce::String (v * 24.0f, 1) + " semi";
            case K_SHARD: return secText (er::xmap (v, 0.018f, 0.45f));
            case K_CHASM: return secText (er::xmap (v, 0.007f, 1.9f));
            case K_TAIL:  return juce::String (er::xmap (v, 0.35f, 16.0f), 1) + " s";
            case K_RUNE:  { const int i = juce::jlimit (0, 7, (int) (v * 7.999f));
                            return juce::String (juce::CharPointer_UTF8 (RUNES[i]))
                                 + " " + juce::String ((int) std::round (v * 100.0f)); }
            default:      return juce::String ((int) std::round (v * 100.0f));
        }
    }
}

//==============================================================================
juce::StringArray EscapeRoomAudioProcessor::paramIds()
{
    juce::StringArray a;
    for (const auto& d : DEFS) a.add (d.id);
    a.add ("sigil");
    a.add ("dmode");
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout EscapeRoomAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : DEFS)
    {
        const int kind = d.kind;
        const bool isSwitch = juce::String (d.id).endsWith ("on");

        if (isSwitch)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { d.id, 1 }, d.name, d.def > 0.5f,
                juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                    [] (bool b, int) { return juce::String (b ? "OPEN" : "SEALED"); })));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { d.id, 1 }, d.name,
                juce::NormalisableRange<float> (0.0f, 1.0f), d.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [kind] (float v, int) { return fmtValue (kind, v); })));
        }
    }

    // The lock. Six bits, sixty-four rooms, and the same room every time.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "sigil", 1 }, "SIGIL", 0, 63, 23,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int)
            {
                juce::String s;
                for (int b = 5; b >= 0; --b) s += ((v >> b) & 1) ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xae"))
                                                                 : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xaf"));
                return s + " " + juce::String (v);
            })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "dmode", 1 }, "DOOR \xc2\xb7 SHAPE",
        juce::StringArray { juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x92 LOW")),
                            juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x92 BAND")),
                            juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x92 HIGH")),
                            juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x92 NOTCH")) }, 1));

    //  the rack's five automatable macros, declared by shared code so
    //  every synth carries the identical five (see bwfx_juce.h)
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
EscapeRoomAudioProcessor::EscapeRoomAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ESCAPEROOM", createParameterLayout())
{
    ids = paramIds();
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);

    startTimerHz (15);        // bwfxRack.service() - editor open or not
    bwfxRack.setWorldModConsumed (true);   // this engine maps the SPECTRA bus
}

EscapeRoomAudioProcessor::~EscapeRoomAudioProcessor() = default;

bool EscapeRoomAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void EscapeRoomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

//==============================================================================
void EscapeRoomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (panicFlag.exchange (false)) engine.panic();

    // notes tapped on the page
    {
        int s1, n1, s2, n2;
        noteFifo.prepareToRead (noteFifo.getNumReady(), s1, n1, s2, n2);
        auto play = [this] (const UiNote& u) { if (u.on) engine.noteOn (u.note, u.vel); else engine.noteOff (u.note); };
        for (int i = 0; i < n1; ++i) play (noteStore[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) play (noteStore[(size_t) (s2 + i)]);
        noteFifo.finishedRead (n1 + n2);
    }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if      (m.isNoteOn())          engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())         engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }

    auto& p = engine.p;
    auto g = [this] (int i) { return raw[(size_t) i]->load(); };
    int k = 0;
    for (int c = 0; c < er::NUM_CELLS; ++c)
    {
        p.cell[(size_t) c].on  = g (k++);
        p.cell[(size_t) c].lvl = g (k++);
        p.cell[(size_t) c].a   = g (k++);
        p.cell[(size_t) c].b   = g (k++);
    }
    p.cut = g (k++); p.res = g (k++); p.key = g (k++); p.glide = g (k++);
    p.atk = g (k++); p.rel = g (k++); p.floorLvl = g (k++); p.envd = g (k++);
    p.twin = g (k++); p.twinAmt = g (k++);
    p.bind = g (k++); p.drift = g (k++); p.rate = g (k++);
    p.shAmt = g (k++); p.shSize = g (k++);
    p.chTime = g (k++); p.chFeed = g (k++); p.chTone = g (k++);
    p.vaSize = g (k++); p.vaMix = g (k++); p.vaShim = g (k++);
    p.drive = g (k++); p.gain = g (k++);
    p.sigil = g (k++);
    p.mode  = (int) g (k++);

    // host tempo for the world rack's synced modules
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                bwfxRack.setBpm (*bpm);

    // SPECTRA world-mod bus: the rack's characters possess the door.
    // One block of modulation latency; a neutral bus is bit-identical.
    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth,
                            wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        // the world rack: one extra stage after the engine (empty = untouched)
    bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros
            bwfxRack.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else if (buffer.getNumChannels() == 1)
    {
        juce::HeapBlock<float> tmp ((size_t) n);
        engine.process (buffer.getWritePointer (0), tmp.get(), n);
        auto* l = buffer.getWritePointer (0);
        bwfxRack.process (l, tmp.get(), n);
        for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + tmp[i]);
    }
}

//==============================================================================
void EscapeRoomAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        copyXmlToBinary (*xml, dest);
    }
}

void EscapeRoomAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            xml->removeAttribute ("bwfx");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;                  // make the page take the new patch
            lastSent.assign ((size_t) ids.size(), -999.0f);
        }
}

juce::AudioProcessorEditor* EscapeRoomAudioProcessor::createEditor()
{
    return new EscapeRoomAudioProcessorEditor (*this);
}

//==============================================================================
void EscapeRoomAudioProcessor::handleUiMessage (const juce::var& payload)
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

void EscapeRoomAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")
    {
        const juce::String id = o->getProperty ("id").toString();
        const float v = (float) (double) o->getProperty ("v");
        if (auto* prm = apvts.getParameter (id))
        {
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            const int idx = ids.indexOf (id);
            if (idx >= 0) lastSent[(size_t) idx] = v;      // do not echo our own move back
        }
    }
    else if (k == "n")
    {
        UiNote u;
        u.note = (int) o->getProperty ("n");
        u.vel  = (float) (double) o->getProperty ("v");
        u.on   = (bool) o->getProperty ("on");
        int s1, n1, s2, n2;
        noteFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) noteStore[(size_t) s1] = u;
        else if (n2 > 0) noteStore[(size_t) s2] = u;
        noteFifo.finishedWrite (n1 + n2);
    }
    else if (k == "panic")  { panicFlag = true; }
    else if (k == "ack")    { uiHasState = true; }
    else if (k == "ready")  { uiReady = true; }
    else if (k == "bwfx")         { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "encode")       { presetSaveAs(); }
    else if (k == "decodeOpen")   { presetOpenDialog(); }
    else if (k == "presetScan")   { presetScan(); }
    else if (k == "presetFolder") { presetPickFolder(); }
    else if (k == "presetLoad")   { presetLoad (o->getProperty ("path").toString()); }
}

//==============================================================================
/*  Patches on disk.

    A patch is the forty-five numbers and nothing else — no photographs, no
    hidden state — so it is a small readable .json file that can be mailed,
    copied between machines or sorted in Explorer. The native side owns the
    disk; the page only asks. */

namespace
{
    /*  hasWriteAccess() answers from the read-only attribute rather than the
        ACL on Windows, so Program Files reports writable and then refuses.
        Ask the file system instead. */
    bool canWriteInto (const juce::File& dir)
    {
        if (! dir.isDirectory()) return false;
        const auto probe = dir.getChildFile (".escaperoom-write-test.tmp");
        if (! probe.replaceWithText ("x")) return false;
        probe.deleteFile();
        return true;
    }
}

void EscapeRoomAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

juce::PropertiesFile& EscapeRoomAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Escape Room";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

/*  A "User presets" folder beside the installed plugin travels with it and is
    the first place anyone looks. Only used when it can genuinely be written
    to — under Program Files it usually cannot be, and a preset folder that
    throws on every save is worse than one somewhere else. */
juce::File EscapeRoomAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/Escape Room/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    return brokild::patchFolder ("Escape Room", { "\"escape-room\"" });
}

juce::File EscapeRoomAudioProcessor::presetFolderOrDefault()
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
                                   .getChildFile ("Escape Room Patches");
        }
    }
    return presetFolder;
}

void EscapeRoomAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

// One level of subfolders becomes one level of submenus.
juce::var EscapeRoomAudioProcessor::presetScanDir (const juce::File& dir, int depth)
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
            if (kids.size() == 0) continue;       // no patches inside: no submenu
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

    struct Sorter                                 // folders first, then names
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

void EscapeRoomAudioProcessor::presetScan()
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

void EscapeRoomAudioProcessor::presetPickFolder()
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

juce::String EscapeRoomAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i)
        pv->setProperty (ids[i], (double) raw[(size_t) i]->load());

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "escape-room");
    o->setProperty ("kind", "patch");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef ER_BUILD_ID
    o->setProperty ("build", juce::String (ER_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));   // a patch stores its own rack
    return juce::JSON::toString (juce::var (o), false);
}

void EscapeRoomAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject())
    { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "escape-room")
    { notice ("THAT IS SOMEONE ELSE'S PATCH"); return; }

    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
    {
        for (const auto& kv : o->getProperties())
        {
            if (auto* prm = apvts.getParameter (kv.name.toString()))
            {
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (double) kv.value));
                ++applied;
            }
        }
    }

    // a patch stores its own rack; pre-BWFX patches come rack-empty
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    emitBwfx();

    // Force the echo loop to hand every value back to the page.
    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("DECODED \"" + name.toUpperCase() + "\" \xc2\xb7 " + juce::String (applied) + " VALUES");
}

void EscapeRoomAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const int sg = (int) raw[(size_t) ids.indexOf ("sigil")]->load();
    const auto stem = juce::File::createLegalFileName ("Room " + juce::String (sg));
    const auto suggested = dir.getChildFile (stem + ".json");

    activeChooser = std::make_unique<juce::FileChooser> ("Encode this patch", suggested, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;      // cancelled: say nothing
            if (! file.hasFileExtension ("json")) file = file.withFileExtension ("json");
            file.getParentDirectory().createDirectory();

            const auto name = file.getFileNameWithoutExtension();
            const bool ok = file.replaceWithText (patchJson (name));

            /*  Encoding somewhere else moves the patch folder there — but not
                into one of its own subfolders, or the menu loses its root. */
            if (ok)
            {
                const auto chosen = file.getParentDirectory();
                const auto root = presetFolderOrDefault();
                if (chosen != root && ! chosen.isAChildOf (root))
                    rememberPresetFolder (chosen);
            }

            notice (ok ? "ENCODED \"" + name.toUpperCase() + "\""
                       : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void EscapeRoomAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Decode a patch", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void EscapeRoomAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::var EscapeRoomAudioProcessor::wiringVar() const
{
    juce::Array<juce::var> arr;
    const auto* w = engine.wiring();
    for (int i = 0; i < er::NUM_SLOTS; ++i)
    {
        auto* s = new juce::DynamicObject();
        s->setProperty ("src", w[i].src);
        s->setProperty ("dst", w[i].dst);
        s->setProperty ("cur", w[i].curve);
        s->setProperty ("dep", (double) w[i].depth);
        arr.add (juce::var (s));
    }
    return arr;
}

void EscapeRoomAudioProcessor::emitInitialState()
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
        e->setProperty ("lo", 0.0);
        e->setProperty ("hi", 1.0);
        if (ids[i] == "sigil") e->setProperty ("hi", 63.0);
        if (ids[i] == "dmode") e->setProperty ("hi", 3.0);
        e->setProperty ("n", prm->getName (64));
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    obj->setProperty ("wiring", wiringVar());
    obj->setProperty ("sigil", (int) raw[(size_t) ids.indexOf ("sigil")]->load());
   #ifdef ER_BUILD_ID
    obj->setProperty ("build", juce::String (ER_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
}

void EscapeRoomAudioProcessor::emitBwfx()
{
    if (emitToUi)
        emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

void EscapeRoomAudioProcessor::emitWiring()
{
    if (! emitToUi) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("wiring", wiringVar());
    obj->setProperty ("sigil", lastSigilSent);
    emitToUi ("wiring", juce::var (obj));
}

//==============================================================================
void EscapeRoomAudioProcessor::timerService()
{
    if (! emitToUi) return;

    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;                                   // nothing else is worth sending yet
    }

    // anything the host (or another editor) moved
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

    const int sg = (int) raw[(size_t) ids.indexOf ("sigil")]->load();
    if (sg != lastSigilSent) { lastSigilSent = sg; emitWiring(); }

    // the panel's live picture of the room
    {
        juce::Array<juce::var> sc;
        for (int i = 0; i < er::SCOPE_LEN; ++i)
            sc.add ((double) engine.scope[(size_t) i]);

        juce::Array<juce::var> cells, mods, ve, vn;
        for (int i = 0; i < er::NUM_CELLS; ++i)  cells.add ((double) engine.cellRms[(size_t) i]);
        for (int i = 0; i < er::NUM_DST; ++i)    mods.add ((double) engine.modOut[(size_t) i]);
        for (int i = 0; i < er::NUM_VOICES; ++i) { ve.add ((double) engine.voiceEnv[(size_t) i]);
                                                   vn.add ((double) engine.voiceNote[(size_t) i]); }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("s", sc);
        obj->setProperty ("c", cells);
        obj->setProperty ("m", mods);
        obj->setProperty ("o", (double) engine.outRms);
        obj->setProperty ("d", (double) engine.doorRing);
        obj->setProperty ("ve", ve);
        obj->setProperty ("vn", vn);
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EscapeRoomAudioProcessor();
}

// relink 2
