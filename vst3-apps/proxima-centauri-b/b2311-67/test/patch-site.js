/*  Wire THE SITE (proxima_site.h) into B2311.67 — processor, engine, panel,
    bench, README, build id. Exact-count anchors, every miss collected, nothing
    written unless every edit matched (the house rule: a patch that applies
    eight of twelve edits leaves a file that is neither old nor new).
    Line endings detected per file.  Run:  node test/patch-site.js  */
const fs = require('fs');
const path = require('path');
const root = path.resolve(__dirname, '..');
const misses = [];
const files = {};

function load(rel) {
  const p = path.join(root, rel);
  const raw = fs.readFileSync(p, 'utf8');
  const crlf = raw.indexOf('\r\n') >= 0;
  files[rel] = { p, text: crlf ? raw.split('\r\n').join('\n') : raw, crlf };
}
function rep(rel, oldS, newS, count) {
  const f = files[rel];
  const n = f.text.split(oldS).length - 1;
  if (n !== (count || 1)) { misses.push(rel + ': expected ' + (count || 1) + ' match, found ' + n + ' for: ' + oldS.slice(0, 60).replace(/\n/g, '\\n')); return; }
  f.text = f.text.split(oldS).join(newS);
}

['Source/PluginProcessor.h', 'Source/PluginProcessor.cpp', 'Source/Engine.h', 'Source/Engine.cpp',
 'CMakeLists.txt', 'test/bench.cpp', 'dist/README.txt', 'Source/ui/ui.html'].forEach(load);

// ---------------------------------------------------------------- processor.h
rep('Source/PluginProcessor.h',
  '#include "Modulation.h"\n#include "bwfx.h"',
  '#include "Modulation.h"\n#include "bwfx.h"\n#include "proxima_site.h"');

rep('Source/PluginProcessor.h',
`    std::atomic<bool> wantPanic { false };
    std::array<std::atomic<bool>, 128> midiHeld;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtefactAudioProcessor)`,
`    std::atomic<bool> wantPanic { false };
    std::array<std::atomic<bool>, 128> midiHeld;

    /*  THE SITE — what the four findings share (proxima_site.h). Stepped
        from the processor's own timer so it runs with the editor closed; the
        engine is handed three plain numbers, and at pull zero the rock it
        makes of them is exactly 0.0. The climate travels through TEMPERATURE,
        which is a host parameter, so a shared move is visible, automatable
        and undoable in any DAW. */
    void siteStep();
    void emitSite();
    proxima::Client site;
    double sitePhase = 0.0, siteLastMs = 0.0;
    float  siteAppliedK = -1.0f, siteLastLocalK = -1.0f;
    float  siteCoh = 0.0f, sitePull = 0.0f, siteK = 0.0f, siteLocalK = 0.0f;
    int    siteOthers = 0, siteTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtefactAudioProcessor)`);

