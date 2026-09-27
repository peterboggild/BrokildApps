/*  1984 — panel gate.  node test/uiprobe.js  [--keep]

    Copies Source/ui/ui.html to a temp folder with a stubbed JUCE bridge,
    feeds it an initialState built by PARSING Source/Engine.cpp (so this
    file hard-codes the parameter list no more than the page does), drives
    it in headless Chrome and reports.

    No npm dependencies. Chrome is found at the usual Windows locations.
*/
"use strict";

const DRIVER = String.raw`
(function () {
  var B = window.__JUCE__.backend;
  var R = [];
  function ok(n, c, d) { R.push({ n: n, ok: !!c, d: d === undefined ? "" : String(d) }); }
  function near(a, b, e) { return Math.abs(a - b) <= (e === undefined ? 1e-6 : e); }
  function tick() { return new Promise(function (r) { setTimeout(r, 0); }); }
  function msgs() {
    var out = [];
    B.__sent.forEach(function (s) {
      var p = s.payload;
      if (p && p.b) p.b.forEach(function (m) { out.push(m); }); else out.push(p);
    });
    return out;
  }
  function clr() { B.__sent.length = 0; }
  function ps() { return msgs().filter(function (m) { return m && m.k === "p"; }); }
  function feed(n, p) { (B.__ls[n] || []).forEach(function (f) { f(p); }); }

  var F = window.__FIX;
  var SPEC = {};
  F.params.forEach(function (p) { SPEC[p.id] = p; });

  /* ---- the formatting laws, written from PROTOCOL.md, not from the page */
  function xm(t, lo, hi) { return lo * Math.pow(hi / lo, Math.max(0, Math.min(1, t))); }
  function hz(x) { return x < 100 ? x.toFixed(2) + " Hz" : (x < 1000 ? Math.round(x) + " Hz" : (x / 1000).toFixed(2) + " kHz"); }
  function ms(x) { return x < 100 ? x.toFixed(1) + " ms" : (x < 1000 ? Math.round(x) + " ms" : (x / 1000).toFixed(2) + " s"); }
  function sg(n) { return (n > 0 ? "+" : "") + n; }
  function law(id, v) {
    var s = SPEC[id];
    switch (s.kind) {
      case 0: return Math.round(v * 100) + " %";
      case 1: return v >= 0.5 ? "ON" : "OFF";
      case 2: return hz(xm(v, s.lo, s.fhi));
      case 3: return ms(xm(v, s.lo, s.fhi));
      case 4: return s.names[Math.round(v)];
      case 5: return sg(Math.round((v - 0.5) * 24)) + " st";
      case 6: return sg(Math.round((v - 0.5) * s.lo)) + " c";
      case 7: return Math.round(v * s.lo) + " c";
      case 8: return sg(Math.round((v - 0.5) * 200)) + " %";
      case 9: return v < 0.01 ? "OFF" : ms(xm(v, s.lo, s.fhi));
      case 10: { var g = 2 * v * v; return (v < 0.005 || g <= 0) ? "-INF dB" : (20 * Math.log10(g)).toFixed(1) + " dB"; }
      case 11: return Math.round(50 + 45 * v) + " %";
      case 12: return String(Math.round(v));
      case 13: return xm(v, s.lo, s.fhi).toFixed(2) + " s";
    }
    return "?";
  }
  function expectSection(id) {
    if (id === "patch") return "header";
    if (id.slice(0, 2) === "a_") return "rank0";
    if (id.slice(0, 2) === "b_") return "rank1";
    if (/^(lfo_|ring_|pm_|wheel_|at_)/.test(id)) return "mod";
    if (/^(drv_|ens_|choir_|tape_|hall_)/.test(id)) return "chain";
    return "perf";
  }
  function pev(t, c, x, y, extra) {
    var o = { bubbles: true, cancelable: true, clientX: x, clientY: y, pointerId: 7, isPrimary: true, buttons: 1, button: 0 };
    if (extra) for (var k in extra) o[k] = extra[k];
    c.dispatchEvent(new PointerEvent(t, o));
  }
  function drag(c, dy, shift) {
    var r = c.getBoundingClientRect(), x = r.left + r.width / 2, y = r.top + r.height / 2;
    pev("pointerdown", c, x, y);
    for (var i = 1; i <= 3; i++) pev("pointermove", c, x, y - dy * i / 3, { shiftKey: !!shift, button: -1 });
    pev("pointerup", c, x, y - dy, { buttons: 0 });
  }

  function run() {
    /* ---- boot ------------------------------------------------------- */
    var boot = msgs();
    ok("hello is sent at boot", boot.length >= 1 && boot[0].k === "hello", JSON.stringify(boot[0] || null));
    ok("nothing else precedes hello", boot.filter(function (m) { return m.k === "hello"; }).length === 1);
    ok("page survives with no state", !!window.__N84 && window.__N84.ids().length === 0, "controls before initialState");

    clr();
    feed("initialState", F);
    return tick().then(function () {
      var m = msgs();
      ok("ack is sent after initialState", m.some(function (x) { return x.k === "ack"; }));
      return new Promise(function (r) { setTimeout(r, 200); });
    }).then(function () {
      ok("ready is sent once the page is painted", msgs().some(function (x) { return x.k === "ready"; }));
      ok("build id is shown", document.getElementById("build").textContent.indexOf(F.build) >= 0,
         document.getElementById("build").textContent);

      /* ---- one control per parameter, in the right place -------------- */
      var nodes = Array.prototype.slice.call(document.querySelectorAll("[data-id]"));
      var byId = {};
      nodes.forEach(function (n) { (byId[n.dataset.id] = byId[n.dataset.id] || []).push(n); });
      var missing = [], dup = [], wrong = [], nohint = [];
      F.params.forEach(function (p) {
        var g = byId[p.id] || [];
        if (g.length === 0) { missing.push(p.id); return; }
        if (g.length > 1) dup.push(p.id + " x" + g.length);
        var sec = g[0].closest("[data-section]");
        var got = sec ? sec.getAttribute("data-section") : "?";
        if (got !== expectSection(p.id)) wrong.push(p.id + ": " + got + " != " + expectSection(p.id));
        if (!g[0].getAttribute("data-hint")) nohint.push(p.id);
      });
      ok("every parameter has a control (" + F.params.length + ")", missing.length === 0, "missing: " + missing.join(" "));
      ok("no parameter has two controls", dup.length === 0, dup.join(" "));
      ok("every control is in its section", wrong.length === 0, wrong.slice(0, 6).join(" | "));
      ok("every control carries a hint", nohint.length === 0, nohint.slice(0, 6).join(" "));
      var extra = Object.keys(byId).filter(function (id) { return !SPEC[id]; });
      ok("no control for a parameter that does not exist", extra.length === 0, extra.join(" "));

      /* ---- the formatting laws, one per kind -------------------------- */
      var seen = {};
      F.params.forEach(function (p) {
        if (seen[p.kind] || !byId[p.id]) return;
        seen[p.kind] = 1;
        var c = byId[p.id][0], want = law(p.id, p.v);
        var got = c.querySelector(".val") ? c.querySelector(".val").textContent : "(none)";
        ok("kind " + p.kind + " formats (" + p.id + ")", got === want, got + " wanted " + want);
      });
      ok("all 14 kinds were exercised", Object.keys(seen).length === 14, Object.keys(seen).join(","));

      /* ---- sending ---------------------------------------------------- */
      var sid = "a_lpf", sc = byId[sid][0], v0 = window.__N84.get(sid);
      clr(); drag(sc, 55);
      return tick().then(function () {
        var p = ps();
        var want = Math.max(0, Math.min(1, v0 + 55 / 220));
        ok("a slider drag sends exactly one p", p.length === 1, JSON.stringify(p));
        ok("the slider drag sends the right value", p.length === 1 && p[0].id === sid && near(p[0].v, want, 1e-5),
           p.length ? p[0].v + " wanted " + want : "-");
        ok("the slider cap moved", parseFloat(sc.querySelector(".cap").style.bottom) > 0, sc.querySelector(".cap").style.bottom);

        var kid = "hall_size", kc = byId[kid][0], k0 = window.__N84.get(kid);
        clr(); drag(kc, -44);
        return tick().then(function () {
          var q = ps();
          ok("a knob drag sends exactly one p", q.length === 1 && q[0].id === kid, JSON.stringify(q));
          ok("the knob drag sends the right value", q.length === 1 && near(q[0].v, Math.max(0, k0 - 44 / 220), 1e-5),
             q.length ? q[0].v + " wanted " + Math.max(0, k0 - 44 / 220) : "-");

          var gc = byId["a_oct"][0];
          clr(); gc.querySelectorAll(".seg b")[4].dispatchEvent(new MouseEvent("click", { bubbles: true }));
          return tick().then(function () {
            var s = ps();
            ok("a segmented LIST sends one p with the index", s.length === 1 && s[0].id === "a_oct" && s[0].v === 4, JSON.stringify(s));
            var wc = byId["gliss"][0], w0 = window.__N84.get("gliss");
            clr();
            var r = wc.getBoundingClientRect();
            pev("pointerdown", wc, r.left + 10, r.top + 10);
            pev("pointerup", wc, r.left + 10, r.top + 10, { buttons: 0 });
            return tick().then(function () {
              var t = ps();
              ok("a switch toggles and sends one p", t.length === 1 && t[0].id === "gliss" && t[0].v === (w0 >= 0.5 ? 0 : 1), JSON.stringify(t));
              var rc = byId["drv_mode"][0];
              clr(); drag(rc, 200);
              return tick().then(function () {
                var u = ps();
                ok("a rotary LIST drags to an index", u.length === 1 && u[0].id === "drv_mode" && u[0].v === SPEC["drv_mode"].hi, JSON.stringify(u));
                clr();
                byId["tape_wow"][0].dispatchEvent(new WheelEvent("wheel", { bubbles: true, cancelable: true, deltaY: -1 }));
                return tick();
              }).then(function () {
                var w = ps();
                ok("the mouse wheel nudges a control", w.length === 1 && near(w[0].v, 0.3 + 0.02, 1e-6), JSON.stringify(w));
                clr();
                byId["tape_wow"][0].dispatchEvent(new MouseEvent("dblclick", { bubbles: true }));
                return tick();
              }).then(function () {
                var w = ps();
                ok("double-click resets to the default", w.length === 1 && near(w[0].v, SPEC["tape_wow"].def), JSON.stringify(w));
                clr(); window.__N84.set("choir_air", 0.77);
                return tick();
              }).then(function () {
                var w = ps();
                ok("__N84.set writes and sends", w.length === 1 && w[0].id === "choir_air" && near(w[0].v, 0.77), JSON.stringify(w));
                return phase2(byId);
              });
            });
          });
        });
      });
    });
  }

  /* ---- hostParam, meters, keyboard, layout --------------------------- */
  function phase2(byId) {
    /*  A control can be working and still look broken: every consumer of a
        value has to redraw, display as well as engine. */
    feed("hostParam", { p: [{ id: "a_lpf", v: 0.2 }] });
    var sc = byId["a_lpf"][0];
    ok("hostParam moves a rank slider", near(parseFloat(sc.querySelector(".cap").style.bottom), 0.2 * (128 - 22), 0.6),
       sc.querySelector(".cap").style.bottom);
    ok("hostParam moves its readout", sc.querySelector(".val").textContent === law("a_lpf", 0.2), sc.querySelector(".val").textContent);

    feed("hostParam", { p: [{ id: "ens_mode", v: 3 }] });
    var ec = byId["ens_mode"][0];
    ok("hostParam moves a LIST", ec.querySelector(".val").textContent === SPEC["ens_mode"].names[3], ec.querySelector(".val").textContent);
    ok("the LIST rotary turned", ec.querySelector(".knr").style.transform.indexOf("rotate") === 0, ec.querySelector(".knr").style.transform);

    var kc = byId["hall_size"][0];
    feed("hostParam", { p: [{ id: "hall_size", v: 0.9 }] });
    ok("hostParam turns a knob", kc.querySelector(".knr").style.transform === "rotate(112deg)", kc.querySelector(".knr").style.transform);
    ok("hostParam moves the knob readout", kc.querySelector(".val").textContent === "90 %", kc.querySelector(".val").textContent);

    var before = byId["ring_speed"][0].querySelector(".val").textContent;
    feed("hostParam", { p: [{ id: "ring_key", v: 1 }] });
    var sw = byId["ring_key"][0];
    ok("hostParam throws a switch", sw.classList.contains("on") && sw.querySelector(".val").textContent === "ON");
    var after = byId["ring_speed"][0].querySelector(".val").textContent;
    ok("a dependent readout follows (ring speed becomes a ratio)",
       after !== before && after.indexOf("×") > 0, before + " -> " + after);

    /* ---- meters ------------------------------------------------------- */
    var sc512 = []; for (var i = 0; i < 512; i++) sc512.push(Math.sin(i / 9) * 0.6);
    var d0 = JSON.parse(JSON.stringify(window.__N84.draws()));
    feed("meter", { out: 0.5, peak: 0.8, feg: [0.4, -0.2], aeg: [0.9, 0.3], cut: [1200, 800], lfo: 0.2,
                    wow: 0.3, drop: false, notes: [60, 64, 67, -1, -1, -1, -1, -1], levels: [1, .8, .6, 0, 0, 0, 0, 0],
                    held: [60, 64, 67], scope: sc512, bend: 0, wheel: 0.4, at: 0 });
    var v = window.__N84.vfd();
    ok("meter lights the voice lamps", v.lamps.join("") === "11100000", v.lamps.join(""));
    ok("the lamps name their notes", v.lnotes[0] === "C4" && v.lnotes[2] === "G4", v.lnotes.slice(0, 3).join(" "));
    ok("meter draws the envelopes and the scope",
       window.__N84.draws().env > d0.env && window.__N84.draws().scope > d0.scope);
    ok("meter reports the held notes", v.held.indexOf("C4") >= 0, v.held);
    ok("meter counts the voices", v.voices === "VOICES 3 / 8", v.voices);

    /* tape: the reels turn only when the machine is on */
    feed("hostParam", { p: [{ id: "tape_mode", v: 0 }] });
    var a0 = window.__N84.vfd().reel;
    feed("meter", { wow: 0, drop: false });
    feed("meter", { wow: 0, drop: false });
    ok("the reels stand still with the tape off", window.__N84.vfd().reel === a0, String(a0));
    feed("hostParam", { p: [{ id: "tape_mode", v: 1 }] });
    feed("meter", { wow: 0.5, drop: false });
    feed("meter", { wow: 0.5, drop: false });
    var a1 = window.__N84.vfd().reel;
    ok("the reels turn with the tape on", a1 > a0, a0 + " -> " + a1);
    ok("the reel element carries the rotation", byId["tape_wow"][0].closest("[data-section]").querySelector(".reel").style.transform.indexOf("rotate") === 0);
    feed("meter", { drop: true });
    ok("a drop-out flickers the window", window.__N84.vfd().drop === true);

    /* ---- patchinfo and notice ---------------------------------------- */
    feed("patchinfo", { i: 6, name: "VP STRINGS", cat: "STRINGS" });
    var v2 = window.__N84.vfd();
    ok("patchinfo shows the patch name", v2.name === "VP STRINGS", v2.name);
    ok("patchinfo shows the category", v2.cat === "STRINGS", v2.cat);
    ok("patchinfo moves the dial", document.getElementById("patchSel").value === "6", document.getElementById("patchSel").value);
    feed("notice", { msg: "probe was here" });
    ok("a notice reaches the display", window.__N84.vfd().notice === "PROBE WAS HERE", window.__N84.vfd().notice);

    /* ---- the patch dial ---------------------------------------------- */
    clr();
    var sel = document.getElementById("patchSel");
    sel.value = "9"; sel.dispatchEvent(new Event("change", { bubbles: true }));
    return tick().then(function () {
      var m = msgs();
      ok("the patch dial sends {k:patch}", m.length === 1 && m[0].k === "patch" && m[0].i === 9, JSON.stringify(m));
      clr(); document.getElementById("b-prev").dispatchEvent(new MouseEvent("click", { bubbles: true }));
      return tick();
    }).then(function () {
      var m = msgs();
      ok("PREV steps back a patch", m.length === 1 && m[0].k === "patch" && m[0].i === 8, JSON.stringify(m));
      clr();
      ["b-rand", "b-save", "b-open", "b-panic"].forEach(function (id) {
        document.getElementById(id).dispatchEvent(new MouseEvent("click", { bubbles: true }));
      });
      return tick();
    }).then(function () {
      var ks = msgs().map(function (m) { return m.k; });
      ok("RANDOM, SAVE, OPEN and PANIC send their messages",
         ks.indexOf("random") >= 0 && ks.indexOf("save") >= 0 && ks.indexOf("open") >= 0 && ks.indexOf("panic") >= 0, ks.join(","));

      /* ---- the keyboard ------------------------------------------------ */
      clr();
      var k = document.querySelector('.key[data-n="52"]');
      var r = k.getBoundingClientRect();
      pev("pointerdown", document.getElementById("kbwrap"), r.left + r.width / 2, r.top + r.height * 0.8);
      pev("pointerup", document.getElementById("kbwrap"), r.left + r.width / 2, r.top + r.height * 0.8, { buttons: 0 });
      return tick();
    }).then(function () {
      var m = msgs();
      var on = m.filter(function (x) { return x.k === "note" && x.on; });
      var off = m.filter(function (x) { return x.k === "note" && !x.on; });
      ok("a key sends note on with a velocity", on.length === 1 && on[0].n === 52 && on[0].v > 0.5 && on[0].v <= 1, JSON.stringify(on));
      ok("releasing a key sends note off", off.length === 1 && off[0].n === 52, JSON.stringify(off));
      clr();
      document.dispatchEvent(new KeyboardEvent("keydown", { key: "a", bubbles: true }));
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "a", bubbles: true }));
      return tick();
    }).then(function () {
      var m = msgs().filter(function (x) { return x.k === "note"; });
      ok("the computer keyboard plays", m.length === 1 && m[0].on === true && m[0].n === 48, JSON.stringify(m));
      clr();
      window.dispatchEvent(new KeyboardEvent("keyup", { key: "a", bubbles: true }));
      window.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true }));
      return tick();
    }).then(function () {
      ok("escape releases everything", msgs().some(function (m) { return m.k === "alloff"; }), JSON.stringify(msgs()));
      return phase3(byId);
    });
  }

  function phase3(byId) {
    /* ---- layout ------------------------------------------------------- */
    var deck = document.getElementById("deck");
    ok("the deck is 1600 by 980", deck.offsetWidth === 1600 && deck.offsetHeight === 980,
       deck.offsetWidth + "x" + deck.offsetHeight);
    ok("no sideways scroll", document.documentElement.scrollWidth <= document.documentElement.clientWidth + 1,
       document.documentElement.scrollWidth + " vs " + document.documentElement.clientWidth);
    ok("the deck holds the whole panel", deck.scrollWidth <= 1601 && deck.scrollHeight <= 981,
       deck.scrollWidth + "x" + deck.scrollHeight);
    var small = [], out = [];
    Array.prototype.forEach.call(document.querySelectorAll("[data-id]"), function (c) {
      var w = c.offsetWidth, h = c.offsetHeight;
      if (Math.min(w, h) < 22) small.push(c.dataset.id + " " + w + "x" + h);
      var r = c.getBoundingClientRect(), dr = deck.getBoundingClientRect();
      if (r.left < dr.left - 1 || r.right > dr.right + 1 || r.top < dr.top - 1 || r.bottom > dr.bottom + 1)
        out.push(c.dataset.id);
    });
    ok("every control is at least 22 px across", small.length === 0, small.slice(0, 6).join(" | "));
    ok("no control escapes the deck", out.length === 0, out.slice(0, 6).join(" "));
    var secs = ["header", "perf", "rank0", "rank1", "mod", "chain", "kbd"];
    var miss = secs.filter(function (s) { return !document.querySelector('[data-section="' + s + '"]'); });
    ok("all seven sections exist", miss.length === 0, miss.join(" "));

    /* ---- the choosers are switches, not HTML selects ------------------ */
    var vis = Array.prototype.filter.call(document.querySelectorAll("select"), function (s) { return s.offsetWidth > 0 || s.offsetHeight > 0; });
    ok("no HTML select is visible on the panel", vis.length === 0, vis.length + " visible");
    var sw = byId["a_oct"][0], kb = sw.querySelector(".sknob"), legs = sw.querySelectorAll(".seg b");
    ok("a slider-row LIST is a slide switch with a knob and a legend", !!kb && legs.length === 5, (kb ? "knob, " : "no knob, ") + legs.length + " legends");
    window.__N84.set("a_oct", 0); var kTop0 = parseFloat(kb.style.top);
    window.__N84.set("a_oct", 4); var kTop4 = parseFloat(kb.style.top);
    ok("the switch knob travels with the value", kTop4 > kTop0 + 40, kTop0 + " -> " + kTop4);
    ok("the knob sits on the lit legend and only that one is lit",
       Math.abs(kTop4 - parseFloat(legs[4].style.top)) < 0.5 && legs[4].classList.contains("on") && sw.querySelectorAll(".seg b.on").length === 1,
       kTop4 + " vs " + legs[4].style.top);
    var tight = [];
    Array.prototype.forEach.call(document.querySelectorAll(".ctl.seg"), function (c) {
      var g = c.querySelector(".seg");
      if (g.scrollWidth > g.clientWidth + 1 || g.scrollHeight > g.clientHeight + 1)
        tight.push(c.dataset.id + " " + g.scrollWidth + "/" + g.clientWidth + " " + g.scrollHeight + "/" + g.clientHeight);
    });
    ok("every legend fits its switch cell", tight.length === 0, tight.join(" | "));

    /* ---- the tooltip sits BESIDE its control -------------------------- */
    var tc = byId["a_fa"][0];
    tc.dispatchEvent(new MouseEvent("mouseenter", { bubbles: false }));
    var t = window.__N84.tip(), cr = tc.getBoundingClientRect();
    ok("hovering opens the hint", t.open && t.forId === "a_fa", t.forId);
    ok("the hint carries the written sentence", t.text.indexOf("filter envelope") > 0, t.text.slice(0, 60));
    ok("the hint is beside the control, not over it",
       t.rect.right <= cr.left + 1 || t.rect.left >= cr.right - 1 || t.rect.bottom <= cr.top + 1 || t.rect.top >= cr.bottom - 1,
       JSON.stringify([t.rect.left, t.rect.right, cr.left, cr.right]));

    /* ---- the rack button ---------------------------------------------- */
    var bw = document.querySelectorAll("[data-bwfx-open]");
    ok("exactly one BWFX button", bw.length === 1, String(bw.length));
    ok("the rack fragment is present and attached", !!window.BWFX && !!bw[0].querySelector("svg"),
       window.BWFX ? "attached" : "fragment missing");

    /* ---- the debug hook ------------------------------------------------ */
    var need = ["get", "set", "state", "notes", "meter", "version"];
    var have = need.filter(function (k) { return window.__N84[k] !== undefined; });
    ok("__N84 carries the documented hook", have.length === need.length, have.join(","));
    ok("__N84.state() returns the model", !!window.__N84.state().V && !!window.__N84.state().KIND);

    /* ---- an id this panel has no home for lands in MORE ---------------- */
    var m0 = document.getElementById("more").hidden;
    var f2 = JSON.parse(JSON.stringify(F));
    f2.params.push({ id: "zz_probe", v: 0.5, hi: 1, n: "PROBE EXTRA", kind: 0, def: 0.5, lo: 0, fhi: 0, rank: -1 });
    feed("initialState", f2);
    ok("MORE was hidden while it was empty", m0 === true);
    ok("an unknown parameter appears in MORE", window.__N84.more().indexOf("zz_probe") >= 0, window.__N84.more().join(" "));
    ok("the MORE strip is shown when it has something", document.getElementById("more").hidden === false);
    ok("a second initialState does not duplicate a control",
       document.querySelectorAll('[data-id="a_lpf"]').length === 1);

    /* ---- the patch window and the bank chart --------------------------- */
    var pw = document.getElementById("pwin");
    feed("patchinfo", { i: 8, name: "PROBE PAD", cat: "PADS" });
    ok("the patch window shows the number of the patch on the dial", document.getElementById("pwinnum").textContent === "09", document.getElementById("pwinnum").textContent);
    ok("the patch window names it", document.getElementById("pwintxt").textContent === (F.patches[8].name || ""), document.getElementById("pwintxt").textContent);
    pw.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    var pm = document.getElementById("pmenu");
    ok("clicking the window opens the bank chart", !!pm);
    var cats = {}; F.patches.forEach(function (q) { cats[q.cat || "PATCHES"] = 1; });
    ok("the chart lists every patch in its category column",
       !!pm && pm.querySelectorAll(".pi").length === F.patches.length && pm.querySelectorAll(".pc").length === Object.keys(cats).length,
       pm ? pm.querySelectorAll(".pi").length + " entries, " + pm.querySelectorAll(".pc").length + " columns" : "no chart");
    ok("the chart marks the patch on the dial", !!pm && pm.querySelectorAll(".pi.on").length === 1 && pm.querySelector(".pi.on").dataset.n === "8",
       pm ? String(pm.querySelectorAll(".pi.on").length) : "-");
    var pr = pm ? pm.getBoundingClientRect() : null;
    ok("the chart is on screen", !!pr && pr.left >= 0 && pr.top >= 0 && pr.right <= innerWidth && pr.bottom <= innerHeight,
       pr ? JSON.stringify([pr.left, pr.top, pr.right, pr.bottom]) : "-");
    document.body.dispatchEvent(new PointerEvent("pointerdown", { bubbles: true }));
    ok("a press outside closes the chart", !document.getElementById("pmenu"));
    pw.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    clr();
    var entry = document.querySelector('#pmenu .pi[data-n="3"]');
    if (entry) entry.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    return tick().then(function () {
      var m = msgs().filter(function (x) { return x && x.k === "patch"; });
      ok("a chart entry loads its patch and closes the chart",
         m.length === 1 && m[0].i === 3 && !document.getElementById("pmenu"),
         JSON.stringify(m) + (document.getElementById("pmenu") ? " still open" : ""));
      ok("the window followed the pick", document.getElementById("pwinnum").textContent === "04", document.getElementById("pwinnum").textContent);

      /* ---- and nothing threw ------------------------------------------- */
      ok("no JS errors", window.__N84ERR.length === 0, window.__N84ERR.join(" | "));
    });
  }

  function done() {
    var pre = document.createElement("pre");
    pre.id = "probeout";
    pre.textContent = "@@N84@@" + JSON.stringify(R) + "@@END@@";
    document.body.appendChild(pre);
  }
  Promise.resolve().then(run).then(done, function (e) {
    R.push({ n: "the probe itself ran to the end", ok: false, d: String(e && e.stack || e) });
    done();
  });
})();
`;

