/* The Moon: nested lat/lon levels centred on the landing site, built by tools/build.py.
 *
 * Every level is a grid mesh with one RGBA map: R,G = normal (east, north), B = elevation angle of the terrain
 * horizon towards the Sun (cast shadows by one comparison), A = albedo detail. A coarser level is not drawn where
 * the next finer one lies (the rectangle uHole). All lighting is texture lookups: nothing procedural per pixel.
 *
 * Coordinates: the site frame. X east, Y up, Z south, origin on the ground at the LM, metres.
 */
import * as THREE from 'three';
import { KTX2Loader } from '../vendor/KTX2Loader.js';
import { makeGeo, gridHeight } from './geo.js?v=3';

const CHUNKS = 4;                       /* each level is cut into CHUNKS x CHUNKS meshes for frustum culling */
export const NEAR_LEVELS = ['L4', 'L5', 'L6'];

const VERT = /* glsl */`
varying vec2 vUv;
varying vec3 vPos;
void main() {
  vUv = uv;
  vPos = position;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}`;

/* Shared by the terrain and the rocks: how lunar soil reflects sunlight.
 * - Lunar-Lambert (Lommel-Seeliger with a Lambert share that grows with phase angle): no limb darkening.
 * - A phase function with the opposition surge: the ground brightens towards the point opposite the Sun.
 * - Shadows are filled only by light bounced off the sunlit ground. */
export const LUNAR_GLSL = /* glsl */`
uniform vec3 uSun;
uniform float uExposure;
vec3 lunar(vec3 alb, vec3 N, vec3 V, vec3 up, float lit) {
  float mu0 = max(dot(N, uSun), 0.0);
  float mu = max(dot(N, V), 0.03);
  float g = acos(clamp(dot(uSun, V), -1.0, 1.0));
  float L = mix(1.0, 0.22, smoothstep(0.0, 1.9, g));
  float refl = L * 2.0 * mu0 / (mu0 + mu) + (1.0 - L) * mu0;
  float phase = (0.30 + 0.70 * exp(-1.6 * g)) * (1.0 + 0.8 / (1.0 + tan(min(g, 3.0) * 0.5) / 0.055));
  float sunUp = max(dot(uSun, up), 0.0);
  vec3 bounce = alb * dot(alb, vec3(0.33)) * 0.55 * sunUp * (0.55 + 0.45 * dot(N, up));
  vec3 earthshine = alb * vec3(0.0016, 0.0019, 0.0024);
  return alb * (refl * phase * lit) + bounce + earthshine;
}
vec3 display(vec3 c) {
  /* a long shoulder: sunward slopes are several times brighter than level ground at a low Sun, and must not clip */
  c *= uExposure;
  c = 1.0 - exp(-c * (1.0 + 0.35 * c) / (1.0 + 0.9 * c));
  return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}
vec3 soilColour(vec3 c) {
  float l = dot(c, vec3(0.3, 0.5, 0.2));
  return mix(vec3(l), c, 0.65);
}`;

