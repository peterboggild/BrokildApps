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
 * It works both ways: from the ground a grip or a trigger by the front footpad puts you on the pad at the ladder
 * (mount()); a pull up and you jump for the last rung, climb, crawl in, and stand up in the cabin; out again as before.
 * On the pad one push back from the ladder sets you down on the ground in one gliding step.
 * When you set out, the view blinks and you are turned to face the cabin, whichever way you stood: you need not
 * turn round in your room. Your own height is kept: on the ladder and the ground your feet are where they would
 * be; crawling, your eyes are where a crawling man's would be.
 *
 * Everything is in the LM's own frame (lm-model.js: y up from the engine bell, +z forward), carried to the site by the
 * model's matrix. */
import * as THREE from 'three';
import { LM } from './lm-model.js?v=35';
import { LMC } from './lm-cabin.js?v=35';

const G_MOON = 1.62;

export function makeEgress(o) {
  /* o: model (the LM, placed), lmc (its cabin), moon, rig, camera, toast, suit, prints, eye(): the eye in the rig's
   * frame (Vector3), yaw(): the view's own yaw there (rad), resetLook(), done(x, z, heading); helmet: your own helmet
   * as you see it (eva-suit.js makeHelmetView), for the suiting-up before the cabin is let down */
  const E = { active: false, stage: 'off', s: 0, auto: false, fade: 0, fadeTo: 0, door: 0, t: 0, H: 1.62, theta: 0, off: new THREE.Vector3(),
    vy: 0, drop: null, pending: 0, said: {}, dir: 1, acc: 0, armed: true, hop: null };
  const M = new THREE.Matrix4(), ry = () => o.model.rotation.y;
  const V = (x, y, z) => new THREE.Vector3(x, y, z);
  let nodes = [], total = 0, iDrop = 0;
  const _p = new THREE.Vector3(), _t = new THREE.Vector3(), _q = new THREE.Quaternion(), _Y = new THREE.Vector3(0, 1, 0), _d = new THREE.Vector3();
  const toSite = v => v.applyMatrix4(M);

  /* the way out: where your eyes' column stands (x, z) and the level of the floor under you (y), LM frame */
  function buildPath() {
    const H = E.H, crawl = (y, z) => V(0, y - H, z), ladder = o.model.userData.ladder, P = LM.PORCH;
    nodes = [crawl(3.02, 0.8), crawl(2.96, LM.FRONT + 0.05), crawl(2.98, 2.2), crawl(3.0, P.z1 - 0.17)];
    /* The ladder leans with the leg (it comes 0.47 m nearer the LM for each metre up), and you stand upright in your
     * room where the astronauts leaned onto it. So your eyes' column goes where the rungs at chest height are an
     * arm's bend away: a little inside the rung your feet are on (owner's headset test: the ladder was two or three
     * feet beyond the hands). */
    for (let k = 2; k < ladder.length; k++) nodes.push(V(0, ladder[k].y, ladder[k].z - 0.18));
    iDrop = nodes.length - 1;                                    /* the last rung: from here you let go */
    nodes.push(V(0, LM.FOOT_Y, LM.FOOT_R - 0.12));               /* on the footpad */
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
  const say = (key, text, secs) => { if (E.said[key]) return; E.said[key] = true; o.toast.say(text, secs); if (o.voice) o.voice(key); };

  /* stand the rig so that your eyes' column is over (x, z) of the LM frame, your floor at y, turned by theta */
  function place(pLM, theta, liftK = 1) {
    _p.copy(pLM); toSite(_p);
    _t.copy(E.off).applyAxisAngle(_Y, theta);
    const k = liftK;
    o.rig.position.set(_p.x - _t.x, _p.y + (E.lift || 0) * k, _p.z - _t.z);
    o.rig.quaternion.setFromAxisAngle(_Y, theta);
  }
  /* your eyes are kept at standing eye height (1.6 m over what you stand on), whatever your own height or whether you
   * sit, until you are on the ground: there your own height takes over, smoothly, over the last step */
  function capture() { const e = o.eye(); E.off.set(e.x, 0, e.z); E.H = 1.6; E.lift = 1.6 - e.y; }

  const out = { pos: new THREE.Vector3(), tangent: new THREE.Vector3() };
  const api = {
    E, get active() { return E.active; }, get fade() { return E.fade; },
    /* the cabin is drawn while you are in it or near the hatch */
    get cabin() { return E.active; },
    begin() {
      o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
      E.active = true; E.stage = 'cabin'; E.t = 0; E.s = 0; E.auto = false; E.armed = true; E.dir = 1; E.acc = 0; E.door = 0; E.said = {}; E.pending = 1; E.fade = 1; E.fadeTo = 0;
      o.lmc.setForwardHatch(0); o.model.userData.hatch.visible = true; o.lmc.setBody(true);
      if (o.helmet) { o.helmet.set(0); o.lmc.setSuitUp(0); }
      /* the Lunar Module Pilot stands back, clear of the hatch as it swings in */
      o.lmc.lmp.position.x = -0.5; o.lmc.lmp.position.z = 0.36;
      E.theta = ry() + Math.PI; capture();
    },
    /* already down: the hatch stands open behind you, the walk is yours (chapter 6 started on the ground) */
    skip() {
      o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
      E.active = false; E.stage = 'done'; E.fade = 0; E.fadeTo = 0; E.door = 1;
      o.lmc.setForwardHatch(1); o.model.userData.hatch.visible = false; o.lmc.setBody(false);
      if (o.helmet) { o.helmet.set(1); o.lmc.setSuitUp(1); }
      o.lmc.lmp.position.x = -0.5; o.lmc.lmp.position.z = 0.36;
    },
    /* from the ground, by the front footpad: back onto the pad and the ladder (and so up into the cabin) */
    mount() {
      if (E.active || E.stage !== 'done') return;
      E.active = true; E.stage = 'mount'; E.fadeTo = 1; E.auto = false;
    },
    /* recapture where you stand (entering VR: the headset's own origin is anywhere) */
    recentre() { E.pending = 1; },
    /* A, or F: set out, or go on by itself / stop */
    go() {
      if (E.stage === 'cabin') { if (E.door >= 1) { E.stage = 'turn'; E.fadeTo = 1; E.auto = true; } }
      else if (E.stage === 'path') E.auto = !E.auto;
      else if (E.stage === 'pad') { E.auto = true; E.t = Math.max(E.t, 5); }
    },
    /* hands: [{ grip: bool, pos: Vector3 (rig frame) }, ...] */
    update(dt, hands = []) {
      if (!E.active) return;
      E.t += dt;
      E.fade += (E.fadeTo - E.fade) * Math.min(1, dt * 5);
      if (E.pending && ++E.pending > 4) { capture(); E.pending = 0; if (E.stage === 'cabin') E.theta = ry() + Math.PI - o.yaw(); }   /* facing the windows, whichever way you stand */
      if (E.stage === 'cabin') {
        place(V(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z), E.theta);
        /* first the helmets (the journey was flown bare-headed): his, then yours, down over your head */
        const T0 = o.helmet ? 9.5 : 0;
        if (o.helmet && E.t < T0 + 1) {
          if (E.t > 0.8) say('helm', 'Helmets on: your crewmate\u2019s, then yours', 6);
          o.lmc.setSuitUp((E.t - 1) / 4); o.helmet.set((E.t - 5.2) / 3.8);
        }
        if (E.t > T0 + 2) say('dep', 'The cabin is down to vacuum. The hatch is coming open.', 5);
        if (E.locked) { /* shut for the lift-off (surface-ops.js) */ } else
        if (E.t > T0 + 4) { E.door = Math.min(1, E.door + dt / 4); o.lmc.setForwardHatch(E.door * E.door * (3 - 2 * E.door)); o.model.userData.hatch.visible = false; }
        if (E.door >= 1) say('go', 'Out backwards through the hatch: grip and pull, or A', 1e6);
        if (!hands.some(h => h.grip)) E.armed = true;                  /* (a grip still closed from the climb in does not send you out again) */
        if (E.door >= 1 && E.armed && hands.some(h => h.grip)) { E.stage = 'turn'; E.fadeTo = 1; E.dir = 1; }
      } else if (E.stage === 'turn') {
        place(V(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z), E.theta);
        if (E.fade > 0.93) {
          /* in the dark: on your knees at the hatch, turned to face the cabin whichever way you stood */
          capture(); buildPath(); o.lmc.setBody(false);
          E.theta = ry() - o.yaw(); E.s = 0; E.stage = 'path'; E.fadeTo = 0; E.dir = 1; E.acc = 0;
          for (const h of hands) h.prev = null;
          E.said.go = false; say('path', 'Facing the cabin. Back out along the porch and down the ladder', 6);
        }
      } else if (E.stage === 'path') {
        at(E.s, out);
        const ds = pull(hands);
        if (ds !== null) { E.s += ds; E.auto = false; E.acc = E.s <= 0 ? E.acc + ds : 0; }
        else if (E.auto) E.s += E.dir * (E.s > nodes[3].s ? 0.22 : 0.3) * dt;
        /* back at the hatch, and still pulling in (or going by itself): into the cabin, and up on your feet */
        if (E.s <= 0 && (E.acc < -0.06 || (E.auto && E.dir < 0))) { E.stage = 'in'; E.fadeTo = 1; E.auto = false; }
        E.s = Math.max(0, E.s);
        if (E.s > nodes[2].s && E.dir > 0) say('porch', 'On the porch. Feet first down the ladder', 5);
        if (E.s >= nodes[iDrop].s) {
          /* the last rung: let go, and fall to the footpad in the Moon's gravity */
          E.s = nodes[iDrop].s; E.stage = 'drop'; E.vy = 0; E.dir = 1;
          E.drop = { from: nodes[iDrop].clone(), to: nodes[iDrop + 1].clone(), y: nodes[iDrop].y };
          say('drop', 'The last rung. Let go: it is a long way down to the pad', 4);
        }
        place(at(E.s, out), E.theta);
      } else if (E.stage === 'in') {
        place(at(0, out), E.theta);
        if (E.fade > 0.93) {
          E.stage = 'cabin'; E.t = 1e3; E.door = 1; E.armed = false; E.dir = 1; E.fadeTo = 0; capture();
          E.theta = ry() + Math.PI - o.yaw(); o.lmc.setBody(true); for (const h of hands) h.prev = null;
          o.toast.say('Back in the cabin. Out again: grip and pull, or A', 6);
        }
      } else if (E.stage === 'drop') {
        const D = E.drop; E.vy -= G_MOON * dt; D.y += E.vy * dt;
        const k = Math.min(1, (D.from.y - D.y) / (D.from.y - D.to.y));
        _p.lerpVectors(D.from, D.to, k); _p.y = Math.max(D.to.y, D.y);
        place(_p, E.theta);
        if (D.y <= D.to.y) { E.stage = 'pad'; E.s = nodes[iDrop + 1].s; E.t = 0; E.acc = 0; if (o.suit) o.suit.step(1.2); say('pad', 'On the footpad. One push back from the ladder, and you are on the Moon', 6); }
      } else if (E.stage === 'mount') {
        /* from the ground (the view blinks): onto the footpad, facing the ladder, whichever way you stood */
        if (E.fade > 0.93) {
          o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
          capture(); buildPath(); E.theta = ry() - o.yaw();
          E.stage = 'pad'; E.s = nodes[iDrop + 1].s; E.t = 0; E.acc = 0; E.auto = false; E.dir = -1; E.fadeTo = 0;
          for (const h of hands) h.prev = null;
          place(nodes[iDrop + 1], E.theta);
          o.toast.say('On the footpad, at the ladder. Pull yourself up, or A. A push back sets you down again', 7);
        }
      } else if (E.stage === 'pad') {
        /* standing on the pad, the ladder before you. One push back from it (or A, coming down) and you glide back
         * onto the ground in one long step, as the other astronaut does; a pull up (or A, going up) and you jump for
         * the last rung (owner's headset test: it took several pushes) */
        out.tangent.subVectors(nodes[iDrop + 1], nodes[iDrop]).normalize();
        const ds = pull(hands);
        if (ds !== null) E.acc += ds; else E.acc *= Math.max(0, 1 - dt * 2);
        place(nodes[iDrop + 1], E.theta);
        const off = E.acc > 0.035 || (E.auto && E.dir > 0 && E.t > 1.2), up = E.acc < -0.035 || (E.auto && E.dir < 0 && E.t > 0.8);
        if (off) E.hop = { from: nodes[iDrop + 1], to: nodes[nodes.length - 1], t: 0, T: 1.1, arc: 0.22, lift: [1, 0], then: 'done' };
        else if (up) E.hop = { from: nodes[iDrop + 1], to: nodes[iDrop], t: 0, T: 0.9, arc: 0.12, lift: [1, 1], then: 'path' };
        if (off || up) { E.stage = 'hop'; E.acc = 0; if (o.suit) o.suit.step(0.6); }
      } else if (E.stage === 'hop') {
        const H = E.hop; H.t += dt; const k = Math.min(1, H.t / H.T);
        _p.lerpVectors(H.from, H.to, k); _p.y += H.arc * 4 * k * (1 - k);
        place(_p, E.theta, H.lift[0] + (H.lift[1] - H.lift[0]) * k);
        if (k >= 1 && H.then === 'path') { E.stage = 'path'; E.s = nodes[iDrop].s - 0.03; E.dir = -1; for (const h of hands) h.prev = null; }
        else if (k >= 1) {
          /* on the ground: two prints where you stand, and the walk is yours */
          E.stage = 'done'; E.active = false; E.s = total;
          const p = o.rig.position, f = _t.set(-Math.sin(E.theta), 0, -Math.cos(E.theta));
          _p.copy(nodes[nodes.length - 1]); toSite(_p);
          if (o.prints) { o.prints.add(_p.x - f.z * 0.12, _p.z + f.x * 0.12, f.x, f.z, 1); o.prints.add(_p.x + f.z * 0.12, _p.z - f.x * 0.12, f.x, f.z, -1); }
          if (o.suit) o.suit.step(0.8);
          say('moon', 'You are standing on the Moon. Taurus-Littrow, 11 December 1972', 8);
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
