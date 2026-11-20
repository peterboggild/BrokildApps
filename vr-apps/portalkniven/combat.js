/*
 * Portalkniven — combat (a test): the fractal dimensions are infested with alien bugs.
 *
 * The bugs live in the fractal's own space, not in the room: their places, sizes and speeds are in fractal units. That is the whole trick:
 *   - scale the fractal up or down and the bugs scale with it, so making the fractal tiny does not crush them (nothing changes in fractal space);
 *   - change the fractal's shape (Y / X, B / A) and the walls move through fractal space: a wall that reaches a bug crushes it;
 *   - fly the ship and the fractal slides past it, bugs and all.
 * They keep to the free space: they are only ever put where the fractal's distance function says there is room, and as they crawl towards the ship they
 * slide along walls instead of entering them (the distance function's gradient is the wall's normal). They are slow. When one reaches the ship it bursts
 * and the ship takes damage.
 *
 * Seeing them: the fractal is a picture marched on the graphics card, the bugs are ordinary 3D objects drawn over it, so a bug behind a wall would show
 * through. Each bug therefore has its line of sight checked now and then (a few bugs a frame), by marching from your head to the bug through the fractal
 * on the CPU; one that is hidden, or lost in the fractal's fog, is not drawn.
 *
 * Draw calls: three (carapaces, glowing sacs, halos), whatever the number of bugs.
 *
 * Shooting: the ship's two cannons fire plasma bolts. A bolt lives in the fractal's space too (so it flies on with the fractal when the ship turns away),
 * leaves the muzzle at the ship's own speed plus 9 m/s, and is stepped in short pieces: the first bug within reach of a piece bursts, the first wall stops
 * it. The bugs are small, so a shot fired within about 7 degrees of a bug in plain view is aimed at it (a gentle aim assist). Two more draw calls, only while bolts are in the air.
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';

const V = THREE.Vector3;
const rnd = (a, b) => a + Math.random() * (b - a);
const MAX = 40, BOLTS = 24, BOLT_V = 9, BOLT_LIFE = 1.4, ASSIST = Math.cos(7 * Math.PI / 180);

function bugGeometry() {
  /* about one unit in radius, nose along -Z: carapace, head, six legs, two feelers; the sac at the back is separate (it glows) */
  const parts = [], up = new V(0, 1, 0), m = new THREE.Matrix4(), q = new THREE.Quaternion();
  const ell = (x, y, z, sx, sy, sz) => { const g = new THREE.SphereGeometry(1, 14, 10); g.scale(sx, sy, sz); g.translate(x, y, z); parts.push(g); };
  const rod = (a, b, r0, r1) => { const A = new V(...a), B = new V(...b), d = new V().subVectors(B, A), g = new THREE.CylinderGeometry(r1, r0, d.length(), 6);
    g.applyMatrix4(m.compose(new V().addVectors(A, B).multiplyScalar(0.5), q.setFromUnitVectors(up, d.clone().normalize()), new V(1, 1, 1))); parts.push(g); };
  ell(0, 0.05, -0.1, 0.5, 0.36, 0.72); ell(0, 0.0, -0.85, 0.3, 0.26, 0.3);                  // thorax, head
  for (const s of [-1, 1]) {
    for (const z of [-0.45, -0.05, 0.35]) { rod([s * 0.38, 0, z], [s * 0.85, 0.28, z - 0.1], 0.07, 0.05); rod([s * 0.85, 0.28, z - 0.1], [s * 1.15, -0.45, z + 0.05], 0.05, 0.02); }
    rod([s * 0.1, 0.15, -1.05], [s * 0.45, 0.6, -1.55], 0.03, 0.01);                   // feelers
    rod([s * 0.12, -0.08, -1.08], [s * 0.05, -0.15, -1.3], 0.06, 0.02);                  // mandibles
  }
  for (const g of parts) g.deleteAttribute('uv');
  const body = mergeGeometries(parts.map(g => g.toNonIndexed()), false); body.computeVertexNormals();
  const sac = new THREE.SphereGeometry(1, 14, 10); sac.scale(0.48, 0.42, 0.62); sac.translate(0, 0.12, 0.72);
  return { body, sac };
}

