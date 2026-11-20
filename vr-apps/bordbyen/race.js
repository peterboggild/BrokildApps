/*
 * Bordbyen — the race: how the cars drive, the opponents, and the gates and laps.
 *
 * Arcade physics in the city's metres: each car has a place on the ground (x, z), a heading, a speed along its nose and a little sideways slip. Gas
 * and brake change the speed (the brake reverses once stopped); the steering turns it by a bicycle model, with less lock the faster it goes. Roads
 * and squares are fast; grass and parks hold each car to its own off-road fraction of top speed. Walls, trees and the fountain push the car out and
 * take speed off it by how hard it hit; cars push each other apart.
 *
 * The opponents follow the race line (the rounded route through the gates from city.route()), each in its own lane, looking further ahead the faster
 * they go and slowing for bends by how much the line turns ahead. A little rubber band keeps the race together. One that has stopped against
 * something backs off and tries again.
 *
 * A lap is the gates in order and then the start/finish; the first time over the start does not count (the grid is behind it).
 */
import * as THREE from './vendor/three.module.min.js';
import { HANDLING } from './cars.js?v=1';

const V2 = THREE.Vector2, clamp = (v, a, b) => Math.max(a, Math.min(b, v));
export const LAPS = 3;

export function createRace({ city, onBump, onGate, onLap, onFinish }) {
  const R = { cars: [], route: null, state: 'idle', t: 0, count: 0, laps: LAPS, finishOrder: [] };

  function addCar(obj, key, name, ai, skill = 1) {
    const c = { obj, key, name, ai, skill, h: HANDLING[key], p: new V2(), yaw: 0, v: 0, slip: 0, steer: 0, ang: 0, yawRate: 0, spin: 0,
      lap: 0, next: 0, passed: 0, done: false, time: 0, li: 0, lane: 0, stuck: 0, back: 0, bumpT: 0, roll: 0, pitch: 0, surf: 'road' };
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
      c.v = 0; c.lap = 0; c.next = 0; c.passed = 0; c.done = false; c.time = 0; c.li = k; c.lane = (ci % 2 ? 1 : -1) * 1.3; c.stuck = 0; c.back = 0;
      city.collide(c.p, c.obj.userData.radius * 0.8); row++;
    }
    R.state = 'count'; R.count = 3.5; R.t = 0; R.finishOrder = [];
    for (const c of R.cars) place(c, 0);
  }

  function place(c, dt) {
    const o = c.obj; o.position.set(c.p.x, 0, c.p.y); o.rotation.set(0, c.yaw, 0);
    /* lean into turns and squat on the gas a little */
    const roll = clamp(-c.yawRate * c.v * 0.012, -0.08, 0.08); c.roll += (roll - c.roll) * Math.min(1, dt * 6);
    o.rotateZ(c.roll); o.rotateX(c.pitch);
    const u = o.userData; for (const w of u.wheels) w.rotation.x -= c.v * dt / 0.5; for (const s of u.steer) s.rotation.y = c.ang;
    if (u.wheel) u.wheel.rotation.z = -c.ang * 3.2;
  }

  function drive(c, gas, brake, steer, dt) {
    const h = c.h, surf = city.surfaceAt(c.p.x, c.p.y), off = surf === 'road' || surf === 'torv' ? 1 : surf === 'lot' ? 0.6 : h.off;
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
    c.yaw += c.yawRate * dt;
    const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw);
    c.slip *= Math.exp(-dt * 5);
    c.p.x += (fx * c.v + fz * c.slip) * dt; c.p.y += (fz * c.v - fx * c.slip) * dt;
    /* walls and trees */
    const hit = city.collide(c.p, c.obj.userData.radius * 0.8);
    if (hit) {
      const vn = fx * hit.x + fz * hit.y;                                              // how head-on (−1 straight in)
      if (c.v * vn < -0.3) { const k = Math.abs(vn); const lost = c.v * (0.25 + 0.6 * k); c.v -= lost; c.slip += (fz * hit.x - fx * hit.y) * Math.abs(lost) * 0.2;
        if (c.bumpT <= 0 && Math.abs(lost) > 2) { onBump(c, Math.abs(lost)); c.bumpT = 0.3; } }
    }
    c.bumpT -= dt;
    c.spin = c.v;
  }

  /* the opponents' driving */
  function ai(c, dt) {
    const L = R.route.line, n = L.length;
    /* where am I on the line: search a little way ahead */
    let best = c.li, bd = Infinity; for (let k = 0; k < 24; k++) { const j = (c.li + k) % n, d = L[j].distanceToSquared(c.p); if (d < bd) { bd = d; best = j; } } c.li = best;
    const look = 5 + Math.abs(c.v) * 0.45; let k = c.li, acc = 0; while (acc < look) { const j = (k + 1) % n; acc += L[k].distanceTo(L[j]); k = j; }
    const a = L[k], b = L[(k + 1) % n], t = new V2(b.x - a.x, b.y - a.y).normalize(), tgt = new V2(a.x - t.y * c.lane, a.y + t.x * c.lane);
    /* how much the line turns over the next stretch: slow down for it */
    let turn = 0, k2 = c.li, acc2 = 0; const span = 10 + Math.abs(c.v) * 1.3;
    while (acc2 < span) { const j = (k2 + 1) % n, j2 = (k2 + 2) % n; const d1 = new V2().subVectors(L[j], L[k2]), d2 = new V2().subVectors(L[j2], L[j]);
      turn += Math.abs(Math.atan2(d1.x * d2.y - d1.y * d2.x, d1.dot(d2))); acc2 += d1.length(); k2 = j; }
    const want = c.h.vmax * c.skill * c.band * clamp(1.15 - turn * 0.5, 0.35, 1);
    const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw), dx = tgt.x - c.p.x, dz = tgt.y - c.p.y;
    const err = Math.atan2(fz * dx - fx * dz, fx * dx + fz * dz);                      // + = the target is to the left (steer + turns left)
    let gas = c.v < want ? 1 : 0, brake = c.v > want + 1.5 ? 1 : 0, steer = clamp(err * 2.0, -1, 1);
    /* stuck against something: back off for a second, steering the other way */
    if (R.state === 'run' && Math.abs(c.v) < 1 && !c.back) { c.stuck += dt; if (c.stuck > 1.2) { c.back = 1.1; c.stuck = 0; } } else c.stuck = 0;
    if (c.back > 0) { c.back -= dt; gas = 0; brake = 1; steer = -steer; if (c.back <= 0) c.back = 0; }
    drive(c, gas, brake, steer, dt);
  }

  function gates(c) {
    if (c.done) return;
    const g = R.route.gates[c.next], d = Math.abs(c.p.x - g.x) < 4.2 && Math.abs(c.p.y - g.z) < 4.2;
    if (!d) return;
    if (c.next === 0) {
      if (c.passed > 0 || c.lap === 0) { c.lap++; if (c.lap > R.laps) { c.done = true; c.time = R.t; R.finishOrder.push(c); onFinish(c, R.finishOrder.length); return; } if (c.lap > 1) onLap(c, c.lap); }
    } else onGate(c, c.next);
    c.passed++; c.next = (c.next + 1) % R.route.gates.length;
  }
  /** how far along the race a car is (for the order) */
  function score(c) {
    if (c.done) return 1e9 - R.finishOrder.indexOf(c);
    const g = R.route.gates[c.next]; return c.passed * 1000 - Math.hypot(c.p.x - g.x, c.p.y - g.z);
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
    /* cars push each other apart */
    for (let i = 0; i < R.cars.length; i++) for (let j = i + 1; j < R.cars.length; j++) {
      const a = R.cars[i], b = R.cars[j], dx = b.p.x - a.p.x, dz = b.p.y - a.p.y, d = Math.hypot(dx, dz), rr = (a.obj.userData.radius + b.obj.userData.radius) * 0.75;
      if (d < rr && d > 1e-4) { const push = (rr - d) / 2, nx = dx / d, nz = dz / d; a.p.x -= nx * push; a.p.y -= nz * push; b.p.x += nx * push; b.p.y += nz * push;
        const rel = Math.abs(a.v - b.v); a.v *= 0.97; b.v *= 0.97; if (rel > 3 && (a.bumpT <= 0 || b.bumpT <= 0)) { onBump(!a.ai ? a : b, rel * 0.6); a.bumpT = b.bumpT = 0.3; } }
    }
    for (const c of R.cars) place(c, dt);
    if (R.state === 'run' && R.cars.every(c => c.done || c.ai) && player && player.done) R.state = 'done';
  }

  function clear() { for (const c of R.cars) c.obj.removeFromParent(); R.cars.length = 0; R.route = null; R.state = 'idle'; }
  return Object.assign(R, { addCar, grid, update, order, clear, score });
}
