// Writes the decal order from ONE source, docs/decal_parts.json:
//   docs/decal_brief.md    the brief as a document (attach it, or paste it)
//   docs/decal_brief.html  the same as a page with a copy button per part
// tools/ingest-decals.ps1 reads the same JSON, so what is ordered and what is
// checked on return cannot disagree.   node tools/build-decal-brief.js
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const D = JSON.parse(fs.readFileSync(path.join(root, "docs", "decal_parts.json"), "utf8"));

const kindText = {
  T: "transparent background with real alpha (fully transparent outside the object, fully opaque inside it)",
  O: "opaque, filling the whole canvas edge to edge (no border, no vignette)",
  S: "semi-transparent where stated, with real alpha (no checkerboard painted in, no white or black halo)"
};
const tileText = { none: "", xy: "It must tile seamlessly in both directions.", x: "It must tile seamlessly left to right." };
const paletteLine = "Palette, use these and nothing brighter: " + D.palette.map(p => p[0] + " " + p[1]).join(", ") + ".";

function fullPrompt(p) {
  const [w, h, shape] = D.sizes[p.size];
  const lines = [
    D.style,
    "",
    "Make ONE image: " + p.file + ".png, " + w + " x " + h + " px (" + shape + "). Background: " + kindText[p.kind] + ".",
    p.prompt + (tileText[p.tile] ? " " + tileText[p.tile] : ""),
    paletteLine,
    "Before you deliver it, check that the drawing fills the canvas as described. A drawing sitting in a strip or a corner of a larger canvas is wrong: make it again."
  ];
  return lines.join("\n");
}

const pairsOf = D.parts.filter(p => p.pair && p.file < p.pair).map(p => [p.file, p.pair]);
const groups = D.groups.map(g => ({ g, parts: D.parts.filter(p => p.group === g.id) }));
const count = D.parts.length;

// ---------------------------------------------------------------- markdown
let md = "";
md += "# " + D.title + "\n\n";
md += "Image parts for a software tape-echo instrument called **Magneto Transport**. Each part is made on its own and the program assembles them, so **never paint a whole panel**: a program places every part at exact coordinates, and an image generator cannot hit exact coordinates. What matters is that each part looks right on its own canvas, at the stated size, with real transparency where asked.\n\n";
md += "There are " + count + " parts. Make them one at a time: one image per request.\n\n";
md += "## How to work\n\n";
md += "1. Every request starts with the style paragraph below, then the part's own text. The page version of this brief has a copy button that puts both together.\n";
md += "2. Use exactly the size given. Only three sizes are used: 1024 x 1024 (square), 1536 x 1024 (landscape) and 1024 x 1536 (portrait).\n";
md += "3. **Transparent means real alpha.** If an image comes back with a white, grey or checkerboard background where it should be transparent, ask for it again with a transparent background.\n";
md += "4. **Fill the canvas as described.** Earlier deliveries came back with the drawing in a thin strip of a larger canvas. That is a fail; make it again.\n";
md += "5. **Pairs must match.** For each pair (off/on, up/pressed) make the second from the first, at the same size and position, so that switching between them moves nothing but the light or the press.\n";
md += "6. Save each image as PNG with exactly the file name given, put them all in one zip, and add a short note listing anything that is not as asked.\n\n";
md += "## The style paragraph (it starts every request)\n\n> " + D.style + "\n\n";
md += "**Palette (hex):**\n\n| role | hex |\n|---|---|\n" + D.palette.map(p => "| " + p[0] + " | " + p[1] + " |").join("\n") + "\n\n";
md += "**Pairs:** " + pairsOf.map(x => x.join(" + ")).join("; ") + ".\n\n";
for (const { g, parts } of groups) {
  md += "## " + g.name + "\n\n" + g.note + "\n\n";
  for (const p of parts) {
    const [w, h, shape] = D.sizes[p.size];
    md += "### " + p.file + ".png\n\n";
    md += "*" + w + " x " + h + " (" + shape + "), " + (p.kind === "T" ? "transparent" : p.kind === "O" ? "opaque" : "semi-transparent") + (p.tile !== "none" ? ", tiling" : "") + (p.pair ? ", paired with " + p.pair : "") + ". On screen: " + p.onscreen + ".*\n\n";
    md += p.prompt + (tileText[p.tile] ? " " + tileText[p.tile] : "") + "\n\n";
  }
}
fs.writeFileSync(path.join(root, "docs", "decal_brief.md"), md);

