/* Things floating free in the cabin, weightlessness made visible, and yours to take: close the grip (the middle
 * finger's button) near one and it is in your hand; open it and it floats on with the speed, direction and spin it
 * had in your hand. Floating, they drift and tumble slowly, and bounce softly off the cabin round you and off the
 * crew (they passed through the astronauts: owner's headset test).
 *
 * Everything is worked out in the cabin's own frame (the rig's: hands from their poses under the head), never in the
 * world, which moves at kilometres a second.
 *
 *   const fl = makeFloaters(cabin.group, { tether: true });
 *   fl.update(dt, { ctl, head, hands, enabled });       every frame, after ctl.update()
 * The items: a checklist card (with a short cord clipped to it in chapter 2) and a pen: a Fisher space pen, the
 * kind the Apollo crews carried, chrome with a pocket clip and a ballpoint. */
import * as THREE from 'three';

const rnd = (a, b) => a + Math.random() * (b - a);
/* the free space round the couches (cabin frame: you at the origin, -z to the panel, +x to the other couches) */
const BOX = { x0: -0.42, x1: 1.6, y0: -0.3, y1: 0.5, z0: -0.48, z1: 0.18 };

function cardTexture() {
  const c = document.createElement('canvas'); c.width = 256; c.height = 352; const x = c.getContext('2d');
  x.fillStyle = '#ece8da'; x.fillRect(0, 0, 256, 352);
  x.fillStyle = '#1d1d1d'; x.font = 'bold 18px Arial'; x.fillText('CSM CHECKLIST', 18, 30);
  x.font = '11px Arial'; x.fillText('TLI - 1:30', 180, 30);
  x.fillStyle = '#c03028'; x.fillRect(18, 38, 220, 2);
  let seed = 5; const r = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  x.fillStyle = '#2a2a2a';
  for (let y = 58; y < 336; y += 13) { x.fillRect(18, y, 6, 6); x.fillRect(30, y, 60 + r() * 120, 5); if (r() < 0.4) x.fillRect(200, y, 30 + r() * 10, 5); }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4; return t;
}

function makePen() {
  const g = new THREE.Group();
  const chrome = new THREE.MeshPhongMaterial({ color: 0xb8bcc2, specular: 0xffffff, shininess: 90 });
  const dark = new THREE.MeshPhongMaterial({ color: 0x1a1a1a, specular: 0x444444, shininess: 40 });
  /* along +y: the ballpoint at the bottom, the push button at the top; 13 cm, 9 mm across */
  const body = new THREE.Mesh(new THREE.CylinderGeometry(0.0045, 0.0045, 0.1, 16), chrome); body.position.y = 0.005; g.add(body);
  const cone = new THREE.Mesh(new THREE.CylinderGeometry(0.0045, 0.0013, 0.022, 16), chrome); cone.position.y = -0.056; g.add(cone);
  const point = new THREE.Mesh(new THREE.SphereGeometry(0.0012, 8, 6), dark); point.position.y = -0.0675; g.add(point);
  const ring = new THREE.Mesh(new THREE.CylinderGeometry(0.0047, 0.0047, 0.004, 16), dark); ring.position.y = 0.035; g.add(ring);
  const button = new THREE.Mesh(new THREE.CylinderGeometry(0.003, 0.0035, 0.008, 12), chrome); button.position.y = 0.059; g.add(button);
  const clip = new THREE.Mesh(new THREE.BoxGeometry(0.003, 0.034, 0.0015), chrome); clip.position.set(0, 0.034, 0.0056); g.add(clip);
  const foot = new THREE.Mesh(new THREE.SphereGeometry(0.0018, 8, 6), chrome); foot.position.set(0, 0.018, 0.0052); g.add(foot);
  return g;
}

