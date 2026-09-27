/*  Bench for FMR buglist 1. The two failing checks describe behaviour we
    still want — running without a DAW — they simply now go through the
    free-run latch that the panel's transport raises. And the fix itself
    needs its own checks: a DAW that stops must stop the machine, and an
    armed machine must not free-run through a host that is merely present.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
const miss = [];
function sub(a, b, tag) {
  const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

sub(`        e.setTransport (90.0, -1.0, false);        // host has a tempo, is not rolling`,
`        e.setTransport (90.0, -1.0, false);        // host has a tempo, is not rolling
        e.setFreeRun (true);                       // ...and the player pressed RUN`, "stopped host");

sub(`        e.p.g[GP_TEMPO] = 0.5f;                    // the internal clock
        //  setTransport is deliberately never called`,
`        e.p.g[GP_TEMPO] = 0.5f;                    // the internal clock
        e.setFreeRun (true);                       // which is what RUN does
        //  setTransport is deliberately never called`, "no host");

//  the fix itself
const ANCHOR = "    //  the sequencer must be inert when it is switched off";
if (s.indexOf(ANCHOR) < 0) miss.push("anchor x0");
const NEW = [
"    /*  BUGLIST 1: the DAW is in charge. A machine that is ARMED must still",
"        stop when the transport stops — arming is an intent, not a clock —",
"        and the free-run latch must be dropped with it, so it does not carry",
"        on by itself afterwards. */",
"    {",
"        Engine e; fresh (e);",
"        seqOneChannel (e, 8, 16, 1, true);",
"        e.setTransport (120.0, 0.0, true);           // the DAW rolls",
"        { Buf b (24000); render (e, b);",
"          ok (onsets (b).size() > 0, \"a rolling DAW plays the armed machine\"); }",
"        e.setTransport (120.0, 2.0, false);          // the DAW stops",
"        ok (! e.isFreeRunning(), \"a stopping DAW did not drop the free-run latch\");",
"        { Buf b (48000); render (e, b);",
"          const int n = (int) onsets (b).size();",
'          std::printf ("    after the DAW stops: %d onsets\\n", n);',
"          ok (n == 0, \"the machine ran on after the DAW stopped\", (double) n, 0.0); }",
"    }",
"",
"    /*  ...and a host that merely EXISTS is not a clock: armed, host present,",
"        never rolling, no latch -> silence until the player presses RUN. */",
"    {",
"        Engine e; fresh (e);",
"        seqOneChannel (e, 8, 16, 1, true);",
"        e.setTransport (120.0, -1.0, false, true);   // present, not rolling",
"        { Buf b (48000); render (e, b);",
"          ok (onsets (b).size() == 0, \"armed alone started the machine\"); }",
"        e.setFreeRun (true);                         // the player presses RUN",
"        { Buf b (48000); render (e, b);",
"          ok (onsets (b).size() > 0, \"RUN did not start it without the DAW\"); }",
"    }",
"",
""].join(NL);
if (!miss.length) s = s.replace(ANCHOR, NEW + ANCHOR);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench: the latch in the two old checks, plus the DAW-is-in-charge rule");
