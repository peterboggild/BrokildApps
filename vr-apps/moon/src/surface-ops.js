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
 * to the left; he gets off too and stays near the rover.
 *
 *   const ops = makeSurfaceOps({ moon, renderer, sun, env, model, lmp, radio, toast, hands, rig, camera, S, clearOf,
 *                                step(strength), makeShadow, makeDust });
 *   scene.add(ops.group);                 (drawn with the near things)
 *   ops.update(dt, { walking, xr, stick: [x, y], headGround: Vector3 })      every frame on the surface
 *   ops.grab(hand)                        a grip closing (hand: { pos: Vector3 rig frame } or null on the screen); true if taken
 *   ops.button('a')                       A pressed; true if it was for the rover
 *   ops.driving, ops.seat                 seated on the rover (the page then places the rig from ops.eye and ops.yaw) */
import * as THREE from 'three';
import { makeFlag } from './flag.js?v=27';
import { makeRover, ROVER } from './rover.js?v=27';

const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const G = 1.62;

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

  const api = {
    group, flag, rover, R, st,
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
      if (st.step === 'rover' || st.step === 'free') {
        seatWorld('driver', _w, 0);
        if (headGround && Math.hypot(headGround.x - _w.x, headGround.z - _w.z) < 1.9) { api.mount(); return true; }
      }
      return false;
    },
    release(hand) { if (st.holding === hand) st.holding = null; },
    mount() {
      R.driving = true; R.v = 0; st.step = 'free';
      if (o.lmp && o.lmp.on) o.lmp.ride(true);
      say('drive'); if (o.radio) o.radio.setNext('next-drive');
      o.toast.say('The left stick drives: forward, back, and steers. A, standing still, to step off', 7);
    },
    button(name) {
      if (name !== 'a' || !R.driving) return false;
      if (Math.abs(R.v) > 0.3) { o.toast.say('Stop first', 2); return true; }
      R.driving = false; R.v = 0;
      const c = Math.cos(R.yaw), s = Math.sin(R.yaw), px = R.x - 1.5 * c, pz = R.z + 1.5 * s, q = o.clearOf(px, pz);
      o.S.pos.set(q.x, 0, q.z); o.S.heading = R.yaw;
      if (o.lmp && o.lmp.on) o.lmp.ride(false, R.x + 1.6 * c, R.z - 1.6 * s, R.x, R.z, R.yaw);
      return true;
    },
    /* the keyboard's stand-in for a grip (G) */
    key(headGround) { return api.grab(null, headGround); },
    update(dt, ctx = {}) {
      st.t += dt;
      const L = o.lmp, down = !!L && L.on && L.S.phase === 'roam';
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
      /* ---- the rover */
      if (R.driving) drive(dt, ctx.stick || [0, 0]); else { R.v = 0; R.thr = 0; R.steer *= Math.max(0, 1 - dt * 3); }
      settle(dt, false);
      if (R.driving && L && L.S.phase === 'ride') { seatWorld('mate', _v, 0); L.seat(_v, R.yaw); }
      R.bakeT += dt;
      if (R.bakeT > 0.15 && (Math.hypot(R.x - R.bx, R.z - R.bz) > 0.03 || Math.abs(R.yaw - R.byaw) > 0.01 || R.bx > 1e8)) { R.bakeT = 0; R.bx = R.x; R.bz = R.z; R.byaw = R.yaw; roverShadow.bake(o.renderer, rover, o.sun); }
      roverShadow.drape(R.x, R.y, R.z);
      dust.update(dt);
    }
  };
  void G;
  return api;
}
