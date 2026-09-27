/*  Two hidden host parameters: SPACE WET (overall FX level, centre = today)
 *  and SPACE SYNC (tempo-lock the time-based effects, default off).
 *
 *  They are deliberately NOT on the panel - the metal has six knobs and the
 *  panel is the specification - but the DAW can reach them, automate them and
 *  save them with the project.
 *
 *  Both new rows go at the END of BO_SPECS. A trailing addition leaves every
 *  existing row's read order untouched; inserting into the middle silently
 *  wires everything after it to the wrong thing. */
const fs = require("fs");
const miss = [];
function patch(path, edits) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const [find, sub] of edits) {
    const f = Array.isArray(find) ? find.join(NL) : find;
    const r = Array.isArray(sub) ? sub.join(NL) : sub;
    const n = s.split(f).length - 1;
    if (n !== 1) { miss.push(path.split(/[\\/]/).pop() + ": " + n + " matches: " + f.split(NL)[0].trim().slice(0, 45)); continue; }
    s = s.replace(f, r);
  }
  return { path, s };
}

const EH = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.h";
const EC = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";
const PH = "C:/Users/peter/b/BattlestarOverdrive/Source/PluginProcessor.h";
const PC = "C:/Users/peter/b/BattlestarOverdrive/Source/PluginProcessor.cpp";

const eh = patch(EH, [[
["struct Params",
"{",
"    float mix        = 1.00f;",
"    float thrust     = 0.35f;",
"    float antithrust = 0.00f;",
"    float space      = 0.00f;",
"    float spectrum   = 0.50f;",
"    int   engine     = E_ION;",
"    bool  autorefill = true;",
"};"],
["/*  SPACE SYNC divisions, as a fraction of a whole note. Index 0 is FREE and",
"    is the default: with it, and with no host clock, the delay behaves exactly",
"    as it always did. */",
"enum { SYNC_FREE = 0, NUM_SYNC = 8 };",
"extern const float SYNC_BEATS[NUM_SYNC];      // in quarter notes",
"extern const char* SYNC_NAMES;                // pipe separated, for the host",
"",
"struct Params",
"{",
"    float mix        = 1.00f;",
"    float thrust     = 0.35f;",
"    float antithrust = 0.00f;",
"    float space      = 0.00f;",
"    float spectrum   = 0.50f;",
"    int   engine     = E_ION;",
"    bool  autorefill = true;",
"",
"    /*  Hidden: not on the metal, but the host can see both.",
"",
"        spaceWet is the overall FX level, and it is BIPOLAR about its centre:",
"        0.5 is exactly today's sound (an IEEE-exact x1.0, so a patch that",
"        never touches it is bit-identical), 0 turns every SPACE effect off,",
"        and 1 goes further than today. */",
"    float spaceWet   = 0.50f;",
"    int   spaceSync  = SYNC_FREE;",
"    double bpm       = 0.0;              // from the host; 0 means no clock",
"};"]
]]);

const ec = patch(EC, [[
["const char* engineName  (int e)"],
["/*  Whole note = 4 quarter notes, so 1/4 is 1.0 here. The triplet and dotted",
"    variants are the usual 2/3 and 3/2 of their straight value. */",
"const float SYNC_BEATS[NUM_SYNC] = { 0.0f, 4.0f, 2.0f, 1.0f, 2.0f/3.0f, 0.75f, 0.5f, 0.25f };",
"const char* SYNC_NAMES = \"FREE|1/1|1/2|1/4|1/4T|1/8D|1/8|1/16\";",
"",
"const char* engineName  (int e)"]
]]);

const ph = patch(PH, [[
["struct PSpec",
"{",
"    const char* id;",
"    const char* name;",
"    float       def;",
"    bool        stepped;        // a choice, not a continuous value",
"    int         steps;          // number of choices when stepped",
"};"],
["struct PSpec",
"{",
"    const char* id;",
"    const char* name;",
"    float       def;",
"    bool        stepped;        // a choice, not a continuous value",
"    int         steps;          // number of choices when stepped",
"    const char* choices;        // pipe separated names, or null",
"};"]
]]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
for (const f of [eh, ec, ph]) fs.writeFileSync(f.path, f.s);
console.log("Params, sync table and PSpec extended");
