// CHUNK E — the processor: transport into the engine, morph applied per block,
// the sequencer serialised, and patches on disk.
//
// Everything that is too big for an APVTS — twelve lanes of thirty-two steps
// across sixteen patterns, and two whole captured kits — rides as an opaque
// blob on the state, exactly as the BWFX rack does. Sparse: only steps that
// are ON are ever written, so a typical project carries a few hundred bytes.
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/Source/";

const writeH = edit(R + "PluginProcessor.h", [
[`    void handleOne (const juce::var& m);
    void emitInitialState();
    void emitBwfx();`,
`    void handleOne (const juce::var& m);
    void emitInitialState();
    void emitBwfx();
    void emitSeq();
    void emitKit();

    // ---- the sequencer and the captured kits, as opaque state -------------
    juce::var  seqVar() const;
    void       seqApply (const juce::var& v);
    juce::var  kitsVar() const;
    void       kitsApply (const juce::var& v);

    // ---- patches on disk --------------------------------------------------
    juce::PropertiesFile& userSettings();
    juce::File presetFolderOrDefault();
    static juce::File installedPresetFolder();
    void rememberPresetFolder (const juce::File& dir);
    void presetScan();
    void presetSaveAs();
    void presetOpenDialog();
    void presetLoad (const juce::String& path);
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> activeChooser;
    juce::File presetFolder;
    int kitIndex = 0;`, "processor header"]
]);