export function createCombat({ scene, worlds, onBlast, onCrush, onShot, onSpark }) {
  const { body, sac } = bugGeometry();
  const carapace = new THREE.InstancedMesh(body, new THREE.MeshStandardMaterial({ color: 0x241a2e, roughness: 0.35, metalness: 0.4, emissive: 0x0a1a08 }), MAX);
  /* the sac at the back: translucent and lit from inside, brightest where you look straight into it, with dark veins over it, and each one pulsing in its own time */
  const sacMat = new THREE.ShaderMaterial({ uniforms: { uT: { value: 0 } }, toneMapped: false,
    vertexShader: `varying vec3 vN; varying vec3 vV; varying vec3 vP; varying float vI;
      void main() { vec4 wp = modelMatrix * instanceMatrix * vec4(position, 1.0); vN = normalize(mat3(modelMatrix) * mat3(instanceMatrix) * normal); vV = cameraPosition - wp.xyz; vP = position; vI = float(gl_InstanceID); gl_Position = projectionMatrix * viewMatrix * wp; }`,
    fragmentShader: `precision highp float; varying vec3 vN; varying vec3 vV; varying vec3 vP; varying float vI; uniform float uT;
      void main() { float f = max(dot(normalize(vN), normalize(vV)), 0.0), pulse = 0.75 + 0.25 * sin(uT * 5.0 + vI * 1.7);
        float vein = smoothstep(0.82, 0.98, abs(sin(vP.x * 13.0 + 2.0 * sin(vP.z * 9.0 + vI)) * sin(vP.y * 11.0 + vP.z * 6.0)));
        vec3 c = mix(vec3(0.05, 0.22, 0.02), vec3(0.75, 1.0, 0.3), pow(f, 1.6) * pulse) * (1.0 - 0.75 * vein);
        gl_FragColor = vec4(c * (0.6 + 0.8 * pulse), 1.0); }` });
  const glow = new THREE.InstancedMesh(sac, sacMat, MAX);
  const outsideDoor = m => { m.stencilWrite = true; m.stencilRef = 1; m.stencilFunc = THREE.NotEqualStencilFunc; m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp; };      // never over the door's other side
  for (const im of [carapace, glow]) { outsideDoor(im.material); im.frustumCulled = false; im.renderOrder = 46; im.count = 0; scene.add(im); }
  /* halos: one point each, sized in metres */
  const hp = new Float32Array(MAX * 3), hs = new Float32Array(MAX), ha = new Float32Array(MAX);
  const hg = new THREE.BufferGeometry(); hg.setAttribute('position', new THREE.BufferAttribute(hp, 3)); hg.setAttribute('aS', new THREE.BufferAttribute(hs, 1)); hg.setAttribute('aA', new THREE.BufferAttribute(ha, 1));
  const halos = new THREE.Points(hg, new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, uniforms: { uScale: { value: 800 } },
    vertexShader: 'attribute float aS; attribute float aA; varying float vA; uniform float uScale; void main() { vA = aA; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_PointSize = clamp(aS * uScale / -mv.z, 1.0, 160.0); gl_Position = projectionMatrix * mv; }',
    fragmentShader: 'precision highp float; varying float vA; void main() { float d = length(gl_PointCoord - 0.5) * 2.0, a = max(0.0, 1.0 - d); a *= a; gl_FragColor = vec4(vec3(0.5, 1.0, 0.25) * a * vA, 1.0); }' }));
  outsideDoor(halos.material); halos.frustumCulled = false; halos.renderOrder = 47; halos.visible = false; scene.add(halos);

  /* the bolts: a short glowing streak each, one instanced draw call, brightest at the head */
  const boltGeo = new THREE.CylinderGeometry(1, 1, 1, 6, 1, false); boltGeo.rotateX(Math.PI / 2); boltGeo.translate(0, 0, 0.5);      // from z 0 (head) to z 1 (tail)
  const boltMat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, toneMapped: false,
    vertexShader: 'varying float vZ; void main() { vZ = position.z; gl_Position = projectionMatrix * viewMatrix * modelMatrix * instanceMatrix * vec4(position, 1.0); }',
    fragmentShader: 'precision highp float; varying float vZ; void main() { float a = pow(1.0 - vZ, 1.5); gl_FragColor = vec4(mix(vec3(1.0, 0.35, 0.05), vec3(1.0, 0.9, 0.55), a) * (0.35 + 1.6 * a), 1.0); }' });
  /* and a wider, faint shell round each for the glow */
  const boltGlowMat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, toneMapped: false,
    vertexShader: 'varying float vZ; varying vec3 vN; varying vec3 vV; void main() { vZ = position.z; vec4 wp = modelMatrix * instanceMatrix * vec4(position, 1.0); vN = normalize(mat3(modelMatrix) * mat3(instanceMatrix) * normal); vV = cameraPosition - wp.xyz; gl_Position = projectionMatrix * viewMatrix * wp; }',
    fragmentShader: 'precision highp float; varying float vZ; varying vec3 vN; varying vec3 vV; void main() { float f = abs(dot(normalize(vN), normalize(vV))); float a = (1.0 - vZ) * f * f; gl_FragColor = vec4(vec3(1.0, 0.42, 0.08) * 0.55 * a, 1.0); }' });
  outsideDoor(boltMat); outsideDoor(boltGlowMat);
  const boltMesh = new THREE.InstancedMesh(boltGeo, boltMat, BOLTS), boltGlow = new THREE.InstancedMesh(boltGeo, boltGlowMat, BOLTS);
  for (const im of [boltMesh, boltGlow]) { im.frustumCulled = false; im.renderOrder = 47; im.count = 0; im.visible = false; scene.add(im); }
  const bolts = [];

  const bugs = []; let dim = -1, spawnT = 0, check = 0, t = 0;
  const _p = new V(), _q = new V(), _d = new V(), _g = new V(), _a = new V(), _b = new V(), _r = new V(), _m = new THREE.Matrix4(), _o = new THREE.Quaternion(), _s = new V(), Z = new V(0, 0, -1), _sf = new V(), _hf = new V(), _nf = new V();
  const de = (p) => worlds.fractDE(dim, p.x, p.y, p.z);
  function grad(p, d0, e, out) { out.set(worlds.fractDE(dim, p.x + e, p.y, p.z) - d0, worlds.fractDE(dim, p.x, p.y + e, p.z) - d0, worlds.fractDE(dim, p.x, p.y, p.z + e) - d0); const l = out.length(); return l > 1e-12 ? out.multiplyScalar(1 / l) : out.set(0, 1, 0); }

  function spawn(info, shipF) {
    const r = 0.06 * info.S0 * rnd(0.8, 1.35);
    for (let k = 0; k < 10; k++) {
      _d.set(rnd(-1, 1), rnd(-0.6, 0.6), rnd(-1, 1)); if (_d.lengthSq() < 0.05) continue; _d.normalize();
      _p.copy(shipF).addScaledVector(_d, rnd(3, 8) * info.S0 + 1.6 * info.S);              // a few metres off at the start, and never on top of the ship
      if (info.ball && _p.length() > info.ball * 0.9) continue;
      if (de(_p) < 3 * r) continue;
      bugs.push({ f: _p.clone(), v: new V(), r, speed: 0.22 * info.S0 * rnd(0.75, 1.25), ph: rnd(0, 6.28), vis: 0, see: true, room: new V() });
      return true;
    }
    return false;
  }
  /** Is the bug in plain view from the head (fractal space), or behind a wall? */
  function inSight(b, headF) {
    _d.subVectors(b.f, headF); const dist = _d.length(); if (dist < 1e-9) return true; _d.multiplyScalar(1 / dist);
    let s = 0;
    for (let k = 0; k < 26; k++) { if (s >= dist - 2 * b.r) return true; _q.copy(headF).addScaledVector(_d, s); const d = de(_q); if (d < 0.2 * b.r) return false; s += Math.max(d, dist / 50); }
    return true;
  }

  const api = {
    count: () => bugs.length,
    /** Clear away the bugs near a room point (when a new ship arrives). */
    clearNear(p, metres) { if (dim < 0) return; worlds.toFrac(dim, p, _sf); const S = worlds.fracInfo(dim).S; for (let k = bugs.length - 1; k >= 0; k--) if (bugs[k].f.distanceTo(_sf) < metres * S) bugs.splice(k, 1); },
    reset() { bugs.length = 0; bolts.length = 0; },
    /** Fire one bolt from a room point along a room direction; shipV is the ship's own speed (m/s). Returns false when there is nothing to fire into. */
    fire(from, dir, shipV) {
      if (dim < 0) return false;
      _d.copy(dir).normalize();
      /* aim assist: the bug in plain view nearest the line of fire, if it is within a few degrees and not too far */
      let best = ASSIST, tgt = null;
      for (const b of bugs) { if (b.vis < 0.5) continue; _a.subVectors(b.room, from); const l = _a.length(); if (l < 0.05 || l > 7) continue; const c = _a.dot(_d) / l; if (c > best) { best = c; tgt = b; } }
      if (tgt) _d.subVectors(tgt.room, from).normalize();
      if (bolts.length >= BOLTS) bolts.shift();
      const f = worlds.toFrac(dim, from, new V()); worlds.toFrac(dim, _a.copy(from).add(_d), _b);
      bolts.push({ f, v: _b.sub(f).multiplyScalar(BOLT_V + Math.max(0, shipV)), age: 0, prev: f.clone(), room: from.clone(), dir: _d.clone() });
      return true;
    },
    /** st: null (no combat here), or { i: the fractal dimension, ship: room position, nose: room direction, head: room position, alive: can the ship be hit } */
    update(dt, st) {
      t += dt; sacMat.uniforms.uT.value = t;
      if (!st) { carapace.count = glow.count = 0; carapace.visible = glow.visible = halos.visible = boltMesh.visible = boltGlow.visible = false; if (dim >= 0) { bugs.length = 0; bolts.length = 0; dim = -1; } return; }
      if (st.i !== dim) { bugs.length = 0; bolts.length = 0; dim = st.i; spawnT = 0; }
      const info = worlds.fracInfo(dim), S = info.S;
      worlds.toFrac(dim, st.ship, _sf); worlds.toFrac(dim, st.head, _hf);
      /* keep about fourteen of them about */
      spawnT -= dt; const want = 14;
      if (bugs.length < want && spawnT <= 0) { let n = bugs.length === 0 ? want : 1; while (n-- > 0 && bugs.length < MAX) spawn(info, _sf); spawnT = 0.6; }
      const far = 14 * info.S0 + 4 * S, fogK = 3.2 / (info.maxT * info.maxT);
      for (let k = bugs.length - 1; k >= 0; k--) {
        const b = bugs[k];
        if (b.f.distanceTo(_sf) > far) { bugs.splice(k, 1); continue; }
        const d0 = de(b.f);
        if (d0 < 0.5 * b.r) { worlds.fromFrac(dim, b.f, _a); bugs.splice(k, 1); onCrush(_a.clone(), b.r / S); continue; }      // a wall has come to it: crushed
        /* crawl towards the ship, wandering a little, and slide along any wall in the way */
        _d.subVectors(_sf, b.f); const dist = _d.length(); _d.multiplyScalar(1 / Math.max(dist, 1e-9));
        _d.x += 0.35 * Math.sin(t * 0.9 + b.ph); _d.y += 0.25 * Math.sin(t * 0.7 + b.ph * 2); _d.z += 0.35 * Math.cos(t * 0.8 + b.ph); _d.normalize().multiplyScalar(b.speed);
        b.v.lerp(_d, Math.min(1, dt * 1.5));
        _q.copy(b.f).addScaledVector(b.v, dt); let d1 = de(_q);
        if (d1 < 2 * b.r) {                                          // slide along the wall; but never run from one: a wall that moves in on a bug gets it
          grad(b.f, d0, 0.3 * b.r, _g); const vn = b.v.dot(_g); if (vn < 0) b.v.addScaledVector(_g, -vn * 1.1);
          _q.copy(b.f).addScaledVector(b.v, dt); d1 = de(_q);
        }
        if (d1 >= b.r) b.f.copy(_q);
        if (info.ball) { const l = b.f.length(); if (l > info.ball * 0.95) b.f.multiplyScalar(info.ball * 0.95 / l); }
        /* has it reached the ship? (the ship as a short thick rod along its nose, in the room) */
        worlds.fromFrac(dim, b.f, b.room);
        if (st.alive) { _a.copy(st.ship).addScaledVector(st.nose, -0.42); _b.copy(st.nose).multiplyScalar(0.84); const u = Math.max(0, Math.min(1, _r.subVectors(b.room, _a).dot(_b) / 0.7056));
          if (_r.copy(_a).addScaledVector(_b, u).distanceTo(b.room) < 0.13 + b.r / S) { bugs.splice(k, 1); onBlast(b.room.clone(), b.r / S); continue; } }
      }
      /* the bolts: stepped in short pieces; the first bug in reach bursts, the first wall stops the bolt */
      for (let k = bolts.length - 1; k >= 0; k--) {
        const o = bolts[k]; o.age += dt; o.prev.copy(o.f);
        const span = o.v.length() * dt, n = Math.max(1, Math.ceil(span / (0.05 * S))), reach = 0.035 * S; let gone = false;
        for (let j = 0; j < n && !gone; j++) {
          o.f.addScaledVector(o.v, dt / n);
          for (let m = bugs.length - 1; m >= 0; m--) { const b = bugs[m]; if (b.f.distanceTo(o.f) < b.r + reach) { worlds.fromFrac(dim, b.f, _a); bugs.splice(m, 1); onShot(_a.clone(), b.r / S); gone = true; break; } }
          if (!gone && de(o.f) < 0.01 * S) { worlds.fromFrac(dim, o.f, _a); onSpark(_a.clone()); gone = true; }
        }
        if (gone || o.age > BOLT_LIFE) { bolts.splice(k, 1); continue; }
        worlds.fromFrac(dim, o.f, o.room); worlds.fracDirToRoom(dim, o.v, o.dir); o.dir.normalize();
      }
      {
        let c = 0;
        for (const o of bolts) { const len = Math.min(0.24, 0.03 + o.age * 2); _o.setFromUnitVectors(_g.set(0, 0, 1), _r.copy(o.dir).negate());
          _s.set(0.011, 0.011, len); boltMesh.setMatrixAt(c, _m.compose(o.room, _o, _s)); _s.set(0.032, 0.032, len * 1.15); boltGlow.setMatrixAt(c, _m.compose(o.room, _o, _s)); c++; }
        boltMesh.count = boltGlow.count = c; boltMesh.visible = boltGlow.visible = c > 0; if (c) boltMesh.instanceMatrix.needsUpdate = boltGlow.instanceMatrix.needsUpdate = true;
      }
      /* line of sight, a few bugs a frame */
      for (let n = 0; n < 4 && bugs.length; n++) { check = (check + 1) % bugs.length; const b = bugs[check]; b.see = inSight(b, _hf); }
      /* draw */
      let c = 0;
      for (const b of bugs) {
        const tF = b.f.distanceTo(_hf), fog = 1 - Math.exp(-tF * tF * fogK);
        b.vis += ((b.see && fog < 0.85 ? 1 : 0) - b.vis) * Math.min(1, dt * 8);
        const rR = b.r / S, sc = rR * b.vis; if (sc < 1e-4) continue;
        worlds.fracDirToRoom(dim, b.v, _g); if (_g.lengthSq() < 1e-12) _g.set(0, 0, -1); _g.normalize();
        _o.setFromUnitVectors(Z, _g); _s.setScalar(sc * (1 + 0.04 * Math.sin(t * 9 + b.ph)));
        carapace.setMatrixAt(c, _m.compose(b.room, _o, _s));
        const pulse = 1 + 0.18 * Math.sin(t * 5 + b.ph); _s.setScalar(sc * pulse); glow.setMatrixAt(c, _m.compose(b.room, _o, _s));
        hp[c * 3] = b.room.x; hp[c * 3 + 1] = b.room.y; hp[c * 3 + 2] = b.room.z; hs[c] = rR * 6; ha[c] = 0.7 * b.vis * (1 - fog) * pulse;
        c++;
      }
      carapace.count = glow.count = c; carapace.visible = glow.visible = c > 0; carapace.instanceMatrix.needsUpdate = glow.instanceMatrix.needsUpdate = true;
      hg.setDrawRange(0, c); hg.attributes.position.needsUpdate = hg.attributes.aS.needsUpdate = hg.attributes.aA.needsUpdate = true; halos.visible = c > 0;
    },
    bugs, bolts,
  };
  return api;
}
