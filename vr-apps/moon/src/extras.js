/* Things around the terrain: the Sun, the Earth (real imagery, also chapter 3's), rocks, a stand-in LM, a handrail.
 * The LM here is a placeholder (stage 5 replaces it). */
import * as THREE from 'three';
import { LUNAR_GLSL } from './moon.js?v=4';
import { makeStars, ECLIPTIC_POLE } from './stars.js?v=28';

const SKY_D = 2.0e6;                 /* sky bodies are drawn at this distance, inside the far plane */

export function makeSky(moon) {
  const g = new THREE.Group();
  const man = moon.manifest, sun = moon.shared.uSun;

  /* The Sun: a disc of the true angular size (0.53 deg) with a soft glare, always facing the eye. */
  const sunMat = new THREE.ShaderMaterial({
    uniforms: { uSun: sun, uSize: { value: SKY_D * Math.tan(14.0 * Math.PI / 180) } },
    vertexShader: /* glsl */`
      uniform vec3 uSun; uniform float uSize; varying vec2 vQ;
      void main() {
        vQ = position.xy;
        vec4 c = viewMatrix * vec4(cameraPosition + uSun * ${SKY_D.toFixed(1)}, 1.0);
        c.xy += position.xy * uSize;
        gl_Position = projectionMatrix * c;
      }`,
    fragmentShader: /* glsl */`
      varying vec2 vQ;
      void main() {
        float r = length(vQ) * 14.0;                      /* degrees from the centre */
        float disc = smoothstep(0.285, 0.255, r);
        /* the glare of an unfiltered Sun in the eye or the visor: far wider than the disc itself */
        float glare = 1.2 * exp(-r * 2.2) + 0.45 * exp(-r * 0.75) + 0.10 * exp(-r * 0.28);
        float a = clamp(disc + glare, 0.0, 1.0) * smoothstep(14.0, 9.0, r);
        gl_FragColor = vec4(vec3(1.0, 0.98, 0.94) * a, a);
      }`,
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending
  });
  const sunMesh = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), sunMat);
  sunMesh.frustumCulled = false; sunMesh.renderOrder = 50;
  g.add(sunMesh);

  /* The Earth: over the Moon's 0N 0E point, true angular size (1.9 deg). Blue Marble (December) and the real
   * clouds of one December day (assets/earth/, as chapters 1 and 2); its axis along the Moon's, its turning set
   * from outside (g.userData.setSpin, radians). */
  const lat = man.site.lat * Math.PI / 180, lon = man.site.lon * Math.PI / 180;
  const eDir = new THREE.Vector3(-Math.sin(lon), Math.cos(lat) * Math.cos(lon), Math.sin(lat) * Math.cos(lon)).normalize();
  const loader = new THREE.TextureLoader(), ebase = new URL('../assets/earth/', import.meta.url).href;
  const etex = (n, srgb) => { const t = loader.load(ebase + n); t.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace; t.wrapS = THREE.RepeatWrapping; t.anisotropy = 4; return t; };
  const earthU = { uSun: sun, uDay: { value: etex('earth.jpg', true) }, uCloud: { value: etex('clouds.jpg', false) },
    uPole: moon.shared.uPole, uRef: { value: eDir.clone().negate() }, uSpin: { value: 0 } };
  const earthMat = new THREE.ShaderMaterial({
    uniforms: earthU,
    vertexShader: /* glsl */`
      varying vec3 vN; varying vec3 vL; varying vec3 vP;
      void main() { vN = normalize(mat3(modelMatrix) * normal); vL = position;
        vec4 p = modelMatrix * vec4(position, 1.0); vP = p.xyz; p.xyz += cameraPosition;
        gl_Position = projectionMatrix * viewMatrix * p; }`,
    fragmentShader: /* glsl */`
      uniform vec3 uSun; uniform sampler2D uDay; uniform sampler2D uCloud; uniform vec3 uPole; uniform vec3 uRef; uniform float uSpin;
      varying vec3 vN; varying vec3 vL; varying vec3 vP;
      void main() {
        vec3 n = normalize(vN), q = normalize(vL);
        vec3 ez = normalize(uPole), ex = normalize(uRef - ez * dot(uRef, ez)), ey = cross(ez, ex);
        float la = asin(clamp(dot(q, ez), -1.0, 1.0)), lo = atan(dot(q, ey), dot(q, ex)) + uSpin;
        vec2 uv = vec2(lo / 6.2831853 + 0.5, la / 3.14159265 + 0.5);
        vec3 col = mix(texture2D(uDay, uv).rgb, vec3(0.92), smoothstep(0.12, 0.75, texture2D(uCloud, uv).r));
        float d = dot(n, uSun);
        float lit = max(d, 0.0) * 0.85 + 0.15 * smoothstep(-0.05, 0.2, d);
        float rim = pow(1.0 - max(dot(n, normalize(-vP)), 0.0), 3.0);
        col = col * lit * 1.35 + vec3(0.22, 0.42, 0.95) * rim * smoothstep(-0.1, 0.3, d) * 0.7;
        gl_FragColor = vec4(pow(col, vec3(1.0 / 2.2)), 1.0);
      }`
  });
  const earth = new THREE.Mesh(new THREE.SphereGeometry(SKY_D * 6371 / 384400, 64, 40), earthMat);
  earth.position.copy(eDir).multiplyScalar(SKY_D);
  earth.frustumCulled = false; earth.renderOrder = 49;
  g.add(earth);
  g.userData.setSpin = a => { earthU.uSpin.value = a; };
  /* the stars, oriented by the Sun of the moment and the Moon's axis (within 1.5 degrees of the ecliptic's) */
  const stars = makeStars(ECLIPTIC_POLE); g.add(stars.points); g.userData.stars = stars;
  stars.points.onBeforeRender = () => stars.orient(sun.value, moon.shared.uPole.value);
  g.userData.earth = earth;
  return g;
}

