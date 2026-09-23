/*  A path-taking form of the import.

    {k:"import"} opens a native modal file chooser, which cannot be driven from
    a probe - so the whole feature would be verifiable only by hand. This is
    the same operation with the path supplied, which makes it testable live and
    is also what a drag-and-drop onto the panel would call.
*/
const fs = require("fs"), path = require("path");
const P = path.resolve(__dirname, "..", "Source", "PluginProcessor.cpp");
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const from = `    else if (k == "importClear")`.split("\n").join(NL);
const to = [
'    else if (k == "importPath")',
'    {',
'        //  the same import with the path handed in, so it can be driven from a',
'        //  probe and, later, from a file dropped on the panel',
'        const juce::File f (o->getProperty ("path").toString());',
'        if (f.exists()) doImport (f);',
'        else { importError = "no such file: " + f.getFullPathName(); emitImport(); notice (importError); }',
'    }',
'    else if (k == "importClear")'].join(NL);
if (s.split(from).length - 1 !== 1){ console.error("anchor not unique"); process.exit(1); }
fs.writeFileSync(P, s.split(from).join(to));
console.log("patched PluginProcessor.cpp");
