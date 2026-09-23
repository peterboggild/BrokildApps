/*  Wire the two hidden parameters through the processor and into the DSP. */
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
const PC = "C:/Users/peter/b/BattlestarOverdrive/Source/PluginProcessor.cpp";
const EC = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";

const pc = patch(PC, [
  // --- the table -----------------------------------------------------------
  [[
'    { "mix",        "MIX",        1.00f, false, 0 },',
'    { "thrust",     "THRUST",     0.35f, false, 0 },',
'    { "antithrust", "ANTITHRUST", 0.00f, false, 0 },',
'    { "engine",     "ENGINE",     1.00f, true,  bo::NUM_ENGINES },',
'    { "space",      "SPACE",      0.00f, false, 0 },',
'    { "spectrum",   "SPECTRUM",   0.50f, false, 0 },',
'    { "autorefill", "AUTOREFILL", 1.00f, true,  2 }',
'};'
  ], [
'    { "mix",        "MIX",        1.00f, false, 0,                nullptr },',
'    { "thrust",     "THRUST",     0.35f, false, 0,                nullptr },',
'    { "antithrust", "ANTITHRUST", 0.00f, false, 0,                nullptr },',
'    { "engine",     "ENGINE",     1.00f, true,  bo::NUM_ENGINES,  nullptr },',
'    { "space",      "SPACE",      0.00f, false, 0,                nullptr },',
'    { "spectrum",   "SPECTRUM",   0.50f, false, 0,                nullptr },',
'    { "autorefill", "AUTOREFILL", 1.00f, true,  2,                nullptr },',
'',
'    /*  Hidden: no pot on the metal, but the host can see, automate and save',
'        them. Added at the END of the table on purpose - processBlock reads in',
'        table order, so a trailing row leaves every existing read untouched',
'        while an inserted one silently rewires everything after it. */',
'    { "spacewet",   "SPACE WET",  0.50f, false, 0,                nullptr },',
'    { "spacesync",  "SPACE SYNC", 0.00f, true,  bo::NUM_SYNC,     bo::SYNC_NAMES }',
'};'
  ]],

  // --- the layout ----------------------------------------------------------
  [[
'        if (s.stepped && s.steps == bo::NUM_ENGINES)',
'        {',
'            juce::StringArray names;',
'            for (int e = 0; e < bo::NUM_ENGINES; ++e) names.add (bo::engineName (e));',
'            layout.add (std::make_unique<juce::AudioParameterChoice> (',
'                juce::ParameterID { s.id, 1 }, s.name, names, (int) s.def));',
'        }'
  ], [
'        if (s.choices != nullptr)',
'        {',
'            layout.add (std::make_unique<juce::AudioParameterChoice> (',
'                juce::ParameterID { s.id, 1 }, s.name,',
'                juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));',
'        }',
'        else if (s.stepped && s.steps == bo::NUM_ENGINES)',
'        {',
'            juce::StringArray names;',
'            for (int e = 0; e < bo::NUM_ENGINES; ++e) names.add (bo::engineName (e));',
'            layout.add (std::make_unique<juce::AudioParameterChoice> (',
'                juce::ParameterID { s.id, 1 }, s.name, names, (int) s.def));',
'        }'
  ]],

  // --- read them, and the host clock ---------------------------------------
  [[
'    current.autorefill = paramPtr[(size_t) k++]->load() > 0.5f;',
'    engine.setParams (current);'
  ], [
'    current.autorefill = paramPtr[(size_t) k++]->load() > 0.5f;',
'    current.spaceWet   = paramPtr[(size_t) k++]->load();',
'    current.spaceSync  = (int) paramPtr[(size_t) k++]->load();',
'',
'    // The host clock, for SPACE SYNC. Absent (or stopped at an unknown tempo)',
'    // leaves bpm at 0, and the engine then behaves exactly as if sync were off.',
'    current.bpm = 0.0;',
'    if (auto* ph = getPlayHead())',
'        if (auto pos = ph->getPosition())',
'            if (auto t = pos->getBpm())',
'                current.bpm = *t;',
'',
'    engine.setParams (current);'
  ]],

]);