// ---------------------------------------------------------------- page
const esc = s => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
const data = D.parts.map(p => ({ file: p.file, prompt: fullPrompt(p) }));
let cards = "";
for (const { g, parts } of groups) {
  cards += '<section class="group" id="' + g.id + '"><h2>' + esc(g.name) + ' <span class="n">' + parts.length + '</span></h2><p class="gnote">' + esc(g.note) + '</p><div class="cards">';
  for (const p of parts) {
    const [w, h, shape] = D.sizes[p.size];
    const kind = p.kind === "T" ? "transparent" : p.kind === "O" ? "opaque" : "semi-transparent";
    cards += '<article class="card" data-file="' + p.file + '">' +
      '<header><label class="done"><input type="checkbox" id="done-' + p.file + '" data-done="' + p.file + '"><span class="fname">' + p.file + '.png</span></label></header>' +
      '<div class="tags"><span class="tag size">' + w + ' &times; ' + h + '</span><span class="tag ' + p.kind + '">' + kind + '</span>' +
      (p.tile !== "none" ? '<span class="tag tile">tiles</span>' : '') +
      (p.pair ? '<span class="tag pair">pair: ' + p.pair + '</span>' : '') + '</div>' +
      '<p class="prompt">' + esc(p.prompt) + '</p>' +
      (p.lettering ? '<p class="letter">Exact lettering: <b>' + esc(p.lettering) + '</b></p>' : '') +
      '<p class="screen">On screen: ' + esc(p.onscreen) + '</p>' +
      '<button type="button" class="copy" data-copy="' + p.file + '">Copy full prompt</button></article>';
  }
  cards += '</div></section>';
}

