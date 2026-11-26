/* What there is to do on the Moon (chapter 6), after the two of you are down: the flag, and the rover.
 *
 * The flag. The Lunar Module Pilot fetches it, carries it out into the sunlight to one side of the LM and holds it
 * there. You take hold of the pole (a grip) and push it down into the regolith, hand over hand if you like; when it
 * is in, the crossbar swings up and the flag hangs out on it, and he salutes.
 *
 * The rover. It stands parked on the other side. A grip by its left seat (the Commander's) seats you; he takes the
 * right seat. The left stick is the T-handle: forward and back to drive (up to 3.6 m/s, about 13 km/h), left and
 * right to steer. It rides the ground on its four wheels (pitch, roll and a soft bounce in the low gravity; your own
 * view stays level), throws dust from the rear wheels, and keeps clear of the LM. A, standing still, and you step off
 * to the left; he gets off too and stays near the rover. You hear and feel it through the suit (o.drive, o.bump:
 * the motors, the rumble, a thump at every bump); the rear wheels leave their tracks (o.track); and the console
 * between the seats shows the way, as the real one did: heading, and the bearing and range of where you are going
 * and of the LM (rover.userData.nav, api.target()).
 *
 * The missions, once you have driven a little (Houston says each, and the wrist panel shows where you are with it):
 * 1. Samples: drive to station 1, the boulder field ahead of the LM; lift rocks with the tongs (a grip with your
 *    hand over one) and drop them in the bags at the back of the rover, until there are 100 kg.
 * 2. The film: park the rover 20 to 30 m from the LM and step off. The camera is turned on the LM; he goes to stand
 *    before it and calls you; stand by him. When you are ready, A (or a word on the radio: X held and let go): a
 *    count of three, then twelve seconds are kept, with the seconds left shown, and a clear cut at the end.
 * 3. Home: take the sample bag from the back of the rover (a grip there). He goes up first (the Commander was the
 *    last one on the Moon): to the footpad, up the ladder, in over the porch; the ladder is yours when he is in.
 *    Climb it with the bag, into the cabin; the hatch shuts, Houston gives the go, the count, and the lift-off.
 * 4. At 50 m the view goes dark, and the film is played back (finale.js, the page draws it): the valley, the flag
 *    going up, a rock lifted and stowed, the two of you waving, then the lift-off as the rover's camera saw it, with
 *    the camera's operator a second and a half behind, as he was.
 *
 * What the film is made of: not pictures but motion. Thirty times a second, 48 numbers: where your suit stood, its
 * turn and height, both arms and elbows as your hands had them, the legs and the waist (a crouch, a jump, a stride),
 * his place and whole pose, the flag (where, how far in, how far out), the rock in your hand. The flag's and the
 * rock's scenes are kept as they happen (the last 12 and 9 seconds of each); the wave when you say so. Played back,
 * the same scene is drawn again from a camera's place, blended between the kept frames: what the camera would have
 * seen, movement for movement.
 *
 *   const ops = makeSurfaceOps({ moon, renderer, sun, env, model, lmp, radio, toast, hands, rig, camera, S, clearOf,
 *                                step(strength), makeShadow, makeDust });
 *   scene.add(ops.group);                 (drawn with the near things)
 *   ops.update(dt, { walking, xr, stick: [x, y], headGround: Vector3 })      every frame on the surface
 *   ops.grab(hand)                        a grip closing (hand: { pos: Vector3 rig frame } or null on the screen); true if taken
 *   ops.button('a')                       A pressed; true if it was for the rover
 *   ops.driving, ops.seat                 seated on the rover (the page then places the rig from ops.eye and ops.yaw) */
import * as THREE from 'three';
import { makeFlag } from './flag.js?v=34';
import { makeRover, ROVER } from './rover.js?v=34';
import { makeSamples } from './samples.js?v=34';
import { makeRecorder } from './finale.js?v=34';

const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const G = 1.62;
const ARROWS = ['\u2191', '\u2197', '\u2192', '\u2198', '\u2193', '\u2199', '\u2190', '\u2196'];     /* ahead, and round to the right */

