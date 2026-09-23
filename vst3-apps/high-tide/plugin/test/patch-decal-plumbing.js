/*  The decals into the binary and out through the resource provider, and into
    the probe's copy so the page loads them from file:// too. */
"use strict";
const fs = require("fs"), path = require("path");
const misses = [];
function edit(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  for (const [a, b] of pairs) {
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(path.basename(file) + ": expected 1 of " + JSON.stringify(a.slice(0, 60)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const decalDir = "C:/Users/peter/b/HighTide/Source/ui/decals";
const decals = fs.readdirSync(decalDir).filter(n => n.endsWith(".png")).sort();
if (!decals.length) { console.log("MISS: no decals in Source/ui/decals"); process.exit(1); }

const cmake = edit("C:/Users/peter/b/HighTide/CMakeLists.txt", [[
`    SOURCES
        Source/ui/ui.html
        "\${BWFX_DIR}/ui/bwfx-rack.js")`,
`    SOURCES
        Source/ui/ui.html
        "\${BWFX_DIR}/ui/bwfx-rack.js"
${decals.map(n => "        Source/ui/decals/" + n).join("\n")})`]]);

const ed = edit("C:/Users/peter/b/HighTide/Source/PluginEditor.cpp", [
  [`    juce::WebBrowserComponent::Options buildOptions (HighTideAudioProcessor& p)`,
`    /*  The panel's photographic parts, looked up by their original filename so
        renaming one does not need a matching change to a mangled symbol. */
    std::optional<juce::WebBrowserComponent::Resource> decalResource (const juce::String& file)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (file == juce::String (BinaryData::originalFilenames[i]))
            {
                int sz = 0;
                if (const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], sz))
                {
                    juce::WebBrowserComponent::Resource r;
                    r.data.resize ((size_t) sz);
                    std::memcpy (r.data.data(), d, (size_t) sz);
                    r.mimeType = "image/png";
                    return r;
                }
            }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (HighTideAudioProcessor& p)`],
  [`                if (path == "/bwfx-rack.js") return bwfxResource();
                return std::nullopt;`,
   `                if (path == "/bwfx-rack.js") return bwfxResource();
                if (path.startsWith ("/decals/")) return decalResource (path.fromLastOccurrenceOf ("/", false, false));
                return std::nullopt;`]
]);

const probe = edit("C:/Users/peter/b/HighTide/test/uiprobe.js", [[
`fs.copyFileSync(BWFX, path.join(SCR, "bwfx-rack.js"));`,
`fs.copyFileSync(BWFX, path.join(SCR, "bwfx-rack.js"));
//  the panel loads its decals from decals/ beside the page
const DEC = path.join(ROOT, "Source", "ui", "decals");
if (fs.existsSync(DEC)) {
    const to = path.join(SCR, "decals");
    fs.mkdirSync(to, { recursive: true });
    for (const n of fs.readdirSync(DEC)) fs.copyFileSync(path.join(DEC, n), path.join(to, n));
}`]]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
for (const [f, s] of [cmake, ed, probe]) fs.writeFileSync(f, s, "utf8");
console.log("plumbed " + decals.length + " decals: " + decals.join(", "));
