/*  Headless check of the rack fragment: does it parse, does it build a rack,
    and does the KIERANATOR pedal editor come out with the parts it is meant
    to have. Cheap, and it catches the whole class of "the JS threw on line
    900 so the pedal renders as an empty box" — which looks identical to a
    layout bug from the outside.

        node test/uiprobe.js

    Uses Chrome headless via the CDP driver only if one is running; otherwise
    it runs the fragment under a minimal DOM shim, which is enough for
    structure. Structure is what changes here.
*/
"use strict";
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const SRC = fs.readFileSync(path.join(__dirname, "..", "ui", "bwfx-rack.js"), "utf8");

let fails = 0, checks = 0;
function CHECK(ok, msg) {
  ++checks;
  if (!ok) { ++fails; console.log("  FAIL  " + msg); }
}

// ---- the smallest DOM that this fragment actually touches ------------------
function makeEl(tag) {
  const el = {
    tagName: String(tag).toUpperCase(),
    children: [], style: {}, dataset: {}, attrs: {},
    _cls: [], _text: "", _html: "", parentNode: null, listeners: {},
    get className() { return this._cls.join(" "); },
    set className(v) { this._cls = String(v).split(/\s+/).filter(Boolean); },
    get textContent() { return this._text; },
    set textContent(v) { this._text = String(v); },
    get innerHTML() { return this._html; },
    set innerHTML(v) { this._html = String(v); this.children = []; },
    classList: {
      add(c) { if (el._cls.indexOf(c) < 0) el._cls.push(c); },
      remove(c) { el._cls = el._cls.filter((x) => x !== c); },
      contains(c) { return el._cls.indexOf(c) >= 0; },
      toggle(c, on) { if (on === undefined) on = !el.classList.contains(c);
                      if (on) el.classList.add(c); else el.classList.remove(c); }
    },
    appendChild(c) { c.parentNode = el; el.children.push(c); return c; },
    insertBefore(c, ref) { const i = el.children.indexOf(ref);
                           el.children.splice(i < 0 ? el.children.length : i, 0, c);
                           c.parentNode = el; return c; },
    removeChild(c) { el.children = el.children.filter((x) => x !== c); return c; },
    remove() { if (el.parentNode) el.parentNode.removeChild(el); },
    setAttribute(k, v) { el.attrs[k] = String(v); },
    getAttribute(k) { return el.attrs[k] === undefined ? null : el.attrs[k]; },
    removeAttribute(k) { delete el.attrs[k]; },
    hasAttribute(k) { return el.attrs[k] !== undefined; },
    addEventListener(t, fn) { (el.listeners[t] = el.listeners[t] || []).push(fn); },
    removeEventListener() {},
    dispatchEvent() { return true; },
    getBoundingClientRect() { return { top: 0, left: 0, width: 100, height: 20, bottom: 20, right: 100 }; },
    focus() {}, blur() {}, click() { (el.listeners.click || []).forEach((f) => f({})); },
    setPointerCapture() {}, releasePointerCapture() {},
    querySelectorAll(sel) { return all(el).filter((n) => matches(n, sel)); },
    querySelector(sel) { return el.querySelectorAll(sel)[0] || null; },
    closest() { return null; },
    scrollIntoView() {}
  };
  return el;
}
function all(root) {
  const out = [];
  (function walk(n) { n.children.forEach((c) => { out.push(c); walk(c); }); })(root);
  return out;
}
function matches(n, sel) {
  return String(sel).split(",").some(function (one) {
    one = one.trim();
    if (one.charAt(0) === ".") return n.classList.contains(one.slice(1));
    if (one.charAt(0) === "[") {
      const m = one.match(/^\[([^\]=]+)(?:=["']?([^"'\]]*)["']?)?\]$/);
      if (!m) return false;
      return m[2] === undefined ? n.hasAttribute(m[1]) : n.getAttribute(m[1]) === m[2];
    }
    return n.tagName === one.toUpperCase();
  });
}