const html = `<title>Magneto Transport Decals</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;700&family=Atkinson+Hyperlegible:wght@400;700&family=JetBrains+Mono:wght@400;600&display=swap">
<style>
/* Layout: a single reading column of cards, grouped like the machine's own plates, on the deck's dark steel. */
:root{
  --bg:#141715; --plate:#1d211e; --plate2:#232824; --edge:#343a33; --fg:#dcd4bd; --muted:#9a937c;
  --brass:#c9b27a; --amber:#ffb340; --ok:#7fbf6a; --semi:#7aa8c9;
  --display:"Barlow Condensed","Arial Narrow",sans-serif; --body:"Atkinson Hyperlegible",system-ui,sans-serif; --mono:"JetBrains Mono",ui-monospace,Consolas,monospace;
  color-scheme:dark;
}
*{box-sizing:border-box}
body{background:var(--bg);color:var(--fg);font:16px/1.55 var(--body);margin:0;padding-block:28px 64px;padding-inline:max(16px,4vw)}
.wrap{max-width:980px;margin:0 auto;display:flex;flex-direction:column;gap:34px}
h1,h2{font-family:var(--display);color:var(--brass);letter-spacing:.04em;text-wrap:balance;margin:0}
h1{font-size:clamp(34px,6vw,54px);font-weight:700;line-height:1}
h2{font-size:28px;font-weight:700;display:flex;align-items:baseline;gap:10px}
h2 .n{font:600 14px var(--mono);color:var(--muted)}
.lede{max-width:68ch;color:var(--fg);margin:12px 0 0}
.sub{font:600 13px var(--mono);color:var(--muted);letter-spacing:.08em;text-transform:uppercase;margin:0 0 8px}
.how{background:var(--plate);border:1px solid var(--edge);border-radius:4px;padding:20px 22px}
.how ol{margin:10px 0 0;padding-left:22px;display:grid;gap:8px;max-width:72ch}
.style{background:var(--plate2);border-left:3px solid var(--brass);padding:14px 16px;margin:10px 0 12px;font-size:15px;max-width:76ch}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}
.sw{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}
.sw span{display:flex;align-items:center;gap:6px;font:12px var(--mono);color:var(--muted);background:var(--plate2);border:1px solid var(--edge);border-radius:3px;padding:3px 8px 3px 4px}
.sw i{width:16px;height:16px;border-radius:2px;border:1px solid #000;display:inline-block}
.progress{font:600 14px var(--mono);color:var(--amber)}
.gnote{color:var(--muted);max-width:72ch;margin:6px 0 14px}
.cards{display:grid;gap:14px}
.card{background:var(--plate);border:1px solid var(--edge);border-radius:4px;padding:16px 18px;display:flex;flex-direction:column;gap:10px;min-width:0}
.card.is-done{opacity:.55}
.done{display:flex;align-items:center;gap:10px;cursor:pointer}
.done input{width:18px;height:18px;accent-color:var(--ok)}
.fname{font:600 17px var(--mono);color:var(--fg);overflow-wrap:anywhere}
.tags{display:flex;flex-wrap:wrap;gap:6px}
.tag{font:600 12px var(--mono);padding:2px 8px;border-radius:3px;border:1px solid var(--edge);color:var(--muted)}
.tag.size{color:var(--fg)} .tag.T{color:var(--ok);border-color:#3c5a34} .tag.O{color:var(--brass);border-color:#5a5138} .tag.S{color:var(--semi);border-color:#34495a}
.tag.tile,.tag.pair{color:var(--amber);border-color:#6a4d1e}
.prompt{margin:0;max-width:78ch}
.letter{margin:0;color:var(--amber)}
.screen{margin:0;font:13px var(--mono);color:var(--muted)}
button{font:700 14px var(--body);background:var(--brass);color:#16130b;border:0;border-radius:3px;padding:8px 14px;cursor:pointer;align-self:flex-start}
button:hover{background:#dbc48c} button:focus-visible,input:focus-visible{outline:2px solid var(--amber);outline-offset:2px}
button.ghost{background:transparent;color:var(--brass);border:1px solid var(--brass)}
textarea.fallback{width:100%;min-height:180px;font:13px var(--mono);background:var(--plate2);color:var(--fg);border:1px solid var(--edge);border-radius:3px;padding:10px}
.toast{position:fixed;left:50%;bottom:calc(20px + env(safe-area-inset-bottom,0px));transform:translateX(-50%);background:var(--ok);color:#0e150b;font:700 14px var(--body);padding:8px 14px;border-radius:3px}
@media (prefers-reduced-motion:no-preference){.card{transition:opacity .2s}}
</style>
<div class="wrap">
  <header>
    <p class="sub">Brokild &middot; decal order &middot; ${count} parts</p>
    <h1>Magneto Transport Decals</h1>
    <p class="lede">Image parts for a software tape-echo instrument. Each part is made on its own and the program assembles them, so please never paint a whole panel. A program places every part at exact coordinates, which an image generator cannot hit. What matters is that each part looks right on its own canvas, at its stated size, with real transparency where asked.</p>
  </header>

  <section class="how" aria-labelledby="how-h">
    <h2 id="how-h">How to work</h2>
    <ol>
      <li>Make the parts one at a time, one image per request. Press <b>Copy full prompt</b> on a card and paste it into ChatGPT: it already contains the style paragraph, the size, the background and the palette.</li>
      <li>Use exactly the size on the card. Only three are used: 1024 &times; 1024, 1536 &times; 1024 and 1024 &times; 1536.</li>
      <li><b>Transparent means real alpha.</b> If an image comes back with a white, grey or checkerboard background where the card says transparent, ask for it again with a transparent background.</li>
      <li><b>Fill the canvas as described.</b> Earlier deliveries came back with the drawing in a thin strip of a larger canvas. That is a fail; make it again.</li>
      <li><b>Pairs must match.</b> For off/on and up/pressed pairs, make the second from the first, at the same size and position, so that switching between them moves only the light or the press.</li>
      <li>Save each image as PNG with the exact file name on the card, put them all in one zip, and add a short note listing anything that is not as asked. Tick a card when its part is done; the ticks are remembered in this browser.</li>
    </ol>
  </section>

  <section aria-labelledby="style-h">
    <h2 id="style-h">The style paragraph</h2>
    <p class="gnote">It starts every request. The copy buttons include it, so you only need this one if you write a request by hand.</p>
    <p class="style" id="styletext">${esc(D.style)}</p>
    <div class="row"><button type="button" class="ghost" data-copy="__style">Copy style paragraph</button><span class="progress" id="progress"></span></div>
    <div class="sw">${D.palette.map(p => '<span><i style="background:' + p[1] + '"></i>' + esc(p[0]) + ' ' + p[1] + '</span>').join("")}</div>
  </section>

  ${cards}
</div>
<div class="toast" id="toast" hidden>Copied</div>
<script>
const PROMPTS = ${JSON.stringify(Object.fromEntries(data.map(d => [d.file, d.prompt])))};
PROMPTS.__style = ${JSON.stringify(D.style)};
const toast = document.getElementById("toast");
let tt = 0;
function flash(msg){ toast.textContent = msg; toast.hidden = false; clearTimeout(tt); tt = setTimeout(() => { toast.hidden = true; }, 1600); }
function fallback(btn, text){
  let ta = btn.parentElement.querySelector("textarea.fallback");
  if (!ta){ ta = document.createElement("textarea"); ta.className = "fallback"; ta.readOnly = true; btn.insertAdjacentElement("afterend", ta); }
  ta.value = text; ta.focus(); ta.select();
  flash("Select-all is ready: press Ctrl+C");
}
document.addEventListener("click", e => {
  const b = e.target.closest("[data-copy]"); if (!b) return;
  const text = PROMPTS[b.dataset.copy];
  try {
    navigator.clipboard.writeText(text).then(() => flash("Copied: " + (b.dataset.copy === "__style" ? "style paragraph" : b.dataset.copy + ".png")), () => fallback(b, text));
  } catch (_) { fallback(b, text); }
});
const KEY = "mt-decals-done";
let done = {};
try { done = JSON.parse(localStorage.getItem(KEY) || "{}") || {}; } catch (_) { done = {}; }
function paint(){
  let n = 0;
  document.querySelectorAll("[data-done]").forEach(cb => {
    const on = !!done[cb.dataset.done]; cb.checked = on; if (on) n++;
    cb.closest(".card").classList.toggle("is-done", on);
  });
  document.getElementById("progress").textContent = n + " of ${count} done";
}
document.addEventListener("change", e => {
  const cb = e.target.closest("[data-done]"); if (!cb) return;
  done[cb.dataset.done] = cb.checked;
  try { localStorage.setItem(KEY, JSON.stringify(done)); } catch (_) {}
  paint();
});
paint();
</script>
`;
fs.writeFileSync(path.join(root, "docs", "decal_brief.html"), html);
console.log("decal_brief.md and decal_brief.html: " + count + " parts, " + pairsOf.length + " pairs");
