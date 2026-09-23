// Wire up the panel decals BEFORE they exist.
//
// The whole design rule here is that a missing part costs nothing: every decal
// is probed at boot, and only the ones that actually load replace their CSS
// equivalent. So a partial delivery is safe, a wrong filename is visible
// rather than fatal, and the panel never ends up half-drawn.
//
// Three pieces:
//   CMake  — glob whatever PNGs exist at configure time into BinaryData.
//   Editor — serve them at /art/<filename>.
//   Page   — probe each, and inject the CSS that uses it.
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

// ─────────────────────────────────────────────────────────────── CMake ──────
const writeC = edit(R + "CMakeLists.txt", [
[`juce_add_binary_data(FullMetalRacketAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "\${BWFX_DIR}/ui/bwfx-rack.js")`,
`# ── panel decals ────────────────────────────────────────────────────────────
# Whatever PNGs exist at CONFIGURE time get compiled in; none is required. Add
# or replace files and re-run "cmake -S . -B build", or the glob is stale.
# Two locations are searched, local first, and a duplicate basename is skipped
# so the same part cannot be compiled in twice.
set(FMR_ART_DIRS
    "\${CMAKE_CURRENT_SOURCE_DIR}/art"
    "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/assets/fmr-panel-decals")
set(FMR_ART_FILES "")
set(_fmr_seen "")
foreach(_dir \${FMR_ART_DIRS})
    file(GLOB _found "\${_dir}/*.png")
    foreach(_f \${_found})
        get_filename_component(_base "\${_f}" NAME)
        list(FIND _fmr_seen "\${_base}" _at)
        if(_at EQUAL -1)
            list(APPEND _fmr_seen "\${_base}")
            list(APPEND FMR_ART_FILES "\${_f}")
        endif()
    endforeach()
endforeach()
list(LENGTH FMR_ART_FILES _fmr_count)
message(STATUS "Full Metal Racket: \${_fmr_count} panel decal(s) compiled in")

juce_add_binary_data(FullMetalRacketAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Source/ui/ui.html
        "\${BWFX_DIR}/ui/bwfx-rack.js"
        \${FMR_ART_FILES})`, "cmake glob"]
]);

// ────────────────────────────────────────────────────────────── Editor ──────
const writeE = edit(R + "Source/PluginEditor.cpp", [
[`    juce::WebBrowserComponent::Options buildOptions (FmrAudioProcessor& p)`,
`    /*  A panel decal, looked up by its ORIGINAL filename. JUCE mangles
        filenames into C identifiers, so the generated table is the only
        honest way back — and a part that was never delivered simply is not in
        it, which is exactly the answer the page wants. */
    std::optional<juce::WebBrowserComponent::Resource> artResource (const juce::String& file)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (file != juce::String (BinaryData::originalFilenames[i])) continue;
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
            {
                juce::WebBrowserComponent::Resource r;
                r.data.resize ((size_t) size);
                std::memcpy (r.data.data(), data, (size_t) size);
                r.mimeType = "image/png";
                return r;
            }
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (FmrAudioProcessor& p)`, "art resource"],

[`                if (path == "/bwfx-rack.js") return bwfxResource();
                return std::nullopt;`,
`                if (path == "/bwfx-rack.js") return bwfxResource();
                if (path.startsWith ("/art/")) return artResource (path.fromLastOccurrenceOf ("/", false, false));
                return std::nullopt;`, "art route"]
]);

