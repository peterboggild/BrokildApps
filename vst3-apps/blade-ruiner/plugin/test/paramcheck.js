/*  Three lists have to agree and nothing in the compiler checks them:

      1. the FLOATS / INTS / BOOLS tables that create the parameters,
      2. paramIds(), which fixes the order of the raw-value cache,
      3. the sequence of g(k++) reads in processBlock.

    A mismatch anywhere silently wires every control after the mistake to
    the wrong thing, and it would take a long time to hear that as a bug
    rather than as a quirk. This reads the source and compares them.       */
const fs = require('fs');
const src = fs.readFileSync(process.argv[2] + '/Source/PluginProcessor.cpp', 'utf8');
let bad = 0;

function block(startMarker, endMarker){
  const i = src.indexOf(startMarker), j = src.indexOf(endMarker, i);
  return src.slice(i, j);
}

// ---- 1. the tables
const declared = [];
for (const m of block('const FDef FLOATS[]', '};').matchAll(/\{\s*"([a-z0-9]+)"/g)) declared.push(m[1]);
for (const m of block('const BDef BOOLS[]',  '};').matchAll(/\{\s*"([a-z0-9]+)"/g)) declared.push(m[1]);
for (const m of block('const IDef INTS[]',   '};').matchAll(/\{\s*"([a-z0-9]+)"/g)) declared.push(m[1]);

// ---- 2. paramIds()
const idsBlock = block('juce::StringArray BladeRuinerAudioProcessor::paramIds()', '\n}');
const ids = [...idsBlock.matchAll(/"([a-z0-9]+)"/g)].map(m => m[1]);

// ---- 3. the read order in processBlock
const pb = block('void BladeRuinerAudioProcessor::processBlock', 'At least one layer');
const reads = [...pb.matchAll(/p\.([A-Za-z]+)\s*=\s*g\s*\(k\+\+\)/g)].map(m => m[1]);

// the engine field name each id maps to
const camel = id => {
  const map = { master:'master', tune:'tune', limiter:'limiter' };
  if (map[id]) return map[id];
  const p = id.slice(0,2), rest = id.slice(2);
  return p + rest.charAt(0).toUpperCase() + rest.slice(1);
};

function report(what, ok, detail){
  console.log((ok ? '  ok   ' : '  FAIL ') + what + (detail ? '   ' + detail : ''));
  if (!ok) ++bad;
}

console.log('\nBLADE RUINER  --  parameter wiring check\n');

report('every declared parameter is in paramIds()',
       declared.every(d => ids.includes(d)),
       declared.filter(d => !ids.includes(d)).join(' '));
report('every id in paramIds() is declared',
       ids.every(i => declared.includes(i)),
       ids.filter(i => !declared.includes(i)).join(' '));
report('no duplicates in paramIds()',
       new Set(ids).size === ids.length,
       ids.filter((v,i) => ids.indexOf(v) !== i).join(' '));
report('counts match  (' + declared.length + ' declared, ' + ids.length + ' listed, '
       + reads.length + ' read)',
       declared.length === ids.length && ids.length === reads.length);

const wrong = [];
for (let i = 0; i < Math.min(ids.length, reads.length); ++i)
  if (camel(ids[i]).toLowerCase() !== reads[i].toLowerCase())
    wrong.push('#' + i + ' ' + ids[i] + ' -> p.' + reads[i]);
report('processBlock reads them in the same order', wrong.length === 0, wrong.slice(0,6).join('; '));

// ---- 4. the panel's local unit table must agree with the C++ one
const ui = fs.readFileSync(process.argv[2] + '/Source/ui/ui.html', 'utf8');
const local = {};
const lu = ui.slice(ui.indexOf('const LOCAL_UNITS'), ui.indexOf('};', ui.indexOf('const LOCAL_UNITS')));
for (const m of lu.matchAll(/([a-z0-9]+)\s*:\s*"([a-z]+)"/g)) local[m[1]] = m[2];

const UNITNAME = ['pct','db','hz','sec','cent'];
const cppUnit = {};
for (const m of block('const FDef FLOATS[]','};').matchAll(/\{\s*"([a-z0-9]+)",[^,]*,[^,]*,\s*U_([A-Z]+)/g))
  cppUnit[m[1]] = m[2].toLowerCase();
for (const m of block('const IDef INTS[]','};').matchAll(/\{\s*"([a-z0-9]+)"[\s\S]*?"([a-z]+)"\s*\}/g))
  cppUnit[m[1]] = m[2];
for (const m of block('const BDef BOOLS[]','};').matchAll(/\{\s*"([a-z0-9]+)"/g))
  cppUnit[m[1]] = 'bool';

const unitBad = Object.keys(local).filter(k => cppUnit[k] && cppUnit[k] !== local[k]);
report('the panel agrees with the plugin about units', unitBad.length === 0,
       unitBad.map(k => k + ' ' + cppUnit[k] + '/' + local[k]).join(' '));
void UNITNAME;

// ---- 5. every id the panel binds actually exists
const bound = new Set();
for (const m of ui.matchAll(/mk(?:Slider|Knob|Seg|Stepper|Switch|Nexus|Mood)\s*\(\s*[A-Za-z0-9_$]+\s*,\s*"([a-z0-9]+)"/g))
  bound.add(m[1]);
for (const m of ui.matchAll(/on:s*"([a-z]{2}on)"/g)) bound.add(m[1]);
if (ui.indexOf('id="moodv"') >= 0) bound.add('mood');   // the header dial
const orphan = [...bound].filter(b => !ids.includes(b));
report('every control on the panel names a real parameter', orphan.length === 0, orphan.join(' '));

const unbound = ids.filter(i => !bound.has(i));
report('every parameter has a control  (' + (ids.length - unbound.length) + '/' + ids.length + ')',
       unbound.length === 0, unbound.join(' '));

console.log('\n' + (bad === 0 ? 'ALL CLEAR  (0 failures)' : bad + ' FAILURES') + '\n');
process.exit(bad === 0 ? 0 : 1);
