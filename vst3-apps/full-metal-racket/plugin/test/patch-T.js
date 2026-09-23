// An ABOUT modal, and a bench check that RUN works with no host at all.
//
// About is a MODAL, never a splash — and the trap from Black Rider is worth
// repeating because it shipped: the overlay carried an inline display:flex for
// centring, and an inline style beats the `hidden` attribute, so it showed at
// boot and darkened a whole batch of manual plates before anyone noticed.
// Toggle style.display directly and never rely on `hidden`.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

// ── the bench: RUN with no host transport whatsoever ───────────────────────
const wt = edit(R + "test/test.cpp", [
["    //  the sequencer must be inert when it is switched off",
`    /*  RUN must start it with NO host transport at all — no playhead, no
        tempo, nothing ever reported. That is the standalone, and it is also a
        DAW whose transport has not rolled since the plugin loaded. */
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_TEMPO] = 0.5f;                    // the internal clock
        //  setTransport is deliberately never called
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        std::printf ("    no host at all: %d onsets on the internal clock\\n", (int) on.size());
        ok (on.size() >= 6, "RUN starts the sequencer with no host transport", (double) on.size(), 6);
    }

    //  the sequencer must be inert when it is switched off`, "free run"]
]);

// ── the About modal ────────────────────────────────────────────────────────
const wu = edit(R + "Source/ui/ui.html", [
["/* ── settings: how the machine looks ─",
`/* ── about ──────────────────────────────────────────────────────────────── */
/*  Positioned and shown by toggling style.display. NOT by the hidden
    attribute: this element needs an inline display for centring, and an
    inline style beats [hidden] — which is how Black Rider's About once
    appeared at boot and darkened a batch of manual plates. */
#about{position:fixed;inset:0;z-index:90;display:none;align-items:center;justify-content:center;
  background:#000000b0}
#aboutbox{width:600px;max-height:86vh;overflow-y:auto;border-radius:5px;
  background:linear-gradient(180deg,#f2ead9,#e7dcc3 30%,#d3c6a6);color:var(--ink);
  border:1px solid #9a8a68;box-shadow:0 30px 80px -18px #000,inset 0 1px 0 #fff}
#aboutbox .ah{padding:20px 24px 16px;border-bottom:1px solid #b6a887;
  background:linear-gradient(168deg,#d6d9db,#8b8f92 52%,#b7bbbe)}
#aboutbox .ah b{display:block;font:700 26px/1 var(--f-disp);letter-spacing:.015em;
  text-transform:uppercase;color:#191d22}
#aboutbox .ah i{display:block;font:700 8.5px/1 var(--f-lab);letter-spacing:.19em;
  color:#4d5359;font-style:normal;margin-top:6px}
#aboutbox .ab{padding:18px 24px 22px}
#aboutbox p{margin:0 0 11px;font:400 13.5px/1.62 var(--f-lab);color:#3a3122}
#aboutbox p b{color:var(--ink)}
#aboutbox .row{display:flex;gap:10px;font:700 9px/1.5 var(--f-lab);letter-spacing:.13em;
  text-transform:uppercase;color:#6b5c42;border-top:1px solid #c2b492;padding-top:12px;margin-top:14px;
  flex-wrap:wrap}
#aboutbox .row span{background:#00000010;border:1px solid #b6a887;border-radius:2px;padding:4px 8px}
#aboutbox .af{display:flex;justify-content:flex-end;padding:0 24px 20px}
#aboutbox .af .k{height:34px;padding:0 20px}

/* ── settings: how the machine looks ─`, "about css"],

["<div id=\"setwin\">",
`<div id="about"><div id="aboutbox">
  <div class="ah"><b>Full Metal Racket</b><i>Polyrhythmic Beat Composer</i></div>
  <div class="ab">
    <p><b>Twelve analogue drum voices, and they are one instrument.</b> They ring
      each other through THE WEB, they share a power rail that dips in level
      <em>and in pitch</em> when something hits it hard, they sit in one shell,
      and they age together. That is the whole design idea.</p>
    <p>Every voice is built from the same six mechanisms rather than being its own
      circuit: a resonator whose damping grows with amplitude, tension that makes a
      hard hit start sharp and fall to pitch, coupled inharmonic modes, a
      six-oscillator metal core, bands that decay at different rates, and a shaped
      excitation pulse — because the trigger <em>is</em> the attack.</p>
    <p>The sequencer is derived from the host's bar position rather than counted, so
      looping and tempo changes take care of themselves. Each lane keeps its own
      last step, division and direction, which is what the name is about.</p>
    <p><b>Two hundred kits</b> are generated from their seed: the category is decided
      first and the parameters are made to fit it, so a kit's name always describes
      its sound. A kit is reproducible anywhere from one integer.</p>
    <p style="margin-top:15px;color:#6b5c42;font-size:12.5px">Brokild &middot; part of the
      Brokild World FX family, so the rack behind the globe is the same rack in every
      Brokild instrument. Panel decals generated to specification and composited from
      measured crop regions.</p>
    <div class="row">
      <span id="ab-build">Build</span><span>12 channels</span><span>200 kits</span>
      <span>1609 bench checks</span><span id="ab-decals">decals</span>
    </div>
  </div>
  <div class="af"><button class="k dark" id="aboutclose">Close</button></div>
</div></div>
<div id="setwin">`, "about markup"],

// wiring
["$(\"#setclose\").addEventListener(\"click\", closeSettings);",
`function openAbout() {
  $("#ab-build").textContent = $("#buildid").textContent;
  $("#ab-decals").textContent = (window.__decals ? window.__decals() : 0) + " panel decals";
  //  style.display, not the hidden attribute — see the note in the CSS
  $("#about").style.display = "flex";
}
function closeAbout() { $("#about").style.display = "none"; }
$("#aboutclose").addEventListener("click", closeAbout);
$("#about").addEventListener("click", e => { if (e.target === $("#about")) closeAbout(); });
$("#plate").addEventListener("click", openAbout);
$("#plate").style.cursor = "pointer";
$("#plate").title = "About Full Metal Racket";

$("#setclose").addEventListener("click", closeSettings);`, "about js"],

// an About row in the settings window too, since that is where people look
["  <div class=\"sect\">\n    <div class=\"why\" id=\"setnote\">Saved on this machine",
`  <div class="sect">
    <div class="opts"><button class="k dark" id="setabout">About this machine</button></div>
  </div>
  <div class="sect">
    <div class="why" id="setnote">Saved on this machine`, "about in settings"],

["$(\"#fin-black\").addEventListener(\"click\",",
`$("#setabout").addEventListener("click", () => { closeSettings(); openAbout(); });
$("#fin-black").addEventListener("click",`, "setabout wiring"],

// escape closes it too
["window.addEventListener(\"keydown\", e => { if (e.key === \"Escape\") { closeMenu(); closeSettings(); } });",
 "window.addEventListener(\"keydown\", e => { if (e.key === \"Escape\") { closeMenu(); closeSettings(); closeAbout(); } });", "escape"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wt(); wu();
console.log("About modal, and a bench check for RUN with no host");
