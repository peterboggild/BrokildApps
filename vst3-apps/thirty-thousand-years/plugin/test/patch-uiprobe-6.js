/*  Probe round 6 — the gap that let three faults through.

    The overflow check only ever looked at panels INSIDE the active view, so
    the header, the foot and the macro rail were never measured. The foot's
    DRONE panel was clipping its last control the whole time and the probe
    said the panel layout was clean. (Same family as the manual's folio gate,
    which exempted exactly the elements the faults were in.)

    Also added: the title must fit its own block, and the spectral record must
    be drawn at the size it is shown rather than stretched.
*/
const fs = require("fs");
const P = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
let s = fs.readFileSync(P, "utf8");

const edits = [
  [`                page.querySelectorAll(".panel").forEach(function (p) {
                  if (p.scrollHeight > p.clientHeight + 2 && getComputedStyle(p).overflowY !== "auto") over.push(v.join("/") + " " + (p.dataset.panel || "?") + " h" + p.scrollHeight + ">" + p.clientHeight);
                  if (p.scrollWidth > p.clientWidth + 2 && getComputedStyle(p).overflowX !== "auto") over.push(v.join("/") + " " + (p.dataset.panel || "?") + " w" + p.scrollWidth + ">" + p.clientWidth);
                });`,
   `                /*  The strips are measured too: the foot's DRONE panel was
                    clipping a control while this check looked only at views. */
                var boxes = Array.prototype.slice.call(page.querySelectorAll(".panel"))
                  .concat(Array.prototype.slice.call(document.querySelectorAll("#head .panel, #foot .panel, #macros .mac, .scene")));
                boxes.forEach(function (p) {
                  if (!p.clientHeight && !p.clientWidth) return;
                  var cs = getComputedStyle(p);
                  var where = (p.dataset.panel || p.className || "?");
                  if (p.scrollHeight > p.clientHeight + 2 && cs.overflowY !== "auto") over.push(v.join("/") + " " + where + " h" + p.scrollHeight + ">" + p.clientHeight);
                  if (p.scrollWidth > p.clientWidth + 2 && cs.overflowX !== "auto") over.push(v.join("/") + " " + where + " w" + p.scrollWidth + ">" + p.clientWidth);
                });`],
  [`            var deck = document.querySelector("#deck");
            ok("the deck is 1440 x 900 and does not scroll",
               deck.scrollWidth <= 1442 && deck.scrollHeight <= 902,
               deck.scrollWidth + "x" + deck.scrollHeight);`,
   `            var deck = document.querySelector("#deck");
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
            /*  The record is drawn at the size it is shown, not stretched. */
            var rec = document.querySelector("#rec");
            ok("the spectral record is drawn at its displayed size",
               rec && Math.abs(rec.width - rec.clientWidth) <= 8 && Math.abs(rec.height - rec.clientHeight) <= 8,
               rec ? rec.width + "x" + rec.height + " bitmap in " + rec.clientWidth + "x" + rec.clientHeight + " box" : "no canvas");`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(P, s);
console.log("probe patched (" + edits.length + " edits)");
