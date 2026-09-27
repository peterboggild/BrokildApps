/*  A window into what the RACK thinks, not what the panel thinks.

    Three rounds of this have now been diagnosed by reading state rather than
    reasoning about it, and the one link never yet observed is the engine's
    own: does applyMacros actually resolve the destination and write a
    non-zero offset? Everything either side of that has been verified.
*/
"use strict";
const fs = require("fs");
const miss = [];

// ── the core: report the resolved table and the live offsets ───────────────
{
  const P = "C:/Users/peter/b/BrokildWorldFX/src/bwfx.h";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = "    bool macroIsDefault() const;              // never edited: macro 5 holds the dry/wet";
  if (s.split(A).length - 1 !== 1) miss.push("header anchor");
  else s = s.split(A).join(
    A + NL
    + "    /*  What the rack RESOLVED and what it is currently adding — the one" + NL
    + "        link in the macro chain a panel probe cannot see. */" + NL
    + "    std::string macroDebugJson() const;");
  if (! miss.length) fs.writeFileSync(P, s);
}

{
  const P = "C:/Users/peter/b/BrokildWorldFX/src/bwfx_macros.cpp";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = "bool Rack::macroIsDefault() const { return macroDefaulted; }";
  if (s.split(A).length - 1 !== 1) { miss.push("cpp anchor"); }
  else {
    const BODY = [
"bool Rack::macroIsDefault() const { return macroDefaulted; }",
"",
"std::string Rack::macroDebugJson() const",
"{",
"    char buf[256];",
"    std::string s = \"{\\\"vals\\\":[\";",
"    for (int i = 0; i < kMacros; ++i)",
"    {",
"        if (i) s += \",\";",
"        std::snprintf (buf, sizeof (buf), \"%g\", (double) getMacro (i));",
"        s += buf;",
"    }",
"    const int idx = destIdx.load (std::memory_order_acquire);",
"    const int n = destCount[(size_t) idx];",
"    std::snprintf (buf, sizeof (buf), \"],\\\"resolved\\\":%d,\\\"dest\\\":[\", n);",
"    s += buf;",
"    for (int i = 0; i < n; ++i)",
"    {",
"        const RDest& d = destBuf[(size_t) idx][(size_t) i];",
"        if (i) s += \",\";",
"        float off = 0.0f, base = 0.0f;",
"        const char* what = \"?\";",
"        if (d.kind == 1) { what = \"mix\"; base = getMix(); off = mixOff.load (std::memory_order_relaxed); }",
"        else if (d.kind == 2)",
"        {",
"            what = mods[(size_t) d.type]->desc().params[d.param].id;",
"            base = mods[(size_t) d.type]->getParamRaw (d.param);",
"            off  = mods[(size_t) d.type]->getParam (d.param) - base;",
"        }",
"        else if (d.kind == 3)",
"        {",
"            what = \"pr\";",
"            base = presenceIn[(size_t) d.type].load (std::memory_order_relaxed);",
"            off  = presenceOff[(size_t) d.type].load (std::memory_order_relaxed);",
"        }",
"        std::snprintf (buf, sizeof (buf),",
"                       \"{\\\"k\\\":%d,\\\"m\\\":%d,\\\"w\\\":\\\"%s\\\",\\\"d\\\":%g,\\\"base\\\":%g,\\\"off\\\":%g}\",",
"                       (int) d.kind, (int) d.macro, what, (double) d.depth,",
"                       (double) base, (double) off);",
"        s += buf;",
"    }",
"    return s + \"]}\";",
"}"].join(NL);
    s = s.split(A).join(BODY);
    fs.writeFileSync(P, s);
  }
}

// ── the adapter carries it in the state payload ────────────────────────────
{
  const P = "C:/Users/peter/b/BrokildWorldFX/adapter/bwfx_juce.h";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = '    obj->setProperty ("macroVals", mv);';
  if (s.split(A).length - 1 !== 1) miss.push("adapter anchor");
  else s = s.split(A).join(
    A + NL
    + '    obj->setProperty ("macroDbg", juce::JSON::parse (juce::String (rack.macroDebugJson())));');
  if (! miss.length) fs.writeFileSync(P, s);
}

// ── and the fragment hands it to a probe ───────────────────────────────────
{
  const P = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = '      if (p.macroVals) { macroVals = p.macroVals; }';
  if (s.split(A).length - 1 !== 1) miss.push("fragment state anchor");
  else s = s.split(A).join(A + NL + '      if (p.macroDbg) { macroDbg = p.macroDbg; }');

  const B = "  var macros = null, macroVals = null, macEls = [], armed = -1;";
  if (s.split(B).length - 1 !== 1) miss.push("fragment var anchor");
  else s = s.split(B).join(B + NL + "  var macroDbg = null;   // what the RACK resolved, for probes");

  const C = "               modRows: MODROWS.length, built: built };";
  if (s.split(C).length - 1 !== 1) miss.push("fragment debug anchor");
  else s = s.split(C).join("               modRows: MODROWS.length, built: built, rack: macroDbg };");

  if (! miss.length) fs.writeFileSync(P, s);
}

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("the rack can now be asked what it resolved and what it is adding");