const FRAG = /* glsl */`
precision highp float;
uniform sampler2D uMap;
uniform sampler2D uAlb;
uniform vec4 uAlbST;
uniform vec4 uHole;
uniform vec3 uCentre;
uniform vec3 uPole;
uniform vec2 uHEnc;
uniform float uPenumbra;
uniform float uDebug;      /* 1: shadow mask tinted by level (test aid, ?debug=shadow) */
uniform vec3 uLevelTint;
uniform vec4 uPatch;       /* x0, z0, x1, z1 of the generated ground round the walker: the levels leave it out */
#ifdef GROUND
/* the generated ground: its own map holds only the detail; normal, far shadows and albedo come from L4 and L5 */
uniform sampler2D uMapA;
uniform sampler2D uMapB;
uniform vec4 uL6Rect;
uniform vec4 uGround;
varying vec2 vUvA;
varying vec2 vUvB;
#endif
#ifdef NEAR
uniform sampler2D uMicro;
uniform float uMicroK;
#endif
#ifdef PRINT
uniform sampler2D uPrint;     /* R,G normal across and along the print, B height, A coverage */
uniform vec2 uPrintSize;      /* metres across, along */
varying vec2 vPUv;
varying vec3 vPT;
varying vec3 vPL;
#endif
varying vec2 vUv;
varying vec3 vPos;
${LUNAR_GLSL}
#ifdef PRINT
float printH(vec2 uv) { return texture2D(uPrint, uv).z * 0.037 - 0.025; }
#endif
void main() {
#ifdef GROUND
  if (vPos.x > uL6Rect.x && vPos.x < uL6Rect.z && vPos.z > uL6Rect.y && vPos.z < uL6Rect.w) discard;
#ifdef PRINT
  if (vPos.x < uGround.x || vPos.x > uGround.z || vPos.z < uGround.y || vPos.z > uGround.w) discard;
#endif
#else
  if (vUv.x > uHole.x && vUv.x < uHole.z && vUv.y > uHole.y && vUv.y < uHole.w) discard;
  if (vPos.x > uPatch.x && vPos.x < uPatch.z && vPos.z > uPatch.y && vPos.z < uPatch.w) discard;
#endif
#ifdef PRINT
  vec4 pr = texture2D(uPrint, vPUv);
  if (pr.a < 0.004) discard;
  float inside = smoothstep(-0.003, -0.008, pr.z * 0.037 - 0.025);
#endif
  vec3 up = normalize(vPos - uCentre);
  vec3 east = normalize(cross(uPole, up));
  vec3 north = cross(up, east);
  vec4 m = texture2D(uMap, vUv);
  vec2 n2 = (m.xy * 255.0 - 128.0) / 127.0;
#ifdef GROUND
  vec4 pa = texture2D(uMapA, vUvA), pb = texture2D(uMapB, vUvB);
  vec2 eb = min(vUvB, 1.0 - vUvB);
  vec4 par = mix(pa, pb, smoothstep(0.0, 0.012, min(eb.x, eb.y)));
  n2 += (par.xy * 255.0 - 128.0) / 127.0;
#endif
  vec3 toCam = cameraPosition - vPos;
  float dist = length(toCam);
#ifdef NEAR
  /* soil relief below the map's resolution: two scales of one tiling normal texture, fading with distance */
  vec2 w = vPos.xz;
  vec2 mi = (texture2D(uMicro, w * 2.9).xy - 0.5) * exp(-dist / 5.0)
          + (texture2D(uMicro, w * 0.61 + 0.37).xy - 0.5) * exp(-dist / 22.0);
#ifdef PRINT
  mi *= 1.0 - 0.75 * inside;                  /* pressed soil is smoother */
#endif
  n2 += vec2(mi.x, -mi.y) * uMicroK;
#endif
  vec3 N = normalize(east * n2.x + north * n2.y + up * sqrt(max(1.0 - dot(n2, n2), 0.02)));
  float H = m.z * uHEnc.y + uHEnc.x;
  float el = asin(clamp(dot(uSun, up), -1.0, 1.0));
  float lit = smoothstep(H - uPenumbra, H + uPenumbra, el);
#ifdef GROUND
  float Hp = par.z * uHEnc.y + uHEnc.x;
  lit *= smoothstep(Hp - uPenumbra, Hp + uPenumbra, el);
  vec3 alb = soilColour(texture2D(uAlb, vUvA * uAlbST.xy + uAlbST.zw).rgb) * (m.a * (255.0 / 128.0)) * (par.a * (255.0 / 128.0));
#else
  vec3 alb = soilColour(texture2D(uAlb, vUv * uAlbST.xy + uAlbST.zw).rgb) * (m.a * (255.0 / 128.0));
#endif
#ifdef PRINT
  /* the print's own relief, and the shadows its walls and ribs cast at a low Sun (a short march over its height) */
  vec2 pn = pr.xy * 2.0 - 1.0;
  vec3 Ng = N;
  N = normalize(vPT * pn.x + vPL * pn.y + N * sqrt(max(1.0 - dot(pn, pn), 0.05)));
  float sT = dot(uSun, vPT), sL = dot(uSun, vPL);
  float sh = length(vec2(sT, sL));
  float tanEl = max(dot(uSun, Ng), 0.0) / max(sh, 1e-3);
  vec2 stepUv = vec2(sT, sL) / max(sh, 1e-3) * 0.007 / uPrintSize;
  float h0 = printH(vPUv), occ = 0.0;
  for (int k = 1; k <= 8; k++) {
    float fk = float(k);
    occ = max(occ, (printH(vPUv + stepUv * fk) - h0 - fk * 0.007 * tanEl) / 0.0015);
  }
  lit *= 1.0 - clamp(occ, 0.0, 1.0);
  alb *= 1.0 - 0.07 * inside;
  gl_FragColor = vec4(display(lunar(alb, N, toCam / dist, up, lit)), pr.a);
#else
  gl_FragColor = vec4(display(lunar(alb, N, toCam / dist, up, lit)), 1.0);
  if (uDebug > 0.5) gl_FragColor = vec4(mix(uLevelTint * 0.25, uLevelTint, lit), 1.0);
#endif
}`;

