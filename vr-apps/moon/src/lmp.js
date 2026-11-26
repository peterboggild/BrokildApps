/* The Lunar Module Pilot on the surface (chapter 6): the second man down.
 *
 * Some seconds after you have stepped onto the Moon he comes out as you did: backwards through the hatch on hands and
 * knees, along the porch, feet first down the ladder, the drop from the last rung to the footpad, a hop back onto the
 * ground. Then he moves about in front of the LM the way the crews found they had to in one sixth of the Earth's
 * gravity: the lope, long low hops from one foot to the other, leaning into it, half a second in the air each time.
 * He stops to look round, to work at something, and now and then turns to you and lifts an arm.
 *
 * What grounds him: his shadow (the low Sun throws it eight metres long, and it leaves his feet when he hops), his
 * bootprints, the dust each landing kicks out (it flies in clean arcs, there being no air), and the LM's own shadow:
 * in it he is lit only by the ground's glow.
 *
 * All motion is made in code from the suit's joints (eva-suit.js); no clips.
 *   const lmp = makeLMP({ moon, model, env, renderer, prints, toast, sun, cabinLmp, shade });
 *   scene.add(lmp.fig, lmp.extras);          the figure must be a direct child of the scene (its shadow is drawn from it)
 *   lmp.reset()          in the cabin still (the cabin's own figure shows)
 *   lmp.startOutside()   already down, moving about
 *   lmp.climb()          back to the LM, up the ladder and in through the hatch (before you: you were the last
 *                        one on the Moon); lmp.boarding while he is at it, lmp.aboard when he is in
 *   lmp.update(dt, { walking, player })      walking: you are down; player: where you stand (world) */
import * as THREE from 'three';
import { makeEVASuit } from './eva-suit.js?v=29';
import { LM } from './lm-model.js?v=29';

const G = 1.62;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const smooth = k => { k = clamp(k, 0, 1); return k * k * (3 - 2 * k); };
const wrap = a => Math.atan2(Math.sin(a), Math.cos(a));

/* A standing figure's shadow on the ground: the figure as the Sun sees it, drawn into a small texture (again whenever
 * you ask), laid out along the ground away from the Sun and draped over the terrain. Image geometry as in
 * lm-shadow.js: across is horizontal and square to the Sun; a point w up the image falls w / sin(el) down-Sun, and
 * h / tan(el) further when the figure is h above the ground (a hop: the shadow leaves the feet). */
export function makeFigureShadow(moon, opt = {}) {
  /* (opt: half the width across, the image's lower and upper edge, the texture's size, the drape's grid: for the
   * flag and the rover, which are wider than a man) */
  const HALF = opt.half ?? 1.0, W0 = opt.w0 ?? -0.8, W1 = opt.w1 ?? 2.8, NX = opt.nx ?? 6, NY = opt.ny ?? 40, SZ = opt.size ?? [128, 256];
  const rt = new THREE.WebGLRenderTarget(SZ[0], SZ[1], { samples: 4 });
  const geo = new THREE.PlaneGeometry(1, 1, NX, NY);
  const mesh = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: 0x000000, alphaMap: rt.texture, transparent: true, opacity: 0.9, depthWrite: false,
    polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -8 }));
  mesh.frustumCulled = false; mesh.renderOrder = 21;
  const cam = new THREE.OrthographicCamera(-HALF, HALF, W1, W0, 1, 60), sc = new THREE.Scene();
  sc.overrideMaterial = new THREE.MeshBasicMaterial({ color: 0xffffff });
  const sh = new THREE.Vector3(), upP = new THREE.Vector3(), right = new THREE.Vector3(1, 0, 0), R = new THREE.Vector3(), cc = new THREE.Color();
  let sinEl = 0.2, tanEl = 0.2, baked = false;
  /* fig: an Object3D whose parent (if it has one) does not move it (the scene) */
  function bake(renderer, fig, sun) {
    const el = Math.asin(clamp(sun.y, 0.03, 1)); sinEl = Math.sin(el); tanEl = Math.tan(el);
    sh.set(-sun.x, 0, -sun.z).normalize(); upP.set(sh.x * sinEl, Math.cos(el), sh.z * sinEl);
    const parent = fig.parent, vis = fig.visible;
    sc.add(fig); fig.visible = true; fig.updateMatrixWorld(true);
    R.setFromMatrixPosition(fig.matrixWorld);
    cam.up.copy(upP); cam.position.copy(R).addScaledVector(sun, 30); cam.lookAt(R); cam.updateMatrixWorld(true);
    right.setFromMatrixColumn(cam.matrixWorld, 0);
    const prev = renderer.getRenderTarget(), xr = renderer.xr.enabled, ca = renderer.getClearAlpha(); renderer.getClearColor(cc);
    renderer.xr.enabled = false; renderer.setRenderTarget(rt); renderer.setClearColor(0x000000, 1); renderer.clear();
    renderer.render(sc, cam);
    renderer.xr.enabled = xr; renderer.setRenderTarget(prev); renderer.setClearColor(cc, ca);
    if (parent) parent.add(fig); else sc.remove(fig);
    fig.visible = vis; baked = true;
  }
  /* lay it out from where the figure stands now (x, y, z: its feet) */
  function drape(x0, y0, z0) {
    if (!baked) return;
    const h = Math.max(0, y0 - moon.heightAt(x0, z0)), pos = geo.attributes.position;
    for (let j = 0; j <= NY; j++) for (let i = 0; i <= NX; i++) {
      const u = (i / NX - 0.5) * 2 * HALF, v = (W1 - j / NY * (W1 - W0)) / sinEl + h / tanEl;
      const x = x0 + right.x * u + sh.x * v, z = z0 + right.z * u + sh.z * v;
      pos.setXYZ(j * (NX + 1) + i, x, moon.heightAt(x, z) + 0.08, z);
    }
    pos.needsUpdate = true;
  }
  return { mesh, bake, drape };
}

