
(function () {
  var B = window.__JUCE__.backend;
  var R = [];
  function ok(n, c, d) { R.push({ n: n, ok: !!c, d: d === undefined ? "" : String(d) }); }
  function tick(ms) { return new Promise(function (r) { setTimeout(r, ms === undefined ? 0 : ms); }); }
  /*  A frame under --virtual-time-budget: rAF does NOT advance there, so this
      waits on a timeout, which does. The page draws from its own rAF where it
      has one, so anything measured after this has had a chance to lay out. */
  function frame() { return tick(24); }
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
  function kinds(k) { return msgs().filter(function (m) { return m && m.k === k; }); }
  function feed(n, p) { (B.__ls[n] || []).forEach(function (f) { f(p); }); }

  var F = window.__FIX;
  var SPEC = {}; F.params.forEach(function (p) { SPEC[p.id] = p; });
  var IDS = F.params.map(function (p) { return p.id; }).filter(function (id) { return id.slice(0, 5) !== "bwfx_"; });

  /* ---- the formatting laws, written from PROTOCOL.md, not from the page ---- */
  function xm(t, lo, hi) { return lo * Math.pow(hi / lo, Math.max(0, Math.min(1, t))); }
  function hz(x) { return x < 1 ? x.toFixed(3) + " Hz" : (x < 100 ? x.toFixed(2) + " Hz" : (x < 1000 ? Math.round(x) + " Hz" : (x / 1000).toFixed(2) + " kHz")); }
  function ms(x) { return x < 100 ? x.toFixed(1) + " ms" : (x < 1000 ? Math.round(x) + " ms" : (x / 1000).toFixed(2) + " s"); }
  function sec(x) { if (x < 60) return x.toFixed(2) + " s"; var m = Math.floor(x / 60), s = Math.round(x - 60 * m); if (s === 60) { s = 0; m++; } return m + " m " + s + " s"; }
  var NOTEN = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
  function sg(n) { return (n > 0 ? "+" : "") + n; }
  function law(id, v) {
    var s = SPEC[id];
    switch (s.kind) {
      case 0: return Math.round(v * 100) + " %";
      case 1: return v >= 0.5 ? "ON" : "OFF";
      case 2: return hz(xm(v, s.lo, s.fhi));
      case 3: return ms(xm(v, s.lo, s.fhi));
      case 4: return s.names[Math.round(v)];
      case 5: { var t = (v - 0.5) * s.lo; return (t > 0 ? "+" : "") + t.toFixed(1) + " st"; }
      case 6: return sg(Math.round((v - 0.5) * s.lo)) + " c";
      case 7: return Math.round(v * s.lo) + " c";
      case 8: return sg(Math.round((v - 0.5) * 200)) + " %";
      case 9: return v < 0.01 ? "OFF" : ms(xm(v, s.lo, s.fhi));
      case 10: { var g = 2 * v * v; return g <= 1e-9 ? "-INF dB" : (20 * Math.log10(g)).toFixed(1) + " dB"; }
      case 11: return Math.round(50 + 45 * v) + " %";
      case 12: return String(Math.round(v));
      case 13: return sec(xm(v, s.lo, s.fhi));
      case 14: { var x = 2 * v - 1, y = (x < 0 ? -1 : 1) * x * x * x * s.lo; return (y > 0 ? "+" : "") + y.toFixed(1) + " Hz"; }
      case 15: { var d = s.lo + v * (s.fhi - s.lo); return (d > 0 ? "+" : "") + d.toFixed(1) + " dB"; }
      case 16: { var n = Math.round(v); return NOTEN[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1); }
    }
    return "?";
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
  function click(el) {
    if (!el) return;
    var r = el.getBoundingClientRect(), x = r.left + r.width / 2, y = r.top + r.height / 2;
    pev("pointerdown", el, x, y); pev("pointerup", el, x, y, { buttons: 0 });
    el.dispatchEvent(new MouseEvent("click", { bubbles: true, cancelable: true, clientX: x, clientY: y }));
  }
  function ctl(id) { return document.querySelector('.ctl[data-id="' + id + '"]'); }
  function ink(cv) {
    if (!cv || !cv.width) return 0;
    var g = cv.getContext("2d"), d;
    try { d = g.getImageData(0, 0, cv.width, cv.height).data; } catch (e) { return -1; }
    var n = 0;
    for (var i = 0; i < d.length; i += 4) {
      if (d[i + 3] < 8) continue;                       /* transparent is not ink */
      if (d[i] < 24 && d[i + 1] < 26 && d[i + 2] < 30) continue;   /* the panel's own near-black */
      n++;
    }
    return n;
  }

  function run() {
    /* ---- 1. boot ---------------------------------------------------- */
    ok("hello was sent before anything else", msgs().length > 0 && msgs()[0].k === "hello", JSON.stringify(msgs()[0] || null));
    ok("no JS errors during boot", window.__TTYERR.length === 0, window.__TTYERR.join(" | ").slice(0, 300));
    ok("no failed sub-resources", window.__TTYRES.filter(function (u) { return u.indexOf("assets/decals/") < 0; }).length === 0,
       window.__TTYRES.join(" ").slice(0, 200));
    ok("the debug hook is there", window.__TTY && typeof window.__TTY.set === "function");

    clr();
    feed("initialState", F);
    return tick(0).then(frame).then(function () {
      ok("initialState was acknowledged", kinds("ack").length === 1);
      ok("the page reported itself ready", kinds("ready").length === 1);
      ok("no JS errors applying initialState", window.__TTYERR.length === 0, window.__TTYERR.join(" | ").slice(0, 400));

      /* ---- 2. one control per parameter, each with a hint ------------ */
      var missing = [], dup = [], nohint = [], autohint = [];
      IDS.forEach(function (id) {
        /*  Ask the page's map, not the DOM: a control in a view that is not
            showing is detached, because the views MOVE the same node. */
        var c = window.__TTY.ctl(id);
        if (!c) { missing.push(id); return; }
        if (document.querySelectorAll('.ctl[data-id="' + id + '"]').length > 1) dup.push(id);
        var h = c.getAttribute("data-hint");
        if (!h) nohint.push(id);
        else if (c.dataset.hintAuto) autohint.push(id);
      });
      ok("every parameter has a control (" + (IDS.length - missing.length) + " of " + IDS.length + ")", missing.length === 0, missing.slice(0, 14).join(" "));
      ok("no parameter has two controls", dup.length === 0, dup.slice(0, 10).join(" "));
      ok("every control carries a hint", nohint.length === 0, nohint.slice(0, 10).join(" "));
      ok("every hint was WRITTEN, not auto-filled (" + autohint.length + " auto)", autohint.length === 0, autohint.slice(0, 16).join(" "));
      ok("the five BWFX macros are left to the rack fragment",
         document.querySelectorAll('.ctl[data-id^="bwfx_"]').length === 0);
      ok("exactly one BWFX button", document.querySelectorAll("[data-bwfx-open]").length === 1);

      /* ---- 3. the formatting laws ----------------------------------- */
      var bad = [];
      var sample = ["volume","ceiling","quality","bassmono","vmode","voices","glide","tune","fine","bend","scale",
                    "drone_root","drone_chord","m_o1wave","m_beat","m_unidet","m_drifttime","m_cut","m_o1pw",
                    "s_shift","s_addspread","s_pshift","mem_dur","mem_pspread","mem_disp","st_pitch",
                    "e_dl_time","e_rv_decay","e_sh_hz","e_fb_delay","l1_rate","h_dur","ext_gain","mac_mass"];
      sample.forEach(function (id) {
        if (!SPEC[id]) { bad.push(id + ":absent"); return; }
        if (!window.__TTY.ctl(id)) { bad.push(id + ":no control"); return; }
        [0, 0.27, 0.5, 0.81, 1].forEach(function (t) {
          var v = SPEC[id].hi > 1 ? Math.round(t * SPEC[id].hi) : t;
          window.__TTY.set(id, v);
          var shown = window.__TTY.text(id);
          var want = law(id, window.__TTY.get(id));
          if (shown !== want) bad.push(id + "@" + v + ": page '" + shown + "' vs law '" + want + "'");
        });
        window.__TTY.set(id, SPEC[id].def);
      });
      ok("every formatting law agrees with PROTOCOL.md (" + sample.length + " parameters x 5 values)", bad.length === 0, bad.slice(0, 6).join(" | "));

      /* ---- 4. a drag is ONE coalesced message ----------------------- */
      clr();
      window.__TTY.set("m_cut", 0.5); clr();
      drag(ctl("m_cut"), 40);
      return tick(0).then(function () {
        var p = ps().filter(function (m) { return m.id === "m_cut"; });
        ok("a drag sends exactly one coalesced p message", p.length === 1, p.length + " messages");
        ok("...with a value in range and in the right direction", p.length === 1 && p[0].v > 0.5 && p[0].v <= 1,
           p.length ? String(p[0].v) : "none");
        ok("...and the control drew the new value", Math.abs(window.__TTY.get("m_cut") - (p[0] ? p[0].v : -1)) < 1e-9);

        /* a LIST sends an integer, a SW sends 0/1 */
        clr(); window.__TTY.set("m_fmode", 0); clr();
        click(ctl("m_fmode"));
        return tick(0).then(function () {
        var lp = ps().filter(function (m) { return m.id === "m_fmode"; });
        ok("a LIST control sends an integer", lp.length === 1 && lp[0].v === Math.round(lp[0].v) && lp[0].v >= 0,
           JSON.stringify(lp));
        clr(); window.__TTY.set("drone", 0); clr();
        click(ctl("drone"));
        return tick(0).then(function () {
        var sw = ps().filter(function (m) { return m.id === "drone"; });
        ok("a switch sends 0 or 1", sw.length === 1 && (sw[0].v === 0 || sw[0].v === 1), JSON.stringify(sw));
        window.__TTY.set("drone", 0);

        /* ---- 4b. tooltips: off by default, CTRL is the shortcut ------- */
        var tip = document.querySelector("#tip");
        var tbtn = document.querySelector("#b-hints");
        ok("the tooltip button says TOOLTIPS", tbtn && /TOOLTIP/i.test(tbtn.textContent), tbtn ? tbtn.textContent : "no button");
        ok("tooltips are OFF on a fresh instance", !tbtn.classList.contains("on"));
        var cc = ctl("m_cut");
        cc.dispatchEvent(new MouseEvent("mouseenter", { bubbles: false }));
        ok("...so hovering a control shows nothing", !tip.classList.contains("show"));
        document.dispatchEvent(new KeyboardEvent("keydown", { key: "Control", bubbles: true }));
        ok("holding CTRL shows the tooltip for what the pointer is over", tip.classList.contains("show"));
        ok("...and it names the control and its value",
           /CUTOFF/i.test(tip.textContent) && /Hz/.test(tip.textContent), tip.textContent.slice(0, 80));
        var tr = tip.getBoundingClientRect(), cr = cc.getBoundingClientRect();
        ok("...beside the control, never over it",
           tr.left >= cr.right - 1 || tr.right <= cr.left + 1 || tr.top >= cr.bottom - 1 || tr.bottom <= cr.top + 1,
           "tip " + Math.round(tr.left) + "," + Math.round(tr.top) + " control " + Math.round(cr.left) + "," + Math.round(cr.top));
        /*  Not a black rectangle: it must be a lit plate, so its background is a
            gradient and the lightest stop in it is well clear of the deck. */
        var cs2 = getComputedStyle(tip);
        var nums = (cs2.backgroundImage.match(/[0-9]+/g) || []).map(Number);
        var lightest = 0; for (var q = 0; q < nums.length; q++) if (nums[q] <= 255 && nums[q] > lightest) lightest = nums[q];
        ok("the tooltip is a lit plate, not a black rectangle",
           cs2.backgroundImage.indexOf("gradient") >= 0 && lightest >= 40,
           "lightest channel " + lightest + " in " + cs2.backgroundImage.slice(0, 70));
        document.dispatchEvent(new KeyboardEvent("keyup", { key: "Control", bubbles: true }));
        ok("releasing CTRL takes it away again", !tip.classList.contains("show"));
        click(tbtn);
        ok("the TOOLTIPS button lights when they are on", tbtn.classList.contains("on"));
        cc.dispatchEvent(new MouseEvent("mouseenter", { bubbles: false }));
        ok("...and then a hover does show one", tip.classList.contains("show"));
        click(tbtn);
        ok("switching them off again hides it", !tbtn.classList.contains("on") && !tip.classList.contains("show"));
        cc.dispatchEvent(new MouseEvent("mouseleave", { bubbles: false }));

        /* ---- 5. hostParam moves the control, and is not echoed ------- */
        clr();
        feed("hostParam", { p: [{ id: "m_res", v: 0.77 }, { id: "st_stress", v: 0.4 }] });
        ok("hostParam moves the control", Math.abs(window.__TTY.get("m_res") - 0.77) < 1e-6,
           String(window.__TTY.get("m_res")));
        ok("...and its readout", ctl("m_res").querySelector(".val").textContent.trim() === law("m_res", 0.77),
           ctl("m_res").querySelector(".val").textContent);
        ok("...and nothing is sent back", ps().length === 0, JSON.stringify(ps().slice(0, 3)));

        /* ---- 6. eff draws the modulated mark ------------------------- */
        var effv = F.params.map(function (p) { return window.__TTY.get(p.id); });
        var iCut = F.params.findIndex(function (p) { return p.id === "m_cut"; });
        effv[iCut] = Math.min(1, window.__TTY.get("m_cut") + 0.3);
        clr();
        feed("eff", { v: effv });
        var mark = ctl("m_cut").querySelector(".em");
        ok("the eff event marks the control as modulated", ctl("m_cut").classList.contains("mod"));
        ok("...and the mark itself is lit", mark && mark.classList.contains("live"));
        ok("...and eff is never sent back", ps().length === 0);
        var unmod = ctl("m_res");
        ok("a parameter that is NOT modulated is not marked", !unmod.classList.contains("mod"));

        /* ---- 7. the meter draws ink --------------------------------- */
        var spec = [], scope = [];
        for (var i = 0; i < 48; i++) spec.push(0.25 + 0.7 * Math.abs(Math.sin(i * 0.4)));
        for (var j = 0; j < 256; j++) scope.push(0.6 * Math.sin(j * 0.1));
        for (var rep = 0; rep < 8; rep++) feed("meter", {
          out: 0.3, peak: 0.6, lim: 0.1, loop: 0.4, space: 0.3,
          strata: [0.5, 0.3, 0.2, 0.7], notes: [45, 52, -1, -1, -1, -1, -1, -1],
          levels: [0.6, 0.4, 0, 0, 0, 0, 0, 0], held: [45, 52],
          history: 0.42, grains: 17, memAct: 0.3, erosion: 0.45, voices: 2,
          spectrum: spec, scope: scope,
          mods: { lfo: [0.5,-0.2,0,0,0,0,0,0], env: [0.7,0,0,0], shape: [0.3,0,0,0], rnd: [0.2,0,0,0], fol: [0.4,0], evt: [0.9,0,0,0],
                  net: { e: [0.5,0.3,0.2,0.7,0.6], b: [0.4,0.5,0.3,0.6,0.5], t: [0.1,0.2,0.1,0.3,0.2] } },
          fracture: rep === 3, capturing: false, capLen: 0, hasCapture: false, remembered: false,
          hasImport: false, importName: "", stopped: false, bend: 0.2, wheel: 0.6, at: 0
        });
        return frame().then(function () {
          ok("no JS errors after the meter", window.__TTYERR.length === 0, window.__TTYERR.join(" | ").slice(0, 300));
          var rec = document.querySelector("#rec") || document.querySelector(".cvwrap canvas");
          ok("the spectral record drew ink", ink(rec) > 20, "ink " + ink(rec));
          ok("the voice lamps lit for the two sounding voices",
             document.querySelectorAll("#vlamps .lamp.on").length === 2,
             String(document.querySelectorAll("#vlamps .lamp.on").length));
          ok("the output meter moved", parseFloat(document.querySelector("#vu-out i").style.width) > 5,
             document.querySelector("#vu-out i").style.width);
          ok("the held notes are shown", /A2|E3/.test(document.querySelector("#heldtxt").textContent),
             document.querySelector("#heldtxt").textContent);
          ok("the stratum activity bars moved",
             Array.prototype.slice.call(document.querySelectorAll(".actbar i")).filter(function (b) { return parseFloat(b.style.width) > 5; }).length === 4);
          feed("meter", { stopped: true });
          ok("the STOPPED banner appears after PANIC", document.querySelector("#stopban").classList.contains("on"));
          feed("meter", { stopped: false });

          /* ---- 8. every view builds and fits its box ----------------- */
          feed("patchinfo", { i: 15, name: "THIRTY THOUSAND YEARS", cat: "EVOLVING WORLD", note: "The flagship journey." });
      var over = [], empty = [], SEEN = {}, STOLEN = [];
      /*  What the always-visible strips own, read from the page before any
          view is switched: these must never move into a view. */
      var STRIPIDS = Array.prototype.slice.call(
        document.querySelectorAll("#head .ctl, #foot .ctl, #macros .ctl")).map(function (c) { return c.dataset.id; });
          var views = [["main"],["sources","mass"],["sources","signal"],["sources","memory"],["sources","structure"],
                       ["life","lfo"],["life","env"],["life","rnd"],["life","evt"],["life","matrix"],
                       ["history"],["env","lanes"],["env","space"],["mix"]];
          return views.reduce(function (chain, v) {
            return chain.then(function () {
              window.__TTY.view(v[0], v[1]);
              return frame().then(function () {
                var page = document.querySelector('.view[data-view="' + v[0] + '"] .page:not([hidden])');
                if (!page) { empty.push(v.join("/") + ":nopage"); return; }
                var pans = page.querySelectorAll("[data-panel]");
                if (!pans.length) empty.push(v.join("/") + ":nopanels");
                page.querySelectorAll(".ctl").forEach(function (c) { SEEN[c.dataset.id] = 1; });
                document.querySelectorAll("#head .ctl, #foot .ctl, #macros .ctl").forEach(function (c) { SEEN[c.dataset.id] = 1; });
                STRIPIDS.forEach(function (id) {
                  if (!document.querySelector('#head .ctl[data-id="' + id + '"], #foot .ctl[data-id="' + id + '"], #macros .ctl[data-id="' + id + '"]')
                      && STOLEN.indexOf(id) < 0) STOLEN.push(id + "@" + v.join("/"));
                });
                /*  The strips are measured too: the foot's DRONE panel was
                    clipping a control while this check looked only at views. */
                var boxes = Array.prototype.slice.call(page.querySelectorAll(".panel"))
                  .concat(Array.prototype.slice.call(document.querySelectorAll("#head .panel, #foot .panel, #macros .mac, .scene")));
                boxes.forEach(function (p) {
                  if (!p.clientHeight && !p.clientWidth) return;
                  var cs = getComputedStyle(p);
                  var where = (p.dataset.panel || p.className || "?");
                  if (p.scrollHeight > p.clientHeight + 2 && cs.overflowY !== "auto") over.push(v.join("/") + " " + where + " h" + p.scrollHeight + ">" + p.clientHeight);
                  if (p.scrollWidth > p.clientWidth + 2 && cs.overflowX !== "auto") over.push(v.join("/") + " " + where + " w" + p.scrollWidth + ">" + p.clientWidth);
                });
              });
            });
          }, Promise.resolve()).then(function () {
            ok("every view builds with panels in it", empty.length === 0, empty.join(" | "));
            /*  The check that matters: after walking every view, is there a
                parameter that has a control but no home anywhere? */
            /*  patch is presented by the header's preset window rather than a
                knob; anything else without a home is a fault. */
            var PRESENTED = { patch: "#pname" };
            var unreachable = IDS.filter(function (id) { return !SEEN[id] && !PRESENTED[id]; });
            ok("every parameter is reachable in some view (" + (IDS.length - unreachable.length) + " of " + IDS.length + ")",
               unreachable.length === 0, unreachable.slice(0, 16).join(" "));
            var notShown = Object.keys(PRESENTED).filter(function (id) {
              var e = document.querySelector(PRESENTED[id]);
              return !e || !e.textContent.trim() || e.textContent.trim() === "\u2014";
            });
            ok("...and the ones presented another way are on screen (patch: the preset window)",
               notShown.length === 0, notShown.join(" "));
            /*  ...and none is in two places at once while a view is open: a
                control is ONE node, so a strip must not lose it to a view. */
            ok("no control was taken from the header, foot or macro strips",
               STOLEN.length === 0, STOLEN.slice(0, 12).join(" "));
            ok("no panel overflows its box at deck size", over.length === 0, over.slice(0, 8).join(" | "));
            window.__TTY.view("main");
            var deck = document.querySelector("#deck");
            ok("the deck is 1440 x 900 and does not scroll",
               deck.scrollWidth <= 1442 && deck.scrollHeight <= 902,
               deck.scrollWidth + "x" + deck.scrollHeight);
            /*  The instrument's own name must fit its block: it ran under the
                patch window and was the one unreadable thing on the deck. */
            var h1 = document.querySelector("#brand h1"), brand = document.querySelector("#brand");
            ok("the title fits inside the brand block",
               h1.scrollWidth <= brand.clientWidth + 1,
               "title " + h1.scrollWidth + " in " + brand.clientWidth);
            var pb = document.querySelector("#patchbox").getBoundingClientRect();
            var tb = h1.getBoundingClientRect();
            ok("...and does not run under the patch window", tb.right <= pb.left + 1,
               "title ends " + Math.round(tb.right) + ", patch window starts " + Math.round(pb.left));


            /* ---- 9. the messages the panel must be able to send ------ */
            window.__TTY.view("main");
            return frame().then(function () {
              /*  Every one of these awaits a tick: NB.send batches into a
                  microtask, so a synchronous read of __sent sees nothing. */
              function press(sel, kind, test, note) {
                clr();
                var el = typeof sel === "string" ? document.querySelector(sel) : sel;
                click(el);
                return tick(0).then(function () {
                  var got = kinds(kind);
                  ok(note, test ? test(got) : got.length === 1, JSON.stringify(got).slice(0, 160));
                });
              }
              var strike = Array.prototype.slice.call(document.querySelectorAll(".btn")).filter(function (b) { return /STRIKE/.test(b.textContent); })[0];
              var storeBtn = Array.prototype.slice.call(document.querySelectorAll(".scene .btn")).filter(function (b) { return b.textContent === "STORE"; })[0];
              return press("#b-panic", "panic", null, "PANIC sends panic")
                .then(function () { return press(strike, "strike", null, "STRIKE sends strike"); })
                .then(function () {
                  clr();
                  var wk = document.querySelector(".wk");
                  pev("pointerdown", wk, 0, 0); pev("pointerup", wk, 0, 0, { buttons: 0 });
                  return tick(0).then(function () {
                    var nn = kinds("note");
                    ok("the keyboard sends note on and off", nn.length === 2 && nn[0].on === true && nn[1].on === false, JSON.stringify(nn));
                  });
                })
                .then(function () { return press("#b-undo", "undo", null, "UNDO sends undo"); })
                .then(function () { return press("#b-mutate", "mutate", function (g) { return g.length === 1 && g[0].amount > 0 && g[0].lock >= 0; }, "MUTATE sends mutate with an amount and a lock mask"); })
                .then(function () { return press("#b-a", "ab", null, "A recalls (or stores) through the ab message"); })
                .then(function () { return press(storeBtn, "scene", function (g) { return g.length === 1 && g[0].op === "store"; }, "a scene STORE sends scene/store"); })
                .then(function () {
              clr(); click(document.querySelector("#b-patch"));
              return tick(10).then(function () {
                var items = document.querySelectorAll(".pop .pi");
                ok("the patch menu lists every preset (" + F.presets.length + ")", items.length >= F.presets.length, String(items.length));
                clr();
                click(items[items.length - 1]);
                return tick(0).then(function () {
                ok("choosing a preset sends patch", kinds("patch").length === 1, JSON.stringify(kinds("patch")));

                /* ---- 10. the matrix, the macros, the shapes --------- */
                window.__TTY.view("life", "matrix");
                return frame().then(function () {
                  clr();
                  var row = document.querySelectorAll(".mxr")[0];
                  click(row.querySelector(".mf.sw"));
                  return tick(0).then(function () {
                  var sl = kinds("slot");
                  ok("a matrix row sends a whole slot", sl.length === 1 && sl[0].i === 0 && "depth" in sl[0] && "curve" in sl[0], JSON.stringify(sl).slice(0, 160));
                  ok("32 matrix rows are built", document.querySelectorAll(".mxr").length === 32, String(document.querySelectorAll(".mxr").length));
                  ok("the source palette has every source (" + (F.sources.length - 1) + ")",
                     document.querySelectorAll(".chipp").length === F.sources.length - 1,
                     String(document.querySelectorAll(".chipp").length));

                  window.__TTY.view("life", "env");
                  return frame().then(function () {
                    clr();
                    var cv = document.querySelectorAll(".shp canvas")[0];
                    ok("four shape editors are drawn", document.querySelectorAll(".shp canvas").length === 4);
                    ok("a shape editor has ink", ink(cv) > 20, "ink " + ink(cv));
                    var r = cv.getBoundingClientRect();
                    pev("pointerdown", cv, r.left + r.width * 0.6, r.top + r.height * 0.3);
                    pev("pointermove", cv, r.left + r.width * 0.62, r.top + r.height * 0.32, { button: -1 });
                    pev("pointerup", cv, r.left + r.width * 0.62, r.top + r.height * 0.32, { buttons: 0 });
                    return tick(0).then(function () {
                    var sh = kinds("mseg");
                    ok("editing a shape sends mseg with its points", sh.length >= 1 && sh[0].pts && sh[0].pts.length >= 2, JSON.stringify(sh[0] || null).slice(0, 140));

                    window.__TTY.view("main");
                    return frame().then(function () {
                      clr();
                      click(document.querySelectorAll(".mac .mn")[7]);
                      return tick(10).then(function () {
                        var rows = document.querySelectorAll(".pop .maprow");
                        ok("the macro map editor opens with eight destination rows", rows.length >= 8, String(rows.length));
                        clr();
                        click(rows[1].querySelector(".btn"));
                        return tick(0).then(function () {
                        ok("clearing a macro destination sends macro/set", kinds("macro").length >= 1, JSON.stringify(kinds("macro")).slice(0, 120));
                        document.querySelectorAll(".pop .ph .x").forEach(function (x) { click(x); });

                        /* ---- 11. capture / import / scala --------- */
                        window.__TTY.view("sources", "memory");
                        return frame().then(function () {
                          clr();
                          var btns = Array.prototype.slice.call(document.querySelectorAll(".btn"));
                          ["CAPTURE", "REMEMBER", "IMPORT WAV", "SCALA"].forEach(function (t) {
                            var b = btns.filter(function (x) { return x.textContent.trim() === t; })[0];
                            if (b) click(b);
                          });
                          return tick(0).then(function () {
                          var want = ["capture", "remember", "import", "scala"];
                          var got = want.filter(function (k) { return kinds(k).length > 0; });
                          ok("the MEMORY buttons send capture, remember, import and scala", got.length === 4, "got " + got.join(","));

                          /* ---- 12. the end ------------------------- */
                          /*  The record lives in the MAIN view: measured from
                              anywhere else its box is 0 and the check would pass
                              or fail for the wrong reason. */
                          window.__TTY.view("main");
                          return frame().then(function () {
                          var rec = document.querySelector("#rec");
                          ok("the spectral record is drawn at its displayed size",
                             rec && rec.clientWidth > 8 && Math.abs(rec.width - rec.clientWidth) <= 8 && Math.abs(rec.height - rec.clientHeight) <= 8,
                             rec ? rec.width + "x" + rec.height + " bitmap in " + rec.clientWidth + "x" + rec.clientHeight + " box" : "no canvas");
                          ok("zero JS errors at the end", window.__TTYERR.length === 0, window.__TTYERR.join(" | ").slice(0, 400));
                          var st2 = window.__TTY.state();
                          ok("the page built " + st2.placed + " controls for " + st2.order + " parameters",
                             st2.placed >= IDS.length, st2.placed + " / " + IDS.length);
                          done();
                          });
                          });
                        });
                        });
                      });
                    });
                    });
                  });
                  });
                });
                });
              });
                });
            });
          });
        });
        });
        });
      });
    });
  }

  function done() {
    var pre = document.createElement("pre");
    pre.id = "probeout";
    pre.textContent = "@@TTY@@" + JSON.stringify(R) + "@@END@@";
    document.body.appendChild(pre);
  }
  Promise.resolve().then(run).catch(function (e) {
    R.push({ n: "the driver ran to the end", ok: false, d: String(e && e.stack || e).slice(0, 500) });
    done();
  });
})();