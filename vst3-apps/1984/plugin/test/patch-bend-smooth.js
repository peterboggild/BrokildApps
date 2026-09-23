// Bend, wheel and aftertouch smoothed at the CONTROL rate, not the block rate.
// Peter: "the bend wheel increases the notes in small steps". A one-pole run
// once per block jumps 35 % of the way every 5-10 ms: audible stairs. Now a
// buffer of per-control-tick values is filled once per block with an 8 ms
// time constant, and every voice reads the tick it is on.
"use strict";
const fs = require("fs");
const files = {};
function load(k, p) { const raw = fs.readFileSync(p, "utf8"); files[k] = { p, crlf: raw.indexOf("\r\n") >= 0, s: raw.replace(/\r\n/g, "\n"), n: 0 }; }
function edit(k, from, to, count = 1) { const f = files[k]; const parts = f.s.split(from); if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + k + " (" + (parts.length - 1) + "): " + from.slice(0, 120)); process.exit(1); } f.s = parts.join(to); f.n++; }
const root = "C:/Users/peter/b/Nineteen84/";
load("h", root + "Source/Engine.h"); load("c", root + "Source/Engine.cpp"); load("b", root + "test/bench.cpp");

edit("h", `    void controlTick (Voice& v, int vi);`, `    void controlTick (Voice& v, int vi, int tick);`);
edit("h", `    float bendSm = 0.0f, wheelSm = 0.0f, atSm = 0.0f;`,
`    float bendSm = 0.0f, wheelSm = 0.0f, atSm = 0.0f;
    std::vector<float> bendBuf, wheelBuf, atBuf;   // per control tick, filled once per block
    float perfK = 0.01f;                            // the 8 ms one-pole, per control tick`);

edit("c", `    osL.assign ((size_t) (maxBlock * MAX_OS) + 8, 0.0f);
    osR.assign ((size_t) (maxBlock * MAX_OS) + 8, 0.0f);`,
`    osL.assign ((size_t) (maxBlock * MAX_OS) + 8, 0.0f);
    osR.assign ((size_t) (maxBlock * MAX_OS) + 8, 0.0f);
    const size_t nTicks = (size_t) (maxBlock * MAX_OS / CTRL) + 4;
    bendBuf.assign (nTicks, 0.0f); wheelBuf.assign (nTicks, 0.0f); atBuf.assign (nTicks, 0.0f);`);
edit("c", `    lastOsParam = o;
    drive.prepare (fsOs);`,
`    lastOsParam = o;
    perfK = 1.0f - std::exp (-(float) CTRL / ((float) fsOs * 0.008f));   // 8 ms
    drive.prepare (fsOs);`);
edit("c", `void Engine::controlTick (Voice& v, int vi)
{`,
`void Engine::controlTick (Voice& v, int vi, int tick)
{
    const float bendSm = bendBuf[(size_t) tick], wheelSm = wheelBuf[(size_t) tick], atSm = atBuf[(size_t) tick];`);
edit("c", `        if ((v.ctrlPhase++ & (CTRL - 1)) == 0) controlTick (v, vi);`,
`        if ((v.ctrlPhase++ & (CTRL - 1)) == 0) controlTick (v, vi, i / CTRL);`);
edit("c", `    // performance controls, smoothed per block
    bendSm += (bendIn - bendSm) * 0.35f;
    wheelSm += (wheelIn - wheelSm) * 0.25f;
    atSm += (atIn - atSm) * 0.25f;`,
`    // performance controls, smoothed at the control rate (8 ms), one value per tick
    {
        const int nTicks = (n * osf) / CTRL + 1;
        for (int t = 0; t < nTicks; ++t)
        {
            bendSm += (bendIn - bendSm) * perfK;
            wheelSm += (wheelIn - wheelSm) * perfK;
            atSm += (atIn - atSm) * perfK;
            bendBuf[(size_t) t] = bendSm; wheelBuf[(size_t) t] = wheelSm; atBuf[(size_t) t] = atSm;
        }
    }`);

// bench: a bend step must glide, not stair
edit("b", `        {
            Rig a; a.set ("bend", 2.0f); a.e.noteOn (69, 0.8f); a.e.setBend (1.0f); a.render (48000);`,
`        {   // a bend step glides over ~8 ms and never stairs: the pitch per 2 ms window moves by less than 25 cents
            Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("bend", 12.0f); r.e.noteOn (69, 0.8f); r.render (9600);
            r.e.setBend (1.0f); r.renderAppend (9600);
            std::vector<double> track; const int hop = 96;
            for (int w = 9600 - 480; w + hop * 4 <= (int) r.L.size(); w += hop)
            {
                int zc = 0; double first = -1, last = -1;
                for (int i = w + 1; i < w + hop * 4; ++i)
                    if (r.L[(size_t) (i - 1)] <= 0 && r.L[(size_t) i] > 0) { const double t = i - r.L[(size_t) i] / (r.L[(size_t) i] - r.L[(size_t) (i - 1)]); if (first < 0) first = t; last = t; ++zc; }
                track.push_back (zc > 2 ? (zc - 1) / ((last - first) / r.fs) : (track.empty() ? 440.0 : track.back()));
            }
            double worstStep = 0; int t90 = -1;
            for (size_t i = 1; i < track.size(); ++i) worstStep = std::max (worstStep, std::abs (cents (track[i], track[i - 1])));
            for (size_t i = 0; i < track.size(); ++i) if (t90 < 0 && cents (track[i], 440.0) > 0.9 * 1200.0) t90 = (int) i;
            check (worstStep < 250.0 && t90 > 3 && t90 < 40, "bend step of an octave: glides (90 % within 8-80 ms), no stair over 250 cents per 2 ms", worstStep, t90 * 2.0);
        }
        {
            Rig a; a.set ("bend", 2.0f); a.e.noteOn (69, 0.8f); a.e.setBend (1.0f); a.render (48000);`);

for (const k in files) { const f = files[k]; fs.writeFileSync(f.p, f.crlf ? f.s.replace(/\n/g, "\r\n") : f.s); console.log(k + ": " + f.n + " edits"); }
