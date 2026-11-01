/* Auto-flight: computes a camera path that skims the fractal surface without touching it.

   The path is computed ONCE and stored in project.flight.track; the evaluator only plays it back,
   so the preview and the movie render show the same flight. The simulator asks a probe (the GPU in
   the app, an exact distance function in the tests) three things about the camera position at each
   step: how far the surface is, which way is "away from the surface" (the gradient), and how far each
   of 25 look-ahead rays gets before it hits something. Like a drone pilot it then
   - holds a chosen height above the surface (closer -> climb, higher -> descend),
   - follows the surface sideways rather than ramming it,
   - steers toward the most open look-ahead ray when the way ahead closes in,
   - wanders, banks into turns and limits how fast the heading may change,
   - never moves more than half the free distance in one step, so it cannot pass through a wall.
   The shape is evaluated at every step with the whole timeline (transitions, keys, music routes),
   so the flight reacts to the shape as it actually is at that moment. */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory(require('./evaluator.js'));
  else root.FV = Object.assign(root.FV || {}, { Flight: factory(root.FVEvaluator) });
})(typeof self !== 'undefined' ? self : globalThis, function (Ev) {
  'use strict';

  const DEG = Math.PI / 180;
  const SIM_HZ = 30;      // simulation steps per second
  const STEP_BEATS = 0.25; // stored samples per beat: 4
  const clamp = (x, a, b) => Math.min(b, Math.max(a, x));
  const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const mul = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const len = (a) => Math.hypot(a[0], a[1], a[2]);
  const norm = (a, fb) => { const l = len(a); return l > 1e-9 ? mul(a, 1 / l) : (fb || [0, 1, 0]); };
  const mix = (a, b, t) => add(mul(a, 1 - t), mul(b, t));
  // Rotate v about unit axis k by angle a (Rodrigues).
  function rot(v, k, a) {
    const c = Math.cos(a), s = Math.sin(a);
    return add(add(mul(v, c), mul(cross(k, v), s)), mul(k, dot(k, v) * (1 - c)));
  }
  // Turn unit vector a toward unit vector b by at most maxA radians.
  function turnToward(a, b, maxA) {
    const c = clamp(dot(a, b), -1, 1);
    const ang = Math.acos(c);
    if (ang <= maxA || ang < 1e-6) return b;
    let axis = cross(a, b);
    if (len(axis) < 1e-6) axis = Math.abs(a[1]) < 0.9 ? cross(a, [0, 1, 0]) : cross(a, [1, 0, 0]);
    return norm(rot(a, norm(axis), maxA));
  }
  function perp(a) { return norm(Math.abs(a[1]) < 0.9 ? cross(a, [0, 1, 0]) : cross(a, [1, 0, 0])); }

  // Look-ahead rays in the camera frame (right, up, forward): straight ahead plus three rings (24).
  // The probe takes 25: the last one is marched straight down to measure the real height.
  const DIRS = (function () {
    const out = [[0, 0, 1]];
    [[15, 8], [32, 8], [55, 7]].forEach(([a, n], ri) => {
      for (let k = 0; k < n; k += 1) {
        const ph = ((k + (ri % 2) * 0.5) / n) * 2 * Math.PI;
        const s = Math.sin(a * DEG);
        out.push([s * Math.cos(ph), s * Math.sin(ph), Math.cos(a * DEG)]);
      }
    });
    return out;
  })();

  // Consecutive sections whose scene is flown form one run (one continuous flight).
  function runs(project) {
    const on = new Set((project.flight && project.flight.on) || []);
    const starts = Ev.sectionStarts(project);
    const out = [];
    project.sections.forEach((s, i) => {
      if (!on.has(s.sceneId)) return;
      const last = out[out.length - 1];
      if (last && last.endIndex === i - 1) { last.endIndex = i; last.b1 = starts[i] + s.length; }
      else out.push({ startIndex: i, endIndex: i, b0: starts[i], b1: starts[i] + s.length });
    });
    return out;
  }

  // Camera look (forward) + up -> yaw/pitch/roll in the convention of Ev.cameraBasis.
  function toAngles(L, U) {
    const dir = mul(L, -1);
    const pitch = Math.asin(clamp(dir[1], -1, 1)) / DEG;
    const yaw = Math.atan2(dir[0], dir[2]) / DEG;
    let right0 = [-L[2], 0, L[0]];
    right0 = norm(right0, [1, 0, 0]);
    const up0 = cross(right0, L);
    const roll = Math.atan2(-dot(U, right0), dot(U, up0)) / DEG;
    return [yaw, clamp(pitch, -89, 89), roll];
  }

  const r5 = (x) => Math.round(x * 1e5) / 1e5;
  const r3 = (x) => Math.round(x * 1e3) / 1e3;

  /* probe(frame, pos, dirs, maxRay, eps) -> { d, grad:[3], rays:[n] } (sync or a Promise).
     opts: { onProgress(fraction), cancelled() -> bool, yieldEvery } */
  async function compute(project, analysis, probe, opts) {
    const o = opts || {};
    const set = project.flight.settings;
    const spb = Ev.secPerBeat(project);
    const recDt = STEP_BEATS * spb;
    const subN = Math.max(1, Math.ceil(recDt * SIM_HZ));
    const dt = recDt / subN;
    const list = runs(project);
    const totalRecs = list.reduce((a, r) => a + Math.floor((r.b1 - r.b0) / STEP_BEATS) + 1, 0) || 1;
    let doneRecs = 0;
    let steps = 0;
    const stats = { minClear: Infinity, minClearRel: Infinity, pushes: 0, steps: 0 };
    const outRuns = [];
    const seed = set.seed || 0;

    for (const run of list) {
      const t0 = Ev.timeAtBeat(project, run.b0);
      const f0 = Ev.evaluate(project, analysis, t0, { flight: false });
      const b0 = Ev.cameraBasis(f0.camera);
      let P = b0.pos.slice();
      let H = norm(b0.fwd);
      let L = H.slice();
      let U = norm(b0.up);
      let lastN = [0, 1, 0];
      let lastFormula = f0.formula;
      // (re)start from the camera the timeline shows at this moment
      const startFrom = (fr) => {
        const b = Ev.cameraBasis(fr.camera);
        P = b.pos.slice(); H = norm(b.fwd); L = H.slice(); U = norm(b.up);
      };
      const nRec = Math.floor((run.b1 - run.b0) / STEP_BEATS) + 1;
      const pts = [];

      const record = () => {
        const a = toAngles(L, U);
        pts.push(r5(P[0]), r5(P[1]), r5(P[2]), r3(a[0]), r3(a[1]), r3(a[2]));
      };

      const stepSim = async (t) => {
        const frame = Ev.evaluate(project, analysis, t, { flight: false });
        // A cut to another formula: the old position was never checked against the new shape (it may be
        // inside it), so the flight starts again from the camera the new scene brings with it.
        if (frame.formula !== lastFormula) { lastFormula = frame.formula; startFrom(frame); stats.restarts = (stats.restarts || 0) + 1; }
        const S = frame.gpu.size || 1;
        const alt = set.height * S;
        const minC = 0.3 * alt;
        const look = Math.max(alt * 6, 0.5 * S);
        const eps = Math.max(1e-5 * S, alt * 0.02);
        const right = norm(cross(H, U), perp(H));
        const up = norm(cross(right, H));
        const dirs = DIRS.map((d) => norm(add(add(mul(right, d[0]), mul(up, d[1])), mul(H, d[2]))));
        dirs.push(mul(norm(U), -1)); // straight down (along the view's up)
        const r = await probe(frame, P, dirs, look, eps);
        const d0 = Number.isFinite(r.d) ? r.d : look;
        const n = norm(r.grad, lastN);
        lastN = n;
        stats.minClear = Math.min(stats.minClear, d0);
        stats.minClearRel = Math.min(stats.minClearRel, d0 / alt);
        // too close (or inside after a cut to another shape): step straight out first
        // (the distance estimate may be scaled down - a formula's step < 1 - so divide by how fast it grows)
        const gl = clamp(len(r.grad || [0, 0, 0]), 0.2, 1);
        if (d0 < minC) { P = add(P, mul(n, (minC - d0) / gl)); stats.pushes += 1; }

        // skim: follow the surface sideways, correct the height along the normal
        let tan = sub(H, mul(n, dot(H, n)));
        tan = norm(tan, perp(n));
        // Height: the formula's distance is only a lower bound (often several times too small), so the
        // height that is held is measured by marching straight down to the surface, as the renderer would see
        // it. Nothing below within reach: fall back to the lower bound (we are far away anyway).
        const down = r.rays[DIRS.length];
        const hgt = down < look * 0.999 ? Math.max(down, d0) : Math.max(d0, look);
        const err = clamp((hgt - alt) / alt, -1, 1);
        let des = norm(sub(tan, mul(n, err * 0.9)));
        // wander: a slow, seeded turning rate about the surface normal
        const wa = Math.sin(t * 0.21 + seed * 1.7) + 0.6 * Math.sin(t * 0.53 + seed * 3.1) + 0.3 * Math.sin(t * 1.13 + seed * 0.7);
        des = norm(rot(des, n, set.wander * 0.6 * wa * dt)); // up to ~0.5 rad/s at full wander
        // obstacles: how open is the way we want to go, and which ray is most open
        let best = null, bestScore = -Infinity, alignBest = -2, freeDes = 1;
        r.rays.forEach((dist, k) => {
          if (k >= DIRS.length) return;
          const free = clamp(dist / look, 0, 1);
          const al = dot(dirs[k], des);
          const score = free + 0.8 * al;
          if (score > bestScore) { bestScore = score; best = dirs[k]; }
          if (al > alignBest) { alignBest = al; freeDes = free; }
        });
        if (best && freeDes < 0.999) des = norm(mix(des, best, Math.pow(1 - freeDes, 0.7)));
        // limited turn rate; measure the turn for banking
        const Hn = turnToward(H, des, set.turn * DEG * dt);
        const turnRate = Math.atan2(dot(cross(H, Hn), n), dot(H, Hn)) / dt; // + = left about the normal
        H = Hn;
        // move, never more than half the free distance
        const v = set.speed * S * clamp(hgt / alt, 0.3, 4);
        const stepLen = Math.min(v * dt, 0.5 * Math.max(d0, minC * 0.5));
        P = add(P, mul(H, stepLen));
        // view: up along the surface normal, banked into turns; look tilted toward the surface
        // up follows the surface normal, so the surface is always below (a little world up against noisy normals)
        let upT = norm(mix([0, 1, 0], n, 0.85), n);
        upT = norm(rot(upT, H, -clamp(turnRate * set.bank * 0.5, -0.8, 0.8)));
        const ku = 1 - Math.exp(-dt / 0.6);
        const kl = 1 - Math.exp(-dt / 0.35);
        L = norm(mix(L, norm(sub(H, mul(n, set.lookDown * 1.5))), kl), H);
        U = norm(mix(U, upT, ku), upT);
        U = norm(sub(U, mul(L, dot(U, L))), perp(L)); // keep up perpendicular to the view
        steps += 1;
        if (o.yieldEvery !== 0 && steps % (o.yieldEvery || 40) === 0) {
          await new Promise((res) => setTimeout(res, 0));
          if (o.onProgress) o.onProgress(doneRecs / totalRecs);
        }
      };

      record();
      doneRecs += 1;
      for (let k = 1; k < nRec; k += 1) {
        for (let j = 1; j <= subN; j += 1) {
          if (o.cancelled && o.cancelled()) return null;
          await stepSim(t0 + ((k - 1) * subN + j) * dt);
        }
        record();
        doneRecs += 1;
      }
      outRuns.push({ b0: run.b0, b1: run.b0 + (nRec - 1) * STEP_BEATS, pts });
    }
    stats.steps = steps;
    if (o.onProgress) o.onProgress(1);
    return { track: outRuns.length ? { sig: Ev.flightSig(project), step: STEP_BEATS, runs: outRuns } : null, stats };
  }

  return { compute, runs, DIRS, STEP_BEATS };
});