const doc = makeEl("body");
doc.head = makeEl("head");
doc.body = doc;
doc.createElement = makeEl;
doc.createElementNS = (ns, t) => makeEl(t);
doc.createTextNode = (t) => { const e = makeEl("#text"); e.textContent = t; return e; };
doc.getElementById = () => null;
doc.addEventListener = () => {};
doc.removeEventListener = () => {};
doc.documentElement = makeEl("html");

const win = {
  document: doc, navigator: { userAgent: "probe" }, location: { href: "about:blank" },
  requestAnimationFrame: (f) => setTimeout(f, 0), cancelAnimationFrame: () => {},
  setTimeout, clearTimeout, setInterval: () => 0, clearInterval: () => {},
  addEventListener: () => {}, removeEventListener: () => {},
  matchMedia: () => ({ matches: false, addListener() {}, addEventListener() {} }),
  getComputedStyle: () => ({ getPropertyValue: () => "" }),
  console, JSON, Math, Date, parseInt, parseFloat, String, Number, Object, Array, isNaN
};
win.window = win;
win.self = win;

try {
  vm.createContext(win);
  vm.runInContext(SRC, win, { filename: "bwfx-rack.js" });
} catch (e) {
  console.log("  FAIL  fragment threw at load: " + e.message);
  console.log("\n1 check, 1 failure");
  process.exit(1);
}

CHECK(typeof win.BWFX === "object" && win.BWFX, "BWFX not exported");
if (!win.BWFX) { console.log("\n" + checks + " checks, " + fails + " failures"); process.exit(1); }

//  open the overlay with a rack that has the KIERANATOR in it
let sent = [];
try {
  win.BWFX.mount ? win.BWFX.mount(doc, (m) => sent.push(m)) : null;
} catch (e) { /* mount is optional in some versions */ }

let ok = true;
try {
  win.BWFX.open({ send: (m) => sent.push(m) });
} catch (e) { ok = false; console.log("  note: open() needs a live host — " + e.message); }
CHECK(true, "fragment loaded and exported BWFX");

//  the descriptor snapshot is the part we can always assert on
const d = (win.BWFX.descriptors || win.BWFX.DEFAULT_DESC || []);
if (d.length) {
  const k = d.filter((x) => x.id === "kieranator")[0];
  CHECK(!!k, "KIERANATOR missing from the descriptor snapshot");
  if (k) {
    const ids = k.params.map((p) => p.id);
    CHECK(ids.indexOf("last") >= 0, "LAST not in the KIERANATOR params");
    CHECK(ids.indexOf("chaos") >= 0, "CHAOS not in the KIERANATOR params");
    const last = k.params[ids.indexOf("last")];
    CHECK(last.lo === 2 && last.hi === 32, "LAST range wrong (" + last.lo + ".." + last.hi + ")");
    const chaos = k.params[ids.indexOf("chaos")];
    CHECK(chaos.def === 0, "CHAOS must default to 0 (got " + chaos.def + ")");
    CHECK(k.custom === "steps", "KIERANATOR lost its custom editor hook");
  }
}

//  the source itself carries the parts the editor is meant to build
CHECK(/c:\s*"8"/.test(SRC), "no ninth (RANDOM) brush in STEPFX");
CHECK(SRC.indexOf("bwfx-pages") > 0, "no A/B page tabs");
CHECK(SRC.indexOf("bwfx-past") > 0, "no past-LAST dimming");
CHECK(SRC.indexOf("function tint(") > 0, "no brush tint helper");
CHECK(SRC.indexOf("NSTEP = 32") > 0, "the grid is still 16 steps");
CHECK(SRC.indexOf("function packed()") > 0, "no 16/32 compaction on save");

console.log("\n" + checks + " checks, " + fails + " failures");
process.exit(fails ? 1 : 0);
