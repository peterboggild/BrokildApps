/*  The bench asserted the additive contract. Under the mapping model the
    contract is different, and narrower — which is the point:

      * a rack with NOTHING assigned is bit-identical. Still true, still the
        one that protects every existing patch;
      * the DEFAULT wiring (macro 5 on the dry/wet at -100 %) leaves a fresh
        rack sounding exactly as it did. Still true, and now the only
        "neutral" claim worth making;
      * an ASSIGNED parameter is owned: at macro 0 it sits at one end of its
        range, at macro 1 at the other, and which end is the sign of the
        depth. "Assigning changes nothing" is no longer true and should not
        be asserted — it was the whole complaint.
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/test/bench.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const miss = [];
function rep(a, b, tag) {
  const A = a.split(String.fromCharCode(10)).join(NL);
  const B = b.split(String.fromCharCode(10)).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

rep('    std::printf ("-- MACROS: neutral at zero, additive, blob-compatible\\n");',
    '    std::printf ("-- MACROS: a mapping, owned destinations, blob-compatible\\n");', "header");

rep(
'        std::vector<float> at0, plain, up, topOut, down, botOut;\n'
+ '        echoRack (50.0f,  1.0f, 0.0f, at0);         // assigned, macro at rest\n'
+ '        echoRack (50.0f,  0.0f, 0.0f, plain);       // no macro at all\n'
+ '        CHECK (std::memcmp (at0.data(), plain.data(), at0.size() * 4) == 0,\n'
+ '               "an assigned macro at 0 is not identity");\n'
+ '\n'
+ '        echoRack (50.0f,  1.0f, 1.0f, up);          // +100 % -> clamps at the top\n'
+ '        echoRack (100.0f, 0.0f, 0.0f, topOut);\n'
+ '        const double dTop = settledDiff (up, topOut);\n'
+ '        std::printf ("   clamp: macro-driven vs set-to-top, settled %.2e\\n", dTop);\n'
+ '        CHECK (dTop < 2e-3, "macro at 1.0 x +100%% did not clamp to the top (%.2e)", dTop);\n'
+ '\n'
+ '        echoRack (50.0f, -1.0f, 1.0f, down);        // -100 % -> the bottom\n'
+ '        echoRack (0.0f,   0.0f, 0.0f, botOut);\n'
+ '        const double dBot = settledDiff (down, botOut);\n'
+ '        CHECK (dBot < 2e-3, "negative depth did not reach the bottom (%.2e)", dBot);',

'        /*  A MAPPING: the macro owns the destination and sweeps it end to\n'
+ '            end. Which end it starts from is the sign of the depth. Note\n'
+ '            what is NOT asserted here any more — that assigning changes\n'
+ '            nothing. Under a mapping it does, immediately, and that is the\n'
+ '            behaviour being asked for. */\n'
+ '        std::vector<float> lowEnd, atZero, up, topOut, down, botOut;\n'
+ '        echoRack (50.0f,  1.0f, 0.0f, atZero);      // owned, macro at rest\n'
+ '        echoRack (0.0f,   0.0f, 0.0f, lowEnd);      // the parameter at its bottom\n'
+ '        const double dZero = settledDiff (atZero, lowEnd);\n'
+ '        std::printf ("   +100%% at macro 0 vs the bottom: %.2e\\n", dZero);\n'
+ '        CHECK (dZero < 2e-3, "a +100%% macro at rest is not at the bottom (%.2e)", dZero);\n'
+ '\n'
+ '        echoRack (50.0f,  1.0f, 1.0f, up);          // ...and at the top when full\n'
+ '        echoRack (100.0f, 0.0f, 0.0f, topOut);\n'
+ '        const double dTop = settledDiff (up, topOut);\n'
+ '        std::printf ("   +100%% at macro 1 vs the top: %.2e\\n", dTop);\n'
+ '        CHECK (dTop < 2e-3, "macro at 1.0 x +100%% did not reach the top (%.2e)", dTop);\n'
+ '\n'
+ '        //  a NEGATIVE depth runs the other way: top at rest, bottom at full\n'
+ '        echoRack (50.0f, -1.0f, 0.0f, down);\n'
+ '        echoRack (100.0f, 0.0f, 0.0f, botOut);      // ...which is the TOP\n'
+ '        const double dNegRest = settledDiff (down, botOut);\n'
+ '        CHECK (dNegRest < 2e-3, "a -100%% macro at rest is not at the top (%.2e)", dNegRest);\n'
+ '\n'
+ '        std::vector<float> negFull, lowAgain;\n'
+ '        echoRack (50.0f, -1.0f, 1.0f, negFull);\n'
+ '        echoRack (0.0f,   0.0f, 0.0f, lowAgain);\n'
+ '        const double dNegFull = settledDiff (negFull, lowAgain);\n'
+ '        CHECK (dNegFull < 2e-3, "a -100%% macro at full is not at the bottom (%.2e)", dNegFull);',
  "section 4");

//  and one new claim the mapping makes: a destination belongs to ONE macro
rep(
'    //  6) assignments round-trip through the blob, and one this build cannot',
'    //  5b) a destination belongs to exactly one macro\n'
+ '    {\n'
+ '        Rack r;\n'
+ '        r.prepare (fs, 512);\n'
+ '        r.setMacroAssign (0, "delay.mix", 1.0f);\n'
+ '        r.setMacroAssign (2, "delay.mix", -0.5f);   // takes it off macro 1\n'
+ '        const std::string j = r.macroAssignJson();\n'
+ '        //  it must appear exactly once across all five\n'
+ '        int count = 0;\n'
+ '        for (size_t i = j.find ("delay.mix"); i != std::string::npos; i = j.find ("delay.mix", i + 1)) ++count;\n'
+ '        CHECK (count == 1, "delay.mix is owned by %d macros, want 1", count);\n'
+ '    }\n'
+ '\n'
+ '    //  6) assignments round-trip through the blob, and one this build cannot',
  "exclusive check");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench: the mapping contract");