/* ---------- rocks ---------- */
function rockGeometry(detail, seed) {
  const g = new THREE.IcosahedronGeometry(0.5, detail);
  const p = g.attributes.position;
  let s = seed * 9301 + 49297;
  const rnd = () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296);
  /* a few random cutting planes give the angular, broken look of lunar blocks; low-frequency lumps round it off */
  const planes = [];
  for (let k = 0; k < 7; k++) {
    const n = new THREE.Vector3(rnd() * 2 - 1, rnd() * 2 - 1, rnd() * 2 - 1).normalize();
    planes.push([n, 0.30 + 0.2 * rnd()]);
  }
  const sx = 0.8 + 0.5 * rnd(), sy = 0.5 + 0.35 * rnd(), sz = 0.8 + 0.5 * rnd();
  const v = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i);
    for (const [n, d] of planes) { const t = v.dot(n) - d; if (t > 0) v.addScaledVector(n, -t * 0.7); }
    const lump = 1 + 0.10 * Math.sin(v.x * 9 + seed) * Math.sin(v.y * 7 + seed * 2) + 0.07 * Math.sin(v.z * 13 + seed * 3)
      + 0.035 * Math.sin(v.x * 31 + seed * 5) * Math.sin(v.z * 27 + seed) + 0.03 * Math.sin(v.y * 41 + v.x * 17);
    v.multiplyScalar(lump); v.x *= sx; v.y *= sy; v.z *= sz;
    v.y += 0.14;                                   /* sits a little into the soil */
    p.setXYZ(i, v.x, v.y, v.z);
  }
  /* smooth normals: the icosphere is not indexed, so average the face normals of coincident vertices by hand */
  g.computeVertexNormals();
  const nrm = g.attributes.normal, acc = new Map();
  const key = i => Math.round(p.getX(i) * 1e4) + ',' + Math.round(p.getY(i) * 1e4) + ',' + Math.round(p.getZ(i) * 1e4);
  for (let i = 0; i < p.count; i++) {
    const k = key(i), a = acc.get(k) || [0, 0, 0];
    a[0] += nrm.getX(i); a[1] += nrm.getY(i); a[2] += nrm.getZ(i); acc.set(k, a);
  }
  for (let i = 0; i < p.count; i++) { const a = acc.get(key(i)), l = Math.hypot(a[0], a[1], a[2]) || 1; nrm.setXYZ(i, a[0] / l, a[1] / l, a[2] / l); }
  return g;
}

