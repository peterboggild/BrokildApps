/*  Match parameters by NAME, not by paramID.
 *
 *  When HOSTING a VST3, JUCE hands back its own host-side wrapper parameters
 *  rather than the plug-in's AudioProcessorParameterWithID objects, so the id
 *  cast is always null and an id-based check reports everything missing even
 *  when the printed list plainly contains it. The name is what a DAW shows in
 *  its automation menu anyway. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/test/host/host.cpp";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = Array.isArray(find) ? find.join(NL) : find;
  const r = Array.isArray(sub) ? sub.join(NL) : sub;
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + f.split(NL)[0].trim().slice(0, 48)); return; }
  s = s.replace(f, r);
}

rep([
'        juce::String id;',
'        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p)) id = wid->paramID;'
], [
'        /*  Matched on NAME. Hosting a VST3 gives back JUCE\'s own host-side',
'            wrapper parameters, not the plug-in\'s AudioProcessorParameterWithID',
'            objects - so the id cast is always null here, and an id-based check',
'            reported BOTH new parameters "MISSING" while printing them in the',
'            list two lines above. A check that contradicts its own output is',
'            worse than no check. */',
'        const juce::String nm = p->getName (32);'
]);

rep([
'        if (id == "spacewet")  sawWet  = true;',
'        if (id == "spacesync") sawSync = true;'
], [
'        if (nm.equalsIgnoreCase ("SPACE WET"))  sawWet  = true;',
'        if (nm.equalsIgnoreCase ("SPACE SYNC")) sawSync = true;'
]);

rep('    std::printf ("  %-3s %-14s %-14s %-12s %s\\n", "#", "id", "name", "automatable", "value");',
    '    std::printf ("  %-3s %-16s %-12s %s\\n", "#", "name", "automatable", "value");');

rep([
'        std::printf ("  %-3d %-14s %-14s %-12s %s\\n",',
'                     idx++,',
'                     id.isEmpty() ? "-" : id.toRawUTF8(),',
'                     p->getName (14).toRawUTF8(),'
], [
'        std::printf ("  %-3d %-16s %-12s %s\\n",',
'                     idx++,',
'                     nm.toRawUTF8(),'
]);

rep([
'        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))',
'            if (wid->paramID == "spacewet")',
'            {'
], [
'        if (p->getName (32).equalsIgnoreCase ("SPACE WET"))',
'            {'
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("host probe matches on name now");
