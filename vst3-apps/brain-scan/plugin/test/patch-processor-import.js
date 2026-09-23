/*  PluginProcessor — the import, wired to the panel and to the state.

    Decisions worth stating, because they are not the obvious ones.

    THE CUBE IS SAVED, NOT THE PATH. A patch is the whole machine, so an
    imported specimen has to travel with it: 64^3 bytes, gzipped and base64'd
    into the state and into the patch file, which is a good deal smaller than
    it sounds because a resampled body is smooth. Keeping only the path would
    make a patch that breaks when a folder is tidied.

    THE OPTIONS RE-RUN THE IMPORT. Phase axis and fit change how the source is
    read, not how the cube is displayed, so they cannot be applied to a cube
    that has already been made. The source path is remembered so changing
    either one re-reads the file; if the file has gone, it says so and leaves
    the cube alone rather than losing it.

    node test/patch-processor-import.js
*/
const fs = require("fs"), path = require("path");
const S = path.resolve(__dirname, "..", "Source");
const edits = [];
const E = (file, name, from, to) => edits.push({ file, name, from, to });

//==============================================================================
E("PluginProcessor.h", "members",
`    juce::StringArray ids;`,
`    //  the imported volume, if any
    void       doImport (const juce::File& f);
    void       reImport();
    void       emitImport();
    juce::String importCubeBase64() const;
    bool       importCubeFromBase64 (const juce::String& b64);

    std::vector<float> importCube;          // VN^3 in [0,1], empty when none
    juce::String importPath, importNote, importError;
    int   importAxis = 0;
    bool  importStretch = false;
    float importWindowLo = 0, importWindowHi = 0;
    int   importFilled[3] { 0, 0, 0 };
    float importSpacing[3] { 0, 0, 0 };

    juce::StringArray ids;`);

//==============================================================================
E("PluginProcessor.cpp", "includes",
`#include "brokild_paths.h"`,
`#include "brokild_paths.h"
#include "Import.h"
#include "ImportJuce.h"`);

E("PluginProcessor.cpp", "messages",
`    else if (k == "hello")   { uiHasState.store (false); specimenSent = -1; emitInitialState(); }`,
`    else if (k == "import")
    {
        auto start = importPath.isNotEmpty() ? juce::File (importPath).getParentDirectory()
                                             : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        activeChooser = std::make_unique<juce::FileChooser> (
            "A volume: a .nii or .nii.gz, a folder of DICOM, or a folder of images", start);
        activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File{}) return;
                doImport (f);
            });
    }
    else if (k == "importClear")
    {
        engine.clearImported();
        importCube.clear(); importPath = {}; importNote = {}; importError = {};
        specimenSent = -1;
        emitImport(); emitVolume();
        notice ("the specimen dial is back");
    }
    else if (k == "importOpts")
    {
        if (o->hasProperty ("axis"))    importAxis = juce::jlimit (0, 2, (int) o->getProperty ("axis"));
        if (o->hasProperty ("stretch")) importStretch = (bool) o->getProperty ("stretch");
        reImport();
    }
    else if (k == "hello")   { uiHasState.store (false); specimenSent = -1; emitInitialState(); }`);

