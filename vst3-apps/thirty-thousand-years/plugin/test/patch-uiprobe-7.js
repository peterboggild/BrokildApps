/*  Probe round 7: the tooltip behaviour Peter asked for, as checks.
      - off on a fresh instance
      - the TOOLTIPS button says so and lights when they are on
      - hovering with them off shows nothing
      - holding CTRL shows one anyway, and releasing CTRL takes it away
      - the tip is not black (it was, and it was hard to see)
      - the tip never covers the control it explains
*/
const fs = require("fs");
const P = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
let s = fs.readFileSync(P, "utf8");

const a = `        /* ---- 5. hostParam moves the control, and is not echoed ------- */`;
const b = `        /* ---- 4b. tooltips: off by default, CTRL is the shortcut ------- */
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
        var bg = getComputedStyle(tip).backgroundImage + " " + getComputedStyle(tip).backgroundColor;
        ok("the tooltip is not a black rectangle", /gradient/.test(bg) || !/rgba?\(1?[0-9], ?1?[0-9], ?1?[0-9]/.test(bg), bg.slice(0, 90));
        document.dispatchEvent(new KeyboardEvent("keyup", { key: "Control", bubbles: true }));
        ok("releasing CTRL takes it away again", !tip.classList.contains("show"));
        click(tbtn);
        ok("the TOOLTIPS button lights when they are on", tbtn.classList.contains("on"));
        cc.dispatchEvent(new MouseEvent("mouseenter", { bubbles: false }));
        ok("...and then a hover does show one", tip.classList.contains("show"));
        click(tbtn);
        ok("switching them off again hides it", !tbtn.classList.contains("on") && !tip.classList.contains("show"));
        cc.dispatchEvent(new MouseEvent("mouseleave", { bubbles: false }));

        /* ---- 5. hostParam moves the control, and is not echoed ------- */`;

if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);
fs.writeFileSync(P, s);
console.log("probe patched");
