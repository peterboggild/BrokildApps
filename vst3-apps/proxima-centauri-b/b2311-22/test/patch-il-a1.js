/*  THE INTERLOCUTOR, phase A1 — a body whose graph can CHANGE.

    Plasticity ("speech is surgery") needs to re-solve a specimen after its
    edge weights have been altered by what it heard. Today the whole
    derivation lives inside generateSpecimen's salt loop and is unreachable
    from outside. Extract it:

        deriveFromGraph (g, out)   everything after buildGraph
        generateSpecimen           salt loop: build graph, draw the three
                                   generator constants, derive, test the floor
        resolveSpecimen (io)       rebuild G from io's own edges and re-derive

    The three r.uni() draws (gamma, wantTop, revivalSeconds) move INTO the
    Specimen so a re-solve reproduces the same register and spread — an
    accent must change the anatomy, not the creature's whole scale. They are
    hoisted in their original order, so the Rng sequence is untouched and all
    300 catalog entries stay bit-identical (the bench's alienness floor and
    max-salt numbers are the proof).

    Also: voiceHz — every specimen now has a natural speaking register,
    derived from its catalog number alone. The interlocutor's ladder must sit
    at FIXED frequencies, or "kin converse, strangers are deaf" is vacuous:
    if the second body simply followed your note, overlap would be perfect
    for every pair and mutual intelligibility would mean nothing.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);

const P = "C:/Users/peter/b/ArtefactB2311/Source/Specimen.cpp";
const H = "C:/Users/peter/b/ArtefactB2311/Source/Specimen.h";

/* ── the header ──────────────────────────────────────────────────────── */
{
  let s = fs.readFileSync(H, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  const subs = [
    [`    float eigenResidual = 0.0f;    // max |A v - lambda v| over checked modes (bench)`,
`    float eigenResidual = 0.0f;    // max |A v - lambda v| over checked modes (bench)

    /*  the generator's own constants, kept so a RE-SOLVE of a changed graph
        reproduces the same register and spread: an accent alters anatomy,
        never the creature's scale */
    float genGamma   = 1.0f;       // spectrum stretch exponent
    float genWantTop = 12.0f;      // where the top mode should land

    /*  the specimen's natural speaking register. Fixed per catalog number,
        independent of any note you play — without this, two bodies would
        always share a ladder and "mutual intelligibility" would be a
        tautology instead of a measurable property of the pair. */
    float voiceHz = 96.0f;

    //  how far this body has been remodelled by listening (0 = catalog pure)
    float accentDepth = 0.0f;`, "spec fields"],

    [`//  Generate catalog entry \`num\` (0..kCatalog-1). Deterministic: the same
//  number is the same being on any machine, forever.
void generateSpecimen (int num, Specimen& out);`,
`//  Generate catalog entry \`num\` (0..kCatalog-1). Deterministic: the same
//  number is the same being on any machine, forever.
void generateSpecimen (int num, Specimen& out);

/*  Re-solve a specimen whose edge weights have been altered (plasticity).
    The graph is taken from the specimen itself; everything downstream —
    eigenmodes, ratios, the 4D body, coupling, path, ladder — is recomputed.
    The generator constants and voiceHz are preserved, so the creature keeps
    its scale and register while its anatomy, and therefore its voice,
    genuinely changes. */
void resolveSpecimen (Specimen& io);

/*  Mutual intelligibility of two bodies, in [0,1]: how much of one's
    ladder the other owns a mode for, at their own speaking registers.
    Symmetric, cheap, and the honest basis for "kin converse". */
float spectralKinship (const Specimen& a, const Specimen& b);`, "decls"]
  ];
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { console.error("ABORT header: " + tag + " x" + n); process.exit(1); }
    s = s.split(A).join(B);
  }
  fs.writeFileSync(H, s);
}