// ──────────────────────────────────────────────────────────────── Page ──────
const writeU = edit(R + "Source/ui/ui.html", [
[`/*  The hello handshake (from Hairfryer): the page announces itself on EVERY`,
`/* ─── 7 · panel decals ───────────────────────────────────────────────────────
   Each part is probed; only the ones that load replace their CSS equivalent,
   so a missing or misnamed file costs the look of that one part and nothing
   else. Rules are injected into a single stylesheet rather than set per
   element — twelve strips share one rule, and the panel repaints once.        */
(function () {
  const sheet = document.createElement("style");
  document.head.appendChild(sheet);
  const add = css => sheet.sheet.insertRule(css, sheet.sheet.cssRules.length);
  const url = f => "/art/" + f;

  const PARTS = [
    ["fmr-panel.png", u => {
      add('#face{background-image:url("' + u + '"),radial-gradient(130% 70% at 50% -10%,#ffffff30,#0000 55%),linear-gradient(180deg,#f6efe0,#e7dcc3 18%,#d3c6a6 92%,#bfb08c)!important;background-size:512px 512px,auto,auto;background-blend-mode:multiply,normal,normal}');
      add('.strip,#webk{background-image:url("' + u + '")!important;background-size:512px 512px;background-blend-mode:multiply}');
    }],
    ["fmr-anodised.png", u => {
      add('.blk,.np,.foot,#vu,#mval{background-image:url("' + u + '")!important;background-size:320px 320px}');
    }],
    ["fmr-cheek.png", u => {
      add('.cheek{background:url("' + u + '") center/100% 100% no-repeat!important}');
    }],
    /*  The pointer is painted into the knob image, so the ELEMENT turns and the
        drawn pointer is switched off. Same variable drives both, so nothing in
        the widget code has to know which of the two is in play. */
    ["fmr-knob.png", u => {
      add('.knob{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important;transform:rotate(var(--a,-135deg))}');
      add('.knob::after,.knob::before{display:none!important}');
    }],
    ["fmr-fadercap-v.png", u => {
      add('.fader .cap{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important}');
      add('.fader .cap::after{display:none!important}');
    }],
    ["fmr-fadercap-m.png", u => {
      add('.fader.big .cap{background:url("' + u + '") center/100% 100% no-repeat!important}');
    }],
    ["fmr-slot-v.png", u => {
      add('.fader .trk{background:url("' + u + '") center/100% 100% no-repeat!important;box-shadow:none!important;left:0;right:0;width:auto;margin-left:0}');
    }],
    ["fmr-slot-m.png", u => {
      add('.fader.big .trk{background:url("' + u + '") center/100% 100% no-repeat!important;box-shadow:none!important;left:0;right:0;width:auto;margin-left:0}');
    }],
    ["fmr-key.png", u => {
      add('.k{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important}');
    }],
    ["fmr-key-lit.png", u => {
      add('.k.on,.k:active{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important}');
    }],
    ["fmr-keydark.png", u => {
      add('.k.dark{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important}');
    }],
    ["fmr-keydark-lit.png", u => {
      add('.k.dark.on,.k.dark:active{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important}');
    }],
    ["fmr-pad.png", u => {
      add('.pad{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important}');
    }],
    ["fmr-led.png", u => {
      add('.pad .led{background:url("' + u + '") center/100% 100% no-repeat!important;box-shadow:none!important;position:relative}');
      document.body.classList.add("led-decal");
    }],
    ["fmr-led-lit.png", u => {
      add('.pad .led i{position:absolute;inset:0;display:block;background:url("' + u + '") center/100% 100% no-repeat;opacity:var(--glow,0);border-radius:50%;filter:drop-shadow(0 0 calc(var(--glow,0)*9px) #ff8f1f)}');
    }],
    ["fmr-nameplate.png", u => {
      add('#plate{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important}');
    }],
    /*  Four screws at the corners of the faceplate. Tiny, and it is remarkable
        how much of "this is a real object" they carry. */
    ["fmr-screw.png", u => {
      const s = 'position:absolute;width:14px;height:14px;background:url("' + u + '") center/100% 100% no-repeat;pointer-events:none;z-index:5';
      ["left:8px;top:8px", "right:8px;top:8px", "left:8px;bottom:8px", "right:8px;bottom:8px"].forEach(pos => {
        const d = document.createElement("div");
        d.style.cssText = s + ";" + pos;
        document.getElementById("face").appendChild(d);
      });
      document.getElementById("face").style.position = "relative";
    }]
  ];

  let found = 0;
  PARTS.forEach(([file, apply]) => {
    const img = new Image();
    img.onload = () => { try { apply(url(file)); ++found; } catch (e) {} };
    img.onerror = () => {};          // not delivered: keep the drawn version
    img.src = url(file);
  });
  window.__decals = () => found;
})();

/*  The hello handshake (from Hairfryer): the page announces itself on EVERY`, "decal loader"],

// the lamp needs a child for the lit layer; harmless when no decal is present
[`    const pad = el("div", "pad", ft);
    c._led = el("div", "led", pad);`,
 `    const pad = el("div", "pad", ft);
    c._led = el("div", "led", pad);
    el("i", "", c._led);                       // the lit lamp layer, if a decal arrives`, "led child"],

[`      const a = clamp(0.3 + v * 3.0, 0, 1);
      c._led.style.background = "rgba(255,155,50," + a.toFixed(3) + ")";
      c._led.style.boxShadow = "0 0 " + (5 + 16 * a).toFixed(1) + "px rgba(255,143,31," + (a * 0.85).toFixed(3) + ")";`,
 `      const a = clamp(0.3 + v * 3.0, 0, 1);
      c._led.style.setProperty("--glow", a.toFixed(3));       // used by the decal layer
      c._led.style.background = "rgba(255,155,50," + a.toFixed(3) + ")";
      c._led.style.boxShadow = "0 0 " + (5 + 16 * a).toFixed(1) + "px rgba(255,143,31," + (a * 0.85).toFixed(3) + ")";`, "led glow var"],

[`    if (v < 0.002) { c._led.style.background = "#4a3110"; c._led.style.boxShadow = "inset 0 1px 2px #000"; }`,
 `    if (v < 0.002) { c._led.style.setProperty("--glow", "0"); c._led.style.background = "#4a3110"; c._led.style.boxShadow = "inset 0 1px 2px #000"; }`, "led off"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeC(); writeE(); writeU();
console.log("decal loader wired (CMake + editor + page)");