export function makeSurfaceOps(o) {
  const group = new THREE.Group();
  const M = new THREE.Matrix4(); o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
  const yawLM = o.model.rotation.y;
  const site = (x, z, out = new THREE.Vector3()) => { out.set(x, 0, z).applyMatrix4(M); out.y = o.moon.heightAt(out.x, out.z); return out; };
  const _v = new THREE.Vector3(), _w = new THREE.Vector3(), _h = new THREE.Vector3();
  const st = { step: 'wait', t: 0, wait: 0, planted: 0, unfurl: 0, holding: null, lastY: 0, flagAt: site(8.2, 6.2), said: {} };
  const say = (k, cue) => { if (st.said[k]) return; st.said[k] = true; if (o.radio) o.radio.cue(cue || k); };

  /* ------------------------------------------------------------ the flag */
  const flag = makeFlag(); flag.visible = false; group.add(flag);
  const flagShadow = o.makeShadow(o.moon, { half: 1.9, w0: -0.6, w1: 3.0 }); flagShadow.mesh.visible = false; group.add(flagShadow.mesh);
  /* it flies across the Sun's direction, so the Sun lights one face and the shadow is the whole flag */
  const flagYaw = Math.atan2(o.sun.x, o.sun.z) + Math.PI / 2;

  /* ------------------------------------------------------------ the rover */
  const rover = makeRover(o.env); rover.rotation.order = 'YXZ'; group.add(rover);
  const roverShadow = o.makeShadow(o.moon, { half: 2.5, w0: -1.8, w1: 3.4, size: [256, 256], nx: 8, ny: 28 }); group.add(roverShadow.mesh);
  const dust = o.makeDust(o.moon); group.add(dust.points);
  /* ------------------------------------------------------------ the missions */
  const _b = new THREE.Vector3();
  const smp = makeSamples({ moon: o.moon, rig: o.rig, get toast() { return o.toast; }, step: o.step, stowed: () => { if (o.buzz) o.buzz(0.7, 140); }, bag: () => rover.localToWorld(_b.copy(rover.userData.bag)) }); group.add(smp.group);
  const rec = makeRecorder(), station = site(-34, 82), lmAt = site(0, 0);
  /* the boulders that make station 1 a boulder field: there from the start, to be seen from the LM */
  smp.boulders(station.x, station.z);
  const boulderShadow = o.makeShadow(o.moon, { half: 12, w0: -4.5, w1: 7.5, size: [512, 512], nx: 16, ny: 48 }); boulderShadow.mesh.visible = false; group.add(boulderShadow.mesh);
  const tr = { l: null, r: null, laid: 0, navT: 0, bumpT: 0, quiet: true };
  /* what is kept for the film (see the top): 48 numbers a frame; you 0, he 20, the flag 38, the rock 44 */
  const NF = 48, SPEC = { yaw: [3, 23, 41], quat: [5, 10], hold: [42, 44] }, REC = 12;
  const clips = { flag: { rec: makeRecorder(12), on: 0 }, rock: { rec: makeRecorder(9), on: 0 }, wave: { rec } };
  const _a = new Float32Array(NF), _f = new Float32Array(NF), smooth = k => { k = clamp(k, 0, 1); return k * k * (3 - 2 * k); };
  const sparks = o.makeDust(o.moon); sparks.points.material.color.set(0xd8b060); sparks.points.material.size = 0.06; group.add(sparks.points);
  const F = { odo: 0, bag: false, recT: 0, alt: 0, vel: 0, tf: 0, wave: new THREE.Vector3(), me: null, finale: false, actors: false, part: 'wave', on: 0, caption: 0, end: false, fade: 0, hatch: 0,
    cam: new THREE.Vector3(), look: new THREE.Vector3(), lookTo: new THREE.Vector3(), fov: 28, burst2: false,
    tv: new THREE.Vector3(), panCam: new THREE.Vector3(), clock: 0, rover: false, parts: null, cur: null, total: 0, snap: false, rockI: -1, flagUp: false, home: null,
    canGo: false, go: false, tx: false, promptT: 0, n: -1 };
  const p0 = site(-7.6, 4.6);
  const R = { x: p0.x, z: p0.z, y: p0.y, yaw: yawLM + 2.2, v: 0, steer: 0, pitch: 0, roll: 0, bounce: 0, bv: 0, gy: p0.y, spin: 0, driving: false, bakeT: 9, bx: 1e9, bz: 0, byaw: 0, off: 0, dustT: 0 };
  const eye = new THREE.Vector3();
  function settle(dt, snap) {
    const c = Math.cos(R.yaw), s = Math.sin(R.yaw), W = rover.userData.wheels, hs = [];
    for (const w of W) hs.push(o.moon.heightAt(R.x + w.x * c + w.z * s, R.z - w.x * s + w.z * c));
    const gy = (hs[0] + hs[1] + hs[2] + hs[3]) / 4, pitch = Math.atan2((hs[0] + hs[1]) / 2 - (hs[2] + hs[3]) / 2, ROVER.BASE), roll = Math.atan2((hs[1] + hs[3]) / 2 - (hs[0] + hs[2]) / 2, ROVER.TRACK);
    if (snap) { R.gy = gy; R.pitch = pitch; R.roll = roll; R.bounce = 0; R.bv = 0; }
    else {
      /* the body follows the ground through soft springs: it floats and bounces a little, as the films show */
      const dy = gy - R.gy; R.gy = gy; R.bounce -= dy;
      R.bv += (-38 * R.bounce - 4.2 * R.bv) * dt; R.bounce = clamp(R.bounce + R.bv * dt, -0.14, 0.14);
      const k = Math.min(1, dt * 5); R.pitch += (pitch - R.pitch) * k; R.roll += (roll - R.roll) * k;
    }
    R.y = R.gy + R.bounce;
    rover.position.set(R.x, R.y, R.z); rover.rotation.set(R.pitch, R.yaw, R.roll);
    for (let i = 0; i < 4; i++) { W[i].g.rotation.y = (i < 2 ? -1 : 0.6) * R.steer; W[i].spin.rotation.x = -R.spin; W[i].g.position.y = ROVER.WHEEL_R + (hs[i] - gy) * 0.6 - R.bounce * 0.5; }
    rover.userData.handle.rotation.set(-R.thr * 0.35 || 0, 0, -R.steer * 0.8);
  }
  R.thr = 0; settle(0, true);
  const seatWorld = (which, out, up = 0) => { out.copy(rover.userData.seat[which]); out.y += up; return out.applyAxisAngle(_w.set(0, 1, 0), R.yaw).add(_h.set(R.x, R.y, R.z)); };

  function drive(dt, stick) {
    const thr = Math.abs(stick[1]) > 0.12 ? -stick[1] : 0, str = Math.abs(stick[0]) > 0.12 ? stick[0] : 0;
    R.thr = thr;
    const want = thr > 0 ? thr * 3.6 : thr * 1.2, acc = Math.abs(want) > Math.abs(R.v) ? 1.1 : 1.8;
    R.v += clamp(want - R.v, -acc * dt, acc * dt);
    R.steer += (str * 0.5 * (1 - 0.45 * Math.min(1, Math.abs(R.v) / 3.6)) - R.steer) * Math.min(1, dt * 4);
    R.yaw -= R.v * Math.tan(R.steer) / ROVER.BASE * dt;
    const fx = -Math.sin(R.yaw), fz = -Math.cos(R.yaw);
    let nx = R.x + fx * R.v * dt, nz = R.z + fz * R.v * dt;
    /* keep clear of the LM and the other things you cannot drive through: the nose, the tail and the middle */
    for (const d of [1.5, 0, -1.5]) { const px = nx + fx * d, pz = nz + fz * d, c = o.clearOf(px, pz, 0.75); if (c.x !== px || c.z !== pz) { nx += c.x - px; nz += c.z - pz; R.v *= 0.85; } }
    R.x = nx; R.z = nz; R.spin += R.v * dt / ROVER.WHEEL_R;
    /* dust off the rear wheels, thrown back and up in clean arcs */
    R.dustT -= dt * Math.abs(R.v);
    if (R.dustT < 0 && Math.abs(R.v) > 0.8) { R.dustT = 0.35; const c = Math.cos(R.yaw), s = Math.sin(R.yaw);
      for (const w of rover.userData.wheels.slice(2)) { const wx = R.x + w.x * c + w.z * s, wz = R.z - w.x * s + w.z * c; dust.emit(wx, o.moon.heightAt(wx, wz), wz, -fx * Math.sign(R.v), -fz * Math.sign(R.v), 5, 0.6 + Math.abs(R.v) * 0.35); } }
  }

  /* the ascent engine lights: the ground's dust blown out flat all round, and the descent stage's foil torn loose */
  function burst() {
    const gy = o.moon.heightAt(lmAt.x, lmAt.z);
    for (let k = 0; k < 12; k++) { const a = k * Math.PI / 6, sx = Math.sin(a), cz = Math.cos(a);
      dust.emit(lmAt.x + sx * 2.4, gy, lmAt.z + cz * 2.4, sx, cz, 12, 3.4); sparks.emit(lmAt.x + sx * 1.5, gy + 3.0, lmAt.z + cz * 1.5, sx, cz, 7, 2.8); }
  }
  /* where the film is taken from, and the spot before the LM where you both stand for it */
  function setStage() {
    const d = Math.max(1, Math.hypot(R.x - lmAt.x, R.z - lmAt.z)), k = 6.4 / d, sx = -(R.z - lmAt.z) / d, sz = (R.x - lmAt.x) / d;
    F.wave.set(lmAt.x + (R.x - lmAt.x) * k, 0, lmAt.z + (R.z - lmAt.z) * k); F.wave.y = o.moon.heightAt(F.wave.x, F.wave.z);
    settle(0, true); rover.updateMatrixWorld(true); rover.userData.camera.getWorldPosition(F.cam); F.tv.copy(F.cam);       /* (the rover where it stands now) */
    F.me = [F.wave.x - sx * 0.8, F.wave.z - sz * 0.8]; F.him = [F.wave.x + sx * 0.8, F.wave.z + sz * 0.8];
    F.look.set(F.wave.x, F.wave.y + 1.25, F.wave.z);
    /* the camera on its mast turns to it */
    const lx = F.wave.x - F.cam.x, lz = F.wave.z - F.cam.z; rover.userData.pan.rotation.y = Math.atan2(-lx, -lz) - R.yaw;
  }
  /* the film, played back: the wave as it was kept (or a stand-in, if none was), then the lift-off */
  /* one frame of it kept: you, he, the flag, the rock in your hand */
  function keep(r) {
    o.ghost.snap(_a, 0); o.lmp.snapshot(_a, 20);
    _a[38] = flag.position.x; _a[39] = flag.position.y; _a[40] = flag.position.z; _a[41] = flag.rotation.y; _a[42] = flag.visible ? flag.userData.planted : -1; _a[43] = st.unfurl;
    const i = smp.rocks.findIndex(q => q.state === 'held'); _a[44] = i;
    if (i >= 0) smp.rocks[i].mesh.position.toArray(_a, 45); else _a[45] = _a[46] = _a[47] = 0;
    r.add(F.clock, _a);
  }
  /* and set again */
  function pose(a) {
    o.ghost.pose(a, 0); o.lmp.pose(a, 20);
    if (a[42] >= 0) { flag.visible = true; flag.position.set(a[38], a[39], a[40]); flag.rotation.y = a[41]; flag.userData.set(a[42]); flag.userData.unfurl(a[43]); }
  }
  /* Your crewmate collects too (he stood and watched: owner's headset test). While the rover stands at the station
   * he goes to a rock (the one furthest from you: the near ones are yours), bends to it, carries it to the bags and
   * comes back for another. H.s: idle, go, lift, carry, drop. */
  const H = { s: 'idle', r: null, t: 4, n: 0 };
  function helpStop(L) {
    if (H.s === 'idle') return;
    if (H.r && H.r.state === 'his') { H.r.state = 'fall'; H.r.vy = 0; }
    H.s = 'idle'; H.r = null; H.t = 3; if (L && L.S.phase === 'roam') L.command(null);
  }
  function help(dt, L, head) {
    const can = st.step === 'samples' && !R.driving && !smp.done && L && L.on && L.S.phase === 'roam' && Math.hypot(R.x - station.x, R.z - station.z) < 32;
    if (!can) return helpStop(L);
    H.t -= dt;
    if (H.s === 'idle') {
      if (H.t > 0) return;
      let best = null, bd = -1;
      for (const r of smp.rocks) { if (r.state !== 'ground') continue; const d = head ? Math.hypot(r.mesh.position.x - head.x, r.mesh.position.z - head.z) : 9; if (d > bd) { bd = d; best = r; } }
      if (!best || bd < 2.5) { H.t = 2; if (L.S.cmd) L.command(null); return; }
      const rx = best.mesh.position.x, rz = best.mesh.position.z, dx = L.S.pos.x - rx, dz = L.S.pos.z - rz, d = Math.hypot(dx, dz) || 1;
      H.r = best; H.s = 'go'; H.t = 30 + d; L.command({ x: rx + dx / d * 0.5, z: rz + dz / d * 0.5, act: 'work', hold: 1e9, face: [rx, rz] }); say('lmp-rock');
    } else if (H.s === 'go' || H.s === 'lift') {
      if (H.r.state !== 'ground') { H.r = null; return helpStop(L); }                        /* (you took it) */
      if (H.s === 'go') { if (L.arrived) { H.s = 'lift'; H.t = 2.4; } else if (H.t <= 0) { H.r = null; return helpStop(L); } }      /* (or he cannot get there) */
      else if (H.t <= 0) {
        H.r.state = 'his'; H.s = 'carry'; H.t = 60;
        const B = rover.localToWorld(_b.copy(rover.userData.bag)), bx = Math.sin(R.yaw), bz = Math.cos(R.yaw);       /* (behind the rover) */
        L.command({ x: B.x + bx * 0.85, z: B.z + bz * 0.85, act: 'hold', hold: 1e9, face: [B.x, B.z] });
      }
    } else {
      /* the rock in his right hand */
      const yaw = L.S.yaw, fx = -Math.sin(yaw), fz = -Math.cos(yaw), p = H.r.mesh.position;
      _w.set(L.S.pos.x + fx * 0.42 + Math.cos(yaw) * 0.24, L.S.pos.y + 1.12, L.S.pos.z + fz * 0.42 - Math.sin(yaw) * 0.24); p.lerp(_w, Math.min(1, dt * 8));
      if (H.s === 'carry') { if (L.arrived) { H.s = 'drop'; H.t = 1.1; } else if (H.t <= 0) return helpStop(L); }
      else if (H.t <= 0) { smp.stow(H.r, true); H.n++; if (o.step) o.step(0.35); H.r = null; H.s = 'idle'; H.t = 2.5; }      /* (he stands there a moment, then goes for the next) */
    }
  }
  const roverAt = p => { R.x = p[0]; R.z = p[1]; R.yaw = p[2]; settle(0, true); };
  const flagAsLeft = () => { if (!F.flagUp) return; flag.visible = true; flag.position.copy(st.flagAt); flag.rotation.y = flagYaw; flag.userData.set(1); flag.userData.unfurl(1); flagShadow.mesh.visible = true; };
  /* the film's scenes, in the order they happened, and where each is seen from: the valley (a slow turn that ends on
   * the LM), the flag (from beyond it, the LM behind), the rock (from the side the Sun is on, you and the rover both
   * in the picture), the wave and the lift-off (the rover's own camera) */
  function plan() {
    const parts = [{ k: 'pan', d: 9 }], cf = clips.flag, cr = clips.rock, up = 1.5;
    if (cf.on && cf.rec.length > 1.5) {
      const dx = st.flagAt.x - lmAt.x, dz = st.flagAt.z - lmAt.z, d = Math.hypot(dx, dz) || 1;
      cf.cam = new THREE.Vector3(st.flagAt.x + dx / d * 6.5, 0, st.flagAt.z + dz / d * 6.5); cf.cam.y = o.moon.heightAt(cf.cam.x, cf.cam.z) + up;
      parts.push({ k: 'flag', d: cf.rec.length, c: cf });
    }
    if (cr.on === 3 && cr.rec.length > 1.5) {
      const a0 = cr.rec.frames[0].data, B = rover.userData.bag.clone().applyAxisAngle(_w.set(0, 1, 0), cr.rover[2]); B.x += cr.rover[0]; B.z += cr.rover[1];
      let dx = -(B.z - a0[2]), dz = B.x - a0[0]; const d = Math.hypot(dx, dz);
      if (d < 0.5) { dx = 1; dz = 0; } else { dx /= d; dz /= d; }
      if (dx * o.sun.x + dz * o.sun.z < 0) { dx = -dx; dz = -dz; }
      const far = Math.max(5, d * 0.8 + 3.5);
      cr.cam = new THREE.Vector3((B.x + a0[0]) / 2 + dx * far, 0, (B.z + a0[2]) / 2 + dz * far); cr.cam.y = o.moon.heightAt(cr.cam.x, cr.cam.z) + up;
      cr.bag = B;
      parts.push({ k: 'rock', d: cr.rec.length, c: cr });
    }
    parts.push({ k: 'wave', d: (rec.length > 1 ? rec.length : 8) + 0.6 }, { k: 'liftoff', d: 17 });
    let t = 1.0; for (const p of parts) { p.t0 = t; t += p.d; }
    F.parts = parts; F.total = t; F.home = [R.x, R.z, R.yaw]; F.flagUp = flag.visible && st.planted >= 1 && st.unfurl >= 1;
    site(26, 58, F.panCam); F.panCam.y += 1.9;
  }
  /* a scene begins: the rover, the flag and the rocks as they were then */
  function enter(p) {
    F.snap = true; F.rockI = -1;
    if (o.radio) o.radio.cue('film-' + p.k, true, Math.max(4, p.d - 2));          /* Houston, over the pictures */
    for (const q of smp.rocks) q.mesh.visible = q.state !== 'stowed';
    flagAsLeft();
    if (p.c) { roverAt(p.c.rover); F.cam.copy(p.c.cam); F.rover = true; }
    else { roverAt(F.home); F.rover = p.k === 'pan'; F.cam.copy(p.k === 'pan' ? F.panCam : F.tv); }   /* (the camera stands on the rover: it is not in its own picture) */
    if (p.k === 'rock') smp.rocks.forEach((q, n) => { q.mesh.visible = !p.c.gone[n]; if (q.mesh.visible) q.mesh.position.copy(q.home); });
  }
  function film(dt) {
    if (!F.parts) plan();
    F.tf += dt; const t = F.tf, L = o.lmp;
    let p = F.parts[F.parts.length - 1]; for (const q of F.parts) if (t < q.t0 + q.d) { p = q; break; }
    const tp = clamp(t - p.t0, 0, p.d), last = p.k === 'liftoff';
    /* the tube warms up; and a blink of black at every cut */
    F.on = Math.min(clamp((t - 1.2) / 1.5, 0, 1), clamp(Math.min(tp, last ? 9 : p.d - tp) / 0.2, 0, 1)); o.fade(clamp(1 - t / 1.2, 0, 1));
    if (p !== F.cur) { F.cur = p; enter(p); }
    F.part = p.k; let rate = 6;
    if (p.k === 'pan') {
      F.actors = false; o.lift(0, false);
      const k = smooth(tp / (p.d - 1.5)), dx = lmAt.x - F.cam.x, dz = lmAt.z - F.cam.z, d = Math.hypot(dx, dz), a = Math.atan2(dx, dz) - 1.9 * (1 - k);
      F.lookTo.set(F.cam.x + Math.sin(a) * d, o.model.position.y + 3.3 + 7 * (1 - k), F.cam.z + Math.cos(a) * d); F.fov = 44; rate = 30;
    } else if (p.c) {
      F.actors = true; o.lift(0, false);
      const a = p.c.rec.sample(tp, _f, SPEC); pose(a);
      if (p.k === 'flag') { flagShadow.mesh.visible = false; F.lookTo.set(st.flagAt.x, st.flagAt.y + 1.35, st.flagAt.z); F.fov = 36; }
      else {
        const i = Math.round(a[44]);
        if (i >= 0) { F.rockI = i; smp.rocks[i].mesh.visible = true; smp.rocks[i].mesh.position.fromArray(a, 45); } else if (F.rockI >= 0) smp.rocks[F.rockI].mesh.visible = false;
        F.lookTo.set((a[0] + p.c.bag.x) / 2, a[1] + 1.0, (a[2] + p.c.bag.z) / 2); F.fov = 40; rate = 2.5;
      }
    } else if (p.k === 'wave') {
      F.actors = true; o.lift(0, false);
      const a = rec.length > 1 ? rec.sample(Math.min(tp, rec.length), _f, SPEC) : null, yaw = Math.atan2(-(F.cam.x - F.wave.x), -(F.cam.z - F.wave.z));
      if (a) pose(a); else { o.ghost.stand(F.me[0], F.me[1], yaw); L.canned(F.him[0], F.him[1], yaw, t); }
      /* The camera zooms in on you both, as the rover's could (a 6:1 zoom lens): wide for two seconds, then in until
       * the two of you, arms up, fill most of the picture. (From 20 to 30 m at its wide setting you were a few
       * millimetres tall on the tube, and a wave with both arms "did not show": owner's headset test.) */
      const dc = Math.max(6, Math.hypot(F.cam.x - F.wave.x, F.cam.z - F.wave.z)), tight = 2 * Math.atan(1.7 / dc) * 180 / Math.PI;
      F.lookTo.set(F.wave.x, F.wave.y + 1.3, F.wave.z); F.fov = 27 + (Math.min(27, tight) - 27) * smooth((tp - 2) / 3);
    } else {
      F.actors = false; const tl = tp - 2.2, alt = tl > 0 ? 0.5 * 1.75 * tl * tl : 0;
      if (tl > 0 && !F.burst2) { F.burst2 = true; burst(); }
      /* the man at the camera's controls sees it all 1.3 light-seconds late, and leads as best he can */
      o.lift(alt, false); F.lookTo.set(lmAt.x, o.model.position.y + 3.3, lmAt.z); F.fov = 36; rate = 1.5;
    }
    if (F.snap) { F.snap = false; F.look.copy(F.lookTo); } else F.look.lerp(F.lookTo, Math.min(1, dt * rate));
    F.caption = clamp((t - (F.total - 2)) / 2, 0, 1); F.end = t > F.total + 1;
    if (t > F.total - 5 && o.radio) o.radio.cue('film-end', true, 12);
  }
  const api = {
    group, flag, rover, R, st, F, smp, rec, clips, tr, station, H, pose, SPEC,
    get solids() { return smp.solids; },
    /* where you are to go now, if anywhere: { name, x, z } (the wrist panel's arrow, the rover's navigation display) */
    target() {
      const s = st.step, at = (name, x, z) => ({ name, x, z });
      if (s === 'flag-plant') return at('The flag', st.flagAt.x, st.flagAt.z);
      if (s === 'rover' || (s === 'free' && !R.driving)) return at('The rover', R.x, R.z);
      if (s === 'samples') {
        const n = smp.nearest(o.S.pos.x, o.S.pos.z);
        if (R.driving || !n || Math.hypot(o.S.pos.x - station.x, o.S.pos.z - station.z) > 30) return at('Station 1', station.x, station.z);
        const held = smp.rocks.some(q => q.state === 'held');
        return held ? at('The rover', R.x, R.z) : at('Nearest rock', n.x, n.z);
      }
      if (s === 'film-go') return at('The LM', lmAt.x, lmAt.z);
      if (s === 'film-pose' && F.me) return at('Your crewmate', F.me[0], F.me[1]);
      if (s === 'load') return F.bag ? at('The ladder', lmAt.x, lmAt.z) : at('The rover', R.x, R.z);
      return null;
    },
    /* something of yours is in a hand (the flag's pole, a rock): the triggers are not for jumping then */
    get busy() { return !!st.holding || smp.rocks.some(q => q.state === 'held'); },
    /* the things you cannot jump about close to: [x, z] of the rover, the flag, him */
    get things() { const a = [[R.x, R.z]]; if (flag.visible) a.push([flag.position.x, flag.position.z]); if (o.lmp && o.lmp.on) a.push([o.lmp.S.pos.x, o.lmp.S.pos.z]); return a; },
    /* the ladder is yours (he is not on his way up it) */
    get ladderFree() { return !(o.lmp && o.lmp.boarding); },
    rate: 1,
    /* for the wrist panel: where you are with the missions */
    status() {
      const rows = [], tg = api.target();
      /* the way to it: an arrow as you look (up is ahead), and how far */
      const way = t => {
        const fw = o.camera.getWorldDirection(_w), hx = o.camera.matrixWorld.elements[12], hz = o.camera.matrixWorld.elements[14];
        const dx = t.x - hx, dz = t.z - hz, turn = Math.atan2(fw.x * dz - fw.z * dx, fw.x * dx + fw.z * dz);
        return ARROWS[((Math.round(turn / (Math.PI / 4)) % 8) + 8) % 8] + '  ' + Math.hypot(dx, dz).toFixed(0) + ' m';
      };
      if (st.step === 'samples') rows.push(['Samples', smp.kg.toFixed(0) + ' of ' + smp.target + ' kg']);
      if (tg) rows.push([tg.name, way(tg)]);
      if (st.step === 'film-go') rows.push(['The rover', 'park it 20-30 m from the LM']);
      if (st.step === 'film-pose') rows.push(['The camera', F.canGo ? 'A when you are ready' : 'stand by your crewmate']);
      if (st.step === 'film-count' || st.step === 'film-rec') rows.push(['The camera', st.step === 'film-rec' ? 'recording: ' + Math.max(0, Math.ceil(REC - st.t)) + ' s left' : 'stand by']);
      if (st.step === 'load') rows.push(['Sample bag', !F.bag ? 'at the back of the rover' : o.lmp && o.lmp.boarding ? 'with you: he goes up first' : 'with you: up the ladder']);
      return rows;
    },
    /* a test's short cut to a later step */
    jump(step) {
      if (step === 'samples') { if (!smp.at) smp.spawn(station.x, station.z); st.step = 'samples'; }
      else if (step === 'film-go') { smp.kg = smp.target; st.step = 'film-go'; }
      else if (step === 'load') { setStage(); st.step = 'load'; }
      else if (step === 'aboard') { setStage(); F.bag = true; st.step = 'aboard'; o.lmp.board(); o.cabin(); }
      st.t = 0;
    },
    get driving() { return R.driving; },
    /* where your eyes are when seated, and which way the rover points */
    get eye() { return seatWorld('driver', eye, 0.8); }, get yaw() { return R.yaw; },
    get hips() { return seatWorld('driver', _v, 0); },
    /* a grip closing: the flag's pole, or the rover's seat */
    grab(hand, headGround) {
      if (R.driving) return false;
      const hw = hand && hand.pos ? o.rig.localToWorld(_v.copy(hand.pos)) : null;
      if (st.step === 'flag-plant') {
        const near = hw ? Math.hypot(hw.x - flag.position.x, hw.z - flag.position.z) < 0.5 && hw.y - flag.position.y > 0.4 && hw.y - flag.position.y < 2.3
          : headGround && Math.hypot(headGround.x - flag.position.x, headGround.z - flag.position.z) < 1.6;
        if (near) { st.holding = hand || true; st.lastY = hw ? hw.y : 0; if (!hw) st.planted = 1; return true; }
      }
      if (st.step === 'samples' && (hw ? smp.grab(hand) : smp.key(headGround))) return true;
      if (st.step === 'load' && !F.bag && headGround) {
        rover.localToWorld(_w.copy(rover.userData.bag));
        if (Math.hypot(headGround.x - _w.x, headGround.z - _w.z) < 1.8) {
          F.bag = true; if (o.step) o.step(0.6);
          /* he goes up first: the Commander was the last one on the Moon */
          if (o.lmp && o.lmp.on) { o.lmp.climb(); say('lmp-up'); }
          o.toast.say('You have the sample bag. Your crewmate goes up first; then up the ladder with it', 8, true); return true;
        }
      }
      if (['rover', 'free', 'samples', 'film-go', 'film-pose', 'load'].includes(st.step)) {
        /* beside the rover, on whichever side (it was not clear how to get on: only by the left seat before) */
        seatWorld('driver', _w, 0);
        if (headGround && (Math.hypot(headGround.x - _w.x, headGround.z - _w.z) < 1.9 || Math.hypot(headGround.x - R.x, headGround.z - R.z) < 2.9)) { api.mount(); return true; }
      }
      return false;
    },
    release(hand) { if (st.holding === hand) st.holding = null; smp.release(hand); },
    mount() {
      R.driving = true; R.v = 0; if (st.step === 'rover') st.step = 'free';
      if (o.lmp && o.lmp.on) o.lmp.ride(true);
      say('drive'); if (o.radio) o.radio.setNext('next-drive');
      if (!st.said.howto) { st.said.howto = true; o.toast.say('The left stick drives: forward, back, and steers. A, standing still, to step off', 8, true); }
    },
    button(name) {
      /* ready for the camera */
      if (name === 'a' && !R.driving && st.step === 'film-pose' && F.canGo) { F.go = true; return true; }
      if (name !== 'a' || !R.driving) return false;
      if (Math.abs(R.v) > 0.3) { o.toast.say('Stop first', 2, true); return true; }
      R.driving = false; R.v = 0;
      const c = Math.cos(R.yaw), s = Math.sin(R.yaw), px = R.x - 1.5 * c, pz = R.z + 1.5 * s, q = o.clearOf(px, pz);
      o.S.pos.set(q.x, 0, q.z); o.S.heading = R.yaw;
      if (o.lmp && o.lmp.on) o.lmp.ride(false, R.x + 1.6 * c, R.z - 1.6 * s, R.x, R.z, R.yaw);
      return true;
    },
    /* the keyboard's stand-in for a grip (G) */
    key(headGround) { return api.grab(null, headGround); },
    update(dt, ctx = {}) {
      dt *= api.rate; st.t += dt;
      const L = o.lmp, down = !!L && L.on && L.S.phase === 'roam';
      /* ---- kept for the film as it happens: the flag going up, a rock lifted and stowed (30 frames a second) */
      F.clock += dt; F.recT -= dt; const tick = F.recT <= 0; if (tick) F.recT = Math.max(0, F.recT + 1 / 30);
      const cf = clips.flag, cr = clips.rock, held = smp.rocks.some(q => q.state === 'held');
      if (cf.on === 0 && st.step === 'flag-plant' && st.holding) { cf.on = 1; cf.rover = [R.x, R.z, R.yaw]; }
      if (cf.on === 1) { if (tick) keep(cf.rec); if ((st.step === 'flag-up' && st.t > 5) || !['flag-plant', 'flag-up'].includes(st.step)) cf.on = 2; }
      if (cr.on === 0 && held) { cr.on = 1; cr.rec.clear(); cr.rover = [R.x, R.z, R.yaw]; cr.gone = smp.rocks.map(q => q.state === 'stowed'); cr.kg = smp.kg; }
      else if (cr.on === 1) { if (tick) keep(cr.rec); if (!held) { if (smp.kg > cr.kg) { cr.on = 2; cr.t = 1.6; } else cr.on = 0; } }
      else if (cr.on === 2) { if (tick) keep(cr.rec); cr.t -= dt; if (cr.t <= 0) cr.on = 3; }
      /* ---- the flag's story */
      if (st.step === 'wait') {
        if (ctx.walking && down) st.wait += dt;
        if (st.wait > 26) {
          st.step = 'flag-carry'; flag.visible = true; flag.userData.set(0); flag.userData.unfurl(0); flag.rotation.y = flagYaw;
          say('flag-go'); if (o.radio) o.radio.setNext('next-flag');
          L.command({ x: st.flagAt.x - 0.55, z: st.flagAt.z + 0.1, act: 'hold', hold: 1e9 });
        }
      } else if (st.step === 'flag-carry') {
        /* in his hand as he goes; at the spot he stands it on the ground and holds it for you */
        if (L.S.mode === 'go' || !L.arrived) { L.fig.updateMatrixWorld(true); flag.position.copy(L.fig.localToWorld(_v.set(0.42, 0.05, -0.28))); flag.position.y = Math.max(flag.position.y, o.moon.heightAt(flag.position.x, flag.position.z)); }
        if (L.arrived) { flag.position.copy(st.flagAt); st.step = 'flag-plant'; say('flag-here'); o.toast.say('The flag: take hold of the pole (grip) and push it down', 8); }
      } else if (st.step === 'flag-plant') {
        if (st.holding && st.holding !== true && st.holding.pos && st.holding.grip) {
          const y = o.rig.localToWorld(_v.copy(st.holding.pos)).y; if (y < st.lastY) st.planted += (st.lastY - y) / 0.32; st.lastY = y;
        } else if (st.holding && st.holding !== true) st.holding = null;
        flag.userData.set(Math.min(1, st.planted));
        if (st.planted >= 1) {
          st.step = 'flag-up'; st.t = 0; st.holding = null; if (o.step) o.step(1.2);
          L.command({ x: st.flagAt.x - 1.5, z: st.flagAt.z + 0.9, act: 'salute', hold: 7, face: [st.flagAt.x, st.flagAt.z] });
        }
      } else if (st.step === 'flag-up') {
        st.unfurl = Math.min(1, st.t / 1.6); flag.userData.unfurl(st.unfurl);
        if (st.unfurl >= 1 && !st.said.shadow) { st.said.shadow = true; flag.updateMatrixWorld(true); flagShadow.bake(o.renderer, flag, o.sun); flagShadow.drape(flag.position.x, flag.position.y, flag.position.z); flagShadow.mesh.visible = true; say('flag-done'); }
        if (st.t > 9) { st.step = 'rover'; say('rover-go'); if (o.radio) o.radio.setNext('next-rover'); L.command(null); }
      }
      else if (st.step === 'free') {
        if (R.driving) F.odo += Math.abs(R.v) * dt;
        if (F.odo > 30) { st.step = 'samples'; smp.spawn(station.x, station.z); say('samples-go'); if (o.radio) o.radio.setNext('next-samples'); o.toast.say('Station 1: the boulder field ahead of the LM. The wrist panel shows the way', 8); }
      } else if (st.step === 'samples') {
        /* at the field (within ten metres of its rocks), on the rover or off it: this is the place */
        { const n = smp.nearest(R.x, R.z); if ((n && n.d < 10) || Math.hypot(R.x - station.x, R.z - station.z) < 12) say('samples-at'); }
        /* how it is going, on the radio: the first one, twenty kilos, half, twenty to go */
        if (smp.mine >= 1) say('samples-first');
        if (smp.kg >= 20 && smp.n >= 2) say('samples-20');
        if (smp.kg >= 50) say('samples-half');
        if (smp.kg >= 80 && !smp.done) say('samples-80');
        if (smp.done) { st.step = 'film-go'; say('samples-done'); if (o.radio) o.radio.setNext('next-film'); }
      } else if (st.step === 'film-go') {
        const d = Math.hypot(R.x - lmAt.x, R.z - lmAt.z);
        if (!R.driving && d > 11 && d < 48) {
          /* the camera is turned on the LM; he goes to stand before it and calls you */
          st.step = 'film-pose'; setStage();
          L.command({ x: F.him[0], z: F.him[1], act: 'look', hold: 1e9, face: [F.cam.x, F.cam.z] });
          say('film-here'); if (o.radio) o.radio.setNext('next-wave');
        } else if (!R.driving && !st.said.far) { st.said.far = true; o.toast.say(d >= 48 ? 'Park the rover nearer the Lunar Module (20 to 30 m), then step off' : 'Park the rover further from the Lunar Module (20 to 30 m), then step off', 6); }
        else if (R.driving) st.said.far = false;
      } else if (st.step === 'film-pose') {
        /* nothing is kept until you say so: A, or a word on the radio (X held and let go) */
        F.canGo = L.arrived && !!ctx.head && Math.hypot(ctx.head.x - F.me[0], ctx.head.z - F.me[1]) < 2.4;
        const tx = !!(o.radio && o.radio.transmitting);
        if (F.canGo) {
          say('film-ready'); F.promptT -= dt;
          if (F.promptT <= 0) { F.promptT = 10; o.toast.say('Ready for the camera? Press A (or hold X and say so)', 9, true); }
          if (F.tx && !tx) F.go = true;
          if (F.go) { st.step = 'film-count'; st.t = 0; F.n = 4; }
        } else F.promptT = 0;
        F.tx = tx; F.go = false;
      } else if (st.step === 'film-count') {
        const n = 3 - Math.floor(st.t);
        if (n !== F.n && n > 0) { F.n = n; o.toast.say('Recording in ' + n, 1.2, true); if (o.step) o.step(0.25); }
        if (st.t >= 3) {
          st.step = 'film-rec'; st.t = 0; F.n = -1; rec.clear(); say('film-rec');
          L.command({ x: F.him[0], z: F.him[1], act: 'wave', hold: 1e9, face: [F.cam.x, F.cam.z] });
        }
      } else if (st.step === 'film-rec') {
        if (tick) keep(rec);
        const left = Math.max(0, Math.ceil(REC - st.t));
        if (left !== F.n) { F.n = left; o.toast.say('\u25CF REC   ' + left + ' s left: face the rover and wave', 1.3, true); }
        if (st.t > REC) { st.step = 'load'; L.command(null); o.toast.say('Cut. ' + REC + ' seconds of film are kept', 5, true); if (o.step) o.step(0.5); say('load-go'); if (o.radio) o.radio.setNext('next-load'); }
      } else if (st.step === 'load') {
        if (o.inCabin()) {
          if (F.bag) { st.step = 'aboard'; st.t = 0; if (!L.aboard) L.board(); o.cabin(); }
          else if (!st.said.nobag) { st.said.nobag = true; o.toast.say('The samples are still on the rover: take the bag from its back and bring it up', 7, true); }
        } else st.said.nobag = false;
      } else if (st.step === 'aboard') {
        /* he is in after you; the hatch swings shut; the go from Houston, the count, ignition */
        F.hatch = Math.min(1, st.t / 4); o.hatch(F.hatch);
        if (st.t > 4.5) say('aboard'); if (st.t > 9.5) say('go-liftoff'); if (st.t > 15.5) say('count');
        if (st.t > 23.5) { st.step = 'ascent'; st.t = 0; F.alt = 0; F.vel = 0; if (!F.me) setStage(); burst(); say('liftoff'); if (o.radio && o.radio.engine) o.radio.engine(1); if (o.step) o.step(1.5); }
      } else if (st.step === 'ascent' || st.step === 'fade') {
        F.vel += 1.75 * dt; F.alt += F.vel * dt; o.lift(F.alt, true);
        if (st.step === 'ascent' && F.alt >= 50) { st.step = 'fade'; st.t = 0; }
        if (st.step === 'fade') { F.fade = Math.min(1, st.t / 1.6); o.fade(F.fade);
          if (F.fade >= 1) { st.step = 'finale'; F.finale = true; F.tf = 0; if (o.radio && o.radio.engine) o.radio.engine(0); o.finale(); } }
      } else if (st.step === 'finale') film(dt);
      /* ---- the rover */
      if (R.driving) drive(dt, ctx.stick || [0, 0]); else { R.v = 0; R.thr = 0; R.steer *= Math.max(0, 1 - dt * 3); }
      settle(dt, false);
      /* through the suit: the motors and the rumble while you are on it, a thump when the springs take a bump */
      if (R.driving) { if (o.drive) o.drive(R.v, R.thr); tr.quiet = false; } else if (!tr.quiet) { tr.quiet = true; if (o.drive) o.drive(0, 0); }
      tr.bumpT -= dt;
      if (R.driving && tr.bumpT <= 0 && Math.abs(R.v) > 0.5 && Math.abs(R.bv) > 0.2) { tr.bumpT = 0.22; tr.bumps = (tr.bumps || 0) + 1; if (o.bump) o.bump(clamp((Math.abs(R.bv) - 0.12) * 2, 0.25, 1.5)); }
      /* the rear wheels' tracks: a stretch for every stretch rolled (the front wheels' lie under them) */
      if (o.track && Math.abs(R.v) > 0.05 && !F.finale) {
        const c = Math.cos(R.yaw), s = Math.sin(R.yaw), W = rover.userData.wheels, L = o.trackLen || 0.5;
        for (const [key, w] of [['l', W[2]], ['r', W[3]]]) {
          const wx = R.x + w.x * c + w.z * s, wz = R.z - w.x * s + w.z * c, p = tr[key];
          if (!p) { tr[key] = [wx, wz]; continue; }
          let d = Math.hypot(wx - p[0], wz - p[1]);
          if (d > 3) { tr[key] = [wx, wz]; continue; }                       /* (moved, not driven) */
          /* (as many stretches as it rolled since: at a low frame rate that is more than one) */
          const ux = (wx - p[0]) / (d || 1), uz = (wz - p[1]) / (d || 1);
          while (d >= L) { o.track(p[0] + ux * L / 2, p[1] + uz * L / 2, ux, uz); p[0] += ux * L; p[1] += uz * L; d -= L; tr.laid++; }
        }
      }
      /* the console's navigation display, a few times a second */
      tr.navT -= dt;
      if (tr.navT <= 0 && rover.userData.nav && !F.finale) {
        tr.navT = 0.2;
        const fx = -Math.sin(R.yaw), fz = -Math.cos(R.yaw), tg = api.target();
        const rel = (x, z) => Math.atan2(fx * (z - R.z) - fz * (x - R.x), fx * (x - R.x) + fz * (z - R.z));
        const on = tg && tg.name !== 'The rover';
        rover.userData.nav.draw({ heading: -R.yaw * 180 / Math.PI, name: on ? tg.name : '', rel: on ? rel(tg.x, tg.z) : 0, range: on ? Math.hypot(tg.x - R.x, tg.z - R.z) : 0,
          lmRel: rel(lmAt.x, lmAt.z), lmRange: Math.hypot(lmAt.x - R.x, lmAt.z - R.z), load: smp.at ? [smp.kg, smp.target] : null,
          board: !R.driving && ['rover', 'free'].includes(st.step) });
        /* by the rover for the first time: how to get on (said whatever the messages' setting) */
        if (!R.driving && st.step === 'rover' && !st.said.board && ctx.head && Math.hypot(ctx.head.x - R.x, ctx.head.z - R.z) < 4) { st.said.board = true; o.toast.say('The rover: pull a trigger or grip beside it to climb on', 7, true); }
      }
      if (!F.bsh) { F.bsh = true; boulderShadow.bake(o.renderer, smp.big, o.sun); boulderShadow.drape(smp.big.position.x, smp.big.position.y, smp.big.position.z); boulderShadow.mesh.visible = true; }
      if (R.driving && L && L.S.phase === 'ride') { seatWorld('mate', _v, 0); L.seat(_v, R.yaw); }
      R.bakeT += dt;
      if (R.bakeT > 0.15 && (Math.hypot(R.x - R.bx, R.z - R.bz) > 0.03 || Math.abs(R.yaw - R.byaw) > 0.01 || R.bx > 1e8)) { R.bakeT = 0; R.bx = R.x; R.bz = R.z; R.byaw = R.yaw; roverShadow.bake(o.renderer, rover, o.sun); }
      roverShadow.drape(R.x, R.y, R.z);
      help(dt, L, ctx.head);
      dust.update(dt); sparks.update(dt); smp.update(dt);
    }
  };
  void G;
  return api;
}
