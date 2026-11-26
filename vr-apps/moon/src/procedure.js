/* A procedure on the panel: switches that are yours to flip, one after another.
 *
 * The owner: "the 'Not now' suggests that there is a 'now', and the panel right in front of me says 'suit' ... They
 * could light up in the sequence they need to be flipped." So, at the few moments when it is the crew's own work
 * (suiting up before the walk outside is the first), a group of the panel's switches (cabin.controls) lights up one
 * at a time: touch the lit one with a fingertip and it flips, with a click in the hand, and the next lights. When
 * the last is done, what the procedure was for happens. A (auto()) does the rest of it for you.
 *
 *   const proc = makeProcedure(cabin, { click(), buzz(hand) });
 *   proc.start('SUIT', 4, () => { ... });      the group named so nearest your couch, at most four of its switches
 *   proc.update(dt, tips);                     every frame; tips: { left, right } fingertips in the cabin's frame
 *   proc.auto(); proc.cancel(); proc.active; proc.left */
import * as THREE from 'three';

export function makeProcedure(cabin, o = {}) {
  const P = { active: false, list: [], i: 0, auto: false, autoT: 0, cool: 0, t: 0, done: null, flipped: 0 };
  const _p = new THREE.Vector3();
  const light = () => cabin.controls.lamp(P.active && P.i < P.list.length ? P.list[P.i] : null);
  function flip(hand) {
    const k = P.list[P.i];
    cabin.controls.set(k, !cabin.controls.list[k].up); P.flipped++;
    if (o.click) o.click(); if (o.buzz && hand) o.buzz(hand);
    P.i++; P.cool = 0.3; P.autoT = 0.55;
    if (P.i >= P.list.length) { P.active = false; light(); const d = P.done; P.done = null; if (d) d(); } else light();
  }
  return {
    get active() { return P.active; }, get left() { return P.active ? P.list.length - P.i : 0; }, get flipped() { return P.flipped; }, get list() { return P.list; }, get at() { return P.i; },
    start(name, n, done) {
      const ks = cabin.controls.group(name).slice(0, n);
      P.list = ks; P.i = 0; P.auto = false; P.cool = 0.4; P.autoT = 0.6; P.t = 0; P.done = done; P.active = ks.length > 0;
      if (!P.active) { P.done = null; if (done) done(); return false; }
      light(); return true;
    },
    auto() { if (P.active) P.auto = true; },
    cancel() { P.active = false; P.done = null; P.auto = false; light(); },
    update(dt, tips) {
      if (!P.active) return;
      P.t += dt; P.cool -= dt; cabin.controls.pulse(P.t);
      if (P.auto) { P.autoT -= dt; if (P.autoT <= 0) flip(null); return; }
      if (P.cool > 0 || !tips) return;
      cabin.controls.pos(P.list[P.i], _p);
      for (const h of ['left', 'right']) if (tips[h] && tips[h].distanceTo(_p) < 0.045) { flip(h); return; }
    }
  };
}
