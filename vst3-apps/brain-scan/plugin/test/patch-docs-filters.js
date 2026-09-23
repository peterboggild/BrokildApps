// 260905.2 — the circuits in the manual, the landing page, app.json, the design
// doc, CLAUDE.md and the BUGLIST.
const fs = require("fs");
const path = require("path");
const misses = [];
const BT = String.fromCharCode(96);
const c = (t) => BT + t + BT;
function edit(file, fn) {
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${path.basename(file)}: expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  const app = (text) => { s = s.replace(/\s*$/, "") + NL + text.split("\n").join(NL); };
  fn(rep, app);
  return { file, get: () => s };
}
const web = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/brain-scan";

const manual = edit(path.join(__dirname, "..", "docs", "manual", "manual.html"), (rep) => {
  rep(String.raw`    <h3>FILTER</h3>
    <p>A state-variable filter with <b>LOW</b>, <b>BAND</b>, <b>HIGH</b> and <b>OFF</b>. CUTOFF is`,
      String.raw`    <h3>FILTER</h3>
    <p><b>CIRCUIT</b> chooses what the filter is made of. <b>SVF</b> is the state-variable filter
    the instrument shipped with. The other three are Black Rider's, ported whole: <b>GROWL</b>, a
    Sallen-Key with a diode clipper in its feedback — a gnarly resonance that screams only at the
    top of RESONANCE; <b>SCREAM</b>, the same circuit pushed harder, self-oscillating from two
    o'clock up, in tune, so with KEY TRACK it plays from the keyboard; <b>LADDER</b>, the four-pole
    transistor ladder, its bass thinning as the resonance rises. Each of the three is a LOW or a
    HIGH pass by the row above it — the highpass is the same circuit with its stages swapped — and
    BAND stays the SVF's, dimming when a circuit is chosen. The bench measures SCREAM and LADDER
    singing at the cutoff within a few cents at full resonance, and all three keep their aliasing
    under &minus;67 dB below the oscillation edge without any oversampling.</p>
    <p>A state-variable filter with <b>LOW</b>, <b>BAND</b>, <b>HIGH</b> and <b>OFF</b>. CUTOFF is`);
  rep(String.raw`      <tr><td>FILTER</td><td>LOW, BAND, HIGH, OFF</td></tr>`,
      String.raw`      <tr><td>FILTER</td><td>LOW, BAND, HIGH, OFF</td></tr>
      <tr><td>CIRCUIT</td><td>SVF, or Black Rider's GROWL, SCREAM and LADDER — each low or high by the row above</td></tr>`);
  rep(String.raw`        lungs; the vertebra has a canal of CSF between two pedicles of bone</td></tr>`,
      String.raw`        lungs; the vertebra has a canal of CSF between two pedicles of bone</td></tr>
      <tr><td>The circuits</td><td>each lowpass at 500 Hz pulls a bright SPINE's centroid from
        1308 Hz to under 310, each highpass at 2 kHz pushes it past 3400; SCREAM and LADDER at full
        resonance sing at a 1 kHz cutoff within a few cents; below the oscillation edge the
        non-harmonic floor at C5 is &minus;67 to &minus;69 dB at 1× — no oversampling needed; the
        SVF at CIRCUIT's default is bit-identical to the filter as it shipped; 32 ladders cost
        19.7 % of a core</td></tr>`);
  rep(String.raw`bench that renders real audio and asserts on it — 118 checks — and the panel has a second one
    that loads the real page and drives it through the plug-in's own message vocabulary — 43 checks.`,
      String.raw`bench that renders real audio and asserts on it — 126 checks — and the panel has a second one
    that loads the real page and drives it through the plug-in's own message vocabulary — 48 checks.`);
  rep(String.raw`<li>Thirty-nine parameters are automatable, plus five rack macros.`, String.raw`<li>Forty parameters are automatable, plus five rack macros.`);
  rep(String.raw`build 260905.1 &middot;`, String.raw`build 260905.2 &middot;`);
});

