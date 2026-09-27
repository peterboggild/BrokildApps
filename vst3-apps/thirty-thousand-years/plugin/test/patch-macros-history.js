/*  Three faults Peter found by looking at the thing, and one I found while
    measuring them.

    1. THE MACRO ROW WAS SILENTLY SHRINKING ITS OWN CHILDREN. The cell gives
       84 px of content box to children that declare 12 + 52 + 18 + 8 = 90, and
       `.mac` is a flex column, so flex did what flex does: it shrank them. The
       NAME was rendering at 8 px of its declared 12 (measured), and the value
       readout was clipped by the bottom of the knob's own 52 px box - which is
       exactly "blocking the text and number values".

       Fixed by making the arithmetic fit instead of leaving it to flex:
         * `.mac > *{flex:none}` - nothing in a cell may be squeezed. This is
           the actual bug; the rest is finding the room.
         * the gloss line drops from a two-line box to one. Measured every
           macro's gloss on one line: 55-99 px against 159 px available, so the
           second line was never used and was 9 px of pure waste.
         * the control box grows 52 -> 56, which is the knob (44) plus the
           value line (11) plus a pixel, so the value can no longer be clipped.
         * the row grows 86 -> 96 out of the deck's own spare 12 px (measured:
           children 828 + gaps 32 + padding 28 = 888 in a 900 px deck), so
           nothing else on the deck moves.
       Cell budget after: 89 px of box against 85 px of children.

    2. THE HEADER OVERFLOWED BY 12 PX, and I put it there. `#hbtns` stacks two
       22 px button rows, but the QUALITY control is a generic `.ctl` and those
       are 72 px tall - so the block wanted 98 px in a 74 px header and its top
       row was drawn 12 px ABOVE the header's own box. A header control gets a
       header-sized box.

    3. HISTORY LOOKED DEAD, AND ON EVERY FACTORY PATCH IT WAS. The mechanism
       works - measured end to end in the running plugin, four scenes
       interpolating 0.000 -> 0.119 -> 0.249 -> 0.150 on the feedback send -
       but `h_on` defaults OFF, no preset ever armed it, and nothing on the
       panel said so. A control that is inert for a reason has to give the
       reason; "four scenes, one journey" is a promise, not a status.
*/
const fs = require ("fs");
const F = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync (F, "utf8");