/*  The same page, driven into a playable-looking state for the picture. */
const SHOT = String.raw`
(function () {
  var B = window.__JUCE__.backend;
  function feed(n, p) { (B.__ls[n] || []).forEach(function (f) { f(p); }); }
  feed("initialState", window.__FIX);
  feed("hostParam", { p: [
    { id: "tape_mode", v: 1 }, { id: "ens_mode", v: 3 }, { id: "ens_mix", v: 0.62 },
    { id: "hall_mix", v: 0.42 }, { id: "hall_shim", v: 0.22 }, { id: "drv_mode", v: 1 },
    { id: "drv_amt", v: 0.38 }, { id: "choir_mix", v: 0.3 }, { id: "ring_depth", v: 0.18 },
    { id: "lfo_pitch", v: 0.22 }, { id: "a_pulse", v: 0.55 }, { id: "a_pw", v: 0.34 },
    { id: "b_saw", v: 0.62 }, { id: "b_fine", v: 0.56 }, { id: "b_semi", v: 0.792 },
    { id: "a_lpf", v: 0.58 }, { id: "b_lpf", v: 0.51 }, { id: "a_fmode", v: 1 },
    { id: "b_sync", v: 0 }, { id: "mode", v: 0 }, { id: "a_pan", v: 0.34 }, { id: "b_pan", v: 0.68 }
  ] });
  feed("patchinfo", { i: 0, name: "BLADE BRASS", cat: "BRASS" });
  var t = 0;
  for (var f = 0; f < 46; f++) {
    t += 1 / 30;
    var sc = [];
    for (var i = 0; i < 512; i++) {
      var ph = i / 512 * 6.2831853 * 3;
      sc.push(0.55 * (Math.sin(ph) + 0.4 * Math.sin(ph * 2 + 0.7) + 0.22 * Math.sin(ph * 5)));
    }
    var env = Math.min(1, t * 2.6), rel = Math.min(1, Math.max(0, (t - 0.9)));
    feed("meter", {
      out: 0.42 + 0.1 * Math.sin(t * 5), peak: 0.7,
      feg: [0.62 - 0.35 * rel, 0.44 - 0.3 * rel],
      aeg: [env * (1 - 0.25 * rel), env * 0.82 * (1 - 0.3 * rel)],
      cut: [2400, 1750], lfo: Math.sin(t * 4), wow: 0.35 * Math.sin(t * 1.7), drop: false,
      notes: [43, 50, 55, 59, -1, -1, -1, -1],
      levels: [env, env * 0.9, env * 0.8, env * 0.7, 0, 0, 0, 0],
      held: [43, 50, 55, 59], scope: sc, bend: 0, wheel: 0.35, at: 0.2
    });
  }
  feed("notice", { msg: "blade brass  ·  poly 8  ·  rank I + II  ·  tape + hall" });
})();
`;