/* ── the implementation: extract deriveFromGraph ─────────────────────── */
{
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;

  const START = ["        const int n = g.n;",
                 "        out.nNodes = n;"].join(NL);
  const END   = "        //  THE FLOOR: no specimen ships near a harmonic series";
  const i = s.indexOf(START);
  const j = s.indexOf(END);
  if (i < 0 || j < 0 || j < i) { console.error("ABORT: extraction bounds"); process.exit(1); }

  let body = s.slice(i, j);

  //  the three generator draws become fields (order preserved -> same Rng)
  const draws = [
    ["        const float gamma = 0.85f + 0.45f * r.uni();",
     "        const float gamma = out.genGamma;"],
    ["        const float wantTop = 7.0f + 19.0f * r.uni();",
     "        const float wantTop = out.genWantTop;"],
    ["            out.revivalSeconds = 1.5f + 2.5f * r.uni();",
     "            //  (drawn by the generator; preserved across a re-solve)"]
  ];
  for (const [a, b] of draws) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = body.split(A).length - 1;
    if (n !== 1) { console.error("ABORT: draw x" + n + " -> " + a.trim()); process.exit(1); }
    body = body.split(A).join(B);
  }

  //  dedent one level (it was inside the salt loop)
  body = body.split(NL).map(l => l.startsWith("    ") ? l.slice(4) : l).join(NL);

  const fn = [
    "/*  Everything downstream of the graph: eigenmodes, spectrum, the 4D body,",
    "    ear weights, coupling, excitation path, ladder. Split out of",
    "    generateSpecimen so a body whose edges have been REMODELLED by",
    "    listening can be re-solved without being reinvented. */",
    "static void deriveFromGraph (const G& g, Specimen& out)",
    "{",
    body.replace(new RegExp(NL + "+$"), ""),
    "}",
    "",
    ""].join(NL);

  //  splice: function before generateSpecimen, call inside the loop
  const GEN = "void generateSpecimen (int num, Specimen& out)";
  const gi = s.indexOf(GEN);
  if (gi < 0) { console.error("ABORT: generateSpecimen"); process.exit(1); }

  const call = [
    "        /*  the generator's three constants, drawn HERE in their original",
    "            order so the Rng sequence — and therefore all 300 catalog",
    "            entries — is bit-identical to before the refactor */",
    "        out.genGamma       = 0.85f + 0.45f * r.uni();",
    "        out.genWantTop     = 7.0f + 19.0f * r.uni();",
    "        out.revivalSeconds = 1.5f + 2.5f * r.uni();",
    "",
    "        /*  the natural speaking register: fixed per catalog number, so",
    "            two bodies' ladders genuinely may or may not overlap */",
    "        {",
    "            uint64_t h = (uint64_t) num * 0x9E3779B97f4A7C15ull + 0x2311ull;",
    "            h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;",
    "            const float u = (float) ((h >> 40) & 0xFFFF) / 65535.0f;",
    "            out.voiceHz = 58.0f * std::pow (2.0f, 1.75f * u);   // 58..195 Hz",
    "        }",
    "",
    "        deriveFromGraph (g, out);",
    ""].join(NL);

  s = s.slice(0, i) + call + s.slice(j);
  //  insert the extracted function ahead of generateSpecimen (index shifted)
  const gi2 = s.indexOf(GEN);
  s = s.slice(0, gi2) + fn + s.slice(gi2);

  //  ── the two new public functions ──────────────────────────────────
  const TAIL = "void fieldPosition (const Specimen& s, float& fx, float& fy)";
  const ti = s.indexOf(TAIL);
  if (ti < 0) { console.error("ABORT: fieldPosition"); process.exit(1); }
  const extra = [
    "void resolveSpecimen (Specimen& io)",
    "{",
    "    //  the graph is the specimen's own, edge weights and all: rebuilding",
    "    //  G from it and re-deriving is exactly 'this body, changed'",
    "    G g;",
    "    g.n = io.nNodes;",
    "    g.ne = io.nEdges;",
    "    for (int e = 0; e < io.nEdges; ++e)",
    "    {",
    "        g.ea[e] = io.edgeA[e];",
    "        g.eb[e] = io.edgeB[e];",
    "        g.ew[e] = io.edgeW[e];",
    "    }",
    "    deriveFromGraph (g, io);",
    "    io.harmonicMarginCents = harmonicMargin (io.ratio.data(), io.nModes);",
    "}",
    "",
    "float spectralKinship (const Specimen& a, const Specimen& b)",
    "{",
    "    /*  How much of b's ladder a can physically answer: for every partial",
    "        of b, the closest partial of a in cents, folded through a",
    "        tolerance the resonators actually have. Both ladders are taken at",
    "        their own speaking registers — that is the whole point. */",
    "    if (a.nModes <= 0 || b.nModes <= 0) return 0.0f;",
    "    double acc = 0;",
    "    for (int j = 0; j < b.nModes; ++j)",
    "    {",
    "        const float fb = b.voiceHz * b.ratio[j];",
    "        if (fb < 20.0f || fb > 12000.0f) continue;",
    "        float best = 1e9f;",
    "        for (int i = 0; i < a.nModes; ++i)",
    "        {",
    "            const float fa = a.voiceHz * a.ratio[i];",
    "            const float c = std::fabs (1200.0f * std::log2 (fa / fb));",
    "            if (c < best) best = c;",
    "        }",
    "        //  ~70 cents is the half-power width of a Q-25 resonator",
    "        acc += std::exp (-(double) best * best / (2.0 * 70.0 * 70.0));",
    "    }",
    "    return (float) (acc / (double) b.nModes);",
    "}",
    "",
    ""].join(NL);
  s = s.slice(0, ti) + extra + s.slice(ti);

  fs.writeFileSync(P, s);
}
console.log("A1: deriveFromGraph extracted; resolveSpecimen + spectralKinship + voiceHz in");