// -------------------------------------------------------------- processor.cpp
rep('Source/PluginProcessor.cpp',
`    applyHabitIndex (0);
    startTimerHz (30);
    bwfxRack.setWorldModConsumed (false);
}

ArtefactAudioProcessor::~ArtefactAudioProcessor() = default;`,
`    applyHabitIndex (0);
    startTimerHz (30);
    bwfxRack.setWorldModConsumed (false);
    site.open (67);                          // join the bench (harmless if it fails)
}

ArtefactAudioProcessor::~ArtefactAudioProcessor() { site.close(); }

//==============================================================================
//  TEMPERATURE is linear, 77 K at 0 and 800 K at 1 (Modulation.h)
static float kelvinOfTemp (float v)  { return ax::TEMP_MIN + (ax::TEMP_MAX - ax::TEMP_MIN) * juce::jlimit (0.0f, 1.0f, v); }
static float tempOfKelvin (float k)  { return juce::jlimit (0.0f, 1.0f, (k - ax::TEMP_MIN) / (ax::TEMP_MAX - ax::TEMP_MIN)); }

void ArtefactAudioProcessor::siteStep()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    double dt = siteLastMs > 0.0 ? (nowMs - siteLastMs) * 0.001 : 1.0 / 30.0;
    siteLastMs = nowMs;
    dt = juce::jlimit (0.0, 0.25, dt);

    const float tv     = raw[(size_t) tempIndex]->load();
    const float warmth = juce::jlimit (0.0f, 1.0f, tv);
    const float myK    = kelvinOfTemp (tv);
    siteLocalK = myK;

    //  an offline render must be deterministic: the bench is not consulted
    if (isNonRealtime() || ! site.isOpen())
    {
        sitePull = 0.0f;
        engine.setSite ((float) sitePhase, 0.5f, 0.0f);
        return;
    }
    if (siteLastLocalK < 0.0f) siteLastLocalK = myK;
    ++siteTick;
    //  SETTLING: no proposals for the first second (the host is still
    //  restoring state, and a restored value is not a player's move); while
    //  settling, the bench's climate overrides whatever the restore brings in
    const bool settled = siteTick > 25;

    const float activity = juce::jlimit (0.0f, 1.0f, engine.outLevel.load() * 2.0f);
    const auto v = site.sync (activity, myK, (float) sitePhase, warmth);

    //  the climate: my move is proposed; a move by another is followed
    //  through the host parameter (visible, automatable, undoable)
    if (v.climateShared)
    {
        const bool iMoved = std::abs (myK - siteLastLocalK) > 0.5f
                         && std::abs (myK - siteAppliedK) > 0.5f;
        const bool follow = v.climateMoved || ! settled;
        //  SEED: a bench nobody has spoken to has no temperature, and nothing
        //  would converge until someone moved a slider — which reads as the
        //  sharing being broken. The first settled finding gives it its own.
        if (v.siteKelvin <= 0.0f && settled)
        {
            site.proposeKelvin (myK);
            siteAppliedK = myK; siteLastLocalK = myK;
        }
        else if (iMoved && settled)
        {
            site.proposeKelvin (myK);
            siteAppliedK = myK;
        }
        else if (follow && v.siteKelvin > 0.0f && std::abs (v.siteKelvin - myK) > 0.5f)
        {
            siteAppliedK = v.siteKelvin;
            siteLastLocalK = v.siteKelvin;
            setParamById ("temp", tempOfKelvin (v.siteKelvin));   // echoed to the page
        }
    }
    if (std::abs (myK - siteLastLocalK) > 0.5f) siteLastLocalK = myK;

    //  the timing: Kuramoto, cold and close
    const float pull = proxima::Client::pullStrength (v, warmth);
    sitePhase = proxima::Client::stepPhase ((float) sitePhase, site.naturalHz(), dt, v, pull);
    engine.setSite ((float) sitePhase, v.pulseHz, pull);

    siteCoh = v.coherence; siteOthers = v.others; sitePull = pull;
    siteK = (v.climateShared && v.siteKelvin > 0.0f) ? v.siteKelvin : myK;
    if (siteTick % 8 == 0) emitSite();
}

void ArtefactAudioProcessor::emitSite()
{
    if (! emitToUi || ! uiReady.load()) return;
    const auto s = site.settings();
    auto* o = new juce::DynamicObject();
    o->setProperty ("open",     site.isOpen());
    o->setProperty ("climate",  s.climate);
    o->setProperty ("timing",   s.timing);
    o->setProperty ("distance", s.distance);
    o->setProperty ("others",   siteOthers);
    o->setProperty ("coh",      siteCoh);
    o->setProperty ("pull",     sitePull);
    o->setProperty ("kelvin",   siteK);
    o->setProperty ("local",    siteLocalK);
    o->setProperty ("phase",    (float) sitePhase);
    emitToUi ("site", juce::var (o));
}`);

