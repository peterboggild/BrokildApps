/*  The adapter's half of the macros: the five host parameters, declared and
    pushed by shared code so each synth pays two lines rather than twenty.
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/adapter/bwfx_juce.h";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const Q = String.fromCharCode(34);
const miss = [];
function rep(a, b, tag) {
  const A = a.split(String.fromCharCode(10)).join(NL);
  const B = b.split(String.fromCharCode(10)).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

// ── the message ops the overlay's macro rail sends ─────────────────────────
rep(
'    else if (op == "blob")      // apply a whole rack blob (the preset path)',
[
'    else if (op == "macro")     // the rail moved a fader (not automation)',
'    {',
'        rack.setMacro ((int) m.getProperty ("i", 0),',
'                       (float) (double) m.getProperty ("v", 0.0));',
'    }',
'    else if (op == "assign")    // arm-and-click: wire a macro to a control',
'    {',
'        rack.setMacroAssign ((int) m.getProperty ("i", 0),',
'                             m.getProperty ("d", juce::var ("")).toString().toStdString(),',
'                             (float) (double) m.getProperty ("a", 0.0));',
'        return true;',
'    }',
'    else if (op == "unassign")  // clear one macro entirely',
'    {',
'        rack.clearMacroAssigns ((int) m.getProperty ("i", 0));',
'        return true;',
'    }',
'    else if (op == "blob")      // apply a whole rack blob (the preset path)'
].join("\n"), "ops");

// ── the state payload carries the wiring and the live values ───────────────
rep(
'    obj->setProperty ("presets", juce::JSON::parse (juce::String (bwfx::presetsJson())));\n'
+ '    return juce::var (obj);',
[
'    obj->setProperty ("presets", juce::JSON::parse (juce::String (bwfx::presetsJson())));',
'    /*  The wiring (structure, from the blob) and the live values (host',
'        parameters, which the rail shows but does not own). */',
'    obj->setProperty ("macros", juce::JSON::parse (juce::String (rack.macroAssignJson())));',
'    juce::Array<juce::var> mv;',
'    for (int i = 0; i < bwfx::kMacros; ++i) mv.add (rack.getMacro (i));',
'    obj->setProperty ("macroVals", mv);',
'    return juce::var (obj);'
].join("\n"), "stateVar");

// ── declaring and pushing the parameters, once for all seven synths ────────
rep("} // namespace bwfx_juce", [
'/*  THE FIVE HOST PARAMETERS.',
'',
'    This is the entire automatable surface of the rack, and the count is',
'    FROZEN — a parameter list that grows is the one thing the blob design',
'    exists to avoid. Ids and names are fixed forever for the same reason:',
'    hosts bind automation to the id and cache the name.',
'',
'    A synth needs two lines: addMacroParameters() in its layout, and',
'    pushMacros() once per processBlock. */',
'inline const char* macroParamId (int i)',
'{',
'    static const char* ids[bwfx::kMacros] =',
'        { "bwfx_macro1", "bwfx_macro2", "bwfx_macro3", "bwfx_macro4", "bwfx_macro5" };',
'    return ids[i < 0 ? 0 : (i >= bwfx::kMacros ? bwfx::kMacros - 1 : i)];',
'}',
'',
'inline const char* macroParamName (int i)',
'{',
'    static const char* names[bwfx::kMacros] =',
'        { "BWFX MACRO 1", "BWFX MACRO 2", "BWFX MACRO 3", "BWFX MACRO 4", "BWFX MACRO 5" };',
'    return names[i < 0 ? 0 : (i >= bwfx::kMacros ? bwfx::kMacros - 1 : i)];',
'}',
'',
'/*  Every macro defaults to 0, which is the contract: zero means the patch',
'    exactly as saved. A fresh instance therefore sounds as it always did, and',
'    a host that never touches these lanes cannot change the sound. */',
'template <typename Layout>',
'inline void addMacroParameters (Layout& layout)',
'{',
'    for (int i = 0; i < bwfx::kMacros; ++i)',
'        layout.add (std::make_unique<juce::AudioParameterFloat> (',
'            juce::ParameterID { macroParamId (i), 1 }, macroParamName (i),',
'            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,',
'            juce::AudioParameterFloatAttributes().withStringFromValueFunction (',
'                [] (float v, int) { return juce::String ((int) std::lround (v * 100.0f)) + " %"; })));',
'}',
'',
'/*  Call once per processBlock, next to setBpm. Cheap: five atomic loads and',
'    five stores, and nothing downstream happens unless something is assigned. */',
'inline void pushMacros (bwfx::Rack& rack, juce::AudioProcessorValueTreeState& apvts)',
'{',
'    for (int i = 0; i < bwfx::kMacros; ++i)',
'        if (auto* p = apvts.getRawParameterValue (macroParamId (i)))',
'            rack.setMacro (i, p->load());',
'}',
'',
'} // namespace bwfx_juce'
].join("\n"), "params");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("adapter: macro ops, state payload, and the five parameters");
