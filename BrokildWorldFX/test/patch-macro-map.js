/*  MACROS BECOME A MAPPING, NOT AN OFFSET.

    Peter: "the macro slider should always map an fx slider from 0 to 100 %."

    I built the additive model — a macro ADDS to the patch value, the Mars
    Wars patch-bay way — because it preserved "a macro at rest is exactly the
    patch". It is a defensible modulation design and it is the wrong one for a
    macro, for two reasons that only show up in use:

      * with the destination already near the top of its range, adding to it
        does almost nothing. That is what "the sound does not change" was;
      * the control never moved, because an offset leaves the patch value
        alone and I had deliberately kept the panel showing that. A macro you
        cannot see working is a macro that is not working.

    So: an assigned parameter is OWNED by its macro and swept across its own
    full range.

        value = (depth >= 0 ? lo : hi) + macro * depth * (hi - lo)

    depth +100 % : macro 0 -> the bottom, macro 1 -> the top.
    depth -100 % : macro 0 -> the top,    macro 1 -> the bottom.
    depth  +50 % : macro 0 -> the bottom, macro 1 -> halfway.

    The contract that actually mattered survives, and it is the one about the
    DEFAULT rack rather than about every assignment: macro 5 ships on the
    dry/wet at -100 %, so at rest the mix is 1.0 — exactly today's sound — and
    pushing it up takes the rack out. A rack with nothing assigned is still
    bit-identical, which is what the empty-rack memcmp checks.

    What is deliberately given up: assigning a control now MOVES it, at once,
    to wherever the macro currently sits. That is how every macro system
    behaves and it is what makes the thing legible.

    One parameter, one macro. Two macros fighting over a destination has no
    sensible answer under a mapping, so assigning takes it off any other.
*/
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs, label) {
  let s;
  try { s = fs.readFileSync(path, "utf8"); } catch (e) { miss.push(label + ": unreadable"); return; }
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  for (const [a, b, tag] of subs) {
    const A = a.split(String.fromCharCode(10)).join(NL);
    const B = b.split(String.fromCharCode(10)).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(label + ": " + tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

// ── the core ───────────────────────────────────────────────────────────────
const wCpp = edit("C:/Users/peter/b/BrokildWorldFX/src/bwfx_macros.cpp", [
// one parameter, one macro
["    materialiseDefault();\n    auto& v = macroAssign[(size_t) macro];",
 "    materialiseDefault();\n"
+ "    /*  One destination, one macro. Under a mapping there is no sensible\n"
+ "        answer to two macros owning the same control, so taking it here\n"
+ "        takes it away from wherever it was. */\n"
+ "    for (int m = 0; m < kMacros; ++m)\n"
+ "        if (m != macro)\n"
+ "        {\n"
+ "            auto& o = macroAssign[(size_t) m];\n"
+ "            for (size_t i = 0; i < o.size(); ++i)\n"
+ "                if (o[i].dest == dest) { o.erase (o.begin() + (long) i); break; }\n"
+ "        }\n"
+ "    auto& v = macroAssign[(size_t) macro];", "exclusive"],

// the mapping itself
["        const float v = macroIn[(size_t) d.macro].load (std::memory_order_relaxed);\n"
+"        const float delta = v * d.depth * (d.hi - d.lo);",
 "        const float v = macroIn[(size_t) d.macro].load (std::memory_order_relaxed);\n"
+"        /*  A MAPPING, not an offset: the macro owns the destination and\n"
+"            sweeps it across its own range. Which end it starts from is the\n"
+"            sign of the depth, so a negative assignment runs top to bottom\n"
+"            and macro 5 can hold the dry/wet at full-rack when at rest. */\n"
+"        const float from = (d.depth >= 0.0f ? d.lo : d.hi);\n"
+"        const float want = from + v * d.depth * (d.hi - d.lo);", "mapping"],

["            case 1:                                          // rack mix\n"
+"            {\n"
+"                const float base = mixIn.load (std::memory_order_relaxed);\n"
+"                const float want = base + delta;\n"
+"                mo += (want < d.lo ? d.lo : (want > d.hi ? d.hi : want)) - base;\n"
+"                moTouched = true;\n"
+"                break;\n"
+"            }",
 "            case 1:                                          // rack mix\n"
+"            {\n"
+"                const float base = mixIn.load (std::memory_order_relaxed);\n"
+"                mo = (want < d.lo ? d.lo : (want > d.hi ? d.hi : want)) - base;\n"
+"                moTouched = true;\n"
+"                break;\n"
+"            }", "mix"],

["            case 2:                                          // module parameter\n"
+"            {\n"
+"                const float base = mods[(size_t) d.type]->getParamRaw (d.param);\n"
+"                const float want = base + delta;\n"
+"                mods[(size_t) d.type]->setParamOffset (\n"
+"                    d.param, (want < d.lo ? d.lo : (want > d.hi ? d.hi : want)) - base);\n"
+"                break;\n"
+"            }",
 "            case 2:                                          // module parameter\n"
+"            {\n"
+"                const float base = mods[(size_t) d.type]->getParamRaw (d.param);\n"
+"                mods[(size_t) d.type]->setParamOffset (\n"
+"                    d.param, (want < d.lo ? d.lo : (want > d.hi ? d.hi : want)) - base);\n"
+"                break;\n"
+"            }", "param"],

["            case 3:                                          // module presence\n"
+"            {\n"
+"                const float base = presenceIn[(size_t) d.type].load (std::memory_order_relaxed);\n"
+"                const float want = base + delta;\n"
+"                presenceOff[(size_t) d.type].store (\n"
+"                    (want < 0.0f ? 0.0f : (want > 1.0f ? 1.0f : want)) - base,\n"
+"                    std::memory_order_relaxed);\n"
+"                break;\n"
+"            }",
 "            case 3:                                          // module presence\n"
+"            {\n"
+"                const float base = presenceIn[(size_t) d.type].load (std::memory_order_relaxed);\n"
+"                presenceOff[(size_t) d.type].store (\n"
+"                    (want < 0.0f ? 0.0f : (want > 1.0f ? 1.0f : want)) - base,\n"
+"                    std::memory_order_relaxed);\n"
+"                break;\n"
+"            }", "presence"]
], "bwfx_macros.cpp");

// ── the fragment computes the same mapping ─────────────────────────────────
const wJs = edit("C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js", [
["  function modOffset(dest, lo, hi) {\n"
+"    if (!macros || !macroVals) return 0;\n"
+"    var off = 0;\n"
+"    for (var m = 0; m < macros.length; m++)\n"
+"      for (var j = 0; j < macros[m].length; j++)\n"
+"        if (macros[m][j].d === dest)\n"
+"          off += (macroVals[m] || 0) * (macros[m][j].a / 100) * (hi - lo);\n"
+"    return off;\n"
+"  }\n"
+"  function effective(dest, base, lo, hi) {\n"
+"    var v = base + modOffset(dest, lo, hi);\n"
+"    return v < lo ? lo : (v > hi ? hi : v);\n"
+"  }",

 "  /*  The SAME mapping Rack::applyMacros computes, sign and clamp included.\n"
+"     Duplicated here on purpose: the alternative is a panel showing one\n"
+"     thing while the ears hear another, which is the bug this replaced. */\n"
+"  function macroOwning(dest) {\n"
+"    if (!macros) return -1;\n"
+"    for (var m = 0; m < macros.length; m++)\n"
+"      for (var j = 0; j < macros[m].length; j++)\n"
+"        if (macros[m][j].d === dest) return m;\n"
+"    return -1;\n"
+"  }\n"
+"  function effective(dest, base, lo, hi) {\n"
+"    var m = macroOwning(dest);\n"
+"    if (m < 0 || !macroVals) return base;\n"
+"    var depth = depthOf(dest) / 100;\n"
+"    var from = depth >= 0 ? lo : hi;\n"
+"    var v = from + (macroVals[m] || 0) * depth * (hi - lo);\n"
+"    return v < lo ? lo : (v > hi ? hi : v);\n"
+"  }", "formula"],

// the slider follows too, because the macro owns it now
["      var e = effective(r.dest, r.base(), r.pd.lo, r.pd.hi);\n"
+"      var moved = Math.abs(e - r.base()) > (r.pd.hi - r.pd.lo) * 1e-4;\n"
+"      r.out.textContent = fmt(r.pd, e);\n"
+"      r.out.classList.toggle('bwfx-modded', moved);",

 "      var owned = macroOwning(r.dest) >= 0;\n"
+"      var e = effective(r.dest, r.base(), r.pd.lo, r.pd.hi);\n"
+"      r.out.textContent = fmt(r.pd, e);\n"
+"      r.out.classList.toggle('bwfx-modded', owned);\n"
+"      /*  The macro OWNS this control, so the slider goes where the macro\n"
+"          puts it — a number that moved while the slider sat still was the\n"
+"          most confusing part of the additive version. */\n"
+"      if (r.input) {\n"
+"        r.input.classList.toggle('bwfx-owned', owned);\n"
+"        if (owned) r.input.value = e;\n"
+"      }", "display"],

["          MODROWS.push({ row: row, out: out, pd: pd, dest: id + \".\" + pd.id,\n"
+"                         base: function () { return parseFloat(inp.value); } });",
 "          MODROWS.push({ row: row, out: out, input: inp, pd: pd, dest: id + \".\" + pd.id,\n"
+"                         base: function () { return parseFloat(inp.value); } });", "register"]
], "bwfx-rack.js");

// ── an owned control reads as owned ────────────────────────────────────────
const wCss = edit("C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js", [
['    ".bwfx-modded{color:var(--bwfx-acc)!important;text-shadow:0 0 6px rgba(63,224,216,.35)}",',
 '    ".bwfx-modded{color:var(--bwfx-acc)!important;text-shadow:0 0 6px rgba(63,224,216,.35)}",\n'
+'    ".bwfx-owned{accent-color:var(--bwfx-acc)!important;opacity:.95}",', "owned css"]
], "bwfx-rack.js css");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wCpp(); wJs(); wCss();
console.log("macros map their destination across its full range");