/* dust thrown by a boot: grains on ballistic arcs (no air: no cloud, no drift), gone where they land */
export function makeDust(moon) {
  const N = 220, p = new Float32Array(N * 3).fill(-1e4), v = new Float32Array(N * 3), gy = new Float32Array(N), live = new Uint8Array(N);
  const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(p, 3));
  const points = new THREE.Points(geo, new THREE.PointsMaterial({ color: 0x8f8b82, size: 0.03, sizeAttenuation: true, transparent: true, opacity: 0.85, depthWrite: false }));
  points.frustumCulled = false;
  let next = 0, any = false;
  return {
    points,
    /* at (x, y, z), thrown mostly along (dx, dz) */
    emit(x, y, z, dx, dz, n, speed = 1) {
      for (let k = 0; k < n; k++) {
        const i = next; next = (next + 1) % N;
        const a = Math.atan2(dx, dz) + (Math.random() - 0.5) * 2.2, s = (0.35 + Math.random() * 0.9) * speed;
        p[i * 3] = x + (Math.random() - 0.5) * 0.16; p[i * 3 + 1] = y + 0.02; p[i * 3 + 2] = z + (Math.random() - 0.5) * 0.16;
        v[i * 3] = Math.sin(a) * s; v[i * 3 + 1] = (0.25 + Math.random() * 0.75) * speed; v[i * 3 + 2] = Math.cos(a) * s;
        gy[i] = y; live[i] = 1; any = true;
      }
    },
    update(dt) {
      if (!any) return;
      any = false;
      for (let i = 0; i < N; i++) {
        if (!live[i]) continue;
        v[i * 3 + 1] -= G * dt;
        p[i * 3] += v[i * 3] * dt; p[i * 3 + 1] += v[i * 3 + 1] * dt; p[i * 3 + 2] += v[i * 3 + 2] * dt;
        if (p[i * 3 + 1] < gy[i] - 0.05 || p[i * 3 + 1] < moon.heightAt(p[i * 3], p[i * 3 + 2])) { live[i] = 0; p[i * 3 + 1] = -1e4; } else any = true;
      }
      geo.attributes.position.needsUpdate = true;
    }
  };
}

/* the pose: numbers the joints are set from, eased towards a target every frame (so every change of stage blends) */
const KEYS = ['lean', 'wx', 'wy', 'alx', 'alz', 'arx', 'arz', 'el', 'er', 'll', 'lr', 'kl', 'kr', 'dy'];
const STAND = { lean: 0, wx: -0.08, wy: 0, alx: 0.35, alz: 0.3, arx: 0.35, arz: 0.3, el: 0.6, er: 0.6, ll: 0.1, lr: 0.1, kl: -0.16, kr: -0.16, dy: 0 };
/* where he goes in front of the LM (the LM's frame: x to the Commander's side, z forward, metres), and what he does there */
const SPOTS = [[2.6, 7.6, 'look'], [-3.4, 9.2, 'work'], [0.6, 12.5, 'look'], [-5.4, 5.8, 'work'], [4.2, 6.4, 'you'], [-1.5, 15, 'look']];