rep('Source/PluginProcessor.cpp',
`void ArtefactAudioProcessor::timerCallback()
{
    refreshParams (0.0);
    engine.service();
    bwfxRack.service();
}`,
`void ArtefactAudioProcessor::timerCallback()
{
    refreshParams (0.0);
    siteStep();                 // before service(): the rock lands in this tick's rebuild
    engine.service();
    bwfxRack.service();
}`);

rep('Source/PluginProcessor.cpp',
`    else if (k == "ack")    uiHasState = true;
    else if (k == "ready")  uiReady = true;
    else if (k == "hello")  uiHasState = false;`,
`    else if (k == "ack")    uiHasState = true;
    else if (k == "ready")  { uiReady = true; emitSite(); }
    else if (k == "hello")  uiHasState = false;
    else if (k == "site")
    {
        //  the SITE panel: settings are the bench's — every finding sees the change
        proxima::Settings s = site.settings();
        const bool wasClimate = s.climate;
        if (o->hasProperty ("climate"))  s.climate  = (bool) o->getProperty ("climate");
        if (o->hasProperty ("timing"))   s.timing   = (bool) o->getProperty ("timing");
        if (o->hasProperty ("distance")) s.distance = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("distance"));
        site.setSettings (s);
        if (s.climate && ! wasClimate && site.isOpen())
        {
            //  CHOSEN HERE, GLOBAL NOW: the finding that turns the climate on
            //  gives the bench its temperature at once
            site.proposeKelvin (siteLocalK);
            siteAppliedK = siteLocalK; siteLastLocalK = siteLocalK;
        }
        emitSite();
    }`);

// ------------------------------------------------------------------ engine.h
rep('Source/Engine.h',
`    double travelDepth() const { return tau; }
    void   setTravelDelta (double d) { tauUi.store (tauUi.load() + d); }
    void   setTravel (double t)      { tauUi.store (t); }`,
`    double travelDepth() const { return tau; }
    void   setTravelDelta (double d) { tauUi.store (tauUi.load() + d); }
    void   setTravel (double t)      { tauUi.store (t); }

    /*  THE SITE (proxima_site.h). The findings share a bench; cooled and
        close together they fall into step. This lattice has one motion that
        is its own — the cut sliding through the fourth dimension, the tiling
        reorganising by phason flips as it goes — so that is what leans: the
        section ROCKS through w in the bench's time, \`pull\` deep. At pull 0
        the rock is exactly 0.0 and the depth is what it always was; the
        bench checks that by memcmp. */
    void   setSite (float phase, float hz, float pull)
    {
        sitePhaseIn.store (phase); siteHz.store (hz); sitePull.store (pull);
    }
    double siteRock() const { return siteTau; }`);

rep('Source/Engine.h',
`    std::atomic<double> tauUi { 0.0 };      // the wheel's contribution`,
`    std::atomic<double> tauUi { 0.0 };      // the wheel's contribution
    std::atomic<float>  sitePhaseIn { 0.0f }, siteHz { 0.5f }, sitePull { 0.0f };
    double siteTau = 0.0;                   // the bench's rock on the depth (message thread)`);

// ---------------------------------------------------------------- engine.cpp
rep('Source/Engine.cpp',
`    tau = ((double) p.travel - 0.5) * 24.0 + tauUi.load();`,
`    /*  THE SITE: cold and close, the section rocks through w in the bench's
        time — a quarter of a unit deep at full pull, which is enough to set
        the rim flipping and the star's shimmer breathing, and nothing like a
        traversal (the neighbours in the field are fourteen units away). The
        negotiated phase arrives from the processor every tick; at pull 0 the
        rock is exactly 0.0 and this line is the line it always was. */
    {
        const float sp = sitePull.load();
        siteTau = sp > 0.0f ? (double) sp * 0.25 * std::sin (6.283185307 * (double) sitePhaseIn.load()) : 0.0;
    }
    tau = ((double) p.travel - 0.5) * 24.0 + tauUi.load() + siteTau;`);

