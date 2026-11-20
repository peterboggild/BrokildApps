/*
 * Ræs i Bordbyen — the race: how the cars drive, the opponents, and the gates and laps.
 *
 * Arcade physics in the city's metres: each car has a place on the ground (x, z), a heading, a speed along its nose and a little sideways slip. Gas
 * and brake change the speed (the brake reverses once stopped); the steering turns it by a bicycle model, with less lock the faster it goes. Roads
 * and squares are fast; grass and parks hold each car to its own off-road fraction of top speed. Walls, trees and the fountain push the car out and
 * take speed off it by how hard it hit; cars push each other apart.
 *
 * Cars hit each other properly: each is two circles along its length, overlaps are pushed apart at once (no driving through each other), and the
 * hit is an impulse at the point of contact, with the cars' masses and a little bounce. A car hit from behind is shoved on down the road, one hit at the
 * side is pushed sideways and set turning; the player is told how hard (for the shake, the sound and the controllers' rumble).
 *
 * The opponents follow the race line (the rounded route through the gates from city.route()), each in its own lane, looking further ahead the faster
 * they go and slowing for bends by how much the line turns ahead. A little rubber band keeps the race together. One that has stopped against
 * something backs off and tries again.
 *
 * The race is like croquet: clear every gate, in any order you like, and then be the first over the start/finish line. The opponents each plan
 * their own way: the nearest gate they have not cleared yet by road, then the next, and at last the finish.
 */
import * as THREE from './vendor/three.module.min.js';
import { HANDLING, CAR_SCALE } from './cars.js?v=10';

const V2 = THREE.Vector2, clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const MASS = { lyn: 1.0, boble: 0.85, bulder: 1.3, brum: 1.5 };
const HALF = 1.15 * CAR_SCALE, RAD = 0.98 * CAR_SCALE, BOUNCE = 0.35;                                  // a car is two circles of radius RAD, HALF metres ahead of and behind its middle
export const LAPS = 1;