export function makeFloaters(parent, opts = {}) {
  const group = new THREE.Group(); parent.add(group);
  const items = [];
  const add = (obj, r, p) => {
    obj.position.copy(p); obj.rotation.set(rnd(0, 6), rnd(0, 6), rnd(0, 6)); group.add(obj);
    items.push({ obj, r, v: new THREE.Vector3(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).multiplyScalar(0.006), w: new THREE.Vector3(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).multiplyScalar(0.25), held: null });
  };
  /* the checklist card, a cord clipped to its corner */
  const card = new THREE.Group();
  const face = new THREE.MeshLambertMaterial({ map: cardTexture() }), edge = new THREE.MeshLambertMaterial({ color: 0xd8d3c2 });
  card.add(new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.22, 0.003), [edge, edge, edge, edge, face, face]));
  if (opts.tether) {
    const pts = [new THREE.Vector3(0.07, 0.1, 0), new THREE.Vector3(0.1, 0.16, 0.02), new THREE.Vector3(0.08, 0.24, 0.05), new THREE.Vector3(0.12, 0.33, 0.04)];
    card.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 24, 0.0018, 5), new THREE.MeshLambertMaterial({ color: 0x333333 })));
    const clip = new THREE.Mesh(new THREE.BoxGeometry(0.014, 0.01, 0.006), new THREE.MeshLambertMaterial({ color: 0x8a8d90 })); clip.position.set(0.07, 0.1, 0); card.add(clip);
  }
  add(card, 0.12, new THREE.Vector3(0.35, 0.05, -0.38));
  add(makePen(), 0.07, new THREE.Vector3(-0.15, 0.12, -0.32));

  const _hm = new THREE.Matrix4(), _inv = new THREE.Matrix4(), _p = new THREE.Vector3(), _q = new THREE.Quaternion(), _s = new THREE.Vector3(), _dq = new THREE.Quaternion();
  const prevGrip = { left: false, right: false };
  /* the crew's bodies, as spheres in the cabin's frame (cabin.js gives every crewman his: userData.touch, in his own
   * frame; yours too): found among the cabin's things, the hidden left out, looked up again twice a second */
  let bodies = [], bodiesT = 0; const _b = new THREE.Vector3(), _n = new THREE.Vector3();
  function findBodies() {
    bodies = []; parent.updateMatrixWorld(true); _inv.copy(parent.matrixWorld).invert();
    parent.traverse(m => {
      if (!m.userData || !m.userData.touch) return;
      for (let q = m; q && q !== parent; q = q.parent) if (!q.visible) return;
      for (const [x, y, z, r] of m.userData.touch) { const c = new THREE.Vector3(x, y, z).applyMatrix4(m.matrixWorld).applyMatrix4(_inv); bodies.push([c, r]); }
    });
  }
  /* the hand's pose in the cabin's frame */
  function handInCabin(ctl, head, h, out) {
    head.updateMatrix(); ctl.grips[h].updateMatrix(); parent.updateMatrix();
    return out.copy(_inv.copy(parent.matrix).invert()).multiply(head.matrix).multiply(ctl.grips[h].matrix);
  }
  function update(dt, ctx = {}) {
    if (dt <= 0) return;
    const { ctl, head, hands, enabled = true } = ctx;
    /* hands: take, carry, let go */
    if (ctl && head) for (const h of ['left', 'right']) {
      const src = ctl.src[h], gp = src && src.gamepad, grip = !!(enabled && gp && gp.buttons[1] && gp.buttons[1].value > 0.55 && ctl.grips[h].visible);
      const hm = handInCabin(ctl, head, h, new THREE.Matrix4());
      hm.decompose(_p, _q, _s);
      if (grip && !prevGrip[h]) {
        let best = null, bd = 1e9;
        for (const it of items) { if (it.held) continue; const d = it.obj.position.distanceTo(_p) - it.r; if (d < 0.07 && d < bd) { bd = d; best = it; } }
        if (best) {
          best.obj.updateMatrix();
          best.held = { h, off: new THREE.Matrix4().copy(hm).invert().multiply(best.obj.matrix), hist: [] };
          gp.hapticActuators?.[0]?.pulse?.(0.3, 30)?.catch?.(() => {});
        }
      }
      for (const it of items) {
        if (!it.held || it.held.h !== h) continue;
        if (grip) {
          /* in the hand: follow it; remember the last few poses for the throw */
          const m = new THREE.Matrix4().multiplyMatrices(hm, it.held.off);
          m.decompose(it.obj.position, it.obj.quaternion, _s);
          it.held.hist.push({ t: dt, p: it.obj.position.clone(), q: it.obj.quaternion.clone() });
          if (it.held.hist.length > 6) it.held.hist.shift();
        } else {
          /* let go: on it floats with the hand's speed and spin over the last frames */
          const hs = it.held.hist; it.v.set(0, 0, 0); it.w.set(0, 0, 0);
          if (hs.length >= 2) {
            const a = hs[0], b = hs[hs.length - 1], T = hs.slice(1).reduce((s, x) => s + x.t, 0);
            it.v.copy(b.p).sub(a.p).divideScalar(Math.max(T, 1e-3)).clampLength(0, 3);
            _dq.copy(a.q).invert().premultiply(b.q);                /* b = dq a */
            if (_dq.w < 0) { _dq.x = -_dq.x; _dq.y = -_dq.y; _dq.z = -_dq.z; _dq.w = -_dq.w; }
            const ang = 2 * Math.acos(Math.min(1, _dq.w)), sn = Math.sqrt(Math.max(1e-9, 1 - _dq.w * _dq.w));
            it.w.set(_dq.x / sn, _dq.y / sn, _dq.z / sn).multiplyScalar(ang / Math.max(T, 1e-3)).clampLength(0, 12);
          }
          it.held = null;
        }
      }
      prevGrip[h] = grip;
      if (hands && hands[h]) hands[h].userData.holding = items.some(it => it.held && it.held.h === h);
    }
    /* free: drift and tumble, a whisper of air drag, soft bounces off the cabin round you and off the crew */
    bodiesT -= dt; if (bodiesT <= 0) { bodiesT = 0.5; findBodies(); }
    for (const it of items) {
      if (it.held) continue;
      const o = it.obj;
      o.position.addScaledVector(it.v, dt);
      for (const [c, r] of bodies) {
        const reach = r + it.r * 0.45, d = _b.copy(o.position).sub(c).length();
        if (d >= reach) continue;
        _n.copy(_b).divideScalar(d || 1); if (d < 1e-6) _n.set(0, 1, 0);
        o.position.copy(c).addScaledVector(_n, reach);                               /* out of him */
        const vn = it.v.dot(_n);
        if (vn < 0) { it.v.addScaledVector(_n, -1.5 * vn); it.w.multiplyScalar(0.7).add(_s.set(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).multiplyScalar(-vn * 4)); it.bounces = (it.bounces || 0) + 1; }
        if (it.v.dot(_n) < 0.004) it.v.addScaledVector(_n, 0.004);                    /* (and never left resting against him) */
      }
      const wl = it.w.length(); if (wl > 1e-6) o.quaternion.premultiply(_dq.setFromAxisAngle(_s.copy(it.w).divideScalar(wl), wl * dt));
      const drag = Math.exp(-dt * 0.03); it.v.multiplyScalar(drag); it.w.multiplyScalar(Math.exp(-dt * 0.02));
      for (const [ax, lo, hi] of [['x', BOX.x0, BOX.x1], ['y', BOX.y0, BOX.y1], ['z', BOX.z0, BOX.z1]]) {
        if (o.position[ax] < lo && it.v[ax] < 0) { o.position[ax] = lo; it.v[ax] *= -0.45; it.w.multiplyScalar(0.7).add(_s.set(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).multiplyScalar(it.v.length() * 4)); }
        if (o.position[ax] > hi && it.v[ax] > 0) { o.position[ax] = hi; it.v[ax] *= -0.45; it.w.multiplyScalar(0.7).add(_s.set(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).multiplyScalar(it.v.length() * 4)); }
      }
    }
  }
  return { group, items, update, get bodies() { return bodies; }, findBodies };
}