// ------------------------------------------------------------------ build id
rep('CMakeLists.txt', 'set(AB_BUILD_ID "260902.2")', 'set(AB_BUILD_ID "260904.1")');

// --------------------------------------------------------------------- bench
rep('test/bench.cpp',
`    std::printf ("\\n%d checks, %d failed  —  %s\\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");
    return fails == 0 ? 0 : 1;
}`,
`    //--------------------------------------------------------------------
    head ("11 · the site");
    {
        /*  THE SITE (proxima_site.h). Uncoupled, the engine must be BYTE-
            identical to one that never heard of it; coupled, cold and close,
            the section must rock through w in the bench's time, and the rock
            must be audible — measured, not assumed. The bench plays the site
            itself: a 0.5 Hz phase handed in every service tick, as the
            processor's timer would. */
        struct SiteTake { std::vector<float> L; double tauMin = 1e9, tauMax = -1e9; };
        auto take = [] (float pull, bool everCall) -> SiteTake
        {
            ax::Engine e; e.prepare (48000.0, 256);
            ax::applyHabit (77, e.p);
            e.p.temp = 0.10f;                       // cold: nothing else stirs
            e.service();
            e.noteOn (50, 0.8f); e.noteOn (57, 0.6f);
            std::vector<float> L (256), R (256);
            SiteTake t;
            const int blocks = (int) (12.0 * 48000.0 / 256.0);
            for (int b = 0; b < blocks; ++b)
            {
                const double tm = (double) b * 256.0 / 48000.0;
                if (everCall) e.setSite ((float) std::fmod (tm * 0.5, 1.0), 0.5f, pull);
                if ((b % 6) == 0) e.service();              // ~30 Hz, as the timer does
                e.process (L.data(), R.data(), 256);
                for (int i = 0; i < 256; ++i) t.L.push_back (L[(size_t) i]);
                t.tauMin = std::min (t.tauMin, e.travelDepth());
                t.tauMax = std::max (t.tauMax, e.travelDepth());
            }
            return t;
        };
        const SiteTake never = take (0.0f, false), off = take (0.0f, true), on = take (1.0f, true);
        const bool same = never.L.size() == off.L.size()
                       && std::memcmp (never.L.data(), off.L.data(), never.L.size() * sizeof (float)) == 0;
        std::printf ("  uncoupled: %s; depth swing %.4f\\n", same ? "byte-identical" : "DIFFERS", off.tauMax - off.tauMin);
        ok (same && off.tauMax - off.tauMin < 1e-12, "uncoupled, the site is an exact no-op");

        const double swing = on.tauMax - on.tauMin;
        std::printf ("  coupled at full pull: the section rocks %.3f deep in w (0.500 asked)\\n", swing);
        ok (swing > 0.45 && swing < 0.55, "cold and close, the section rocks through w with the bench");

        //  and it is HEARD: the output energy folded on the bench's period
        auto fold = [] (const std::vector<float>& L, double hz) -> double
        {
            double cx = 0, cy = 0, w = 0;
            const int hop = 256;
            for (size_t i = (size_t) (2.0 * 48000.0); i + hop < L.size(); i += hop)
            {
                double en = 0; for (int k = 0; k < hop; ++k) en += (double) L[i + k] * L[i + k];
                const double ph = std::fmod ((double) i / 48000.0 * hz, 1.0) * 2.0 * 3.14159265358979;
                cx += en * std::cos (ph); cy += en * std::sin (ph); w += en;
            }
            return w > 0 ? std::sqrt (cx * cx + cy * cy) / w : 0.0;
        };
        const double rFree = fold (off.L, 0.5), rLock = fold (on.L, 0.5);
        std::printf ("  the sound folded on the bench's 0.5 Hz: %.3f coupled vs %.3f free\\n", rLock, rFree);
        ok (rLock > 0.05 && rLock > rFree * 2.0, "the rock is heard: the output breathes with the bench");
    }

    std::printf ("\\n%d checks, %d failed  —  %s\\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");
    return fails == 0 ? 0 : 1;
}`);

