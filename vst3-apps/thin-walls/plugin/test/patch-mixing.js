// Mixing fix: Hadamard with a per-line sign flip (no longer an involution) and an
// output observer with its own sign pattern. Measured before: y under-read the
// stored energy by 5-8 dB and the door/diffuse renders inherited that.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " for " + a.slice(0, 60)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}
edit("Source/Engine.cpp", rep => {
  rep(`    static const float SGN_SRC[3][16] = {
        { 1,-1, 1, 1,-1, 1,-1,-1, 1,-1,-1, 1, 1,-1, 1,-1 },
        { 1, 1,-1, 1, 1,-1,-1, 1,-1, 1,-1,-1, 1,-1, 1, 1 },
        { -1,1, 1,-1, 1, 1,-1, 1,-1,-1, 1, 1,-1,-1, 1,-1 } };`,
`    static const float SGN_SRC[3][16] = {
        { 1,-1, 1, 1,-1, 1,-1,-1, 1,-1,-1, 1, 1,-1, 1,-1 },
        { 1, 1,-1, 1, 1,-1,-1, 1,-1, 1,-1,-1, 1,-1, 1, 1 },
        { -1,1, 1,-1, 1, 1,-1, 1,-1,-1, 1, 1,-1,-1, 1,-1 } };
    /*  A plain Hadamard is an involution (H/4 squared is the identity), so with
        equal-ish delays the state alternates between a spread pattern and a
        concentrated one and never mixes; the per-line sign flip breaks that.
        The observer has its own signs so it is not blind to the pattern the
        injection puts in. */
    static const float SGN_MIX[16] = { 1, 1,-1, 1,-1,-1, 1, 1,-1, 1, 1,-1, 1,-1,-1,-1 };
    static const float SGN_OUT[16] = { 1,-1,-1, 1, 1, 1,-1, 1,-1,-1, 1,-1, 1, 1,-1, 1 };`);
  rep(`                F.out[(size_t) k] = v; y += v;
            }
            F.y = y * 0.25f;`,
`                F.out[(size_t) k] = v; y += SGN_OUT[k] * v;
            }
            F.y = y * 0.25f;`);
  rep(`                F.line[(size_t) k].write (0.25f * v[k] + 0.25f * SGN_SRC[r][k] * srcIn);`,
      `                F.line[(size_t) k].write (0.25f * SGN_MIX[k] * v[k] + 0.25f * SGN_SRC[r][k] * srcIn);`);
});
if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("ok");