const ec = patch(EC, [
  // --- the wet scaling and the sync ----------------------------------------
  [[
'                // 30 ms out to 600 and back again, so the knob passes THROUGH',
'                // the useful delay range instead of ending on it.',
'                const float u = clampf (q / 0.45f, 0.0f, 1.0f);',
'                dlTimeMs = 30.0f + 570.0f * std::sin ((float) PI * u);'
  ], [
'                // 30 ms out to 600 and back again, so the knob passes THROUGH',
'                // the useful delay range instead of ending on it.',
'                const float u = clampf (q / 0.45f, 0.0f, 1.0f);',
'                dlTimeMs = 30.0f + 570.0f * std::sin ((float) PI * u);',
'',
'                /*  SPACE SYNC: the delay locks to the host instead of sweeping.',
'                    FREE, or no clock, leaves the line above untouched - so the',
'                    default path is bit-identical to a build without any of',
'                    this. Engaging sync SNAPS the smoother rather than gliding',
'                    into the new time: a glide lands the first echo early, which',
'                    Black Rider shipped once and had to be fixed. */',
'                if (p.spaceSync > SYNC_FREE && p.bpm > 1.0)',
'                {',
'                    const float beats = SYNC_BEATS[(size_t) std::min (p.spaceSync, NUM_SYNC - 1)];',
'                    const float ms = (float) (beats * 60000.0 / p.bpm);',
'                    dlTimeMs = clampf (ms, 5.0f, 700.0f);',
'                    if (! syncWasOn || std::abs (dlTimeMs - dlTimeSm) > 40.0f)',
'                        dlTimeSm = dlTimeMs;',
'                    syncWasOn = true;',
'                }',
'                else syncWasOn = false;'
  ]],

  // --- the overall FX level -------------------------------------------------
  [[
'                const float raw = tapeAmt + std::max (hallAmt, shimAmt * 0.9f);',
'                if (raw > 1.0e-6f)',
'                {',
'                    const float inv = 1.0f / raw;',
'                    const float w = 0.62f * std::min (1.0f, raw);   // never fully wet',
'                    y[0] = y[0] * (1.0f - w) + wetL * inv * w;',
'                    y[1] = y[1] * (1.0f - w) + wetR * inv * w;',
'                }'
  ], [
'                const float raw = tapeAmt + std::max (hallAmt, shimAmt * 0.9f);',
'                if (raw > 1.0e-6f)',
'                {',
'                    const float inv = 1.0f / raw;',
'                    const float w = clampf (0.62f * std::min (1.0f, raw) * fxWet,',
'                                            0.0f, 0.92f);',
'                    y[0] = y[0] * (1.0f - w) + wetL * inv * w;',
'                    y[1] = y[1] * (1.0f - w) + wetR * inv * w;',
'                }'
  ]],

  [['                const float d = 0.85f * tremAmt;'],
   ['                const float d = 0.85f * clampf (tremAmt * fxWet, 0.0f, 1.0f);']],

  [['                const float dep = vibAmt * 0.0022f * (float) sr;'],
   ['                const float dep = clampf (vibAmt * fxWet, 0.0f, 1.0f) * 0.0022f * (float) sr;']],

  // --- compute it once per control tick ------------------------------------
  [['            antiOn  = sAnti  > 1.0e-4f;'],
   ['            antiOn  = sAnti  > 1.0e-4f;',
    '',
    '            /*  Overall FX level, BIPOLAR about the centre: 0.5 is exactly',
    '                1.0 - an IEEE-exact multiply, so a patch that never touches',
    '                it is bit-identical - 0 turns every SPACE effect off, and 1',
    '                goes half again beyond today. */',
    '            fxWet = (p.spaceWet <= 0.5f) ? p.spaceWet * 2.0f',
    '                                         : 1.0f + (p.spaceWet - 0.5f) * 1.6f;']],

  [['    float widthAmt = 0.0f, widthDelayTarget = 0.0f;'],
   ['    float widthAmt = 0.0f, widthDelayTarget = 0.0f, fxWet = 1.0f;']]
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
for (const f of [pc, ec]) fs.writeFileSync(f.path, f.s);
console.log("hidden parameters wired through");