E("PluginProcessor.cpp", "doImport",
`void BrainScanAudioProcessor::notice (const juce::String& msg)`,
`//==============================================================================
void BrainScanAudioProcessor::doImport (const juce::File& f)
{
    bs::SrcVolume src;
    juce::String err;
    if (! bsjuce::loadVolume (f, src, err))
    {
        importError = err;
        emitImport();
        notice (err);
        return;
    }

    bs::ImportOpts opts;
    opts.phaseAxis = importAxis;
    opts.stretch   = importStretch;

    bs::ImportReport rep;
    std::string e;
    std::vector<float> cube ((size_t) bs::VN * bs::VN * bs::VN);
    if (! bs::resampleToCube (src, bs::VN, opts, cube.data(), rep, e))
    {
        importError = juce::String (juce::CharPointer_UTF8 (e.c_str()));
        emitImport();
        notice (importError);
        return;
    }

    importCube.swap (cube);
    importPath = f.getFullPathName();
    importError = {};
    importWindowLo = rep.loValue; importWindowHi = rep.hiValue;
    for (int a = 0; a < 3; ++a) { importFilled[a] = rep.filled[a]; importSpacing[a] = rep.srcSpacing[a]; }
    importNote = juce::String (juce::CharPointer_UTF8 (rep.note.c_str()))
               + "  at " + juce::String (rep.srcSpacing[0], 2) + " x " + juce::String (rep.srcSpacing[1], 2)
               + " x " + juce::String (rep.srcSpacing[2], 2) + " mm";

    engine.setImported (importCube.data());
    specimenSent = -1;
    emitImport(); emitVolume();
    notice ("imported " + f.getFileName() + " - window "
            + juce::String ((int) rep.loValue) + " to " + juce::String ((int) rep.hiValue));
}

void BrainScanAudioProcessor::reImport()
{
    if (importPath.isEmpty()) { emitImport(); return; }
    const juce::File f (importPath);
    if (! f.exists())
    {
        importError = "the file this came from has moved; the volume already imported is kept.";
        emitImport();
        notice (importError);
        return;
    }
    doImport (f);
}

juce::String BrainScanAudioProcessor::importCubeBase64() const
{
    const size_t n = (size_t) bs::VN * bs::VN * bs::VN;
    if (importCube.size() != n) return {};
    std::vector<juce::uint8> bytes (n);
    for (size_t i = 0; i < n; ++i)
        bytes[i] = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (importCube[i] * 255.0f));
    juce::MemoryOutputStream packed;
    {
        juce::GZIPCompressorOutputStream gz (packed, 9);
        gz.write (bytes.data(), bytes.size());
    }
    return juce::Base64::toBase64 (packed.getData(), packed.getDataSize());
}

bool BrainScanAudioProcessor::importCubeFromBase64 (const juce::String& b64)
{
    if (b64.isEmpty()) return false;
    juce::MemoryOutputStream raw;
    if (! juce::Base64::convertFromBase64 (raw, b64)) return false;
    juce::MemoryInputStream src (raw.getData(), raw.getDataSize(), false);
    juce::GZIPDecompressorInputStream gz (&src, false, juce::GZIPDecompressorInputStream::zlibFormat);
    juce::MemoryOutputStream out;
    out.writeFromInputStream (gz, -1);
    const size_t n = (size_t) bs::VN * bs::VN * bs::VN;
    if (out.getDataSize() != n) return false;
    const auto* p = (const juce::uint8*) out.getData();
    importCube.resize (n);
    for (size_t i = 0; i < n; ++i) importCube[i] = (float) p[i] / 255.0f;
    engine.setImported (importCube.data());
    specimenSent = -1;
    return true;
}

void BrainScanAudioProcessor::emitImport()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", engine.importedActive());
    o->setProperty ("path", importPath);
    o->setProperty ("name", importPath.isEmpty() ? juce::String()
                                                 : juce::File (importPath).getFileName());
    o->setProperty ("note", importNote);
    o->setProperty ("err", importError);
    o->setProperty ("axis", importAxis);
    o->setProperty ("stretch", importStretch);
    o->setProperty ("lo", (double) importWindowLo);
    o->setProperty ("hi", (double) importWindowHi);
    juce::Array<juce::var> fl;
    for (int a = 0; a < 3; ++a) fl.add (importFilled[a]);
    o->setProperty ("filled", fl);
    emitToUi ("import", juce::var (o));
}

void BrainScanAudioProcessor::notice (const juce::String& msg)`);