const landing = edit(web + "/index.html", (rep) => {
  rep(String.raw`<span class="buildtag">Build 260905.1</span>`, String.raw`<span class="buildtag">Build 260905.2</span>`);
  rep(String.raw`<tr><td>Build</td><td>260905.1 — shown on the loading screen and in the About box</td></tr>`,
      String.raw`<tr><td>Build</td><td>260905.2 — shown on the loading screen and in the About box</td></tr>`);
  rep(String.raw`      dragged on the gantry too — with shift, along the line of sight.</p>`,
      String.raw`      dragged on the gantry too — with shift, along the line of sight.</p>
    <p><b>And the filter has circuits.</b> Beside the state-variable filter sit Black Rider's three,
      ported whole: <b>GROWL</b>, a Sallen-Key with a diode clipper in its feedback that screams only at
      the top of the dial; <b>SCREAM</b>, the same circuit pushed harder, self-oscillating in tune from
      two o'clock so KEY TRACK plays it; <b>LADDER</b>, the four-pole transistor ladder. Each is a
      lowpass or a highpass. The bench hears SCREAM and LADDER sing at the cutoff within a few cents,
      and the SVF at its default is bit-identical to the filter that shipped.</p>`);
  rep(String.raw`<p class="sub">Two benches, 161 checks, both in the source.</p>`, String.raw`<p class="sub">Two benches, 174 checks, both in the source.</p>`);
  rep(String.raw`      <tr><td>The bodies</td><td>tissue fractions per body; five rays from the centre of the head all cross bone; SKULL's cranium is empty and CORTEX's holds white matter; the chest has two lungs; the vertebra a canal of CSF between two pedicles</td></tr>`,
      String.raw`      <tr><td>The bodies</td><td>tissue fractions per body; five rays from the centre of the head all cross bone; SKULL's cranium is empty and CORTEX's holds white matter; the chest has two lungs; the vertebra a canal of CSF between two pedicles</td></tr>
      <tr><td>The circuits</td><td>each lowpass at 500 Hz pulls a bright SPINE's centroid from 1308 Hz to under 310 and each highpass at 2 kHz pushes it past 3400; SCREAM and LADDER at full resonance sing at a 1 kHz cutoff within a few cents; non-harmonic floor at C5 below the oscillation edge &minus;67 to &minus;69 dB at 1×</td></tr>`);
  rep(String.raw`<tr><td>Automation</td><td>39 parameters plus 5 rack macros.`, String.raw`<tr><td>Automation</td><td>40 parameters plus 5 rack macros.`);
});

const app = edit(web + "/app.json", (rep) => {
  rep(String.raw`and dragged on the gantry - all anti-aliased. The Brokild World FX rack and five macros. Every claim measured: two benches, 161 checks."`,
      String.raw`and dragged on the gantry - all anti-aliased. The filter has circuits: beside the state-variable filter, Black Rider's GROWL, SCREAM and LADDER, each as lowpass or highpass, SCREAM and LADDER singing in tune at full resonance. The Brokild World FX rack and five macros. Every claim measured: two benches, 174 checks."`);
  rep(String.raw`"note": "Windows VST3 - build 260905.1 - free download`, String.raw`"note": "Windows VST3 - build 260905.2 - free download`);
});