// -------------------------------------------------------------------- README
rep('dist/README.txt',
`  room. Above five hundred kelvin the section itself will not hold still, and
  the object can no longer be said to be where it was put.

Requires Windows 10 or later, 64-bit, and a VST3 host.`,
`  room. Above five hundred kelvin the section itself will not hold still, and
  the object can no longer be said to be where it was put.

THE BENCH

  The four findings were recovered from one site, and on the bench they
  behave as if they still were. The SITE button on the frame opens the
  bench's settings, which every finding present shares: CLIMATE - one
  temperature for all of them - and TIMING - cooled and set close together,
  they fall into step; warm, or far apart, each keeps its own time. This
  lattice joins by rocking its section through the fourth dimension in the
  bench's time, the rim flipping as it goes. Both are off unless you turn
  them on, and off, each finding is exactly what it was.

Requires Windows 10 or later, 64-bit, and a VST3 host.`);

// ---------------------------------------------------------------------- panel
rep('Source/ui/ui.html',
`  #prov.on{display:block}`,
`  #prov.on{display:block}

  /* the site: what the four findings share. Shared stationery with .22. */
  #site{position:absolute;inset:0;z-index:9;display:none;background:rgba(3,5,9,.90);backdrop-filter:blur(4px);cursor:default}
  #site.on{display:block}
  #sitecard{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);width:min(600px,92vw);
    padding:22px 26px;color:#93a9ad;font-size:12px;line-height:1.75;background:rgba(9,14,19,.96);
    border:1px solid #2a3135;border-radius:3px;box-shadow:0 18px 60px rgba(0,0,0,.85)}
  #sitecard h2{font-size:11px;letter-spacing:.24em;color:#4fd6c4;margin:0 0 8px;font-weight:600;text-transform:uppercase}
  #sitecard p{color:#6f8589;margin:0 0 12px}
  #sitecard .row{display:flex;gap:14px;align-items:center;margin:5px 0}
  #sitecard .row b{min-width:96px;color:#dcecee;font-weight:600;letter-spacing:.1em;font-size:10px;text-transform:uppercase}
  #sitecard .stog{cursor:pointer;color:#dcecee;border:1px solid #3a4a52;padding:0 9px;letter-spacing:.14em;font-size:10px;text-transform:uppercase}
  #sitecard .stog.on{background:#1d4a48;border-color:#4fd6c4;color:#fff}
  #sitecard .sitehint{color:#5c757a}
  #sitecard input[type=range]{width:170px;accent-color:#4fd6c4}
  #siteclose{position:absolute;right:14px;top:12px}
  #rSite{color:#4fd6c4}`);

rep('Source/ui/ui.html',
`      <span class="btn" id="bProv">provenance</span>`,
`      <span class="btn" id="bProv">provenance</span>
      <span class="btn" id="bSite" title="the site: what the four findings share">site</span>`);

rep('Source/ui/ui.html',
`    <div style="margin-top:3px">ceiling <b id="rGR">0.0 dB</b></div>`,
`    <div style="margin-top:3px">ceiling <b id="rGR">0.0 dB</b></div>
    <div style="margin-top:3px" id="rSite"></div>`);

rep('Source/ui/ui.html',
`<div id="prov">
  <div id="provcard">`,
`<div id="site">
  <div id="sitecard">
    <span class="btn" id="siteclose">close</span>
    <h2>The site &middot; what the findings share</h2>
    <p>Four findings, one bench. Cooled and set close together they fall into step; warm, or far apart, each keeps its own time. These are the bench's settings, not this finding's: change them here and every artefact present sees the change. Off, each finding is exactly what it was.</p>
    <div class="row"><b>climate</b><span class="stog" id="siteClimate">own</span><span class="sitehint">one temperature for the whole bench</span></div>
    <div class="row"><b>timing</b><span class="stog" id="siteTiming">own</span><span class="sitehint">cold and close, they fall into step</span></div>
    <div class="row"><b>distance</b><input type="range" id="siteDist" min="0" max="1" step="0.01" value="0.5"><span id="siteDistRd">the same room</span></div>
    <div class="row"><b>present</b><span id="siteStatus">only this finding</span></div>
  </div>
</div>

<div id="prov">
  <div id="provcard">`);