const PRINT_VERT = /* glsl */`
uniform float uHalf;
varying vec2 vUv;
varying vec3 vPos;
varying vec2 vPUv;
varying vec3 vPT;
varying vec3 vPL;
void main() {
  vec4 w = modelMatrix * instanceMatrix * vec4(position, 1.0);
  vPos = w.xyz;
  vUv = vPos.xz / uHalf * 0.5 + 0.5;
  vPUv = uv;
  vPT = normalize(mat3(instanceMatrix) * vec3(1.0, 0.0, 0.0));
  vPL = normalize(mat3(instanceMatrix) * vec3(0.0, 0.0, -1.0));
  gl_Position = projectionMatrix * viewMatrix * w;
}`;

const GROUND_VERT = /* glsl */`
attribute vec2 uvA;
attribute vec2 uvB;
varying vec2 vUv;
varying vec3 vPos;
varying vec2 vUvA;
varying vec2 vUvB;
void main() {
  vUv = uv; vUvA = uvA; vUvB = uvB;
  vPos = position;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}`;

const PRINT_GROUND_VERT = /* glsl */`
attribute vec4 aUvAB;       /* per print: its place in L4's and L5's maps */
uniform vec4 uGround;
varying vec2 vUv;
varying vec3 vPos;
varying vec2 vUvA;
varying vec2 vUvB;
varying vec2 vPUv;
varying vec3 vPT;
varying vec3 vPL;
void main() {
  vec4 w = modelMatrix * instanceMatrix * vec4(position, 1.0);
  vPos = w.xyz;
  vUv = (vPos.xz - uGround.xy) / (uGround.zw - uGround.xy);
  vUvA = aUvAB.xy; vUvB = aUvAB.zw;
  vPUv = uv;
  vPT = normalize(mat3(instanceMatrix) * vec3(1.0, 0.0, 0.0));
  vPL = normalize(mat3(instanceMatrix) * vec3(0.0, 0.0, -1.0));
  gl_Position = projectionMatrix * viewMatrix * w;
}`;

function warpFn(a) { return s => s * (a + (1 - a) * s * s); }

/* A tiling soil normal texture (value-noise fBm), made once at start-up. */
function microTexture() {
  const n = 256, h = new Float32Array(n * n);
  let seed = 7;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  for (let oct = 0; oct < 5; oct++) {
    const c = 8 << oct, g = new Float32Array(c * c), amp = Math.pow(0.62, oct);
    for (let i = 0; i < g.length; i++) g[i] = rnd();
    for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
      const fx = x / n * c, fy = y / n * c, x0 = Math.floor(fx), y0 = Math.floor(fy);
      let tx = fx - x0, ty = fy - y0; tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
      const x1 = (x0 + 1) % c, y1 = (y0 + 1) % c;
      h[y * n + x] += amp * ((g[y0 * c + x0] * (1 - tx) + g[y0 * c + x1] * tx) * (1 - ty)
                           + (g[y1 * c + x0] * (1 - tx) + g[y1 * c + x1] * tx) * ty);
    }
  }
  const d = new Uint8Array(n * n * 4);
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    const dx = h[y * n + (x + 1) % n] - h[y * n + (x + n - 1) % n];
    const dy = h[((y + 1) % n) * n + x] - h[((y + n - 1) % n) * n + x];
    const i = (y * n + x) * 4;
    d[i] = Math.max(0, Math.min(255, 127.5 - dx * 420)); d[i + 1] = Math.max(0, Math.min(255, 127.5 - dy * 420));
    d[i + 2] = 255; d[i + 3] = 255;
  }
  const t = new THREE.DataTexture(d, n, n, THREE.RGBAFormat);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.generateMipmaps = true;
  t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter; t.needsUpdate = true;
  return t;
}