/* ═══════════════════════════════════════════════════════════════════════
   Reading the engine. The fixture is built from the C++ table, so this
   gate does not carry a copy of the parameter list any more than the
   panel does — a parameter added in Engine.cpp is tested the same day.
   ═══════════════════════════════════════════════════════════════════════ */
const fs = require("fs");
const path = require("path");
const os = require("os");
const cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const ENGINE_CPP = path.join(ROOT, "Source", "Engine.cpp");
const ENGINE_H = path.join(ROOT, "Source", "Engine.h");
const PATCHES_CPP = path.join(ROOT, "Source", "Patches.cpp");
const PAGE = path.join(ROOT, "Source", "ui", "ui.html");
const BWFX = process.env.BWFX_DIR
  ? path.join(process.env.BWFX_DIR, "ui", "bwfx-rack.js")
  : "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";

function read(p) { return fs.readFileSync(p, "utf8"); }
function num(s) { return parseFloat(String(s).replace(/f\s*$/, "").trim()); }

/* the kind enum, in its own order, so a kind inserted in the middle does
   not silently renumber this file's expectations */
function kindMap() {
  const h = read(ENGINE_H);
  const i = h.indexOf("KP_PCT");
  const j = h.indexOf("};", i);
  const body = h.slice(i, j);
  const out = {};
  let n = 0;
  body.split("\n").forEach((line) => {
    const m = /^\s*(KP_[A-Z]+)/.exec(line.replace(/\/\/.*$/, ""));
    if (m) out[m[1]] = n++;
  });
  return out;
}

