/*  Probe round 3. Two faults, both the probe's, both already written down in
    this workshop's notes:

    1. NB.send batches into a MICROTASK, so reading __sent synchronously after
       a click sees nothing. Every message check has to await a tick. (The
       Photo-Synth lesson, verbatim: "every snapshot must await a tick".)
    2. A control that belongs to a view which is not on screen is DETACHED -
       the page moves nodes between views - so document.querySelector cannot
       find it. The page keeps them all in CTL{}, so the hook exposes them.
*/
const fs = require("fs");
const P = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
const U = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(P, "utf8");
let u = fs.readFileSync(U, "utf8");

/* ---- the page: let a probe reach a control that is not on screen -------- */
const ua = `  meter: function () { return M; },`;
const ub = `  meter: function () { return M; },
  /*  A control belonging to a view that is not showing is DETACHED (the views
      move the same node), so a probe cannot reach it with querySelector. */
  ctl: function (id) { return CTL[id]; },
  text: function (id) { var c = CTL[id]; return c ? c.querySelector(".val").textContent.trim() : null; },`;
if (u.split(ua).length !== 2) { console.log("UI ANCHOR MISS"); process.exit(1); }
u = u.replace(ua, ub);
fs.writeFileSync(U, u);

/* ---- the driver -------------------------------------------------------- */
const edits = [
  // read the readout through the hook, so a detached control still answers
  [`        var c = ctl(id);
        if (!c || !c.querySelector(".val")) { bad.push(id + ":no control"); return; }`,
   `        if (!window.__TTY.ctl(id)) { bad.push(id + ":no control"); return; }`],
  [`          var shown = c.querySelector(".val").textContent.trim();`,
   `          var shown = window.__TTY.text(id);`],
  // the LIST and SW clicks, awaited
  [`        clr(); window.__TTY.set("m_fmode", 0); clr();
        click(ctl("m_fmode"));
        var lp = ps().filter(function (m) { return m.id === "m_fmode"; });
        ok("a LIST control sends an integer", lp.length === 1 && lp[0].v === Math.round(lp[0].v) && lp[0].v >= 0,
           JSON.stringify(lp));
        clr(); window.__TTY.set("drone", 0); clr();
        click(ctl("drone"));
        var sw = ps().filter(function (m) { return m.id === "drone"; });
        ok("a switch sends 0 or 1", sw.length === 1 && (sw[0].v === 0 || sw[0].v === 1), JSON.stringify(sw));
        window.__TTY.set("drone", 0);`,
   `        clr(); window.__TTY.set("m_fmode", 0); clr();
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
        window.__TTY.set("drone", 0);`],
  // close the two extra closures opened above, at the end of that block
  [`        var unmod = ctl("m_res");
        ok("a parameter that is NOT modulated is not marked", !unmod.classList.contains("mod"));`,
   `        var unmod = ctl("m_res");
        ok("a parameter that is NOT modulated is not marked", !unmod.classList.contains("mod"));`],
  // the header/foot message block: one await after each click
  [`              clr(); click(document.querySelector("#b-panic"));
              ok("PANIC sends panic", kinds("panic").length === 1);
              clr();
              var strike = Array.prototype.slice.call(document.querySelectorAll(".btn")).filter(function (b) { return /STRIKE/.test(b.textContent); })[0];
              click(strike);
              ok("STRIKE sends strike", kinds("strike").length === 1, JSON.stringify(kinds("strike")));
              clr();
              var wk = document.querySelector(".wk");
              pev("pointerdown", wk, 0, 0); pev("pointerup", wk, 0, 0, { buttons: 0 });
              var nn = kinds("note");
              ok("the keyboard sends note on and off", nn.length === 2 && nn[0].on === true && nn[1].on === false, JSON.stringify(nn));
              clr(); click(document.querySelector("#b-undo"));
              ok("UNDO sends undo", kinds("undo").length === 1);
              clr(); click(document.querySelector("#b-mutate"));
              var mu = kinds("mutate");
              ok("MUTATE sends mutate with an amount and a lock mask", mu.length === 1 && mu[0].amount > 0 && mu[0].lock >= 0, JSON.stringify(mu));
              clr(); click(document.querySelector("#b-a"));
              ok("A recalls (or stores) through the ab message", kinds("ab").length === 1, JSON.stringify(kinds("ab")));

              /* scenes */
              clr();
              var st = Array.prototype.slice.call(document.querySelectorAll(".scene .btn")).filter(function (b) { return b.textContent === "STORE"; })[0];
              click(st);
              ok("a scene STORE sends scene/store", kinds("scene").length === 1 && kinds("scene")[0].op === "store", JSON.stringify(kinds("scene")));

              /* the patch menu */
              clr(); click(document.querySelector("#b-patch"));
              return tick(10).then(function () {`,
   `              /*  Every one of these awaits a tick: NB.send batches into a
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
              return tick(10).then(function () {`],
  // the patch pick, awaited
  [`                click(items[items.length - 1]);
                ok("choosing a preset sends patch", kinds("patch").length === 1, JSON.stringify(kinds("patch")));

                /* ---- 10. the matrix, the macros, the shapes --------- */
                window.__TTY.view("life", "matrix");
                return frame().then(function () {
                  clr();
                  var row = document.querySelectorAll(".mxr")[0];
                  click(row.querySelector(".mf.sw"));
                  var sl = kinds("slot");
                  ok("a matrix row sends a whole slot", sl.length === 1 && sl[0].i === 0 && "depth" in sl[0] && "curve" in sl[0], JSON.stringify(sl).slice(0, 160));`,
   `                clr();
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
                  ok("a matrix row sends a whole slot", sl.length === 1 && sl[0].i === 0 && "depth" in sl[0] && "curve" in sl[0], JSON.stringify(sl).slice(0, 160));`],
  // the shape edit, awaited
  [`                    pev("pointerup", cv, r.left + r.width * 0.62, r.top + r.height * 0.32, { buttons: 0 });
                    var sh = kinds("mseg");
                    ok("editing a shape sends mseg with its points", sh.length >= 1 && sh[0].pts && sh[0].pts.length >= 2, JSON.stringify(sh[0] || null).slice(0, 140));

                    window.__TTY.view("main");
                    return frame().then(function () {
                      clr();
                      click(document.querySelectorAll(".mac .mn")[7]);
                      return tick(10).then(function () {
                        var rows = document.querySelectorAll(".pop .maprow");
                        ok("the macro map editor opens with eight destination rows", rows.length >= 8, String(rows.length));
                        click(rows[1].querySelector(".btn"));
                        ok("clearing a macro destination sends macro/set", kinds("macro").length >= 1, JSON.stringify(kinds("macro")).slice(0, 120));
                        document.querySelectorAll(".pop .ph .x").forEach(function (x) { click(x); });`,
   `                    pev("pointerup", cv, r.left + r.width * 0.62, r.top + r.height * 0.32, { buttons: 0 });
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
                        document.querySelectorAll(".pop .ph .x").forEach(function (x) { click(x); });`],
  // the memory buttons, awaited
  [`                          var want = ["capture", "remember", "import", "scala"];
                          var got = want.filter(function (k) { return kinds(k).length > 0; });
                          ok("the MEMORY buttons send capture, remember, import and scala", got.length === 4, "got " + got.join(","));

                          /* ---- 12. the end ------------------------- */
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
  }`,
   `                          return tick(0).then(function () {
                          var want = ["capture", "remember", "import", "scala"];
                          var got = want.filter(function (k) { return kinds(k).length > 0; });
                          ok("the MEMORY buttons send capture, remember, import and scala", got.length === 4, "got " + got.join(","));

                          /* ---- 12. the end ------------------------- */
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
  }`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(P, s);
console.log("probe patched (" + edits.length + " edits) and the page gained __TTY.ctl/.text");