const ROCK_VERT = /* glsl */`
uniform sampler2D uMap; uniform vec2 uHEnc; uniform vec3 uSunV; uniform float uHalf; uniform float uPen;
#ifdef GROUND
uniform sampler2D uMapA; uniform vec4 uGround; attribute vec2 aUvA;
#endif
varying vec3 vN; varying vec3 vW; varying float vLit; varying float vY;
void main() {
  vec4 w = modelMatrix * instanceMatrix * vec4(position, 1.0);
  vW = w.xyz; vY = position.y;
  vN = normalize(mat3(modelMatrix) * mat3(instanceMatrix) * normal);
  /* is the rock in another thing's shadow? read the baked horizon just sunward of the rock, at ground level */
  vec3 c = (modelMatrix * instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  float size = length(mat3(instanceMatrix)[0]);
  vec2 h = normalize(uSunV.xz);
  vec2 gp = c.xz + h * (0.62 * size + 0.12);
  float el = asin(uSunV.y) - 0.02;
#ifdef GROUND
  /* on the generated ground: near shadows from its own map, the mountains' shadows from L4's */
  float H = texture2D(uMap, (gp - uGround.xy) / (uGround.zw - uGround.xy)).z * uHEnc.y + uHEnc.x;
  float Hp = texture2D(uMapA, aUvA).z * uHEnc.y + uHEnc.x;
  vLit = smoothstep(H - uPen, H + uPen, el) * smoothstep(Hp - uPen, Hp + uPen, el);
#else
  float H = texture2D(uMap, gp / uHalf * 0.5 + 0.5).z * uHEnc.y + uHEnc.x;
  vLit = smoothstep(H - uPen, H + uPen, el);
#endif
  gl_Position = projectionMatrix * viewMatrix * w;
}`;
const ROCK_FRAG = /* glsl */`
precision highp float;
uniform vec3 uTint;
varying vec3 vN; varying vec3 vW; varying float vLit; varying float vY;
${LUNAR_GLSL}
void main() {
  vec3 face = normalize(cross(dFdx(vW), dFdy(vW)));
  vec3 N = normalize(mix(normalize(vN), face, 0.25));
  vec3 up = vec3(0.0, 1.0, 0.0);
  float ao = smoothstep(-0.02, 0.22, vY);
  vec3 c = lunar(uTint * (0.75 + 0.25 * ao), N, normalize(cameraPosition - vW), up, vLit);
  gl_FragColor = vec4(display(c), 1.0);
}`;

/* Rocks for the generated ground (ground.js): the same shapes and shader, lit against the ground's own map. */
let _geos = null;
export function rockGeometries() {
  if (!_geos) _geos = [0, 1, 2].map(k => [rockGeometry(1, k + 1), rockGeometry(2, k + 1), rockGeometry(3, k + 1)]);
  return _geos;
}
export function groundRockMaterial(moon) {
  const u = moon.groundUniforms;
  return new THREE.ShaderMaterial({
    vertexShader: ROCK_VERT, fragmentShader: ROCK_FRAG, defines: { GROUND: 1 },
    uniforms: {
      uSun: moon.shared.uSun, uSunV: moon.shared.uSun, uExposure: moon.shared.uExposure, uEarth: moon.shared.uEarth,
      uMap: u.uMap, uMapA: u.uMapA, uGround: u.uGround, uHEnc: moon.shared.uHEnc, uHalf: { value: 1 },
      uPen: { value: 0.006 }, uTint: { value: new THREE.Color(0.105, 0.10, 0.094) }
    }
  });
}