const writeC = edit(R + "PluginProcessor.cpp", [

// ---- transport into the engine, and morph applied per block --------------
[`                    engine.p.bpm = *bpm;
                    bwfxRack.setBpm (*bpm);
                    if (auto ppq = pos->getPpqPosition())
                        bwfxRack.setTransport (*bpm, *ppq, pos->getIsPlaying());
                }`,
`                    engine.p.bpm = *bpm;
                    bwfxRack.setBpm (*bpm);
                    const auto ppq = pos->getPpqPosition();
                    bwfxRack.setTransport (*bpm, ppq ? *ppq : 0.0, pos->getIsPlaying());
                    engine.setTransport (*bpm, ppq ? *ppq : -1.0, pos->getIsPlaying() && ppq.hasValue());
                }`, "transport"],

[`    for (int i = 0; i < fmr::numParams(); ++i)
        fmr::pvalue (engine.p, fmr::paramSpec (i)) = raw[(size_t) i]->load();

    if (auto* ph = getPlayHead())`,
`    for (int i = 0; i < fmr::numParams(); ++i)
        fmr::pvalue (engine.p, fmr::paramSpec (i)) = raw[(size_t) i]->load();

    /*  MORPH is applied to the engine's COPY of the parameters, never written
        back to the APVTS. A performance fader that silently rewrote a hundred
        knobs would make both its own automation and theirs unusable — this way
        the knobs stay where the player left them and the sound follows the
        fader. */
    engine.applyMorph (engine.p);

    if (auto* ph = getPlayHead())`, "morph"],

// ---- state: the sequencer and the kits ride as attributes ---------------
[`        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        copyXmlToBinary (*xml, dest);`,
`        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        xml->setAttribute ("seq",  juce::JSON::toString (seqVar(), true));
        xml->setAttribute ("kits", juce::JSON::toString (kitsVar(), true));
        xml->setAttribute ("kit",  kitIndex);
        copyXmlToBinary (*xml, dest);`, "save state"],

[`            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            xml->removeAttribute ("bwfx");`,
`            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            //  a project made before any of this simply has no attribute, and
            //  gets an empty sequencer and no captured kits — which is exactly
            //  what it sounded like
            seqApply  (juce::JSON::parse (xml->getStringAttribute ("seq")));
            kitsApply (juce::JSON::parse (xml->getStringAttribute ("kits")));
            kitIndex = xml->getIntAttribute ("kit", 0);
            xml->removeAttribute ("bwfx");
            xml->removeAttribute ("seq");
            xml->removeAttribute ("kits");
            xml->removeAttribute ("kit");`, "load state"],

// ---- the kit dial is the seed library now -------------------------------
[`void FmrAudioProcessor::applyKitIndex (int i)
{
    fmr::Params q;
    fmr::applyKit (i, q);
    for (int k = 0; k < fmr::numParams(); ++k)
    {
        const auto& s = fmr::paramSpec (k);
        const juce::String sid (s.id);
        if (sid == "os" || sid == "volume") continue;    // selectors, not a kit's character
        setParamById (s.id, fmr::pvalue (q, s));
    }
    bwfxRack.fromJson ("");        // a kit brings its own rack; for now, none
    emitBwfx();
    notice (juce::String ("KIT ") + fmr::kitName (i));
}`,
`void FmrAudioProcessor::applyKitIndex (int i)
{
    kitIndex = juce::jlimit (0, fmr::numSeeds() - 1, i);
    fmr::Params q;
    fmr::applySeed (kitIndex, q);
    for (int k = 0; k < fmr::numParams(); ++k)
    {
        const auto& s = fmr::paramSpec (k);
        const juce::String sid (s.id);
        //  the master, the oversampling and everything that belongs to the
        //  PERFORMANCE rather than to the kit is left alone
        if (sid == "os" || sid == "volume" || sid == "seq" || sid == "tempo"
            || sid == "morph" || sid == "swing" || sid == "feel" || sid == "grip") continue;
        setParamById (s.id, fmr::pvalue (q, s));
    }
    bwfxRack.fromJson ("");        // a kit brings its own rack; the seeds carry none
    emitBwfx();
    emitKit();
    notice (juce::String ("KIT ") + juce::String (kitIndex + 1) + " " +
            juce::String (fmr::seedCategoryName (fmr::seedCategory (kitIndex))) + " " +
            DOT + " " + fmr::kitName (kitIndex));
}

void FmrAudioProcessor::emitKit()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("i", kitIndex);
    o->setProperty ("name", juce::String (fmr::kitName (kitIndex)));
    o->setProperty ("cat", juce::String (fmr::seedCategoryName (fmr::seedCategory (kitIndex))));
    o->setProperty ("a", engine.haveA);
    o->setProperty ("b", engine.haveB);
    emitToUi ("kit", juce::var (o));
}

//==============================================================================
/*  The sequencer, sparse. Only steps that are ON are written, and only lanes
    that have any — an empty machine costs about forty bytes of state. */
juce::var FmrAudioProcessor::seqVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("cur", engine.curPat);
    juce::Array<juce::var> pats;
    for (int pi = 0; pi < fmr::NPAT; ++pi)
    {
        juce::Array<juce::var> lanes;
        for (int c = 0; c < fmr::NCH; ++c)
        {
            const auto& L = engine.pat[pi].lane[c];
            juce::Array<juce::var> steps;
            for (int k = 0; k < fmr::NSTEP; ++k)
            {
                const auto& st = L.step[k];
                if (! st.on) continue;
                juce::Array<juce::var> e;
                e.add (k); e.add ((int) st.vel); e.add ((int) st.prob);
                e.add ((int) st.ratchet); e.add ((int) st.cond); e.add ((int) st.micro);
                steps.add (juce::var (e));
            }
            const bool plain = steps.isEmpty() && L.len == 16 && L.div == 1
                            && L.dir == 0 && L.swing == 0 && L.mute == 0;
            if (plain) continue;
            auto* lo = new juce::DynamicObject();
            lo->setProperty ("c", c);
            lo->setProperty ("len", (int) L.len);
            lo->setProperty ("div", (int) L.div);
            lo->setProperty ("dir", (int) L.dir);
            lo->setProperty ("sw", (int) L.swing);
            lo->setProperty ("m", (int) L.mute);
            lo->setProperty ("st", steps);
            lanes.add (juce::var (lo));
        }
        if (lanes.isEmpty()) continue;
        auto* po = new juce::DynamicObject();
        po->setProperty ("i", pi);
        po->setProperty ("lanes", lanes);
        pats.add (juce::var (po));
    }
    root->setProperty ("pats", pats);
    return juce::var (root);
}

void FmrAudioProcessor::seqApply (const juce::var& v)
{
    for (int pi = 0; pi < fmr::NPAT; ++pi) engine.pat[pi] = fmr::Pattern();
    engine.curPat = 0;
    auto* o = v.getDynamicObject();
    if (o == nullptr) return;
    engine.curPat = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("cur"));
    if (auto* pats = o->getProperty ("pats").getArray())
        for (const auto& pv : *pats)
        {
            auto* po = pv.getDynamicObject();
            if (po == nullptr) continue;
            const int pi = juce::jlimit (0, fmr::NPAT - 1, (int) po->getProperty ("i"));
            if (auto* lanes = po->getProperty ("lanes").getArray())
                for (const auto& lv : *lanes)
                {
                    auto* lo = lv.getDynamicObject();
                    if (lo == nullptr) continue;
                    const int c = juce::jlimit (0, fmr::NCH - 1, (int) lo->getProperty ("c"));
                    auto& L = engine.pat[pi].lane[c];
                    L.len   = (uint8_t) juce::jlimit (1, fmr::NSTEP, (int) lo->getProperty ("len"));
                    L.div   = (uint8_t) juce::jlimit (0, 3, (int) lo->getProperty ("div"));
                    L.dir   = (uint8_t) juce::jlimit (0, 3, (int) lo->getProperty ("dir"));
                    L.swing = (int8_t)  juce::jlimit (-50, 50, (int) lo->getProperty ("sw"));
                    L.mute  = (uint8_t) ((int) lo->getProperty ("m") != 0 ? 1 : 0);
                    if (auto* steps = lo->getProperty ("st").getArray())
                        for (const auto& sv : *steps)
                            if (auto* e = sv.getArray())
                                if (e->size() >= 6)
                                {
                                    const int k = juce::jlimit (0, fmr::NSTEP - 1, (int) (*e)[0]);
                                    auto& st = L.step[k];
                                    st.on      = 1;
                                    st.vel     = (uint8_t) juce::jlimit (1, 127, (int) (*e)[1]);
                                    st.prob    = (uint8_t) juce::jlimit (0, 100, (int) (*e)[2]);
                                    st.ratchet = (uint8_t) juce::jlimit (1, 8,   (int) (*e)[3]);
                                    st.cond    = (uint8_t) juce::jlimit (0, 5,   (int) (*e)[4]);
                                    st.micro   = (int8_t)  juce::jlimit (-50, 50, (int) (*e)[5]);
                                }
                }
        }
}

juce::var FmrAudioProcessor::kitsVar() const
{
    auto* root = new juce::DynamicObject();
    auto pack = [] (const fmr::Params& p)
    {
        auto* o = new juce::DynamicObject();
        for (int i = 0; i < fmr::numParams(); ++i)
        {
            const auto& s = fmr::paramSpec (i);
            if (s.chan < 0) continue;
            o->setProperty (s.id, (double) fmr::pvalue (const_cast<fmr::Params&> (p), s));
        }
        return juce::var (o);
    };
    root->setProperty ("haveA", engine.haveA);
    root->setProperty ("haveB", engine.haveB);
    if (engine.haveA) root->setProperty ("A", pack (engine.kitA));
    if (engine.haveB) root->setProperty ("B", pack (engine.kitB));
    return juce::var (root);
}

void FmrAudioProcessor::kitsApply (const juce::var& v)
{
    engine.haveA = engine.haveB = false;
    auto* o = v.getDynamicObject();
    if (o == nullptr) return;
    auto unpack = [] (const juce::var& src, fmr::Params& dst)
    {
        dst = fmr::Params();
        if (auto* so = src.getDynamicObject())
            for (int i = 0; i < fmr::numParams(); ++i)
            {
                const auto& s = fmr::paramSpec (i);
                if (s.chan < 0) continue;
                if (so->hasProperty (s.id))
                    fmr::pvalue (dst, s) = (float) (double) so->getProperty (s.id);
            }
    };
    if ((bool) o->getProperty ("haveA")) { unpack (o->getProperty ("A"), engine.kitA); engine.haveA = true; }
    if ((bool) o->getProperty ("haveB")) { unpack (o->getProperty ("B"), engine.kitB); engine.haveB = true; }
}

void FmrAudioProcessor::emitSeq()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("seq", seqVar());
    emitToUi ("seq", juce::var (o));
}`, "kit dial + serialisation"],

// ---- the sequencer's running step goes out with the meters --------------
[`        juce::Array<juce::var> lv;
        for (int c = 0; c < fmr::NCH; ++c) lv.add ((double) engine.channelLevel (c));
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("m", lv);`,
`        juce::Array<juce::var> lv;
        for (int c = 0; c < fmr::NCH; ++c) lv.add ((double) engine.channelLevel (c));
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("m", lv);
        obj->setProperty ("step", engine.seqStep());`, "meter step"],

// ---- messages ------------------------------------------------------------
[`    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, m)) emitBwfx(); }`,
`    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, m)) emitBwfx(); }
    else if (k == "capture")
    {
        //  capture the CURRENT panel into kit A or B, so a morph is always
        //  between two things the player actually made
        fmr::Params q;
        for (int i = 0; i < fmr::numParams(); ++i)
            fmr::pvalue (q, fmr::paramSpec (i)) = raw[(size_t) i]->load();
        const bool toB = o->getProperty ("slot").toString() == "b";
        if (toB) { engine.kitB = q; engine.haveB = true; }
        else     { engine.kitA = q; engine.haveA = true; }
        emitKit();
        notice (toB ? "CAPTURED INTO B" : "CAPTURED INTO A");
    }
    else if (k == "step")
    {
        const int pi = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("p"));
        const int c  = juce::jlimit (0, fmr::NCH - 1,  (int) o->getProperty ("c"));
        const int s  = juce::jlimit (0, fmr::NSTEP - 1, (int) o->getProperty ("s"));
        auto& st = engine.pat[pi].lane[c].step[s];
        if (o->hasProperty ("on"))   st.on      = (uint8_t) ((int) o->getProperty ("on") ? 1 : 0);
        if (o->hasProperty ("vel"))  st.vel     = (uint8_t) juce::jlimit (1, 127, (int) o->getProperty ("vel"));
        if (o->hasProperty ("prob")) st.prob    = (uint8_t) juce::jlimit (0, 100, (int) o->getProperty ("prob"));
        if (o->hasProperty ("rt"))   st.ratchet = (uint8_t) juce::jlimit (1, 8,   (int) o->getProperty ("rt"));
        if (o->hasProperty ("cond")) st.cond    = (uint8_t) juce::jlimit (0, 5,   (int) o->getProperty ("cond"));
        if (o->hasProperty ("mic"))  st.micro   = (int8_t)  juce::jlimit (-50, 50, (int) o->getProperty ("mic"));
    }
    else if (k == "lane")
    {
        const int pi = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("p"));
        const int c  = juce::jlimit (0, fmr::NCH - 1,  (int) o->getProperty ("c"));
        auto& L = engine.pat[pi].lane[c];
        if (o->hasProperty ("len")) L.len   = (uint8_t) juce::jlimit (1, fmr::NSTEP, (int) o->getProperty ("len"));
        if (o->hasProperty ("div")) L.div   = (uint8_t) juce::jlimit (0, 3, (int) o->getProperty ("div"));
        if (o->hasProperty ("dir")) L.dir   = (uint8_t) juce::jlimit (0, 3, (int) o->getProperty ("dir"));
        if (o->hasProperty ("sw"))  L.swing = (int8_t)  juce::jlimit (-50, 50, (int) o->getProperty ("sw"));
        if (o->hasProperty ("m"))   L.mute  = (uint8_t) ((int) o->getProperty ("m") ? 1 : 0);
    }
    else if (k == "pat")    { engine.curPat = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("i")); emitSeq(); }
    else if (k == "clearpat")
    {
        engine.pat[engine.curPat] = fmr::Pattern();
        emitSeq();
        notice ("PATTERN CLEARED");
    }
    else if (k == "copypat")
    {
        const int to = juce::jlimit (0, fmr::NPAT - 1, (int) o->getProperty ("to"));
        engine.pat[to] = engine.pat[engine.curPat];
        engine.curPat = to;
        emitSeq();
        notice ("PATTERN COPIED");
    }
    else if (k == "seqget") { emitSeq(); }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan") { presetScan(); }
    else if (k == "presetLoad") { presetLoad (o->getProperty ("path").toString()); }`, "messages"],

// ---- initial state carries the kit library and the sequencer ------------
[`    juce::Array<juce::var> chans, kits;`,
 `    juce::Array<juce::var> chans, kits, cats;`, "arrays"],

[`    for (int i = 0; i < fmr::numKits(); ++i) kits.add (juce::String (fmr::kitName (i)));`,
`    for (int i = 0; i < fmr::numKits(); ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", juce::String (fmr::kitName (i)));
        e->setProperty ("c", fmr::seedCategory (i));
        kits.add (juce::var (e));
    }
    for (int i = 0; i < fmr::numSeedCategories(); ++i) cats.add (juce::String (fmr::seedCategoryName (i)));`, "kit list"],

[`    obj->setProperty ("kits", kits);`,
`    obj->setProperty ("kits", kits);
    obj->setProperty ("cats", cats);
    obj->setProperty ("nstep", fmr::NSTEP);
    obj->setProperty ("npat", fmr::NPAT);
    obj->setProperty ("kit", kitIndex);`, "initial state"],

[`    emitToUi ("initialState", juce::var (obj));
    emitBwfx();`,
 `    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
    emitSeq();
    emitKit();`, "emit seq"],

// ---- patches on disk ------------------------------------------------------
[`//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()`,
`//==============================================================================
//  PATCHES ON DISK
//
//  Default folder is "User patches" beside the installed .vst3, which is where
//  the rest of the Brokild plugins put theirs. NOTE: File::hasWriteAccess() is
//  useless on Windows — it answers from the read-only attribute, so Program
//  Files claims to be writable and then refuses. Probe by writing.
//==============================================================================
juce::File FmrAudioProcessor::installedPresetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (int i = 0; i < 6 && f.exists(); ++i)
    {
        if (f.getFileName().endsWithIgnoreCase (".vst3"))
            return f.getParentDirectory().getChildFile ("User patches");
        f = f.getParentDirectory();
    }
    return {};
}

juce::PropertiesFile& FmrAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "FullMetalRacket";
        o.filenameSuffix = "settings";
        o.folderName = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File FmrAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder.isDirectory()) return presetFolder;
    const auto remembered = userSettings().getValue ("patchFolder");
    if (remembered.isNotEmpty() && juce::File (remembered).isDirectory())
        return (presetFolder = juce::File (remembered));

    auto beside = installedPresetFolder();
    if (beside != juce::File())
    {
        beside.createDirectory();
        auto probe = beside.getChildFile (".fmr-write-probe");
        if (probe.replaceWithText ("ok")) { probe.deleteFile(); return (presetFolder = beside); }
    }
    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("Full Metal Racket");
    docs.createDirectory();
    return (presetFolder = docs);
}

void FmrAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    presetFolder = dir;
    userSettings().setValue ("patchFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

void FmrAudioProcessor::presetScan()
{
    if (! emitToUi) return;
    auto dir = presetFolderOrDefault();
    juce::Array<juce::var> files;
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.fmrkit"))
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", f.getFileNameWithoutExtension());
        e->setProperty ("p", f.getFullPathName());
        files.add (juce::var (e));
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("dir", dir.getFullPathName());
    o->setProperty ("files", files);
    emitToUi ("patches", juce::var (o));
}

juce::String FmrAudioProcessor::patchJson (const juce::String& name)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "fmrkit");
    root->setProperty ("version", 1);
    root->setProperty ("name", name);
   #ifdef FM_BUILD_ID
    root->setProperty ("build", juce::String (FM_BUILD_ID));
   #endif
    auto* ps = new juce::DynamicObject();
    for (int i = 0; i < fmr::numParams(); ++i)
        ps->setProperty (fmr::paramSpec (i).id, (double) raw[(size_t) i]->load());
    root->setProperty ("params", juce::var (ps));
    root->setProperty ("seq", seqVar());
    root->setProperty ("kits", kitsVar());
    root->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (root), false);
}

void FmrAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    const auto v = juce::JSON::parse (json);
    auto* o = v.getDynamicObject();
    if (o == nullptr) { notice ("NOT A PATCH FILE"); return; }

    if (auto* ps = o->getProperty ("params").getDynamicObject())
        for (int i = 0; i < fmr::numParams(); ++i)
        {
            const auto& s = fmr::paramSpec (i);
            //  a patch written by an older build simply has fewer keys, and
            //  anything it does not name keeps its current value
            if (ps->hasProperty (s.id))
                setParamById (s.id, (float) (double) ps->getProperty (s.id));
        }
    seqApply  (o->getProperty ("seq"));
    kitsApply (o->getProperty ("kits"));
    bwfxRack.fromJson (o->getProperty ("bwfx").toString().toStdString());
    emitBwfx(); emitSeq(); emitKit();
    notice ("LOADED " + name.toUpperCase());
}

void FmrAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    activeChooser = std::make_unique<juce::FileChooser> (
        "Save kit", dir.getChildFile (juce::String (fmr::kitName (kitIndex)) + ".fmrkit"), "*.fmrkit");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("fmrkit")) f = f.withFileExtension ("fmrkit");
            if (f.replaceWithText (patchJson (f.getFileNameWithoutExtension())))
            {
                //  remember a NEW folder, but not a subfolder of the current one
                auto d = f.getParentDirectory();
                if (! d.isAChildOf (presetFolderOrDefault())) rememberPresetFolder (d);
                notice ("SAVED " + f.getFileNameWithoutExtension().toUpperCase());
                presetScan();
            }
            else notice ("COULD NOT WRITE THAT FILE");
        });
}

void FmrAudioProcessor::presetOpenDialog()
{
    activeChooser = std::make_unique<juce::FileChooser> ("Open kit", presetFolderOrDefault(), "*.fmrkit");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) presetLoad (f.getFullPathName());
        });
}

void FmrAudioProcessor::presetLoad (const juce::String& path)
{
    juce::File f (path);
    if (! f.existsAsFile()) { notice ("NO SUCH FILE"); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()`, "patches"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeH(); writeC();
console.log("chunk E patched OK — transport, morph, sequencer state, patches on disk");
