/* Out of the Lunar Module and down to the Moon (stage 5, part 2), on the surface page (main.js, ?egress).
 *
 * You stand at the Commander's station in the landed LM, the valley in the windows. The cabin is let down to vacuum,
 * the Lunar Module Pilot swings the forward hatch in. Then, as it was done: backwards out through the hatch on
 * hands and knees, facing the cabin, along the porch, feet first down the ladder on the front leg, the drop from the
 * last rung to the footpad (0.8 m, falling in the Moon's gravity), and the step off the pad onto the ground. From
 * there the free walk takes over.
 *
 * Moving: close the grip and pull, as in the tunnel (a gripping hand stays where it took hold, so you move the other
 * way), along one path: the hatch, the porch, the ladder. Or A (F on the screen): it goes by itself; A again stops.
 * When you set out, the view blinks and you are turned to face the cabin, whichever way you stood: you need not
 * turn round in your room. Your own height is kept: on the ladder and the ground your feet are where they would
 * be; crawling, your eyes are where a crawling man's would be.
 *
 * Everything is in the LM's own frame (lm-model.js: y up from the engine bell, +z forward), carried to the site by the
 * model's matrix. */
import * as THREE from 'three';
import { LM } from './lm-model.js?v=20';
import { LMC } from './lm-cabin.js?v=20';

const G_MOON = 1.62;