export async function makeRocks(moon, base) {
  const man = moon.manifest, g = new THREE.Group();
  if (!man.rocks) return { group: g, update() {} };
  const d = new Float32Array(await (await fetch(base + 'rocks.bin')).arrayBuffer());
  const n = d.length / 6;
  const mat = new THREE.ShaderMaterial({
    vertexShader: ROCK_VERT, fragmentShader: ROCK_FRAG,
    uniforms: {
      uSun: moon.shared.uSun, uSunV: moon.shared.uSun, uExposure: moon.shared.uExposure, uEarth: moon.shared.uEarth,
      uMap: { value: moon.l6.map }, uHEnc: moon.shared.uHEnc, uHalf: { value: man.walk.half },
      uPen: { value: 0.006 }, uTint: { value: new THREE.Color(0.105, 0.10, 0.094) }
    }
  });
  const geos = rockGeometries();
  /* tiles of 50 m, three size classes: small rocks are only drawn near the viewer */
  const TILE = 50, classes = [{ max: 0.22, range: 45 }, { max: 0.55, range: 130 }, { max: 9, range: 1e9 }];
  const buckets = new Map();
  for (let i = 0; i < n; i++) {
    const x = d[i * 6], z = d[i * 6 + 2], size = d[i * 6 + 3], yaw = d[i * 6 + 4], shape = d[i * 6 + 5] | 0;
    /* stand on the ground as drawn (the mesh), not on the finer data behind its shading: take the lowest point
     * under the rock's footprint and bed it in a little, so no side hangs in the air on a slope or a crater rim */
    const r = 0.36 * size;
    const y = Math.min(moon.heightAt(x, z), moon.heightAt(x + r, z), moon.heightAt(x - r, z), moon.heightAt(x, z + r), moon.heightAt(x, z - r)) - 0.06 * size;
    const cls = size < classes[0].max ? 0 : size < classes[1].max ? 1 : 2;
    /* small rocks in 50 m tiles (only the near tiles are drawn); the larger ones in one batch per shape */
    const key = (cls === 0 ? [Math.floor(x / TILE), Math.floor(z / TILE), cls, shape % 3] : [0, 0, cls, shape % 3]).join(',');
    if (!buckets.has(key)) buckets.set(key, []);
    buckets.get(key).push([x, y, z, size, yaw]);
  }
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), s = new THREE.Vector3(), p = new THREE.Vector3();
  const meshes = [];
  let tris = 0;
  for (const [key, list] of buckets) {
    const [tx, tz, cls, shape] = key.split(',').map(Number);
    const geo = geos[shape][cls];
    const im = new THREE.InstancedMesh(geo, mat, list.length);
    list.forEach((r, i) => {
      e.set((Math.sin(r[4] * 7) * 0.25), r[4], Math.cos(r[4] * 5) * 0.25);
      m4.compose(p.set(r[0], r[1], r[2]), q.setFromEuler(e), s.setScalar(r[3]));
      im.setMatrixAt(i, m4);
    });
    im.instanceMatrix.needsUpdate = true;
    im.frustumCulled = false;
    im.userData = { cx: (tx + 0.5) * TILE, cz: (tz + 0.5) * TILE, range: classes[cls].range, all: cls > 0 };
    g.add(im); meshes.push(im);
    tris += list.length * geo.attributes.position.count / 3;
  }
  return {
    group: g, count: n, tris,
    update(cam) {
      for (const m of meshes) {
        const u = m.userData;
        m.visible = u.all || Math.hypot(cam.x - u.cx, cam.z - u.cz) < u.range + TILE;
      }
    }
  };
}

