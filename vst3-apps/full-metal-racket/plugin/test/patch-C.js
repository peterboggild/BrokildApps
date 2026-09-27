// CHUNK C — the kit library moves into Kits.cpp: two hundred seeds, category
// first. The nine handmade kits go, because a generated library that the bench
// can prove distinct is worth more than nine that it cannot.
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

const writeH = edit(R + "Source/Engine.h", [
[`int         numKits();
const char* kitName (int i);
void        applyKit (int i, Params& p);`,
`//  The kit library — two hundred seeds, in Kits.cpp. Category is decided
//  first from a bijective permutation, THEN parameters are generated to fit,
//  so a kit's name always describes its sound by construction.
int         numKits();
const char* kitName (int i);
void        applyKit (int i, Params& p);
int         numSeeds();
int         numSeedCategories();
const char* seedCategoryName (int c);
int         seedCategory (int seed);
const char* seedName (int seed);
void        applySeed (int seed, Params& p);`, "kit api"]
]);

let cpp = fs.readFileSync(R + "Source/Engine.cpp", "utf8");
const a = cpp.indexOf("//==============================================================================\n//  A handful of kits for the prototype.");
const b = cpp.indexOf("} // namespace fmr");
if (a < 0 || b < 0 || b <= a) { miss.push("Engine.cpp: kit block not found"); }
else cpp = cpp.slice(0, a) + cpp.slice(b);

const writeC = () => fs.writeFileSync(R + "Source/Engine.cpp", cpp);

const writeM = edit(R + "CMakeLists.txt", [
[`        Source/Engine.cpp
        Source/PluginProcessor.cpp`,
 `        Source/Engine.cpp
        Source/Kits.cpp
        Source/PluginProcessor.cpp`, "cmake sources"]
]);

const writeT = edit(R + "test/CMakeLists.txt", [
[`add_executable(fmrtest test.cpp ../Source/Engine.cpp)`,
 `add_executable(fmrtest test.cpp ../Source/Engine.cpp ../Source/Kits.cpp)`, "test sources"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeH(); writeC(); writeM(); writeT();
console.log("chunk C patched OK — 200 seed kits");
