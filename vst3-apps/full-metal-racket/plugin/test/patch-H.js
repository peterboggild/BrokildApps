// THE TEMPO BUG, and the round-2 decals.
//
// The sequencer took its tempo from the host only while the host was PLAYING.
// With the transport stopped it fell back to the internal TEMPO knob — which
// defaults to about 72 BPM — so in a DAW it ran at the wrong speed until you
// pressed play, and reads exactly as "it ignores the DAW tempo".
//
// The fallback exists for the standalone, where there is no host clock at all.
// The fix is to separate the two things it was conflating: the host's TEMPO is
// used whenever the host has one, and only the host's POSITION depends on the
// transport actually running.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

// ── the tempo bug ───────────────────────────────────────────────────────────
const wh = edit(R + "Source/Engine.h", [
[`    double bpmNow = 120.0, ppqNow = 0.0, ppqFree = 0.0;
    bool   playing = false;`,
`    double bpmNow = 120.0, ppqNow = 0.0, ppqFree = 0.0;
    bool   playing = false;
    /*  Separate from the playing flag on purpose. A host has a tempo whether or not
        its transport is rolling, and the sequencer should use it either way —
        conflating the two made the machine run at its internal tempo until
        you pressed play. */
    bool   hostBpm = false;`, "hostBpm"]
]);

const wc = edit(R + "Source/Engine.cpp", [
[`void Engine::setTransport (double bpm, double ppq, bool play)
{
    if (bpm > 20.0 && bpm < 999.0) bpmNow = bpm;`,
`void Engine::setTransport (double bpm, double ppq, bool play)
{
    if (bpm > 20.0 && bpm < 999.0) { bpmNow = bpm; hostBpm = true; }`, "settransport"],

[`    /*  With no host transport — the standalone, or a stopped DAW — the
        sequencer runs on its own clock so the machine is playable on its own.
        With one, the host's position wins outright. */
    const double bpm = playing ? bpmNow : (double) xmap (p.g[GP_TEMPO], 40.0f, 300.0f);`,
`    /*  The host's TEMPO is used whenever the host has one, playing or not, so
        a stopped DAW does not silently hand the machine back to its own clock.
        The internal tempo is for the standalone, where there is no host at
        all. The host's POSITION is a separate question, below. */
    const double bpm = hostBpm ? bpmNow : (double) xmap (p.g[GP_TEMPO], 40.0f, 300.0f);`, "bpm source"],

[`    if (playing) { a = ppqNow; b = ppqNow + beats; }
    else         { a = ppqFree; b = ppqFree + beats; ppqFree = b; }`,
`    //  rolling: locked to the bar. Stopped: free-running at the same tempo,
    //  so the machine can be auditioned without arming the transport.
    if (playing) { a = ppqNow; b = ppqNow + beats; }
    else         { a = ppqFree; b = ppqFree + beats; ppqFree = b; }`, "position"]
]);