export function createRace({ city, onBump, onGate, onLap, onFinish }) {
  const R = { cars: [], route: null, state: 'idle', t: 0, count: 0, laps: LAPS, finishOrder: [] };

  function addCar(obj, key, name, ai, skill = 1) {
    const c = { obj, key, name, ai, skill, h: HANDLING[key], p: new V2(), yaw: 0, v: 0, slip: 0, steer: 0, ang: 0, yawRate: 0, spin: 0,
      lap: 0, next: 0, passed: 0, done: false, time: 0, li: 0, lane: 0, stuck: 0, back: 0, bumpT: 0, roll: 0, pitch: 0, surf: 'road',
      m: MASS[key] || 1, wx: 0, delay: 0, laneT: 0, cleared: [], nCleared: 0, path: null, pi: 0, target: 0, planT: 0, level: 0, rampT: 0 };
    R.cars.push(c); return c;
  }
  /** Line up on the grid behind the start, two abreast, in the order given (the player last, like a proper underdog). */
  function grid(route) {
    R.route = route; const L = route.line, n = L.length, s0 = route.gateAt[0];
    const tan = k => { const a = L[(k + n - 1) % n], b = L[(k + 1) % n]; return new V2(b.x - a.x, b.y - a.y).normalize(); };
    let k = s0, dist = 0, row = 0;
    for (let ci = 0; ci < R.cars.length; ci++) {
      const want = 5 + Math.floor(ci / 2) * 7;                                     // rows 7 m apart, the first 5 m behind the line
      while (dist < want) { const a = L[k], b = L[(k + n - 1) % n]; dist += a.distanceTo(b); k = (k + n - 1) % n; }
      const t = tan(k), side = new V2(-t.y, t.x), c = R.cars[ci];
      c.p.copy(L[k]).addScaledVector(side, ci % 2 ? 1.7 : -1.7); c.yaw = Math.atan2(-t.x, -t.y);
      c.v = 0; c.slip = 0; c.wx = 0; c.lap = 0; c.next = 0; c.passed = 0; c.done = false; c.time = 0; c.li = k; c.lane = (ci % 2 ? 1 : -1) * 1.7; c.stuck = 0; c.back = 0;
      c.delay = c.ai ? Math.floor(ci / 2) * 0.35 + (ci % 2) * 0.12 : 0; c.laneT = 0;
      c.cleared = route.gates.map(() => false); c.nCleared = 0; c.path = null; c.planT = 0; c.level = 0; c.rampT = 0;                                     // the front row goes first: no pile-up off the line
      city.collideL(c.p, c.obj.userData.radius * 0.8, 0); row++;
    }
    R.state = 'count'; R.count = 3.5; R.t = 0; R.finishOrder = [];
    for (const c of R.cars) { place(c, 0); if (c.ai) plan(c); }
  }

  function place(c, dt) {
    const o = c.obj, fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw), H = (x, z) => city.groundY(c.level, x, z);
    /* on the ground: its height, tipped by the slope along and across it; and shaken about off the road */
    const pitchG = Math.atan((H(c.p.x + fx * 1.4, c.p.y + fz * 1.4) - H(c.p.x - fx * 1.4, c.p.y - fz * 1.4)) / 2.8), rollG = Math.atan((H(c.p.x - fz * 0.9, c.p.y + fx * 0.9) - H(c.p.x + fz * 0.9, c.p.y - fx * 0.9)) / 1.8);
    const shake = c.rough ? Math.min(1, Math.abs(c.v) / 8) * (c.noTilt ? 0.3 : 1) : 0; const tilt = c.noTilt ? 0 : 1;      // your own car, seen from its seat, keeps level (its bonnet would rise into the view uphill)
    o.position.set(c.p.x, H(c.p.x, c.p.y) + shake * (Math.random() * 0.09), c.p.y); o.rotation.set(0, c.yaw, 0);
    o.rotateZ(rollG * tilt + shake * (Math.random() - 0.5) * 0.05); o.rotateX(pitchG * tilt + shake * (Math.random() - 0.5) * 0.05);
    /* lean into turns and squat on the gas a little */
    const roll = clamp(-c.yawRate * c.v * 0.012, -0.08, 0.08); c.roll += (roll - c.roll) * Math.min(1, dt * 6);
    o.rotateZ(c.roll); o.rotateX(c.pitch);
    const u = o.userData; for (const w of u.wheels) w.rotation.x -= c.v * dt / 0.5; for (const s of u.steer) s.rotation.y = c.ang;
    if (u.wheel) u.wheel.rotation.z = -c.ang * 3.2;
  }

  const _w = new V2(), _hn = new V2();
  function drive(c, gas, brake, steer, dt) {
    const h = c.h, surf = city.surfaceAt(c.p.x, c.p.y, c.level), off = surf === 'road' || surf === 'torv' ? 1 : h.off;      // off the road: a third of the speed
    c.rough = off < 1;
    c.surf = surf;
    const vmax = h.vmax * off * (c.ai ? c.skill : 1);
    const a0 = c.v;
    if (gas > 0 && c.v > -0.5) c.v += h.acc * gas * dt * (1 - Math.max(0, c.v) / (vmax + 0.01) * 0.55);
    if (brake > 0) { if (c.v > 0.3) c.v -= 22 * brake * dt; else c.v -= 7 * brake * dt; }
    if (gas > 0 && c.v < -0.3) c.v += 22 * gas * dt;                                  // gas while rolling back brakes first
    c.v -= c.v * (0.18 + (gas || brake ? 0 : 0.5)) * dt;                              // drag, and rolling to a stop
    if (c.v > vmax) c.v -= Math.min(c.v - vmax, (off < 1 ? 26 : 8) * dt);
    c.v = clamp(c.v, -7, h.vmax * 1.05);
    c.pitch += (clamp(-(c.v - a0) / dt * 0.004, -0.04, 0.04) - c.pitch) * Math.min(1, dt * 5);
    /* steering: smoothed, with less lock at speed */
    c.steer += (steer - c.steer) * Math.min(1, dt * 7);
    c.ang = c.steer * 0.62 / (1 + Math.abs(c.v) / 16);
    c.yawRate = c.v / 2.7 * Math.tan(c.ang) * h.turn * h.grip;
    c.yaw += (c.yawRate + c.wx) * dt; c.wx *= Math.exp(-dt * 2.5);               // wx: a spin from being hit, dying away
    const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw);
    c.slip *= Math.exp(-dt * 3.5 * h.grip);
    { const H = (x, z) => city.groundY(c.level, x, z), sl = (H(c.p.x + fx * 1.4, c.p.y + fz * 1.4) - H(c.p.x - fx * 1.4, c.p.y - fz * 1.4)) / 2.8; c.v -= 9.8 * sl * 0.75 * dt; }      // uphill slows, downhill speeds
    c.p.x += (fx * c.v + fz * c.slip) * dt; c.p.y += (fz * c.v - fx * c.slip) * dt;
    /* walls and trees: the car's whole length (its two circles) is kept out of them, on its own level; then the level it is on now */
    let hit = null;
    for (let it = 0; it < 2; it++) for (const sg of [1, -1]) {
      _w.set(c.p.x + fx * HALF * sg, c.p.y + fz * HALF * sg); const ox = _w.x, oz = _w.y, hh = city.collideL(_w, RAD, c.level);
      if (hh) { hit = _hn.copy(hh); c.p.x += _w.x - ox; c.p.y += _w.y - oz; }
    }
    { const lv = city.levelAt(c.level, c.rampT, c.p.x, c.p.y); c.level = lv.level; c.rampT = lv.t; }
    if (hit) {
      const vn = fx * hit.x + fz * hit.y;                                              // how head-on (−1 straight in)
      if (c.v * vn < -0.3) { const k = Math.abs(vn); const lost = c.v * (0.25 + 0.6 * k); c.v -= lost; c.slip += (fz * hit.x - fx * hit.y) * Math.abs(lost) * 0.2;
        if (c.bumpT <= 0 && Math.abs(lost) > 2) { onBump(c, Math.abs(lost)); c.bumpT = 0.3; } }
    }
    c.bumpT -= dt;
    c.spin = c.v;
  }

  /* the opponents' driving */
  /** an opponent's next aim: the nearest gate it has not cleared, by road; with them all cleared, the finish */
  function plan(c) {
    const G = R.route.gates, left = G.map((g, k) => k).filter(k => k > 0 && !c.cleared[k]);
    let best = null, bestK = 0;
    for (const k of left.length ? left : [0]) { const p = city.pathTo(c.p.x, c.p.y, G[k].i, G[k].j, c.level); if (p && (!best || p.len < best.len)) { best = p; bestK = k; } }
    c.path = best ? best.pts : null; c.target = bestK; c.pi = 0; c.planT = 4;
  }
  function ai(c, dt) {
    c.planT -= dt; if (!c.path || c.planT <= 0) plan(c);
    const L = c.path || [c.p.clone()], n = L.length, at = k => L[Math.min(n - 1, Math.max(0, k))];
    /* where am I on my way: search a little way ahead */
    let best = c.pi, bd = Infinity; for (let k = 0; k < 24 && c.pi + k < n; k++) { const d = L[c.pi + k].distanceToSquared(c.p); if (d < bd) { bd = d; best = c.pi + k; } } c.pi = best;
    const look = 5 + Math.abs(c.v) * 0.45; let k = c.pi, acc = 0; while (acc < look && k < n - 1) { acc += L[k].distanceTo(L[k + 1]); k++; }
    const a = at(k), b = at(k + 1), t = b.distanceToSquared(a) > 1e-6 ? new V2(b.x - a.x, b.y - a.y).normalize() : new V2(-Math.sin(c.yaw), -Math.cos(c.yaw)), lane = k >= n - 3 ? 0 : c.lane, tgt = new V2(a.x - t.y * lane, a.y + t.x * lane);
    /* how much the way turns over the next stretch: slow down for it (and for the gate at its end, if it is a sharp one) */
    let turn = 0, k2 = c.pi, acc2 = 0; const span = 10 + Math.abs(c.v) * 1.3;
    while (acc2 < span && k2 < n - 2) { const d1 = new V2().subVectors(L[k2 + 1], L[k2]), d2 = new V2().subVectors(L[k2 + 2], L[k2 + 1]);
      turn += Math.abs(Math.atan2(d1.x * d2.y - d1.y * d2.x, d1.dot(d2))); acc2 += d1.length(); k2++; }
    let want = c.h.vmax * c.skill * c.band * clamp(1.15 - turn * 0.5, 0.35, 1);
    /* a car ahead in my lane: keep a gap behind it, and pull out into the other lane if that is free */
    { const fx0 = -Math.sin(c.yaw), fz0 = -Math.cos(c.yaw); let ahead = null, gap = Infinity;
      for (const o of R.cars) { if (o === c) continue; const dx = o.p.x - c.p.x, dz = o.p.y - c.p.y, along = dx * fx0 + dz * fz0, lat = dx * fz0 - dz * fx0;
        if (along > 0 && along < 5 + Math.abs(c.v) * 0.9 && Math.abs(lat) < 2.4 && along < gap) { gap = along; ahead = o; } }
      c.laneT -= dt;
      if (ahead) {
        if (c.laneT <= 0 && ahead.v < c.v + 1) { c.lane = -c.lane; c.laneT = 2.5; }
        const keep = ahead.v * (gap < 6 ? 0.8 : 1.0) - (gap < 4 ? 3 : 0); want = Math.min(want, Math.max(2, keep));
      } }
    const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw), dx = tgt.x - c.p.x, dz = tgt.y - c.p.y;
    const err = Math.atan2(fz * dx - fx * dz, fx * dx + fz * dz);                      // + = the target is to the left (steer + turns left)
    let gas = c.v < want ? 1 : 0, brake = c.v > want + 1.5 ? 1 : 0, steer = clamp(err * 2.0, -1, 1);
    /* stuck against something: back off for a second, steering the other way */
    if (R.t < c.delay) { gas = 0; brake = 0; }                                    // waiting for my turn off the line
    if (R.state === 'run' && R.t > c.delay + 1.5 && Math.abs(c.v) < 1 && !c.back) { c.stuck += dt; if (c.stuck > 1.2) { c.back = 1.1; c.stuck = 0; } } else c.stuck = 0;
    if (c.back > 0) { c.back -= dt; gas = 0; brake = 1; steer = -steer; if (c.back <= 0) c.back = 0; }
    drive(c, gas, brake, steer, dt);
  }

  function gates(c) {
    if (c.done) return;
    const G = R.route.gates;
    for (let k = 1; k < G.length; k++) {
      if (c.cleared[k]) continue; const g = G[k];
      if (Math.abs(c.p.x - g.x) < 4.2 && Math.abs(c.p.y - g.z) < 4.2) { c.cleared[k] = true; c.nCleared++; onGate(c, k, c.nCleared, G.length - 1); if (c.ai) plan(c); }
    }
    const s0 = G[0];
    if (c.nCleared === G.length - 1 && Math.abs(c.p.x - s0.x) < 4.2 && Math.abs(c.p.y - s0.z) < 4.2) { c.done = true; c.lap = 1; c.time = R.t; R.finishOrder.push(c); onFinish(c, R.finishOrder.length); }
  }
  /** how far along the race a car is (for the order) */
  function score(c) {
    if (c.done) return 1e9 - R.finishOrder.indexOf(c);
    /* gates cleared count most; then how near the next one is (the nearest left, or the finish) */
    const G = R.route.gates; let d = Infinity; for (let k = 0; k < G.length; k++) { if (k === 0 ? c.nCleared < G.length - 1 : c.cleared[k]) continue; d = Math.min(d, Math.hypot(c.p.x - G[k].x, c.p.y - G[k].z)); }
    return c.nCleared * 1000 - (isFinite(d) ? d : 0);
  }

  /* ---- car against car ---- */
  const _ca = [new V2(), new V2()], _cb = [new V2(), new V2()], _n = new V2(), _r1 = new V2(), _r2 = new V2();
  const crs = (r, n) => r.y * n.x - r.x * n.y;                                // the turning effect of a push n at r (yaw is about y; r.y is z)
  function circles(c, out) { const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw); out[0].set(c.p.x + fx * HALF, c.p.y + fz * HALF); out[1].set(c.p.x - fx * HALF, c.p.y - fz * HALF); return out; }
  function carHits(report) {
    for (let i = 0; i < R.cars.length; i++) for (let j = i + 1; j < R.cars.length; j++) {
      const A = R.cars[i], B = R.cars[j]; if (A.p.distanceToSquared(B.p) > 36) continue;
      if (Math.abs(city.groundY(A.level, A.p.x, A.p.y) - city.groundY(B.level, B.p.x, B.p.y)) > 2) continue;      // one in a tunnel, one above it: they do not meet
      circles(A, _ca); circles(B, _cb);
      let best = null, pen = 0;
      for (const a of _ca) for (const b of _cb) { const d = a.distanceTo(b), pn = 2 * RAD - d; if (pn > pen && d > 1e-5) { pen = pn; best = [a, b, d]; } }
      if (!best) continue;
      const [a, b, d] = best; _n.set((b.x - a.x) / d, (b.y - a.y) / d);
      /* out of each other at once, the lighter one moved further */
      const tot = A.m + B.m; A.p.addScaledVector(_n, -pen * B.m / tot); B.p.addScaledVector(_n, pen * A.m / tot);
      /* the impulse at the contact point */
      const cx = a.x + _n.x * RAD, cz = a.y + _n.y * RAD; _r1.set(cx - A.p.x, cz - A.p.y); _r2.set(cx - B.p.x, cz - B.p.y);
      const vel = (c, r) => { const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw), w = c.yawRate + c.wx; return [fx * c.v + fz * c.slip + w * r.y, fz * c.v - fx * c.slip - w * r.x]; };
      const va = vel(A, _r1), vb = vel(B, _r2), vn = (vb[0] - va[0]) * _n.x + (vb[1] - va[1]) * _n.y;
      if (vn >= 0) continue;                                                       // already parting
      const IA = A.m * 1.6, IB = B.m * 1.6, ra = crs(_r1, _n), rb = crs(_r2, _n);
      const jj = -(1 + BOUNCE) * vn / (1 / A.m + 1 / B.m + ra * ra / IA + rb * rb / IB);
      const push = (c, sgn, r, I) => {
        const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw), dvx = sgn * jj * _n.x / c.m, dvz = sgn * jj * _n.y / c.m;
        c.v += dvx * fx + dvz * fz; c.slip += dvx * fz - dvz * fx;                   // along the nose, and sideways
        c.wx += clamp(sgn * crs(r, _n) * jj / I, -3, 3);
      };
      push(A, -1, _r1, IA); push(B, 1, _r2, IB);
      if (report && jj > 0.6) for (const c of [A, B]) if (!c.ai && c.bumpT <= 0) { onBump(c, jj, c === A ? B : A, _n.clone().multiplyScalar(c === A ? -1 : 1)); c.bumpT = 0.18; }
    }
  }

  /** the order of the cars, leader first */
  const order = () => [...R.cars].sort((a, b) => score(b) - score(a));

  function update(dt, input) {
    if (!R.route) return;
    if (R.state === 'count') { R.count -= dt; if (R.count <= 0) { R.state = 'run'; R.t = 0; } }
    else if (R.state === 'run' || R.state === 'done') R.t += dt;
    const go = R.state === 'run' || R.state === 'done';
    const ord = go ? order() : R.cars, player = R.cars.find(c => !c.ai), pr = ord.indexOf(player);
    for (const c of R.cars) {
      if (c.ai) {
        /* rubber band: a little faster when behind the player, a little slower when well ahead */
        const r = ord.indexOf(c); c.band = !player || player.done ? 1 : clamp(1 + (r - pr) * 0.03 + (score(player) - score(c)) * 0.00002, 0.9, 1.08);
        if (go) ai(c, dt); else c.v = 0;
      } else if (go && !c.done) drive(c, input.gas, input.brake, input.steer, dt);
      else if (c.done) drive(c, 0, 0, 0, dt);
      else c.v = 0;                                                               // on the grid: held still until the lights go
      if (go) gates(c);
    }
    /* cars hit each other: twice over, so a car squeezed between two others is pushed clear of both */
    for (let it = 0; it < 2; it++) carHits(it === 0);
    for (const c of R.cars) place(c, dt);
    if (R.state === 'run' && R.cars.every(c => c.done || c.ai) && player && player.done) R.state = 'done';
  }

  function clear() { for (const c of R.cars) c.obj.removeFromParent(); R.cars.length = 0; R.route = null; R.state = 'idle'; }
  return Object.assign(R, { addCar, grid, update, order, clear, score });
}