export function makeLMP(o) {
  /* his visor: a gold mirror of the cube map the page draws from his helmet (o.mirror), the valley, the LM and you in it */
  const fig = makeEVASuit({ env: o.env, visor: o.mirror ? new THREE.MeshBasicMaterial({ color: 0xf0bc58, envMap: o.mirror, reflectivity: 1 }) : null }); fig.rotation.order = 'YXZ'; fig.visible = false;
  const J = fig.userData.joints, mats = new Set(), base = new Map();
  fig.traverse(m => { if (m.isMesh) mats.add(m.material); });
  for (const m of mats) if (m !== fig.userData.visor) base.set(m, m.color.clone());
  const shadow = makeFigureShadow(o.moon), dust = makeDust(o.moon);
  const extras = new THREE.Group(); extras.add(shadow.mesh, dust.points); extras.visible = false;

  const cur = { ...STAND }, tgt = { ...STAND };
  const S = { phase: 'cabin', t: 0, rate: 1, pos: new THREE.Vector3(), yaw: 0, zc: 0, a: 2, vy: 0, from: new THREE.Vector3(), to: new THREE.Vector3(),
    mode: 'act', act: 'look', actT: 0, actD: 6, spot: 0, cmd: null, arrived: false, anchor: null, boarding: false, home: false, route: [], target: new THREE.Vector3(), speed: 0, u: 0, lead: 1, waved: 0, lit: 1, bakeT: 1, cyc: 0, falls: 0 };
  const set = (p) => { for (const k of KEYS) tgt[k] = p[k] ?? STAND[k]; };
  const M = new THREE.Matrix4(), _v = new THREE.Vector3(), _w = new THREE.Vector3(), V = (x, y, z) => new THREE.Vector3(x, y, z);
  const toWorld = v => v.applyMatrix4(M);
  const ladder = () => o.model.userData.ladder;
  const ladderAt = (a, out) => { const L = ladder(), i = clamp(Math.floor(a), 0, L.length - 2), k = clamp(a - i, 0, 1); return out.lerpVectors(L[i], L[i + 1], k).add(_w.set(0, 0, 0.13)); };
  /* (his spots lie in front of the LM; or round the rover, once it has carried him off: S.anchor = [x, z, yaw]) */
  const spotWorld = (i, out) => {
    if (S.anchor) { const c = Math.cos(S.anchor[2]), sn = Math.sin(S.anchor[2]), x = SPOTS[i][0] * 0.9 + 2.5 * Math.sign(SPOTS[i][0]), z = (SPOTS[i][1] - 9) * 0.8; out.set(S.anchor[0] + x * c + z * sn, 0, S.anchor[1] - x * sn + z * c); }
    else toWorld(out.set(SPOTS[i][0], 0, SPOTS[i][1]));
    out.y = o.moon.heightAt(out.x, out.z); return out;
  };
  const yawTo = (x, z) => Math.atan2(-(x - S.pos.x), -(z - S.pos.z));          /* the figure faces -z at yaw 0 */

  function apply(dt, snap) {
    const k = snap ? 1 : Math.min(1, dt * 9);
    for (const key of KEYS) cur[key] += (tgt[key] - cur[key]) * k;
    J.waist.rotation.set(cur.wx, cur.wy, 0);
    J.armL.rotation.set(cur.alx, 0, -cur.alz); J.armR.rotation.set(cur.arx, 0, cur.arz);
    J.elbowL.rotation.x = cur.el; J.elbowR.rotation.x = cur.er;
    J.legL.rotation.set(cur.ll, 0, -0.03); J.legR.rotation.set(cur.lr, 0, 0.03);
    J.kneeL.rotation.x = cur.kl; J.kneeR.rotation.x = cur.kr;
    fig.position.set(S.pos.x, S.pos.y + cur.dy, S.pos.z); fig.rotation.set(cur.lean, S.yaw, 0);
  }
  /* a boot comes down: a print, and dust thrown out ahead and to the sides */
  function footfall(side, strength = 1) {
    const fx = -Math.sin(S.yaw), fz = -Math.cos(S.yaw), gx = S.pos.x - fz * side * 0.13, gz = S.pos.z + fx * side * 0.13;
    if (o.prints) o.prints.add(gx, gz, fx, fz, side);
    dust.emit(gx, o.moon.heightAt(gx, gz), gz, fx, fz, Math.round(9 * strength), 0.8 + 0.5 * strength);
    S.falls++;
  }
  function nextSpot(avoid) {
    let i = S.spot; for (let n = 0; n < 8 && (i === S.spot || i === avoid); n++) i = Math.floor(Math.random() * SPOTS.length);
    S.spot = i; spotWorld(i, S.target); S.mode = 'go';
  }
  /* the next stretch of his way back to the ladder */
  const _m = new THREE.Matrix4();
  function leg() {
    const [x, z] = S.route.shift(); toWorld(_v.set(x, 0, z));
    const c = { x: _v.x, z: _v.z, act: 'look', hold: 1e9 };
    if (!S.route.length) { toWorld(_w.set(0, 0, 0)); c.face = [_w.x, _w.z]; }
    api.command(c);
  }
  function begin(phase) {
    o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
    S.phase = phase; S.t = 0; S.speed = 0; S.u = 0; S.bakeT = 1;
  }

  const api = {
    fig, extras, mats, S,
    get on() { return S.phase !== 'cabin'; },
    /* in the cabin still, in his suit by the hatch (the cabin's own figure) */
    reset() { begin('cabin'); S.home = false; S.boarding = false; fig.visible = false; if (o.cabinLmp) o.cabinLmp.visible = true; },
    /* already down and about (chapter 6 started on the ground) */
    startOutside() {
      begin('roam'); if (o.cabinLmp) o.cabinLmp.visible = false;
      S.spot = 0; spotWorld(0, S.pos); S.yaw = o.model.rotation.y + Math.PI; S.mode = 'act'; S.act = 'look'; S.actT = 0; S.actD = 5;
      set(STAND); apply(0, true);
    },
    /* his motion for the film: 18 numbers (where he is, his heading, the pose), kept and set again */
    snapshot(a, off) { a[off] = S.pos.x; a[off + 1] = S.pos.y; a[off + 2] = S.pos.z; a[off + 3] = S.yaw; KEYS.forEach((k, i) => { a[off + 4 + i] = cur[k]; }); },
    pose(a, off) {
      S.pos.set(a[off], a[off + 1], a[off + 2]); S.yaw = a[off + 3]; KEYS.forEach((k, i) => { cur[k] = tgt[k] = a[off + 4 + i]; }); apply(0, true);
      for (const [mt, c] of base) mt.color.copy(c);
      if (++S.cyc % 5 === 0 && o.renderer) shadow.bake(o.renderer, fig, o.sun);
      shadow.drape(fig.position.x, fig.position.y, fig.position.z);
    },
    /* a stand-in for the film when no motion was kept: standing at (x, z), an arm up and waving */
    canned(x, z, yaw, t) {
      const a = new Float32Array(18); a[0] = x; a[1] = o.moon.heightAt(x, z); a[2] = z; a[3] = yaw;
      const p = { ...STAND, arx: 2.5, arz: 0.35 + 0.3 * Math.sin(t * 5), er: 0.35 }; KEYS.forEach((k, i) => { a[4 + i] = p[k]; }); api.pose(a, 0);
    },
    /* into the cabin after you (the cabin's own figure takes over) */
    board() { S.phase = 'cabin'; S.t = -1e9; S.cmd = null; S.home = true; S.boarding = false; fig.visible = false; },
    /* back to the LM and up: round to the front if he is behind it, to the footpad, a jump for the lowest rung, up
     * the ladder, over the porch on his knees and in through the hatch */
    climb() {
      if (S.home || S.boarding) return;
      if (S.phase !== 'roam') { api.board(); return; }
      o.model.updateMatrixWorld(true); M.copy(o.model.matrixWorld);
      const p = _v.copy(S.pos).applyMatrix4(_m.copy(M).invert());
      S.boarding = true; S.anchor = null; S.route = [];
      if (p.z < 3.5) S.route.push([(p.x >= 0 ? 1 : -1) * 7.5, 6]);
      S.route.push([0, LM.FOOT_R + 1.5]);
      leg();
    },
    get boarding() { return S.boarding; },
    get aboard() { return S.home; },
    /* told what to do (surface-ops.js): go to (x, z), then act ('hold': stand holding something out at his right;
     * 'salute': facing face[x, z]) for hold seconds; null: as he pleases again. arrived: he is there */
    command(c) { S.cmd = c; S.arrived = false; if (c && S.phase === 'roam') { S.target.set(c.x, 0, c.z); S.mode = 'go'; } else if (!c && S.phase === 'roam' && S.mode === 'act') { S.actT = 1e9; } },
    get arrived() { return S.arrived; },
    /* on the rover's right seat, and off it again (beside it, then about it) */
    ride(on, x, z, ax, az, ayaw) {
      if (on) { if (S.phase !== 'roam') return; S.phase = 'ride'; S.cmd = null; S.speed = 0; S.u = 0; }
      else if (S.phase === 'ride') { S.phase = 'roam'; S.pos.set(x, o.moon.heightAt(x, z), z); S.mode = 'act'; S.act = 'look'; S.actT = 0; S.actD = 5; S.anchor = [ax, az, ayaw]; }
    },
    seat(hips, yaw) { S.pos.set(hips.x, hips.y - 0.97, hips.z); S.yaw = yaw; },
    /* he answers you on the radio: if he is standing, he turns to you */
    hail() { if (S.phase === 'roam' && S.mode === 'act') { S.act = 'you'; S.actT = 0.9; S.actD = Math.max(S.actD, 5); } },
    update(dt, ctx = {}) {
      const real = dt; dt *= S.rate; S.t += dt;
      const yawLM = o.model.rotation.y, pl = ctx.player;
      if (S.phase === 'cabin') {
        if (S.home) return;                                       /* in again, to stay */
        /* he follows a little after you are down */
        if (!ctx.walking) S.t = 0;
        else if (S.t > 7) {
          begin('crawl'); S.zc = 0.8; S.yaw = yawLM; if (o.cabinLmp) o.cabinLmp.visible = false;
          if (o.toast) o.toast.say('The Lunar Module Pilot is coming out', 5);
          if (o.voice) o.voice('lmpout');
          set({ wx: -1.45, alx: 1.45, arx: 1.45, alz: 0.1, arz: 0.1, el: 0.3, er: 0.3, ll: -0.55, lr: -0.55, kl: -0.95, kr: -0.95 }); S.cyc = 0;
          toWorld(S.pos.set(0, 2.45 + 0.5 - 0.94, S.zc)); apply(0, true);
        }
        return;
      }
      if (S.phase === 'crawl') {
        /* on hands and knees, backwards, facing the cabin: over the sill and along the porch */
        S.zc += 0.2 * dt; S.cyc += dt * 3.2;
        const fy = S.zc < LM.FRONT ? 2.45 : LM.PORCH.y + 0.02, sw = Math.sin(S.cyc);
        set({ wx: -1.45, alx: 1.45 + 0.22 * sw, arx: 1.45 - 0.22 * sw, alz: 0.1, arz: 0.1, el: 0.3, er: 0.3, ll: -0.55 - 0.14 * sw, lr: -0.55 + 0.14 * sw, kl: -0.95 + 0.14 * sw, kr: -0.95 - 0.14 * sw });
        _v.set(0, fy + 0.5 - 0.94, S.zc); toWorld(_v); S.pos.lerp(_v, Math.min(1, dt * 6));
        if (S.zc >= LM.PORCH.z1 - 0.3) { S.phase = 'mount'; S.t = 0; S.from.copy(S.pos); toWorld(ladderAt(2, S.to)); }
      } else if (S.phase === 'mount') {
        /* the legs go over the porch's edge and find the rungs; he comes upright against the ladder */
        const k = smooth(S.t / 3);
        S.pos.lerpVectors(S.from, S.to, k);
        set({ lean: -0.38 * k, wx: -1.45 + 1.4 * k, alx: 1.45 - 0.3 * k, arx: 1.45 - 0.3 * k, alz: 0.12, arz: 0.12, el: 0.3 + 0.2 * k, er: 0.3 + 0.2 * k, ll: -0.55 + 1.0 * k, lr: -0.55 + 0.8 * k, kl: -0.95 + 0.4 * k, kr: -0.95 + 0.2 * k });
        if (S.t >= 3) { S.phase = 'ladder'; S.a = 2; }
      } else if (S.phase === 'ladder') {
        /* down the rungs, a foot and a hand at a time */
        S.a += 0.85 * dt; const sw = Math.sin(Math.PI * S.a), L = ladder();
        toWorld(ladderAt(S.a, S.pos));
        set({ lean: -0.38, wx: -0.05, alx: 1.15 + 0.2 * sw, arx: 1.15 - 0.2 * sw, alz: 0.12, arz: 0.12, el: 0.5, er: 0.5, ll: 0.45 + 0.35 * sw, lr: 0.45 - 0.35 * sw, kl: -0.55 - 0.35 * sw, kr: -0.55 + 0.35 * sw });
        if (S.a >= L.length - 1) { S.phase = 'drop'; S.vy = 0; S.from.copy(S.pos); toWorld(S.to.set(0, LM.FOOT_Y + 0.02, LM.FOOT_R - 0.05)); S.t = 0; }
      } else if (S.phase === 'drop') {
        /* the last rung: he lets go and falls to the footpad, slowly, as things fall here */
        S.vy -= G * dt; S.pos.y += S.vy * dt;
        const k = clamp((S.from.y - S.pos.y) / Math.max(0.05, S.from.y - S.to.y), 0, 1);
        S.pos.x = S.from.x + (S.to.x - S.from.x) * k; S.pos.z = S.from.z + (S.to.z - S.from.z) * k;
        set({ lean: -0.38 * (1 - k), wx: -0.1, alx: 0.9, arx: 0.9, alz: 0.3, arz: 0.3, el: 0.5, er: 0.5, ll: 0.3, lr: 0.3, kl: -0.5, kr: -0.5 });
        if (S.pos.y <= S.to.y) { S.pos.copy(S.to); S.phase = 'pad'; S.t = 0; }
      } else if (S.phase === 'pad') {
        /* the knees take it; then one hop back onto the Moon, clear of you */
        const c = Math.sin(Math.PI * clamp(S.t / 0.7, 0, 1));
        set({ wx: -0.08 - 0.25 * c, ll: 0.1 + 0.45 * c, lr: 0.1 + 0.45 * c, kl: -0.16 - 0.8 * c, kr: -0.16 - 0.8 * c, dy: -0.12 * c });
        if (S.t > 1.6) {
          /* (straight back, or to one side of you if you stand there) */
          for (const [x, z] of [[0, 1.5], [-1.4, 0.9], [1.4, 0.9]]) {
            toWorld(S.to.set(x, 0, LM.FOOT_R + z)); S.to.y = o.moon.heightAt(S.to.x, S.to.z);
            if (!pl || Math.hypot(pl.x - S.to.x, pl.z - S.to.z) > 1.4) { S.phase = 'off'; S.t = 0; S.from.copy(S.pos); break; }
          }
        }
      } else if (S.phase === 'off') {
        const T = 1.0, k = clamp(S.t / T, 0, 1);
        S.pos.lerpVectors(S.from, S.to, k); S.pos.y += 0.3 * 4 * k * (1 - k);
        set({ wx: -0.15, alx: 0.6, arx: 0.6, alz: 0.5, arz: 0.5, ll: -0.1, lr: 0.25, kl: -0.5, kr: -0.4 });
        if (k >= 1) { S.pos.copy(S.to); footfall(1, 1.3); footfall(-1, 1.3); S.phase = 'roam'; S.mode = 'act'; S.act = 'look'; S.actT = 0; S.actD = 4; S.spot = -1; if (o.voice) o.voice('lmpdown'); }
      } else if (S.phase === 'ride') {
        /* seated: the knees up, the hands on his lap, looking about a little */
        set({ wx: -0.02, wy: 0.25 * Math.sin(S.t * 0.37), alx: 0.75, arx: 0.75, alz: 0.12, arz: 0.12, el: 1.0, er: 1.0, ll: 1.45, lr: 1.45, kl: -1.35, kr: -1.35 });
      } else if (S.phase === 'roam') {
        roam(dt, pl);
        /* on his way back in: stretch by stretch, then square to the ladder and onto the footpad */
        if (S.boarding && S.arrived) {
          if (S.route.length) leg();
          else if (S.u < 0.05 && (Math.abs(wrap(yawLM - S.yaw)) < 0.2 || S.actT > 4)) { begin('hopon'); S.yaw = yawLM; S.from.copy(S.pos); toWorld(S.to.set(0, LM.FOOT_Y + 0.02, LM.FOOT_R - 0.05)); }
        }
      } else if (S.phase === 'hopon') {
        /* one hop up onto the footpad */
        const k = clamp(S.t / 1.0, 0, 1);
        S.pos.lerpVectors(S.from, S.to, k); S.pos.y += 0.3 * 4 * k * (1 - k);
        set({ wx: -0.15, alx: 0.6, arx: 0.6, alz: 0.5, arz: 0.5, ll: 0.25, lr: -0.1, kl: -0.4, kr: -0.5 });
        if (k >= 1) { S.pos.copy(S.to); footfall(1, 1.0); S.phase = 'jumpup'; S.t = 0; S.from.copy(S.pos); toWorld(ladderAt(ladder().length - 1, S.to)); }
      } else if (S.phase === 'jumpup') {
        /* the knees bend; then the jump for the lowest rung, the hands up for the rails (it hangs well clear of the pad) */
        const c = Math.sin(Math.PI * clamp(S.t / 0.9, 0, 1)), k = smooth(clamp((S.t - 0.9) / 1.1, 0, 1));
        S.pos.lerpVectors(S.from, S.to, k);
        if (S.t < 0.9) set({ wx: -0.08 - 0.25 * c, alx: 1.2, arx: 1.2, alz: 0.15, arz: 0.15, ll: 0.1 + 0.45 * c, lr: 0.1 + 0.45 * c, kl: -0.16 - 0.8 * c, kr: -0.16 - 0.8 * c, dy: -0.12 * c });
        else set({ lean: -0.38 * k, wx: -0.1, alx: 1.6, arx: 1.6, alz: 0.12, arz: 0.12, el: 0.4, er: 0.4, ll: 0.3, lr: 0.3, kl: -0.5, kr: -0.5 });
        if (S.t >= 2.0) { S.phase = 'up'; S.a = ladder().length - 1; }
      } else if (S.phase === 'up') {
        /* up the rungs, a hand and a foot at a time */
        S.a -= 0.85 * dt; const sw = Math.sin(Math.PI * S.a);
        toWorld(ladderAt(Math.max(2, S.a), S.pos));
        set({ lean: -0.38, wx: -0.05, alx: 1.15 + 0.2 * sw, arx: 1.15 - 0.2 * sw, alz: 0.12, arz: 0.12, el: 0.5, er: 0.5, ll: 0.45 + 0.35 * sw, lr: 0.45 - 0.35 * sw, kl: -0.55 - 0.35 * sw, kr: -0.55 + 0.35 * sw });
        if (S.a <= 2) { S.phase = 'unmount'; S.t = 0; S.from.copy(S.pos); toWorld(S.to.set(0, LM.PORCH.y + 0.02 + 0.5 - 0.94, LM.PORCH.z1 - 0.3)); }
      } else if (S.phase === 'unmount') {
        /* off the ladder's top onto the porch: down on his hands and knees, the legs drawn up after him */
        const k = 1 - smooth(S.t / 3);
        S.pos.lerpVectors(S.to, S.from, k);
        set({ lean: -0.38 * k, wx: -1.45 + 1.4 * k, alx: 1.45 - 0.3 * k, arx: 1.45 - 0.3 * k, alz: 0.12, arz: 0.12, el: 0.3 + 0.2 * k, er: 0.3 + 0.2 * k, ll: -0.55 + 1.0 * k, lr: -0.55 + 0.8 * k, kl: -0.95 + 0.4 * k, kr: -0.95 + 0.2 * k });
        if (S.t >= 3) { S.phase = 'crawlin'; S.zc = LM.PORCH.z1 - 0.3; S.cyc = 0; }
      } else if (S.phase === 'crawlin') {
        /* along the porch and in over the sill, head first */
        S.zc -= 0.3 * dt; S.cyc += dt * 4.2;
        const fy = S.zc < LM.FRONT ? 2.45 : LM.PORCH.y + 0.02, sw = Math.sin(S.cyc);
        set({ wx: -1.45, alx: 1.45 + 0.22 * sw, arx: 1.45 - 0.22 * sw, alz: 0.1, arz: 0.1, el: 0.3, er: 0.3, ll: -0.55 - 0.14 * sw, lr: -0.55 + 0.14 * sw, kl: -0.95 + 0.14 * sw, kr: -0.95 - 0.14 * sw });
        _v.set(0, fy + 0.5 - 0.94, S.zc); toWorld(_v); S.pos.lerp(_v, Math.min(1, dt * 6));
        if (S.zc <= 0.8) {
          api.board(); if (o.cabinLmp) o.cabinLmp.visible = true;
          if (o.voice) o.voice('lmpin');
          return;
        }
      }
      apply(dt, false);
      /* in the LM's shadow only the ground's glow lights him */
      const want = o.shade ? (o.shade(_v.copy(fig.position).setY(fig.position.y + 0.4)) + o.shade(_v.setY(fig.position.y + 1.1)) + o.shade(_v.setY(fig.position.y + 1.8))) / 3 : 1;
      S.lit += (want - S.lit) * Math.min(1, dt * 6);
      const f = 0.36 + 0.64 * S.lit; for (const [m, c] of base) m.color.copy(c).multiplyScalar(f);
      fig.visible = true;
      /* his shadow: drawn again a few times a second (he turns, his limbs move), laid out every frame */
      S.bakeT += real;
      if (S.bakeT > 0.14 && o.renderer) { S.bakeT = 0; shadow.bake(o.renderer, fig, o.sun); }
      shadow.drape(fig.position.x, fig.position.y, fig.position.z);
      dust.update(dt);
    }
  };

  /* on the ground: go to a spot in the lope, do something there, go on */
  function roam(dt, pl) {
    const near = pl ? Math.hypot(pl.x - S.pos.x, pl.z - S.pos.z) : 1e3;
    let turning = false, wantSpeed = 0;
    if (S.mode === 'go') {
      const dx = S.target.x - S.pos.x, dz = S.target.z - S.pos.z, d = Math.hypot(dx, dz), dy = wrap(yawTo(S.target.x, S.target.z) - S.yaw);
      S.yaw += clamp(dy, -2.0 * dt, 2.0 * dt); turning = Math.abs(dy) > 0.45;
      wantSpeed = turning ? 0 : Math.min(1.3, 0.35 + d * 0.8);
      /* you are in the way: he stops and turns to you */
      if (pl && near < 1.6 && !S.cmd) { const ahead = -(pl.x - S.pos.x) * Math.sin(S.yaw) - (pl.z - S.pos.z) * Math.cos(S.yaw); if (ahead > 0) { S.mode = 'act'; S.act = 'you'; S.actT = 0; S.actD = 4; wantSpeed = 0; } }
      if (d < 0.35 && S.mode === 'go') { S.mode = 'act'; S.actT = 0; if (S.cmd) { S.act = S.cmd.act; S.actD = S.cmd.hold; S.arrived = true; } else { S.act = SPOTS[Math.max(0, S.spot)][2]; S.actD = 6 + Math.random() * 8; } }
    } else {
      S.actT += dt;
      if (S.act === 'you' && pl) { const dy = wrap(yawTo(pl.x, pl.z) - S.yaw); S.yaw += clamp(dy, -1.6 * dt, 1.6 * dt); turning = Math.abs(dy) > 0.35; }
      if (S.cmd && S.cmd.face) { const dy = wrap(yawTo(S.cmd.face[0], S.cmd.face[1]) - S.yaw); S.yaw += clamp(dy, -1.6 * dt, 1.6 * dt); turning = Math.abs(dy) > 0.35; }
      if (S.actT > S.actD && S.u < 0.05) { S.cmd = null; S.arrived = false; nextSpot(-1); }
    }
    S.speed += (wantSpeed - S.speed) * Math.min(1, dt * 2.5);
    const moving = S.speed > 0.08 || turning || S.u > 0.001;
    /* the hop's cycle: a short push from the ground, a long flight (period 0.85 s) */
    const C = 0.28; let h = 0, air = 0, push = 0;
    if (moving) {
      S.u += dt / 0.85;
      if (S.u >= 1) { S.u = (S.speed > 0.08 || turning) ? S.u - 1 : 0; footfall(S.lead, 0.5 + S.speed * 0.6); S.lead = -S.lead; }
      if (S.u < C) push = Math.sin(Math.PI * S.u / C);
      else { const q = (S.u - C) / (1 - C); air = 4 * q * (1 - q); h = (0.05 + 0.13 * S.speed / 1.3) * air; }
    }
    const fx = -Math.sin(S.yaw), fz = -Math.cos(S.yaw);
    S.pos.x += fx * S.speed * dt; S.pos.z += fz * S.speed * dt;
    S.pos.y = o.moon.heightAt(S.pos.x, S.pos.z) + h;
    const g = S.speed / 1.3;
    if (moving) {
      const a = S.lead > 0 ? 1 : 0, b = 1 - a;                 /* which leg leads this hop (right when lead > 0) */
      const lead = 0.1 + 0.5 * g * air, trail = 0.1 - 0.35 * g * air, kLead = -0.2 - 0.45 * air - 0.5 * push, kTrail = -0.2 - 0.3 * air - 0.5 * push;
      set({ lean: -0.2 * g, wx: -0.1 - 0.08 * g, alx: 0.4 + 0.25 * g * (a ? -air : air), arx: 0.4 + 0.25 * g * (a ? air : -air), alz: 0.38 + 0.15 * air, arz: 0.38 + 0.15 * air, el: 0.75, er: 0.75,
        lr: a ? lead : trail, ll: b ? lead : trail, kr: a ? kLead : kTrail, kl: b ? kLead : kTrail, dy: -0.06 * push });
    } else if (S.act === 'hold') {
      /* something stood on the ground at his right, his hand on it */
      set({ arx: 1.05, arz: 0.2, er: 0.45, wy: 0.15 * Math.sin(S.actT * 0.4) });
    } else if (S.act === 'wave') {
      set({ arx: 2.5, arz: 0.35 + 0.3 * Math.sin(S.actT * 5), er: 0.35, wx: -0.02 });
    } else if (S.act === 'salute') {
      set({ arx: 1.75, arz: 0.75, er: 2.1, wx: -0.02 });
    } else if (S.act === 'work') {
      /* bent to something at his feet, the hands busy */
      const s1 = Math.sin(S.actT * 1.7), s2 = Math.sin(S.actT * 1.3 + 1);
      set({ wx: -0.55, alx: 0.95 + 0.14 * s1, arx: 0.95 + 0.14 * s2, alz: 0.1, arz: 0.1, el: 0.85 + 0.2 * s2, er: 0.85 + 0.2 * s1, ll: 0.3, lr: 0.3, kl: -0.5, kr: -0.5, dy: -0.05 });
    } else if (S.act === 'you' && near < 14 && S.actT > 0.8 && S.actT < 4.2) {
      /* an arm up to you */
      set({ arx: 2.5, arz: 0.35 + 0.3 * Math.sin(S.actT * 5), er: 0.35, wx: -0.02 });
    } else {
      /* standing: the stiff torso turns as he looks about; the suit never quite stands still */
      set({ wy: 0.4 * Math.sin(S.actT * 0.45) + 0.1 * Math.sin(S.actT * 1.1), wx: -0.08 + 0.02 * Math.sin(S.actT * 0.9), alx: 0.35 + 0.04 * Math.sin(S.actT * 0.7), arx: 0.35 + 0.04 * Math.sin(S.actT * 0.6 + 1) });
    }
  }
  return api;
}