export async function loadMoon(renderer, base, onProgress = () => {}) {
  const man = await (await fetch(base + 'moon.json')).json();
  const ktx = man.ext === 'ktx2';
  let ktxLoader = null;
  if (ktx) {
    ktxLoader = new KTX2Loader().setTranscoderPath(new URL('../vendor/basis/', import.meta.url).href).detectSupport(renderer);
  }
  const aniso = Math.min(4, renderer.capabilities.getMaxAnisotropy());
  const texLoader = new THREE.TextureLoader();
  let done = 0;
  const total = man.levels.length * 2 + 2;
  const tick = () => onProgress(++done / total);
  const loadTex = async (name, srgb, plainExt) => {
    const t = ktx ? await ktxLoader.loadAsync(base + name + '.ktx2') : await texLoader.loadAsync(base + name + '.' + plainExt);
    t.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace;
    if (!ktx) { t.flipY = false; t.generateMipmaps = true; }
    t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter;
    t.anisotropy = aniso; t.needsUpdate = true;
    tick();
    return t;
  };

  const lat = man.site.lat * Math.PI / 180, lon = man.site.lon * Math.PI / 180;
  const shared = {
    uSun: { value: new THREE.Vector3().fromArray(man.sun.local).normalize() },
    uExposure: { value: 3.4 },
    uCentre: { value: new THREE.Vector3(0, -man.r0, 0) },
    uPole: { value: new THREE.Vector3(0, Math.sin(lat), -Math.cos(lat)) },
    uHEnc: { value: new THREE.Vector2(man.hEncode[0], man.hEncode[1]) },
    uMicro: { value: microTexture() },
    uDebug: { value: new URLSearchParams(location.search).get('debug') === 'shadow' ? 1 : 0 },
    uPatch: { value: new THREE.Vector4(1e9, 1e9, -1e9, -1e9) }
  };
  const geo = makeGeo(man);

  const [gColour, aColour] = await Promise.all([loadTex('Gcolor', true, 'jpg'), loadTex('A1color', true, 'jpg')]);
  gColour.wrapS = THREE.RepeatWrapping;

  const far = new THREE.Group(), near = new THREE.Group();
  const L1 = man.levels[1];
  const levels = [];

  await Promise.all(man.levels.map(async (lv, k) => {
    const [map, buf] = await Promise.all([
      loadTex(lv.name, false, 'png'),
      fetch(base + lv.name + '.pos.bin').then(r => r.arrayBuffer()).then(b => { tick(); return b; })
    ]);
    if (lv.name === 'G') map.wrapS = THREE.RepeatWrapping;
    const nx = lv.nx, ny = lv.ny, w = warpFn(lv.warp);
    const pos = new Float32Array(buf);
    const nv = (nx + 1) * (ny + 1);
    if (pos.length !== nv * 3) throw new Error(lv.name + ': mesh size mismatch');
    const us = new Float32Array(nx + 1), vs = new Float32Array(ny + 1);
    for (let i = 0; i <= nx; i++) us[i] = (w(i / nx * 2 - 1) + 1) / 2;
    for (let j = 0; j <= ny; j++) vs[j] = (w(j / ny * 2 - 1) + 1) / 2;

    /* skirt: a strip hanging from the border, so small steps against the parent never show as cracks */
    const border = [];
    for (let i = 0; i < nx; i++) border.push(i);
    for (let j = 0; j < ny; j++) border.push(j * (nx + 1) + nx);
    for (let i = nx; i > 0; i--) border.push(ny * (nx + 1) + i);
    for (let j = ny; j > 0; j--) border.push(j * (nx + 1));
    const hasSkirt = lv.name !== 'G';
    const ns = hasSkirt ? border.length : 0;
    const P = new Float32Array((nv + ns) * 3), UV = new Float32Array((nv + ns) * 2);
    P.set(pos);
    for (let j = 0; j <= ny; j++) for (let i = 0; i <= nx; i++) { const q = (j * (nx + 1) + i) * 2; UV[q] = us[i]; UV[q + 1] = vs[j]; }
    const cell = 2 * lv.dlat * Math.PI / 180 * man.radius / ny;
    const c = shared.uCentre.value, up = new THREE.Vector3();
    for (let s = 0; s < ns; s++) {
      const b = border[s];
      up.set(pos[b * 3] - c.x, pos[b * 3 + 1] - c.y, pos[b * 3 + 2] - c.z).normalize();
      const drop = cell * 1.5;
      P[(nv + s) * 3] = pos[b * 3] - up.x * drop; P[(nv + s) * 3 + 1] = pos[b * 3 + 1] - up.y * drop; P[(nv + s) * 3 + 2] = pos[b * 3 + 2] - up.z * drop;
      UV[(nv + s) * 2] = UV[b * 2]; UV[(nv + s) * 2 + 1] = UV[b * 2 + 1];
    }
    const aPos = new THREE.BufferAttribute(P, 3), aUv = new THREE.BufferAttribute(UV, 2);

    /* the rectangle of this level that the next finer level covers */
    const child = man.levels[k + 1];
    const hole = new THREE.Vector4(2, 2, -2, -2);
    let hi0 = 1, hi1 = 0, hj0 = 1, hj1 = 0;
    if (child) {
      const inset = 0.75 / child.nx;                         /* keep three quarters of a child cell of overlap */
      const fu = f => (f * child.dlon * (1 - 2 * inset) + (child.lon0 - lv.lon0)) / lv.dlon;
      const fv = f => (f * child.dlat * (1 - 2 * inset) - (child.lat0 - lv.lat0)) / lv.dlat;
      hole.set((fu(-1) + 1) / 2, (fv(-1) + 1) / 2, (fu(1) + 1) / 2, (fv(1) + 1) / 2);
      /* cells entirely inside the hole are not even sent to the GPU */
      hi0 = 0; while (hi0 < nx && us[hi0] < hole.x) hi0++;
      hi1 = nx; while (hi1 > 0 && us[hi1] > hole.z) hi1--;
      hj0 = 0; while (hj0 < ny && vs[hj0] < hole.y) hj0++;
      hj1 = ny; while (hj1 > 0 && vs[hj1] > hole.w) hj1--;
    }

    const isNear = NEAR_LEVELS.includes(lv.name);
    const s = lv.name === 'G' ? 1 : lv.dlat / L1.dlat;
    const mat = new THREE.ShaderMaterial({
      vertexShader: VERT, fragmentShader: FRAG,
      defines: (lv.name === 'L5' || lv.name === 'L6') ? { NEAR: 1 } : {},
      uniforms: {
        ...shared,
        uMap: { value: map },
        uAlb: { value: lv.name === 'G' ? gColour : aColour },
        uAlbST: { value: new THREE.Vector4(s, s, 0.5 - 0.5 * s, 0.5 - 0.5 * s) },
        uHole: { value: hole },
        uPenumbra: { value: Math.max(0.0047, 0.6 * man.hEncode[1] / 255) },
        uMicroK: { value: lv.name === 'L6' ? 0.30 : 0.2 },
        uDebug: shared.uDebug,
        uLevelTint: { value: new THREE.Color().setHSL(k / 7, 0.7, 0.6) },
        uPatch: lv.name === 'L6' ? { value: new THREE.Vector4(1e9, 1e9, -1e9, -1e9) } : shared.uPatch
      },
      polygonOffset: true, polygonOffsetFactor: (man.levels.length - 1 - k) * 0.5, polygonOffsetUnits: (man.levels.length - 1 - k) * 2
    });
    const group = isNear ? near : far;
    const cx = Math.ceil(nx / CHUNKS), cy = Math.ceil(ny / CHUNKS);
    const Index = (nv + ns) > 65535 ? Uint32Array : Uint16Array;
    let tris = 0;
    /* two versions of every chunk: all cells, and every fourth vertex for when the level is far from the viewer */
    const fine = [], coarse = [];
    const strides = lv.name === 'G' ? [1] : [1, 4];
    for (const st of strides) for (let b = 0; b < CHUNKS; b++) for (let a = 0; a < CHUNKS; a++) {
      const idx = [];
      for (let j = b * cy; j < Math.min(ny, (b + 1) * cy); j += st) for (let i = a * cx; i < Math.min(nx, (a + 1) * cx); i += st) {
        if (i >= hi0 && i + st <= hi1 && j >= hj0 && j + st <= hj1) continue;
        const p = j * (nx + 1) + i, q = p + st * (nx + 1);
        idx.push(p, q, p + st, q, q + st, p + st);
      }
      if (!idx.length) continue;
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', aPos); g.setAttribute('uv', aUv);
      g.setIndex(new THREE.BufferAttribute(new Index(idx), 1));
      /* bounding sphere of this chunk only (the attribute is shared by all chunks) */
      const box = new THREE.Box3(), v = new THREE.Vector3();
      for (let n = 0; n < idx.length; n += 3) box.expandByPoint(v.fromArray(P, idx[n] * 3));
      g.boundingSphere = box.getBoundingSphere(new THREE.Sphere());
      g.boundingSphere.radius *= 1.05;
      const mesh = new THREE.Mesh(g, mat);
      mesh.matrixAutoUpdate = false; mesh.renderOrder = k;
      group.add(mesh); (st === 1 ? fine : coarse).push(mesh);
      if (st === 1) tris += idx.length / 3; else mesh.visible = false;
    }
    if (hasSkirt) {
      const idx = [];
      for (let s2 = 0; s2 < ns; s2++) {
        const a = border[s2], b = border[(s2 + 1) % ns], sa = nv + s2, sb = nv + (s2 + 1) % ns;
        idx.push(a, sa, b, b, sa, sb);
      }
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', aPos); g.setAttribute('uv', aUv);
      g.setIndex(new THREE.BufferAttribute(new Index(idx), 1));
      const mesh = new THREE.Mesh(g, mat);
      mesh.matrixAutoUpdate = false; mesh.frustumCulled = false; mesh.renderOrder = k;
      mat.side = THREE.DoubleSide;
      group.add(mesh); tris += idx.length / 3;
    }
    levels[k] = { ...lv, material: mat, map, tris, fine, coarse, half: lv.dlat * Math.PI / 180 * man.radius, shown: true, far: false,
                  meshHeight: k === man.levels.length - 1 ? gridHeight(pos, nx, ny) : null, P: pos };
  }));

  /* walking heights: the finest mesh exactly as drawn; outside it, the baked table */
  const meshHeight = levels[levels.length - 1].meshHeight;
  let walk = null;
  if (man.walk) {
    const raw = new Int16Array(await (await fetch(base + 'walk.bin')).arrayBuffer());
    walk = { n: man.walk.n, half: man.walk.half, data: raw };
  }
  const lvA = levels.find(l => l.name === 'L4'), lvB = levels.find(l => l.name === 'L5');
  const _ll = {}, _uv = {};
  const api = { groundHeight: null };          /* set by ground.js: the generated ground round the walker */
  function heightAt(x, z) {
    let h = meshHeight(x, z);
    if (h !== null) return h;
    if (api.groundHeight) { h = api.groundHeight(x, z); if (h !== null) return h; }
    /* further out: the coarser levels' meshes as drawn */
    geo.latlon(x, 0, z, _ll);
    h = geo.surfaceY(lvB, lvB.P, geo.levelUV(lvB, _ll, _uv));
    if (h === null) h = geo.surfaceY(lvA, lvA.P, geo.levelUV(lvA, _ll, _uv));
    if (h !== null) return h;
    if (!walk) return 0;
    const n = walk.n, fx = Math.min(n - 1.001, Math.max(0, (x / walk.half + 1) / 2 * (n - 1)));
    const fz = Math.min(n - 1.001, Math.max(0, (z / walk.half + 1) / 2 * (n - 1)));
    const i = Math.floor(fx), j = Math.floor(fz), tx = fx - i, tz = fz - j, d = walk.data;
    return ((d[j * n + i] * (1 - tx) + d[j * n + i + 1] * tx) * (1 - tz)
          + (d[(j + 1) * n + i] * (1 - tx) + d[(j + 1) * n + i + 1] * tx) * tz) / 100;
  }

  const sun0 = shared.uSun.value.clone(), pole = shared.uPole.value;
  /* Move the Sun as the Moon turns: degrees of rotation since landing (0.51 deg per hour). Positive = later = higher. */
  function setSunOffset(deg) {
    shared.uSun.value.copy(sun0).applyAxisAngle(pole, -deg * Math.PI / 180).normalize();
  }
  /* Per frame: levels far from the viewer are drawn coarsely; from the ground, the levels beyond the mountains
   * that close the valley are not drawn at all. `cam` in the site frame. */
  function setDetail(cam, onSurface) {
    const d = cam.length();
    for (const lv of levels) {
      const show = !(onSurface && (lv.name === 'G' || lv.name === 'L1'));
      const far2 = lv.coarse.length > 0 && d > lv.half * 12;
      if (show === lv.shown && far2 === lv.far) continue;
      lv.shown = show; lv.far = far2;
      for (const m of lv.fine) m.visible = show && !far2;
      for (const m of lv.coarse) m.visible = show && far2;
    }
  }
  /* The material for bootprints: the finest level's own shader and uniforms, so a print matches the ground round it. */
  function printMaterial(printTex, size) {
    const l6 = levels[levels.length - 1], u = l6.material.uniforms;
    return new THREE.ShaderMaterial({
      vertexShader: PRINT_VERT, fragmentShader: FRAG,
      defines: { NEAR: 1, PRINT: 1 },
      uniforms: { ...u, uHole: { value: new THREE.Vector4(2, 2, -2, -2) }, uHalf: { value: l6.half },
                  uPrint: { value: printTex }, uPrintSize: { value: size } },
      transparent: true, depthWrite: false, side: THREE.DoubleSide,
      polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -8
    });
  }
  /* The material of the generated ground (ground.js), and of bootprints pressed into it. */
  const l6r = levels[levels.length - 1].half - 0.3;
  const groundUniforms = {
    ...levels[levels.length - 1].material.uniforms,
    uMap: { value: null }, uMapA: { value: lvA.map }, uMapB: { value: lvB.map },
    uAlbST: lvA.material.uniforms.uAlbST,
    uHole: { value: new THREE.Vector4(2, 2, -2, -2) },
    uL6Rect: { value: new THREE.Vector4(-l6r, -l6r, l6r, l6r) },
    uGround: { value: new THREE.Vector4(1e9, 1e9, -1e9, -1e9) },
    uLevelTint: { value: new THREE.Color(0.9, 0.5, 0.9) }
  };
  function groundMaterial() {
    return new THREE.ShaderMaterial({
      vertexShader: GROUND_VERT, fragmentShader: FRAG, defines: { NEAR: 1, GROUND: 1 }, uniforms: groundUniforms,
      polygonOffset: true, polygonOffsetFactor: -0.5, polygonOffsetUnits: -2
    });
  }
  function printGroundMaterial(printTex, size) {
    return new THREE.ShaderMaterial({
      vertexShader: PRINT_GROUND_VERT, fragmentShader: FRAG, defines: { NEAR: 1, GROUND: 1, PRINT: 1 },
      uniforms: { ...groundUniforms, uPrint: { value: printTex }, uPrintSize: { value: size } },
      transparent: true, depthWrite: false, side: THREE.DoubleSide,
      polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -8
    });
  }
  /* where a point lies in L4's and L5's maps (for things standing on the generated ground) */
  function uvAB(x, y, z, out) {
    geo.latlon(x, y, z, _ll);
    geo.levelUV(lvA, _ll, _uv); out[0] = _uv.u; out[1] = _uv.v;
    geo.levelUV(lvB, _ll, _uv); out[2] = _uv.u; out[3] = _uv.v;
    return out;
  }
  return Object.assign(api, { manifest: man, far, near, levels, shared, heightAt, setSunOffset, setDetail, printMaterial,
    groundMaterial, printGroundMaterial, groundUniforms, uvAB, geo, l6: levels[levels.length - 1] });
}