const design = edit("c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BRAIN-SCAN-DESIGN.md", (rep, app) => {
  app(String.raw`
## 14. The circuits — 260905.2

Peter: *"can I have the same three filter types as Black Rider, with both low
pass and high pass? Please be careful with the panel design."*

Black Rider's ` + c("Korg35") + String.raw` (Sallen-Key, diode clipper in the feedback, zero-delay
loop refined twice) already had a lowpass and a highpass; its ` + c("Ladder") + String.raw` had a
lowpass only. The ladder highpass is the same loop with highpass stages,
h = (1−G)(u − s), solved the same way: h4 = ((1−G)⁴x − S)/(1 + k(1−G)⁴), then
the tanh at the summing node and the stages run. Both structs now live per
reader in ` + c("Uni") + String.raw`; the model's K comes from the same RESONANCE knob by Black
Rider's own laws (GROWL 2.25·r^0.9, SCREAM crossing K = 2 at 0.62, LADDER
4·1.12·r^0.85) and the Sallen-Key prewarp carries Black Rider's piecewise
tuning lift for the clipper's drag. A new ` + c("fmodel") + String.raw` parameter (CIRCUIT: SVF /
GROWL / SCREAM / LADDER), default SVF — every existing patch is untouched, and
the bench memcmp-proves it. BAND is the SVF's; the panel dims it off the SVF.

**Measured at 1×, no oversampling.** Each lowpass at 500 Hz pulls a bright
SPINE's centroid from 1308 Hz to 259–304; each highpass at 2 kHz pushes it to
3414–3723. SCREAM and LADDER at full resonance sing at a 1 kHz cutoff within a
few cents (the first run read −150 cents on both — KEY TRACK at its default 50 %
had pulled the cutoff to 917 Hz at A3, exactly what the ladder sang; the probe
measured the note, not the circuit). Non-harmonic floor at C5 below the
oscillation edge: −69.2 / −67.1 / −67.0 dB, so the circuits do not need the
oversampling Black Rider runs at. Six circuit/response pairs driven with
CONTRAST 1 and FOLD into RESONANCE 1 stay bounded at the ceiling. 32 ladders
cost 19.7 % of a core. The panel: one segmented row in the FILTER module,
and the probe asserts the module still fits its column (698 of 698 px, no
scroll). Bench 126, panel probe 48.
`);
});

const claude = edit("c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md", (rep) => {
  const anchor = "- 2026-09-04 **real scans, verified live.**";
  rep(anchor, String.raw`- 2026-09-05 **260905.2 — Black Rider's three circuits in Brain Scan** (Peter: "the same three filter types as Black Rider, with both low pass and high pass… be careful with the panel design… let me know if it is not gonna work"). It worked: ` + c("Korg35") + String.raw` already had LP + HP; the ladder HP is the same ZDF loop with highpass stages, h4 = ((1−G)⁴x − S)/(1 + k(1−G)⁴). Per reader in ` + c("Uni") + String.raw`, K by Black Rider's laws from the same RESONANCE knob, the Sallen-Key prewarp with BR's piecewise clipper lift. New ` + c("fmodel") + String.raw` (CIRCUIT) defaults SVF — memcmp-identical to the shipped filter; BAND stays the SVF's and dims off it. **At 1× the circuits alias at −67 dB below the oscillation edge** — no oversampling needed here (BR runs 2–4× for its VCOs, not for these). **The tuning probe measured the note, not the circuit**: KEY TRACK defaults 50 % and pulled a 1 kHz cutoff to 917 Hz at A3 — exactly the −150 cents both models "sang" flat. Set every parameter a probe does not mean to test. Panel: one seg row in FILTER; the probe asserts the module still fits its column. Bench 126 / probe 48.
` + anchor);
});

const buglist = edit("C:/Users/peter/b/BrainScan/BUGLIST.md", (rep, app) => {
  app(String.raw`
## 2026-09-05 · 260905.2 — the circuits

- **Shipped**: CIRCUIT (SVF / GROWL / SCREAM / LADDER), Black Rider's Sallen-Key and ladder per reader, each as LOW or HIGH; BAND stays the SVF's. SCREAM and LADDER sing in tune at full resonance; no oversampling needed at the measured −67 dB floor.
- **Parked**: a resonant highpass in series the way Black Rider has it (a second cutoff — a new line destination, arguably the FILTER B anchor's job); KEY TRACK as a per-circuit switch; the K laws as a MOD destination.
`);
});

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [manual, landing, app, design, claude, buglist]) fs.writeFileSync(f.file, f.get(), "utf8");
console.log("docs patched for 260905.2");
