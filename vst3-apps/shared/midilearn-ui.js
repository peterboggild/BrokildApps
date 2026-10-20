/* MIDI learn for the panel (pairs with shared/MidiLearn.h). Inlined into each
   plugin's ui.html. MLUI.init({ send, on, say }) once the bridge exists, then
   MLUI.attach(element, paramId, label) for every knob: right-click opens the
   LEARN / CLEAR menu, and a grab/release on the knob is sent to the host as a
   touch gesture (so Configure and automation recording see it). */
var MLUI = (function () {
  var B = null, MAP = {}, LEARNING = "", ITEMS = [], grabbed = null;
  function paint() {
    ITEMS.forEach(function (it) {
      it.el.classList.toggle("ml-learning", it.id === LEARNING);
      if (MAP[it.id] !== undefined) it.el.setAttribute("data-cc", "CC " + MAP[it.id]); else it.el.removeAttribute("data-cc");
    });
  }
  function release() { if (grabbed) { B.send({ k: "g", id: grabbed, on: false }); grabbed = null; } }
  function menu(id, label, e) {
    var old = document.getElementById("ml-menu"); if (old) old.remove();
    var m = document.createElement("div"); m.id = "ml-menu";
    m.style.cssText = "position:fixed;z-index:99999;background:#1b1b1e;border:1px solid #666;border-radius:4px;padding:3px;" +
      "font:700 10px Arial,sans-serif;letter-spacing:1px;color:#e9e7df;box-shadow:0 4px 14px #000a;left:" + e.clientX + "px;top:" + e.clientY + "px";
    function item(t, f) {
      var b = document.createElement("div"); b.textContent = t; b.style.cssText = "padding:6px 12px;cursor:pointer;white-space:nowrap";
      b.onpointerenter = function () { b.style.background = "#35353a"; }; b.onpointerleave = function () { b.style.background = ""; };
      b.onclick = function () { m.remove(); f(); }; m.appendChild(b);
    }
    var cc = MAP[id];
    item(LEARNING === id ? "CANCEL MIDI LEARN" : "MIDI LEARN" + (cc !== undefined ? "  (now CC " + cc + ")" : ""),
         function () { B.send({ k: "learn", id: id, on: LEARNING !== id }); });
    if (cc !== undefined) item("CLEAR MIDI CC " + cc, function () { B.send({ k: "unlearn", id: id }); if (B.say) B.say((label || id) + "   MIDI cleared"); });
    document.body.appendChild(m);
    var r = m.getBoundingClientRect();
    if (r.right > innerWidth) m.style.left = Math.max(0, innerWidth - r.width - 4) + "px";
    if (r.bottom > innerHeight) m.style.top = Math.max(0, innerHeight - r.height - 4) + "px";
    setTimeout(function () {
      document.addEventListener("pointerdown", function c(ev) { if (!m.contains(ev.target)) { m.remove(); document.removeEventListener("pointerdown", c, true); } }, true);
    }, 0);
  }
  return {
    init: function (bridge) {
      B = bridge;
      var st = document.createElement("style");
      st.textContent = ".ml-learning{outline:2px solid #ff9f1c !important;outline-offset:1px;animation:mlp 1s infinite}" +
        "[data-cc]{position:relative}[data-cc]::after{content:attr(data-cc);position:absolute;right:-2px;top:-5px;font:700 8px Arial,sans-serif;color:#ff9f1c;pointer-events:none;text-shadow:0 0 3px #000}" +
        "@keyframes mlp{50%{outline-color:transparent}}";
      document.head.appendChild(st);
      B.on("ccmap", function (d) {
        MAP = (d && d.map) || {}; LEARNING = (d && d.learn) || ""; paint();
        if (LEARNING && B.say) B.say("MIDI LEARN   move a knob on your controller");
      });
      addEventListener("pointerup", release, true); addEventListener("pointercancel", release, true);
    },
    attach: function (el, id, label) {
      if (!el || !B) return;
      ITEMS.push({ el: el, id: id });
      el.addEventListener("contextmenu", function (e) { e.preventDefault(); e.stopPropagation(); menu(id, label, e); });
      el.addEventListener("pointerdown", function (e) {
        if (e.button !== 0) return;
        release(); grabbed = id; B.send({ k: "g", id: id, on: true });
      }, true);
      paint();
    }
  };
})();