// ── the round-2 decals ──────────────────────────────────────────────────────
const wi = edit(R + "tools/ingest-decals.ps1", [
[`  "fmr-screw"        = 1.0
}`,
`  "fmr-screw"        = 1.0
  "fmr-step"         = 144.0 / 132.0
  "fmr-step-on"      = 144.0 / 132.0
  "fmr-step-acc"     = 144.0 / 132.0
}`, "step aspect"],

[`  "fmr-screw"       = 96
}`,
`  "fmr-screw"       = 96
  "fmr-step"        = 144
  "fmr-step-on"     = 144
  "fmr-step-acc"    = 144
}`, "step maxdim"],

// pairs become groups: the step button has three states, not two
[`# parts whose two states must stay registered with one another
$PAIRS  = @{ "fmr-key" = "fmr-key-lit"; "fmr-keydark" = "fmr-keydark-lit"; "fmr-led" = "fmr-led-lit" }`,
`# sets of states that must stay registered with one another. Not pairs — the
# step button has three, and trimming any of them independently shifts it
# against the others so the button appears to twitch as it lights.
$GROUPS = @(
  @("fmr-key", "fmr-key-lit"),
  @("fmr-keydark", "fmr-keydark-lit"),
  @("fmr-led", "fmr-led-lit"),
  @("fmr-step", "fmr-step-on", "fmr-step-acc")
)`, "groups"],

[`    foreach ($a in $PAIRS.Keys) {
      $b = $PAIRS[$a]
      if ($boxes.ContainsKey($a) -and $boxes.ContainsKey($b)) {
        # union in CROP-RELATIVE coordinates: the same object seen in two
        # frames, wherever those frames happen to sit on the sheet
        $rx = [Math]::Min($boxes[$a].rx, $boxes[$b].rx)
        $ry = [Math]::Min($boxes[$a].ry, $boxes[$b].ry)
        $rw = [Math]::Max($boxes[$a].rx + $boxes[$a].w, $boxes[$b].rx + $boxes[$b].w) - $rx
        $rh = [Math]::Max($boxes[$a].ry + $boxes[$a].h, $boxes[$b].ry + $boxes[$b].h) - $ry
        foreach ($k in @($a, $b)) {
          $boxes[$k] = @{ x = ($boxes[$k].ox + $rx); y = ($boxes[$k].oy + $ry); w = $rw; h = $rh
                          ox = $boxes[$k].ox; oy = $boxes[$k].oy; rx = $rx; ry = $ry }
        }
        Write-Output ("  {0} / {1} registered against each other ({2} x {3})" -f $a, $b, $rw, $rh)
      }
    }`,
`    foreach ($grp in $GROUPS) {
      $have = @($grp | Where-Object { $boxes.ContainsKey($_) })
      if ($have.Count -lt 2) { continue }
      # union in CROP-RELATIVE coordinates: the same object seen in several
      # frames, wherever those frames happen to sit on the sheet
      $rx = ($have | ForEach-Object { $boxes[$_].rx } | Measure-Object -Minimum).Minimum
      $ry = ($have | ForEach-Object { $boxes[$_].ry } | Measure-Object -Minimum).Minimum
      $rw = ($have | ForEach-Object { $boxes[$_].rx + $boxes[$_].w } | Measure-Object -Maximum).Maximum - $rx
      $rh = ($have | ForEach-Object { $boxes[$_].ry + $boxes[$_].h } | Measure-Object -Maximum).Maximum - $ry
      foreach ($k in $have) {
        $boxes[$k] = @{ x = ($boxes[$k].ox + $rx); y = ($boxes[$k].oy + $ry); w = $rw; h = $rh
                        ox = $boxes[$k].ox; oy = $boxes[$k].oy; rx = $rx; ry = $ry }
      }
      Write-Output ("  {0} registered against each other ({1} x {2})" -f ($have -join " / "), $rw, $rh)
    }`, "group union"]
]);

const wu = edit(R + "Source/ui/ui.html", [
[`    ["fmr-nameplate.png", u => {`,
`    ["fmr-step", u => {
      add('.cell{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important;box-shadow:none!important}');
      //  the downbeat of each group of four stays a shade brighter, which is
      //  most of how a step grid stays readable at a glance
      add('.cell:nth-child(4n+1){filter:brightness(1.11)}');
    }],
    ["fmr-step-on", u => {
      add('.cell.on{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important}');
    }],
    ["fmr-step-acc", u => {
      add('.cell.acc{background:url("' + u + '") center/100% 100% no-repeat!important;border:none!important}');
    }],
    ["fmr-nameplate.png", u => {`, "step css"],
[`  let found = 0;`, `  let found = 0;   // 20 parts now: the three step states joined the set`, "count note"]
]);

// ── a bench check for the tempo bug, so it cannot come back ─────────────────
const wt = edit(R + "test/test.cpp", [
[`    //  the sequencer must be inert when it is switched off`,
`    /*  THE HOST'S TEMPO IS USED EVEN WHEN THE TRANSPORT IS STOPPED. It used
        to fall back to the internal tempo knob the moment the DAW stopped
        rolling, which in a DAW is most of the time you are editing. */
    {
        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_TEMPO] = 0.0f;                    // internal clock at its slowest
        e.setTransport (90.0, -1.0, false);        // host has a tempo, is not rolling
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        if (on.size() >= 3)
        {
            const double got = (double) (on[2] - on[1]);
            const double want = 48000.0 * 60.0 / 90.0 / 4.0;      // a 16th at 90 BPM
            std::printf ("    stopped host at 90 BPM: %.0f samples between steps (want %.0f)\\n", got, want);
            ok (std::fabs (got - want) <= 3.0, "a stopped host still sets the tempo", got, want);
        }
        else ok (false, "the sequencer runs with the host stopped", (double) on.size(), 3);
    }

    //  the sequencer must be inert when it is switched off`, "tempo check"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wh(); wc(); wi(); wu(); wt();
console.log("tempo fix + round-2 decals wired");