const es = [
  /* ---- 1. the macro row ------------------------------------------------ */
  [`#macros{flex:none;height:86px;display:flex;gap:4px;align-items:stretch}
.mac{flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;
  border:1px solid var(--hair2);border-radius:2px;padding:2px 3px 3px;
  background:linear-gradient(180deg,#1a1d22,#111418);overflow:hidden}`,
   `#macros{flex:none;height:96px;display:flex;gap:4px;align-items:stretch}
.mac{flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;
  border:1px solid var(--hair2);border-radius:2px;padding:2px 3px 3px;
  background:linear-gradient(180deg,#1a1d22,#111418);overflow:hidden}
/*  Nothing in a macro cell may be squeezed. Without this, flex shrank the
    NAME from its declared 12 px to 8 and pushed the value under the knob. */
.mac > *{flex:none}`],

  [`.mac .ms{font:400 6.8px/9px var(--sans);color:var(--bone3);text-align:center;width:100%;
  overflow:hidden;height:18px;letter-spacing:.02em}
.mac .ctl{height:52px}`,
   `/*  One line, because one line is all any of them needs: measured, the eight
    glosses are 55-99 px wide against 159 px of cell. */
.mac .ms{font:400 6.8px/9px var(--sans);color:var(--bone3);text-align:center;width:100%;
  overflow:hidden;height:9px;white-space:nowrap;text-overflow:ellipsis;letter-spacing:.02em}
/*  44 px knob + 11 px value + 1. At 52 the value was clipped by its own box. */
.mac .ctl{height:56px}`],

  /* ---- 2. the header --------------------------------------------------- */
  [`#hbtns{flex:none;display:flex;flex-direction:column;justify-content:center;gap:4px;align-items:flex-end}`,
   `#hbtns{flex:none;display:flex;flex-direction:column;justify-content:center;gap:4px;align-items:flex-end}
/*  A generic .ctl is 72 px, and two rows of those do not fit a 74 px header -
    the top row was being drawn above the header's own box. */
#hbtns .ctl{height:46px}
#hbtns .ctl .kn{width:26px;height:26px}`],

  /* ---- 3. the HISTORY status line -------------------------------------- */
  [`.mac .nas{font:400 6.5px/8px var(--mono);color:var(--cyandim);letter-spacing:.04em}`,
   `.mac .nas{font:400 6.5px/8px var(--mono);color:var(--cyandim);letter-spacing:.04em}
/*  Why the journey is not moving. Amber when something needs doing, dim when
    it is simply under way. */
.hstat{font:400 8px/11px var(--mono);letter-spacing:.03em;color:var(--bone3);
  margin-top:3px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.hstat.todo{color:var(--amber)}`],

  [`SP.hsc = function (p) {`,
   `/*  Every HISTORY panel carries one of these and syncHist fills them all. */
var HSTAT = [];
SP.hsc = function (p) {`],

  [`  cw._row = el("div", "col", cw); cw._row.style.gap = "2px"; cw._want = TRAVELCH; PANELS.push(cw);
};`,
   `  cw._row = el("div", "col", cw); cw._row.style.gap = "2px"; cw._want = TRAVELCH; PANELS.push(cw);
  HSTAT.push (el ("div", "hstat", p._row.parentNode, ""));
};`],

  [`  HTL = el("canvas", "scr", p._row.parentNode); HTL.width = 660; HTL.height = 38;
  HTL.style.cssText = "width:100%;height:38px;margin-top:4px";
};`,
   `  HTL = el("canvas", "scr", p._row.parentNode); HTL.width = 660; HTL.height = 38;
  HTL.style.cssText = "width:100%;height:38px;margin-top:4px";
  HSTAT.push (el ("div", "hstat", p._row.parentNode, ""));
};`],

  [`  [SREC, SBTN].forEach(function (arr) { arr.forEach(function (s, i) { s.t.classList.toggle("here", i === here && V.h_on >= 0.5); }); });
  drawTravel(pos, sp);
}`,
   `  [SREC, SBTN].forEach(function (arr) { arr.forEach(function (s, i) { s.t.classList.toggle("here", i === here && V.h_on >= 0.5); }); });
  drawTravel(pos, sp);
  /*  Say why nothing is happening. HISTORY is off by default and needs two
      different scenes, and neither fact is visible from a slider that moves
      and changes nothing. */
  var nset = 0; for (var k = 0; k < 4; k++) if (SCENES.set[k]) nset++;
  var on = V.h_on >= 0.5, todo = true, msg;
  if (nset === 0)      msg = "NO SCENES STORED  ·  set the instrument up, then STORE it into a scene";
  else if (nset === 1) msg = "ONE SCENE STORED  ·  HISTORY needs two — change something and store another";
  else if (! on)       msg = nset + " SCENES STORED  ·  HISTORY is OFF — switch it on and this slider travels between them";
  else if ((V.h_mode || 0) < 0.5) { msg = "TRAVELLING BY HAND  ·  drag HISTORY, or set DRIVE to AUTO to let it run"; todo = false; }
  else                 { msg = "RUNNING  ·  " + fmt("h_mode", V.h_mode) + " over " + fmt("h_dur", V.h_dur) + ", " + fmt("h_loop", V.h_loop) + " at the end"; todo = false; }
  HSTAT.forEach(function (e) { e.textContent = msg; e.classList.toggle("todo", todo); });
}`],
];

let miss = [];
for (const [a] of es) { const n = s.split (a).length - 1; if (n !== 1) miss.push (n + "x: " + a.slice (0, 64).replace (/\n/g, " ")); }
if (miss.length) { console.log ("ANCHOR MISS:\n" + miss.join ("\n")); process.exit (1); }
for (const [a, b] of es) s = s.replace (a, b);
fs.writeFileSync (F, s);
console.log ("panel patched: macro row, header, HISTORY status");
