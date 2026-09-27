/*  Engine.h / Engine.cpp — an imported volume, WITHOUT touching the specimen
    parameter.

    The obvious move is to add a tenth entry to the SPECIMEN list. It is also
    wrong: the parameter is a normalised float mapped onto nine discrete slots,
    so a tenth would silently re-point every saved patch (1.0 means CORTEX
    today and would mean IMPORTED tomorrow), in project state as well as in
    patch files, with nothing to migrate from.

    So an import is an OVERRIDE, held in the state blob beside the lines rather
    than in the parameter list. The dial still selects among the nine for when
    the override is off, and the volume slot carries a sentinel so the existing
    "is the right volume loaded" check keeps working unchanged.

    node test/patch-engine-import.js
*/
const fs = require("fs"), path = require("path");
const S = path.resolve(__dirname, "..", "Source");
const edits = [];
function edit(file, name, from, to, count){
  count = count === undefined ? 1 : count;
  edits.push({ file, name, from, to, count });
}

edit("Engine.h", "sentinel",
`enum LineId { L_WAVE_A = 0, L_WAVE_B, L_FILT_A, L_FILT_B, L_MOD_A, L_MOD_B };`,
`enum LineId { L_WAVE_A = 0, L_WAVE_B, L_FILT_A, L_FILT_B, L_MOD_A, L_MOD_B };

/*  The volume slot a specimen index lives in also has to name "the imported
    one", and it must not collide with 0..8 or with the -1 that means empty. */
constexpr int SPEC_IMPORTED = -2;`);

edit("Engine.h", "api",
`    //  the lines — message thread writes, audio thread reads at a block edge
    void setLine (int which, const Line& l);`,
`    /*  An imported volume overrides the specimen dial until it is cleared.
        VN^3 floats in [0,1]; message thread only. */
    void setImported (const float* cube);
    void clearImported();
    bool importedActive() const { return importOn; }

    //  the lines — message thread writes, audio thread reads at a block edge
    void setLine (int which, const Line& l);`);

edit("Engine.h", "members",
`    Volume vols[2];
    std::atomic<int> volCur { 0 };
    int    volBuilding = -1;`,
`    Volume vols[2];
    std::atomic<int> volCur { 0 };
    int    volBuilding = -1;
    std::vector<float> importCube;          // VN^3, message thread owns it
    bool   importOn = false, importDirty = false;`);

edit("Engine.cpp", "service",
`void Engine::service()
{
    const int want = listIndex (paramSpec (paramIndex ("specimen")), p.specimen);
    if (vols[volCur.load()].specimen == want) return;
    const int spare = 1 - volCur.load();
    std::vector<float> tmp ((size_t) VN * VN * VN);
    buildSpecimen (want, tmp.data());
    vols[spare].build (tmp.data(), specimenPeriodicX (want));
    vols[spare].specimen = want;
    volCur.store (spare);
}`,
`void Engine::service()
{
    const int want = importOn ? SPEC_IMPORTED
                              : listIndex (paramSpec (paramIndex ("specimen")), p.specimen);
    if (vols[volCur.load()].specimen == want && ! importDirty) return;
    const int spare = 1 - volCur.load();
    if (importOn)
    {
        if (importCube.size() != (size_t) VN * VN * VN) { importOn = false; importDirty = false; return; }
        /*  A real body is not periodic in x: its wrap is a genuine edge, and
            the polyBLEP is what deals with it — the same path CORTEX takes. */
        vols[spare].build (importCube.data(), false);
    }
    else
    {
        std::vector<float> tmp ((size_t) VN * VN * VN);
        buildSpecimen (want, tmp.data());
        vols[spare].build (tmp.data(), specimenPeriodicX (want));
    }
    vols[spare].specimen = want;
    volCur.store (spare);
    importDirty = false;
}

void Engine::setImported (const float* cube)
{
    importCube.assign (cube, cube + (size_t) VN * VN * VN);
    importOn = true;
    importDirty = true;
    service();
}

void Engine::clearImported()
{
    if (! importOn) return;
    importOn = false;
    importDirty = true;
    importCube.clear();
    importCube.shrink_to_fit();
    service();
}`);

let bad = [];
for (const e of edits){
  const p = path.join(S, e.file);
  let s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const from = e.from.split("\n").join(NL);
  const n = s.split(from).length - 1;
  if (n !== e.count) { bad.push(e.file + " / " + e.name + " (found " + n + ")"); }
}
if (bad.length){ console.error("MISSED, nothing written:\n  " + bad.join("\n  ")); process.exit(1); }

/*  one file, one read, one write — two closures over the same file each holding
    their own copy is how half a change gets silently discarded */
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