E("PluginProcessor.cpp", "initial state also sends the import",
`    emitVolume(); emitLines(); emitBwfx(); emitPatch(); presetScan();`,
`    emitVolume(); emitLines(); emitBwfx(); emitPatch(); emitImport(); presetScan();`);

E("PluginProcessor.cpp", "volume name",
`    o->setProperty ("name", juce::String (specimenName (v.specimen)));`,
`    o->setProperty ("name", v.specimen == SPEC_IMPORTED ? juce::String ("IMPORTED")
                                                        : juce::String (specimenName (v.specimen)));`);

E("PluginProcessor.cpp", "save state",
`    state.setProperty ("lines", juce::JSON::toString (linesToVar(), true), nullptr);`,
`    state.setProperty ("lines", juce::JSON::toString (linesToVar(), true), nullptr);
    /*  the imported cube travels with the project, so a patch is still the
        whole machine when the folder it came from has been tidied away */
    if (engine.importedActive())
    {
        state.setProperty ("importCube", importCubeBase64(), nullptr);
        state.setProperty ("importPath", importPath, nullptr);
        state.setProperty ("importNote", importNote, nullptr);
        state.setProperty ("importAxis", importAxis, nullptr);
        state.setProperty ("importStretch", importStretch, nullptr);
    }`);

E("PluginProcessor.cpp", "restore state",
`        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();
        const juce::String ln   = tree.getProperty ("lines", juce::String()).toString();
        patchName = tree.getProperty ("patchName", patchName).toString();
        patchIsUser = (bool) tree.getProperty ("patchUser", false);
        for (const char* k : { "bwfx", "lines", "patchName", "patchUser" }) tree.removeProperty (k, nullptr);`,
`        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();
        const juce::String ln   = tree.getProperty ("lines", juce::String()).toString();
        const juce::String cube = tree.getProperty ("importCube", juce::String()).toString();
        patchName = tree.getProperty ("patchName", patchName).toString();
        patchIsUser = (bool) tree.getProperty ("patchUser", false);
        importPath = tree.getProperty ("importPath", juce::String()).toString();
        importNote = tree.getProperty ("importNote", juce::String()).toString();
        importAxis = juce::jlimit (0, 2, (int) tree.getProperty ("importAxis", 0));
        importStretch = (bool) tree.getProperty ("importStretch", false);
        for (const char* k : { "bwfx", "lines", "patchName", "patchUser",
                               "importCube", "importPath", "importNote", "importAxis", "importStretch" })
            tree.removeProperty (k, nullptr);`);

E("PluginProcessor.cpp", "apply the restored cube",
`        if (ln.isNotEmpty()) { juce::var v; if (! juce::JSON::parse (ln, v).failed()) linesFromVar (v); }
        emitInitialState();`,
`        if (ln.isNotEmpty()) { juce::var v; if (! juce::JSON::parse (ln, v).failed()) linesFromVar (v); }
        /*  a project that had no import must not inherit one from the
            instance this state is being loaded into */
        engine.clearImported();
        importCube.clear();
        if (cube.isNotEmpty() && ! importCubeFromBase64 (cube))
            importError = "the imported volume in this project could not be read back.";
        emitInitialState();`);

let bad = [];
for (const e of edits){
  const p = path.join(S, e.file);
  const s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const n = s.split(e.from.split("\n").join(NL)).length - 1;
  if (n !== 1) bad.push(e.file + " / " + e.name + " (found " + n + ")");
}
if (bad.length){ console.error("MISSED, nothing written:\n  " + bad.join("\n  ")); process.exit(1); }

const byFile = {};
for (const e of edits) (byFile[e.file] = byFile[e.file] || []).push(e);
for (const f in byFile){
  const p = path.join(S, f);
  let s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const e of byFile[f]) s = s.split(e.from.split("\n").join(NL)).join(e.to.split("\n").join(NL));
  fs.writeFileSync(p, s);
  console.log("patched " + f + "  (" + byFile[f].length + " edits)");
}