rep('Source/ui/ui.html',
`NB.on("notice", p => say(String(p.msg), "", ""));`,
`NB.on("notice", p => say(String(p.msg), "", ""));

/* ── the site: what the four findings share (settings are the bench's) ── */
const SITE = {open:false, climate:false, timing:false, distance:0.5, others:0, coh:0, pull:0, kelvin:0, local:0};
function siteDistWord(d){ return d<0.15?"the same bench":d<0.4?"the same table":d<0.7?"the same room":d<0.9?"across the hall":"different rooms"; }
function renderSite(){
  const c=$("#siteClimate"), t=$("#siteTiming");
  c.textContent = SITE.climate?"shared":"own"; c.classList.toggle("on", !!SITE.climate);
  t.textContent = SITE.timing?"shared":"own";  t.classList.toggle("on", !!SITE.timing);
  if(document.activeElement!==$("#siteDist")) $("#siteDist").value = SITE.distance;
  $("#siteDistRd").textContent = siteDistWord(SITE.distance);
  let st = !SITE.open ? "no bench (the site is unavailable here)"
         : SITE.others>0 ? SITE.others+" other finding"+(SITE.others>1?"s":"")+" present" : "only this finding";
  if(SITE.open && SITE.others>0 && SITE.timing)
    st += " · coherence "+(+SITE.coh).toFixed(2)+(SITE.pull>0.05 ? " · pulling "+Math.round(SITE.pull*100)+"%" : " · too warm or too far to lock");
  if(SITE.open && SITE.climate && SITE.kelvin>0)
    st += " · bench "+Math.round(SITE.kelvin)+" K"+(SITE.local>0 && Math.abs(SITE.local-SITE.kelvin)>1 ? " · here "+Math.round(SITE.local)+" K" : "");
  $("#siteStatus").textContent = st;
  $("#rSite").textContent = (SITE.open && SITE.others>0 && (SITE.timing||SITE.climate))
    ? "bench "+(SITE.others+1)+" present"+(SITE.timing?" · lock "+(+SITE.coh).toFixed(2):"") : "";
}
NB.on("site", p => { Object.assign(SITE, p); renderSite(); });
$("#bSite").addEventListener("click", () => $("#site").classList.add("on"));
$("#siteclose").addEventListener("click", () => $("#site").classList.remove("on"));
$("#site").addEventListener("click", ev => { if (ev.target.id === "site") $("#site").classList.remove("on"); });
window.addEventListener("keydown", ev => { if (ev.key === "Escape") $("#site").classList.remove("on"); });
$("#siteClimate").addEventListener("click", () => NB.send({k:"site", climate: !SITE.climate}));
$("#siteTiming").addEventListener("click",  () => NB.send({k:"site", timing:  !SITE.timing}));
$("#siteDist").addEventListener("input",  e => { SITE.distance=+e.target.value; $("#siteDistRd").textContent=siteDistWord(SITE.distance); });
$("#siteDist").addEventListener("change", e => NB.send({k:"site", distance:+e.target.value}));`);

// ----------------------------------------------------------------------------
if (misses.length) { console.error('NOT WRITTEN — misses:'); misses.forEach(m => console.error('  ' + m)); process.exit(1); }
for (const rel in files) {
  const f = files[rel];
  fs.writeFileSync(f.p, f.crlf ? f.text.split('\n').join('\r\n') : f.text, 'utf8');
  console.log('patched ' + rel);
}