export function makeEgress(o) {
  /* o: model (the LM, placed), lmc (its cabin), moon, rig, camera, toast, suit, prints, eye(): the eye in the rig's
   * frame (Vector3), yaw(): the view's own yaw there (rad), resetLook(), done(x, z, heading) */
  const E = { active: false, stage: 'off', s: 0, auto: false, fade: 0, fadeTo: 0, door: 0, t: 0, H: 1.62, theta: 0, off: new THREE.Vector3(),
    vy: 0, drop: null, pending: 0, said: {} };
  const M = new THREE.Matrix4(), ry = () => o.model.rotation.y;
  const V = (x, y, z) => new THREE.Vector3(x, y, z);
  let nodes = [], total = 0, iDrop = 0;
  const _p = new THREE.Vector3(), _t = new THREE.Vector3(), _q = new THREE.Quaternion(), _Y = new THREE.Vector3(0, 1, 0), _d = new THREE.Vector3();
  const toSite = v => v.applyMatrix4(M);

  /* the way out: where your eyes' column stands (x, z) and the level of the floor under you (y), LM frame */
  function buildPath() {
    const H = E.H, crawl = (y, z) => V(0, y - H, z), ladder = o.model.userData.ladder, P = LM.PORCH;
    nodes = [crawl(3.02, 0.8), crawl(2.96, LM.FRONT + 0.05), crawl(2.98, 2.2), crawl(3.0, P.z1 - 0.17)];
    for (let k = 2; k < ladder.length; k++) nodes.push(V(0, ladder[k].y, ladder[k].z + 0.22));
    iDrop = nodes.length - 1;                                    /* the last rung: from here you let go */
    nodes.push(V(0, LM.FOOT_Y, LM.FOOT_R));                      /* on the footpad */
    /* the ground a step out from the pad, at the terrain's own height there */
    const g = toSite(V(0, 0, LM.FOOT_R + 1.0)), gy = o.moon.heightAt(g.x, g.z);
    nodes.push(V(0, gy - o.model.position.y, LM.FOOT_R + 1.0));
    let acc = 0; nodes.forEach((n, i) => { if (i) acc += n.distanceTo(nodes[i - 1]); n.s = acc; });
    total = acc;
  }
  function at(s, out) {
    s = Math.min(total, Math.max(0, s));
    let i = 1; while (i < nodes.length - 1 && nodes[i].s < s) i++;
    const a = nodes[i - 1], b = nodes[i], k = (s - a.s) / Math.max(1e-6, b.s - a.s);
    if (out.tangent) out.tangent.subVectors(b, a).normalize();
    return out.pos.lerpVectors(a, b, k);
  }
  const say = (key, text, secs) => { if (E.said[key]) return; E.said[key] = true; o.toast.say(text, secs); };

  /* stand the rig so that your eyes' column is over (x, z) of the LM frame, your floor at y, turned by theta */
  function place(pLM, theta) {
    _p.copy(pLM); toSite(_p);
    _t.copy(E.off).applyAxisAngle(_Y, theta);
    o.rig.position.set(_p.x - _t.x, _p.y, _p.z - _t.z);
    o.rig.quaternion.setFromAxisAngle(_Y, theta);
  }
  function capture() { const e = o.eye(); E.off.set(e.x, 0, e.z); E.H = Math.min(1.95, Math.max(1.2, e.y)); }

  const out = { pos: new THREE.Vector3(), tangent: new THREE.Vector3() };
  const api = {
    E, get active() { return E.active; }, get fade() { return E.fade; },
    /* the cabin is drawn while you are in it or near the hatch */
    get cabin() { return E.active; },
    begin() {
      o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
      E.active = true; E.stage = 'cabin'; E.t = 0; E.s = 0; E.auto = false; E.door = 0; E.said = {}; E.pending = 1; E.fade = 1; E.fadeTo = 0;
      o.lmc.setForwardHatch(0); o.model.userData.hatch.visible = true; o.lmc.setBody(true);
      /* the Lunar Module Pilot stands back, clear of the hatch as it swings in */
      o.lmc.lmp.position.x = -0.66; o.lmc.lmp.position.z = 0.5;
      E.theta = ry() + Math.PI; capture();
    },
    /* already down: the hatch stands open behind you, the walk is yours (chapter 6 started on the ground) */
    skip() {
      o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
      E.active = false; E.stage = 'done'; E.fade = 0; E.fadeTo = 0; E.door = 1;
      o.lmc.setForwardHatch(1); o.model.userData.hatch.visible = false; o.lmc.setBody(false);
      o.lmc.lmp.position.x = -0.66; o.lmc.lmp.position.z = 0.5;
    },
    /* recapture where you stand (entering VR: the headset's own origin is anywhere) */
    recentre() { E.pending = 1; },
    /* A, or F: set out, or go on by itself / stop */
    go() {
      if (E.stage === 'cabin') { if (E.door >= 1) { E.stage = 'turn'; E.fadeTo = 1; E.auto = true; } }
      else if (E.stage === 'path') E.auto = !E.auto;
      else if (E.stage === 'pad') E.auto = true;
    },
    /* hands: [{ grip: bool, pos: Vector3 (rig frame) }, ...] */
    update(dt, hands = []) {
      if (!E.active) return;
      E.t += dt;
      E.fade += (E.fadeTo - E.fade) * Math.min(1, dt * 5);
      if (E.pending && ++E.pending > 4) { capture(); E.pending = 0; if (E.stage === 'cabin') E.theta = ry() + Math.PI - o.yaw(); }   /* facing the windows, whichever way you stand */
      if (E.stage === 'cabin') {
        place(V(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z), E.theta);
        if (E.t > 2) say('dep', 'The cabin is down to vacuum. The hatch is coming open.', 5);
        if (E.t > 4) { E.door = Math.min(1, E.door + dt / 4); o.lmc.setForwardHatch(E.door * E.door * (3 - 2 * E.door)); o.model.userData.hatch.visible = false; }
        if (E.door >= 1) say('go', 'Out backwards through the hatch: grip and pull, or A', 1e6);
        if (E.door >= 1 && hands.some(h => h.grip)) { E.stage = 'turn'; E.fadeTo = 1; }
      } else if (E.stage === 'turn') {
        place(V(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z), E.theta);
        if (E.fade > 0.93) {
          /* in the dark: on your knees at the hatch, turned to face the cabin whichever way you stood */
          capture(); buildPath(); o.lmc.setBody(false);
          E.theta = ry() - o.yaw(); E.s = 0; E.stage = 'path'; E.fadeTo = 0;
          for (const h of hands) h.prev = null;
          E.said.go = false; say('path', 'Facing the cabin. Back out along the porch and down the ladder', 6);
        }
      } else if (E.stage === 'path') {
        at(E.s, out);
        const ds = pull(hands);
        if (ds !== null) { E.s += ds; E.auto = false; }
        else if (E.auto) E.s += (E.s > nodes[3].s ? 0.22 : 0.3) * dt;
        E.s = Math.max(0, E.s);
        if (E.s > nodes[2].s) say('porch', 'On the porch. Feet first down the ladder', 5);
        if (E.s >= nodes[iDrop].s) {
          /* the last rung: let go, and fall to the footpad in the Moon's gravity */
          E.s = nodes[iDrop].s; E.stage = 'drop'; E.vy = 0;
          E.drop = { from: nodes[iDrop].clone(), to: nodes[iDrop + 1].clone(), y: nodes[iDrop].y };
          say('drop', 'The last rung. Let go: it is a long way down to the pad', 4);
        }
        place(at(E.s, out), E.theta);
      } else if (E.stage === 'drop') {
        const D = E.drop; E.vy -= G_MOON * dt; D.y += E.vy * dt;
        const k = Math.min(1, (D.from.y - D.y) / (D.from.y - D.to.y));
        _p.lerpVectors(D.from, D.to, k); _p.y = Math.max(D.to.y, D.y);
        place(_p, E.theta);
        if (D.y <= D.to.y) { E.stage = 'pad'; E.s = nodes[iDrop + 1].s; E.t = 0; if (o.suit) o.suit.step(1.2); say('pad', 'On the footpad. One step back, onto the Moon', 6); }
      } else if (E.stage === 'pad') {
        at(E.s, out);
        const ds = pull(hands);
        if (ds !== null) E.s += ds; else if (E.auto && E.t > 1.2) E.s += 0.35 * dt;
        E.s = Math.max(nodes[iDrop + 1].s, E.s);
        place(at(E.s, out), E.theta);
        if (E.s >= total) {
          /* the first step: two prints where you stand, and the walk is yours */
          E.stage = 'done'; E.active = false;
          const p = o.rig.position, f = _t.set(-Math.sin(E.theta), 0, -Math.cos(E.theta));
          _p.copy(nodes[nodes.length - 1]); toSite(_p);
          if (o.prints) { o.prints.add(_p.x - f.z * 0.12, _p.z + f.x * 0.12, f.x, f.z, 1); o.prints.add(_p.x + f.z * 0.12, _p.z - f.x * 0.12, f.x, f.z, -1); }
          if (o.suit) o.suit.step(0.8);
          o.toast.say('You are standing on the Moon. Taurus-Littrow, 11 December 1972', 8);
          o.done(p.x, p.z, E.theta);
        }
      }
    }
  };
  /* hand over hand: the gripping hands' movement along the path (out.tangent), turned into yours the other way; null
   * when no hand grips */
  function pull(hands) {
    let ds = 0, n = 0;
    _t.copy(out.tangent).transformDirection(M);                 /* the path's direction at the site */
    for (const h of hands) {
      if (h.grip && h.pos) { if (h.prev) { _d.subVectors(h.pos, h.prev).applyAxisAngle(_Y, E.theta); ds -= _d.dot(_t); n++; } h.prev = (h.prev || new THREE.Vector3()).copy(h.pos); }
      else h.prev = null;
    }
    return n ? Math.max(-0.08, Math.min(0.08, ds / n)) : null;
  }
  return api;
}
