// 260904.3 — the SPECIMEN dial grew from nine slots to twelve. The parameter is
// normalised, so a value written by an older build points at the wrong tissue.
// This patch stamps the build id into the project state and remaps the dial
// once, keyed on the build that wrote it (patch files already carry "build";
// a project with none is older than any build that does).
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];

function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${rel}: expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { p, get: () => s };
}

const hdr = edit("Source/PluginProcessor.h", (rep) => {
  rep(String.raw`    void applyPatchJson (const juce::String& json, const juce::String& name);`,
      String.raw`    void applyPatchJson (const juce::String& json, const juce::String& name);
    void migrateSpecimen (const juce::String& writtenBy);`);
});

const cpp = edit("Source/PluginProcessor.cpp", (rep) => {
  rep(String.raw`    state.setProperty ("patchUser", patchIsUser, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);`,
      String.raw`    state.setProperty ("patchUser", patchIsUser, nullptr);
    state.setProperty ("build", juce::String (BS_BUILD_ID), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);`);
  rep(String.raw`        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();`,
      String.raw`        const juce::String built = tree.getProperty ("build", juce::String()).toString();
        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();`);
  rep(String.raw`        for (const char* k : { "bwfx", "lines", "patchName", "patchUser",
                               "importCube", "importPath", "importNote", "importAxis", "importStretch" })
            tree.removeProperty (k, nullptr);
        apvts.replaceState (tree);`,
      String.raw`        for (const char* k : { "build", "bwfx", "lines", "patchName", "patchUser",
                               "importCube", "importPath", "importNote", "importAxis", "importStretch" })
            tree.removeProperty (k, nullptr);
        apvts.replaceState (tree);
        migrateSpecimen (built);`);
  rep(String.raw`            if (apvts.getParameter (kv.name.toString()) != nullptr) { setParamById (kv.name.toString(), (float) (double) kv.value, false); ++applied; }
    linesFromVar (v.getProperty ("lines", juce::var()));`,
      String.raw`            if (apvts.getParameter (kv.name.toString()) != nullptr) { setParamById (kv.name.toString(), (float) (double) kv.value, false); ++applied; }
    migrateSpecimen (v.getProperty ("build", juce::var ("")).toString());
    linesFromVar (v.getProperty ("lines", juce::var()));`);
  rep(String.raw`void BrainScanAudioProcessor::presetSaveAs()
{`,
      String.raw`/*  260904.3 gave the SPECIMEN dial twelve slots where it had nine. The
    parameter is normalised (index / (slots - 1)), so a value written by an
    older build lands on the wrong tissue — CORTEX at 8/8 would read TENDON at
    11/11. Remap once, keyed on the build that wrote the patch or project; a
    project with no build id at all predates every build that writes one. */
void BrainScanAudioProcessor::migrateSpecimen (const juce::String& writtenBy)
{
    if (writtenBy.isNotEmpty() && writtenBy.compare ("260904.3") >= 0) return;
    auto* rawSpec = apvts.getRawParameterValue ("specimen");
    if (rawSpec == nullptr) return;
    const int idx = juce::jlimit (0, 8, (int) std::lround (rawSpec->load() * 8.0f));
    setParamById ("specimen", (float) idx / 11.0f, false);
}

void BrainScanAudioProcessor::presetSaveAs()
{`);
});

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [hdr, cpp]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("patched: PluginProcessor.h, PluginProcessor.cpp (migrateSpecimen)");
