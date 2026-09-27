/*  initHints() was called from the boot line, which runs BEFORE the hints
    block is reached — and `const HINT` is in its temporal dead zone until
    then, so the call threw and took the whole script with it (window.__HT
    never existed). Function declarations hoist; const declarations do not.
    The call belongs at the end of the block that defines it. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const misses = [];
function rep(a, b) {
  const c = s.split(a).length - 1;
  if (c !== 1) { misses.push("expected 1 of " + JSON.stringify(a.slice(0, 70)) + ", found " + c); return; }
  s = s.split(a).join(b);
}
rep(`layout(); glInit(); buildRail(); paintPatch(); initHints();`,
    `layout(); glInit(); buildRail(); paintPatch();`);
rep(`  addEventListener("blur", tipHide);
}
`,
    `  addEventListener("blur", tipHide);
}
initHints();
`);
if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("hints boot fixed");