/* ---------- stand-in LM: right size and silhouette class, nothing more. Stage 5 builds the real one. ---------- */
export function makeLMStandIn(moon) {
  const g = new THREE.Group();
  const mat = (r, gr, b) => new THREE.ShaderMaterial({
    uniforms: { uSun: moon.shared.uSun, uExposure: moon.shared.uExposure, uCol: { value: new THREE.Color(r, gr, b) } },
    vertexShader: `varying vec3 vN; void main(){ vN = normalize(mat3(modelMatrix) * normal); gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
    fragmentShader: `uniform vec3 uSun; uniform vec3 uCol; uniform float uExposure; varying vec3 vN;
      void main(){ float d = max(dot(normalize(vN), uSun), 0.0); vec3 c = uCol * (d * 1.6 + 0.035) * uExposure;
      c = (c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14); gl_FragColor = vec4(pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0); }`
  });
  const gold = mat(0.30, 0.20, 0.06), grey = mat(0.14, 0.14, 0.15), dark = mat(0.04, 0.04, 0.04);
  const strut = (a, b, r) => {
    const o = new THREE.Mesh(new THREE.CylinderGeometry(r, r, a.distanceTo(b), 8), gold);
    o.position.copy(a).add(b).multiplyScalar(0.5);
    o.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize());
    g.add(o);
  };
  const add = (geo, m, x, y, z, rx = 0, ry = 0, rz = 0) => { const o = new THREE.Mesh(geo, m); o.position.set(x, y, z); o.rotation.set(rx, ry, rz); g.add(o); return o; };
  add(new THREE.CylinderGeometry(2.1, 2.1, 1.65, 8), gold, 0, 2.35, 0, 0, Math.PI / 8);            /* descent stage */
  add(new THREE.CylinderGeometry(1.15, 0.75, 1.3, 12, 1, true), dark, 0, 1.0, 0);                    /* engine bell */
  add(new THREE.CylinderGeometry(1.25, 1.45, 2.3, 10), grey, 0, 4.4, 0.1, Math.PI / 2, 0, 0);        /* cabin */
  add(new THREE.BoxGeometry(2.6, 1.5, 1.6), grey, 0, 4.2, -0.9);
  add(new THREE.CylinderGeometry(0.4, 0.4, 0.5, 12), grey, 0, 5.75, 0.2);                            /* docking tunnel */
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2, cx = Math.sin(a), cz = Math.cos(a);
    /* the footpad on the ground under it, the primary strut from the top of the stage, two from its bottom */
    const base = moon.heightAt(0, 0), fx = cx * 4.5, fz = cz * 4.5, fy = moon.heightAt(fx, fz) - base;
    add(new THREE.CylinderGeometry(0.47, 0.42, 0.14, 16), gold, fx, fy + 0.09, fz);
    const foot = new THREE.Vector3(fx, fy + 0.2, fz);
    strut(new THREE.Vector3(cx * 2.05, 3.0, cz * 2.05), foot, 0.085);
    strut(new THREE.Vector3(cx * 1.9 + cz * 0.9, 1.6, cz * 1.9 - cx * 0.9), new THREE.Vector3(fx * 0.8, fy + 0.75, fz * 0.8), 0.05);
    strut(new THREE.Vector3(cx * 1.9 - cz * 0.9, 1.6, cz * 1.9 + cx * 0.9), new THREE.Vector3(fx * 0.8, fy + 0.75, fz * 0.8), 0.05);
  }
  g.position.y = moon.heightAt(0, 0);
  return g;
}

/* A plain handrail around the viewer in the orbit views: something still to stand in. Test rig only. */
export function makeRail() {
  const g = new THREE.Group();
  const m = new THREE.MeshBasicMaterial({ color: 0x1b1d20 });
  const ring = new THREE.Mesh(new THREE.TorusGeometry(0.75, 0.016, 8, 64), m);
  ring.rotation.x = Math.PI / 2; ring.position.y = 1.02; g.add(ring);
  const floor = new THREE.Mesh(new THREE.RingGeometry(0.72, 0.78, 64), m);
  floor.rotation.x = -Math.PI / 2; floor.position.y = 0.0; g.add(floor);
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2 + Math.PI / 4;
    const post = new THREE.Mesh(new THREE.CylinderGeometry(0.012, 0.012, 1.02, 6), m);
    post.position.set(Math.sin(a) * 0.75, 0.51, Math.cos(a) * 0.75); g.add(post);
  }
  return g;
}