function listNames() {
  const s = read(ENGINE_CPP);
  const i = s.indexOf("const char* const* listNames");
  const j = s.indexOf("\n}", i);
  const body = s.slice(i, j);
  const arrays = {};
  const re = /static const char\*\s+(\w+)\s*\[\]\s*=\s*\{([^}]*)\}/g;
  let m;
  while ((m = re.exec(body))) {
    arrays[m[1]] = m[2].split(",").map((x) => x.trim()).filter(Boolean)
      .map((x) => x.replace(/^"/, "").replace(/"$/, ""));
  }
  const out = {};
  const re2 = /if\s*\(s == "([^"]+)"(?:\s*\|\|\s*s == "([^"]+)")?\)\s*\{\s*count = (\d+);\s*return (\w+);/g;
  while ((m = re2.exec(body))) {
    const arr = arrays[m[4]];
    if (!arr) continue;
    out[m[1]] = arr.slice(0, +m[3]);
    if (m[2]) out[m[2]] = arr.slice(0, +m[3]);
  }
  return out;
}

function rankRowTemplate() {
  const s = read(ENGINE_CPP);
  const i = s.indexOf("#define RANKROWS");
  const j = s.indexOf("static const PSpec SPECS", i);
  const body = s.slice(i, j);
  const rows = [];
  const re = /RROW\s*\(\s*r,\s*P\s*"([^"]+)"\s*,\s*N\s*"([^"]+)"\s*,\s*([^,]+),\s*(KP_\w+)\s*,\s*([^,]+),\s*([^,]+),/g;
  let m;
  while ((m = re.exec(body))) rows.push({ sfx: m[1], sfxName: m[2], def: num(m[3]), kind: m[4], lo: num(m[5]), hi: num(m[6]) });
  return rows;
}

function specs() {
  const KM = kindMap();
  const NAMES = listNames();
  const tmpl = rankRowTemplate();
  const s = read(ENGINE_CPP);
  const i = s.indexOf("static const PSpec SPECS[] =");
  const j = s.indexOf("\n};", i);
  const body = s.slice(i, j);
  const out = [];
  const push = (id, name, def, kind, lo, hi, rank) => {
    const k = KM[kind];
    if (k === undefined) throw new Error("unknown kind " + kind + " for " + id);
    const stepped = (kind === "KP_LIST" || kind === "KP_INT");
    const e = { id, n: name, def, kind: k, lo, fhi: hi, hi: stepped ? hi : 1, rank, v: def };
    if (NAMES[id]) e.names = NAMES[id];
    out.push(e);
  };
  body.split("\n").forEach((line) => {
    const L = line.replace(/\/\/.*$/, "");
    let m = /GROW\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*([^,]+),\s*(KP_\w+)\s*,\s*([^,]+),\s*([^,]+),/.exec(L);
    if (m) { push(m[1], m[2], num(m[3]), m[4], num(m[5]), num(m[6]), -1); return; }
    m = /RANKROWS\s*\(\s*(\d+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)/.exec(L);
    if (m) {
      const r = +m[1], P = m[2], N = m[3];
      tmpl.forEach((t) => push(P + t.sfx, N + t.sfxName, t.def, t.kind, t.lo, t.hi, r));
      return;
    }
    m = /RROW\s*\(\s*(\d+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*([^,]+),\s*(KP_\w+)\s*,\s*([^,]+),\s*([^,]+),/.exec(L);
    if (m) push(m[2], m[3], num(m[4]), m[5], num(m[6]), num(m[7]), +m[1]);
  });
  return out;
}

function patches() {
  const s = read(PATCHES_CPP);
  const out = [];
  const re = /\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*\w+\s*\}/g;
  let m;
  while ((m = re.exec(s))) out.push({ n: out.length, name: m[1], cat: m[2] });
  return out;
}

/* ═══════════════════════════════════════════════════════════════════════
   Staging and running
   ═══════════════════════════════════════════════════════════════════════ */
const STUB = `
window.__JUCE__ = { backend: {
  __sent: [], __ls: {},
  emitEvent: function (name, payload) {
    this.__sent.push({ name: name, payload: JSON.parse(JSON.stringify(payload)) });
  },
  addEventListener: function (n, f) { (this.__ls[n] = this.__ls[n] || []).push(f); },
  removeEventListener: function () {}
} };
`;

function chromeExe() {
  const c = [
    "C:/Program Files/Google/Chrome/Application/chrome.exe",
    "C:/Program Files (x86)/Google/Chrome/Application/chrome.exe",
    process.env.CHROME
  ].filter(Boolean);
  for (const p of c) if (fs.existsSync(p)) return p;
  throw new Error("no chrome found; set CHROME=<path>");
}

function unesc(s) {
  return s.replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&lt;/g, "<")
          .replace(/&gt;/g, ">").replace(/&amp;/g, "&");
}

function main() {
  const keep = process.argv.indexOf("--keep") >= 0;
  const results = [];
  const add = (n, c, d) => results.push({ n, ok: !!c, d: d === undefined ? "" : String(d) });

  /* ---- static checks on the shipped file ---------------------------- */
  const page = read(PAGE);
  add("exactly one </head>", page.split("</head>").length - 1 === 1);
  add("exactly one </body>", page.split("</body>").length - 1 === 1);
  add("every <script> is closed", page.split("<script").length === page.split("</script>").length,
      page.split("<script").length - 1 + " open / " + (page.split("</script>").length - 1) + " close");
  add("[hidden] is forced (an author display: rule beats the UA sheet)",
      page.indexOf("[hidden]{display:none!important}") >= 0);
  add("the error listener is in <head>",
      page.indexOf('addEventListener("error"') > 0 && page.indexOf('addEventListener("error"') < page.indexOf("</head>"));
  add("exactly one data-bwfx-open button", page.split("data-bwfx-open").length - 1 === 1);
  add("the rack fragment is loaded beside the page", page.indexOf('src="bwfx-rack.js"') > 0);
  add("no external URLs (nothing is fetched from the web)",
      !/(src|href)\s*=\s*["']https?:/i.test(page), (/(src|href)\s*=\s*["']https?:[^"']*/i.exec(page) || [""])[0]);
  add("the page carries no copy of the kind enum", !/KP_[A-Z]+/.test(page),
      (/KP_[A-Z]+/.exec(page) || [""])[0]);
  add("the page names no parameter default, range or list name",
      page.indexOf("CATHEDRAL\"") < 0 && page.indexOf("LADDER 24") < 0 && !/numParams/.test(page));
  const decalHooks = (page.match(/--decal-[a-z-]+:/g) || []).length;
  add("the decal hooks are exposed (" + decalHooks + ")", decalHooks >= 17, String(decalHooks));

  /* ---- the fixture, read out of the engine --------------------------- */
  const P = specs();
  const FIX = { product: "1984", build: "260922.1", params: P, patches: patches() };
  add("the engine table parsed (" + P.length + " parameters)", P.length > 100, String(P.length));
  add("both ranks parsed", P.filter((x) => x.rank === 0).length === P.filter((x) => x.rank === 1).length - 1,
      P.filter((x) => x.rank === 0).length + " / " + P.filter((x) => x.rank === 1).length);
  add("every LIST parameter has its names",
      P.filter((x) => x.kind === 4).every((x) => x.names && x.names.length === x.hi + 1),
      P.filter((x) => x.kind === 4 && (!x.names || x.names.length !== x.hi + 1)).map((x) => x.id).join(" "));
  add("the patch list parsed (" + FIX.patches.length + ")", FIX.patches.length > 30, String(FIX.patches.length));

  /* ---- stage --------------------------------------------------------- */
  const dir = path.join(os.tmpdir(), "n84-uiprobe");
  fs.rmSync(dir, { recursive: true, force: true });
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "stub.js"), STUB);
  fs.writeFileSync(path.join(dir, "fixture.js"), "window.__FIX = " + JSON.stringify(FIX) + ";");
  fs.writeFileSync(path.join(dir, "driver.js"), DRIVER);
  if (fs.existsSync(BWFX)) fs.copyFileSync(BWFX, path.join(dir, "bwfx-rack.js"));

  const hi = page.indexOf("</head>");
  const bi = page.lastIndexOf("</body>");
  const inject = (extra) =>
    page.slice(0, hi) + '<script src="stub.js"></script><script src="fixture.js"></script>' +
    page.slice(hi, bi) + extra + page.slice(bi);
  fs.writeFileSync(path.join(dir, "index.html"), inject('<script src="driver.js"></script>'));
  fs.writeFileSync(path.join(dir, "shot.html"), inject('<script src="shot.js"></script>'));
  fs.writeFileSync(path.join(dir, "shot.js"), SHOT);

  /* ---- drive --------------------------------------------------------- */
  const exe = chromeExe();
  const args = ["--headless=new", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
                "--force-device-scale-factor=1", "--window-size=1600,980",
                "--virtual-time-budget=25000", "--dump-dom", "file:///" + path.join(dir, "index.html").replace(/\\/g, "/")];
  const out = cp.execFileSync(exe, args, { encoding: "utf8", maxBuffer: 64 * 1024 * 1024, timeout: 120000 });
  const a = out.indexOf("@@N84@@"), b = out.indexOf("@@END@@");
  if (a < 0 || b < 0) {
    add("the page produced a report", false, "no marker in the dumped DOM (the page threw before the driver ran?)");
  } else {
    JSON.parse(unesc(out.slice(a + 7, b))).forEach((r) => results.push(r));
  }

  /* ---- the picture, because a passing probe on an ugly panel is not
     done. Written whether or not the checks passed. -------------------- */
  const shotDir = path.join(ROOT, "docs");
  fs.mkdirSync(shotDir, { recursive: true });
  const shot = path.join(shotDir, "panel-first.png");
  try {
    cp.execFileSync(exe, ["--headless=new", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
      "--force-device-scale-factor=1", "--window-size=1600,980", "--virtual-time-budget=9000",
      "--screenshot=" + shot, "file:///" + path.join(dir, "shot.html").replace(/\\/g, "/")],
      { encoding: "utf8", timeout: 120000, stdio: "pipe" });
    add("a screenshot was taken", fs.existsSync(shot) && fs.statSync(shot).size > 40000,
        fs.existsSync(shot) ? fs.statSync(shot).size + " bytes -> " + shot : "not written");
  } catch (e) {
    add("a screenshot was taken", false, String(e.message || e).slice(0, 160));
  }

  /* ---- report -------------------------------------------------------- */
  let bad = 0;
  results.forEach((r, i) => {
    if (!r.ok) bad++;
    const tag = r.ok ? "  ok  " : " FAIL ";
    console.log(tag + String(i + 1).padStart(3) + "  " + r.n + (r.d && !r.ok ? "\n            " + r.d : ""));
  });
  console.log("");
  console.log(bad === 0
    ? results.length + " CHECKS ALL CLEAR"
    : bad + " of " + results.length + " CHECKS FAILED");
  if (keep) console.log("staged at " + dir);
  else fs.rmSync(dir, { recursive: true, force: true });
  process.exit(bad === 0 ? 0 : 1);
}

main();
